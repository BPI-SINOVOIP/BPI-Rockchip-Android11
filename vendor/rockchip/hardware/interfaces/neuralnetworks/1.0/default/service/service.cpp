// Copyright (c) 2021 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define LOG_TAG "rockchip.hardware.neuralnetworks@1.0-service"

#include <rockchip/hardware/neuralnetworks/1.0/IRKNeuralnetworks.h>

#include <hidl/LegacySupport.h>
#include <sched.h>
#include <binder/ProcessState.h>
using android::sp;

// Generated HIDL files
using namespace rockchip::hardware::neuralnetworks::V1_0;
using rockchip::hardware::neuralnetworks::V1_0::IRKNeuralnetworks;
using android::hardware::defaultPassthroughServiceImplementation;

int main() {
    ALOGD("***************defaultPassthroughServiceImplementation IRKNeuralnetworks ******");
    setenv("ANDROID_LOG_TAGS", "*:v", 1);
    // This function must be called before you join to ensure the proper
    // number of threads are created. The threadpool will never exceed
    // size one because of this call.
    signal(SIGPIPE, SIG_IGN);
    // vndbinder is needed by BufferQueue.
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->startThreadPool();
    ::android::hardware::configureRpcThreadpool(8 /*threads*/, true /*willJoin*/);

    return defaultPassthroughServiceImplementation<IRKNeuralnetworks>(4);; // joinRpcThreadpool should never return
}
