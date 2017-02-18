// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_ADDRESS_NORMALIZER_H_
#define COMPONENTS_PAYMENTS_ADDRESS_NORMALIZER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"

using autofill::AutofillProfile;

namespace payments {

// A class used to normalize addresses.
class AddressNormalizer : public autofill::LoadRulesListener,
                          public base::SupportsWeakPtr<AddressNormalizer> {
 public:
  // The interface for the normalization delegates.
  class Delegate {
   public:
    virtual void OnAddressNormalized(
        const autofill::AutofillProfile& normalized_profile) = 0;

    virtual void OnCouldNotNormalize(
        const autofill::AutofillProfile& profile) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // The interface for the normalization request.
  class Request {
   public:
    virtual void OnRulesLoaded(bool success) = 0;
    virtual ~Request() {}
  };

  AddressNormalizer(std::unique_ptr<i18n::addressinput::Source> source,
                    std::unique_ptr<i18n::addressinput::Storage> storage);
  ~AddressNormalizer() override;

  // Start loading the validation rules for the specified |region_code|.
  void LoadRulesForRegion(const std::string& region_code);

  // Returns whether the rules for the specified |region_code| have finished
  // loading.
  bool AreRulesLoadedForRegion(const std::string& region_code);

  // Starts the normalization of the |profile| based on the |region_code|. The
  // normalized profile will be returned to the |requester| possibly
  // asynchronously.
  void StartAddressNormalization(const autofill::AutofillProfile& profile,
                                 const std::string& region_code,
                                 Delegate* requester);

 private:
  // Called when the validation rules for the |region_code| have finished
  // loading. Implementation of the LoadRulesListener interface.
  void OnAddressValidationRulesLoaded(const std::string& region_code,
                                      bool success) override;

  // Map associating a region code to pending normalizations.
  std::map<std::string, std::vector<std::unique_ptr<Request>>>
      pending_normalization_;

  // The address validator used to normalize addresses.
  autofill::AddressValidator address_validator_;

  DISALLOW_COPY_AND_ASSIGN(AddressNormalizer);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_ADDRESS_NORMALIZER_H_
