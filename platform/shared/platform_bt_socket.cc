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
#include "pw_multibuf/from_span.h"

namespace chre {

namespace {

// TODO(b/393485754): determine correct number of credits
constexpr uint16_t kRxAdditionalCredits = 0xFFFF;

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

  pw::Result<pw::bluetooth::proxy::L2capCoc> result =
      platformBtSocketResources.getProxyHost().AcquireL2capCoc(
          mSimpleAllocator, socketData.connectionHandle, pwRxConfig, pwTxConfig,
          [this](pw::multibuf::MultiBuf &&payload) {
            handleSocketData(std::move(payload));
          },
          [this](pw::bluetooth::proxy::L2capChannelEvent event) {
            handleSocketEvent(event);
          });
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
  int32_t result = CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS;

  auto nonConstData = const_cast<uint8_t *>(static_cast<const uint8_t *>(data));
  pw::ByteSpan byteSpan(reinterpret_cast<std::byte *>(nonConstData), length);
  std::optional<pw::multibuf::MultiBuf> multibuf = pw::multibuf::FromSpan(
      mTxFirstFitAllocator, byteSpan, [](pw::ByteSpan) {});
  if (!multibuf.has_value()) {
    LOG_OOM();
    result = CHRE_BLE_SOCKET_SEND_STATUS_FAILURE;
  } else {
    pw::bluetooth::proxy::StatusWithMultiBuf status =
        mL2capCoc.value().Write(std::move(*multibuf));
    if (!status.status.ok()) {
      LOGD("L2CAP COC socket queue full");
      result = CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL;
    }
  }
  // Per CHRE API, if this call results in
  // CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL, do not use the free callback. In
  // this scenario, it is the responsibility of the nanoapp to free the data.
  // The nanoapp may choose to hold on to the data until it receives a
  // CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE event when it can re-attempt the send.
  if (result != CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL) {
    freeCallback(nonConstData, length);
  }
  return result;
}

}  // namespace chre
