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

#ifndef CAMERA3_HAL_RKISP2ITaskEventSource_H_
#define CAMERA3_HAL_RKISP2ITaskEventSource_H_

#include <map>
#include <list>
#include <mutex>
#include <utils/Errors.h> // status_t
#include "RKISP2ITaskEventListener.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

class RKISP2ITaskEventListener;

/**
 * \class RKISP2ITaskEventSource
 * An interface class to be implemented by Tasks that
 * will send events to other Tasks.
 */
class RKISP2ITaskEventSource {
public:
    RKISP2ITaskEventSource() {}
    virtual ~RKISP2ITaskEventSource() {}

    virtual status_t attachListener(RKISP2ITaskEventListener *aListener,
                                    RKISP2ITaskEventListener::PUTaskEventType event);
    virtual status_t notifyListeners(RKISP2ITaskEventListener::PUTaskMessage *msg);
    virtual void cleanListener();

private:
    std::mutex   mListenerLock;  /*!< Protects accesses to the Listener management variables */
    typedef std::list<RKISP2ITaskEventListener*> listener_list_t;
    std::map<RKISP2ITaskEventListener::PUTaskEventType, listener_list_t> mListeners;

};

} // namespace rkisp2
} // namespace camera2
} // namespace android

#endif
