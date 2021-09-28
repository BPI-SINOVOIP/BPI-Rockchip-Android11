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

#ifndef CAMERA3_HAL_TASKEVENTLISTENER_H_
#define CAMERA3_HAL_TASKEVENTLISTENER_H_


#include "Camera3Request.h"

namespace android {
namespace camera2 {

// Forward-declarations
struct ProcTaskMsg;

/**
 * \class ITaskEventListener
 *
 * Abstract interface implemented by Tasks that are interested on receiving
 * notifications from the previous task via events.
 *
 * An example event is a buffer handling completion in the previous task.
 */
class ITaskEventListener {
public:

    enum PUTaskId {
        PU_TASK_MSG_ID_EVENT = 0,
        PU_TASK_MSG_ID_ERROR,
        PU_TASK_NOT_SET
    };

    enum PUTaskEventType {
        PU_TASK_EVENT_BUFFER_COMPLETE = 0,
        PU_TASK_EVENT_JPEG_BUFFER_COMPLETE,
        PU_TASK_EVENT_MAX
    };

    // For MESSAGE_ID_EVENT
    struct PUTaskEvent {
        PUTaskEventType type;
        std::shared_ptr<CameraBuffer> buffer;
        Camera3Request *request;
        std::shared_ptr<CameraBuffer> jpegInputbuffer;

        PUTaskEvent() :
            type(PU_TASK_EVENT_MAX),
            request(nullptr) {}
    };

    // For MESSAGE_ID_ERROR
    struct PUTaskError {
        status_t code;

        PUTaskError() :
            code(0) {}
    };

    struct PUTaskMessage {
        PUTaskId   id;
        PUTaskEvent event;
        PUTaskError error;

        PUTaskMessage() :
            id(PU_TASK_NOT_SET) {}
    };

    virtual ~ITaskEventListener() {}

    // To be implemented by listener
    virtual status_t notifyPUTaskEvent(PUTaskMessage *msg) = 0;


    virtual status_t settings(ProcTaskMsg &msg);
    virtual void cleanListeners();
};

} // namespace camera2
} // namespace android

#endif  // CAMERA3_HAL_TASKEVENTLISTENER_H_
