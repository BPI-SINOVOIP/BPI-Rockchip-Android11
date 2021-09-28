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

#ifndef NOS_NUGGET_CLIENT_DEBUGGABLE_H
#define NOS_NUGGET_CLIENT_DEBUGGABLE_H

#include <cstdint>
#include <string>
#include <vector>

#include <nos/device.h>
#include <nos/NuggetClient.h>

namespace nos {

/**
 * This adds some debug functions around NuggetClient::CallApp()
 */
class NuggetClientDebuggable : public NuggetClient {
public:

  using request_cb_t = std::function<void(const std::vector<uint8_t>&)>;
  using response_cb_t = std::function<void(uint32_t, const std::vector<uint8_t>&)>;

  /* Need to pass the base constructor params up */
  NuggetClientDebuggable(request_cb_t req_cb_ = 0, response_cb_t resp_cb_ = 0);
  NuggetClientDebuggable(const std::string& device_name,
                         request_cb_t req_cb_ = 0, response_cb_t resp_cb_ = 0);
  NuggetClientDebuggable(const char* device_name,
                         request_cb_t req_cb_ = 0, response_cb_t resp_cb_ = 0);

  /* We'll override this */
  uint32_t CallApp(uint32_t appId, uint16_t arg,
                   const std::vector<uint8_t>& request,
                   std::vector<uint8_t>* response) override;


private:
  request_cb_t request_cb_;
  response_cb_t response_cb_;
};

} // namespace nos

#endif // NOS_NUGGET_CLIENT_DEBUGGABLE_H
