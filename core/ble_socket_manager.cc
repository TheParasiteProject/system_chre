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

#ifdef CHRE_BLE_SOCKET_SUPPORT_ENABLED

#include "chre/core/ble_socket_manager.h"

#include "chre/core/event_loop_manager.h"
#include "chre/platform/log.h"

namespace chre {

chreError BleSocketManager::socketConnected(
    const BleL2capCocSocketData &socketData) {
  LOGI("socketConnected request for endpointId: %" PRIx64 " socketId: %" PRIx64,
       socketData.endpointId, socketData.socketId);
  PlatformBtSocket *btSocket =
      mBtSockets.allocate(socketData, mPlatformBtSocketResources);
  if (btSocket == nullptr) {
    LOGE("No available sockets");
    return CHRE_ERROR_NO_MEMORY;
  }

  chreError error = CHRE_ERROR_NONE;
  if (!btSocket->isInitialized()) {
    LOGE("Failed to initialize socket %" PRIu64, socketData.socketId);
    error = CHRE_ERROR;
  } else {
    uint16_t targetInstanceId;
    bool foundNanoapp = EventLoopManagerSingleton::get()
                            ->getEventLoop()
                            .findNanoappInstanceIdByAppId(socketData.endpointId,
                                                          &targetInstanceId);
    if (!foundNanoapp) {
      LOGE("Failed to find nanoapp id %" PRIu64 " for socket %" PRIu64,
           socketData.endpointId, socketData.socketId);
      error = CHRE_ERROR_DESTINATION_NOT_FOUND;
    } else {
      // TODO(b/425747779): Populate BT socket name
      chreBleSocketConnectionEvent event = {
          .socketId = socketData.socketId,
          .socketName = nullptr,
          .maxTxPacketLength = socketData.txConfig.mtu,
          .maxRxPacketLength = socketData.rxConfig.mtu};
      EventLoopManagerSingleton::get()->getEventLoop().distributeEventSync(
          CHRE_EVENT_BLE_SOCKET_CONNECTION, &event, targetInstanceId);
      if (!btSocket->getSocketAccepted()) {
        LOGE("Nanoapp id %" PRIu64 " did not accept socket %" PRIu64,
             socketData.endpointId, socketData.socketId);
        error = CHRE_ERROR;
      }
    }
  }
  if (error != CHRE_ERROR_NONE) {
    mBtSockets.deallocate(btSocket);
  }
  return error;
}

bool BleSocketManager::acceptBleSocket(uint64_t socketId) {
  PlatformBtSocket *btSocket = mBtSockets.find(
      [](PlatformBtSocket *btSocket, void *data) {
        uint64_t socketId = *(static_cast<uint64_t *>(data));
        if (btSocket->getId() == socketId) {
          btSocket->setSocketAccepted(true);
          return true;
        }
        return false;
      },
      &socketId);
  return btSocket != nullptr;
}

}  // namespace chre

#endif  // CHRE_BLE_SOCKET_SUPPORT_ENABLED
