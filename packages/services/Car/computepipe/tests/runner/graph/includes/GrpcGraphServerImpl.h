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

#ifndef CPP_COMPUTEPIPE_TESTS_RUNNER_GRAPH_INCLUDES_GRPCGRAPHSERVERIMPL_H_
#define CPP_COMPUTEPIPE_TESTS_RUNNER_GRAPH_INCLUDES_GRPCGRAPHSERVERIMPL_H_

#include <map>
#include <memory>
#include <string>
#include <thread>

#include <android-base/logging.h>
#include <grpc++/grpc++.h>

#include "GrpcPrebuiltGraphService.grpc.pb.h"
#include "GrpcPrebuiltGraphService.pb.h"
#include "Options.pb.h"
#include "PrebuiltEngineInterface.h"
#include "PrebuiltGraph.h"
#include "RunnerComponent.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

constexpr char kGraphName[] = "Stub graph name";
constexpr char kSetGraphConfigMessage[] = "Stub set config message";
constexpr char kSetDebugOptionMessage[] = "Stub set debug option message";
constexpr char kStartGraphMessage[] = "Stub start graph message";
constexpr char kStopGraphMessage[] = "Stub stop graph message";
constexpr char kOutputStreamPacket[] = "Stub output stream packet";
constexpr char kResetGraphMessage[] = "ResetGraphMessage";

// This is a barebones synchronous server implementation. A better implementation would be an
// asynchronous implementation and it is upto the graph provider to do that. This implementation
// is very specific to tests being conducted here.
class GrpcGraphServerImpl : public proto::GrpcGraphService::Service {
private:
    std::string mServerAddress;
    std::unique_ptr<::grpc::Server> mServer;
    std::mutex mLock;
    std::condition_variable mShutdownCv;
    bool mShutdown = false;

public:
    explicit GrpcGraphServerImpl(std::string address) : mServerAddress(address) {}

    virtual ~GrpcGraphServerImpl() {
        if (mServer) {
            mServer->Shutdown();
            std::unique_lock lock(mLock);
            if (!mShutdown) {
                mShutdownCv.wait_for(lock, std::chrono::seconds(10),
                                     [this]() { return mShutdown; });
            }
        }
    }

    void startServer() {
        if (mServer == nullptr) {
            ::grpc::ServerBuilder builder;
            builder.RegisterService(this);
            builder.AddListeningPort(mServerAddress, ::grpc::InsecureServerCredentials());
            mServer = builder.BuildAndStart();
            mServer->Wait();
            std::lock_guard lock(mLock);
            mShutdown = true;
            mShutdownCv.notify_one();
        }
    }

    ::grpc::Status GetGraphOptions(::grpc::ServerContext* context,
                                   const proto::GraphOptionsRequest* request,
                                   proto::GraphOptionsResponse* response) override {
        proto::Options options;
        options.set_graph_name(kGraphName);
        response->set_serialized_options(options.SerializeAsString());
        return ::grpc::Status::OK;
    }

    ::grpc::Status SetGraphConfig(::grpc::ServerContext* context,
                                  const proto::SetGraphConfigRequest* request,
                                  proto::StatusResponse* response) override {
        response->set_code(proto::RemoteGraphStatusCode::SUCCESS);
        response->set_message(kSetGraphConfigMessage);
        return ::grpc::Status::OK;
    }

    ::grpc::Status SetDebugOption(::grpc::ServerContext* context,
                                  const proto::SetDebugRequest* request,
                                  proto::StatusResponse* response) override {
        response->set_code(proto::RemoteGraphStatusCode::SUCCESS);
        response->set_message(kSetDebugOptionMessage);
        return ::grpc::Status::OK;
    }

    ::grpc::Status StartGraphExecution(::grpc::ServerContext* context,
                                       const proto::StartGraphExecutionRequest* request,
                                       proto::StatusResponse* response) override {
        response->set_code(proto::RemoteGraphStatusCode::SUCCESS);
        response->set_message(kStartGraphMessage);
        return ::grpc::Status::OK;
    }

