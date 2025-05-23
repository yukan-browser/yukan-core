/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_ads/core/internal/common/test/internal/local_state_pref_storage_test_util_internal.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "brave/components/brave_ads/core/internal/common/test/internal/current_test_util_internal.h"
#include "brave/components/brave_ads/core/internal/common/test/internal/pref_value_test_info.h"

namespace brave_ads::test {

namespace {

base::flat_map</*uuid=*/std::string, PrefValueInfo>& LocalStatePrefStorage() {
  static base::NoDestructor<base::flat_map<std::string, PrefValueInfo>> prefs;
  return *prefs;
}

}  // namespace

bool FindLocalStatePref(const std::string& path) {
  const std::string uuid = GetUuidForCurrentTestAndValue(path);
  return LocalStatePrefStorage().contains(uuid);
}

PrefValueInfo& LocalStatePref(const std::string& path) {
  const std::string uuid = GetUuidForCurrentTestAndValue(path);
  return LocalStatePrefStorage()[uuid];
}

bool HasLocalStatePref(const std::string& path) {
  return LocalStatePrefStorage().contains(GetUuidForCurrentTestAndValue(path));
}

}  // namespace brave_ads::test
