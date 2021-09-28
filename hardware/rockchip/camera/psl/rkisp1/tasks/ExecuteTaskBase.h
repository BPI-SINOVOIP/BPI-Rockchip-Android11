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

#ifndef CAMERA3_HAL_EXECUTETASKBASE_H_
#define CAMERA3_HAL_EXECUTETASKBASE_H_

#include <memory>
#include <string>
#include <vector>
#include "Camera3Request.h"
#include "CameraStream.h"
#include "MessageQueue.h"
#include "TaskThreadBase.h"
#include "CameraWindow.h"
#include "IExecuteTask.h"
#include "ITaskEventListener.h"
#include "GraphConfig.h"
#include "ProcUnitSettings.h"

namespace android {
namespace camera2 {

struct CaptureUnitSettings;
struct ProcUnitSettings;

struct StreamConfig {
    std::vector<camera3_stream_t *> yuvStreams;
    std::vector<camera3_stream_t *> rawStreams;
    std::vector<camera3_stream_t *> blobStreams;
    camera3_stream_t *inputStream;
};

/**
 * \struct ProcTaskMsg
 * Structure to pass data to ExecuteTaskBase-based task objects
 */
struct ProcTaskMsg {
    bool immediate;
    unsigned int reqId;
    std::shared_ptr<ProcUnitSettings> processingSettings;

    // Default constructor:
    ProcTaskMsg() : immediate(false), reqId(0){}
};

/**
 * \class ExecuteTaskBase
 * Base class of all Processing unit Tasks.Defines how Tasks
 * behave and communicate with other Tasks
 *
 * A common base for tasks that implement a "basic task".
 * ExecuteTaskBase has the basic messageThreadLoop.
 *
 * In case a new Task needs a specific \e messageThreadLoop() and custom
 * message queue, the task should derive \e TaskThreadBase and implement
 * a specialized message loop and queue.
 *
 * \sa TaskThreadBase
 */
class ExecuteTaskBase : public IExecuteTask, public TaskThreadBase {
public:
    ExecuteTaskBase(const char* name, int priority = PRIORITY_CAMERA);
    virtual ~ExecuteTaskBase();

    virtual status_t init();

    virtual status_t executeTask(ProcTaskMsg &msg); // override IExecuteTask
    std::string getName() { return mName; }

    virtual status_t allocateInterBuffer(bool isFallback, camera3_stream_t *stream,
            int w,
            int h,
            int cameraId,
            std::map<camera3_stream_t *, std::shared_ptr<CameraBuffer>> &interBufMap);
    virtual status_t setIntermediateBuffer(
            bool isFallback,
            std::vector<GraphConfig::PSysPipelineConnection>::iterator it,
            int cameraId,
            std::map<camera3_stream_t *, std::shared_ptr<CameraBuffer>> &interBufMap);
    virtual bool isVideoStream(CameraStream *stream);
// MessageThread -related types
// Declared 'protected' to be accessable in subclasses
protected:
    enum MessageId {
        MESSAGE_ID_EXIT = 0,
        MESSAGE_ID_PREPARE,
        MESSAGE_ID_EXECUTE_TASK,
        MESSAGE_ID_ITERATION_DONE,
        MESSAGE_ID_MAX
    };

    struct Message {
        MessageId id;
        ProcTaskMsg data;
    };

protected:
    virtual status_t handleExecuteTask(Message &msg) = 0;
    virtual status_t handleMessageIterationDone(Message &msg);
    uint8_t analyzeIntent(ProcTaskMsg &pMsg);
    virtual status_t requestExitAndWait();
protected:
    MessageQueue<Message, MessageId> mMessageQueue;

private:
    /* IMessageHandler overloads */
    virtual void messageThreadLoop();
};

}  // namespace camera2
}  // namespace android

#endif  // CAMERA3_HAL_EXECUTETASKBASE_H_
