/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_WEBUI_BRAVE_SETTINGS_UI_H_
#define BRAVE_BROWSER_UI_WEBUI_BRAVE_SETTINGS_UI_H_

#include <memory>

#include "brave/components/ai_chat/core/common/mojom/settings_helper.mojom.h"
#include "brave/components/brave_account/core/mojom/brave_account.mojom.h"
#include "brave/components/commands/common/commands.mojom.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class BraveSettingsUI;

namespace brave_account {
class BraveAccountHandler;
}

namespace content {
class WebUIDataSource;
}

class Profile;

class BraveSettingsUIConfig
    : public content::DefaultWebUIConfig<BraveSettingsUI> {
 public:
  BraveSettingsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISettingsHost) {}
};

class BraveSettingsUI : public settings::SettingsUI {
 public:
  explicit BraveSettingsUI(content::WebUI* web_ui);
  BraveSettingsUI(const BraveSettingsUI&) = delete;
  BraveSettingsUI& operator=(const BraveSettingsUI&) = delete;
  ~BraveSettingsUI() override;

  static void AddResources(content::WebUIDataSource* html_source,
                           Profile* profile);
  static bool& ShouldExposeElementsForTesting();

  void BindInterface(
      mojo::PendingReceiver<commands::mojom::CommandsService> pending_receiver);
  void BindInterface(mojo::PendingReceiver<ai_chat::mojom::AIChatSettingsHelper>
                         pending_receiver);
  void BindInterface(
      mojo::PendingReceiver<brave_account::mojom::BraveAccountHandler>
          pending_receiver);

 private:
  std::unique_ptr<brave_account::BraveAccountHandler> brave_account_handler_;
};

#endif  // BRAVE_BROWSER_UI_WEBUI_BRAVE_SETTINGS_UI_H_
