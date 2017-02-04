// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace {

using WeakToolbarActions = std::vector<ToolbarActionViewController*>;

}

ToolbarActionsBar::PlatformSettings::PlatformSettings()
{
}

ToolbarActionsBar::ToolbarActionsBar(ToolbarActionsBarDelegate* delegate,
                                     Browser* browser,
                                     ToolbarActionsBar* main_bar):
      delegate_(delegate),
      browser_(browser),
      model_(ToolbarActionsModel::Get(browser_->profile())),
      main_bar_(main_bar),
      platform_settings_(),
      popup_owner_(nullptr),
      model_observer_(this),
      suppress_layout_(false),
      suppress_animation_(true),
      should_check_extension_bubble_(!main_bar),
      is_drag_in_progress_(false),
      popped_out_action_(nullptr),
      is_popped_out_sticky_(false),
      is_showing_bubble_(false),
      tab_strip_observer_(this),
      weak_ptr_factory_(this)
{
}

ToolbarActionsBar::~ToolbarActionsBar() {
}

// static
int ToolbarActionsBar::IconWidth(bool include_padding) {
  return 0;
}

// static
int ToolbarActionsBar::IconHeight() {
#if defined(OS_MACOSX)
  // On the Mac, the spec is a 24x24 button in a 28x28 space.
  return 24;
#else
  return 28;
#endif
}

// static
void ToolbarActionsBar::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
}

gfx::Size ToolbarActionsBar::GetPreferredSize() const {
    return gfx::Size();
  

}

int ToolbarActionsBar::GetMinimumWidth() const {
  return 0;
}

int ToolbarActionsBar::GetMaximumWidth() const {
  return 0;
}

int ToolbarActionsBar::IconCountToWidth(int icons) const {
  return 0;
}

size_t ToolbarActionsBar::WidthToIconCount(int pixels) const {
  return 0;
}

size_t ToolbarActionsBar::GetIconCount() const {

    return 0;
}

size_t ToolbarActionsBar::GetStartIndexInBounds() const {
  return  0;
}

size_t ToolbarActionsBar::GetEndIndexInBounds() const {
  return 0;
}

bool ToolbarActionsBar::NeedsOverflow() const {
  return false;
}

gfx::Rect ToolbarActionsBar::GetFrameForIndex(
    size_t index) const {
  
    return gfx::Rect();
}

std::vector<ToolbarActionViewController*>
ToolbarActionsBar::GetActions() const {
  std::vector<ToolbarActionViewController*> actions = toolbar_actions_.get();

  return actions;
}

void ToolbarActionsBar::CreateActions() {
}

bool ToolbarActionsBar::ShowToolbarActionPopup(const std::string& action_id,
                                               bool grant_active_tab) {
  return true;
}

void ToolbarActionsBar::SetOverflowRowWidth(int width) {
}

void ToolbarActionsBar::OnResizeComplete(int width) {
}

void ToolbarActionsBar::OnDragStarted() {
}

void ToolbarActionsBar::OnDragEnded() {
}

void ToolbarActionsBar::OnDragDrop(int dragged_index,
                                   int dropped_index,
                                   DragType drag_type) {
}

void ToolbarActionsBar::OnAnimationEnded() {
}

void ToolbarActionsBar::OnBubbleClosed() {
}

bool ToolbarActionsBar::IsActionVisibleOnMainBar(
    const ToolbarActionViewController* action) const {
  return false;
}

void ToolbarActionsBar::PopOutAction(ToolbarActionViewController* controller,
                                     bool is_sticky,
                                     const base::Closure& closure) {
}

void ToolbarActionsBar::UndoPopOut() {
}

void ToolbarActionsBar::SetPopupOwner(
    ToolbarActionViewController* popup_owner) {
}

void ToolbarActionsBar::HideActivePopup() {
}

ToolbarActionViewController* ToolbarActionsBar::GetMainControllerForAction(
    ToolbarActionViewController* action) {
  return nullptr;
}

void ToolbarActionsBar::AddObserver(ToolbarActionsBarObserver* observer) {
}

void ToolbarActionsBar::RemoveObserver(ToolbarActionsBarObserver* observer) {
}

void ToolbarActionsBar::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
}

void ToolbarActionsBar::ShowToolbarActionBubbleAsync(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> bubble) {
}

void ToolbarActionsBar::MaybeShowExtensionBubble() {
}

// static
void ToolbarActionsBar::set_extension_bubble_appearance_wait_time_for_testing(
    int time_in_seconds) {
}

void ToolbarActionsBar::OnToolbarActionAdded(
    const ToolbarActionsModel::ToolbarItem& item,
    int index) {
}

void ToolbarActionsBar::OnToolbarActionRemoved(const std::string& action_id) {
}

void ToolbarActionsBar::OnToolbarActionMoved(const std::string& action_id,
                                             int index) {
}

void ToolbarActionsBar::OnToolbarActionUpdated(const std::string& action_id) {
}

void ToolbarActionsBar::OnToolbarVisibleCountChanged() {
}

void ToolbarActionsBar::ResizeDelegate(gfx::Tween::Type tween_type,
                                       bool suppress_chevron) {
}

void ToolbarActionsBar::OnToolbarHighlightModeChanged(bool is_highlighting) {
}

void ToolbarActionsBar::OnToolbarModelInitialized() {
}

void ToolbarActionsBar::TabInsertedAt(TabStripModel* tab_strip_model,
                                      content::WebContents* contents,
                                      int index,
                                      bool foreground) {
}

void ToolbarActionsBar::ReorderActions() {
}

ToolbarActionViewController* ToolbarActionsBar::GetActionForId(const std::string& action_id) {
  return nullptr;
}

content::WebContents* ToolbarActionsBar::GetCurrentWebContents() {
  return nullptr;
}



