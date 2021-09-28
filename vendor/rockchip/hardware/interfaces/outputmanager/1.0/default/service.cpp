/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "rockchip.hardware.outputmanager@1.0-service"

#include <rockchip/hardware/outputmanager/1.0/IRkOutputManager.h>

#include <hidl/LegacySupport.h>
#include <sched.h>
#include <binder/ProcessState.h>
using android::sp;

// Generated HIDL files
using namespace rockchip::hardware::outputmanager::V1_0;
using rockchip::hardware::outputmanager::V1_0::IRkOutputManager;
using android::hardware::defaultPassthroughServiceImplementation;

int main() {
    ALOGD("***************defaultPassthroughServiceImplementation IRkOutputManager ******");
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->setThreadPoolMaxThreadCount(4);
    android::ProcessState::self()->startThreadPool();
    struct sched_param param = {0};
    param.sched_priority = 2;
    if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK,
                &param) != 0) {
        ALOGE("Couldn't set SCHED_FIFO: %d", errno);
    }
    return defaultPassthroughServiceImplementation<IRkOutputManager>(4);
}
