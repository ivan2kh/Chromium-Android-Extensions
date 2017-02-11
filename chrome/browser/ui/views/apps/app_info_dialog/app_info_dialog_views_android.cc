// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_container.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_panel.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/features.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/arc_app_info_links_panel.h"
#endif

namespace {

// The color of the separator used inside the dialog - should match the app
// list's app_list::kDialogSeparatorColor
//const SkColor kDialogSeparatorColor = SkColorSetRGB(0xD1, 0xD1, 0xD1);

#if defined(OS_MACOSX)
bool IsAppInfoDialogMacEnabled() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableAppInfoDialogMac))
    return false;
  if (command_line->HasSwitch(switches::kEnableAppInfoDialogMac))
    return true;
  return false;  // Current default.
}
#endif

}  // namespace

bool CanShowAppInfoDialog() {
#if defined(OS_MACOSX)
  static const bool can_show = IsAppInfoDialogMacEnabled();
  return can_show;
#else
  return true;
#endif
}

gfx::Size GetAppInfoNativeDialogSize() {
  return gfx::Size(380, 490);
}

#if BUILDFLAG(ENABLE_APP_LIST)
void ShowAppInfoInAppList(gfx::NativeWindow parent,
                          const gfx::Rect& app_list_bounds,
                          Profile* profile,
                          const extensions::Extension* app,
                          const base::Closure& close_callback) {
  views::View* app_info_view = new AppInfoDialog(parent, profile, app);
  views::DialogDelegate* dialog =
      CreateAppListContainerForView(app_info_view, close_callback);
  views::Widget* dialog_widget =
      constrained_window::CreateBrowserModalDialogViews(dialog, parent);
  dialog_widget->SetBounds(app_list_bounds);
  dialog_widget->Show();
}
#endif

void ShowAppInfoInNativeDialog(content::WebContents* web_contents,
                               const gfx::Size& size,
                               Profile* profile,
                               const extensions::Extension* app,
                               const base::Closure& close_callback) {

}
