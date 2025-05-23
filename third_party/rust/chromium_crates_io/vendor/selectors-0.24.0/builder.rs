/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Helper module to build up a selector safely and efficiently.
//!
//! Our selector representation is designed to optimize matching, and has
//! several requirements:
//! * All simple selectors and combinators are stored inline in the same buffer
//!   as Component instances.
//! * We store the top-level compound selectors from right to left, i.e. in
//!   matching order.
//! * We store the simple selectors for each combinator from left to right, so
//!   that we match the cheaper simple selectors first.
//!
//! Meeting all these constraints without extra memmove traffic during parsing
//! is non-trivial. This module encapsulates those details and presents an
//! easy-to-use API for the parser.

use crate::parser::{Combinator, Component, SelectorImpl};
use crate::sink::Push;
use servo_arc::{Arc, HeaderWithLength, ThinArc};
use smallvec::{self, SmallVec};
use std::cmp;
use std::iter;
use std::ptr;
use std::slice;

/// Top-level SelectorBuilder struct. This should be stack-allocated by the
/// consumer and never moved (because it contains a lot of inline data that
/// would be slow to memmov).
///
/// After instantation, callers may call the push_simple_selector() and
/// push_combinator() methods to append selector data as it is encountered
/// (from left to right). Once the process is complete, callers should invoke
/// build(), which transforms the contents of the SelectorBuilder into a heap-
/// allocated Selector and leaves the builder in a drained state.
#[derive(Debug)]
pub struct SelectorBuilder<Impl: SelectorImpl> {
    /// The entire sequence of simple selectors, from left to right, without combinators.
    ///
    /// We make this large because the result of parsing a selector is fed into a new
    /// Arc-ed allocation, so any spilled vec would be a wasted allocation. Also,
    /// Components are large enough that we don't have much cache locality benefit
    /// from reserving stack space for fewer of them.
    simple_selectors: SmallVec<[Component<Impl>; 32]>,
    /// The combinators, and the length of the compound selector to their left.
    combinators: SmallVec<[(Combinator, usize); 16]>,
    /// The length of the current compount selector.
    current_len: usize,
}

impl<Impl: SelectorImpl> Default for SelectorBuilder<Impl> {
    #[inline(always)]
    fn default() -> Self {
        SelectorBuilder {
            simple_selectors: SmallVec::new(),
            combinators: SmallVec::new(),
            current_len: 0,
        }
    }
}

impl<Impl: SelectorImpl> Push<Component<Impl>> for SelectorBuilder<Impl> {
    fn push(&mut self, value: Component<Impl>) {
        self.push_simple_selector(value);
    }
}

impl<Impl: SelectorImpl> SelectorBuilder<Impl> {
    /// Pushes a simple selector onto the current compound selector.
    #[inline(always)]
    pub fn push_simple_selector(&mut self, ss: Component<Impl>) {
        assert!(!ss.is_combinator());
        self.simple_selectors.push(ss);
        self.current_len += 1;
    }

    /// Completes the current compound selector and starts a new one, delimited
    /// by the given combinator.
    #[inline(always)]
    pub fn push_combinator(&mut self, c: Combinator) {
        self.combinators.push((c, self.current_len));
        self.current_len = 0;
    }

    /// Returns true if combinators have ever been pushed to this builder.
    #[inline(always)]
    pub fn has_combinators(&self) -> bool {
        !self.combinators.is_empty()
    }

    /// Consumes the builder, producing a Selector.
    #[inline(always)]
    pub fn build(
        &mut self,
        parsed_pseudo: bool,
        parsed_slotted: bool,
        parsed_part: bool,
    ) -> ThinArc<SpecificityAndFlags, Component<Impl>> {
        // Compute the specificity and flags.
        let specificity = specificity(self.simple_selectors.iter());
        let mut flags = SelectorFlags::empty();
        if parsed_pseudo {
            flags |= SelectorFlags::HAS_PSEUDO;
        }
        if parsed_slotted {
            flags |= SelectorFlags::HAS_SLOTTED;
        }
        if parsed_part {
            flags |= SelectorFlags::HAS_PART;
        }
        self.build_with_specificity_and_flags(SpecificityAndFlags { specificity, flags })
    }

