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
#ifndef CAMERA3_HAL_RKISP1_PROCUNITSETTINGS_H_
#define CAMERA3_HAL_RKISP1_PROCUNITSETTINGS_H_

#include "Camera3Request.h"
#include "CaptureUnitSettings.h"
#include "GraphConfig.h"

namespace android {
namespace camera2 {

/**
 * \struct ProcUnitSettings
 *
 * Contains all the settings the processing unit needs to know to fulfill a
 * particular capture request.
 *
 * This is mainly the results from AIQ (3A + AIC) algorithms.
 * The GraphConfig object associated with this request
 *
 */
struct ProcUnitSettings {

    Camera3Request *request;
    CameraWindow cropRegion; /**< Crop region in ANDROID_COORDINATES */
    std::shared_ptr<CaptureUnitSettings> captureSettings;
    std::shared_ptr<GraphConfig> graphConfig;
    bool dump; /**< 'true' if (PAL) dump needs to be done */

    ProcUnitSettings() :
        request(nullptr),
        dump(false)
    {
        clearStructs(this);
    };

    static void clearStructs(ProcUnitSettings *me)
    {
        CLEAR(me->cropRegion);
    }

    /**
     * Used by the templated class SharedItemPool to clear the object when
     * an instance is returned to the pool.
     */
    static void reset(ProcUnitSettings *me)
    {
        if (CC_LIKELY(me != nullptr)) {
            clearStructs(me);
            me->request = nullptr;
            me->dump = false;
            me->captureSettings.reset();
            me->graphConfig.reset();
        } else {
            LOGE("Trying to reset a null ProcUnitSettings structure !! - BUG ");
        }
    }
};

} // namespace camera2
} // namespace android

#endif /* CAMERA3_HAL_RKISP1_PROCUNITSETTINGS_H_ */
