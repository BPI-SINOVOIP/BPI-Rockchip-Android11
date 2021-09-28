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
#include <string>

#include "ClientConfig.pb.h"
#include "LocalPrebuiltGraph.h"
#include "PrebuiltEngineInterface.h"
#include "PrebuiltEngineInterfaceImpl.h"
#include "ProfilingType.pb.h"
#include "RunnerComponent.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "types/Status.h"

using ::android::automotive::computepipe::runner::ClientConfig;
using ::android::automotive::computepipe::runner::RunnerComponentInterface;
using ::android::automotive::computepipe::runner::RunnerEvent;
using ::testing::HasSubstr;

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {
namespace {

// The stub graph implementation is a passthrough implementation that does not run
// any graph and returns success for all implementations. The only useful things that
// it does for the tests are
//
//    1. Stores the name of the function last visited and returns that with GetErrorMessage call
//    2. When an input stream is set, it immediately returns an output callback with the same input
//       data and timestamp. Similar callback is issued for when input stream pixel data is set too
//
// The above two properties are used to test that the prebuilt graph wrapper calls the correct
// functions and callbacks are issued as expected. These tests do not test the internals of the
// graph themselves and such tests must be written along with the graph implementation.
TEST(LocalPrebuiltGraphTest, FunctionMappingFromLibraryIsSuccessful) {
    PrebuiltEngineInterfaceImpl callback;
    std::shared_ptr<PrebuiltEngineInterface> engineInterface =
            std::static_pointer_cast<PrebuiltEngineInterface, PrebuiltEngineInterfaceImpl>(
                    std::make_shared<PrebuiltEngineInterfaceImpl>(callback));
    PrebuiltGraph* graph = GetLocalGraphFromLibrary("libstubgraphimpl.so", engineInterface);
    ASSERT_TRUE(graph);
    EXPECT_EQ(graph->GetGraphType(), PrebuiltGraphType::LOCAL);
    EXPECT_NE(graph->GetGraphState(), PrebuiltGraphState::UNINITIALIZED);
    EXPECT_EQ(graph->GetSupportedGraphConfigs().graph_name(), "stub_graph");
}

TEST(LocalPrebuiltGraphTest, GraphConfigurationIssuesCorrectFunctionCalls) {
    PrebuiltEngineInterfaceImpl callback;
    std::shared_ptr<PrebuiltEngineInterface> engineInterface =
            std::static_pointer_cast<PrebuiltEngineInterface, PrebuiltEngineInterfaceImpl>(
                    std::make_shared<PrebuiltEngineInterfaceImpl>(callback));
    PrebuiltGraph* graph = GetLocalGraphFromLibrary("libstubgraphimpl.so", engineInterface);
    ASSERT_TRUE(graph);
    EXPECT_EQ(graph->GetGraphType(), PrebuiltGraphType::LOCAL);
    ASSERT_NE(graph->GetGraphState(), PrebuiltGraphState::UNINITIALIZED);

    graph->GetSupportedGraphConfigs();
    std::string functionVisited = graph->GetErrorMessage();
    EXPECT_THAT(functionVisited, HasSubstr("GetSupportedGraphConfigs"));

    std::map<int, int> maxOutputPacketsPerStream;
    ClientConfig e(0, 0, 0, maxOutputPacketsPerStream, proto::ProfilingType::DISABLED);
    e.setPhaseState(runner::PhaseState::ENTRY);
    EXPECT_EQ(graph->handleConfigPhase(e), Status::SUCCESS);
    functionVisited = graph->GetErrorMessage();

    EXPECT_EQ(graph->GetStatus(), Status::SUCCESS);
    functionVisited = graph->GetErrorMessage();
    EXPECT_THAT(functionVisited, HasSubstr("GetErrorCode"));
}

TEST(LocalPrebuiltGraphTest, GraphOperationEndToEndIsSuccessful) {
    bool graphHasTerminated = false;
    int numOutputStreamCallbacksReceived[4] = {0, 0, 0, 0};

    PrebuiltEngineInterfaceImpl callback;
    callback.SetGraphTerminationCallback(
            [&graphHasTerminated](Status, std::string) { graphHasTerminated = true; });

    // Add multiple pixel stream callback functions to see if all of them register.
    callback.SetPixelCallback([&numOutputStreamCallbacksReceived](int streamIndex, int64_t,
                                                                  const runner::InputFrame&) {
        ASSERT_TRUE(streamIndex == 0 || streamIndex == 1);
        numOutputStreamCallbacksReceived[streamIndex]++;
    });

    // Add multiple stream callback functions to see if all of them register.
    callback.SetSerializedStreamCallback(
            [&numOutputStreamCallbacksReceived](int streamIndex, int64_t, std::string&&) {
                ASSERT_TRUE(streamIndex == 2 || streamIndex == 3);
                numOutputStreamCallbacksReceived[streamIndex]++;
            });

    std::shared_ptr<PrebuiltEngineInterface> engineInterface =
            std::static_pointer_cast<PrebuiltEngineInterface, PrebuiltEngineInterfaceImpl>(
                    std::make_shared<PrebuiltEngineInterfaceImpl>(callback));

    PrebuiltGraph* graph = GetLocalGraphFromLibrary("libstubgraphimpl.so", engineInterface);

    EXPECT_EQ(graph->GetGraphType(), PrebuiltGraphType::LOCAL);
    ASSERT_NE(graph->GetGraphState(), PrebuiltGraphState::UNINITIALIZED);

    graph->GetSupportedGraphConfigs();
    std::string functionVisited = graph->GetErrorMessage();
    EXPECT_THAT(functionVisited, HasSubstr("GetSupportedGraphConfigs"));

    std::map<int, int> maxOutputPacketsPerStream;
    ClientConfig e(0, 0, 0, maxOutputPacketsPerStream, proto::ProfilingType::DISABLED);
    e.setPhaseState(runner::PhaseState::ENTRY);
    EXPECT_EQ(graph->handleConfigPhase(e), Status::SUCCESS);
    functionVisited = graph->GetErrorMessage();

    EXPECT_EQ(graph->handleExecutionPhase(e), Status::SUCCESS);
    functionVisited = graph->GetErrorMessage();
    EXPECT_THAT(functionVisited, HasSubstr("StartGraphExecution"));

    runner::InputFrame inputFrame(0, 0, PixelFormat::RGB, 0, nullptr);
    EXPECT_EQ(graph->SetInputStreamPixelData(
                      /*streamIndex =*/0, /*timestamp =*/0, /*inputFrame =*/inputFrame),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamPixelData(
                      /*streamIndex =*/0, /*timestamp =*/0, /*inputFrame =*/inputFrame),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamPixelData(
                      /*streamIndex =*/0, /*timestamp =*/0, /*inputFrame =*/inputFrame),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamPixelData(
                      /*streamIndex =*/1, /*timestamp =*/0, /*inputFrame =*/inputFrame),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamPixelData(
                      /*streamIndex =*/1, /*timestamp =*/0, /*inputFrame =*/inputFrame),
              Status::SUCCESS);
    functionVisited = graph->GetErrorMessage();
    EXPECT_THAT(functionVisited, HasSubstr("SetInputStreamPixelData"));

    EXPECT_EQ(graph->SetInputStreamData(/*streamIndex =*/2, /* timestamp =*/0, /* data =*/""),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamData(/*streamIndex =*/2, /* timestamp =*/0, /* data =*/""),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamData(/*streamIndex =*/2, /* timestamp =*/0, /* data =*/""),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamData(/*streamIndex =*/3, /* timestamp =*/0, /* data =*/""),
              Status::SUCCESS);
    EXPECT_EQ(graph->SetInputStreamData(/*streamIndex =*/3, /* timestamp =*/0, /* data =*/""),
              Status::SUCCESS);
    functionVisited = graph->GetErrorMessage();
    EXPECT_THAT(functionVisited, HasSubstr("SetInputStreamData"));

    EXPECT_EQ(numOutputStreamCallbacksReceived[0], 3);
    EXPECT_EQ(numOutputStreamCallbacksReceived[1], 2);
    EXPECT_EQ(numOutputStreamCallbacksReceived[2], 3);
    EXPECT_EQ(numOutputStreamCallbacksReceived[3], 2);

    EXPECT_FALSE(graphHasTerminated);
    EXPECT_EQ(graph->handleStopImmediatePhase(e), Status::SUCCESS);

    EXPECT_EQ(graph->handleResetPhase(e), Status::SUCCESS);
    functionVisited = graph->GetErrorMessage();
    EXPECT_THAT(functionVisited, HasSubstr("ResetGraph"));

    EXPECT_TRUE(graphHasTerminated);
}

}  // namespace
}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
