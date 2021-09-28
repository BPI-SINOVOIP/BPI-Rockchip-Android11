/*
 * Copyright (C) 2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef PSL_RKISP1_WORKERS_RKISP2InputFrameWorker_H_
#define PSL_RKISP1_WORKERS_RKISP2InputFrameWorker_H_

#include "RKISP2IDeviceWorker.h"
#include "NodeTypes.h"
#include "tasks/RKISP2ICaptureEventSource.h"
#include "RKISP2PostProcessPipeline.h"
namespace android {
namespace camera2 {
namespace rkisp2 {

class RKISP2InputFrameWorker: public RKISP2IDeviceWorker, public RKISP2ICaptureEventSource, public RKISP2IPostProcessListener
{
public:
    RKISP2InputFrameWorker(int cameraId,
                     camera3_stream_t* stream, std::vector<camera3_stream_t*>& outStreams,
                     size_t pipelineDepth);
    virtual ~RKISP2InputFrameWorker();

    virtual status_t configure(bool configChanged);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();
    virtual status_t flushWorker();
    status_t stopWorker();
    status_t startWorker();
    std::shared_ptr<V4L2VideoNode> getNode() const { return nullptr; }
    status_t notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                            const std::shared_ptr<RKISP2ProcUnitSettings>& settings,
                            int err);
    virtual status_t asyncPollDone(std::shared_ptr<DeviceMessage> msg, bool polled) {
        mMsg = msg;
        return OK;
    }

private:
    status_t bufferDone(std::shared_ptr<PostProcBuffer> buf);
    std::shared_ptr<CameraBuffer> findInputBuffer(Camera3Request* request,
                                             camera3_stream_t* stream);
    std::vector<std::shared_ptr<CameraBuffer>> findOutputBuffers(Camera3Request* request);
    status_t prepareBuffer(std::shared_ptr<CameraBuffer>& buffer);
    void returnBuffers();

private:
    std::vector<std::shared_ptr<CameraBuffer>> mProcessingInputBufs;
    camera3_stream_t* mStream; /* RKISP2InputFrameWorker doesn't own mStream */
    std::vector<camera3_stream_t*> mOutputStreams; /* RKISP2InputFrameWorker doesn't own mStream */
    bool mNeedPostProcess;
    size_t mPipelineDepth;
    int mBufferReturned;
    std::mutex mBufDoneLock;
    std::condition_variable mCondition;
    // for bufferdone in order, store the buffers arrived ahead of time
    std::vector<std::shared_ptr<PostProcBuffer>> mProcessingPostProcBufs;
    // for bufferdone in order, store the processing requests
    std::vector<Camera3Request *> mProcessingRequests;

    std::unique_ptr<RKISP2PostProcessPipeline> mPostPipeline;
};

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_WORKERS_RKISP2InputFrameWorker_H_ */
