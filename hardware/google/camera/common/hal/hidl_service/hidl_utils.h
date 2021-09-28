/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_UTILS_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_UTILS_H_

#include <android/hardware/camera/common/1.0/types.h>
#include <android/hardware/camera/device/3.5/ICameraDeviceSession.h>
#include <fmq/MessageQueue.h>
#include <hal_types.h>
#include <hidl/HidlSupport.h>
#include <memory>

namespace android {
namespace hardware {
namespace camera {
namespace implementation {
namespace hidl_utils {

using ::android::hardware::hidl_vec;
using ::android::hardware::camera::common::V1_0::CameraDeviceStatus;
using ::android::hardware::camera::common::V1_0::CameraMetadataType;
using ::android::hardware::camera::common::V1_0::CameraResourceCost;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::common::V1_0::TorchMode;
using ::android::hardware::camera::common::V1_0::TorchModeStatus;
using ::android::hardware::camera::common::V1_0::VendorTagSection;
using ::android::hardware::camera::device::V3_2::BufferCache;
using ::android::hardware::camera::device::V3_2::BufferStatus;
using ::android::hardware::camera::device::V3_2::CameraMetadata;
using ::android::hardware::camera::device::V3_2::NotifyMsg;
using ::android::hardware::camera::device::V3_2::RequestTemplate;
using ::android::hardware::camera::device::V3_2::StreamBuffer;
using ::android::hardware::camera::device::V3_2::StreamConfigurationMode;
using ::android::hardware::camera::device::V3_2::StreamRotation;
using ::android::hardware::camera::device::V3_2::StreamType;
using ::android::hardware::camera::device::V3_4::CaptureRequest;
using ::android::hardware::camera::device::V3_4::CaptureResult;
using ::android::hardware::camera::device::V3_4::HalStreamConfiguration;
using ::android::hardware::camera::device::V3_4::Stream;
using ::android::hardware::camera::device::V3_5::BufferRequest;
using ::android::hardware::camera::device::V3_5::BufferRequestStatus;
using ::android::hardware::camera::device::V3_5::StreamBufferRequestError;
using ::android::hardware::camera::device::V3_5::StreamBufferRet;
using ::android::hardware::camera::device::V3_5::StreamBuffersVal;
using ::android::hardware::camera::device::V3_5::StreamConfiguration;

// Util functions to convert the types between HIDL and Google Camera HAL.

// Conversions from HAL to HIDL
status_t ConvertToHidlVendorTagSections(
    const std::vector<google_camera_hal::VendorTagSection>& hal_sections,
    hidl_vec<VendorTagSection>* hidl_sections);

status_t ConvertToHidlVendorTagType(
    google_camera_hal::CameraMetadataType hal_type,
    CameraMetadataType* hidl_type);

status_t ConvertToHidlResourceCost(
    const google_camera_hal::CameraResourceCost& hal_cost,
    CameraResourceCost* hidl_cost);

status_t ConvertToHidlHalStreamConfig(
    const std::vector<google_camera_hal::HalStream>& hal_configured_streams,
    HalStreamConfiguration* hidl_hal_stream_config);

// Convert a HAL result to a HIDL result. It will try to write the result
// metadata to result_metadata_queue. If it fails, it will write the result
// metadata in hidl_result.
status_t ConvertToHidlCaptureResult(
    MessageQueue<uint8_t, kSynchronizedReadWrite>* result_metadata_queue,
    std::unique_ptr<google_camera_hal::CaptureResult> hal_result,
    CaptureResult* hidl_result);

status_t ConverToHidlNotifyMessage(
    const google_camera_hal::NotifyMessage& hal_message,
    NotifyMsg* hidl_message);

// Convert from HAL status_t to HIDL Status
// OK is converted to Status::OK.
// BAD_VALUE is converted to Status::ILLEGAL_ARGUMENT.
// -EBUSY is converted to Status::CAMERA_IN_USE.
// -EUSERS is converted to Status::MAX_CAMERAS_IN_USE.
// UNKNOWN_TRANSACTION is converted to Status::METHOD_NOT_SUPPORTED.
// INVALID_OPERATION is converted to Status::OPERATION_NOT_SUPPORTED.
// DEAD_OBJECT is converted to Status::CAMERA_DISCONNECTED.
// All other errors are converted to Status::INTERNAL_ERROR.
Status ConvertToHidlStatus(status_t hal_status);

// Convert from HAL CameraDeviceStatus to HIDL CameraDeviceStatus
// kNotPresent is converted to CameraDeviceStatus::NOT_PRESENT.
// kPresent is converted to CameraDeviceStatus::PRESENT.
// kEnumerating is converted to CameraDeviceStatus::ENUMERATING.
status_t ConvertToHidlCameraDeviceStatus(
    google_camera_hal::CameraDeviceStatus hal_camera_device_status,
    CameraDeviceStatus* hidl_camera_device_status);

// Convert from HAL TorchModeStatus to HIDL TorchModeStatus
// kNotAvailable is converted to TorchModeStatus::NOT_AVAILABLE.
// kAvailableOff is converted to TorchModeStatus::AVAILABLE_OFF.
// kAvailableOn is converted to TorchModeStatus::AVAILABLE_ON.
status_t ConvertToHidlTorchModeStatus(
    google_camera_hal::TorchModeStatus hal_torch_status,
    TorchModeStatus* hidl_torch_status);

// Convert a HAL request to a HIDL request.
status_t ConvertToHidlBufferRequest(
    const std::vector<google_camera_hal::BufferRequest>& hal_buffer_requests,
    hidl_vec<BufferRequest>* hidl_buffer_requests);

// Convert a HAL stream buffer to a HIDL hidl stream buffer.
status_t ConvertToHidlStreamBuffer(
    const google_camera_hal::StreamBuffer& hal_buffer,
    StreamBuffer* hidl_buffer);

// Conversions from HIDL to HAL.
status_t ConvertToHalTemplateType(
    RequestTemplate hidl_template,
    google_camera_hal::RequestTemplate* hal_template);

status_t ConvertToHalStreamBuffer(const StreamBuffer& hidl_buffer,
                                  google_camera_hal::StreamBuffer* hal_buffer);

status_t ConvertToHalMetadata(
    uint32_t message_queue_setting_size,
    MessageQueue<uint8_t, kSynchronizedReadWrite>* request_metadata_queue,
    const CameraMetadata& request_settings,
    std::unique_ptr<google_camera_hal::HalCameraMetadata>* hal_metadata);

status_t ConvertToHalCaptureRequest(
    const CaptureRequest& hidl_request,
    MessageQueue<uint8_t, kSynchronizedReadWrite>* request_metadata_queue,
    google_camera_hal::CaptureRequest* hal_request);

status_t ConvertToHalBufferCaches(
    const hidl_vec<BufferCache>& hidl_buffer_caches,
    std::vector<google_camera_hal::BufferCache>* hal_buffer_caches);

status_t ConverToHalStreamConfig(
    const StreamConfiguration& hidl_stream_config,
    google_camera_hal::StreamConfiguration* hal_stream_config);

status_t ConverToHalStreamConfig(
    const device::V3_4::StreamConfiguration& hidl_stream_config,
    google_camera_hal::StreamConfiguration* hal_stream_config);

status_t ConvertToHalStreamConfigurationMode(
    StreamConfigurationMode hidl_mode,
    google_camera_hal::StreamConfigurationMode* hal_mode);

status_t ConvertToHalBufferStatus(BufferStatus hidl_status,
                                  google_camera_hal::BufferStatus* hal_status);

status_t ConvertToHalStream(const Stream& hidl_stream,
                            google_camera_hal::Stream* hal_stream);

status_t ConvertToHalStreamRotation(
    StreamRotation hidl_stream_rotation,
    google_camera_hal::StreamRotation* hal_stream_rotation);

status_t ConvertToHalStreamType(StreamType hidl_stream_type,
                                google_camera_hal::StreamType* hal_stream_type);

status_t ConvertToHalTorchMode(TorchMode hidl_torch_mode,
                               google_camera_hal::TorchMode* hal_torch_mode);

status_t ConvertToHalBufferRequestStatus(
    const BufferRequestStatus& hidl_buffer_request_status,
    google_camera_hal::BufferRequestStatus* hal_buffer_request_status);

status_t ConvertToHalBufferReturnStatus(
    const StreamBufferRet& hidl_stream_buffer_return,
    google_camera_hal::BufferReturn* hal_buffer_return);
}  // namespace hidl_utils
}  // namespace implementation
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_UTILS_H_