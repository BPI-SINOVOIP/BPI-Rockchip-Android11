// Copyright (C) 2020 The Android Open Source Project
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

#include "GrpcGraph.h"

#include <cstdlib>

#include <android-base/logging.h>
#include <grpcpp/grpcpp.h>

#include "ClientConfig.pb.h"
#include "GrpcGraph.h"
#include "InputFrame.h"
#include "RunnerComponent.h"
#include "prebuilt_interface.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {
namespace {
constexpr int64_t kRpcDeadlineMilliseconds = 100;

template <class ResponseType, class RpcType>
std::pair<Status, std::string> FinishRpcAndGetResult(
        ::grpc::ClientAsyncResponseReader<RpcType>* rpc, ::grpc::CompletionQueue* cq,
        ResponseType* response) {
    int random_tag = rand();
    ::grpc::Status grpcStatus;
    rpc->Finish(response, &grpcStatus, reinterpret_cast<void*>(random_tag));
    bool ok = false;
    void* got_tag;
    if (!cq->Next(&got_tag, &ok)) {
        LOG(ERROR) << "Unexpected shutdown of the completion queue";
        return std::pair(Status::FATAL_ERROR, "Unexpected shutdown of the completion queue");
    }

    if (!ok) {
        LOG(ERROR) << "Unable to complete RPC request";
        return std::pair(Status::FATAL_ERROR, "Unable to complete RPC request");
    }

    CHECK_EQ(got_tag, reinterpret_cast<void*>(random_tag));
    if (!grpcStatus.ok()) {
        std::string error_message =
                std::string("Grpc failed with error: ") + grpcStatus.error_message();
        LOG(ERROR) << error_message;
        return std::pair(Status::FATAL_ERROR, std::move(error_message));
    }

    return std::pair(Status::SUCCESS, std::string(""));
}

}  // namespace

GrpcGraph::~GrpcGraph() {
    mStreamSetObserver.reset();
}

PrebuiltGraphState GrpcGraph::GetGraphState() const {
    std::lock_guard lock(mLock);
    return mGraphState;
}

Status GrpcGraph::GetStatus() const {
    std::lock_guard lock(mLock);
    return mStatus;
}

std::string GrpcGraph::GetErrorMessage() const {
    std::lock_guard lock(mLock);
    return mErrorMessage;
}

Status GrpcGraph::initialize(const std::string& address,
                             std::weak_ptr<PrebuiltEngineInterface> engineInterface) {
    std::shared_ptr<::grpc::ChannelCredentials> creds = ::grpc::InsecureChannelCredentials();
    std::shared_ptr<::grpc::Channel> channel = ::grpc::CreateChannel(address, creds);
    mGraphStub = proto::GrpcGraphService::NewStub(channel);
    mEngineInterface = engineInterface;

    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));
    ::grpc::CompletionQueue cq;

    proto::GraphOptionsRequest getGraphOptionsRequest;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::GraphOptionsResponse>> rpc(
            mGraphStub->AsyncGetGraphOptions(&context, getGraphOptionsRequest, &cq));

    proto::GraphOptionsResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);

    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to get graph options: " << mErrorMessage;
        return Status::FATAL_ERROR;
    }

    std::string serialized_options = response.serialized_options();
    if (!mGraphConfig.ParseFromString(serialized_options)) {
        mErrorMessage = "Failed to parse graph options";
        LOG(ERROR) << "Failed to parse graph options";
        return Status::FATAL_ERROR;
    }

    mGraphState = PrebuiltGraphState::STOPPED;
    return Status::SUCCESS;
}

// Function to confirm that there would be no further changes to the graph configuration. This
// needs to be called before starting the graph.
Status GrpcGraph::handleConfigPhase(const runner::ClientConfig& e) {
    std::lock_guard lock(mLock);
    if (mGraphState == PrebuiltGraphState::UNINITIALIZED) {
        mStatus = Status::ILLEGAL_STATE;
        return Status::ILLEGAL_STATE;
    }

    // handleConfigPhase is a blocking call, so abort call is pointless for this RunnerEvent.
    if (e.isAborted()) {
        mStatus = Status::INVALID_ARGUMENT;
        return mStatus;
    } else if (e.isTransitionComplete()) {
        mStatus = Status::SUCCESS;
        return mStatus;
    }

    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));
    ::grpc::CompletionQueue cq;

    std::string serializedConfig = e.getSerializedClientConfig();
    proto::SetGraphConfigRequest setGraphConfigRequest;
    setGraphConfigRequest.set_serialized_config(std::move(serializedConfig));

    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::StatusResponse>> rpc(
            mGraphStub->AsyncSetGraphConfig(&context, setGraphConfigRequest, &cq));

    proto::StatusResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Rpc failed while trying to set configuration";
        return mStatus;
    }

    if (response.code() != proto::RemoteGraphStatusCode::SUCCESS) {
        LOG(ERROR) << "Failed to cofngure remote graph. " << response.message();
    }

    mStatus = static_cast<Status>(static_cast<int>(response.code()));
    mErrorMessage = response.message();

    mStreamSetObserver = std::make_unique<StreamSetObserver>(e, this);

    return mStatus;
}

