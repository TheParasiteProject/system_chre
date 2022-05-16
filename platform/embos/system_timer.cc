/*
 * Copyright (C) 2022 The Android Open Source Project
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
#include <algorithm>
#include <cinttypes>

#include "chre/platform/fatal_error.h"
#include "chre/platform/log.h"
#include "chre/platform/system_timer.h"
#include "chre/util/time.h"

namespace chre {

void SystemTimerBase::invokeCallback(void *instance) {
  auto *timer = static_cast<SystemTimer *>(instance);
  timer->mCallback(timer->mData);
}

SystemTimer::~SystemTimer() {
  // cancel an existing timer if any, and delete the timer instance.
  cancel();
  OS_TIMER_DeleteEx(&mTimer);
}

bool SystemTimer::init() {
  constexpr uint32_t kSomeInitialPeriod = 100;
  OS_TIMER_CreateEx(&mTimer, SystemTimerBase::invokeCallback,
                    kSomeInitialPeriod, this /*context*/);
  return true;
}

bool SystemTimer::set(SystemTimerCallback *callback, void *data,
                      Nanoseconds delay) {
  // The public EmbOS documentation does not specify how it handles calls to
  // its timer create API if the values lie beyond the specified interval of
  // 1 ≤ Period ≤ 0x7FFFFFFF. Since there's not return value to assess API
  // call success, we clamp the delay to the supported interval.
  constexpr uint64_t kMinDelayMs = 1;
  constexpr uint64_t kMaxDelayMs = INT32_MAX;
  uint64_t delayMs = Milliseconds(delay).getMilliseconds();
  delayMs = std::min(std::max(delayMs, kMinDelayMs), kMaxDelayMs);

  OS_TIMER_StopEx(&mTimer);
  OS_TIMER_SetPeriodEx(&mTimer, static_cast<OS_TIME>(delayMs));

  mCallback = callback;
  mData = data;

  OS_TIMER_RestartEx(&mTimer);
  return true;
}

// The return value for this function is not guaranteed to be correct - please
// see the note in @ref SystemTimerBase.
bool SystemTimer::cancel() {
  bool success = false;
  if (isActive()) {
    OS_TIMER_StopEx(&mTimer);
    success = true;
  }
  return success;
}

bool SystemTimer::isActive() {
  return (OS_TIMER_GetStatusEx(&mTimer) != 0);
}

}  // namespace chre