/*
 * Copyright (C) 2014-2017 Intel Corporation
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
#ifndef _CAMERA3_HAL_RKISP1CAMERAHW_H_
#define _CAMERA3_HAL_RKISP1CAMERAHW_H_

#include "ICameraHw.h"

#include "HwStreamBase.h"
#include "GraphConfigManager.h"
#include "MediaController.h"

namespace android {
namespace camera2 {

class ControlUnit;
class ImguUnit;
class TuningServer;
/**
 * \enum
 * This enum is used as index when acquiring the partial result metadata buffer
 * In theory there should be one metadata partial result per thread context
 * that writes result
 * in RKISP1 ControlUnit and Capture Unit update metadata result and return it
 */
enum PartialResultEnum {
    CONTROL_UNIT_PARTIAL_RESULT = 0,
    PARTIAL_RESULT_COUNT /* keep last to use as counter */
};

class RKISP1CameraHw: public ICameraHw {
 public:
    explicit RKISP1CameraHw(int cameraId);
    virtual ~RKISP1CameraHw();

  // override of ICameraHw
    virtual status_t init();
    virtual const camera_metadata_t * getDefaultRequestSettings(int type);
    virtual status_t bindStreams(std::vector<CameraStreamNode *> activeStreams);
    virtual status_t configStreams(std::vector<camera3_stream_t*> &activeStreams,
                                   uint32_t operation_mode);
    virtual status_t processRequest(Camera3Request* request, int inFlightCount);
    virtual void registerErrorCallback(IErrorCallback* errCb);
    virtual status_t flush();
    virtual void dump(int fd);
    void sendTuningDumpCmd(int w, int h);
 private:
    enum UseCase {
        USECASE_STILL,
        USECASE_VIDEO,
        USECASE_TUNING
    };
 // prevent copy constructor and assignment operator
 private:
    RKISP1CameraHw(const RKISP1CameraHw& other);
    RKISP1CameraHw& operator=(const RKISP1CameraHw& other);

    void deInit();
    status_t  initStaticMetadata();
    status_t checkStreamSizes(std::vector<camera3_stream_t*> &activeStreams);
    status_t checkStreamRotation(const std::vector<camera3_stream_t*> activeStreams);
    camera3_stream_t* findStreamForStillCapture(const std::vector<camera3_stream_t*>& streams);

    void checkNeedReconfig(UseCase newUseCase, std::vector<camera3_stream_t*> &activeStreams);
    UseCase checkUseCase(Camera3Request* request);
    int64_t getMinFrameDurationNs(camera3_stream_t* stream);
    status_t doConfigureStreams(UseCase newUseCase, uint32_t operation_mode, int32_t testPatternMode);

 private:  //members
    int mCameraId;
    bool mConfigChanged;
    /////////////////////////////////////////////////////////////////////
    // for tuning tool
    camera3_stream_t mFakeRawStream;   //fake tuning stream
    bool mTuningSizeChanged;
    /////////////////////////////////////////////////////////////////////

    CameraMetadata* mStaticMeta;
    /**
     * locally cached static metadata tag values
     */
    int mPipelineDepth;  /*!< How many request we allow in the PSL at one time*/
    ImguUnit *mImguUnit;
    ControlUnit *mControlUnit;
    TuningServer *mTuningServer;
    // Vector to store dummy Hw streams
    std::vector<std::shared_ptr<HwStreamBase>> mDummyHwStreamsVector;
    GraphConfigManager mGCM;

    std::shared_ptr<MediaController> mMediaCtl;
    std::shared_ptr<MediaController> mImguMediaCtl;

    // Configuring ISP with large size will lead to low fps.
    // So ignore stream with large size if requests don't require its output.
    // The bool flag records if those streams are configured.
    UseCase mUseCase;
    std::vector<camera3_stream_t*> mStreamsStill;
    std::vector<camera3_stream_t*> mStreamsVideo;
    uint32_t mOperationMode;
    int32_t mTestPatternMode;
    status_t getTestPatternMode(Camera3Request* request, int32_t* testPatternMode);
};

} /* namespace camera2 */
} /* namespace android */
#endif /* _CAMERA3_HAL_RKISP1CAMERAHW_H_ */