// Starts the graph.
Status GrpcGraph::handleExecutionPhase(const runner::RunnerEvent& e) {
    std::lock_guard lock(mLock);
    if (mGraphState != PrebuiltGraphState::STOPPED || mStreamSetObserver == nullptr) {
        mStatus = Status::ILLEGAL_STATE;
        return mStatus;
    }

    if (e.isAborted()) {
        // Starting the graph is a blocking call and cannot be aborted in between.
        mStatus = Status::INVALID_ARGUMENT;
        return mStatus;
    } else if (e.isTransitionComplete()) {
        mStatus = Status::SUCCESS;
        return mStatus;
    }

    // Start observing the output streams
    mStatus = mStreamSetObserver->startObservingStreams();
    if (mStatus != Status::SUCCESS) {
        mErrorMessage = "Failed to observe output streams";
        return mStatus;
    }

    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));

    proto::StartGraphExecutionRequest startExecutionRequest;
    ::grpc::CompletionQueue cq;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::StatusResponse>> rpc(
            mGraphStub->AsyncStartGraphExecution(&context, startExecutionRequest, &cq));

    proto::StatusResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to start graph execution";
        return mStatus;
    }

    mStatus = static_cast<Status>(static_cast<int>(response.code()));
    mErrorMessage = response.message();

    if (mStatus == Status::SUCCESS) {
        mGraphState = PrebuiltGraphState::RUNNING;
    }

    return mStatus;
}

// Stops the graph while letting the graph flush output packets in flight.
Status GrpcGraph::handleStopWithFlushPhase(const runner::RunnerEvent& e) {
    std::lock_guard lock(mLock);
    if (mGraphState != PrebuiltGraphState::RUNNING) {
        return Status::ILLEGAL_STATE;
    }

    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));

    proto::StopGraphExecutionRequest stopExecutionRequest;
    stopExecutionRequest.set_stop_immediate(false);
    ::grpc::CompletionQueue cq;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::StatusResponse>> rpc(
            mGraphStub->AsyncStopGraphExecution(&context, stopExecutionRequest, &cq));

    proto::StatusResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to stop graph execution";
        return Status::FATAL_ERROR;
    }

    // Stop observing streams immendiately.
    mStreamSetObserver->stopObservingStreams(false);

    mStatus = static_cast<Status>(static_cast<int>(response.code()));
    mErrorMessage = response.message();

    if (mStatus == Status::SUCCESS) {
        mGraphState = PrebuiltGraphState::FLUSHING;
    }

    return mStatus;
}

// Stops the graph and cancels all the output packets.
Status GrpcGraph::handleStopImmediatePhase(const runner::RunnerEvent& e) {
    std::lock_guard lock(mLock);
    if (mGraphState != PrebuiltGraphState::RUNNING) {
        return Status::ILLEGAL_STATE;
    }

    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));

    proto::StopGraphExecutionRequest stopExecutionRequest;
    stopExecutionRequest.set_stop_immediate(true);
    ::grpc::CompletionQueue cq;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::StatusResponse>> rpc(
            mGraphStub->AsyncStopGraphExecution(&context, stopExecutionRequest, &cq));

    proto::StatusResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to stop graph execution";
        return Status::FATAL_ERROR;
    }

    mStatus = static_cast<Status>(static_cast<int>(response.code()));
    mErrorMessage = response.message();

    // Stop observing streams immendiately.
    mStreamSetObserver->stopObservingStreams(true);

    if (mStatus == Status::SUCCESS) {
        mGraphState = PrebuiltGraphState::STOPPED;
    }
    return mStatus;
}

Status GrpcGraph::handleResetPhase(const runner::RunnerEvent& e) {
    std::lock_guard lock(mLock);
    if (mGraphState != PrebuiltGraphState::STOPPED) {
        return Status::ILLEGAL_STATE;
    }

    if (e.isAborted()) {
        return Status::INVALID_ARGUMENT;
    } else if (e.isTransitionComplete()) {
        return Status::SUCCESS;
    }

    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));

    proto::ResetGraphRequest resetGraphRequest;
    ::grpc::CompletionQueue cq;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::StatusResponse>> rpc(
            mGraphStub->AsyncResetGraph(&context, resetGraphRequest, &cq));

    proto::StatusResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to stop graph execution";
        return Status::FATAL_ERROR;
    }

    mStatus = static_cast<Status>(static_cast<int>(response.code()));
    mErrorMessage = response.message();
    mStreamSetObserver.reset();

    return mStatus;
}

