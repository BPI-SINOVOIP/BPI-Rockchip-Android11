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

#include "CaptureUnit.h"
#include "ControlUnit.h"
#include "ProcUnitSettings.h"
#include "GraphConfigManager.h"

#ifndef PSL_RKISP1_SETTINGSPROCESSOR_H_
#define PSL_RKISP1_SETTINGSPROCESSOR_H_

namespace android {
namespace camera2 {

class SettingsProcessor
{
public:
    SettingsProcessor(int cameraId,
            IStreamConfigProvider &aStreamCfgProv);
    virtual ~SettingsProcessor();
    status_t init();

    /* Parameter processing methods */
    status_t processRequestSettings(const CameraMetadata &settings,
                                    RequestCtrlState &aiqCfg);

private:
    struct StaticMetadataCache {
        camera_metadata_ro_entry availableEffectModes;
        camera_metadata_ro_entry availableEdgeModes;
        camera_metadata_ro_entry availableNoiseReductionModes;
        camera_metadata_ro_entry availableTonemapModes;
        camera_metadata_ro_entry availableHotPixelMapModes;
        camera_metadata_ro_entry availableHotPixelModes;
        camera_metadata_ro_entry availableVideoStabilization;
        camera_metadata_ro_entry availableOpticalStabilization;
        camera_metadata_ro_entry currentAperture;
        camera_metadata_ro_entry currentFocalLength;
        camera_metadata_ro_entry flashInfoAvailable;
        camera_metadata_ro_entry lensShadingMapSize;
        camera_metadata_ro_entry maxAnalogSensitivity;
        camera_metadata_ro_entry pipelineDepth;
        camera_metadata_ro_entry lensSupported;
        camera_metadata_ro_entry availableTestPatternModes;
        StaticMetadataCache() { CLEAR(*this); }

        status_t getFlashInfoAvailable(bool &available) const
        {
            if (flashInfoAvailable.count == 1) {
                available = flashInfoAvailable.data.u8[0];
                return OK;
            }
            return NAME_NOT_FOUND;
        }

        status_t getPipelineDepth(uint8_t &depth) const
        {
            if (pipelineDepth.count == 1) {
                depth = pipelineDepth.data.u8[0];
                return OK;
            }
            return NAME_NOT_FOUND;
        }
    };

public:
    const StaticMetadataCache& getStaticMetadataCache() const
    {
        return mStaticMetadataCache;
    }


private:
    void cacheStaticMetadata();

    void processCroppingRegion(const CameraMetadata &settings,
                                   RequestCtrlState &reqAiqCfg);
private:
    /**
     * Static values cached at init to avoid multiple find operations
     * on the static metadata
     */
    CameraWindow    mAPA;   /*!< Active Pixel Array */
    StaticMetadataCache mStaticMetadataCache; /*!< metadata fetched at init */
    int mCameraId;

    /*
     * Provider of details of the stream configuration
     */
    IStreamConfigProvider &mStreamCfgProv;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_SETTINGSPROCESSOR_H_ */
