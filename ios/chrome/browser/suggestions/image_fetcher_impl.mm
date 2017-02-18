// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/suggestions/image_fetcher_impl.h"

#import <UIKit/UIKit.h>

#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/image_fetcher/image_fetcher_delegate.h"
#include "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace suggestions {

ImageFetcherImpl::ImageFetcherImpl(
    net::URLRequestContextGetter* url_request_context,
    base::SequencedWorkerPool* blocking_pool)
    : image_fetcher_(
          base::MakeUnique<image_fetcher::IOSImageDataFetcherWrapper>(
              url_request_context,
              blocking_pool)) {}

ImageFetcherImpl::~ImageFetcherImpl() {
}

void ImageFetcherImpl::SetImageFetcherDelegate(
    image_fetcher::ImageFetcherDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
}

void ImageFetcherImpl::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  image_fetcher_->SetDataUseServiceName(data_use_service_name);
}

void ImageFetcherImpl::StartOrQueueNetworkRequest(
    const std::string& id,
    const GURL& image_url,
    base::Callback<void(const std::string&, const gfx::Image&)> callback) {
  if (image_url.is_empty()) {
    gfx::Image empty_image;
    callback.Run(id, empty_image);
    if (delegate_) {
      delegate_->OnImageFetched(id, empty_image);
    }
    return;
  }
  // Copy string reference so it's retained.
  const std::string fetch_id(id);
  // If image_fetcher_ is destroyed the request will be cancelled and this block
  // will never be called. A reference to delegate_ can be kept.
  image_fetcher::IOSImageDataFetcherCallback fetcher_callback =
      ^(NSData* data, const image_fetcher::RequestMetadata& metadata) {
        if (data) {
          // Most likely always returns 1x images.
          UIImage* ui_image = [UIImage imageWithData:data scale:1];
          if (ui_image) {
            gfx::Image gfx_image(ui_image, base::scoped_policy::ASSUME);
            callback.Run(fetch_id, gfx_image);
            if (delegate_) {
              delegate_->OnImageFetched(fetch_id, gfx_image);
            }
            return;
          }
        }
        gfx::Image empty_image;
        callback.Run(fetch_id, empty_image);
        if (delegate_) {
          delegate_->OnImageFetched(fetch_id, empty_image);
        }
      };
  image_fetcher_->FetchImageDataWebpDecoded(image_url, fetcher_callback);
}

}  // namespace suggestions
