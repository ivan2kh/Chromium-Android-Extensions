// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/url_constants.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
using base::UserMetricsAction;
using content::WebContents;

class TabStripModel::WebContentsData {
};

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

TabStripModel::TabStripModel(TabStripModelDelegate* delegate, Profile* profile)
    : 
      profile_(profile),
      closing_all_(false),
      in_notify_(false),
      weak_factory_(this) {
  
}

TabStripModel::~TabStripModel() {
}

void TabStripModel::AddObserver(TabStripModelObserver* observer) {

}

void TabStripModel::RemoveObserver(TabStripModelObserver* observer) {

}

bool TabStripModel::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModel::AppendWebContents(WebContents* contents,
                                      bool foreground) {
  InsertWebContentsAt(count(), contents,
                      foreground ? (ADD_INHERIT_GROUP | ADD_ACTIVE) :
                                   ADD_NONE);
}

void TabStripModel::InsertWebContentsAt(int index,
                                        WebContents* contents,
                                        int add_types) {
 
}

WebContents* TabStripModel::ReplaceWebContentsAt(int index,
                                                 WebContents* new_contents) {
 
  return nullptr;
}

WebContents* TabStripModel::DetachWebContentsAt(int index) {
 
  return nullptr;
}

void TabStripModel::ActivateTabAt(int index, bool user_gesture) {

}

void TabStripModel::AddTabAtToSelection(int index) {

}

void TabStripModel::MoveWebContentsAt(int index,
                                      int to_position,
                                      bool select_after_move) {
}

void TabStripModel::MoveSelectedTabsTo(int index) {

}

WebContents* TabStripModel::GetActiveWebContents() const {
  return nullptr;
}

WebContents* TabStripModel::GetWebContentsAt(int index) const {
  return nullptr;
}

int TabStripModel::GetIndexOfWebContents(const WebContents* contents) const {
  return kNoTab;
}

void TabStripModel::UpdateWebContentsStateAt(int index,
    TabStripModelObserver::TabChangeType change_type) {
}

void TabStripModel::CloseAllTabs() {
}

bool TabStripModel::CloseWebContentsAt(int index, uint32_t close_types) {
  return false;
}

bool TabStripModel::TabsAreLoading() const {
  return false;
}

WebContents* TabStripModel::GetOpenerOfWebContentsAt(int index) {
  DCHECK(ContainsIndex(index));
  return nullptr;
}

void TabStripModel::SetOpenerOfWebContentsAt(int index,
                                             WebContents* opener) {
}

int TabStripModel::GetIndexOfNextWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index,
                                                     bool use_group) const {
  return kNoTab;
}

int TabStripModel::GetIndexOfLastWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  return 0;
}

void TabStripModel::TabNavigating(WebContents* contents,
                                  ui::PageTransition transition) {
}

void TabStripModel::ForgetAllOpeners() {
}

void TabStripModel::ForgetGroup(WebContents* contents) {
}

bool TabStripModel::ShouldResetGroupOnSelect(WebContents* contents) const {
  return false;
}

void TabStripModel::SetTabBlocked(int index, bool blocked) {
}

void TabStripModel::SetTabPinned(int index, bool pinned) {
}

bool TabStripModel::IsTabPinned(int index) const {
  return false;
}

bool TabStripModel::IsTabBlocked(int index) const {
  return false;
}

int TabStripModel::IndexOfFirstNonPinnedTab() const {
  return false;
}

int TabStripModel::ConstrainInsertionIndex(int index, bool pinned_tab) {
  return false;
}

void TabStripModel::ExtendSelectionTo(int index) {
}

void TabStripModel::ToggleSelectionAt(int index) {
}

void TabStripModel::AddSelectionFromAnchorTo(int index) {
}

bool TabStripModel::IsTabSelected(int index) const {
  return false;
}

void TabStripModel::SetSelectionFromModel(
    const ui::ListSelectionModel& source) {
}

void TabStripModel::AddWebContents(WebContents* contents,
                                   int index,
                                   ui::PageTransition transition,
                                   int add_types) {
}

void TabStripModel::CloseSelectedTabs() {
}

void TabStripModel::SelectNextTab() {
}

void TabStripModel::SelectPreviousTab() {
}

void TabStripModel::SelectLastTab() {
}

void TabStripModel::MoveTabNext() {
}

void TabStripModel::MoveTabPrevious() {
}

// Context menu functions.
bool TabStripModel::IsContextMenuCommandEnabled(
    int context_index, ContextMenuCommand command_id) const {
  return false;
}

void TabStripModel::ExecuteContextMenuCommand(
    int context_index, ContextMenuCommand command_id) {
}

std::vector<int> TabStripModel::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  std::vector<int> indices;
  return indices;
}

bool TabStripModel::WillContextMenuPin(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  // If all tabs are pinned, then we unpin, otherwise we pin.
  bool all_pinned = true;
  return !all_pinned;
}

// static
bool TabStripModel::ContextMenuCommandToBrowserCommand(int cmd_id,
                                                       int* browser_cmd) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, private:

std::vector<WebContents*> TabStripModel::GetWebContentsFromIndices(
    const std::vector<int>& indices) const {
  std::vector<WebContents*> contents;
  return contents;
}

void TabStripModel::GetIndicesWithSameDomain(int index,
                                             std::vector<int>* indices) {
}

void TabStripModel::GetIndicesWithSameOpener(int index,
                                             std::vector<int>* indices) {
}

std::vector<int> TabStripModel::GetIndicesForCommand(int index) const {
  std::vector<int> z;
  return z;
}

bool TabStripModel::IsNewTabAtEndOfTabStrip(WebContents* contents) const {
  return false;
}

bool TabStripModel::InternalCloseTabs(const std::vector<int>& indices,
                                      uint32_t close_types) {
  return false;
}

void TabStripModel::InternalCloseTab(WebContents* contents,
                                     int index,
                                     bool create_historical_tabs) {
}

WebContents* TabStripModel::GetWebContentsAtImpl(int index) const {
  return nullptr;
}

void TabStripModel::NotifyIfActiveTabChanged(WebContents* old_contents,
                                             NotifyTypes notify_types) {
}

void TabStripModel::NotifyIfActiveOrSelectionChanged(
    WebContents* old_contents,
    NotifyTypes notify_types,
    const ui::ListSelectionModel& old_model) {
}

void TabStripModel::SetSelection(
    const ui::ListSelectionModel& new_model,
    NotifyTypes notify_types) {
}

void TabStripModel::SelectRelativeTab(bool next) {
}

void TabStripModel::MoveWebContentsAtImpl(int index,
                                          int to_position,
                                          bool select_after_move) {
}

void TabStripModel::MoveSelectedTabsToImpl(int index,
                                           size_t start,
                                           size_t length) {
}


void TabStripModel::FixOpenersAndGroupsReferencing(int index) {
}
