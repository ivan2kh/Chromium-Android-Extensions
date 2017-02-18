// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include <queue>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/offline_pages/downloads/offline_page_notification_bridge.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(offline_pages::RecentTabHelper);

namespace {
class DefaultDelegate: public offline_pages::RecentTabHelper::Delegate {
 public:
  DefaultDelegate() : is_low_end_device_(base::SysInfo::IsLowEndDevice()) {}

  // offline_pages::RecentTabHelper::Delegate
  std::unique_ptr<offline_pages::OfflinePageArchiver> CreatePageArchiver(
      content::WebContents* web_contents) override {
    return base::MakeUnique<offline_pages::OfflinePageMHTMLArchiver>(
        web_contents);
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }
  bool GetTabId(content::WebContents* web_contents, int* tab_id) override {
    return offline_pages::OfflinePageUtils::GetTabId(web_contents, tab_id);
  }
  bool IsLowEndDevice() override { return is_low_end_device_; }

 private:
  // Cached value of whether low end device.
  bool is_low_end_device_;
};
}  // namespace

namespace offline_pages {

using PageQuality = SnapshotController::PageQuality;

// Keeps client_id/request_id that will be used for the offline snapshot.
struct RecentTabHelper::SnapshotProgressInfo {
 public:
  // For a downloads snapshot request, where the |request_id| is defined.
  SnapshotProgressInfo(const ClientId& client_id, int64_t request_id)
      : client_id(client_id), request_id(request_id) {}

  // For a last_n snapshot request.
  explicit SnapshotProgressInfo(const ClientId& client_id)
      : client_id(client_id) {}

  bool IsForLastN() { return client_id.name_space == kLastNNamespace; }

  // The ClientID to go with the offline page.
  ClientId client_id;

  // Id of the suspended request in Background Offliner. Used to un-suspend
  // the request if the capture of the current page was not possible (e.g.
  // the user navigated to another page before current one was loaded).
  // 0 if this is a "last_n" info.
  int64_t request_id = OfflinePageModel::kInvalidOfflineId;

