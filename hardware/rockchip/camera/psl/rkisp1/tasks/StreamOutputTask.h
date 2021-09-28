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

#ifndef CAMERA3_HAL_STREAMOUTPUTTASK_H_
#define CAMERA3_HAL_STREAMOUTPUTTASK_H_

#include "ExecuteTaskBase.h"
#include "ITaskEventSource.h"

namespace android {
namespace camera2 {
/**
 * \class StreamOutputStage
 * Returns processed request buffers to the framework
 * Listens to PU Task for the buffers that need to be returned
 *
 */
class StreamOutputTask : public ITaskEventListener, public ITaskEventSource {
public:
    StreamOutputTask();
    virtual ~StreamOutputTask();

private:
    /* ITaskEventListener interface*/
    status_t notifyPUTaskEvent(PUTaskMessage *puMsg);
    void cleanListeners();

private:
    int mCaptureDoneCount;
};

}  // namespace camera2
}  // namespace android

#endif  // CAMERA3_HAL_STREAMOUTPUTTASK_H_