    /// Builds with an explicit SpecificityAndFlags. This is separated from build() so
    /// that unit tests can pass an explicit specificity.
    #[inline(always)]
    pub fn build_with_specificity_and_flags(
        &mut self,
        spec: SpecificityAndFlags,
    ) -> ThinArc<SpecificityAndFlags, Component<Impl>> {
        // First, compute the total number of Components we'll need to allocate
        // space for.
        let full_len = self.simple_selectors.len() + self.combinators.len();

        // Create the header.
        let header = HeaderWithLength::new(spec, full_len);

        // Create the Arc using an iterator that drains our buffers.

        // Use a raw pointer to be able to call set_len despite "borrowing" the slice.
        // This is similar to SmallVec::drain, but we use a slice here because
        // we’re gonna traverse it non-linearly.
        let raw_simple_selectors: *const [Component<Impl>] = &*self.simple_selectors;
        unsafe {
            // Panic-safety: if SelectorBuilderIter is not iterated to the end,
            // some simple selectors will safely leak.
            self.simple_selectors.set_len(0)
        }
        let (rest, current) = split_from_end(unsafe { &*raw_simple_selectors }, self.current_len);
        let iter = SelectorBuilderIter {
            current_simple_selectors: current.iter(),
            rest_of_simple_selectors: rest,
            combinators: self.combinators.drain(..).rev(),
        };

        Arc::into_thin(Arc::from_header_and_iter(header, iter))
    }
}

struct SelectorBuilderIter<'a, Impl: SelectorImpl> {
    current_simple_selectors: slice::Iter<'a, Component<Impl>>,
    rest_of_simple_selectors: &'a [Component<Impl>],
    combinators: iter::Rev<smallvec::Drain<'a, [(Combinator, usize); 16]>>,
}

impl<'a, Impl: SelectorImpl> ExactSizeIterator for SelectorBuilderIter<'a, Impl> {
    fn len(&self) -> usize {
        self.current_simple_selectors.len() +
            self.rest_of_simple_selectors.len() +
            self.combinators.len()
    }
}

impl<'a, Impl: SelectorImpl> Iterator for SelectorBuilderIter<'a, Impl> {
    type Item = Component<Impl>;
    #[inline(always)]
    fn next(&mut self) -> Option<Self::Item> {
        if let Some(simple_selector_ref) = self.current_simple_selectors.next() {
            // Move a simple selector out of this slice iterator.
            // This is safe because we’ve called SmallVec::set_len(0) above,
            // so SmallVec::drop won’t drop this simple selector.
            unsafe { Some(ptr::read(simple_selector_ref)) }
        } else {
            self.combinators.next().map(|(combinator, len)| {
                let (rest, current) = split_from_end(self.rest_of_simple_selectors, len);
                self.rest_of_simple_selectors = rest;
                self.current_simple_selectors = current.iter();
                Component::Combinator(combinator)
            })
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len(), Some(self.len()))
    }
}

fn split_from_end<T>(s: &[T], at: usize) -> (&[T], &[T]) {
    s.split_at(s.len() - at)
}

