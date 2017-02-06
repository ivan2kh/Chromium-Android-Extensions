// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "ui/base/accelerators/accelerator.h"

#if defined(USE_ASH)
#include "ash/common/accelerators/accelerator_table.h"  // nogncheck
#endif  // USE_ASH

namespace chrome {

bool IsChromeAccelerator(const ui::Accelerator& accelerator, Profile* profile) {


  return false;
}

ui::Accelerator GetPrimaryChromeAcceleratorForCommandId(int command_id) {


  return ui::Accelerator();
}

}  // namespace chrome
