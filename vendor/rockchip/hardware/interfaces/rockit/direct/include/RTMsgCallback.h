/*
 * Copyright 2018 The Android Open Source Project
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

#ifndef ROCKIT_DIRECT_RTMSGCALLBACK_H
#define ROCKIT_DIRECT_RTMSGCALLBACK_H

#include "RTMediaPlayerInterface.h"
#include "RockitPlayer.h"
#include <media/MediaPlayerInterface.h>

namespace android {

class RTMsgCallback : public RTPlayerListener {
 public:
    RTMsgCallback(android::MediaPlayerInterface *player);
    virtual ~RTMsgCallback();
    virtual void notify(int32_t msg, int32_t ext1, int32_t ext2, void* ptr);
private:
    MediaPlayerInterface *mPlayer;
};
}
#endif  // ROCKIT_DIRECT_RTMSGCALLBACK_H
