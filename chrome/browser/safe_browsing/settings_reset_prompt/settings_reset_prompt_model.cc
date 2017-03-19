// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
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

// These values are used for UMA metrics reporting. New enum values can be
// added, but existing enums must never be renumbered or deleted and reused.
enum SettingsReset {
  SETTINGS_RESET_HOMEPAGE = 1,
  SETTINGS_RESET_DEFAULT_SEARCH = 2,
  SETTINGS_RESET_STARTUP_URLS = 3,
  SETTINGS_RESET_MAX,
};

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

Profile* SettingsResetPromptModel::profile() const {
  return profile_;
}

SettingsResetPromptConfig* SettingsResetPromptModel::config() const {
  return prompt_config_.get();
}

bool SettingsResetPromptModel::ShouldPromptForReset() const {
  return SomeSettingRequiresReset();
}

void SettingsResetPromptModel::PerformReset(
    const base::Closure& done_callback) {
}

void SettingsResetPromptModel::DialogShown() {
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

void SettingsResetPromptModel::ReportUmaMetrics() const {
  UMA_HISTOGRAM_BOOLEAN("SettingsResetPrompt.PromptRequired",
                        ShouldPromptForReset());
  UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ResetState_DefaultSearch",
                            default_search_reset_state(), RESET_STATE_MAX);
  UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ResetState_StartupUrls",
                            startup_urls_reset_state(), RESET_STATE_MAX);
  UMA_HISTOGRAM_ENUMERATION("SettingsResetPrompt.ResetState_Homepage",
                            homepage_reset_state(), RESET_STATE_MAX);
  UMA_HISTOGRAM_COUNTS_100("SettingsResetPrompt.NumberOfExtensionsToDisable",
                           extensions_to_disable().size());
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      "SettingsResetPrompt.DelayBeforePromptParam",
      prompt_config_->delay_before_prompt().InSeconds());
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
      prefs_manager_(profile, prompt_config->prompt_wave()),
      prompt_config_(std::move(prompt_config)),
      time_since_last_prompt_(base::Time::Now() -
                              prefs_manager_.LastTriggeredPrompt()),
      settings_types_initialized_(0),
      homepage_reset_domain_id_(-1),
      homepage_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      default_search_reset_domain_id_(-1),
      default_search_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED),
      startup_urls_reset_state_(NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED) {
  DCHECK(profile_);
  DCHECK(prompt_config_);

  InitDefaultSearchData();
  InitStartupUrlsData();
  InitHomepageData();
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_ALL);

  InitExtensionData();

  if (!SomeSettingRequiresReset())
    return;

  // For now, during the experimental phase, if policy controls any of the
  // settings that we consider for reset (search, startup pages, homepage) or if
  // an extension that needs to be disabled is managed by policy, then we do not
  // show the reset prompt.
  //
  // TODO(alito): Consider how clients with policies should be prompted for
  // reset.
  if (SomeSettingIsManaged() || SomeExtensionMustRemainEnabled()) {
    if (homepage_reset_state_ == RESET_REQUIRED)
      homepage_reset_state_ = NO_RESET_REQUIRED_DUE_TO_POLICY;
    if (default_search_reset_state_ == RESET_REQUIRED)
      default_search_reset_state_ = NO_RESET_REQUIRED_DUE_TO_POLICY;
    if (startup_urls_reset_state_ == RESET_REQUIRED)
      startup_urls_reset_state_ = NO_RESET_REQUIRED_DUE_TO_POLICY;
  }
}

void SettingsResetPromptModel::InitDefaultSearchData() {
  // Default search data must be the first setting type to be initialized.
  DCHECK_EQ(settings_types_initialized_, 0U);

  settings_types_initialized_ |= SETTINGS_TYPE_DEFAULT_SEARCH;

  if (default_search_reset_domain_id_ < 0)
    return;

  default_search_reset_state_ = GetResetStateForSetting(
      prefs_manager_.LastTriggeredPromptForDefaultSearch());
}

void SettingsResetPromptModel::InitStartupUrlsData() {
  // Default search data must have been initialized before startup URLs data.
  DCHECK_EQ(settings_types_initialized_, SETTINGS_TYPE_DEFAULT_SEARCH);

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

void SettingsResetPromptModel::InitHomepageData() {
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

SettingsResetPromptModel::ResetState
SettingsResetPromptModel::GetResetStateForSetting(
    const base::Time& last_triggered_for_setting) const {
  if (!last_triggered_for_setting.is_null())
    return NO_RESET_REQUIRED_DUE_TO_ALREADY_PROMPTED_FOR_SETTING;

  if (time_since_last_prompt_ < prompt_config_->time_between_prompts())
    return NO_RESET_REQUIRED_DUE_TO_RECENTLY_PROMPTED;

  if (SomeSettingRequiresReset())
    return NO_RESET_REQUIRED_DUE_TO_OTHER_SETTING_REQUIRING_RESET;

  return RESET_REQUIRED;
}

bool SettingsResetPromptModel::SomeSettingRequiresReset() const {
  return default_search_reset_state_ == RESET_REQUIRED ||
         startup_urls_reset_state_ == RESET_REQUIRED ||
         homepage_reset_state_ == RESET_REQUIRED;
}

bool SettingsResetPromptModel::SomeSettingIsManaged() const {
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);

  // Check if homepage is managed.
  const PrefService::Preference* homepage =
      prefs->FindPreference(prefs::kHomePage);
  if (homepage && (homepage->IsManaged() || homepage->IsManagedByCustodian()))
    return true;

  // Check if startup pages are managed.
  if (SessionStartupPref::TypeIsManaged(prefs) ||
      SessionStartupPref::URLsAreManaged(prefs)) {
    return true;
  }

  // Check if default search is managed.
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (service && service->is_default_search_managed())
    return true;

  return false;
}

bool SettingsResetPromptModel::SomeExtensionMustRemainEnabled() const {
  return false;
}

}  // namespace safe_browsing.