  // Expected snapshot quality should the saving succeed. This value is only
  // valid for successfully saved snapshots.
  SnapshotController::PageQuality expected_page_quality =
      SnapshotController::PageQuality::POOR;
};

RecentTabHelper::RecentTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(new DefaultDelegate()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

RecentTabHelper::~RecentTabHelper() {
}

void RecentTabHelper::SetDelegate(
    std::unique_ptr<RecentTabHelper::Delegate> delegate) {
  DCHECK(delegate);
  delegate_ = std::move(delegate);
}

void RecentTabHelper::ObserveAndDownloadCurrentPage(
    const ClientId& client_id, int64_t request_id) {
  // Note: as this implementation only supports one client namespace, enforce
  // that the call is from Downloads.
  DCHECK_EQ(kDownloadNamespace, client_id.name_space);
  auto new_downloads_snapshot_info =
      base::MakeUnique<SnapshotProgressInfo>(client_id, request_id);

  // If this tab helper is not enabled, immediately give the job back to
  // RequestCoordinator.
  if (!EnsureInitialized()) {
    ReportDownloadStatusToRequestCoordinator(new_downloads_snapshot_info.get(),
                                             false);
    return;
  }

  // If there is an ongoing snapshot request, completely ignore this one and
  // cancel the Background Offliner request.
  // TODO(carlosk): it might be better to make the decision to schedule or not
  // the background request here. See https://crbug.com/686165.
  // TODO(carlosk): there is an edge case that happens when the ongoing request
  // was automatically and transparently scheduled by a navigation event and
  // this call happens due to the user pressing the download button. The user's
  // request to download the page will be immediately dismissed. See
  // https://crbug.com/686283.
  if (downloads_ongoing_snapshot_info_) {
    ReportDownloadStatusToRequestCoordinator(new_downloads_snapshot_info.get(),
                                             true);
    return;
  }

  // Stores the new snapshot info.
  downloads_ongoing_snapshot_info_ = std::move(new_downloads_snapshot_info);

  // If the page is not yet ready for a snapshot return now as it will be
  // started later, once page loading advances.
  if (PageQuality::POOR == snapshot_controller_->current_page_quality()) {
    downloads_snapshot_on_hold_ = true;
    return;
  }

  // Otherwise start saving the snapshot now.
  SaveSnapshotForDownloads(false);
}

// Initialize lazily. It needs TabAndroid for initialization, which is also a
// TabHelper - so can't initialize in constructor because of uncertain order
// of creation of TabHelpers.
bool RecentTabHelper::EnsureInitialized() {
  if (snapshot_controller_)  // Initialized already.
    return snapshots_enabled_;

  snapshot_controller_.reset(
      new SnapshotController(delegate_->GetTaskRunner(), this));
  snapshot_controller_->Stop();  // It is reset when navigation commits.

  int tab_id_number = 0;
  tab_id_.clear();

  if (delegate_->GetTabId(web_contents(), &tab_id_number))
    tab_id_ = base::IntToString(tab_id_number);

  // TODO(dimich): When we have BackgroundOffliner, avoid capturing prerenderer
  // WebContents with its origin as well.
  snapshots_enabled_ = !tab_id_.empty() &&
                       !web_contents()->GetBrowserContext()->IsOffTheRecord();

  if (snapshots_enabled_) {
    page_model_ = OfflinePageModelFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
  }

  return snapshots_enabled_;
}

void RecentTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  if (!EnsureInitialized())
    return;

  // If there is an ongoing downloads request, lets allow Background Offliner to
  // continue downloading this page.
  if (downloads_ongoing_snapshot_info_) {
    ReportDownloadStatusToRequestCoordinator(
        downloads_ongoing_snapshot_info_.get(), false);
  }

  // Cancel any and all in flight snapshot tasks from the previous page.
  CancelInFlightSnapshots();
  downloads_snapshot_on_hold_ = false;

  // New navigation, new snapshot session.
  snapshot_url_ = web_contents()->GetLastCommittedURL();

  // Always reset so that posted tasks get canceled.
  snapshot_controller_->Reset();

  // Check for conditions that would cause us not to snapshot.
  bool can_save = !navigation_handle->IsErrorPage() &&
                  OfflinePageModel::CanSaveURL(snapshot_url_) &&
                  OfflinePageUtils::GetOfflinePageFromWebContents(
                      web_contents()) == nullptr;

  UMA_HISTOGRAM_BOOLEAN("OfflinePages.CanSaveRecentPage", can_save);

  if (!can_save)
    snapshot_controller_->Stop();
  last_n_listen_to_tab_hidden_ = can_save && !delegate_->IsLowEndDevice() &&
                                 IsOffliningRecentPagesEnabled();
  last_n_latest_saved_quality_ = PageQuality::POOR;
}

void RecentTabHelper::DocumentAvailableInMainFrame() {
  EnsureInitialized();
  snapshot_controller_->DocumentAvailableInMainFrame();
}

void RecentTabHelper::DocumentOnLoadCompletedInMainFrame() {
  EnsureInitialized();
  snapshot_controller_->DocumentOnLoadCompletedInMainFrame();
}

void RecentTabHelper::WebContentsDestroyed() {
  // If there is an ongoing downloads request, lets allow Background Offliner to
  // continue downloading this page.
  if (downloads_ongoing_snapshot_info_)
    ReportDownloadStatusToRequestCoordinator(
        downloads_ongoing_snapshot_info_.get(), false);
  // And cancel any ongoing snapshots.
  CancelInFlightSnapshots();
}

// TODO(carlosk): this method is also called when the tab is being closed, when
// saving a snapshot is probably useless (low probability of the user undoing
// the close). We should detect that and avoid the saving.
void RecentTabHelper::WasHidden() {
  if (!IsOffliningRecentPagesEnabled())
    return;

  // Return immediately if last_n is not listening to tab hidden events or if a
  // last_n snapshot is currently being saved.
  if (!last_n_listen_to_tab_hidden_ || last_n_ongoing_snapshot_info_)
    return;

  // Do not save if page quality is too low or if we already have a snapshot
  // with the current quality level.
  // Note: we assume page quality for a page can only increase.
  PageQuality current_quality = snapshot_controller_->current_page_quality();
  if (current_quality == PageQuality::POOR ||
      current_quality == last_n_latest_saved_quality_) {
    return;
  }

  last_n_ongoing_snapshot_info_ =
      base::MakeUnique<SnapshotProgressInfo>(GetRecentPagesClientId());
  DCHECK(last_n_ongoing_snapshot_info_->IsForLastN());
  DCHECK(snapshots_enabled_);
  // Remove previously captured pages for this tab.
  page_model_->GetOfflineIdsForClientId(
      GetRecentPagesClientId(),
      base::Bind(&RecentTabHelper::ContinueSnapshotWithIdsToPurge,
                 weak_ptr_factory_.GetWeakPtr(),
                 last_n_ongoing_snapshot_info_.get()));
}

// TODO(carlosk): rename this to RequestSnapshot and make it return a bool
// representing the acceptance of the snapshot request.
void RecentTabHelper::StartSnapshot() {
  DCHECK_NE(PageQuality::POOR, snapshot_controller_->current_page_quality());

  // As long as snapshots are enabled for this tab, there are two situations
  // that allow for a navigation event to start a snapshot:
  // 1) There is a request on hold waiting for the page to be minimally loaded.
  if (snapshots_enabled_ && downloads_snapshot_on_hold_) {
    downloads_snapshot_on_hold_ = false;
    SaveSnapshotForDownloads(false);
    return;
  }

  // 2) There's no ongoing snapshot and a previous one was saved with lower
  // expected quality than what would be possible now.
  if (snapshots_enabled_ &&
      (!downloads_ongoing_snapshot_info_ &&
       downloads_latest_saved_snapshot_info_ &&
       downloads_latest_saved_snapshot_info_->expected_page_quality <
           snapshot_controller_->current_page_quality())) {
    SaveSnapshotForDownloads(true);
    return;
  }

  // Notify the controller that a snapshot was not started.
  snapshot_controller_->PendingSnapshotCompleted();
}

void RecentTabHelper::SaveSnapshotForDownloads(bool replace_latest) {
  DCHECK_NE(PageQuality::POOR, snapshot_controller_->current_page_quality());

  if (replace_latest) {
    // Start by requesting the deletion of the existing previous snapshot of
    // this page.
    DCHECK(downloads_latest_saved_snapshot_info_);
    DCHECK(!downloads_ongoing_snapshot_info_);
    downloads_ongoing_snapshot_info_ = base::MakeUnique<SnapshotProgressInfo>(
        downloads_latest_saved_snapshot_info_->client_id,
        downloads_latest_saved_snapshot_info_->request_id);
    std::vector<int64_t> ids{downloads_latest_saved_snapshot_info_->request_id};
    ContinueSnapshotWithIdsToPurge(downloads_ongoing_snapshot_info_.get(), ids);
  } else {
    // Otherwise go straight to saving the page.
    DCHECK(downloads_ongoing_snapshot_info_);
    ContinueSnapshotAfterPurge(downloads_ongoing_snapshot_info_.get(),
                               OfflinePageModel::DeletePageResult::SUCCESS);
  }
}

// This is the 1st step of a sequence of async operations chained through
// callbacks, mostly shared between last_n and downloads:
// 1) Compute the set of old 'last_n' pages that have to be purged.
// 2) Delete the pages found in the previous step.
// 3) Snapshot the current web contents.
// 4) Notify requesters about the final result of the operation.
//
// For last_n requests the sequence is always started in 1). For downloads it
// starts in either 2) or 3). Step 4) might be called anytime during the chain
// for early termination in case of errors.
void RecentTabHelper::ContinueSnapshotWithIdsToPurge(
    SnapshotProgressInfo* snapshot_info,
    const std::vector<int64_t>& page_ids) {
  DCHECK(snapshot_info);

  page_model_->DeletePagesByOfflineId(
      page_ids, base::Bind(&RecentTabHelper::ContinueSnapshotAfterPurge,
                           weak_ptr_factory_.GetWeakPtr(), snapshot_info));
}

void RecentTabHelper::ContinueSnapshotAfterPurge(
    SnapshotProgressInfo* snapshot_info,
    OfflinePageModel::DeletePageResult result) {
  DCHECK_EQ(snapshot_url_, web_contents()->GetLastCommittedURL());
  if (result != OfflinePageModel::DeletePageResult::SUCCESS) {
    ReportSnapshotCompleted(snapshot_info, false);
    return;
  }

  snapshot_info->expected_page_quality =
      snapshot_controller_->current_page_quality();
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = snapshot_url_;
  save_page_params.client_id = snapshot_info->client_id;
  save_page_params.proposed_offline_id = snapshot_info->request_id;
  save_page_params.is_background = false;
  page_model_->SavePage(
      save_page_params, delegate_->CreatePageArchiver(web_contents()),
      base::Bind(&RecentTabHelper::SavePageCallback,
                 weak_ptr_factory_.GetWeakPtr(), snapshot_info));
}

void RecentTabHelper::SavePageCallback(SnapshotProgressInfo* snapshot_info,
                                       OfflinePageModel::SavePageResult result,
                                       int64_t offline_id) {
  DCHECK(snapshot_info->IsForLastN() ||
         snapshot_info->request_id == offline_id);
  ReportSnapshotCompleted(snapshot_info, result == SavePageResult::SUCCESS);
}

// Note: this is the final step in the chain of callbacks and it's where the
// behavior is different depending on this being a last_n or downloads snapshot.
void RecentTabHelper::ReportSnapshotCompleted(
    SnapshotProgressInfo* snapshot_info,
    bool success) {
  if (snapshot_info->IsForLastN()) {
    DCHECK_EQ(snapshot_info, last_n_ongoing_snapshot_info_.get());
    if (success)
      last_n_latest_saved_quality_ = snapshot_info->expected_page_quality;
    last_n_ongoing_snapshot_info_.reset();
    return;
  }

  DCHECK_EQ(snapshot_info, downloads_ongoing_snapshot_info_.get());
  snapshot_controller_->PendingSnapshotCompleted();
  // Tell RequestCoordinator how the request should be processed further.
  ReportDownloadStatusToRequestCoordinator(snapshot_info, success);
  if (success) {
    downloads_latest_saved_snapshot_info_ =
        std::move(downloads_ongoing_snapshot_info_);
  } else {
    downloads_ongoing_snapshot_info_.reset();
  }
}

// Note: we cannot assume that snapshot_info == downloads_latest_snapshot_info_
// because further calls made to ObserveAndDownloadCurrentPage will replace
// downloads_latest_snapshot_info_ with a new instance.
void RecentTabHelper::ReportDownloadStatusToRequestCoordinator(
    SnapshotProgressInfo* snapshot_info,
    bool cancel_background_request) {
  DCHECK(snapshot_info);
  DCHECK(!snapshot_info->IsForLastN());

  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(
          web_contents()->GetBrowserContext());
  if (!request_coordinator)
    return;

  // It is OK to call these methods more then once, depending on
  // number of snapshots attempted in this tab helper. If the request_id is not
  // in the list of RequestCoordinator, these calls have no effect.
  if (cancel_background_request)
    request_coordinator->MarkRequestCompleted(snapshot_info->request_id);
  else
    request_coordinator->EnableForOffliner(snapshot_info->request_id,
                                           snapshot_info->client_id);
}

ClientId RecentTabHelper::GetRecentPagesClientId() const {
  return ClientId(kLastNNamespace, tab_id_);
}

void RecentTabHelper::CancelInFlightSnapshots() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  downloads_ongoing_snapshot_info_.reset();
  downloads_latest_saved_snapshot_info_.reset();
  last_n_ongoing_snapshot_info_.reset();
}

}  // namespace offline_pages
