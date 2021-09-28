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

#ifndef COMPUTEPIPE_RUNNER_GRAPH_STREAM_SET_OBSERVER_H
#define COMPUTEPIPE_RUNNER_GRAPH_STREAM_SET_OBSERVER_H

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "GrpcPrebuiltGraphService.grpc.pb.h"
#include "GrpcPrebuiltGraphService.pb.h"
#include "InputFrame.h"
#include "RunnerComponent.h"
#include "types/Status.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace graph {

class GrpcGraph;

class EndOfStreamReporter {
  public:
    virtual ~EndOfStreamReporter() = default;

    virtual void reportStreamClosed(int streamId) = 0;
};

class StreamGraphInterface {
  public:
    virtual ~StreamGraphInterface() = default;

    virtual void dispatchPixelData(int streamId, int64_t timestamp_us,
                                   const runner::InputFrame& frame) = 0;

    virtual void dispatchSerializedData(int streamId, int64_t timestamp_us,
                                        std::string&& serialized_data) = 0;

    virtual void dispatchGraphTerminationMessage(Status, std::string&&) = 0;

    virtual proto::GrpcGraphService::Stub* getServiceStub() = 0;
};

class SingleStreamObserver {
  public:
    SingleStreamObserver(int streamId, EndOfStreamReporter* endOfStreamReporter,
                         StreamGraphInterface* streamGraphInterface);

    virtual ~SingleStreamObserver();

    Status startObservingStream();

    void stopObservingStream();
  private:
    int mStreamId;
    EndOfStreamReporter* mEndOfStreamReporter;
    StreamGraphInterface* mStreamGraphInterface;
    std::thread mThread;
    bool mStopped = true;
    std::mutex mStopObservationLock;
};

class StreamSetObserver : public EndOfStreamReporter {
  public:
    virtual ~StreamSetObserver();

    StreamSetObserver(const runner::ClientConfig& clientConfig,
                      StreamGraphInterface* streamGraphInterface);

    Status startObservingStreams();

    void stopObservingStreams(bool stopImmediately);

    void reportStreamClosed(int streamId) override;
  private:
    const runner::ClientConfig& mClientConfig;
    StreamGraphInterface* mStreamGraphInterface;
    std::map<int, std::unique_ptr<SingleStreamObserver>> mStreamObservers;
    std::mutex mLock;
    std::condition_variable mStoppedCv;
    std::thread mGraphTerminationThread;
    bool mStopped = true;
};

}  // namespace graph
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif  // #define COMPUTEPIPE_RUNNER_GRAPH_STREAM_SET_OBSERVER_H
