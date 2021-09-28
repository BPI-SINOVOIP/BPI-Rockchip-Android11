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

#define LOG_TAG "IoModuleTests"

#include "CarModelConfigReader.h"

#include "MathHelp.h"
#include "core_lib.h"

#include <gtest/gtest.h>
#include <string>

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {
namespace {

TEST(CarModelConfigReaderTests, CarModelReadConfigSuccess) {
    AnimationConfig animationConfig;
    EXPECT_EQ(ReadCarModelConfig("/vendor/etc/automotive/sv/sv_sample_car_model_config.xml",
                                 &animationConfig),
              IOStatus::OK);

    EXPECT_EQ(animationConfig.version, "1.0");

    ASSERT_EQ(animationConfig.animations.size(), 2);

    {
        AnimationInfo doorAnimation = animationConfig.animations.at(0);
        EXPECT_EQ(doorAnimation.partId, "door");
        EXPECT_EQ(doorAnimation.childIds.size(), 1);
        EXPECT_EQ(doorAnimation.pose, gMat4Identity);

        EXPECT_EQ(doorAnimation.rotationOpsMap.size(), 1);
        {
            RotationOp rotationOp = (doorAnimation.rotationOpsMap.at(0x16000B0000000001)).at(0);
            EXPECT_EQ(rotationOp.vhalProperty, 0x16000B0000000001);
            EXPECT_EQ(rotationOp.type, AnimationType::ROTATION_ANGLE);
            EXPECT_EQ(rotationOp.animationTime, 2000);
            std::array<float, 3> axis = {0, 0, 1};
            EXPECT_EQ(rotationOp.axis.axisVector, axis);
            std::array<float, 3> point = {0, 0, 0};
            EXPECT_EQ(rotationOp.axis.rotationPoint, point);
            EXPECT_EQ(rotationOp.rotationRange.start, 0.0);
            EXPECT_EQ(rotationOp.rotationRange.end, 90.0);
            EXPECT_EQ(rotationOp.vhalRange.start, 0);
            EXPECT_EQ(rotationOp.vhalRange.end, 0x7FFFFFFF);
        }
    }

    {
        AnimationInfo windowAnimation = animationConfig.animations.at(1);
        EXPECT_EQ(windowAnimation.partId, "window");
        EXPECT_EQ(windowAnimation.childIds.size(), 0);
        EXPECT_EQ(windowAnimation.pose, gMat4Identity);

        EXPECT_EQ(windowAnimation.translationOpsMap.size(), 1);
        {
            TranslationOp translationOp =
                    (windowAnimation.translationOpsMap.at(0x13000BC000000010)).at(0);
            EXPECT_EQ(translationOp.vhalProperty, 0x13000BC000000010);
            EXPECT_EQ(translationOp.type, AnimationType::TRANSLATION);
            EXPECT_EQ(translationOp.animationTime, 2000);
            std::array<float, 3> dir = {0.0, 0.0, -1.0};
            EXPECT_EQ(translationOp.direction, dir);
            EXPECT_EQ(translationOp.defaultTranslationValue, 0.0);
            EXPECT_EQ(translationOp.translationRange.start, 0.0);
            EXPECT_EQ(translationOp.translationRange.end, 1.0);
            EXPECT_EQ(translationOp.vhalRange.start, 0);
            EXPECT_EQ(translationOp.vhalRange.end, 0x7FFFFFFF);
        }
    }
}

}  // namespace
}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
