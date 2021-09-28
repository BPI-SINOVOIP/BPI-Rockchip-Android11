/*
 * Copyright (C) 2016-2017 Intel Corporation
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

#ifndef PSL_RKISP1_RKISP2RequestCtrlState_H_
#define PSL_RKISP1_RKISP2RequestCtrlState_H_

#include "Camera3Request.h"
#include "RKISP2CaptureUnitSettings.h"
#include "RKISP2GraphConfig.h"
#include "RKISP2ProcUnitSettings.h"
#include "CaptureUnit.h"

namespace android {
namespace camera2 {
namespace rkisp2 {

/**
 * \struct RKISP2RequestCtrlState
 */
struct RKISP2RequestCtrlState {
    void init(Camera3Request *req, std::shared_ptr<RKISP2GraphConfig> aGraphConfig);
    static void reset(RKISP2RequestCtrlState* me);
    CameraMetadata *ctrlUnitResult; /**< metadata results written in the
                                         context of the ControlUnit */
    Camera3Request *request;        /**< user request associated to this AIQ
                                         configuration */
    std::shared_ptr<RKISP2CaptureUnitSettings> captureSettings; /**< Results from 3A calculations */
    std::shared_ptr<RKISP2ProcUnitSettings> processingSettings; /**< Per request parameters
                                                        for the processing unit */
    std::shared_ptr<RKISP2GraphConfig> graphConfig; /**< Details of the graph configuration for this request */

    bool shutterDone;
    uint8_t intent;          /**< Capture intent, needed for precapture */

    //Metadata reuslt are mainly divided into three parts
    //1. some settings from app
    //2. 3A metas from Control loop
    //3. some items like sensor timestamp from shutter
    //3a metadata received from Control loop
    bool mClMetaReceived;
    //metadata received from ImgUnit, now only sensor timestamp
    bool mShutterMetaReceived;
    //imgunit process done
    bool mImgProcessDone;
};

} // namespace rkisp2
} // namespace camera2
} // namespace android

#endif /* PSL_RKISP1_RKISP2RequestCtrlState_H_ */
