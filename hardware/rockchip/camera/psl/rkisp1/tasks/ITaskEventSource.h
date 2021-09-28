/*
 * Copyright (C) 2014-2017 Intel Corporation.
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

#ifndef CAMERA3_HAL_ITASKEVENTSOURCE_H_
#define CAMERA3_HAL_ITASKEVENTSOURCE_H_

#include <map>
#include <list>
#include <mutex>
#include <utils/Errors.h> // status_t
#include "ITaskEventListener.h"

namespace android {
namespace camera2 {

class ITaskEventListener;

/**
 * \class ITaskEventSource
 * An interface class to be implemented by Tasks that
 * will send events to other Tasks.
 */
class ITaskEventSource {
public:
    ITaskEventSource() {}
    virtual ~ITaskEventSource() {}

    virtual status_t attachListener(ITaskEventListener *aListener,
                                    ITaskEventListener::PUTaskEventType event);
    virtual status_t notifyListeners(ITaskEventListener::PUTaskMessage *msg);
    virtual void cleanListener();

private:
    std::mutex   mListenerLock;  /*!< Protects accesses to the Listener management variables */
    typedef std::list<ITaskEventListener*> listener_list_t;
    std::map<ITaskEventListener::PUTaskEventType, listener_list_t> mListeners;

};

} // namespace camera2
} // namespace android

#endif
