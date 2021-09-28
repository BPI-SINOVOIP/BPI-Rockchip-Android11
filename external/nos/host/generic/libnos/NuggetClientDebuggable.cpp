/*
 * Copyright 2020 The Android Open Source Project
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

#include <nos/NuggetClientDebuggable.h>
#include <limits>
#include <nos/transport.h>
#include <application.h>

namespace nos {

NuggetClientDebuggable::NuggetClientDebuggable(request_cb_t req_fn, response_cb_t resp_fn)
  : request_cb_(req_fn), response_cb_(resp_fn) {}

NuggetClientDebuggable::NuggetClientDebuggable(const std::string& device_name,
                                               request_cb_t req_fn, response_cb_t resp_fn)
  : NuggetClient(device_name), request_cb_(req_fn), response_cb_(resp_fn) {}

NuggetClientDebuggable::NuggetClientDebuggable(const char* device_name,
                                               request_cb_t req_fn, response_cb_t resp_fn)
  : NuggetClient(device_name), request_cb_(req_fn), response_cb_(resp_fn) {}

uint32_t NuggetClientDebuggable::CallApp(uint32_t appId, uint16_t arg,
                                         const std::vector<uint8_t>& request,
                                         std::vector<uint8_t>* response) {
  if (!open_) {
    return APP_ERROR_IO;
  }

  if (request.size() > std::numeric_limits<uint32_t>::max()) {
    return APP_ERROR_TOO_MUCH;
  }

  const uint32_t requestSize = request.size();
  uint32_t replySize = 0;
  uint8_t* replyData = nullptr;

  if (response != nullptr) {
    response->resize(response->capacity());
    replySize = response->size();
    replyData = response->data();
  }

  if (request_cb_) {
    (request_cb_)(request);
  }

  uint32_t status_code = nos_call_application(&device_, appId, arg,
                                              request.data(), requestSize,
                                              replyData, &replySize);

  if (response != nullptr) {
    response->resize(replySize);
    if (response_cb_) {
      (response_cb_)(status_code, *response);
    }
  }

  return status_code;
}

}  // namespace nos
