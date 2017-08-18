/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CHRE_PLATFORM_POWER_CONTROL_MANAGER_BASE_H
#define CHRE_PLATFORM_POWER_CONTROL_MANAGER_BASE_H

extern "C" {

#include "qurt.h"
#include "sns_pm.h"

} // extern "C"

namespace chre {

class PowerControlManagerBase {
 public:
  PowerControlManagerBase();

  ~PowerControlManagerBase();

  /**
    * Votes for a power mode to the SLPI power manager. Should only be called
    * from the context of the main CHRE thread.
    *
    * @param mode The power mode to vote for.
    *
    * @return true if the vote returned SNS_PM_SUCCESS.
    */
  bool votePowerMode(sns_pm_img_mode_e mode);

 protected:
  //! Client handle for the subscription to the power manager
  sns_pm_handle_t mClientHandle = nullptr;
};

} // namespace chre

#endif // CHRE_PLATFORM_POWER_CONTROL_MANAGER_BASE_H
