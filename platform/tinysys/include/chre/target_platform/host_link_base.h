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

#ifndef CHRE_PLATFORM_TINYSYS_HOST_LINK_BASE_H_
#define CHRE_PLATFORM_TINYSYS_HOST_LINK_BASE_H_

#include <cinttypes>
#include <cstddef>

namespace chre {
/**
 * @brief Platform specific host link.
 */
class HostLinkBase {
 public:
  HostLinkBase() = default;
  /**
   * Implements the IPC message receive handler.
   *
   * @param cookie An opaque pointer that was provided to the IPC driver during
   *        callback registration.
   * @param message The host message sent to CHRE.
   * @param messageLen The host message length in bytes.
   */
  static void receive(void * /*cookie*/, void * /*message*/,
                      int /*messageLen*/);
  /**
   * Send a message to the host.
   *
   * @param data The message to host payload.
   * @param dataLen Size of the message payload in bytes.
   * @return true if the operation succeeds, false otherwise.
   */
  bool send(uint8_t * /*data*/, size_t /*dataLen*/);

  /**
   * Sends a request to the host for a time sync message.
   */
  static void sendTimeSyncRequest();
  /**
   * Enqueues a V2 log message to be sent to the host.
   *
   * @param logMessage Pointer to a buffer that has the log message. Note that
   * the message might be encoded
   * @param logMessageSize length of the log message buffer
   * @param numLogsDropped the number of logs dropped since CHRE started
   */
  void sendLogMessageV2(const uint8_t * /*logMessage*/,
                        size_t /*logMessageSize*/,
                        uint32_t /*num_logs_dropped*/);
};
}  // namespace chre

#endif  // CHRE_PLATFORM_TINYSYS_HOST_LINK_BASE_H_