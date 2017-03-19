// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_list.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"

using base::UserMetricsAction;
using content::WebContents;

// static
base::LazyInstance<base::ObserverList<chrome::BrowserListObserver>>::Leaky
    BrowserList::observers_ = LAZY_INSTANCE_INITIALIZER;

// static
BrowserList* BrowserList::instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// BrowserList, public:

Browser* BrowserList::GetLastActive() const {
  if (!last_active_browsers_.empty())
    return *(last_active_browsers_.rbegin());
  return NULL;
}

// static
BrowserList* BrowserList::GetInstance() {
  BrowserList** list = NULL;
  list = &instance_;
  if (!*list)
    *list = new BrowserList;
  return *list;
}

// static
void BrowserList::AddBrowser(Browser* browser) {
  NOTIMPLEMENTED();
}

// static
void BrowserList::RemoveBrowser(Browser* browser) {
  NOTIMPLEMENTED();
}

// static
void BrowserList::AddObserver(chrome::BrowserListObserver* observer) {
  observers_.Get().AddObserver(observer);
}

// static
void BrowserList::RemoveObserver(chrome::BrowserListObserver* observer) {
  observers_.Get().RemoveObserver(observer);
}

// static
void BrowserList::CloseAllBrowsersWithProfile(Profile* profile) {
  NOTIMPLEMENTED();
}

// static
void BrowserList::TryToCloseBrowserList(const BrowserVector& browsers_to_close,
                                        const CloseCallback& on_close_success,
                                        const CloseCallback& on_close_aborted,
                                        const base::FilePath& profile_path,
                                        const bool skip_beforeunload) {
  NOTIMPLEMENTED();
}

// static
void BrowserList::MoveBrowsersInWorkspaceToFront(
    const std::string& new_workspace) {
  NOTIMPLEMENTED();
}

// static
void BrowserList::SetLastActive(Browser* browser) {
  NOTIMPLEMENTED();
}

// static
void BrowserList::NotifyBrowserNoLongerActive(Browser* browser) {
  NOTIMPLEMENTED();
}

// static
bool BrowserList::IsIncognitoSessionActive() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool BrowserList::IsIncognitoSessionActiveForProfile(Profile* profile) {
  NOTIMPLEMENTED();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserList, private:

BrowserList::BrowserList() {
}

BrowserList::~BrowserList() {
}

// static
void BrowserList::RemoveBrowserFrom(Browser* browser,
                                    BrowserVector* browser_list) {
  NOTIMPLEMENTED();
}
