/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "chre/pal/util/wifi_scan_cache.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "chre/platform/log.h"
#include "chre/platform/shared/pal_system_api.h"
#include "chre/util/fixed_size_vector.h"
#include "chre/util/macros.h"
#include "chre/util/nanoapp/wifi.h"
#include "chre/util/optional.h"
#include "chre_api/chre/common.h"
#include "gtest/gtest.h"

namespace {

/************************************************
 *  Prototypes
 ***********************************************/
void chreWifiScanResponseCallback(bool pending, uint8_t errorCode);
void chreWifiScanEventCallback(struct chreWifiScanEvent *event);

struct WifiScanResponse {
  bool pending;
  uint8_t errorCode;
};

/************************************************
 *  Global variables
 ***********************************************/
const chrePalWifiCallbacks gChreWifiPalCallbacks = {
    .scanResponseCallback = chreWifiScanResponseCallback,
    .scanEventCallback = chreWifiScanEventCallback,
};

chre::Optional<WifiScanResponse> gWifiScanResponse;
chre::FixedSizeVector<chreWifiScanResult, CHRE_PAL_WIFI_SCAN_CACHE_CAPACITY>
    gWifiScanResultList;
chre::Optional<chreWifiScanEvent> gExpectedWifiScanEvent;

/************************************************
 *  Test class
 ***********************************************/
class WifiScanCacheTests : public ::testing::Test {
 protected:
  void SetUp() override {
    clearTestState();
    EXPECT_TRUE(chreWifiScanCacheInit(&chre::gChrePalSystemApi,
                                      &gChreWifiPalCallbacks));
  }

  void TearDown() override {
    chreWifiScanCacheDeinit();
  }

  void clearTestState() {
    gExpectedWifiScanEvent.reset();
    gWifiScanResponse.reset();
    while (!gWifiScanResultList.empty()) {
      gWifiScanResultList.pop_back();
    }
  }
};

/************************************************
 *  Private functions
 ***********************************************/
void chreWifiScanResponseCallback(bool pending, uint8_t errorCode) {
  WifiScanResponse response = {
      .pending = pending,
      .errorCode = errorCode,
  };
  gWifiScanResponse = response;
}

void chreWifiScanEventCallback(struct chreWifiScanEvent *event) {
  ASSERT_TRUE(gExpectedWifiScanEvent.has_value());
  EXPECT_EQ(event->version, gExpectedWifiScanEvent->version);
  EXPECT_EQ(event->scanType, gExpectedWifiScanEvent->scanType);
  EXPECT_EQ(event->ssidSetSize, gExpectedWifiScanEvent->ssidSetSize);
  ASSERT_EQ(event->scannedFreqListLen,
            gExpectedWifiScanEvent->scannedFreqListLen);
  if (event->scannedFreqListLen > 0) {
    ASSERT_NE(event->scannedFreqList, nullptr);
    EXPECT_EQ(
        memcmp(gExpectedWifiScanEvent->scannedFreqList, event->scannedFreqList,
               event->scannedFreqListLen * sizeof(uint32_t)),
        0);
  }

  EXPECT_EQ(event->radioChainPref, gExpectedWifiScanEvent->radioChainPref);
  EXPECT_EQ(event->eventIndex, gExpectedWifiScanEvent->eventIndex);
  gExpectedWifiScanEvent->eventIndex++;

  for (uint8_t i = 0; i < event->resultCount; i++) {
    const chreWifiScanResult &result = event->results[i];
    gWifiScanResultList.push_back(result);
  }

  chreWifiScanCacheReleaseScanEvent(event);
}

void beginDefaultWifiCache(const uint32_t *scannedFreqList,
                           uint16_t scannedFreqListLen) {
  chreWifiScanEvent event;
  memset(&event, 0, sizeof(chreWifiScanEvent));
  event.version = CHRE_WIFI_SCAN_EVENT_VERSION;
  event.scanType = CHRE_WIFI_SCAN_TYPE_ACTIVE;
  event.scannedFreqList = scannedFreqList;
  event.scannedFreqListLen = scannedFreqListLen;
  event.radioChainPref = CHRE_WIFI_RADIO_CHAIN_PREF_DEFAULT;
  gExpectedWifiScanEvent = event;

  chreWifiScanCacheScanEventBegin(
      static_cast<enum chreWifiScanType>(gExpectedWifiScanEvent->scanType),
      gExpectedWifiScanEvent->ssidSetSize,
      gExpectedWifiScanEvent->scannedFreqList,
      gExpectedWifiScanEvent->scannedFreqListLen,
      gExpectedWifiScanEvent->radioChainPref, true /* activeScanResult */);
}

void cacheDefaultWifiCacheTest(size_t numEvents,
                               const uint32_t *scannedFreqList,
                               uint16_t scannedFreqListLen) {
  beginDefaultWifiCache(scannedFreqList, scannedFreqListLen);

  chreWifiScanResult result = {};
  for (size_t i = 0; i < numEvents; i++) {
    result.rssi = static_cast<int8_t>(i);
    chreWifiScanCacheScanEventAdd(&result);
  }

  chreWifiScanCacheScanEventEnd(CHRE_ERROR_NONE);

  // Check async response
  EXPECT_TRUE(gWifiScanResponse.has_value());
  EXPECT_EQ(gWifiScanResponse->pending, true);
  ASSERT_EQ(gWifiScanResponse->errorCode, CHRE_ERROR_NONE);

  // Check WiFi results
  size_t numEventsExpected = std::min(
      numEvents, static_cast<size_t>(CHRE_PAL_WIFI_SCAN_CACHE_CAPACITY));
  EXPECT_EQ(gWifiScanResultList.size(), numEventsExpected);
  for (size_t i = 0; i < gWifiScanResultList.size(); i++) {
    // ageMs is not known apriori
    result.ageMs = gWifiScanResultList[i].ageMs;
    result.rssi = static_cast<int8_t>(i);
    EXPECT_EQ(
        memcmp(&gWifiScanResultList[i], &result, sizeof(chreWifiScanResult)),
        0);
  }
}

}  // anonymous namespace

