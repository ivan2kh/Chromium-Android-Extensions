// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_driver_impl.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/core/browser/history_service.h"

namespace favicon {
namespace {

#if defined(OS_ANDROID) || defined(OS_IOS)
const bool kEnableTouchIcon = true;
#else
const bool kEnableTouchIcon = false;
#endif

}  // namespace

FaviconDriverImpl::FaviconDriverImpl(FaviconService* favicon_service,
                                     history::HistoryService* history_service,
                                     bookmarks::BookmarkModel* bookmark_model)
    : favicon_service_(favicon_service),
      history_service_(history_service),
      bookmark_model_(bookmark_model) {
  favicon_handler_.reset(new FaviconHandler(
      favicon_service_, this, kEnableTouchIcon
                                  ? FaviconDriverObserver::NON_TOUCH_LARGEST
                                  : FaviconDriverObserver::NON_TOUCH_16_DIP));
  if (kEnableTouchIcon) {
    touch_icon_handler_.reset(new FaviconHandler(
        favicon_service_, this, FaviconDriverObserver::TOUCH_LARGEST));
  }
}

FaviconDriverImpl::~FaviconDriverImpl() {
}

void FaviconDriverImpl::FetchFavicon(const GURL& url) {
  favicon_handler_->FetchFavicon(url);
  if (touch_icon_handler_.get())
    touch_icon_handler_->FetchFavicon(url);
}

void FaviconDriverImpl::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if (bitmaps.empty() && http_status_code == 404) {
    DVLOG(1) << "Failed to Download Favicon:" << image_url;
    if (favicon_service_)
      favicon_service_->UnableToDownloadFavicon(image_url);
  }

  favicon_handler_->OnDidDownloadFavicon(id, image_url, bitmaps,
                                         original_bitmap_sizes);
  if (touch_icon_handler_.get()) {
    touch_icon_handler_->OnDidDownloadFavicon(id, image_url, bitmaps,
                                              original_bitmap_sizes);
  }
}

bool FaviconDriverImpl::IsBookmarked(const GURL& url) {
  return bookmark_model_ && bookmark_model_->IsBookmarked(url);
}

bool FaviconDriverImpl::HasPendingTasksForTest() {
  if (favicon_handler_->HasPendingTasksForTest())
    return true;
  if (touch_icon_handler_ && touch_icon_handler_->HasPendingTasksForTest())
    return true;
  return false;
}

bool FaviconDriverImpl::WasUnableToDownloadFavicon(const GURL& url) {
  return favicon_service_ && favicon_service_->WasUnableToDownloadFavicon(url);
}

void FaviconDriverImpl::SetFaviconOutOfDateForPage(const GURL& url,
                                                   bool force_reload) {
  if (favicon_service_) {
    favicon_service_->SetFaviconOutOfDateForPage(url);
    if (force_reload)
      favicon_service_->ClearUnableToDownloadFavicons();
  }
}

void FaviconDriverImpl::OnUpdateFaviconURL(
    const GURL& page_url,
    const std::vector<FaviconURL>& candidates) {
  DCHECK(!candidates.empty());
  favicon_handler_->OnUpdateFaviconURL(page_url, candidates);
  if (touch_icon_handler_.get())
    touch_icon_handler_->OnUpdateFaviconURL(page_url, candidates);
}

}  // namespace favicon
