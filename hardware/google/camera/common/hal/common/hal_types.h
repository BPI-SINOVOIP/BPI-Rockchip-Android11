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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_COMMON_HAL_TYPES_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_COMMON_HAL_TYPES_H_

#include <cutils/native_handle.h>
#include <system/graphics-base-v1.0.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "hal_camera_metadata.h"

namespace android {
namespace google_camera_hal {

using ::android::status_t;

// Used to identify an invalid buffer handle.
static constexpr buffer_handle_t kInvalidBufferHandle = nullptr;

// See the definition of
// ::android::hardware::camera::common::V1_0::TorchMode
enum class TorchMode : uint32_t {
  kOff = 0,
  kOn = 1,
};

// See the definition of
// ::hardware::camera::common::V1_0::CameraDeviceStatus
enum class CameraDeviceStatus : uint32_t {
  kNotPresent = 0,
  kPresent,
  kEnumerating,
};

// See the definition of
// ::hardware::camera::common::V1_0::TorchModeStatus
enum class TorchModeStatus : uint32_t {
  kNotAvailable = 0,
  kAvailableOff,
  kAvailableOn,
};

// See the definition of
// ::android::hardware::camera::common::V1_0::CameraResourceCost
struct CameraResourceCost {
  uint32_t resource_cost = 0;
  std::vector<uint32_t> conflicting_devices;
};

// See the definition of
// ::android::hardware::camera::common::V1_0::CameraMetadataType
enum class CameraMetadataType : uint32_t {
  kByte = 0,
  kInt32,
  kFloat,
  kInt64,
  kDouble,
  kRational,
};

// See the definition of
// ::android::hardware::camera::common::V1_0::VendorTag
struct VendorTag {
  uint32_t tag_id = 0;
  std::string tag_name;
  CameraMetadataType tag_type = CameraMetadataType::kByte;
};

// See the definition of
// ::android::hardware::camera::common::V1_0::VendorTagSection
struct VendorTagSection {
  std::string section_name;
  std::vector<VendorTag> tags;
};

// See the definition of
// ::android::hardware::camera::device::V3_2::StreamType;
enum class StreamType : uint32_t {
  kOutput = 0,
  kInput,
};

// See the definition of
// ::android::hardware::camera::device::V3_2::StreamRotation;
enum class StreamRotation : uint32_t {
  kRotation0 = 0,
  kRotation90,
  kRotation180,
  kRotation270,
};

// See the definition of
// ::android::hardware::camera::device::V3_4::Stream;
struct Stream {
  int32_t id = -1;
  StreamType stream_type = StreamType::kOutput;
  uint32_t width = 0;
  uint32_t height = 0;
  android_pixel_format_t format = HAL_PIXEL_FORMAT_RGBA_8888;
  uint64_t usage = 0;
  android_dataspace_t data_space = HAL_DATASPACE_UNKNOWN;
  StreamRotation rotation = StreamRotation::kRotation0;
  bool is_physical_camera_stream = false;
  uint32_t physical_camera_id = 0;
  uint32_t buffer_size = 0;
};

// See the definition of
// ::android::hardware::camera::device::V3_2::StreamConfigurationMode;
enum class StreamConfigurationMode : uint32_t {
  kNormal = 0,
  kConstrainedHighSpeed,
};

// See the definition of
// ::android::hardware::camera::device::V3_5::StreamConfiguration;
struct StreamConfiguration {
  std::vector<Stream> streams;
  StreamConfigurationMode operation_mode;
  std::unique_ptr<HalCameraMetadata> session_params;
  uint32_t stream_config_counter = 0;
};

struct CameraIdAndStreamConfiguration {
  uint32_t camera_id = 0;
  StreamConfiguration stream_configuration;
};

// See the definition of
// ::android::hardware::camera::device::V3_4::HalStream
struct HalStream {
  int32_t id = -1;
  android_pixel_format_t override_format = HAL_PIXEL_FORMAT_RGBA_8888;
  uint64_t producer_usage = 0;
  uint64_t consumer_usage = 0;
  uint32_t max_buffers = 0;
  android_dataspace_t override_data_space = HAL_DATASPACE_UNKNOWN;
  bool is_physical_camera_stream = false;
  uint32_t physical_camera_id = 0;
};

// See the definition of
// ::android::hardware::camera::device::V3_2::BufferCache
struct BufferCache {
  int32_t stream_id = -1;
  uint64_t buffer_id = 0;

  bool operator==(const BufferCache& other) const {
    return stream_id == other.stream_id && buffer_id == other.buffer_id;
  }
};

// See the definition of
// ::android::hardware::camera::device::V3_2::BufferStatus
enum class BufferStatus : uint32_t {
  kOk = 0,
  kError,
};

// See the definition of
// ::android::hardware::camera::device::V3_2::StreamBuffer
struct StreamBuffer {
  int32_t stream_id = -1;
  uint64_t buffer_id = 0;
  buffer_handle_t buffer = nullptr;
  BufferStatus status = BufferStatus::kOk;

  // The fences are owned by the caller. If they will be used after a call
  // returns, the callee should duplicate it.
  const native_handle_t* acquire_fence = nullptr;
  const native_handle_t* release_fence = nullptr;
};

// See the definition of
// ::android::hardware::camera::device::V3_4::CaptureRequest
struct CaptureRequest {
  uint32_t frame_number = 0;
  std::unique_ptr<HalCameraMetadata> settings;

  // If empty, the output buffers are captured from the camera sensors. If
  // not empty, the output buffers are captured from the input buffers.
  std::vector<StreamBuffer> input_buffers;

