// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string>

#include "InputConfig.pb.h"
#include "Options.pb.h"
#include "gmock/gmock-matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "runner/client_interface/PipeOptionsConverter.h"
#include "types/Status.h"

using ::aidl::android::automotive::computepipe::runner::PipeDescriptor;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigCameraType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigFormatType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigImageFileType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigInputType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigVideoFileType;

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {
namespace {

TEST(OptionsToPipeDescriptorTest, InputTypesConvertAsExpected) {
    proto::Options options;

    // Add a driver view camera type
    options.add_input_configs()->add_input_stream()->set_type(
        proto::InputStreamConfig_InputType_CAMERA);
    options.mutable_input_configs(0)->mutable_input_stream(0)->mutable_cam_config()->set_camera_type(
        proto::CameraConfig_CameraType_DRIVER_VIEW_CAMERA);
    options.mutable_input_configs(0)->set_config_id(0);

    // Add an occupant view camera type
    options.add_input_configs()->add_input_stream()->set_type(
        proto::InputStreamConfig_InputType_CAMERA);
    options.mutable_input_configs(1)->mutable_input_stream(0)->mutable_cam_config()->set_camera_type(
        proto::CameraConfig_CameraType_OCCUPANT_VIEW_CAMERA);
    options.mutable_input_configs(1)->set_config_id(1);

    // Add external camera type
    options.add_input_configs()->add_input_stream()->set_type(
        proto::InputStreamConfig_InputType_CAMERA);
    options.mutable_input_configs(2)->mutable_input_stream(0)->mutable_cam_config()->set_camera_type(
        proto::CameraConfig_CameraType_EXTERNAL_CAMERA);
    options.mutable_input_configs(2)->set_config_id(2);

    // Add surround camera type
    options.add_input_configs()->add_input_stream()->set_type(
        proto::InputStreamConfig_InputType_CAMERA);
    options.mutable_input_configs(3)->mutable_input_stream(0)->mutable_cam_config()->set_camera_type(
        proto::CameraConfig_CameraType_SURROUND_VIEW_CAMERA);
    options.mutable_input_configs(3)->set_config_id(3);

    // Add image file type
    options.add_input_configs()->add_input_stream()->set_type(
        proto::InputStreamConfig_InputType_IMAGE_FILES);
    options.mutable_input_configs(4)->mutable_input_stream(0)->mutable_image_config()->set_file_type(
        proto::ImageFileConfig_ImageFileType_PNG);
    options.mutable_input_configs(4)->set_config_id(4);

    // Add video file type
    options.add_input_configs()->add_input_stream()->set_type(
        proto::InputStreamConfig_InputType_VIDEO_FILE);
    options.mutable_input_configs(5)->mutable_input_stream(0)->mutable_video_config()->set_file_type(
        proto::VideoFileConfig_VideoFileType_MPEG);
    options.mutable_input_configs(5)->set_config_id(5);

    PipeDescriptor desc = OptionsToPipeDescriptor(options);

    ASSERT_EQ(desc.inputConfig.size(), 6);
    ASSERT_EQ(desc.inputConfig[0].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[0].inputSources[0].type, PipeInputConfigInputType::CAMERA);
    EXPECT_EQ(desc.inputConfig[0].inputSources[0].camDesc.type,
              PipeInputConfigCameraType::DRIVER_VIEW_CAMERA);
    EXPECT_EQ(desc.inputConfig[0].configId, 0);

    ASSERT_EQ(desc.inputConfig[1].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[1].inputSources[0].type, PipeInputConfigInputType::CAMERA);
    EXPECT_EQ(desc.inputConfig[1].inputSources[0].camDesc.type,
              PipeInputConfigCameraType::OCCUPANT_VIEW_CAMERA);
    EXPECT_EQ(desc.inputConfig[1].configId, 1);

    ASSERT_EQ(desc.inputConfig[2].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[2].inputSources[0].type, PipeInputConfigInputType::CAMERA);
    EXPECT_EQ(desc.inputConfig[2].inputSources[0].camDesc.type,
              PipeInputConfigCameraType::EXTERNAL_CAMERA);
    EXPECT_EQ(desc.inputConfig[2].configId, 2);

    ASSERT_EQ(desc.inputConfig[3].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[3].inputSources[0].type, PipeInputConfigInputType::CAMERA);
    EXPECT_EQ(desc.inputConfig[3].inputSources[0].camDesc.type,
              PipeInputConfigCameraType::SURROUND_VIEW_CAMERA);
    EXPECT_EQ(desc.inputConfig[3].configId, 3);

    ASSERT_EQ(desc.inputConfig[4].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[4].inputSources[0].type, PipeInputConfigInputType::IMAGE_FILES);
    EXPECT_EQ(desc.inputConfig[4].inputSources[0].imageDesc.fileType,
              PipeInputConfigImageFileType::PNG);
    EXPECT_EQ(desc.inputConfig[4].configId, 4);

    ASSERT_EQ(desc.inputConfig[5].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[5].inputSources[0].type, PipeInputConfigInputType::VIDEO_FILE);
    EXPECT_EQ(desc.inputConfig[5].inputSources[0].videoDesc.fileType,
              PipeInputConfigVideoFileType::MPEG);
    EXPECT_EQ(desc.inputConfig[5].configId, 5);
}

TEST(OptionsToPipeDescriptorTest, FormatTypesConvertAsExpected) {
    proto::Options options;

    // Add an RGB format
    options.add_input_configs()->add_input_stream()->set_format(
        proto::InputStreamConfig_FormatType_RGB);

    options.add_input_configs()->add_input_stream()->set_format(
        proto::InputStreamConfig_FormatType_NIR);

    options.add_input_configs()->add_input_stream()->set_format(
        proto::InputStreamConfig_FormatType_NIR_DEPTH);

    PipeDescriptor desc = OptionsToPipeDescriptor(options);

    ASSERT_EQ(desc.inputConfig.size(), 3);
    ASSERT_EQ(desc.inputConfig[0].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[0].inputSources[0].format, PipeInputConfigFormatType::RGB);

    ASSERT_EQ(desc.inputConfig[1].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[1].inputSources[0].format, PipeInputConfigFormatType::NIR);

    ASSERT_EQ(desc.inputConfig[2].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[2].inputSources[0].format, PipeInputConfigFormatType::NIR_DEPTH);
}

TEST(OptionsToPipeDescriptorTest, ImageDimensionsAreTranslatedCorrectly) {
    proto::Options options;

    options.add_input_configs()->add_input_stream()->set_width(640);
    options.mutable_input_configs(0)->mutable_input_stream(0)->set_height(480);
    options.mutable_input_configs(0)->mutable_input_stream(0)->set_stride(640 * 3);
    options.mutable_input_configs(0)->mutable_input_stream(0)->set_stride(640 * 3);

    PipeDescriptor desc = OptionsToPipeDescriptor(options);
    ASSERT_EQ(desc.inputConfig.size(), 1);
    ASSERT_EQ(desc.inputConfig[0].inputSources.size(), 1);
    ASSERT_EQ(desc.inputConfig[0].inputSources[0].width, 640);
    ASSERT_EQ(desc.inputConfig[0].inputSources[0].height, 480);
    ASSERT_EQ(desc.inputConfig[0].inputSources[0].stride, 640 * 3);
}

TEST(OptionsToPipeDescriptorTest, CameraIdIsReflectedCorrectly) {
    proto::Options options;
    std::string expectedCameraName = "Camera 1";
    options.add_input_configs()->add_input_stream()->mutable_cam_config()->set_cam_id(
        expectedCameraName);

    PipeDescriptor desc = OptionsToPipeDescriptor(options);
    ASSERT_EQ(desc.inputConfig.size(), 1);
    ASSERT_EQ(desc.inputConfig[0].inputSources.size(), 1);
    EXPECT_EQ(desc.inputConfig[0].inputSources[0].camDesc.camId, expectedCameraName);
}

}  // namespace
}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
