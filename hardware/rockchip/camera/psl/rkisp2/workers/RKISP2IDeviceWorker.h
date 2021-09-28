/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef PSL_RKISP1_WORKERS_RKISP2IDeviceWorker_H_
#define PSL_RKISP1_WORKERS_RKISP2IDeviceWorker_H_

#include "common/PollerThread.h"
#include "RKISP2RequestCtrlState.h"
#include "RKISP2GraphConfig.h"
#include "tasks/RKISP2ExecuteTaskBase.h"
#include "Camera3Request.h"
#include "IErrorCallback.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

enum DeviceMessageId {
    MESSAGE_ID_EXIT = 0,
    MESSAGE_COMPLETE_REQ,
    MESSAGE_ID_POLL,
    MESSAGE_ID_POLL_META,
    MESSAGE_ID_FLUSH,
    MESSAGE_ID_MAX
};

const element_value_t ImguMsg_stringEnum[] = {
    {"MESSAGE_ID_EXIT", MESSAGE_ID_EXIT },
    {"MESSAGE_COMPLETE_REQ", MESSAGE_COMPLETE_REQ },
    {"MESSAGE_ID_POLL", MESSAGE_ID_POLL },
    {"MESSAGE_ID_POLL_META", MESSAGE_ID_POLL_META },
    {"MESSAGE_ID_FLUSH", MESSAGE_ID_FLUSH },
    {"MESSAGE_ID_MAX", MESSAGE_ID_MAX },
};

class MessageCallbackMetadata {
public:
    Camera3Request* request;
    bool updateMeta;

    MessageCallbackMetadata():
        request(nullptr),
        updateMeta(false) {}
};

class MessagePollEvent {
public:
    int requestId;
    std::shared_ptr<V4L2VideoNode> *activeDevices;
    int polledDevices;
    int numDevices;
    IPollEventListener::PollEventMessageId pollMsgId;

    MessagePollEvent() : requestId(-1),
            activeDevices(nullptr),
            polledDevices(0),
            numDevices(0),
            pollMsgId(IPollEventListener::POLL_EVENT_ID_ERROR) {}
};

class DeviceMessage {
public:
    DeviceMessageId id;
    RKISP2ProcTaskMsg pMsg;
    MessageCallbackMetadata cbMetadataMsg;
    MessagePollEvent pollEvent;

    DeviceMessage():
        id(MESSAGE_ID_MAX) {}
};

class RKISP2IDeviceWorker: public IErrorCallback
{
public:
    explicit RKISP2IDeviceWorker(int cameraId) : mCameraId(cameraId), mDevError(false) {}
    virtual ~RKISP2IDeviceWorker() {}

    virtual status_t configure(bool configChanged) = 0;
    virtual status_t startWorker() = 0;
    virtual status_t flushWorker() = 0;
    virtual status_t stopWorker() = 0;
    virtual status_t prepareRun(std::shared_ptr<DeviceMessage> msg) = 0;
    virtual status_t run() = 0;
    virtual status_t postRun() = 0;
    virtual std::shared_ptr<V4L2VideoNode> getNode() const = 0;
    virtual status_t deviceError(void) { mDevError = true; return NO_ERROR; }
    virtual status_t asyncPollDone(std::shared_ptr<DeviceMessage> msg, bool polled) =0;

protected:
    std::shared_ptr<DeviceMessage> mMsg; /*!Set in prepareRun and should be valid until
                           postRun is called */

    int mCameraId;
    bool mDevError;
private:
    RKISP2IDeviceWorker();
};

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_WORKERS_RKISP2IDeviceWorker_H_ */
