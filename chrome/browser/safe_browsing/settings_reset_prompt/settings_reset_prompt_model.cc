// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

namespace safe_browsing {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
constexpr char kOmahaUrl[] = "https://tools.google.com/service/update2";
#endif  // defined(GOOGLE_CHROME_BUILD)

// Used to keep track of which settings types have been initialized in
// |SettingsResetPromptModel|.
enum SettingsType : uint32_t {
  SETTINGS_TYPE_HOMEPAGE = 1 << 0,
  SETTINGS_TYPE_DEFAULT_SEARCH = 1 << 1,
  SETTINGS_TYPE_STARTUP_URLS = 1 << 2,
  SETTINGS_TYPE_ALL = SETTINGS_TYPE_HOMEPAGE | SETTINGS_TYPE_DEFAULT_SEARCH |
                      SETTINGS_TYPE_STARTUP_URLS,
};

}  // namespace

// static
void SettingsResetPromptModel::Create(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    CreateCallback callback){
}

// static
std::unique_ptr<SettingsResetPromptModel>
SettingsResetPromptModel::CreateForTesting(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings,
    std::unique_ptr<ProfileResetter> profile_resetter) {
  return base::WrapUnique(new SettingsResetPromptModel(
      profile, std::move(prompt_config), std::move(settings_snapshot),
      std::move(default_settings), std::move(profile_resetter)));
}

SettingsResetPromptModel::~SettingsResetPromptModel() {}

SettingsResetPromptConfig* SettingsResetPromptModel::config() const {
  return prompt_config_.get();
}

bool SettingsResetPromptModel::ShouldPromptForReset() const {
  return homepage_reset_state() == RESET_REQUIRED ||
         default_search_reset_state() == RESET_REQUIRED ||
         startup_urls_reset_state() == RESET_REQUIRED;
}

GURL SettingsResetPromptModel::homepage() const {
  return GURL();
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::homepage_reset_state() const {
  DCHECK(homepage_reset_state_ != RESET_REQUIRED ||
         homepage_reset_domain_id_ >= 0);
  return homepage_reset_state_;
}

GURL SettingsResetPromptModel::default_search() const {
  return GURL();//settings_snapshot_->dse_url();
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::default_search_reset_state() const {
  DCHECK(default_search_reset_state_ != RESET_REQUIRED ||
         default_search_reset_domain_id_ >= 0);
  return default_search_reset_state_;
}

const std::vector<GURL>& SettingsResetPromptModel::startup_urls() const {
  return startup_urls_;
}

const std::vector<GURL>& SettingsResetPromptModel::startup_urls_to_reset()
    const {
  return startup_urls_to_reset_;
}

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::startup_urls_reset_state() const {
  return startup_urls_reset_state_;
}

const SettingsResetPromptModel::ExtensionMap&
SettingsResetPromptModel::extensions_to_disable() const {
  return extensions_to_disable_;
}

// static
void SettingsResetPromptModel::OnSettingsFetched(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    SettingsResetPromptModel::CreateCallback callback,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings) {
  DCHECK(profile);
  DCHECK(prompt_config);
  DCHECK(default_settings);

  callback.Run(base::WrapUnique(new SettingsResetPromptModel(
      profile, std::move(prompt_config),
      base::MakeUnique<ResettableSettingsSnapshot>(profile),
      std::move(default_settings),
      base::MakeUnique<ProfileResetter>(profile))));
}

SettingsResetPromptModel::SettingsResetPromptModel(
    Profile* profile,
    std::unique_ptr<SettingsResetPromptConfig> prompt_config,
    std::unique_ptr<ResettableSettingsSnapshot> settings_snapshot,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings,
    std::unique_ptr<ProfileResetter> profile_resetter)
    : profile_(profile),
      prompt_config_(std::move(prompt_config)),
      settings_types_initialized_(0),
      homepage_reset_domain_id_(-1),
      homepage_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      default_search_reset_domain_id_(-1),
      default_search_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      startup_urls_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED) {
  DCHECK(profile_);
  DCHECK(prompt_config_);

  InitHomepageData();
  InitDefaultSearchData();
  InitStartupUrlsData();
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_ALL);

  InitExtensionData();

  // TODO(alito): Figure out cases where settings cannot be reset, for example
  // due to policy or extensions that cannot be disabled.
}

