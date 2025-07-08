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

#include "chre/core/ble_l2cap_coc_socket_data.h"
#include "chre/core/event_loop_manager.h"
#include "test_util.h"

namespace chre {

namespace {

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

}  // namespace chre
