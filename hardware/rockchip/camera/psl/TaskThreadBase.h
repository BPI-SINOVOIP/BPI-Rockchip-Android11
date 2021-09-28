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

#ifndef CAMERA3_HAL_TASKTHREADBASE_H_
#define CAMERA3_HAL_TASKTHREADBASE_H_

#include <memory>
#include <string>
#include "MessageThread.h"

namespace android {
namespace camera2 {

/**
 * \class TaskThreadBase
 * Base class containing base operations Task that wants
 * to implement its own custom \e messageThreadLoop() and message queue
 *
 * If the basic common message queue operations are enough for the task,
 * the Task can derive from \e ExecuteTaskBase
 *
 * \sa ExecuteTaskBase
 */
class TaskThreadBase : public IMessageHandler {
public:
    TaskThreadBase(const char* name, int priority = PRIORITY_CAMERA) :
        mThreadRunning(false),
        mName(name),
        mPriority(priority) {}

protected:
        status_t initMessageThread();
        virtual status_t deInit();
        virtual status_t requestExitAndWait() = 0;

protected:
    std::unique_ptr<MessageThread> mMessageThread;
    bool mThreadRunning;
    std::string mName;

private:
    int mPriority;
};

}  // namespace camera2
}  // namespace android

#endif // CAMERA3_HAL_TASKTHREADBASE_H_
