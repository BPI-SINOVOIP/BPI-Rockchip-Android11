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

#ifndef PSL_RKISP1_WORKERS_FRAMEWORKER_H_
#define PSL_RKISP1_WORKERS_FRAMEWORKER_H_

#include "IDeviceWorker.h"
#include "v4l2device.h"

namespace android {
namespace camera2 {

class FrameWorker : public IDeviceWorker
{
public:
    FrameWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId,
                size_t pipelineDepth, std::string name = "FrameWorker");
    virtual ~FrameWorker();

    virtual status_t configure(bool configChanged);
    virtual status_t startWorker();
    virtual status_t flushWorker();
    virtual status_t stopWorker();
    virtual status_t prepareRun(std::shared_ptr<DeviceMessage> msg) = 0;

    virtual status_t attachNode(std::shared_ptr<V4L2VideoNode> node);

    // Restore the mMsg and mPollMe after async polled
    virtual status_t asyncPollDone(std::shared_ptr<DeviceMessage> msg, bool polled)
    {
        mMsg = msg;
        mPollMe = polled;
        return OK;
    };

    virtual status_t run() = 0;
    virtual status_t postRun() = 0;
    virtual bool needPolling() { return mPollMe && mNode->getBufsInDeviceCount(); }
    std::shared_ptr<V4L2VideoNode> getNode() const { return mNode; }
    virtual const char *name() { return mNode->name(); }

protected:
    status_t allocateWorkerBuffers();
    status_t setWorkerDeviceFormat(FrameInfo &frame);
    status_t setWorkerDeviceBuffers(int memType);

protected:
    std::vector<V4L2Buffer> mBuffers;
    unsigned int mIndex;
    std::string mName;
    std::vector<std::shared_ptr<CameraBuffer>> mCameraBuffers;

    V4L2Format mFormat;
    std::shared_ptr<V4L2VideoNode> mNode;
    bool mIsStarted;
    bool mPollMe;
    size_t mPipelineDepth;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_WORKERS_FRAMEWORKER_H_ */
