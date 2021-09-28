/*
 * Copyright (C) 2016-2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "ICaptureEventSource"

#include "ICaptureEventSource.h"
#include "LogHelper.h"

namespace android {
namespace camera2 {

/**
 * Attach a Listening client to a particular event
 *
 * @param observer interface pointer to attach
 * @param event concrete event to listen to
 */
status_t
ICaptureEventSource::attachListener(ICaptureEventListener *observer)
{
    LOGD("@%s: %p", __FUNCTION__, observer);
    if (observer == nullptr)
        return BAD_VALUE;
    std::lock_guard<std::mutex> l(mListenerLock);

    mListeners.push_back(observer);

    return OK;
}

/**
 * Detach all observers interface
 */
void
ICaptureEventSource::cleanListener()
{
    LOGD("@%s", __FUNCTION__);

    std::lock_guard<std::mutex> l(mListenerLock);

    mListeners.clear();
}

status_t
ICaptureEventSource::notifyListeners(ICaptureEventListener::CaptureMessage *msg)
{
    LOGD("@%s", __FUNCTION__);
    bool ret = false;
    std::lock_guard<std::mutex> l(mListenerLock);
    for (const auto &listener : mListeners) {
        ret |= listener->notifyCaptureEvent(msg);
    }

    return ret;
}

} // namespace camera2
} // namespace android
