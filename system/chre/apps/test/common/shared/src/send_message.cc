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

#include <chre.h>
#include <pb_encode.h>
#include <cinttypes>

#include "chre_test_common.nanopb.h"

#include "chre/util/nanoapp/callbacks.h"
#include "chre/util/nanoapp/log.h"

#define LOG_TAG "[TestShared]"

namespace chre {

namespace test_shared {

void sendTestResultToHost(uint16_t hostEndpointId, uint32_t messageType,
                          bool success) {
  // Unspecified endpoint is not allowed in chreSendMessageToHostEndpoint.
  if (hostEndpointId == CHRE_HOST_ENDPOINT_UNSPECIFIED) {
    hostEndpointId = CHRE_HOST_ENDPOINT_BROADCAST;
    LOGE("Unspecified endpoint ID is not allowed");
    success = false;
  }

  chre_test_common_TestResult result = chre_test_common_TestResult_init_default;
  result.has_code = true;
  result.code = success ? chre_test_common_TestResult_Code_PASSED
                        : chre_test_common_TestResult_Code_FAILED;
  size_t size;
  if (!pb_get_encoded_size(&size, chre_test_common_TestResult_fields,
                           &result)) {
    LOGE("Failed to get message size");
  } else {
    pb_byte_t *bytes = static_cast<pb_byte_t *>(chreHeapAlloc(size));
    if (bytes == nullptr) {
      LOG_OOM();
    } else {
      pb_ostream_t stream = pb_ostream_from_buffer(bytes, size);
      if (!pb_encode(&stream, chre_test_common_TestResult_fields, &result)) {
        LOGE("Failed to encode test result error %s", PB_GET_ERROR(&stream));
        chreHeapFree(bytes);
      } else {
        chreSendMessageToHostEndpoint(bytes, size, messageType, hostEndpointId,
                                      heapFreeMessageCallback);
      }
    }
  }

  // Abort to ensure test does not continue
  if (!success) {
    chreAbort(0);
  }
}

void sendEmptyMessageToHost(uint16_t hostEndpointId, uint32_t messageType) {
  // Unspecified endpoint is not allowed in chreSendMessageToHostEndpoint.
  if (hostEndpointId == CHRE_HOST_ENDPOINT_UNSPECIFIED) {
    hostEndpointId = CHRE_HOST_ENDPOINT_BROADCAST;
    LOGE("Unspecified endpoint ID is not allowed");
    // TODO: Send failure message to host
    return;
  }

  chreSendMessageToHostEndpoint(nullptr /* message */, 0 /* messageSize */,
                                messageType, hostEndpointId,
                                nullptr /* freeCallback */);
}

}  // namespace test_shared

}  // namespace chre
