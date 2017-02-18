// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"

#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/util.h"
#include "components/toolbar/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/vector_icons/vector_icons.h"

namespace {

// The touch bar's identifier.
const NSTouchBarCustomizationIdentifier kBrowserWindowTouchBarId =
    @"BrowserWindowTouchBarId";

// Touch bar items identifiers.
const NSTouchBarItemIdentifier kBackForwardTouchId = @"BackForwardTouchId";
const NSTouchBarItemIdentifier kReloadOrStopTouchId = @"ReloadOrStopTouchId";
const NSTouchBarItemIdentifier kSearchTouchId = @"SearchTouchId";
const NSTouchBarItemIdentifier kNewTabTouchId = @"NewTabTouchId";
const NSTouchBarItemIdentifier kStarTouchId = @"StarTouchId";

// The button indexes in the back and forward segment control.
const int kBackSegmentIndex = 0;
const int kForwardSegmentIndex = 1;

// Touch bar icon colors values.
const SkColor kTouchBarDefaultIconColor = SK_ColorWHITE;

// The size of the touch bar icons.
const int kTouchBarIconSize = 16;

// The width of the search button in the touch bar.
const int kTouchBarSearchButtonWidth = 280;

// Creates an NSImage from the given VectorIcon.
NSImage* CreateNSImageFromIcon(const gfx::VectorIcon& icon,
                               SkColor color = kTouchBarDefaultIconColor) {
  return NSImageFromImageSkiaWithColorSpace(
      gfx::CreateVectorIcon(icon, kTouchBarIconSize, color),
      base::mac::GetSRGBColorSpace());
}

// Creates a NSButton for the touch bar.
NSButton* CreateTouchBarButton(const gfx::VectorIcon& icon,
                               BrowserWindowTouchBar* owner,
                               int command,
                               SkColor color = kTouchBarDefaultIconColor) {
  NSButton* button =
      [NSButton buttonWithImage:CreateNSImageFromIcon(icon, color)
                         target:owner
                         action:@selector(executeCommand:)];
  button.tag = command;
  return button;
}

}  // namespace

@interface BrowserWindowTouchBar () {
  // Used to execute commands such as navigating back and forward.
  CommandUpdater* commandUpdater_;  // Weak, owned by Browser.

  // The browser associated with the touch bar.
  Browser* browser_;  // Weak.
}

// Creates and return the back and forward segmented buttons.
- (NSView*)backOrForwardTouchBarView;

// Creates and returns the search button.
- (NSView*)searchTouchBarView;
@end

@implementation BrowserWindowTouchBar

@synthesize isPageLoading = isPageLoading_;
@synthesize isStarred = isStarred_;

- (instancetype)initWithBrowser:(Browser*)browser {
  if ((self = [self init])) {
    DCHECK(browser);
    commandUpdater_ = browser->command_controller()->command_updater();
    browser_ = browser;
  }

  return self;
}

- (NSTouchBar*)makeTouchBar {
  if (!base::FeatureList::IsEnabled(features::kBrowserTouchBar))
    return nil;

  base::scoped_nsobject<NSTouchBar> touchBar(
      [[NSClassFromString(@"NSTouchBar") alloc] init]);
  NSArray* touchBarItemIdentifiers = @[
    kBackForwardTouchId, kReloadOrStopTouchId, kSearchTouchId, kNewTabTouchId,
    kStarTouchId
  ];
  [touchBar setCustomizationIdentifier:kBrowserWindowTouchBarId];
  [touchBar setDefaultItemIdentifiers:touchBarItemIdentifiers];
  [touchBar setCustomizationAllowedItemIdentifiers:touchBarItemIdentifiers];
  [touchBar setDelegate:self];

  return touchBar.autorelease();
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
  if (!touchBar)
    return nil;

  base::scoped_nsobject<NSCustomTouchBarItem> touchBarItem([[NSClassFromString(
      @"NSCustomTouchBarItem") alloc] initWithIdentifier:identifier]);
  if ([identifier isEqualTo:kBackForwardTouchId]) {
    [touchBarItem setView:[self backOrForwardTouchBarView]];
  } else if ([identifier isEqualTo:kReloadOrStopTouchId]) {
    const gfx::VectorIcon& icon =
        isPageLoading_ ? kNavigateStopIcon : kNavigateReloadIcon;
    int command_id = isPageLoading_ ? IDC_STOP : IDC_RELOAD;
    [touchBarItem setView:CreateTouchBarButton(icon, self, command_id)];
  } else if ([identifier isEqualTo:kNewTabTouchId]) {
    [touchBarItem setView:CreateTouchBarButton(kNewTabMacTouchbarIcon, self,
                                               IDC_NEW_TAB)];
  } else if ([identifier isEqualTo:kStarTouchId]) {
    const SkColor kStarredIconColor =
        ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
            ui::NativeTheme::kColorId_ProminentButtonColor);
    const gfx::VectorIcon& icon =
        isStarred_ ? toolbar::kStarActiveIcon : toolbar::kStarIcon;
    SkColor iconColor =
        isStarred_ ? kStarredIconColor : kTouchBarDefaultIconColor;
    [touchBarItem
        setView:CreateTouchBarButton(icon, self, IDC_BOOKMARK_PAGE, iconColor)];
  } else if ([identifier isEqualTo:kSearchTouchId]) {
    [touchBarItem setView:[self searchTouchBarView]];
  }

  return touchBarItem.autorelease();
}

- (NSView*)backOrForwardTouchBarView {
  NSArray* images = @[
    CreateNSImageFromIcon(ui::kBackArrowIcon),
    CreateNSImageFromIcon(ui::kForwardArrowIcon)
  ];

  NSSegmentedControl* control = [NSSegmentedControl
      segmentedControlWithImages:images
                    trackingMode:NSSegmentSwitchTrackingMomentary
                          target:self
                          action:@selector(backOrForward:)];
  control.segmentStyle = NSSegmentStyleSeparated;
  [control setEnabled:commandUpdater_->IsCommandEnabled(IDC_BACK)
           forSegment:kBackSegmentIndex];
  [control setEnabled:commandUpdater_->IsCommandEnabled(IDC_FORWARD)
           forSegment:kForwardSegmentIndex];
  return control;
}

- (NSView*)searchTouchBarView {
  // TODO(spqchan): Use the Google search icon if the default search engine is
  // Google.
  base::string16 title = l10n_util::GetStringUTF16(IDS_TOUCH_BAR_SEARCH);
  NSButton* searchButton =
      [NSButton buttonWithTitle:base::SysUTF16ToNSString(title)
                          image:CreateNSImageFromIcon(omnibox::kSearchIcon)
                         target:self
                         action:@selector(executeCommand:)];
  searchButton.imageHugsTitle = YES;
  searchButton.tag = IDC_FOCUS_LOCATION;
  [searchButton.widthAnchor
      constraintEqualToConstant:kTouchBarSearchButtonWidth]
      .active = YES;
  return searchButton;
}

- (void)backOrForward:(id)sender {
  NSSegmentedControl* control = sender;
  if ([control selectedSegment] == kBackSegmentIndex)
    commandUpdater_->ExecuteCommand(IDC_BACK);
  else
    commandUpdater_->ExecuteCommand(IDC_FORWARD);
}

- (void)executeCommand:(id)sender {
  commandUpdater_->ExecuteCommand([sender tag]);
}

@end