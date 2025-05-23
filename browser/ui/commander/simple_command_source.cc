// Copyright (c) 2023 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/browser/ui/commander/simple_command_source.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "brave/app/brave_command_ids.h"
#include "brave/app/command_utils.h"
#include "brave/browser/misc_metrics/profile_misc_metrics_service.h"
#include "brave/browser/misc_metrics/profile_misc_metrics_service_factory.h"
#include "brave/browser/ui/commander/command_source.h"
#include "brave/browser/ui/commander/fuzzy_finder.h"
#include "brave/components/ai_chat/core/browser/ai_chat_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "ui/base/accelerators/accelerator.h"

namespace commander {

namespace {

void MaybeReportCommandExecution(Browser* browser, int command_id) {
  if (command_id == IDC_TOGGLE_AI_CHAT) {
    auto* profile_metrics =
        misc_metrics::ProfileMiscMetricsServiceFactory::GetServiceForContext(
            browser->profile());
    if (!profile_metrics) {
      return;
    }
    auto* ai_chat_metrics = profile_metrics->GetAIChatMetrics();
    if (!ai_chat_metrics) {
      return;
    }
    ai_chat_metrics->HandleOpenViaEntryPoint(
        ai_chat::EntryPoint::kOmniboxCommand);
  }
}

}  // namespace

SimpleCommandSource::SimpleCommandSource() = default;
SimpleCommandSource::~SimpleCommandSource() = default;

CommandSource::CommandResults SimpleCommandSource::GetCommands(
    const std::u16string& input,
    Browser* browser) const {
  CommandSource::CommandResults results;
  if (!browser || !browser->command_controller()) {
    return results;
  }

  auto commands = commands::GetCommands();
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;

  for (const int command_id : commands) {
    if (!chrome::IsCommandEnabled(browser, command_id)) {
      continue;
    }

    std::u16string name =
        base::UTF8ToUTF16(commands::GetCommandName(command_id));

    double score = finder.Find(name, ranges);
    if (score == 0) {
      continue;
    }

    auto item = std::make_unique<CommandItem>(name, score, ranges);
    ui::Accelerator accelerator;
    ui::AcceleratorProvider* provider =
        chrome::AcceleratorProviderForBrowser(browser);
    if (provider->GetAcceleratorForCommandId(command_id, &accelerator)) {
      item->annotation = accelerator.GetShortcutText();
    }

    item->command = base::BindOnce(
        [](Browser* browser, int command_id) {
          MaybeReportCommandExecution(browser, command_id);
          chrome::ExecuteCommand(browser, command_id);
        },
        // Unretained is safe here, as the commands will be reset if the browser
        // is closed.
        base::Unretained(browser), command_id);

    results.push_back(std::move(item));
  }

  return results;
}

}  // namespace commander