    ::grpc::Status ObserveOutputStream(
            ::grpc::ServerContext* context, const proto::ObserveOutputStreamRequest* request,
            ::grpc::ServerWriter<proto::OutputStreamResponse>* writer) override {
        // Write as many output packets as stream id. This is just to test different number of
        // packets received with each stream. Also write even numbered stream as a pixel packet
        // and odd numbered stream as a data packet.
        for (int i = 0; i < request->stream_id(); i++) {
            proto::OutputStreamResponse response;
            if (request->stream_id() % 2 == 0) {
                response.mutable_pixel_data()->set_data(kOutputStreamPacket);
                response.mutable_pixel_data()->set_height(1);
                response.mutable_pixel_data()->set_width(sizeof(kOutputStreamPacket));
                response.mutable_pixel_data()->set_step(sizeof(kOutputStreamPacket));
                response.mutable_pixel_data()->set_format(proto::PixelFormat::GRAY);
                EXPECT_TRUE(response.has_pixel_data());
            } else {
                response.set_semantic_data(kOutputStreamPacket);
                EXPECT_TRUE(response.has_semantic_data());
            }
            if (!writer->Write(response)) {
                return ::grpc::Status(::grpc::StatusCode::ABORTED, "Connection lost");
            }
        }

        return ::grpc::Status::OK;
    }

    ::grpc::Status StopGraphExecution(::grpc::ServerContext* context,
                                      const proto::StopGraphExecutionRequest* request,
                                      proto::StatusResponse* response) override {
        response->set_code(proto::RemoteGraphStatusCode::SUCCESS);
        response->set_message(kStopGraphMessage);
        return ::grpc::Status::OK;
    }

    ::grpc::Status ResetGraph(::grpc::ServerContext* context,
                              const proto::ResetGraphRequest* request,
                              proto::StatusResponse* response) override {
        response->set_code(proto::RemoteGraphStatusCode::SUCCESS);
        response->set_message(kResetGraphMessage);
        return ::grpc::Status::OK;
    }

    ::grpc::Status GetProfilingData(::grpc::ServerContext* context,
                                    const proto::ProfilingDataRequest* request,
                                    proto::ProfilingDataResponse* response) {
        response->set_data(kSetGraphConfigMessage);
        return ::grpc::Status::OK;
    }
};

class PrebuiltEngineInterfaceImpl : public PrebuiltEngineInterface {
private:
    std::map<int, int> mNumPacketsPerStream;
    std::mutex mLock;
    std::condition_variable mCv;
    bool mGraphTerminated = false;

public:
    // Prebuilt to engine interface
    void DispatchPixelData(int streamId, int64_t timestamp,
                           const runner::InputFrame& frame) override {
        ASSERT_EQ(streamId % 2, 0);
        std::lock_guard lock(mLock);
        if (mNumPacketsPerStream.find(streamId) == mNumPacketsPerStream.end()) {
            mNumPacketsPerStream[streamId] = 1;
        } else {
            mNumPacketsPerStream[streamId]++;
        }
    }

    void DispatchSerializedData(int streamId, int64_t timestamp, std::string&& data) override {
        ASSERT_EQ(streamId % 2, 1);
        std::lock_guard lock(mLock);
        if (mNumPacketsPerStream.find(streamId) == mNumPacketsPerStream.end()) {
            mNumPacketsPerStream[streamId] = 1;
        } else {
            mNumPacketsPerStream[streamId]++;
        }
    }

    void DispatchGraphTerminationMessage(Status status, std::string&& msg) override {
        std::lock_guard lock(mLock);
        mGraphTerminated = true;
        mCv.notify_one();
    }

    bool waitForTermination() {
        std::unique_lock lock(mLock);
        if (!mGraphTerminated) {
            mCv.wait_for(lock, std::chrono::seconds(10), [this] { return mGraphTerminated; });
        }
        return mGraphTerminated;
    }

    int numPacketsForStream(int streamId) {
        std::lock_guard lock(mLock);
        auto it = mNumPacketsPerStream.find(streamId);
        if (it == mNumPacketsPerStream.end()) {
            return 0;
        }
        return it->second;
    }
};

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // CPP_COMPUTEPIPE_TESTS_RUNNER_GRAPH_INCLUDES_GRPCGRAPHSERVERIMPL_H_
