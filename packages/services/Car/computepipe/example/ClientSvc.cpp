// Copyright 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <android-base/logging.h>
#include <android/binder_process.h>
#include <utils/Log.h>

#include "FaceTracker.h"

using android::automotive::computepipe::FaceTracker;

namespace {

void terminationCallback(bool error, std::string errorMsg) {
    if (error) {
        LOG(ERROR) << errorMsg;
        exit(2);
    }
    LOG(ERROR) << "Test completed";
    exit(0);
}
}  // namespace
int main(int /* argc */, char** /* argv */) {
    std::function<void(bool, std::string)> cb = terminationCallback;
    FaceTracker client;

    ABinderProcess_startThreadPool();
    ndk::ScopedAStatus status = client.init(std::move(cb));
    if (!status.isOk()) {
        LOG(ERROR) << "Unable to init client connection";
        return -1;
    }
    ABinderProcess_joinThreadPool();

    return 0;
}
