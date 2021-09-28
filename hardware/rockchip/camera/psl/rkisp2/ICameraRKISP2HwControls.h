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

#ifndef CAMERA2600_ICAMERARKISP1HWCONTROLS_H_
#define CAMERA2600_ICAMERARKISP1HWCONTROLS_H_

#include <linux/types.h>
#include <linux/v4l2-controls.h>

namespace android {
namespace camera2 {

#define CAM_WXH_STR(w,h) STRINGIFY_(w##x##h)
#define CAM_RESO_STR(w,h) CAM_WXH_STR(w,h)  // example: CAM_RESO_STR(VGA_WIDTH,VGA_HEIGHT) -> "640x480"

/* Abstraction of HW sensor control interface for 3A support */
class ICameraRKISP2HwControls {
public:
    virtual ~ICameraRKISP2HwControls() { }

    virtual int getCurrentCameraId(void) = 0;

    virtual int getActivePixelArraySize(int &width, int &height, int &code) = 0;
    virtual int getSensorOutputSize(int &width, int &height, int &code) = 0;
    virtual int getPixelRate(int &pixel_rate) = 0;
    virtual int setExposure(int coarse_exposure, int fine_exposure) = 0;
    virtual int getExposure(int &coarse_exposure, int &fine_exposure) = 0;
    virtual int setGains(int analog_gain, int digital_gain) = 0;
    virtual int getGains(int &analog_gain, int &digital_gain) = 0;
    virtual int setFrameDuration(unsigned int llp, unsigned int fll) = 0;
    virtual int getFrameDuration(unsigned int &llp, unsigned int &fll) = 0;
};

/* Abstraction of HW lens control interface for 3A support */
class IRKISP1HWLensControl {
public:
    virtual ~IRKISP1HWLensControl() { }

    virtual const char* getLensName(void)= 0;
    virtual int getCurrentCameraId(void)= 0;

    // FOCUS
    virtual int moveFocusToPosition(int position) = 0;
    virtual int moveFocusToBySteps(int steps) = 0;
    virtual int getFocusPosition(int &position) = 0;
    virtual int getFocusStatus(int &status) = 0;
    virtual int startAutoFocus(void) = 0;
    virtual int stopAutoFocus(void) = 0;
    virtual int getAutoFocusStatus(int &status) = 0;
    virtual int setAutoFocusRange(int value) = 0;
    virtual int getAutoFocusRange(int &value) = 0;
    virtual int enableOis(bool enable) = 0;

    // ZOOM
    virtual int moveZoomToPosition(int position) = 0;
    virtual int moveZoomToBySteps(int steps) = 0;
    virtual int getZoomPosition(int &position) = 0;
    virtual int moveZoomContinuous(int position) = 0;

};

}   // namespace camera2
}   // namespace android
#endif  // CAMERA2600_ICAMERARKISP1HWCONTROLS_H_