Status GrpcGraph::SetInputStreamData(int /*streamIndex*/, int64_t /*timestamp*/,
                                     const std::string& /*streamData*/) {
    LOG(ERROR) << "Cannot set input stream for remote graphs";
    return Status::FATAL_ERROR;
}

Status GrpcGraph::SetInputStreamPixelData(int /*streamIndex*/, int64_t /*timestamp*/,
                                          const runner::InputFrame& /*inputFrame*/) {
    LOG(ERROR) << "Cannot set input streams for remote graphs";
    return Status::FATAL_ERROR;
}

Status GrpcGraph::StartGraphProfiling() {
    std::lock_guard lock(mLock);
    if (mGraphState != PrebuiltGraphState::RUNNING) {
        return Status::ILLEGAL_STATE;
    }

    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));

    proto::StartGraphProfilingRequest startProfilingRequest;
    ::grpc::CompletionQueue cq;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::StatusResponse>> rpc(
            mGraphStub->AsyncStartGraphProfiling(&context, startProfilingRequest, &cq));

    proto::StatusResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to start graph profiling";
        return Status::FATAL_ERROR;
    }

    mStatus = static_cast<Status>(static_cast<int>(response.code()));
    mErrorMessage = response.message();

    return mStatus;
}

Status GrpcGraph::StopGraphProfiling() {
    // Stopping profiling after graph has already stopped can be a no-op
    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));

    proto::StopGraphProfilingRequest stopProfilingRequest;
    ::grpc::CompletionQueue cq;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::StatusResponse>> rpc(
            mGraphStub->AsyncStopGraphProfiling(&context, stopProfilingRequest, &cq));

    proto::StatusResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to stop graph profiling";
        return Status::FATAL_ERROR;
    }

    mStatus = static_cast<Status>(static_cast<int>(response.code()));
    mErrorMessage = response.message();

    return mStatus;
}

std::string GrpcGraph::GetDebugInfo() {
    ::grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::milliseconds(kRpcDeadlineMilliseconds));

    proto::ProfilingDataRequest profilingDataRequest;
    ::grpc::CompletionQueue cq;
    std::unique_ptr<::grpc::ClientAsyncResponseReader<proto::ProfilingDataResponse>> rpc(
            mGraphStub->AsyncGetProfilingData(&context, profilingDataRequest, &cq));

    proto::ProfilingDataResponse response;
    auto [mStatus, mErrorMessage] = FinishRpcAndGetResult(rpc.get(), &cq, &response);
    if (mStatus != Status::SUCCESS) {
        LOG(ERROR) << "Failed to get profiling info";
        return "";
    }

    return response.data();
}

void GrpcGraph::dispatchPixelData(int streamId, int64_t timestamp_us,
                                  const runner::InputFrame& frame) {
    std::shared_ptr<PrebuiltEngineInterface> engineInterface = mEngineInterface.lock();
    if (engineInterface) {
        return engineInterface->DispatchPixelData(streamId, timestamp_us, frame);
    }
}

void GrpcGraph::dispatchSerializedData(int streamId, int64_t timestamp_us,
                                       std::string&& serialized_data) {
    std::shared_ptr<PrebuiltEngineInterface> engineInterface = mEngineInterface.lock();
    if (engineInterface) {
        return engineInterface->DispatchSerializedData(streamId, timestamp_us,
                                                       std::move(serialized_data));
    }
}

void GrpcGraph::dispatchGraphTerminationMessage(Status status, std::string&& errorMessage) {
    std::lock_guard lock(mLock);
    mErrorMessage = std::move(errorMessage);
    mStatus = status;
    mGraphState = PrebuiltGraphState::STOPPED;
    std::shared_ptr<PrebuiltEngineInterface> engineInterface = mEngineInterface.lock();
    if (engineInterface) {
        std::string errorMessageTmp = mErrorMessage;
        engineInterface->DispatchGraphTerminationMessage(mStatus, std::move(errorMessageTmp));
    }
}

std::unique_ptr<PrebuiltGraph> GetRemoteGraphFromAddress(
        const std::string& address, std::weak_ptr<PrebuiltEngineInterface> engineInterface) {
    auto prebuiltGraph = std::make_unique<GrpcGraph>();
    Status status = prebuiltGraph->initialize(address, engineInterface);
    if (status != Status::SUCCESS) {
        return nullptr;
    }

    return prebuiltGraph;
}

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
