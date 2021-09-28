/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef CPP_COMPUTEPIPE_TESTS_RUNNER_GRAPH_INCLUDES_PREBUILTENGINEINTERFACEIMPL_H_
#define CPP_COMPUTEPIPE_TESTS_RUNNER_GRAPH_INCLUDES_PREBUILTENGINEINTERFACEIMPL_H_

#include <string>

#include "ClientConfig.pb.h"
#include "LocalPrebuiltGraph.h"
#include "PrebuiltEngineInterface.h"
#include "ProfilingType.pb.h"
#include "RunnerComponent.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

// Barebones implementation of the PrebuiltEngineInterface. This implementation should suffice for
// basic cases. More complicated use cases might need their own implementation of it.
typedef std::function<void(int, int64_t, const runner::InputFrame&)> PixelCallback;
typedef std::function<void(int, int64_t, std::string&&)> SerializedStreamCallback;
typedef std::function<void(Status, std::string&&)> GraphTerminationCallback;
class PrebuiltEngineInterfaceImpl : public PrebuiltEngineInterface {
private:
    PixelCallback mPixelCallbackFn;
    SerializedStreamCallback mSerializedStreamCallbackFn;
    GraphTerminationCallback mGraphTerminationCallbackFn;

public:
    virtual ~PrebuiltEngineInterfaceImpl() = default;

    void DispatchPixelData(int streamId, int64_t timestamp,
                           const runner::InputFrame& frame) override {
        mPixelCallbackFn(streamId, timestamp, frame);
    }

    void DispatchSerializedData(int streamId, int64_t timestamp, std::string&& data) override {
        mSerializedStreamCallbackFn(streamId, timestamp, std::move(data));
    }

    void DispatchGraphTerminationMessage(Status status, std::string&& msg) override {
        mGraphTerminationCallbackFn(status, std::move(msg));
    }

    void SetPixelCallback(PixelCallback callback) { mPixelCallbackFn = callback; }

    void SetSerializedStreamCallback(SerializedStreamCallback callback) {
        mSerializedStreamCallbackFn = callback;
    }

    void SetGraphTerminationCallback(GraphTerminationCallback callback) {
        mGraphTerminationCallbackFn = callback;
    }
};

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // CPP_COMPUTEPIPE_TESTS_RUNNER_GRAPH_INCLUDES_PREBUILTENGINEINTERFACEIMPL_H_
