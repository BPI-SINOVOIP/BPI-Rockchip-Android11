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

#ifndef PSL_RKISP1_WORKERS_RKISP2OutputFrameWorker_H_
#define PSL_RKISP1_WORKERS_RKISP2OutputFrameWorker_H_

#include "RKISP2FrameWorker.h"
#include "tasks/RKISP2ICaptureEventSource.h"
//#include "tasks/RKISP2JpegEncodeTask.h"
#include "NodeTypes.h"
#include "RKISP2PostProcessPipeline.h"
namespace android {
namespace camera2 {
namespace rkisp2 {

class RKISP2OutputFrameWorker: public RKISP2FrameWorker, public RKISP2ICaptureEventSource, public RKISP2IPostProcessListener
{
public:
    RKISP2OutputFrameWorker(int cameraId, std::string name,
                      NodeTypes nodeName, size_t pipelineDepth);
    virtual ~RKISP2OutputFrameWorker();

    void addListener(camera3_stream_t* stream);
    void attachStream(camera3_stream_t* stream);
    void clearListeners();
    virtual status_t configure(bool configChanged);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();
    virtual status_t flushWorker();
    status_t stopWorker();
    status_t notifyNewFrame(const std::shared_ptr<PostProcBuffer>& buf,
                            const std::shared_ptr<RKISP2ProcUnitSettings>& settings,
                            int err);

private:
    std::shared_ptr<CameraBuffer> findBuffer(Camera3Request* request,
                                             camera3_stream_t* stream);
    status_t prepareBuffer(std::shared_ptr<CameraBuffer>& buffer);
    bool checkListenerBuffer(Camera3Request* request);
    std::shared_ptr<CameraBuffer> getOutputBufferForListener();
    void returnBuffers(bool returnListenerBuffers);
    status_t configPostPipeLine();

private:
    std::vector<std::shared_ptr<CameraBuffer>> mOutputBuffers;
    std::shared_ptr<CameraBuffer> mOutputBuffer;
    camera3_stream_t* mStream; /* RKISP2OutputFrameWorker doesn't own mStream */
    bool mNeedPostProcess;
    NodeTypes mNodeName;
    int mLastPipelineDepth;

    // For listeners
    std::vector<camera3_stream_t*> mListeners;
    std::shared_ptr<CameraBuffer> mOutputForListener;

    std::unique_ptr<RKISP2PostProcessPipeline> mPostPipeline;
    SharedItemPool<PostProcBuffer> mPostProcItemsPool;
    std::vector<std::shared_ptr<PostProcBuffer>> mPostWorkingBufs;
    std::shared_ptr<PostProcBuffer> mPostWorkingBuf;
};

} /* namespace rkisp2 */
} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_WORKERS_RKISP2OutputFrameWorker_H_ */
