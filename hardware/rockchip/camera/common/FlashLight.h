/*
 * Copyright (C) 2014-2017 Intel Corporation.
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

#ifndef __FLASHLIGHT__H
#define __FLASHLIGHT__H

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <hardware/camera_common.h>

namespace android {
namespace camera2 {

#define MAX_NUM_CAMERA                  2
#define MAX_NUM_FLASH_OF_ONE_MODULE     2

class FlashLight {
public:
    static FlashLight& getInstance();

    int32_t setCallbacks(const camera_module_callbacks_t* callbacks);
    int32_t init(const int cameraId);
    int32_t deinit(const int cameraId);
    int32_t setFlashMode(const int cameraId, const bool on);
    int32_t reserveFlashForCamera(const int cameraId);
    int32_t releaseFlashFromCamera(const int cameraId);

private:
    FlashLight();
    virtual ~FlashLight();
    FlashLight(const FlashLight&);
    FlashLight& operator=(const FlashLight&);
    int32_t getFlashLightInfo(const int cameraId,
                              bool &hasFlash,
                              const char* (&flashNode)[MAX_NUM_FLASH_OF_ONE_MODULE]);
    int32_t setFlashMode(const int cameraId, int flashIdx, const bool on);

    const camera_module_callbacks_t *mCallbacks;
    int32_t mFlashFds[MAX_NUM_CAMERA][MAX_NUM_FLASH_OF_ONE_MODULE];
    bool mFlashOn[MAX_NUM_CAMERA][MAX_NUM_FLASH_OF_ONE_MODULE];
    bool mCameraOpen[MAX_NUM_CAMERA];
};

} /* namespace camera2 */
} /* namespace android */

#endif // __FLASHLIGHT__H

