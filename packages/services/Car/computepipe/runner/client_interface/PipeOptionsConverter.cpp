// Copyright (C) 2019 The Android Open Source Project
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

#include "PipeOptionsConverter.h"

#include "aidl/android/automotive/computepipe/runner/PipeInputConfigInputType.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace client_interface {
namespace aidl_client {

using ::aidl::android::automotive::computepipe::runner::PipeDescriptor;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfig;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigCameraDesc;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigCameraType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigFormatType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigImageFileType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigInputSourceDesc;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigInputType;
using ::aidl::android::automotive::computepipe::runner::PipeInputConfigVideoFileType;
using ::aidl::android::automotive::computepipe::runner::PipeOffloadConfig;
using ::aidl::android::automotive::computepipe::runner::PipeOffloadConfigOffloadType;
using ::aidl::android::automotive::computepipe::runner::PipeOutputConfig;
using ::aidl::android::automotive::computepipe::runner::PipeOutputConfigPacketType;
using ::aidl::android::automotive::computepipe::runner::PipeTerminationConfig;
using ::aidl::android::automotive::computepipe::runner::PipeTerminationConfigTerminationType;

namespace {

PipeInputConfigInputType ConvertInputType(proto::InputStreamConfig::InputType type) {
    switch (type) {
        case proto::InputStreamConfig::CAMERA:
            return PipeInputConfigInputType::CAMERA;
        case proto::InputStreamConfig::VIDEO_FILE:
            return PipeInputConfigInputType::VIDEO_FILE;
        case proto::InputStreamConfig::IMAGE_FILES:
            return PipeInputConfigInputType::IMAGE_FILES;
    }
}

PipeInputConfigCameraType ConvertCameraType(proto::CameraConfig::CameraType type) {
    switch (type) {
        case proto::CameraConfig::DRIVER_VIEW_CAMERA:
            return PipeInputConfigCameraType::DRIVER_VIEW_CAMERA;
        case proto::CameraConfig::OCCUPANT_VIEW_CAMERA:
            return PipeInputConfigCameraType::OCCUPANT_VIEW_CAMERA;
        case proto::CameraConfig::EXTERNAL_CAMERA:
            return PipeInputConfigCameraType::EXTERNAL_CAMERA;
        case proto::CameraConfig::SURROUND_VIEW_CAMERA:
            return PipeInputConfigCameraType::SURROUND_VIEW_CAMERA;
    }
}

PipeInputConfigImageFileType ConvertImageFileType(proto::ImageFileConfig::ImageFileType type) {
    switch (type) {
        case proto::ImageFileConfig::JPEG:
            return PipeInputConfigImageFileType::JPEG;
        case proto::ImageFileConfig::PNG:
            return PipeInputConfigImageFileType::PNG;
    }
}

PipeInputConfigVideoFileType ConvertVideoFileType(proto::VideoFileConfig::VideoFileType type) {
    switch (type) {
        case proto::VideoFileConfig::MPEG:
            return PipeInputConfigVideoFileType::MPEG;
    }
}

PipeInputConfigFormatType ConvertInputFormat(proto::InputStreamConfig::FormatType type) {
    switch (type) {
        case proto::InputStreamConfig::RGB:
            return PipeInputConfigFormatType::RGB;
        case proto::InputStreamConfig::NIR:
            return PipeInputConfigFormatType::NIR;
        case proto::InputStreamConfig::NIR_DEPTH:
            return PipeInputConfigFormatType::NIR_DEPTH;
    }
}

PipeOffloadConfigOffloadType ConvertOffloadType(proto::OffloadOption::OffloadType type) {
    switch (type) {
        case proto::OffloadOption::CPU:
            return PipeOffloadConfigOffloadType::CPU;
        case proto::OffloadOption::GPU:
            return PipeOffloadConfigOffloadType::GPU;
        case proto::OffloadOption::NEURAL_ENGINE:
            return PipeOffloadConfigOffloadType::NEURAL_ENGINE;
        case proto::OffloadOption::CV_ENGINE:
            return PipeOffloadConfigOffloadType::CV_ENGINE;
    }
}

PipeOutputConfigPacketType ConvertOutputType(proto::PacketType type) {
    switch (type) {
        case proto::PacketType::SEMANTIC_DATA:
            return PipeOutputConfigPacketType::SEMANTIC_DATA;
        case proto::PacketType::PIXEL_DATA:
            return PipeOutputConfigPacketType::PIXEL_DATA;
        case proto::PacketType::PIXEL_ZERO_COPY_DATA:
            return PipeOutputConfigPacketType::PIXEL_ZERO_COPY_DATA;
    }
}

PipeTerminationConfigTerminationType ConvertTerminationType(
    proto::TerminationOption::TerminationType type) {
    switch (type) {
        case proto::TerminationOption::CLIENT_STOP:
            return PipeTerminationConfigTerminationType::CLIENT_STOP;
        case proto::TerminationOption::MIN_PACKET_COUNT:
            return PipeTerminationConfigTerminationType::MIN_PACKET_COUNT;
        case proto::TerminationOption::MAX_RUN_TIME:
            return PipeTerminationConfigTerminationType::MAX_RUN_TIME;
        case proto::TerminationOption::EVENT:
            return PipeTerminationConfigTerminationType::EVENT;
    }
}

PipeInputConfig ConvertInputConfigProto(const proto::InputConfig& proto) {
    PipeInputConfig aidlConfig;

    for (const auto& inputStreamConfig : proto.input_stream()) {
        PipeInputConfigInputSourceDesc aidlInputDesc;
        aidlInputDesc.type = ConvertInputType(inputStreamConfig.type());
        aidlInputDesc.format = ConvertInputFormat(inputStreamConfig.format());
        aidlInputDesc.width = inputStreamConfig.width();
        aidlInputDesc.height = inputStreamConfig.height();
        aidlInputDesc.stride = inputStreamConfig.stride();
        aidlInputDesc.camDesc.camId = inputStreamConfig.cam_config().cam_id();
        aidlInputDesc.camDesc.type =
                ConvertCameraType(inputStreamConfig.cam_config().camera_type());
        aidlInputDesc.imageDesc.fileType =
            ConvertImageFileType(inputStreamConfig.image_config().file_type());
        aidlInputDesc.imageDesc.filePath = inputStreamConfig.image_config().image_dir();
        aidlInputDesc.videoDesc.fileType =
            ConvertVideoFileType(inputStreamConfig.video_config().file_type());
        aidlInputDesc.videoDesc.filePath = inputStreamConfig.video_config().file_path();
        aidlConfig.inputSources.emplace_back(aidlInputDesc);
    }
    aidlConfig.configId = proto.config_id();

    return aidlConfig;
}

PipeOffloadConfig ConvertOffloadConfigProto(const proto::OffloadConfig& proto) {
    PipeOffloadConfig aidlConfig;

    for (int i = 0; i < proto.options().offload_types_size(); i++) {
        auto offloadType =
            static_cast<proto::OffloadOption_OffloadType>(proto.options().offload_types()[i]);
        PipeOffloadConfigOffloadType aidlType = ConvertOffloadType(offloadType);
        aidlConfig.desc.type.emplace_back(aidlType);
        aidlConfig.desc.isVirtual.emplace_back(proto.options().is_virtual()[i]);
    }

    aidlConfig.configId = proto.config_id();
    return aidlConfig;
}

PipeOutputConfig ConvertOutputConfigProto(const proto::OutputConfig& proto) {
    PipeOutputConfig aidlConfig;
    aidlConfig.output.name = proto.stream_name();
    aidlConfig.output.type = ConvertOutputType(proto.type());
    aidlConfig.outputId = proto.stream_id();
    return aidlConfig;
}

PipeTerminationConfig ConvertTerminationConfigProto(const proto::TerminationConfig& proto) {
    PipeTerminationConfig aidlConfig;
    aidlConfig.desc.type = ConvertTerminationType(proto.options().type());
    aidlConfig.desc.qualifier = proto.options().qualifier();
    aidlConfig.configId = proto.config_id();
    return aidlConfig;
}

}  // namespace

PipeDescriptor OptionsToPipeDescriptor(const proto::Options& options) {
    PipeDescriptor desc;
    for (int i = 0; i < options.input_configs_size(); i++) {
        PipeInputConfig inputConfig = ConvertInputConfigProto(options.input_configs()[i]);
        desc.inputConfig.emplace_back(inputConfig);
    }

    for (int i = 0; i < options.offload_configs_size(); i++) {
        PipeOffloadConfig offloadConfig = ConvertOffloadConfigProto(options.offload_configs()[i]);
        desc.offloadConfig.emplace_back(offloadConfig);
    }

    for (int i = 0; i < options.termination_configs_size(); i++) {
        PipeTerminationConfig terminationConfig =
            ConvertTerminationConfigProto(options.termination_configs()[i]);
        desc.terminationConfig.emplace_back(terminationConfig);
    }

    for (int i = 0; i < options.output_configs_size(); i++) {
        PipeOutputConfig outputConfig = ConvertOutputConfigProto(options.output_configs()[i]);
        desc.outputConfig.emplace_back(outputConfig);
    }
    return desc;
}

}  // namespace aidl_client
}  // namespace client_interface
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android
