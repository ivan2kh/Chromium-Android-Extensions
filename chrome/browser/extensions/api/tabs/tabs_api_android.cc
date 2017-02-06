// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/browser/file_reader.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/host_id.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "net/base/escape.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/ui_base_types.h"

#if defined(USE_ASH)
#include "chrome/browser/extensions/api/tabs/ash_panel_contents.h"  // nogncheck
#include "extensions/browser/app_window/app_window_registry.h"  // nogncheck
#endif

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using zoom::ZoomController;

namespace extensions {

namespace windows = api::windows;
namespace keys = tabs_constants;
namespace tabs = api::tabs;

using api::extension_types::InjectDetails;

void ZoomModeToZoomSettings(ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_MANUAL;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_DISABLED;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
  }
}

// Windows ---------------------------------------------------------------------

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));
}

bool WindowsCreateFunction::ShouldOpenIncognitoWindow(
    const windows::Create::Params::CreateData* create_data,
    std::vector<GURL>* urls,
    std::string* error) {
  return false;
}

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {  
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

// Tabs ------------------------------------------------------------------------

ExtensionFunction::ResponseAction TabsGetSelectedFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsGetAllInWindowFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

bool TabsHighlightFunction::HighlightTab(TabStripModel* tabstrip,
                                         ui::ListSelectionModel* selection,
                                         int* active_index,
                                         int index,
                                         std::string* error) {
    return false;
}

TabsUpdateFunction::TabsUpdateFunction() : web_contents_(NULL) {
}

bool TabsUpdateFunction::RunAsync() {

  return true;
}

bool TabsUpdateFunction::UpdateURL(const std::string &url_string,
                                   int tab_id,
                                   bool* is_async) {

  return true;
}

void TabsUpdateFunction::PopulateResult() {
    return;

}

void TabsUpdateFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& url,
    const base::ListValue& script_result) {
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

bool TabsMoveFunction::MoveTab(int tab_id,
                               int* new_index,
                               int iteration,
                               base::ListValue* tab_values,
                               int* window_id,
                               std::string* error) {
    return false;

}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {  
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

ExtensionFunction::ResponseAction TabsRemoveFunction::Run() {
  return RespondNow(Error(kUnknownErrorDoNotUse));

}

bool TabsRemoveFunction::RemoveTab(int tab_id, std::string* error) {

  return true;
}

TabsCaptureVisibleTabFunction::TabsCaptureVisibleTabFunction()
    : chrome_details_(this) {
}

bool TabsCaptureVisibleTabFunction::HasPermission() {
  return true;
}

bool TabsCaptureVisibleTabFunction::IsScreenshotEnabled() {
  return true;
}

bool TabsCaptureVisibleTabFunction::ClientAllowsTransparency() {
  return false;
}

WebContents* TabsCaptureVisibleTabFunction::GetWebContentsForID(int window_id) {
    return NULL;
}

bool TabsCaptureVisibleTabFunction::RunAsync() {
  return false;
}

void TabsCaptureVisibleTabFunction::OnCaptureSuccess(const SkBitmap& bitmap) {
}

void TabsCaptureVisibleTabFunction::OnCaptureFailure(FailureReason reason) {
}

void TabsCaptureVisibleTabFunction::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {;
}

bool TabsDetectLanguageFunction::RunAsync() {
  return true;
}

void TabsDetectLanguageFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
}

void TabsDetectLanguageFunction::GotLanguage(const std::string& language) {
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : chrome_details_(this), execute_tab_id_(-1) {
}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

bool ExecuteCodeInTabFunction::HasPermission() {
    return true;
  
}

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
    return set_init_result(VALIDATION_FAILURE);;

}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage() {
  execute_tab_id_ = true;
    return false;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor() {
  return nullptr;
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool TabsExecuteScriptFunction::ShouldInsertCSS() const {
  return false;
}

void TabsExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& on_url,
    const base::ListValue& result) {
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

content::WebContents* ZoomAPIFunction::GetWebContents(int tab_id) {
  return nullptr;
}

bool TabsSetZoomFunction::RunAsync() {
  return true;
}

bool TabsGetZoomFunction::RunAsync() {
  return true;
}

bool TabsSetZoomSettingsFunction::RunAsync() {
  return true;
}

bool TabsGetZoomSettingsFunction::RunAsync() {
  return true;
}

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
    return RespondNow(Error(kUnknownErrorDoNotUse));

}

TabsDiscardFunction::TabsDiscardFunction() {}
TabsDiscardFunction::~TabsDiscardFunction() {}

}  // namespace extensions
