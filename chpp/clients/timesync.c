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

#include "chpp/clients/timesync.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "chpp/app.h"
#include "chpp/clients.h"
#include "chpp/common/timesync.h"
#include "chpp/log.h"
#include "chpp/memory.h"
#include "chpp/time.h"
#include "chpp/transport.h"

#include "chpp/clients/discovery.h"

/************************************************
 *  Private Definitions
 ***********************************************/

/**
 * Structure to maintain state for the Timesync client and its Request/Response
 * (RR) functionality.
 */
struct ChppTimesyncClientState {
  struct ChppClientState client;                // Timesync client state
  struct ChppRequestResponseState getTimesync;  // Request response state

  struct ChppTimesyncResult timesyncResult;  // Result of getTimesync
};

/************************************************
 *  Public Functions
 ***********************************************/

void chppTimesyncClientInit(struct ChppAppState *context) {
  CHPP_LOGD("Timesync client init");

  context->timesyncClientContext =
      chppMalloc(sizeof(struct ChppTimesyncClientState));
  CHPP_NOT_NULL(context->timesyncClientContext);

  context->timesyncClientContext->client.appContext = context;
  chppClientInit(&context->timesyncClientContext->client, CHPP_HANDLE_TIMESYNC);
  context->timesyncClientContext->timesyncResult.error =
      CHPP_APP_ERROR_UNSPECIFIED;
  context->timesyncClientContext->client.openState = CHPP_OPEN_STATE_OPENED;
}

void chppTimesyncClientDeinit(struct ChppAppState *context) {
  CHPP_LOGD("Timesync client deinit");

  CHPP_NOT_NULL(context->timesyncClientContext);
  chppClientDeinit(&context->timesyncClientContext->client);
  CHPP_FREE_AND_NULLIFY(context->timesyncClientContext);
}

bool chppDispatchTimesyncServiceResponse(struct ChppAppState *context,
                                         const uint8_t *buf, size_t len) {
  CHPP_LOGD("Timesync client dispatch service response");

  CHPP_NOT_NULL(context->timesyncClientContext);
  CHPP_NOT_NULL(buf);

  if (len != sizeof(struct ChppTimesyncResponse)) {
    context->timesyncClientContext->timesyncResult.error =
        CHPP_APP_ERROR_INVALID_LENGTH;
    return false;
  }

  const struct ChppTimesyncResponse *response =
      (const struct ChppTimesyncResponse *)buf;
  chppClientTimestampResponse(&context->timesyncClientContext->getTimesync,
                              &response->header);

  uint64_t rttNs = context->timesyncClientContext->getTimesync.responseTimeNs -
                   context->timesyncClientContext->getTimesync.requestTimeNs;
  if (rttNs < context->timesyncClientContext->timesyncResult.rttNs) {
    // A more accurate measurement has arrived
    context->timesyncClientContext->timesyncResult.rttNs = rttNs;
    context->timesyncClientContext->timesyncResult.offsetNs =
        (int64_t)(response->timeNs -
                  context->timesyncClientContext->getTimesync.requestTimeNs -
                  context->timesyncClientContext->timesyncResult.rttNs / 2);
  }

  CHPP_LOGI(
      "Timesync client processed response. request t=%" PRIu64
      ", response t=%" PRIu64 ", service t=%" PRIu64 ", req2srv=%" PRIu64
      ", srv2res=%" PRIi64 ", offset=%" PRIi64 ", rtt=%" PRIi64 ", updated=%s",
      context->timesyncClientContext->getTimesync.requestTimeNs,
      context->timesyncClientContext->getTimesync.responseTimeNs,
      response->timeNs,
      (int64_t)(response->timeNs -
                context->timesyncClientContext->getTimesync.requestTimeNs),
      (int64_t)(context->timesyncClientContext->getTimesync.responseTimeNs -
                response->timeNs),
      context->timesyncClientContext->timesyncResult.offsetNs,
      context->timesyncClientContext->timesyncResult.rttNs,
      (context->timesyncClientContext->timesyncResult.rttNs == rttNs) ? "yes"
                                                                      : "no");

  // Notify waiting (synchronous) client
  chppMutexLock(&context->timesyncClientContext->client.responseMutex);
  context->timesyncClientContext->client.responseReady = true;
  chppConditionVariableSignal(
      &context->timesyncClientContext->client.responseCondVar);
  chppMutexUnlock(&context->timesyncClientContext->client.responseMutex);

  return true;
}

struct ChppTimesyncResult chppGetTimesync(struct ChppAppState *context) {
  CHPP_LOGI("Running chppGetTimesync at time~=%" PRIu64 " with %d measurements",
            chppGetCurrentTimeNs(),
            CHPP_CLIENT_TIMESYNC_DEFAULT_MEASUREMENT_COUNT);

  if (!chppWaitForDiscoveryComplete(context,
                                    CHPP_DISCOVERY_DEFAULT_TIMEOUT_MS)) {
    static const struct ChppTimesyncResult result = {
        .error = CHPP_APP_ERROR_NOT_READY,
    };
    return result;
  }

  if (context->timesyncClientContext->timesyncResult.error ==
      CHPP_APP_ERROR_BLOCKED) {
    CHPP_LOGE("Timesync cannot be run while another is in progress");
    CHPP_DEBUG_ASSERT(false);

  } else {
    context->timesyncClientContext->timesyncResult.error =
        CHPP_APP_ERROR_BLOCKED;  // Indicates a measurement is in progress
    context->timesyncClientContext->timesyncResult.rttNs = UINT64_MAX;

    uint8_t i = 0;
    while (i++ < CHPP_CLIENT_TIMESYNC_DEFAULT_MEASUREMENT_COUNT &&
           context->timesyncClientContext->timesyncResult.error ==
               CHPP_APP_ERROR_BLOCKED) {
      struct ChppAppHeader *request =
          chppAllocClientRequestCommand(&context->timesyncClientContext->client,
                                        CHPP_TIMESYNC_COMMAND_GETTIME);
      size_t requestLen = sizeof(request);

      if (request == NULL) {
        context->timesyncClientContext->timesyncResult.error =
            CHPP_APP_ERROR_OOM;
        CHPP_LOG_OOM();

      } else if (!chppSendTimestampedRequestAndWait(
                     &context->timesyncClientContext->client,
                     &context->timesyncClientContext->getTimesync, request,
                     requestLen)) {
        context->timesyncClientContext->timesyncResult.error =
            CHPP_APP_ERROR_UNSPECIFIED;
      }
    }

    if (context->timesyncClientContext->timesyncResult.error ==
        CHPP_APP_ERROR_BLOCKED) {
      context->timesyncClientContext->timesyncResult.error =
          CHPP_APP_ERROR_NONE;
    }
  }

  return context->timesyncClientContext->timesyncResult;
}
