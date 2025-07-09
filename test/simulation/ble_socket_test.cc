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

#include "test_base.h"

#include <array>

#include "test_util.h"

#include "chre/core/ble_l2cap_coc_socket_data.h"
#include "chre/core/event_loop_manager.h"
#include "pw_bluetooth/emboss_util.h"
#include "pw_bluetooth/hci_common.emb.h"
#include "pw_bluetooth/hci_events.emb.h"
#include "pw_bluetooth/hci_h4.emb.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/try.h"

namespace chre {

using ::testing::_;

namespace {

// Populate event header and return writer of the template parameter type.
template <typename EmbossT>
pw::Result<EmbossT> buildEvent(pw::bluetooth::proxy::H4PacketWithHci &h4_packet,
                               pw::bluetooth::emboss::EventCode event_code) {
  size_t parameter_total_size =
      h4_packet.GetHciSpan().size() -
      pw::bluetooth::emboss::EventHeader::IntrinsicSizeInBytes();
  h4_packet.SetH4Type(pw::bluetooth::emboss::H4PacketType::EVENT);

  PW_TRY_ASSIGN(EmbossT view, pw::bluetooth::MakeEmbossWriter<EmbossT>(
                                  h4_packet.GetHciSpan()));

  view.header().event_code().Write(event_code);
  view.header().parameter_total_size().Write(parameter_total_size);

  return view;
}

// Populate command response event header and return writer of the template
// parameter type.
template <typename EmbossT>
pw::Result<EmbossT> buildCommandResponseSuccessEvent(
    pw::bluetooth::proxy::H4PacketWithHci &h4_packet,
    pw::bluetooth::emboss::EventCode event_code) {
  PW_TRY_ASSIGN(EmbossT view, buildEvent<EmbossT>(h4_packet, event_code));

  view.status().Write(pw::bluetooth::emboss::StatusCode::SUCCESS);

  return view;
}

class BleSocketTest : public TestBase {
 public:
  void setupSocket(chreError expectedError) {
    mExpectedSocketConnectionError = expectedError;
    EventLoopManagerSingleton::get()->deferCallback(
        SystemCallbackType::BleSocketConnected, &mSocketData,
        [](uint16_t, void *data, void *extraData) {
          auto socketData = static_cast<BleL2capCocSocketData *>(data);
          auto error = static_cast<chreError *>(extraData);
          EXPECT_EQ(EventLoopManagerSingleton::get()
                        ->getBleSocketManager()
                        .socketConnected(*socketData),
                    *error);
        },
        &mExpectedSocketConnectionError);
  }

  // Send an LE_Read_Buffer_Size (V2) CommandComplete event to the ProxyHost so
  // it can request the reservation of a number of LE ACL send credits.
  pw::Status sendLeReadBufferResponseFromController(
      uint8_t num_credits_to_reserve,
      uint16_t le_acl_data_packet_length = 251) {
    using pw::bluetooth::emboss::EventCode;
    using pw::bluetooth::emboss::LEReadBufferSizeV2CommandCompleteEventWriter;

    std::array<uint8_t,
               LEReadBufferSizeV2CommandCompleteEventWriter::SizeInBytes()>
        hci_arr{};
    hci_arr.fill(0);
    pw::bluetooth::proxy::H4PacketWithHci h4_packet{
        pw::bluetooth::emboss::H4PacketType::EVENT, hci_arr};

    PW_TRY_ASSIGN(LEReadBufferSizeV2CommandCompleteEventWriter view,
                  buildCommandResponseSuccessEvent<
                      LEReadBufferSizeV2CommandCompleteEventWriter>(
                      h4_packet, EventCode::COMMAND_COMPLETE));

    view.command_complete().command_opcode().Write(
        pw::bluetooth::emboss::OpCode::LE_READ_BUFFER_SIZE_V2);
    view.total_num_le_acl_data_packets().Write(num_credits_to_reserve);
    view.le_acl_data_packet_length().Write(le_acl_data_packet_length);

    EXPECT_TRUE(view.Ok());

    mProxyHost.value().HandleH4HciFromController(
        {h4_packet.GetH4Type(), h4_packet.GetHciSpan()});
    return pw::OkStatus();
  }

 protected:
  BleL2capCocSocketData mSocketData = {
      .socketId = 1,
      .endpointId = kDefaultTestNanoappId,
      .connectionHandle = 1,
      .hostClientId = 1,
      .rxConfig =
          L2capCocConfig{.cid = 1, .mtu = 400, .mps = 200, .credits = 2},
      .txConfig =
          L2capCocConfig{.cid = 1, .mtu = 400, .mps = 200, .credits = 2}};

  chreError mExpectedSocketConnectionError = CHRE_ERROR_NONE;

  uint8_t mDefaultMessage[6] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6};
};

class BleSocketTestNanoapp : public TestNanoapp {
 public:
  BleSocketTestNanoapp()
      : TestNanoapp(
            TestNanoappInfo{.perms = NanoappPermissions::CHRE_PERMS_BLE}) {}

  bool start() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_BLE_AVAILABLE,
                                   true /* enable */);
    return true;
  }

  void end() override {
    chreUserSettingConfigureEvents(CHRE_USER_SETTING_BLE_AVAILABLE,
                                   false /* enable */);
  }
};

}  // namespace

TEST_F(BleSocketTest, BleSocketAcceptConnectionTest) {
  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          break;
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  setupSocket(CHRE_ERROR_NONE);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);
}

