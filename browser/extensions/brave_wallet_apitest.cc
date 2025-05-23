/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "base/environment.h"
#include "base/path_service.h"
#include "brave/browser/ethereum_remote_client/ethereum_remote_client_constants.h"
#include "brave/browser/ethereum_remote_client/pref_names.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/pref_names.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/components/constants/brave_paths.h"
#include "brave/components/constants/pref_names.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/constants.h"
#include "extensions/test/result_catcher.h"

namespace extensions {
namespace {

class BraveWalletExtensionApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    base::PathService::Get(brave::DIR_TEST_DATA, &extension_dir_);
    extension_dir_ = extension_dir_.AppendASCII("extensions/api_test");
  }
  PrefService* GetPrefs() { return browser()->profile()->GetPrefs(); }
  base::FilePath extension_dir_;
};

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       BraveExtensionWithWalletHasAccess) {
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveShieldsWithWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), brave_extension_id, "testBasics()"));
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest, BraveWalletAPIAvailable) {
  auto env = base::Environment::Create();
  env->SetVar("BRAVE_INFURA_PROJECT_ID", "test-project-id");
  env->SetVar("BRAVE_SERVICES_KEY", "test-brave-key");
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), kEthereumRemoteClientExtensionId, "testBasics()"));
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       BraveWalletAPIKnownValuesTest) {
  GetPrefs()->SetString(kERCAES256GCMSivNonce, "yJngKDr5nCGYz7EM");
  GetPrefs()->SetString(
      kERCEncryptedSeed,
      "IQu5fUMbXG6E7v8ITwcIKL3TI3rst0LU1US7ZxCKpgAGgLNAN6DbCN7nMF2Eg7Kx");
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), kEthereumRemoteClientExtensionId,
      "testKnownSeedValuesEndToEnd()"));
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       BraveWalletWeb3ProviderCryptoWallets) {
  brave_wallet::SetDefaultEthereumWallet(
      GetPrefs(), brave_wallet::mojom::DefaultWallet::CryptoWallets);
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), kEthereumRemoteClientExtensionId,
      "testProviderIsCryptoWallets()"));
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       BraveWalletWeb3ProviderIsBraveWalletPreferExtension) {
  brave_wallet::SetDefaultEthereumWallet(
      GetPrefs(),
      brave_wallet::mojom::DefaultWallet::BraveWalletPreferExtension);
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), kEthereumRemoteClientExtensionId,
      "testProviderIsBraveWalletPreferExtension()"));
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       BraveWalletWeb3ProviderNone) {
  brave_wallet::SetDefaultEthereumWallet(
      GetPrefs(), brave_wallet::mojom::DefaultWallet::None);
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), kEthereumRemoteClientExtensionId,
      "testProviderIsNone()"));
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       BraveWalletWeb3ProviderBraveWallet) {
  brave_wallet::SetDefaultEthereumWallet(
      GetPrefs(), brave_wallet::mojom::DefaultWallet::BraveWallet);
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser()->profile(), kEthereumRemoteClientExtensionId,
      "testProviderIsBraveWallet()"));
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       BraveWalletAPINotAvailable) {
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("notBraveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       ChangeTypeCryptoWalletsToBraveWallet) {
  brave_wallet::SetDefaultEthereumWallet(
      browser()->profile()->GetPrefs(),
      brave_wallet::mojom::DefaultWallet::CryptoWallets);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("brave://settings/")));

  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extensions::ExtensionRegistrar::Get(browser()->profile())
                  ->IsExtensionEnabled(kEthereumRemoteClientExtensionId));
  brave_wallet::SetDefaultEthereumWallet(
      browser()->profile()->GetPrefs(),
      brave_wallet::mojom::DefaultWallet::BraveWallet);
  ASSERT_FALSE(extensions::ExtensionRegistrar::Get(browser()->profile())
                   ->IsExtensionEnabled(kEthereumRemoteClientExtensionId));
}

IN_PROC_BROWSER_TEST_F(BraveWalletExtensionApiTest,
                       ChangeTypeCryptoWalletsToBraveWalletPreferExtension) {
  brave_wallet::SetDefaultEthereumWallet(
      browser()->profile()->GetPrefs(),
      brave_wallet::mojom::DefaultWallet::CryptoWallets);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("brave://settings/")));

  const Extension* extension =
      LoadExtension(extension_dir_.AppendASCII("braveWallet"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extensions::ExtensionRegistrar::Get(browser()->profile())
                  ->IsExtensionEnabled(kEthereumRemoteClientExtensionId));
  brave_wallet::SetDefaultEthereumWallet(
      browser()->profile()->GetPrefs(),
      brave_wallet::mojom::DefaultWallet::BraveWalletPreferExtension);
  ASSERT_FALSE(extensions::ExtensionRegistrar::Get(browser()->profile())
                   ->IsExtensionEnabled(kEthereumRemoteClientExtensionId));
}

}  // namespace
}  // namespace extensions
