/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef HARDWARE_GOOGLE_PIXEL_PIXELSTATS_DROPDETECT_H
#define HARDWARE_GOOGLE_PIXEL_PIXELSTATS_DROPDETECT_H

#include <utils/StrongPointer.h>
#include "chre_host/host_protocol_host.h"
#include "chre_host/socket_client.h"

namespace android {
namespace hardware {
namespace google {
namespace pixel {

class DropDetect : public android::chre::SocketClient::ICallbacks,
                   public android::chre::IChreMessageHandlers,
                   public android::chre::SocketClient {
  public:
    static android::sp<DropDetect> start(const uint64_t drop_detect_app_id = 0x476f6f676c001010ULL,
                                         const char *const chre_socket = "chre");
    void onConnected() override;
    void onMessageReceived(const void *data, size_t length) override;

    void handleNanoappMessage(const ::chre::fbs::NanoappMessageT &message) override;
    void handleNanoappListResponse(const ::chre::fbs::NanoappListResponseT &response) override;

  private:
    DropDetect(const uint64_t drop_detect_app_id);

    const uint64_t kDropDetectAppId;
};

}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_PIXELSTATS_DROPDETECT_H
