/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "SampleDriverUtils.h"
#include "SampleDriver.h"

#include <android-base/logging.h>

namespace android {
namespace nn {
namespace sample_driver {

using namespace hal;

void notify(const sp<V1_0::IPreparedModelCallback>& callback, const ErrorStatus& status,
            const sp<SamplePreparedModel>& preparedModel) {
    const auto ret = callback->notify(convertToV1_0(status), preparedModel);
    if (!ret.isOk()) {
        LOG(ERROR) << "Error when calling IPreparedModelCallback::notify: " << ret.description();
    }
}

void notify(const sp<V1_2::IPreparedModelCallback>& callback, const ErrorStatus& status,
            const sp<SamplePreparedModel>& preparedModel) {
    const auto ret = callback->notify_1_2(convertToV1_0(status), preparedModel);
    if (!ret.isOk()) {
        LOG(ERROR) << "Error when calling IPreparedModelCallback::notify_1_2: "
                   << ret.description();
    }
}

void notify(const sp<V1_3::IPreparedModelCallback>& callback, const ErrorStatus& status,
            const sp<SamplePreparedModel>& preparedModel) {
    const auto ret = callback->notify_1_3(status, preparedModel);
    if (!ret.isOk()) {
        LOG(ERROR) << "Error when calling IPreparedModelCallback::notify_1_3: "
                   << ret.description();
    }
}

void notify(const sp<V1_0::IExecutionCallback>& callback, const ErrorStatus& status,
            const hidl_vec<OutputShape>&, Timing) {
    const auto ret = callback->notify(convertToV1_0(status));
    if (!ret.isOk()) {
        LOG(ERROR) << "Error when calling IExecutionCallback::notify: " << ret.description();
    }
}

void notify(const sp<V1_2::IExecutionCallback>& callback, const ErrorStatus& status,
            const hidl_vec<OutputShape>& outputShapes, Timing timing) {
    const auto ret = callback->notify_1_2(convertToV1_0(status), outputShapes, timing);
    if (!ret.isOk()) {
        LOG(ERROR) << "Error when calling IExecutionCallback::notify_1_2: " << ret.description();
    }
}

void notify(const sp<V1_3::IExecutionCallback>& callback, const ErrorStatus& status,
            const hidl_vec<OutputShape>& outputShapes, Timing timing) {
    const auto ret = callback->notify_1_3(status, outputShapes, timing);
    if (!ret.isOk()) {
        LOG(ERROR) << "Error when calling IExecutionCallback::notify_1_3" << ret.description();
    }
}

}  // namespace sample_driver
}  // namespace nn
}  // namespace android
