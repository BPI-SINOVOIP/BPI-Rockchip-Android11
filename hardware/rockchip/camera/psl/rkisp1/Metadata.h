/*
 * Copyright (C) 2016 - 2017 Intel Corporation.
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

#ifndef PSL_RKISP1_METADATA_H_
#define PSL_RKISP1_METADATA_H_

#include "ControlUnit.h"
#include "PlatformData.h"
#include <math.h>

namespace android {
namespace camera2 {

class Metadata
{
public:
    Metadata(int cameraId);
    virtual ~Metadata();
    status_t init();

    void writeJpegMetadata(RequestCtrlState &reqState) const;
    void writeRestMetadata(RequestCtrlState &reqState) const;

private:
    void checkResultMetadata(CameraMetadata *results, int cameraId) const;

private:
    int mCameraId;
};
} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_SETTINGSPROCESSOR_H_ */
