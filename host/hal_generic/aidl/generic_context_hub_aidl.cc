/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "generic_context_hub_aidl.h"

#include "chre_host/fragmented_load_transaction.h"
#include "chre_host/host_protocol_host.h"
#include "permissions_util.h"

namespace aidl {
namespace android {
namespace hardware {
namespace contexthub {

// Aliased for consistency with the way these symbols are referenced in
// CHRE-side code
namespace fbs = ::chre::fbs;

using ::android::chre::FragmentedLoadTransaction;
using ::android::chre::getStringFromByteVector;
using ::android::hardware::contexthub::common::implementation::
    chreToAndroidPermissions;
using ::android::hardware::contexthub::common::implementation::
    kSupportedPermissions;

namespace {
constexpr uint32_t kDefaultHubId = 0;

inline constexpr uint8_t extractChreApiMajorVersion(uint32_t chreVersion) {
  return static_cast<uint8_t>(chreVersion >> 24);
}

inline constexpr uint8_t extractChreApiMinorVersion(uint32_t chreVersion) {
  return static_cast<uint8_t>(chreVersion >> 16);
}

inline constexpr uint16_t extractChrePatchVersion(uint32_t chreVersion) {
  return static_cast<uint16_t>(chreVersion);
}

bool getFbsSetting(const Setting &setting, fbs::Setting *fbsSetting) {
  bool foundSetting = true;

  switch (setting) {
    case Setting::LOCATION:
      *fbsSetting = fbs::Setting::LOCATION;
      break;
    case Setting::AIRPLANE_MODE:
      *fbsSetting = fbs::Setting::AIRPLANE_MODE;
      break;
    case Setting::MICROPHONE:
      *fbsSetting = fbs::Setting::MICROPHONE;
      break;
    default:
      foundSetting = false;
      ALOGE("Setting update with invalid enum value %hhu", setting);
      break;
  }

  return foundSetting;
}

}  // anonymous namespace

::ndk::ScopedAStatus ContextHub::getContextHubs(
    std::vector<ContextHubInfo> *out_contextHubInfos) {
  ::chre::fbs::HubInfoResponseT response;
  bool success = mConnection.getContextHubs(&response);
  if (success) {
    ContextHubInfo hub;
    hub.name = getStringFromByteVector(response.name);
    hub.vendor = getStringFromByteVector(response.vendor);
    hub.toolchain = getStringFromByteVector(response.toolchain);
    hub.id = kDefaultHubId;
    hub.peakMips = response.peak_mips;
    hub.maxSupportedMessageLengthBytes = response.max_msg_len;
    hub.chrePlatformId = response.platform_id;

    uint32_t version = response.chre_platform_version;
    hub.chreApiMajorVersion = extractChreApiMajorVersion(version);
    hub.chreApiMinorVersion = extractChreApiMinorVersion(version);
    hub.chrePatchVersion = extractChrePatchVersion(version);

    hub.supportedPermissions = kSupportedPermissions;

    out_contextHubInfos->push_back(hub);
  }

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::loadNanoapp(int32_t contextHubId,
                                             const NanoappBinary &appBinary,
                                             int32_t transactionId,
                                             bool *_aidl_return) {
  bool success = false;
  if (contextHubId != kDefaultHubId) {
    ALOGE("Invalid ID %" PRId32, contextHubId);
  } else {
    uint32_t targetApiVersion = (appBinary.targetChreApiMajorVersion << 24) |
                                (appBinary.targetChreApiMinorVersion << 16);
    FragmentedLoadTransaction transaction(
        transactionId, appBinary.nanoappId, appBinary.nanoappVersion,
        appBinary.flags, targetApiVersion, appBinary.customBinary);
    success = mConnection.loadNanoapp(transaction);
  }

  *_aidl_return = success;

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::unloadNanoapp(int32_t contextHubId,
                                               int64_t appId,
                                               int32_t transactionId,
                                               bool *_aidl_return) {
  bool success = false;
  if (contextHubId != kDefaultHubId) {
    ALOGE("Invalid ID %" PRId32, contextHubId);
  } else {
    success = mConnection.unloadNanoapp(appId, transactionId);
  }

  *_aidl_return = success;

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::disableNanoapp(int32_t /* contextHubId */,
                                                int64_t appId,
                                                int32_t /* transactionId */,
                                                bool *_aidl_return) {
  ALOGW("Attempted to disable app ID 0x%016" PRIx64 ", but not supported",
        appId);
  *_aidl_return = false;

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::enableNanoapp(int32_t /* contextHubId */,
                                               int64_t appId,
                                               int32_t /* transactionId */,
                                               bool *_aidl_return) {
  ALOGW("Attempted to enable app ID 0x%016" PRIx64 ", but not supported",
        appId);
  *_aidl_return = false;

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::onSettingChanged(Setting setting,
                                                  bool enabled) {
  mSettingEnabled[setting] = enabled;
  fbs::Setting fbsSetting;
  if ((setting != Setting::WIFI_MAIN) && (setting != Setting::WIFI_SCANNING) &&
      getFbsSetting(setting, &fbsSetting)) {
    mConnection.sendSettingChangedNotification(fbsSetting,
                                               toFbsSettingState(enabled));
  }

  bool isWifiMainEnabled = isSettingEnabled(Setting::WIFI_MAIN);
  bool isWifiScanEnabled = isSettingEnabled(Setting::WIFI_SCANNING);
  bool isAirplaneModeEnabled = isSettingEnabled(Setting::AIRPLANE_MODE);

  // Because the airplane mode impact on WiFi is not standardized in Android,
  // we write a specific handling in the Context Hub HAL to inform CHRE.
  // The following definition is a default one, and can be adjusted
  // appropriately if necessary.
  bool isWifiAvailable = isAirplaneModeEnabled
                             ? (isWifiMainEnabled)
                             : (isWifiMainEnabled || isWifiScanEnabled);
  if (!mIsWifiAvailable.has_value() || (isWifiAvailable != mIsWifiAvailable)) {
    mConnection.sendSettingChangedNotification(
        fbs::Setting::WIFI_AVAILABLE, toFbsSettingState(isWifiAvailable));
    mIsWifiAvailable = isWifiAvailable;
  }

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::queryNanoapps(int32_t contextHubId,
                                               bool *_aidl_return) {
  bool success = false;
  if (contextHubId != kDefaultHubId) {
    ALOGE("Invalid ID %" PRId32, contextHubId);
  } else {
    success = mConnection.queryNanoapps();
  }

  *_aidl_return = success;

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::registerCallback(
    int32_t contextHubId, const std::shared_ptr<IContextHubCallback> &cb,
    bool *_aidl_return) {
  bool success = false;
  if (contextHubId != kDefaultHubId) {
    ALOGE("Invalid ID %" PRId32, contextHubId);
  } else {
    std::lock_guard<std::mutex> lock(mCallbackMutex);
    if (mCallback != nullptr) {
      binder_status_t binder_status = AIBinder_unlinkToDeath(
          mCallback->asBinder().get(), mDeathRecipient.get(), this);
      if (binder_status != STATUS_OK) {
        ALOGE("Failed to unlink to death");
      }
    }

    mCallback = cb;

    binder_status_t binder_status =
        AIBinder_linkToDeath(cb->asBinder().get(), mDeathRecipient.get(), this);
    if (binder_status != STATUS_OK) {
      ALOGE("Failed to link to death");
    }

    success = true;
  }

  *_aidl_return = success;

  return ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus ContextHub::sendMessageToHub(
    int32_t contextHubId, const ContextHubMessage &message,
    bool *_aidl_return) {
  bool success = false;
  if (contextHubId != kDefaultHubId) {
    ALOGE("Invalid ID %" PRId32, contextHubId);
  } else {
    success = mConnection.sendMessageToHub(
        message.nanoappId, message.messageType, message.hostEndPoint,
        message.messageBody.data(), message.messageBody.size());
  }

  *_aidl_return = success;

  return ndk::ScopedAStatus::ok();
}

void ContextHub::onNanoappMessage(const ::chre::fbs::NanoappMessageT &message) {
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  if (mCallback != nullptr) {
    ContextHubMessage outMessage;
    outMessage.nanoappId = message.app_id;
    outMessage.hostEndPoint = message.host_endpoint;
    outMessage.messageType = message.message_type;
    outMessage.messageBody = message.message;
    outMessage.permissions = chreToAndroidPermissions(message.permissions);

    std::vector<std::string> messageContentPerms =
        chreToAndroidPermissions(message.message_permissions);
    mCallback->handleContextHubMessage(outMessage, messageContentPerms);
  }
}

void ContextHub::onNanoappListResponse(
    const ::chre::fbs::NanoappListResponseT &response) {
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  if (mCallback != nullptr) {
    std::vector<NanoappInfo> appInfoList;

    for (const std::unique_ptr<::chre::fbs::NanoappListEntryT> &nanoapp :
         response.nanoapps) {
      // TODO: determine if this is really required, and if so, have
      // HostProtocolHost strip out null entries as part of decode
      if (nanoapp == nullptr) {
        continue;
      }

      ALOGV("App 0x%016" PRIx64 " ver 0x%" PRIx32 " permissions 0x%" PRIx32
            " enabled %d system %d",
            nanoapp->app_id, nanoapp->version, nanoapp->permissions,
            nanoapp->enabled, nanoapp->is_system);
      if (!nanoapp->is_system) {
        NanoappInfo appInfo;

        appInfo.nanoappId = nanoapp->app_id;
        appInfo.nanoappVersion = nanoapp->version;
        appInfo.enabled = nanoapp->enabled;
        appInfo.permissions = chreToAndroidPermissions(nanoapp->permissions);

        appInfoList.push_back(appInfo);
      }
    }

    mCallback->handleNanoappInfo(appInfoList);
  }
}

void ContextHub::onTransactionResult(uint32_t transactionId, bool success) {
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  if (mCallback != nullptr) {
    mCallback->handleTransactionResult(transactionId, success);
  }
}

void ContextHub::onContextHubRestarted() {
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mIsWifiAvailable.reset();
  if (mCallback != nullptr) {
    mCallback->handleContextHubAsyncEvent(AsyncEventType::RESTARTED);
  }
}

void ContextHub::onDebugDumpData(const ::chre::fbs::DebugDumpDataT &data) {
  if (mDebugFd == kInvalidFd) {
    ALOGW("Got unexpected debug dump data message");
  } else {
    writeToDebugFile(reinterpret_cast<const char *>(data.debug_str.data()),
                     data.debug_str.size());
  }
}

void ContextHub::onDebugDumpComplete(
    const ::chre::fbs::DebugDumpResponseT & /* response */) {
  std::lock_guard<std::mutex> lock(mDebugDumpMutex);
  if (!mDebugDumpPending) {
    ALOGI("Ignoring duplicate/unsolicited debug dump response");
  } else {
    mDebugDumpPending = false;
    mDebugDumpCond.notify_all();
  }
}

void ContextHub::handleServiceDeath() {
  ALOGI("Context Hub Service died ...");
  std::lock_guard<std::mutex> lock(mCallbackMutex);
  mCallback.reset();
}

void ContextHub::onServiceDied(void *cookie) {
  auto *contexthub = static_cast<ContextHub *>(cookie);
  contexthub->handleServiceDeath();
}

binder_status_t ContextHub::dump(int fd, const char ** /* args */,
                                 uint32_t /* numArgs */) {
  // Timeout inside CHRE is typically 5 seconds, grant 500ms extra here to let
  // the data reach us
  constexpr auto kDebugDumpTimeout = std::chrono::milliseconds(5500);

  mDebugFd = fd;
  if (mDebugFd < 0) {
    ALOGW("Can't dump debug info to invalid fd %d", mDebugFd);
  } else {
    writeToDebugFile("-- Dumping CHRE/ASH debug info --\n");

    ALOGV("Sending debug dump request");
    std::unique_lock<std::mutex> lock(mDebugDumpMutex);
    mDebugDumpPending = true;
    if (!mConnection.requestDebugDump()) {
      ALOGW("Couldn't send debug dump request");
    } else {
      mDebugDumpCond.wait_for(lock, kDebugDumpTimeout,
                              [this]() { return !mDebugDumpPending; });
      if (mDebugDumpPending) {
        ALOGE("Timed out waiting on debug dump data");
        mDebugDumpPending = false;
      }
    }
    writeToDebugFile("\n-- End of CHRE/ASH debug info --\n");

    mDebugFd = kInvalidFd;
    ALOGV("Debug dump complete");
  }

  return STATUS_OK;
}

}  // namespace contexthub
}  // namespace hardware
}  // namespace android
}  // namespace aidl