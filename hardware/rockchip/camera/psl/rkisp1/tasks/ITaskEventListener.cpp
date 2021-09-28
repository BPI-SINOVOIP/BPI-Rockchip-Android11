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

#define LOG_TAG "ITaskEventListener"

#include "ITaskEventListener.h"
#include "ExecuteTaskBase.h"

namespace android {
namespace camera2 {

status_t ITaskEventListener::settings(ProcTaskMsg &/*msg*/)
{
    // no-op base implementation
    return NO_ERROR;
}

void ITaskEventListener::cleanListeners()
{
    // no-op base implementation
    return;
}
} // namespace camera2
} // namespace android
