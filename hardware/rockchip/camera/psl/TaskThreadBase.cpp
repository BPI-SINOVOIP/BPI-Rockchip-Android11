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

#define LOG_TAG "TaskThreadBase"

#include "TaskThreadBase.h"
#include "LogHelper.h"


namespace android {
namespace camera2 {


status_t TaskThreadBase::initMessageThread()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

    std::string threadName = mName + std::string("Thread");
    mMessageThread = std::unique_ptr<MessageThread>(new MessageThread(this, threadName.c_str(), mPriority));
    mMessageThread->run();

    return status;
}

status_t
TaskThreadBase::deInit()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

    if (mMessageThread != nullptr) {
        status = requestExitAndWait();
        if (status == NO_ERROR) {
            mMessageThread.reset();
            mMessageThread = nullptr;
        }
    }
    return status;
}

}  // namespace camera2
}  // namespace android
