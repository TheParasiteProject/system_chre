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

#include "chre/platform/platform_bt_socket.h"
#include "chre/target_platform/platform_bt_socket_base.h"

#include "chre/core/event_loop_manager.h"
#include "chre/platform/log.h"
#include "chre/platform/shared/memory.h"
#include "chre/util/unique_ptr.h"
#include "pw_multibuf/from_span.h"
#include "pw_status/status.h"

namespace chre {

namespace {

// TODO(b/393485754): determine correct number of credits
constexpr uint16_t kRxAdditionalCredits = 0xFFFF;

SocketEvent convertPwSocketEventToChre(
    pw::bluetooth::proxy::L2capChannelEvent event) {
  switch (event) {
    case pw::bluetooth::proxy::L2capChannelEvent::kWriteAvailable:
      return SocketEvent::SEND_AVAILABLE;
    default:
      return SocketEvent::UNKNOWN;
  }
}

}  // namespace

PlatformBtSocketBase::PlatformBtSocketBase(
    const BleL2capCocSocketData &socketData,
    PlatformBtSocketResources &platformBtSocketResources) {
  pw::bluetooth::proxy::L2capCoc::CocConfig pwRxConfig = {
      .cid = socketData.rxConfig.cid,
      .mtu = socketData.rxConfig.mtu,
      .mps = socketData.rxConfig.mps,
      .credits = kRxAdditionalCredits,
  };
  pw::bluetooth::proxy::L2capCoc::CocConfig pwTxConfig = {
      .cid = socketData.txConfig.cid,
      .mtu = socketData.txConfig.mtu,
      .mps = socketData.txConfig.mps,
      .credits = socketData.txConfig.credits,
  };

  uint64_t socketId = socketData.socketId;

  /**
   * The eventCallback will not be invoked from the CHRE thread. It is expected
   * that the caller invokes DramVoteClient::incrementDramVoteCount() and
   * DramVoteClient::decrementDramVoteCount() around use of this function.
   *
   * NOTE: handlePlatformSocketEvent() adds an event to CHRE's event queue. We
   * call forceDramAccess after adding this event to CHRE's event queue to avoid
   * the race condition in which forceDramAccess() is called and CHRE's event
   * queue empties, triggering a call to removeDramAccessVote() right before
   * this event is enqueued.
   */
  auto eventCallback = [socketId](
                           pw::bluetooth::proxy::L2capChannelEvent event) {
    EventLoopManagerSingleton::get()
        ->getBleSocketManager()
        .handlePlatformSocketEvent(socketId, convertPwSocketEventToChre(event));
    // Call after enqueuing event on CHRE's event loop queue
    // TODO(b/429237573): Support enqueueing high power events on CHRE's event
    // queue
    forceDramAccess();
  };

  pw::Result<pw::bluetooth::proxy::L2capCoc> result =
      platformBtSocketResources.getProxyHost().AcquireL2capCoc(
          mSimpleAllocator, socketData.connectionHandle, pwRxConfig, pwTxConfig,
          [this](pw::multibuf::MultiBuf &&payload) {
            handleSocketData(std::move(payload));
          },
          eventCallback);
  if (!result.ok()) {
    LOGE("AcquireL2capCoc failed: %s", result.status().str());
    return;
  }
  mL2capCoc.emplace(std::move(result.value()));
}

bool PlatformBtSocket::isInitialized() {
  return mL2capCoc.has_value();
}

int32_t PlatformBtSocket::sendSocketPacket(
    const void *data, uint16_t length,
    chreBleSocketPacketFreeFunction *freeCallback) {
  // Per CHRE API, if this call results in
  // CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL, do not use the free callback. In
  // this scenario, it is the responsibility of the nanoapp to free the data.
  // The nanoapp may choose to hold on to the data until it receives a
  // CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE event when it can re-attempt the send.
  if (mL2capCoc.value().IsWriteAvailable() != pw::OkStatus()) {
    return CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL;
  }

  auto nonConstData = const_cast<uint8_t *>(static_cast<const uint8_t *>(data));
  pw::ByteSpan byteSpan(reinterpret_cast<std::byte *>(nonConstData), length);

  /**
   * This deleter function can either be called from the CHRE thread, in which
   * case, this code is already running in DRAM, or from the BT Rx thread. If it
   * is called from the BT Rx thread, it is expected that the caller invokes
   * DramVoteClient::incrementDramVoteCount() and
   * DramVoteClient::decrementDramVoteCount() around use of this function.
   *
   * NOTE: freeSocketPacket() adds an event to CHRE's event queue. We call
   * forceDramAccess after adding this event to CHRE's event queue to avoid the
   * race condition in which forceDramAccess() is called and CHRE's event queue
   * empties, triggering a call to removeDramAccessVote() right before this
   * event is enqueued.
   */
  auto deleter = [freeCallback](pw::ByteSpan byteSpan) {
    EventLoopManagerSingleton::get()->getBleSocketManager().freeSocketPacket(
        byteSpan.data(), byteSpan.size(), freeCallback);
    // Call after enqueuing free socket packet event on CHRE's event loop queue
    // TODO(b/429237573): Support enqueueing high power events on CHRE's event
    // queue
    forceDramAccess();
  };
  std::optional<pw::multibuf::MultiBuf> multibuf =
      pw::multibuf::FromSpan(mTxFirstFitAllocator, byteSpan, deleter);

  // If multibuf creation is not successful, the deleter will not be used.
  if (!multibuf.has_value()) {
    LOG_OOM();
    freeCallback(nonConstData, length);
    return CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  }
  pw::bluetooth::proxy::StatusWithMultiBuf status =
      mL2capCoc.value().Write(std::move(*multibuf));
  CHRE_ASSERT(status.status.ok());
  return CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS;
}

}  // namespace chre