/************************************************
 *  Tests
 ***********************************************/
TEST_F(WifiScanCacheTests, SingleWifiResultTest) {
  cacheDefaultWifiCacheTest(1 /* numEvents */, nullptr /* scannedFreqList */,
                            0 /* scannedFreqListLen */);
}

TEST_F(WifiScanCacheTests, MultiWifiResultTest) {
  cacheDefaultWifiCacheTest(
      CHRE_PAL_WIFI_SCAN_CACHE_MAX_RESULT_COUNT + 1 /* numEvents */,
      nullptr /* scannedFreqList */, 0 /* scannedFreqListLen */);
}

TEST_F(WifiScanCacheTests, WifiResultOverflowTest) {
  cacheDefaultWifiCacheTest(
      CHRE_PAL_WIFI_SCAN_CACHE_CAPACITY + 1 /* numEvents */,
      nullptr /* scannedFreqList */, 0 /* scannedFreqListLen */);
}

TEST_F(WifiScanCacheTests, EmptyWifiResultTest) {
  cacheDefaultWifiCacheTest(0 /* numEvents */, nullptr /* scannedFreqList */,
                            0 /* scannedFreqListLen */);
}

TEST_F(WifiScanCacheTests, FailedWifiCacheTest) {
  beginDefaultWifiCache(nullptr /* scannedFreqList */,
                        0 /* scannedFreqListLen */);

  chreWifiScanCacheScanEventEnd(CHRE_ERROR);

  // Check async response
  EXPECT_TRUE(gWifiScanResponse.has_value());
  EXPECT_FALSE(gWifiScanResponse->pending);
  EXPECT_EQ(gWifiScanResponse->errorCode, CHRE_ERROR);

  EXPECT_EQ(gWifiScanResultList.size(), 0);
}

TEST_F(WifiScanCacheTests, FrequencyListTest) {
  const uint32_t freqList[2] = {5210, 5240};
  cacheDefaultWifiCacheTest(1 /* numEvents */, freqList, ARRAY_SIZE(freqList));
}

TEST_F(WifiScanCacheTests, SequentialWifiResultTest) {
  cacheDefaultWifiCacheTest(1 /* numEvents */, nullptr /* scannedFreqList */,
                            0 /* scannedFreqListLen */);

  clearTestState();
  cacheDefaultWifiCacheTest(1 /* numEvents */, nullptr /* scannedFreqList */,
                            0 /* scannedFreqListLen */);
}