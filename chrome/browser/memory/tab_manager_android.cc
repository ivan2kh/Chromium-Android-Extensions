// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager.h"

#include <stddef.h>

#include <algorithm>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/memory/oom_memory_details.h"
#include "chrome/browser/memory/tab_manager_observer.h"
#include "chrome/browser/memory/tab_manager_web_contents_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/system_memory_stats_recorder.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/memory_pressure_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_importance_signals.h"

#if defined(OS_CHROMEOS)
#include "ash/common/multi_profile_uma.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"
#endif

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;
using content::WebContents;

namespace memory {

////////////////////////////////////////////////////////////////////////////////
// TabManager

TabManagerObserver::~TabManagerObserver() {}
TabManager::TabManager()
    : 
      //browser_tab_strip_tracker_(this, nullptr, nullptr),
      weak_ptr_factory_(this)
       {
       }

TabManager::~TabManager() {
}


void TabManager::Start() {
}

void TabManager::Stop() {
}

TabStatsList TabManager::GetTabStats() const {
  return TabStatsList();
}

std::vector<content::RenderProcessHost*>
TabManager::GetOrderedRenderers() const {
  // Get the tab stats.
  std::vector<content::RenderProcessHost*> sorted_renderers;
  return sorted_renderers;
}

bool TabManager::IsTabDiscarded(content::WebContents* contents) const {
  return false;
}
bool TabManager::CanDiscardTab(int64_t target_web_contents_id) const {
  return false;
}

void TabManager::DiscardTab() {
}

WebContents* TabManager::DiscardTabById(int64_t target_web_contents_id) {
  return NULL;
}

WebContents* TabManager::DiscardTabByExtension(content::WebContents* contents) {
  return NULL;
}

void TabManager::LogMemoryAndDiscardTab() {
}

void TabManager::LogMemory(const std::string& title,
                           const base::Closure& callback) {
}

void TabManager::set_test_tick_clock(base::TickClock* test_tick_clock) {
  test_tick_clock_ = test_tick_clock;
}
TabStatsList TabManager::GetUnsortedTabStats() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TabStatsList stats_list;
  return  stats_list;
}

void TabManager::AddObserver(TabManagerObserver* observer) {}

void TabManager::RemoveObserver(TabManagerObserver* observer) {
  
}
void TabManager::set_minimum_protection_time_for_tests(
        base::TimeDelta minimum_protection_time) {
  minimum_protection_time_ = minimum_protection_time;
}

bool TabManager::IsTabAutoDiscardable(content::WebContents* contents) const {
  return false;
}

void TabManager::SetTabAutoDiscardableState(content::WebContents* contents,
                                            bool state) {
}

content::WebContents* TabManager::GetWebContentsById(
        int64_t tab_contents_id) const {
  return NULL;
}

bool TabManager::CanSuspendBackgroundedRenderer(int render_process_id) const {
  return true;
}

// static
bool TabManager::CompareTabStats(const TabStats& first,
                                 const TabStats& second) {
  return false;
}

// static
int64_t TabManager::IdFromWebContents(WebContents* web_contents) {
  return reinterpret_cast<int64_t>(web_contents);
}

///////////////////////////////////////////////////////////////////////////////
// TabManager, private:

void TabManager::OnDiscardedStateChange(content::WebContents* contents,
                                        bool is_discarded) {
}

void TabManager::OnAutoDiscardableStateChange(content::WebContents* contents,
                                              bool is_auto_discardable) {
}

// static
void TabManager::PurgeMemoryAndDiscardTab() {
}

// static
bool TabManager::IsInternalPage(const GURL& url) {
  return false;
}

void TabManager::RecordDiscardStatistics() {
}

void TabManager::RecordRecentTabDiscard() {
}

void TabManager::PurgeBrowserMemory() {
}

int TabManager::GetTabCount() const {
  return 0;
}

void TabManager::AddTabStats(TabStatsList* stats_list) const {
}

void TabManager::AddTabStats(const TabStripModel* model,
                             bool is_app,
                             bool active_model,
                             TabStatsList* stats_list) const {

}

// This function is called when |update_timer_| fires. It will adjust the clock
// if needed (if it detects that the machine was asleep) and will fire the stats
// updating on ChromeOS via the delegate. This function also tries to purge
// cache memory and suspend tabs which becomes and keeps backgrounded for a
// while.
void TabManager::UpdateTimerCallback() {
}

WebContents* TabManager::DiscardWebContentsAt(int index, TabStripModel* model) {
    return nullptr;

}

void TabManager::OnMemoryPressure(
        base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
}

void TabManager::TabChangedAt(content::WebContents* contents,
                              int index,
                              TabChangeType change_type) {
}

void TabManager::ActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) {
}

void TabManager::TabInsertedAt(TabStripModel* tab_strip_model,
                               content::WebContents* contents,
                               int index,
                               bool foreground) {
}

bool TabManager::IsMediaTab(WebContents* contents) const {
    return true;

}

TabManager::WebContentsData* TabManager::GetWebContentsData(
        content::WebContents* contents) const {
  return nullptr;
}

TimeTicks TabManager::NowTicks() const {
  if (!test_tick_clock_)
    return TimeTicks::Now();

  return test_tick_clock_->NowTicks();
}

void TabManager::DoChildProcessDispatch() {
}

// TODO(jamescook): This should consider tabs with references to other tabs,
// such as tabs created with JavaScript window.open(). Potentially consider
// discarding the entire set together, or use that in the priority computation.
content::WebContents* TabManager::DiscardTabImpl() {
    return nullptr;
}

// Check the variation parameter to see if a tab can be discarded only once or
// multiple times.
// Default is to only discard once per tab.
bool TabManager::CanOnlyDiscardOnce() const {
#if defined(OS_WIN) || defined(OS_MACOSX)
  // On Windows and MacOS, default to discarding only once unless otherwise
  // specified by the variation parameter.
  // TODO(georgesak): Add Linux when automatic discarding is enabled for that
  // platform.
  std::string allow_multiple_discards = variations::GetVariationParamValue(
      features::kAutomaticTabDiscarding.name, "AllowMultipleDiscards");
  return (allow_multiple_discards != "true");
#else
  return false;
#endif
}


}  // namespace memory