bitflags! {
    /// Flags that indicate at which point of parsing a selector are we.
    #[derive(Default)]
    #[cfg_attr(feature = "shmem", derive(ToShmem))]
    pub (crate) struct SelectorFlags : u8 {
        const HAS_PSEUDO = 1 << 0;
        const HAS_SLOTTED = 1 << 1;
        const HAS_PART = 1 << 2;
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[cfg_attr(feature = "shmem", derive(ToShmem))]
pub struct SpecificityAndFlags {
    /// There are two free bits here, since we use ten bits for each specificity
    /// kind (id, class, element).
    pub(crate) specificity: u32,
    /// There's padding after this field due to the size of the flags.
    pub(crate) flags: SelectorFlags,
}

impl SpecificityAndFlags {
    #[inline]
    pub fn specificity(&self) -> u32 {
        self.specificity
    }

    #[inline]
    pub fn has_pseudo_element(&self) -> bool {
        self.flags.intersects(SelectorFlags::HAS_PSEUDO)
    }

    #[inline]
    pub fn is_slotted(&self) -> bool {
        self.flags.intersects(SelectorFlags::HAS_SLOTTED)
    }

    #[inline]
    pub fn is_part(&self) -> bool {
        self.flags.intersects(SelectorFlags::HAS_PART)
    }
}

const MAX_10BIT: u32 = (1u32 << 10) - 1;

#[derive(Add, AddAssign, Clone, Copy, Default, Eq, Ord, PartialEq, PartialOrd)]
struct Specificity {
    id_selectors: u32,
    class_like_selectors: u32,
    element_selectors: u32,
}

impl From<u32> for Specificity {
    #[inline]
    fn from(value: u32) -> Specificity {
        assert!(value <= MAX_10BIT << 20 | MAX_10BIT << 10 | MAX_10BIT);
        Specificity {
            id_selectors: value >> 20,
            class_like_selectors: (value >> 10) & MAX_10BIT,
            element_selectors: value & MAX_10BIT,
        }
    }
}

impl From<Specificity> for u32 {
    #[inline]
    fn from(specificity: Specificity) -> u32 {
        cmp::min(specificity.id_selectors, MAX_10BIT) << 20 |
            cmp::min(specificity.class_like_selectors, MAX_10BIT) << 10 |
            cmp::min(specificity.element_selectors, MAX_10BIT)
    }
}

fn specificity<Impl>(iter: slice::Iter<Component<Impl>>) -> u32
where
    Impl: SelectorImpl,
{
    complex_selector_specificity(iter).into()
}

fn complex_selector_specificity<Impl>(iter: slice::Iter<Component<Impl>>) -> Specificity
where
    Impl: SelectorImpl,
{
    fn simple_selector_specificity<Impl>(
        simple_selector: &Component<Impl>,
        specificity: &mut Specificity,
    ) where
        Impl: SelectorImpl,
    {
        match *simple_selector {
            Component::Combinator(..) => {
                unreachable!("Found combinator in simple selectors vector?");
            },
            Component::Part(..) | Component::PseudoElement(..) | Component::LocalName(..) => {
                specificity.element_selectors += 1
            },
            Component::Slotted(ref selector) => {
                specificity.element_selectors += 1;
                // Note that due to the way ::slotted works we only compete with
                // other ::slotted rules, so the above rule doesn't really
                // matter, but we do it still for consistency with other
                // pseudo-elements.
                //
                // See: https://github.com/w3c/csswg-drafts/issues/1915
                *specificity += Specificity::from(selector.specificity());
            },
            Component::Host(ref selector) => {
                specificity.class_like_selectors += 1;
                if let Some(ref selector) = *selector {
                    // See: https://github.com/w3c/csswg-drafts/issues/1915
                    *specificity += Specificity::from(selector.specificity());
                }
            },
            Component::ID(..) => {
                specificity.id_selectors += 1;
            },
            Component::Class(..) |
            Component::AttributeInNoNamespace { .. } |
            Component::AttributeInNoNamespaceExists { .. } |
            Component::AttributeOther(..) |
            Component::FirstChild |
            Component::LastChild |
            Component::OnlyChild |
            Component::Root |
            Component::Empty |
            Component::Scope |
            Component::NthChild(..) |
            Component::NthLastChild(..) |
            Component::NthOfType(..) |
            Component::NthLastOfType(..) |
            Component::FirstOfType |
            Component::LastOfType |
            Component::OnlyOfType |
            Component::NonTSPseudoClass(..) => {
                specificity.class_like_selectors += 1;
            },
            Component::Negation(ref list) | Component::Is(ref list) => {
                // https://drafts.csswg.org/selectors/#specificity-rules:
                //
                //     The specificity of an :is() pseudo-class is replaced by the
                //     specificity of the most specific complex selector in its
                //     selector list argument.
                let mut max = 0;
                for selector in &**list {
                    max = std::cmp::max(selector.specificity(), max);
                }
                *specificity += Specificity::from(max);
            },
            Component::Where(..) |
            Component::ExplicitUniversalType |
            Component::ExplicitAnyNamespace |
            Component::ExplicitNoNamespace |
            Component::DefaultNamespace(..) |
            Component::Namespace(..) => {
                // Does not affect specificity
            },
        }
    }

    let mut specificity = Default::default();
    for simple_selector in iter {
        simple_selector_specificity(&simple_selector, &mut specificity);
    }
    specificity
}