  // The metadata of the input_buffers. This is used for multi-frame merging
  // like HDR+. The input_buffer_metadata at entry k must be for the input
  // buffer at entry k in the input_buffers.
  std::vector<std::unique_ptr<HalCameraMetadata>> input_buffer_metadata;

  std::vector<StreamBuffer> output_buffers;

  // Maps from physical camera ID to physical camera settings.
  std::unordered_map<uint32_t, std::unique_ptr<HalCameraMetadata>>
      physical_camera_settings;
};

// See the definition of
// ::android::hardware::camera::device::V3_2::RequestTemplate
enum class RequestTemplate : uint32_t {
  kPreview = 1,
  kStillCapture = 2,
  kVideoRecord = 3,
  kVideoSnapshot = 4,
  kZeroShutterLag = 5,
  kManual = 6,
  kVendorTemplateStart = 0x40000000,
};

// See the definition of
// ::android::hardware::camera::device::V3_2::MsgType
enum class MessageType : uint32_t {
  kError = 1,
  kShutter = 2,
};

// See the definition of
// ::android::hardware::camera::device::V3_2::ErrorCode
enum class ErrorCode : uint32_t {
  kErrorDevice = 1,
  kErrorRequest = 2,
  kErrorResult = 3,
  kErrorBuffer = 4,
};

// See the definition of
// ::android::hardware::camera::device::V3_2::ErrorMsg
struct ErrorMessage {
  uint32_t frame_number = 0;
  int32_t error_stream_id = -1;
  ErrorCode error_code = ErrorCode::kErrorDevice;
};

// See the definition of
// ::android::hardware::camera::device::V3_2::ShutterMsg
struct ShutterMessage {
  uint32_t frame_number = 0;
  uint64_t timestamp_ns = 0;
};

// See the definition of
// ::android::hardware::camera::device::V3_2::NotifyMsg
struct NotifyMessage {
  MessageType type = MessageType::kError;

  union Message {
    ErrorMessage error;
    ShutterMessage shutter;
  } message;
};

// See the definition of
// ::android::hardware::camera::device::V3_4::PhysicalCameraMetadata
struct PhysicalCameraMetadata {
  uint32_t physical_camera_id = 0;
  std::unique_ptr<HalCameraMetadata> metadata;
};

// See the definition of
// ::android::hardware::camera::device::V3_4::CaptureResult
struct CaptureResult {
  uint32_t frame_number = 0;
  std::unique_ptr<HalCameraMetadata> result_metadata;
  std::vector<StreamBuffer> output_buffers;
  std::vector<StreamBuffer> input_buffers;
  uint32_t partial_result = 0;
  std::vector<PhysicalCameraMetadata> physical_metadata;
};

struct Rect {
  uint32_t left;
  uint32_t top;
  uint32_t right;
  uint32_t bottom;
};

struct WeightedRect : Rect {
  int32_t weight;
};

struct Dimension {
  uint32_t width;
  uint32_t height;
};

struct Point {
  uint32_t x;
  uint32_t y;
};

struct PointI {
  int32_t x;
  int32_t y;
};

struct PointF {
  float x = 0.0f;
  float y = 0.0f;
};

// Hash function for std::pair
struct PairHash {
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2>& p) const {
    return std::hash<T1>{}(p.first) ^ std::hash<T2>{}(p.second);
  }
};

// See the definition of
// ::android::hardware::camera::device::V3_5::BufferRequestStatus;
enum class BufferRequestStatus : uint32_t {
  kOk = 0,
  kFailedPartial = 1,
  kFailedConfiguring = 2,
  kFailedIllegalArgs = 3,
  kFailedUnknown = 4,
};

// See the definition of
// ::android::hardware::camera::device::V3_5::StreamBufferRequestError
enum class StreamBufferRequestError : uint32_t {
  kOk = 0,
  kNoBufferAvailable = 1,
  kMaxBufferExceeded = 2,
  kStreamDisconnected = 3,
  kUnknownError = 4,
};

// See the definition of
// ::android::hardware::camera::device::V3_5::BufferRequest
struct BufferRequest {
  int32_t stream_id = -1;
  uint32_t num_buffers_requested = 0;
};

// See the definition of
// ::android::hardware::camera::device::V3_5::StreamBuffersVal
struct BuffersValue {
  StreamBufferRequestError error = StreamBufferRequestError::kUnknownError;
  std::vector<StreamBuffer> buffers;
};

// See the definition of
// ::android::hardware::camera::device::V3_5::StreamBufferRet
struct BufferReturn {
  int32_t stream_id = -1;
  BuffersValue val;
};

// Callback function invoked to process capture results.
using ProcessCaptureResultFunc =
    std::function<void(std::unique_ptr<CaptureResult> /*result*/)>;

// Callback function invoked to notify messages.
using NotifyFunc = std::function<void(const NotifyMessage& /*message*/)>;

// HAL buffer allocation descriptor
struct HalBufferDescriptor {
  int32_t stream_id = -1;
  uint32_t width = 0;
  uint32_t height = 0;
  android_pixel_format_t format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
  uint64_t producer_flags = 0;
  uint64_t consumer_flags = 0;
  uint32_t immediate_num_buffers = 0;
  uint32_t max_num_buffers = 0;
  uint64_t allocator_id_ = 0;
};

// Callback function invoked to request stream buffers.
using RequestStreamBuffersFunc = std::function<BufferRequestStatus(
    const std::vector<BufferRequest>& /*buffer_requests*/,
    std::vector<BufferReturn>* /*buffer_returns*/)>;

// Callback function invoked to return stream buffers.
using ReturnStreamBuffersFunc =
    std::function<void(const std::vector<StreamBuffer>& /*buffers*/)>;

struct ZoomRatioRange {
  float min = 1.0f;
  float max = 1.0f;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_COMMON_HAL_TYPES_H_
