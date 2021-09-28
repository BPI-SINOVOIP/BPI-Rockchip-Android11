/*
 * Copyright (C) 2017 Intel Corporation
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

#ifndef AAA_ANDROIDISPCONTROLS_H_
#define AAA_ANDROIDISPCONTROLS_H_

namespace android {
namespace camera2 {

const uint8_t ANDROID_MAX_STRENGTH = 10;

/**
 * \struct NoiseReductionControls
 *
 * Noise reduction saved and passed for post processing
 *
 */
struct NoiseReductionControls {
    uint8_t mode;       /**< ANDROID_NOISE_REDUCTION_MODE */
    uint8_t strength;   /**< ANDROID_NOISE_REDUCTION_STRENGTH*/

    NoiseReductionControls():
        mode(0),
        strength(0) {}
};

/**
 * \struct EdgeEnhancementControls
 *
 * Edge Enhancement setting saved for post processing
 *
 */
struct EdgeEnhancementControls {
    uint8_t mode;       /**< ANDROID_EDGE_MODE */
    uint8_t strength;   /**< ANDROID_EDGE_STRENGTH*/
    EdgeEnhancementControls() :
        mode(0),
        strength(0) {}
};


struct AndroidIspControls {
    uint8_t effect;             /**< ANDROID_CONTROL_EFFECT_MODE */
    NoiseReductionControls  nr;
    EdgeEnhancementControls ee;

    AndroidIspControls() :
        effect(0) {}
};

} // namespace camera2
} // namespace android
#endif //AAA_ANDROIDISPCONTROLS_H_