TEST_F(BleSocketTest, BleSocketNanoappNotFoundTest) {
  uint64_t appId = loadNanoapp(MakeUnique<BleSocketTestNanoapp>());

  constexpr uint64_t kInvalidEndpointId = 1;
  mSocketData.endpointId = kInvalidEndpointId;
  setupSocket(CHRE_ERROR_DESTINATION_NOT_FOUND);
}

TEST_F(BleSocketTest, BleSocketDoNotAcceptConnectionTest) {
  uint64_t appId = loadNanoapp(MakeUnique<BleSocketTestNanoapp>());

  setupSocket(CHRE_ERROR);
}

TEST_F(BleSocketTest, BleSocketSendTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);

  struct socketSendData {
    void *data;
    uint16_t length;
    chreBleSocketPacketFreeFunction *freeCallback;
  };

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          mSocketId = event->socketId;
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<socketSendData *>(event->data);
              int32_t status = chreBleSocketSend(
                  mSocketId, data->data, data->length, data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }

   private:
    uint64_t mSocketId = 0;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  setupSocket(CHRE_ERROR_NONE);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  // Provide ACL credits to ProxyHost to allow the L2capChannel to start sending
  // packets.
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);
  sendLeReadBufferResponseFromController(1);

  socketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  int32_t status = 0;
  waitForEvent(SOCKET_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS);
  // Even though the multibuf is destroyed immediately in this case, the free
  // callback is handled on the event loop thread and will occur after the
  // SOCKET_SEND event
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
}

TEST_F(BleSocketTest, BleSocketSendNoSocketFoundTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);

  struct socketSendData {
    void *data;
    uint16_t length;
    chreBleSocketPacketFreeFunction *freeCallback;
  };

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<socketSendData *>(event->data);
              int32_t status = chreBleSocketSend(1, data->data, data->length,
                                                 data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  socketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  // Free callback is invoked synchronously on socket send failure
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
  int32_t status = 0;
  waitForEvent(SOCKET_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_FAILURE);
}

TEST_F(BleSocketTest, BleSocketSendQueueFullTest) {
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND, 0);
  CREATE_CHRE_TEST_EVENT(SOCKET_SEND_FREE_CALLBACK, 1);
  CREATE_CHRE_TEST_EVENT(SOCKET_RETRY_SEND, 2);

  struct socketSendData {
    void *data;
    uint16_t length;
    chreBleSocketPacketFreeFunction *freeCallback;
  };

  class App : public BleSocketTestNanoapp {
   public:
    void handleEvent(uint32_t, uint16_t eventType,
                     const void *eventData) override {
      switch (eventType) {
        case CHRE_EVENT_BLE_SOCKET_CONNECTION: {
          auto *event =
              static_cast<const struct chreBleSocketConnectionEvent *>(
                  eventData);
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_CONNECTION, event->socketId);
          chreBleSocketAccept(event->socketId);
          mSocketId = event->socketId;
          break;
        }
        case CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE: {
          TestEventQueueSingleton::get()->pushEvent(
              CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE);
          int32_t status =
              chreBleSocketSend(mSocketId, mSendData.data, mSendData.length,
                                mSendData.freeCallback);
          TestEventQueueSingleton::get()->pushEvent(SOCKET_RETRY_SEND, status);
          break;
        }
        case CHRE_EVENT_TEST_EVENT: {
          auto event = static_cast<const TestEvent *>(eventData);
          switch (event->type) {
            case SOCKET_SEND: {
              auto data = static_cast<socketSendData *>(event->data);
              mSendData = *data;
              int32_t status = chreBleSocketSend(
                  mSocketId, data->data, data->length, data->freeCallback);
              TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND, status);
              break;
            }
          }
        }
      }
    }

   private:
    uint64_t mSocketId = 0;

    socketSendData mSendData;
  };

  uint64_t appId = loadNanoapp(MakeUnique<App>());

  setupSocket(CHRE_ERROR_NONE);
  waitForEvent(CHRE_EVENT_BLE_SOCKET_CONNECTION);

  socketSendData data = {
      .data = mDefaultMessage,
      .length = 6,
      .freeCallback = [](void *, uint16_t) {
        TestEventQueueSingleton::get()->pushEvent(SOCKET_SEND_FREE_CALLBACK);
      }};
  int32_t status = 0;

  // TODO(b/430672746): 5 is the hard coded queue size of an L2CAP channel.
  // Revisit this number when https://pwbug.dev/349700888 has been addressed.
  for (size_t i = 0; i < 5; i++) {
    sendEventToNanoapp(appId, SOCKET_SEND, data);
    waitForEvent(SOCKET_SEND, &status);
    EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS);
  }
  // The 6th socket send request should result in the queue full status
  sendEventToNanoapp(appId, SOCKET_SEND, data);
  waitForEvent(SOCKET_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_QUEUE_FULL);

  // Provide ACL credits to ProxyHost to allow the L2capChannel to start sending
  // packets.
  EXPECT_CALL(mMockBtOffload, sendToController(_)).Times(1);
  sendLeReadBufferResponseFromController(1);

  // First packet in queue is sent and its freeCallback is invoked.
  waitForEvent(SOCKET_SEND_FREE_CALLBACK);
  // Callback notifying CHRE that second callback is available
  waitForEvent(CHRE_EVENT_BLE_SOCKET_SEND_AVAILABLE);
  // Nanoapp successfully re-sends packet
  waitForEvent(SOCKET_RETRY_SEND, &status);
  EXPECT_EQ(status, CHRE_BLE_SOCKET_SEND_STATUS_SUCCESS);
}

}  // namespace chre
