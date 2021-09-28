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

#define LOG_TAG "ConfigReaderTests"

#include "ConfigReader.h"

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

using android_auto::surround_view::SurroundView2dParams;
using android_auto::surround_view::SurroundView3dParams;

TEST(ConfigReaderTests, ReadConfigSuccess) {
    SurroundViewConfig svConfig;
    EXPECT_EQ(ReadSurroundViewConfig("/vendor/etc/automotive/sv/sv_sample_config.xml", &svConfig),
              IOStatus::OK);

    EXPECT_EQ(svConfig.version, "1.0");

    // Camera config
    EXPECT_EQ(svConfig.cameraConfig.evsGroupId, "v4l2loopback_group0");

    // Camera Ids
    EXPECT_EQ(svConfig.cameraConfig.evsCameraIds[0], "/dev/video60");
    EXPECT_EQ(svConfig.cameraConfig.evsCameraIds[1], "/dev/video61");
    EXPECT_EQ(svConfig.cameraConfig.evsCameraIds[2], "/dev/video62");
    EXPECT_EQ(svConfig.cameraConfig.evsCameraIds[3], "/dev/video63");

    // Masks
    EXPECT_EQ(svConfig.cameraConfig.maskFilenames.size(), 4);
    EXPECT_EQ(svConfig.cameraConfig.maskFilenames[0], "/vendor/etc/automotive/sv/mask_front.png");
    EXPECT_EQ(svConfig.cameraConfig.maskFilenames[1], "/vendor/etc/automotive/sv/mask_right.png");
    EXPECT_EQ(svConfig.cameraConfig.maskFilenames[2], "/vendor/etc/automotive/sv/mask_rear.png");
    EXPECT_EQ(svConfig.cameraConfig.maskFilenames[3], "/vendor/etc/automotive/sv/mask_left.png");

    // Surround view 2D
    EXPECT_EQ(svConfig.sv2dConfig.sv2dEnabled, true);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.resolution.width, 768);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.resolution.height, 1024);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.physical_size.width, 9.0);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.physical_size.height, 12.0);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.physical_center.x, 0.0);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.physical_center.y, 0.0);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.gpu_acceleration_enabled, false);
    EXPECT_EQ(svConfig.sv2dConfig.carBoundingBox.width, 2.0);
    EXPECT_EQ(svConfig.sv2dConfig.carBoundingBox.height, 3.0);
    EXPECT_EQ(svConfig.sv2dConfig.carBoundingBox.x, 1.0);
    EXPECT_EQ(svConfig.sv2dConfig.carBoundingBox.y, 1.5);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.high_quality_blending,
              SurroundView2dParams::BlendingType::MULTIBAND);
    EXPECT_EQ(svConfig.sv2dConfig.sv2dParams.low_quality_blending,
              SurroundView2dParams::BlendingType::ALPHA);

    // Surround view 3D
    EXPECT_EQ(svConfig.sv3dConfig.sv3dEnabled, true);
    EXPECT_NE(svConfig.sv3dConfig.carModelConfigFile, "");
    EXPECT_NE(svConfig.sv3dConfig.carModelObjFile, "");
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.plane_radius, 8.0);
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.plane_divisions, 50);
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.curve_height, 6.0);
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.curve_divisions, 50);
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.angular_divisions, 90);
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.curve_coefficient, 3.0);
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.high_details_shadows, true);
    EXPECT_EQ(svConfig.sv3dConfig.sv3dParams.high_details_reflections, true);
}

}  // namespace
}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
