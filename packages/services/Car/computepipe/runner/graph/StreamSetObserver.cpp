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

#include "StreamSetObserver.h"

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

SingleStreamObserver::SingleStreamObserver(int streamId, EndOfStreamReporter* endOfStreamReporter,
                                           StreamGraphInterface* streamGraphInterface) :
      mStreamId(streamId),
      mEndOfStreamReporter(endOfStreamReporter),
      mStreamGraphInterface(streamGraphInterface) {}

Status SingleStreamObserver::startObservingStream() {
    {
        std::lock_guard lock(mStopObservationLock);
        mStopped = false;
    }
    mThread = std::thread([this]() {
        proto::ObserveOutputStreamRequest observeStreamRequest;
        observeStreamRequest.set_stream_id(mStreamId);
        ::grpc::ClientContext context;
        ::grpc::CompletionQueue cq;

        void* tag;
        bool cqStatus;

        std::unique_ptr<::grpc::ClientAsyncReader<proto::OutputStreamResponse>> rpc(
                mStreamGraphInterface->getServiceStub()
                        ->AsyncObserveOutputStream(&context, observeStreamRequest, &cq, nullptr));

        proto::OutputStreamResponse response;

        cq.Next(&tag, &cqStatus);
        while (cqStatus) {
            // Dispatch data only stream is being observed.
            rpc->Read(&response, nullptr);
            {
                std::lock_guard lock(mStopObservationLock);
                if (mStopped || mStreamGraphInterface == nullptr) {
                    LOG(INFO) << "Graph stopped. ";
                    break;
                }

                // Since this is a separate thread, we need not worry about recursive locking
                // and callback can be executed without creating a new thread.
                if (response.has_pixel_data()) {
                    proto::PixelData pixels = response.pixel_data();
                    runner::InputFrame frame(pixels.height(), pixels.width(),
                                             static_cast<PixelFormat>(
                                                     static_cast<int>(pixels.format())),
                                             pixels.step(),
                                             reinterpret_cast<const unsigned char*>(
                                                     pixels.data().c_str()));
                    mStreamGraphInterface->dispatchPixelData(mStreamId, response.timestamp_us(),
                                                             frame);
                } else if (response.has_semantic_data()) {
                    mStreamGraphInterface
                            ->dispatchSerializedData(mStreamId, response.timestamp_us(),
                                                     std::move(*response.mutable_semantic_data()));
                }
            }

            cq.Next(&tag, &cqStatus);
        }

        ::grpc::Status grpcStatus;
        rpc->Finish(&grpcStatus, nullptr);
        if (!grpcStatus.ok()) {
            LOG(ERROR) << "Failed RPC with message: " << grpcStatus.error_message();
        }

        cq.Shutdown();
        if (mEndOfStreamReporter) {
            std::lock_guard lock(mStopObservationLock);
            mStopped = true;
            std::thread t =
                    std::thread([this]() { mEndOfStreamReporter->reportStreamClosed(mStreamId); });

            t.detach();
        }

        proto::OutputStreamResponse streamResponse;
    });

    return Status::SUCCESS;
}

void SingleStreamObserver::stopObservingStream() {
    std::lock_guard lock(mStopObservationLock);
    mStopped = true;
}

SingleStreamObserver::~SingleStreamObserver() {
    stopObservingStream();

    if (mThread.joinable()) {
        mThread.join();
    }
}

StreamSetObserver::StreamSetObserver(const runner::ClientConfig& clientConfig,
                                     StreamGraphInterface* streamGraphInterface) :
      mClientConfig(clientConfig), mStreamGraphInterface(streamGraphInterface) {}

Status StreamSetObserver::startObservingStreams() {
    std::lock_guard lock(mLock);
    std::map<int, int> outputConfigs = {};
    mClientConfig.getOutputStreamConfigs(outputConfigs);

    if (!mStopped || !mStreamObservers.empty()) {
        LOG(ERROR) << "Already started observing streams. Duplicate call is not allowed";
        return Status::ILLEGAL_STATE;
    }

    for (const auto& it : outputConfigs) {
        auto streamObserver =
                std::make_unique<SingleStreamObserver>(it.first, this, mStreamGraphInterface);
        Status status = streamObserver->startObservingStream();
        if (status != Status::SUCCESS) {
            std::thread t([this]() { stopObservingStreams(true); });
            t.detach();
            return status;
        }
        mStreamObservers.emplace(std::make_pair(it.first, std::move(streamObserver)));
    }

    mStopped = mStreamObservers.empty();
    return Status::SUCCESS;
}

void StreamSetObserver::stopObservingStreams(bool stopImmediately) {
    std::unique_lock lock(mLock);
    if (mStopped) {
        // Separate thread is necessary here to avoid recursive locking.
        if (mGraphTerminationThread.joinable()) {
            mGraphTerminationThread.join();
        }
        mGraphTerminationThread = std::thread([this]() {
            this->mStreamGraphInterface->dispatchGraphTerminationMessage(Status::SUCCESS, "");
        });
        return;
    }

    // Wait for the streams to close if we are not stopping immediately.
    if (stopImmediately) {
        for (auto& it : mStreamObservers) {
            it.second->stopObservingStream();
        }

        mStoppedCv.wait(lock, [this]() -> bool { return mStopped; });
    }
}

void StreamSetObserver::reportStreamClosed(int streamId) {
    std::lock_guard lock(mLock);
    auto streamObserver = mStreamObservers.find(streamId);
    if (streamObserver == mStreamObservers.end()) {
        return;
    }
    mStreamObservers.erase(streamObserver);
    if (mStreamObservers.empty()) {
        mStopped = true;
        mStoppedCv.notify_one();
        mGraphTerminationThread = std::thread([streamGraphInterface(mStreamGraphInterface)]() {
            streamGraphInterface->dispatchGraphTerminationMessage(Status::SUCCESS, "");
        });
    }
}

StreamSetObserver::~StreamSetObserver() {
    std::unique_lock lock(mLock);
    if (mGraphTerminationThread.joinable()) {
        mGraphTerminationThread.join();
    }
}

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
