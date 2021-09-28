/*
 * Copyright 2020 The Android Open Source Project
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

#ifndef SURROUND_VIEW_SERVICE_IMPL_ANIMATION_H_
#define SURROUND_VIEW_SERVICE_IMPL_ANIMATION_H_

#include "IOModuleCommon.h"
#include "core_lib.h"

#include <utils/SystemClock.h>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>

using namespace ::android::hardware::automotive::vehicle::V2_0;
using namespace android_auto::surround_view;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

// Car animation class. It is constructed with textures, animations, and
// vhal_handler. It automatically updates animation params when
// GetUpdatedAnimationParams() is called.
class AnimationModule {
public:
    // Constructor.
    // |parts| is from I/O module. The key value is part id.
    // |textures| is from I/O module. The key value is texture id.
    // |animations| is from I/O module.
    AnimationModule(const std::map<std::string, CarPart>& partsMap,
                    const std::map<std::string, CarTexture>& texturesMap,
                    const std::vector<AnimationInfo>& animations);

    // Gets Animation parameters with input of VehiclePropValue.
    std::vector<AnimationParam> getUpdatedAnimationParams(
            const std::vector<VehiclePropValue>& vehiclePropValue);

private:
    // Internal car part status.
    struct CarPartStatus {
        // Car part id.
        std::string partId;

        // Car part children ids.
        std::vector<std::string> childIds;

        // Parent model matrix.
        Mat4x4 parentModel;

        // Local model in local coordinate.
        Mat4x4 localModel;

        // Current status model matrix in global coordinate with
        // animations combined.
        // current_model = local_model * parent_model;
        Mat4x4 currentModel;

        // Gamma parameters.
        float gamma;

        // Texture id.
        std::string textureId;

        // Internal vhal percentage. Each car part maintain its own copy
        // the vhal percentage.
        // Key value is vhal property (combined with area id).
        std::map<uint64_t, float> vhalProgressMap;

        // Vhal off map. Key value is vhal property (combined with area id).
        // Assume off status when vhal value is 0.
        std::map<uint64_t, bool> vhalOffMap;
    };

    // Internal Vhal status.
    struct VhalStatus {
        float vhalValueFloat;
    };

    // Help function to get vhal to parts map.
    void mapVhalToParts();

    // Help function to init car part status for constructor.
    void initCarPartStatus();

    // Iteratively update children parts status if partent status is changed.
    void updateChildrenParts(const std::string& partId, const Mat4x4& parentModel);

    // Perform gamma opertion for the part with given vhal property.
    void performGammaOp(const std::string& partId, uint64_t vhalProperty, const GammaOp& gammaOp);

    // Perform translation opertion for the part with given vhal property.
    void performTranslationOp(const std::string& partId, uint64_t vhalProperty,
                              const TranslationOp& translationOp);

    // Perform texture opertion for the part with given vhal property.
    // Not implemented yet.
    void performTextureOp(const std::string& partId, uint64_t vhalProperty,
                          const TextureOp& textureOp);

    // Perform rotation opertion for the part with given vhal property.
    void performRotationOp(const std::string& partId, uint64_t vhalProperty,
                           const RotationOp& rotationOp);

    // Last call time of GetUpdatedAnimationParams() in millisecond.
    float mLastCallTime;

    // Current call time of GetUpdatedAnimationParams() in millisecond.
    float mCurrentCallTime;

    // Flag indicating if GetUpdatedAnimationParams() was called before.
    bool mIsCalled;

    std::map<std::string, CarPart> mPartsMap;

    std::map<std::string, CarTexture> mTexturesMap;

    std::vector<AnimationInfo> mAnimations;

    std::map<std::string, AnimationInfo> mPartsToAnimationMap;

    std::map<uint64_t, VhalStatus> mVhalStatusMap;

    std::map<uint64_t, std::set<std::string>> mVhalToPartsMap;

    std::map<std::string, CarPartStatus> mCarPartsStatusMap;

    std::map<std::string, AnimationParam> mUpdatedPartsMap;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // SURROUND_VIEW_SERVICE_IMPL_ANIMATION_H_
