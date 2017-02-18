// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_items_display_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#import "ios/chrome/browser/payments/cells/price_item.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_test_util.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/payments/payment_request.h"
#include "testing/gtest/include/gtest/gtest.h"

class PaymentItemsDisplayViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    personal_data_manager_ =
        base::MakeUnique<autofill::TestPersonalDataManager>();

    web::PaymentRequest web_payment_request =
        payment_request_test_util::CreateTestWebPaymentRequest();

    payment_request_ = base::MakeUnique<PaymentRequest>(
        base::MakeUnique<web::PaymentRequest>(web_payment_request),
        personal_data_manager_.get());

    return [[PaymentItemsDisplayViewController alloc]
        initWithPaymentRequest:payment_request_.get()
              payButtonEnabled:YES];
  }

  PaymentItemsDisplayViewController* GetPaymentItemsViewController() {
    return base::mac::ObjCCastStrict<PaymentItemsDisplayViewController>(
        controller());
  }

  std::unique_ptr<autofill::TestPersonalDataManager> personal_data_manager_;
  std::unique_ptr<PaymentRequest> payment_request_;
};

// Tests that the correct number of items are displayed after loading the model.
TEST_F(PaymentItemsDisplayViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_PAYMENT_ITEMS_TITLE);

  [GetPaymentItemsViewController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // There should be one item for the total and another one for sub-total.
  ASSERT_EQ(2U, static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // They both should be of type PriceItem.
  id item = GetCollectionViewItem(0, 0);
  EXPECT_TRUE([item isMemberOfClass:[PriceItem class]]);
  item = GetCollectionViewItem(0, 1);
  EXPECT_TRUE([item isMemberOfClass:[PriceItem class]]);
}
