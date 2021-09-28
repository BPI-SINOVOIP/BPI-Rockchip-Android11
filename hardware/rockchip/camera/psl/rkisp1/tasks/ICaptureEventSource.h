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

#ifndef CAMERA3_HAL_ICaptureEventSource_H_
#define CAMERA3_HAL_ICaptureEventSource_H_

#include <vector>
#include <utils/Errors.h> // status_t
#include "CaptureUnit.h"

namespace android {
namespace camera2 {

/**
 * \class ICaptureEventSource
 * An interface class to be implemented by Tasks that
 * will send events to other Tasks.
 */
class ICaptureEventSource {
public:
    ICaptureEventSource() {}
    virtual ~ICaptureEventSource() {}

    virtual status_t attachListener(ICaptureEventListener *aListener);
    virtual status_t notifyListeners(ICaptureEventListener::CaptureMessage *msg);
    virtual void cleanListener();

private:
    std::mutex   mListenerLock;  /* Protects mListeners */
    std::vector<ICaptureEventListener *> mListeners;

};

} // namespace camera2
} // namespace android

#endif
