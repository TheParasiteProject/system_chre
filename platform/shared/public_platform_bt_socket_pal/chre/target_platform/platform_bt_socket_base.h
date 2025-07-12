/*
 * Copyright (C) 2025 The Android Open Source Project
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

#pragma once

#include <cinttypes>
#include <cstdint>

#include "chre/core/ble_l2cap_coc_socket_data.h"
#include "chre/platform/platform_bt_socket_resources.h"
#include "chre/util/unique_ptr.h"

#include "pw_allocator/first_fit.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_bluetooth_proxy/l2cap_coc.h"
#include "pw_multibuf/allocator.h"
#include "pw_multibuf/multibuf.h"
#include "pw_multibuf/simple_allocator.h"
#include "pw_status/status.h"

namespace chre {

/**
 * AOC-specific implementation of a BT socket.
 */
class PlatformBtSocketBase {
 public:
  PlatformBtSocketBase(const BleL2capCocSocketData &socketData,
                       PlatformBtSocketResources &platformBtSocketResources);

  /**
   * Callback to be invoked on Rx SDUs.
   *
   * @see pw::bluetooth::proxy::ProxyHost::AcquireL2capCoc()
   */
  void handleSocketData(pw::multibuf::MultiBuf &&) {
    // TODO(b/392139857): Implement receiving data from the BT offload socket
  }

 protected:
  // Multibuf Rx allocators

  static constexpr size_t kRxMultiBufAreaSize = 2 * 1024;

  static constexpr size_t kRxMultiBufMetaDataSize = 256;

  // TODO(b/430672746): This is 5 * the metadata needed for a single multibuf
  // based on the hard coded tx queue size for a pigweed L2capChannel. When the
  // queue size becomes configurable (or multibuf metadata size is reduced),
  // consider making this value smaller.
  static constexpr size_t kTxMultiBufMetaDataSize = 5 * 256;

  std::array<std::byte, kRxMultiBufAreaSize> mRxMultibufArea{};

  std::array<std::byte, kRxMultiBufMetaDataSize> mRxMultibufMetaData{};

  pw::allocator::FirstFitAllocator<pw::allocator::FirstFitBlock<uintptr_t>>
      mRxFirstFitAllocator{mRxMultibufMetaData};

  pw::allocator::SynchronizedAllocator<pw::sync::Mutex> mSyncAllocator{
      mRxFirstFitAllocator};

  // Allocator used for Rx data received from the BT socket.
  pw::multibuf::SimpleAllocator mSimpleAllocator{mRxMultibufArea,
                                                 mSyncAllocator};

  // PW L2CAP COC utility used for interacting with the BT socket.
  std::optional<pw::bluetooth::proxy::L2capCoc> mL2capCoc;

  std::array<std::byte, kTxMultiBufMetaDataSize> mTxMultibufMetaData{};

  pw::allocator::FirstFitAllocator<pw::allocator::FirstFitBlock<uintptr_t>>
      mTxFirstFitAllocator{mTxMultibufMetaData};
};

}  // namespace chre