void SettingsResetPromptModel::InitHomepageData() {
  DCHECK(!(settings_types_initialized_ & SETTINGS_TYPE_HOMEPAGE));

  settings_types_initialized_ |= SETTINGS_TYPE_HOMEPAGE;

  // If the home button is not visible to the user, then the homepage setting
  // has no real user-visible effect.
//  if (!settings_snapshot_->show_home_button())
//    return;

  // We do not currently support resetting New Tab pages that are set by
  // extensions.
//  if (settings_snapshot_->homepage_is_ntp())
//    return;

  homepage_reset_state_ = RESET_REQUIRED;
}

void SettingsResetPromptModel::InitDefaultSearchData() {
  DCHECK(!(settings_types_initialized_ & SETTINGS_TYPE_DEFAULT_SEARCH));

  settings_types_initialized_ |= SETTINGS_TYPE_DEFAULT_SEARCH;

  if (default_search_reset_domain_id_ < 0)
    return;

  default_search_reset_state_ = RESET_REQUIRED;
}

void SettingsResetPromptModel::InitStartupUrlsData() {
  DCHECK(!(settings_types_initialized_ & SETTINGS_TYPE_STARTUP_URLS));

  settings_types_initialized_ |= SETTINGS_TYPE_STARTUP_URLS;

  // Only the URLS startup type is a candidate for resetting.
//  if (settings_snapshot_->startup_type() == SessionStartupPref::URLS) {
//    startup_urls_ = settings_snapshot_->startup_urls();
//    for (const GURL& startup_url : settings_snapshot_->startup_urls()) {
//      int reset_domain_id = prompt_config_->UrlToResetDomainId(startup_url);
//      if (reset_domain_id >= 0) {
//        startup_urls_reset_state_ = RESET_REQUIRED;
//        startup_urls_to_reset_.push_back(startup_url);
//        domain_ids_for_startup_urls_to_reset_.insert(reset_domain_id);
//      }
//    }
//  }
}

// Populate |extensions_to_disable_| with all enabled extensions that override
// the settings whose values were determined to need resetting. Note that all
// extensions that override such settings are included in the list, not just the
// one that is currently actively overriding the setting, in order to ensure
// that default values can be restored. This function should be called after
// other Init*() functions.
void SettingsResetPromptModel::InitExtensionData() {
//  DCHECK(settings_snapshot_);
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_ALL);

  // |enabled_extensions()| is a container of [id, name] pairs.
//  for (const auto& id_name : settings_snapshot_->enabled_extensions()) {
//    // Just in case there are duplicates in the list of enabled extensions.
//    if (extensions_to_disable_.find(id_name.first) !=
//        extensions_to_disable_.end()) {
//      continue;
//    }
//
//    const extensions::Extension* extension =
//        GetExtension(profile_, id_name.first);
//    if (!extension)
//      continue;
//
//    const extensions::SettingsOverrides* overrides =
//        extensions::SettingsOverrides::Get(extension);
//    if (!overrides)
//      continue;
//
//    if ((homepage_reset_state_ == RESET_REQUIRED && overrides->homepage) ||
//        (default_search_reset_state_ == RESET_REQUIRED &&
//         overrides->search_engine) ||
//        (startup_urls_reset_state_ == RESET_REQUIRED &&
//         !overrides->startup_pages.empty())) {
//      ExtensionInfo extension_info(extension);
//      extensions_to_disable_.insert(
//          std::make_pair(extension_info.id, extension_info));
//    }
//  }
}

}  // namespace safe_browsing.
