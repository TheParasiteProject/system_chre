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

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED

#include "chre/core/ble_l2cap_coc_socket_data.h"
#include "chre/platform/platform_bt_socket.h"
#include "chre/platform/platform_bt_socket_resources.h"
#include "chre/util/fixed_size_vector.h"
#include "chre/util/memory_pool.h"
#include "chre_api/chre.h"

namespace chre {

/**
 * Manages offloaded BLE sockets. Handles sending packets between nanoapps and
 * BLE sockets.
 */
class BleSocketManager : public NonCopyable {
 public:
  // Forward all arguments passed to the BleSocketManager constructor to the
  // PlatformBtSocketResources constructor
  template <typename... Args>
  BleSocketManager(Args &&...args)
      : mPlatformBtSocketResources(std::forward<Args>(args)...) {}

  /**
   * Creates a PlatformBtSocket and notifies the nanoapp that a BLE socket has
   * been connected and is ready to be used.
   *
   * @param socketData Metadata for the BLE socket.
   * @return chreError Result of whether the socket was created successfully and
   * whether the nanoapp has accepted it.
   */
  chreError socketConnected(const BleL2capCocSocketData &socketData);

  /**
   * Callback a nanoapp uses to accept the socket. This will be used in the
   * middle of socketConnected and is part of a synchronous interaction with the
   * nanoapp
   */
  bool acceptBleSocket(uint64_t socketId);

  /**
   * Sends a packet to the socket.
   *
   * @see chreBleSocketSend
   */
  int32_t sendBleSocketPacket(
      uint64_t /*socketId*/, const void * /*data*/, uint16_t /*length*/,
      chreBleSocketPacketFreeFunction * /*freeCallback*/) {
    return CHRE_ERROR_NOT_SUPPORTED;
  }

 private:
  static constexpr uint8_t kMaxNumSockets = 3;

  /**
   * Tracks BT sockets and their corresponding nanoapp.
   *
   * TODO(b/418832158): We can't use a CHRE FixedSizeVector here because some
   * PlatformBtSocket implementations have dependencies which delete the copy
   * and move assignment operators. Look into adding move assignment operators
   * to those dependencies and refactor this code when finished.
   */
  MemoryPool<PlatformBtSocket, kMaxNumSockets> mBtSockets;

  /**
   * Platform resources used for creating a new BT socket.
   */
  PlatformBtSocketResources mPlatformBtSocketResources;
};

}  // namespace chre

#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED
