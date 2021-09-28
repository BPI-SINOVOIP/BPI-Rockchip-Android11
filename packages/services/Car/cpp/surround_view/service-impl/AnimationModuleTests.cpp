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

#define LOG_TAG "AnimationModuleTests"

#include "AnimationModule.h"
#include "MathHelp.h"

#include <android/hardware/automotive/vehicle/2.0/IVehicle.h>
#include <gtest/gtest.h>
#include <map>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {
namespace {

std::map<std::string, CarPart> getSampleCarPartsMap() {
    std::vector<std::string> carFrameChildPartIds{"front_left_door", "front_right_door",
                                                  "front_left_blinker", "front_right_blinker",
                                                  "sun_roof"};

    android_auto::surround_view::CarPart frame(std::vector<CarVertex>(),
                                               android_auto::surround_view::CarMaterial(),
                                               gMat4Identity, "root", carFrameChildPartIds);

    android_auto::surround_view::CarPart frameChild(std::vector<CarVertex>(),
                                                    android_auto::surround_view::CarMaterial(),
                                                    gMat4Identity, "frame",
                                                    std::vector<std::string>());

    std::map<std::string, CarPart> sampleCarParts;
    sampleCarParts.emplace(std::make_pair("frame", frame));
    sampleCarParts.emplace(std::make_pair("front_left_door", frameChild));
    sampleCarParts.emplace(std::make_pair("front_right_door", frameChild));
    sampleCarParts.emplace(std::make_pair("front_left_blinker", frameChild));
    sampleCarParts.emplace(std::make_pair("front_right_blinker", frameChild));
    sampleCarParts.emplace(std::make_pair("sun_roof", frameChild));
    return sampleCarParts;
}

std::vector<AnimationInfo> getSampleAnimations() {
    AnimationInfo frameAnimation = AnimationInfo{
            .partId = "frame",
            .parentId = "root",
            .pose = gMat4Identity,
    };

    RotationOp frontLeftDoorRotationOp =
            RotationOp{.vhalProperty = (int64_t)(0x0200 | VehiclePropertyGroup::SYSTEM |
                                                 VehiclePropertyType::INT32 | VehicleArea::DOOR)
                                       << 32 |
                               (int64_t)(VehicleArea::DOOR),
                       .type = AnimationType::ROTATION_ANGLE,
                       .axis =
                               RotationAxis{
                                       .axisVector = std::array<float, 3>{0.0f, 0.0f, 1.0f},
                                       .rotationPoint = std::array<float, 3>{-1.0f, 0.5f, 0.0f},
                               },
                       .animationTime = 2000,
                       .rotationRange =
                               Range{
                                       .start = 0.0f,
                                       .end = 90.0f,
                               },
                       .vhalRange = Range{
                               .start = 0.0f,
                               .end = (float)INT32_MAX,
                       }};

    std::map<uint64_t, std::vector<RotationOp>> frontLeftDoorRotationOpsMap;

    frontLeftDoorRotationOpsMap.emplace(
            std::make_pair(frontLeftDoorRotationOp.vhalProperty,
                           std::vector<RotationOp>{frontLeftDoorRotationOp}));

    AnimationInfo frontLeftDoorAnimation = AnimationInfo{
            .partId = "front_left_door",
            .parentId = "frame",
            .pose = gMat4Identity,
            .rotationOpsMap = frontLeftDoorRotationOpsMap,
    };

    RotationOp frontRightDoorRotationOp =
            RotationOp{.vhalProperty = (int64_t)(0x0201 | VehiclePropertyGroup::SYSTEM |
                                                 VehiclePropertyType::INT32 | VehicleArea::DOOR)
                                       << 32 |
                               (int64_t)(VehicleArea::DOOR),
                       .type = AnimationType::ROTATION_ANGLE,
                       .axis =
                               RotationAxis{
                                       .axisVector = std::array<float, 3>{0.0f, 0.0f, 1.0f},
                                       .rotationPoint = std::array<float, 3>{1.0f, 0.5f, 0.0f},
                               },
                       .animationTime = 2000,
                       .rotationRange =
                               Range{
                                       .start = 0.0f,
                                       .end = -90.0f,
                               },
                       .vhalRange = Range{
                               .start = 0.0f,
                               .end = (float)INT32_MAX,
                       }};

    std::map<uint64_t, std::vector<RotationOp>> frontRightDoorRotationOpsMap;

    frontRightDoorRotationOpsMap.emplace(
            std::make_pair(frontRightDoorRotationOp.vhalProperty,
                           std::vector<RotationOp>{frontRightDoorRotationOp}));

    AnimationInfo frontRightDoorAnimation = AnimationInfo{
            .partId = "front_right_door",
            .parentId = "frame",
            .pose = gMat4Identity,
            .rotationOpsMap = frontRightDoorRotationOpsMap,
    };

    GammaOp frontLeftBlinkerGammaOp = GammaOp{
            .vhalProperty = (int64_t)(0x0300 | VehiclePropertyGroup::SYSTEM |
                                      VehiclePropertyType::INT32 | VehicleArea::GLOBAL)
                            << 32 |
                    (int64_t)(VehicleArea::GLOBAL),
            .type = AnimationType::ADJUST_GAMMA_REPEAT,
            .animationTime = 1000,
            .gammaRange =
                    Range{
                            .start = 1.0f,
                            .end = 0.5f,
                    },
            .vhalRange =
                    Range{
                            .start = 0.0f,
                            .end = (float)INT32_MAX,
                    },
    };

    std::map<uint64_t, std::vector<GammaOp>> frontLeftBlinkerGammaOpsMap;

    frontLeftBlinkerGammaOpsMap.emplace(
            std::make_pair(frontLeftBlinkerGammaOp.vhalProperty,
                           std::vector<GammaOp>{frontLeftBlinkerGammaOp}));

    AnimationInfo frontLeftBlinkerAnimation = AnimationInfo{
            .partId = "front_left_blinker",
            .parentId = "frame",
            .pose = gMat4Identity,
            .gammaOpsMap = frontLeftBlinkerGammaOpsMap,
    };

    GammaOp frontRightBlinkerGammaOp = GammaOp{
            .vhalProperty = (int64_t)(0x0301 | VehiclePropertyGroup::SYSTEM |
                                      VehiclePropertyType::INT32 | VehicleArea::GLOBAL)
                            << 32 |
                    (int64_t)(VehicleArea::GLOBAL),
            .type = AnimationType::ADJUST_GAMMA_REPEAT,
            .animationTime = 1000,
            .gammaRange =
                    Range{
                            .start = 1.0f,
                            .end = 0.5f,
                    },
            .vhalRange =
                    Range{
                            .start = 0.0f,
                            .end = (float)INT32_MAX,
                    },
    };

    std::map<uint64_t, std::vector<GammaOp>> frontRightBlinkerGammaOpsMap;

    frontRightBlinkerGammaOpsMap.emplace(
            std::make_pair(frontRightBlinkerGammaOp.vhalProperty,
                           std::vector<GammaOp>{frontRightBlinkerGammaOp}));

    AnimationInfo frontRightBlinkerAnimation = AnimationInfo{
            .partId = "front_right_blinker",
            .parentId = "frame",
            .pose = gMat4Identity,
            .gammaOpsMap = frontRightBlinkerGammaOpsMap,
    };

    TranslationOp sunRoofTranslationOp = TranslationOp{
            .vhalProperty = (int64_t)(0x0400 | VehiclePropertyGroup::SYSTEM |
                                      VehiclePropertyType::INT32 | VehicleArea::GLOBAL)
                            << 32 |
                    (int64_t)(VehicleArea::GLOBAL),
            .type = AnimationType::TRANSLATION,
            .direction = std::array<float, 3>{0.0f, -1.0f, 0.0f},
            .animationTime = 3000,
            .translationRange =
                    Range{
                            .start = 0.0f,
                            .end = 0.5f,
                    },
            .vhalRange =
                    Range{
                            .start = 0.0f,
                            .end = (float)INT32_MAX,
                    },
    };

    std::map<uint64_t, std::vector<TranslationOp>> sunRoofRotationOpsMap;
    sunRoofRotationOpsMap.emplace(std::make_pair(sunRoofTranslationOp.vhalProperty,
                                                 std::vector<TranslationOp>{sunRoofTranslationOp}));

    AnimationInfo sunRoofAnimation = AnimationInfo{
            .partId = "sun_roof",
            .parentId = "frame",
            .pose = gMat4Identity,
            .translationOpsMap = sunRoofRotationOpsMap,
    };

    return std::vector<AnimationInfo>{frameAnimation,
                                      frontLeftDoorAnimation,
                                      frontRightDoorAnimation,
                                      frontLeftBlinkerAnimation,
                                      frontRightBlinkerAnimation,
                                      sunRoofAnimation};
}

TEST(AnimationModuleTests, EmptyVhalSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    std::vector<AnimationParam> result =
            animationModule.getUpdatedAnimationParams(std::vector<VehiclePropValue>());
    EXPECT_EQ(result.size(), 0);
}

TEST(AnimationModuleTests, LeftDoorAnimationOnceSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
            std::vector<VehiclePropValue>{VehiclePropValue{
                    .areaId = (int32_t)VehicleArea::DOOR,
                    .prop = 0x0200 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                            VehicleArea::DOOR,
                    .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
            }});
    EXPECT_EQ(result.size(), 1);
}

TEST(AnimationModuleTests, LeftDoorAnimationTenTimesSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    for (int i = 0; i < 10; ++i) {
        std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
                std::vector<VehiclePropValue>{VehiclePropValue{
                        .areaId = (int32_t)VehicleArea::DOOR,
                        .prop = 0x0200 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                                VehicleArea::DOOR,
                        .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
                }});
        EXPECT_EQ(result.size(), 1);
    }
}

TEST(AnimationModuleTests, RightDoorAnimationOnceSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
            std::vector<VehiclePropValue>{VehiclePropValue{
                    .areaId = (int32_t)VehicleArea::DOOR,
                    .prop = 0x0201 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                            VehicleArea::DOOR,
                    .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
            }});
    EXPECT_EQ(result.size(), 1);
}

TEST(AnimationModuleTests, RightDoorAnimationTenTimesSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    for (int i = 0; i < 10; ++i) {
        std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
                std::vector<VehiclePropValue>{VehiclePropValue{
                        .areaId = (int32_t)VehicleArea::DOOR,
                        .prop = 0x0201 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                                VehicleArea::DOOR,
                        .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
                }});
        EXPECT_EQ(result.size(), 1);
    }
}

TEST(AnimationModuleTests, LeftBlinkerAnimationOnceSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
            std::vector<VehiclePropValue>{VehiclePropValue{
                    .areaId = (int32_t)VehicleArea::GLOBAL,
                    .prop = 0x0300 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                            VehicleArea::GLOBAL,
                    .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
            }});
    EXPECT_EQ(result.size(), 1);
}

TEST(AnimationModuleTests, LeftBlinkerAnimationTenTimesSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    for (int i = 0; i < 10; ++i) {
        std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
                std::vector<VehiclePropValue>{VehiclePropValue{
                        .areaId = (int32_t)VehicleArea::GLOBAL,
                        .prop = 0x0300 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                                VehicleArea::GLOBAL,
                        .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
                }});
        EXPECT_EQ(result.size(), 1);
    }
}

TEST(AnimationModuleTests, RightBlinkerAnimationOnceSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
            std::vector<VehiclePropValue>{VehiclePropValue{
                    .areaId = (int32_t)VehicleArea::GLOBAL,
                    .prop = 0x0301 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                            VehicleArea::GLOBAL,
                    .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
            }});
    EXPECT_EQ(result.size(), 1);
}

TEST(AnimationModuleTests, RightBlinkerAnimationTenTimesSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    for (int i = 0; i < 10; ++i) {
        std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
                std::vector<VehiclePropValue>{VehiclePropValue{
                        .areaId = (int32_t)VehicleArea::GLOBAL,
                        .prop = 0x0301 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                                VehicleArea::GLOBAL,
                        .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
                }});
        EXPECT_EQ(result.size(), 1);
    }
}

TEST(AnimationModuleTests, SunRoofAnimationOnceSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
            std::vector<VehiclePropValue>{VehiclePropValue{
                    .areaId = (int32_t)VehicleArea::GLOBAL,
                    .prop = 0x0400 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                            VehicleArea::GLOBAL,
                    .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
            }});
    EXPECT_EQ(result.size(), 1);
}

TEST(AnimationModuleTests, SunRoofAnimationTenTimesSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    for (int i = 0; i < 10; ++i) {
        std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
                std::vector<VehiclePropValue>{VehiclePropValue{
                        .areaId = (int32_t)VehicleArea::GLOBAL,
                        .prop = 0x0400 | VehiclePropertyGroup::SYSTEM | VehiclePropertyType::INT32 |
                                VehicleArea::GLOBAL,
                        .value.int32Values = std::vector<int32_t>(1, INT32_MAX),
                }});
        EXPECT_EQ(result.size(), 1);
    }
}

TEST(AnimationModuleTests, All5PartsAnimationOnceSuccess) {
    AnimationModule animationModule(getSampleCarPartsMap(), std::map<std::string, CarTexture>(),
                                    getSampleAnimations());
    std::vector<AnimationParam> result = animationModule.getUpdatedAnimationParams(
            std::vector<VehiclePropValue>{VehiclePropValue{
                                                  .areaId = (int32_t)VehicleArea::DOOR,
                                                  .prop = 0x0200 | VehiclePropertyGroup::SYSTEM |
                                                          VehiclePropertyType::INT32 |
                                                          VehicleArea::DOOR,
                                                  .value.int32Values =
                                                          std::vector<int32_t>(1, INT32_MAX),
                                          },
                                          VehiclePropValue{
                                                  .areaId = (int32_t)VehicleArea::DOOR,
                                                  .prop = 0x0201 | VehiclePropertyGroup::SYSTEM |
                                                          VehiclePropertyType::INT32 |
                                                          VehicleArea::DOOR,
                                                  .value.int32Values =
                                                          std::vector<int32_t>(1, INT32_MAX),
                                          },
                                          VehiclePropValue{
                                                  .areaId = (int32_t)VehicleArea::GLOBAL,
                                                  .prop = 0x0300 | VehiclePropertyGroup::SYSTEM |
                                                          VehiclePropertyType::INT32 |
                                                          VehicleArea::GLOBAL,
                                                  .value.int32Values =
                                                          std::vector<int32_t>(1, INT32_MAX),
                                          },
                                          VehiclePropValue{
                                                  .areaId = (int32_t)VehicleArea::GLOBAL,
                                                  .prop = 0x0301 | VehiclePropertyGroup::SYSTEM |
                                                          VehiclePropertyType::INT32 |
                                                          VehicleArea::GLOBAL,
                                                  .value.int32Values =
                                                          std::vector<int32_t>(1, INT32_MAX),
                                          },
                                          VehiclePropValue{
                                                  .areaId = (int32_t)VehicleArea::GLOBAL,
                                                  .prop = 0x0400 | VehiclePropertyGroup::SYSTEM |
                                                          VehiclePropertyType::INT32 |
                                                          VehicleArea::GLOBAL,
                                                  .value.int32Values =
                                                          std::vector<int32_t>(1, INT32_MAX),
                                          }});
    EXPECT_EQ(result.size(), 5);
}

}  // namespace
}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
