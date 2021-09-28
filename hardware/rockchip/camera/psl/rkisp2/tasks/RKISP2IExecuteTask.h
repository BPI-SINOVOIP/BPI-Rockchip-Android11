
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

#ifndef CAMERA3_HAL_RKISP2IExecuteTask_H_
#define CAMERA3_HAL_RKISP2IExecuteTask_H_

#include <string>


namespace android {
namespace camera2 {
namespace rkisp2 {

struct RKISP2ProcTaskMsg;

/**
 * \class RKISP2IExecuteTask
 *
 * Specifies the interface for a Task that needs to be "executed"
 * explicitly - that is, to implement its own \e executeTask()
 *
 * For Tasks wanting to receive/send events, see the respective
 * interface classes \e ITaskEventListener and \e ITaskEventSource
 *
 * \sa ITaskEventListener
 * \sa ITaskEventSource
 */
class RKISP2IExecuteTask {
public:
    virtual ~RKISP2IExecuteTask() {}
    virtual status_t executeTask(RKISP2ProcTaskMsg &msg) = 0;
    virtual std::string getName() = 0;
};

} // namespace rkisp2
} // namespace camera2
} // namespace android

#endif

