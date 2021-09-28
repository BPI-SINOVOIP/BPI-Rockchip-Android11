/*
 * Copyright (C) 2015-2017 Intel Corporation.
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

#define LOG_TAG "RKISP2ITaskEventSource"

#include "RKISP2ITaskEventSource.h"
#include "LogHelper.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

/**
 * Attach a Listening client to a particular event
 *
 * @param observer interface pointer to attach
 * @param event concrete event to listen to
 */
status_t
RKISP2ITaskEventSource::attachListener(RKISP2ITaskEventListener *observer,
                                 RKISP2ITaskEventListener::PUTaskEventType event)
{
    LOGD("@%s: %p to event type %d", __FUNCTION__, observer, event);
    status_t status = NO_ERROR;
    if (observer == nullptr)
        return BAD_VALUE;
    std::lock_guard<std::mutex> l(mListenerLock);
    // Check event to be in the range of allowed events.
    if ((event < RKISP2ITaskEventListener::PU_TASK_EVENT_BUFFER_COMPLETE ) ||
        (event > RKISP2ITaskEventListener::PU_TASK_EVENT_MAX)) {
        LOGE("Event is outside the range of allowed events: %d", event);
        return BAD_VALUE;
    }

    // Check if we have any listener registered to this event
    std::map<RKISP2ITaskEventListener::PUTaskEventType, listener_list_t>::
            iterator itListener = mListeners.find(event);
    if (itListener == mListeners.end()) {
        // First time someone registers for this event
        listener_list_t theList;
        theList.push_back(observer);
        mListeners.insert(std::make_pair(event, theList));
        return NO_ERROR;
    }

    // Now we will have more than one listener to this event
    listener_list_t &theList = itListener->second;
    for (const auto &listener : theList)
        if (listener == observer) {
            LOGW("listener previously added, ignoring");
            return ALREADY_EXISTS;
        }

    theList.push_back(observer);

    itListener->second = theList;
    return status;
}

/**
 * Detach all bservers interface
 */
void
RKISP2ITaskEventSource::cleanListener()
{
    LOGD("@%s", __FUNCTION__);

    std::lock_guard<std::mutex> l(mListenerLock);

    for (auto &listener : mListeners)
        listener.second.clear();

    mListeners.clear();
}


status_t
RKISP2ITaskEventSource::notifyListeners(RKISP2ITaskEventListener::PUTaskMessage *msg)
{
    LOGD("@%s", __FUNCTION__);
    bool ret = false;
    std::lock_guard<std::mutex> l(mListenerLock);
    if (mListeners.size() > 0) {
        std::map<RKISP2ITaskEventListener::PUTaskEventType, listener_list_t>::
                iterator it = mListeners.find(msg->event.type);
        if (it != mListeners.end()) {
            listener_list_t &theList = it->second;
            for (const auto &listener : theList)
                ret |= listener->notifyPUTaskEvent((RKISP2ITaskEventListener::PUTaskMessage*)msg);
        }
    }

    return ret;
}

} // namespace rkisp2
} // namespace camera2
} // namespace android
