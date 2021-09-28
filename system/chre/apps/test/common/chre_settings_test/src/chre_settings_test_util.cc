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

#include "chre_settings_test.nanopb.h"
#include "chre_settings_test_manager.h"

#include "chre/util/nanoapp/callbacks.h"
#include "chre/util/nanoapp/log.h"

#define LOG_TAG "ChreSettingsTest"

namespace chre {

namespace settings_test {

void sendTestResultToHost(uint16_t hostEndpointId, bool success) {
  // Unspecified endpoint is not allowed in chreSendMessageToHostEndpoint.
  if (hostEndpointId == CHRE_HOST_ENDPOINT_UNSPECIFIED) {
    hostEndpointId = CHRE_HOST_ENDPOINT_BROADCAST;
  }

  chre_settings_test_TestResult result =
      chre_settings_test_TestResult_init_default;
  result.has_code = true;
  result.code = success ? chre_settings_test_TestResult_Code_PASSED
                        : chre_settings_test_TestResult_Code_FAILED;
  size_t size;
  if (!pb_get_encoded_size(&size, chre_settings_test_TestResult_fields,
                           &result)) {
    LOGE("Failed to get message size");
  } else {
    pb_byte_t *bytes = static_cast<pb_byte_t *>(chreHeapAlloc(size));
    if (bytes == nullptr) {
      LOG_OOM();
    } else {
      pb_ostream_t stream = pb_ostream_from_buffer(bytes, size);
      if (!pb_encode(&stream, chre_settings_test_TestResult_fields, &result)) {
        LOGE("Failed to encode test result error %s", PB_GET_ERROR(&stream));
        chreHeapFree(bytes);
      } else {
        chreSendMessageToHostEndpoint(
            bytes, size, chre_settings_test_MessageType_TEST_RESULT,
            hostEndpointId, heapFreeMessageCallback);
      }
    }
  }
}

void sendEmptyMessageToHost(uint16_t hostEndpointId, uint32_t messageType) {
  // Unspecified endpoint is not allowed in chreSendMessageToHostEndpoint.
  if (hostEndpointId == CHRE_HOST_ENDPOINT_UNSPECIFIED) {
    hostEndpointId = CHRE_HOST_ENDPOINT_BROADCAST;
  }

  chreSendMessageToHostEndpoint(nullptr /* message */, 0 /* messageSize */,
                                messageType, hostEndpointId,
                                nullptr /* freeCallback */);
}

}  // namespace settings_test

}  // namespace chre
