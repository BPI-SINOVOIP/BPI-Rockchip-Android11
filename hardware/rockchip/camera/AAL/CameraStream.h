/*
 * Copyright (C) 2013-2017 Intel Corporation
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

#ifndef _CAMERA3_HAL_CAMERA_STREAM_H_
#define _CAMERA3_HAL_CAMERA_STREAM_H_

#include <mutex>
#include <atomic>
#include <hardware/camera3.h>
#include "CameraStreamNode.h"
#include "CameraBuffer.h"
#include "PlatformData.h" // for macro MAX_REQUEST_IN_PROCESS_NUM
#include "ICameraHw.h"
#include <memory>
#include "PerformanceTraces.h"

NAMESPACE_DECLARATION {

enum STREAM_TYPE {
    STREAM_PREVIEW = 1,
    STREAM_CAPTURE = 1 << 1,
    STREAM_VIDEO = 1 << 2,
    STREAM_ZSL = 1 << 3
};

class IRequestCallback;
class Camera3Request;

/**
 * \class CameraStream
 * This class represents a user created stream
 *
 * This class is an HAL internal representation of the user provided stream
 * It is stored in the priv field of the camera3_stream_t passed by the user.
 *
 * It handles the  sequential return of buffers from the Camera HW class
 * Each CameraStream is bound to a HW counter part that will produce the data
 *
 * This class can be used by request thread and callback thread
 *
 */
class CameraStream : public CameraStreamNode {
public:
    CameraStream(int seqNo, camera3_stream_t * stream, IRequestCallback * callback);
    ~CameraStream();

    void setActive(bool active);
    bool isActive() const {return mActive; }

    void dump(bool dumpBuffers = false) const;

    int seqNo(void) const { return mSeqNo; };
    int width(void) const { return mStream3->width; }
    int height(void) const { return mStream3->height; }
    int format(void) const { return mStream3->format; }
    int buffersNum(void) const { return mCamera3Buffers.size(); }
    //override API
    virtual int usage(void) const { return (mStream3 ? mStream3->usage : 0); }
    virtual status_t query(FrameInfo *info);
    virtual status_t capture(std::shared_ptr<CameraBuffer> aBuffer,
                             Camera3Request* request);
    virtual status_t captureDone(std::shared_ptr<CameraBuffer> aBuffer,
                                 Camera3Request* request = NULL);
    virtual status_t reprocess(std::shared_ptr<CameraBuffer> aBuffer,
                             Camera3Request* request);

    status_t processRequest(Camera3Request* request);
    camera3_stream_t * getStream() const { return mStream3;};
    void dump(int fd) const;

    void incOutBuffersInHal() { mOutputBuffersInHal++; }
    void decOutBuffersInHal() { mOutputBuffersInHal--; }
    int32_t outBuffersInHal() { return mOutputBuffersInHal; }
    int getStreamType() { return mStreamType; }

private: /* Methods */
    // CameraStreamNode override API
    virtual status_t configure(void);
    void showDebugFPS(int streamType);

private: /* Members */
    bool mActive;   /* Tracks the status of the stream during config time*/
    int mSeqNo;     /* Index of the stream */
    int mStreamType;
    IRequestCallback * mCallback;
    std::atomic<int32_t> mOutputBuffersInHal;

    std::vector<std::shared_ptr<CameraBuffer> > mCamera3Buffers;
    camera3_stream_t * mStream3; /* one stream of config_streams from client which not owned here */
    std::vector<Camera3Request*>      mPendingRequests;
    std::mutex mPendingLock; /* Protects mPendingRequests */
    int mFrameCount;
    int mLastFrameCount;
    nsecs_t mLastFpsTime;
};

} NAMESPACE_DECLARATION_END

#endif // _CAMERA3_HAL_CAMERA_STREAM_H_
