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

#define LOG_TAG "GCH_HidlUtils"
//#define LOG_NDEBUG 0
#include <log/log.h>
#include <regex>

#include "hidl_camera_device.h"
#include "hidl_camera_provider.h"
#include "hidl_utils.h"

namespace android {
namespace hardware {
namespace camera {
namespace implementation {
namespace hidl_utils {

using ::android::hardware::camera::device::V3_2::ErrorCode;
using ::android::hardware::camera::device::V3_2::ErrorMsg;
using ::android::hardware::camera::device::V3_2::MsgType;
using ::android::hardware::camera::device::V3_2::ShutterMsg;
using ::android::hardware::camera::device::V3_5::implementation::HidlCameraDevice;
using ::android::hardware::camera::provider::V2_6::implementation::HidlCameraProvider;

status_t ConvertToHidlVendorTagType(
    google_camera_hal::CameraMetadataType hal_type,
    CameraMetadataType* hidl_type) {
  if (hidl_type == nullptr) {
    ALOGE("%s: hidl_type is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hal_type) {
    case google_camera_hal::CameraMetadataType::kByte:
      *hidl_type = CameraMetadataType::BYTE;
      break;
    case google_camera_hal::CameraMetadataType::kInt32:
      *hidl_type = CameraMetadataType::INT32;
      break;
    case google_camera_hal::CameraMetadataType::kFloat:
      *hidl_type = CameraMetadataType::FLOAT;
      break;
    case google_camera_hal::CameraMetadataType::kInt64:
      *hidl_type = CameraMetadataType::INT64;
      break;
    case google_camera_hal::CameraMetadataType::kDouble:
      *hidl_type = CameraMetadataType::DOUBLE;
      break;
    case google_camera_hal::CameraMetadataType::kRational:
      *hidl_type = CameraMetadataType::RATIONAL;
      break;
    default:
      ALOGE("%s: Unknown google_camera_hal::CameraMetadataType: %u",
            __FUNCTION__, hal_type);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHidlResourceCost(
    const google_camera_hal::CameraResourceCost& hal_cost,
    CameraResourceCost* hidl_cost) {
  if (hidl_cost == nullptr) {
    ALOGE("%s: hidl_cost is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_cost->resourceCost = hal_cost.resource_cost;
  hidl_cost->conflictingDevices.resize(hal_cost.conflicting_devices.size());

  for (uint32_t i = 0; i < hal_cost.conflicting_devices.size(); i++) {
    hidl_cost->conflictingDevices[i] =
        "device@" + HidlCameraDevice::kDeviceVersion + "/" +
        HidlCameraProvider::kProviderName + "/" +
        std::to_string(hal_cost.conflicting_devices[i]);
  }

  return OK;
}

status_t ConvertToHidlVendorTagSections(
    const std::vector<google_camera_hal::VendorTagSection>& hal_sections,
    hidl_vec<VendorTagSection>* hidl_sections) {
  if (hidl_sections == nullptr) {
    ALOGE("%s: hidl_sections is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_sections->resize(hal_sections.size());
  for (uint32_t i = 0; i < hal_sections.size(); i++) {
    (*hidl_sections)[i].sectionName = hal_sections[i].section_name;
    (*hidl_sections)[i].tags.resize(hal_sections[i].tags.size());

    for (uint32_t j = 0; j < hal_sections[i].tags.size(); j++) {
      (*hidl_sections)[i].tags[j].tagId = hal_sections[i].tags[j].tag_id;
      (*hidl_sections)[i].tags[j].tagName = hal_sections[i].tags[j].tag_name;
      status_t res =
          ConvertToHidlVendorTagType(hal_sections[i].tags[j].tag_type,
                                     &(*hidl_sections)[i].tags[j].tagType);
      if (res != OK) {
        ALOGE("%s: Converting to hidl vendor tag type failed. ", __FUNCTION__);
        return res;
      }
    }
  }

  return OK;
}

Status ConvertToHidlStatus(status_t hal_status) {
  switch (hal_status) {
    case OK:
      return Status::OK;
    case BAD_VALUE:
      return Status::ILLEGAL_ARGUMENT;
    case -EBUSY:
      return Status::CAMERA_IN_USE;
    case -EUSERS:
      return Status::MAX_CAMERAS_IN_USE;
    case UNKNOWN_TRANSACTION:
      return Status::METHOD_NOT_SUPPORTED;
    case INVALID_OPERATION:
      return Status::OPERATION_NOT_SUPPORTED;
    case DEAD_OBJECT:
      return Status::CAMERA_DISCONNECTED;
    default:
      return Status::INTERNAL_ERROR;
  }
}

status_t ConvertToHalTemplateType(
    RequestTemplate hidl_template,
    google_camera_hal::RequestTemplate* hal_template) {
  if (hal_template == nullptr) {
    ALOGE("%s: hal_template is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_template) {
    case RequestTemplate::PREVIEW:
      *hal_template = google_camera_hal::RequestTemplate::kPreview;
      break;
    case RequestTemplate::STILL_CAPTURE:
      *hal_template = google_camera_hal::RequestTemplate::kStillCapture;
      break;
    case RequestTemplate::VIDEO_RECORD:
      *hal_template = google_camera_hal::RequestTemplate::kVideoRecord;
      break;
    case RequestTemplate::VIDEO_SNAPSHOT:
      *hal_template = google_camera_hal::RequestTemplate::kVideoSnapshot;
      break;
    case RequestTemplate::ZERO_SHUTTER_LAG:
      *hal_template = google_camera_hal::RequestTemplate::kZeroShutterLag;
      break;
    case RequestTemplate::MANUAL:
      *hal_template = google_camera_hal::RequestTemplate::kManual;
      break;
    default:
      ALOGE("%s: Unknown HIDL RequestTemplate: %u", __FUNCTION__,
            hidl_template);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHidlHalStreamConfig(
    const std::vector<google_camera_hal::HalStream>& hal_configured_streams,
    HalStreamConfiguration* hidl_hal_stream_config) {
  if (hidl_hal_stream_config == nullptr) {
    ALOGE("%s: hidl_hal_stream_config is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_hal_stream_config->streams.resize(hal_configured_streams.size());

  for (uint32_t i = 0; i < hal_configured_streams.size(); i++) {
    if (hal_configured_streams[i].is_physical_camera_stream) {
      hidl_hal_stream_config->streams[i].physicalCameraId =
          std::to_string(hal_configured_streams[i].physical_camera_id);
    }

    hidl_hal_stream_config->streams[i].v3_3.overrideDataSpace =
        hal_configured_streams[i].override_data_space;

    hidl_hal_stream_config->streams[i].v3_3.v3_2.id =
        hal_configured_streams[i].id;

    hidl_hal_stream_config->streams[i].v3_3.v3_2.overrideFormat =
        (::android::hardware::graphics::common::V1_0::PixelFormat)
            hal_configured_streams[i]
                .override_format;

    hidl_hal_stream_config->streams[i].v3_3.v3_2.producerUsage =
        hal_configured_streams[i].producer_usage;

    hidl_hal_stream_config->streams[i].v3_3.v3_2.consumerUsage =
        hal_configured_streams[i].consumer_usage;

    hidl_hal_stream_config->streams[i].v3_3.v3_2.maxBuffers =
        hal_configured_streams[i].max_buffers;
  }

  return OK;
}

status_t WriteToResultMetadataQueue(
    camera_metadata_t* metadata,
    MessageQueue<uint8_t, kSynchronizedReadWrite>* result_metadata_queue) {
  if (result_metadata_queue == nullptr) {
    return BAD_VALUE;
  }

  if (result_metadata_queue->availableToWrite() <= 0) {
    ALOGW("%s: result_metadata_queue is not available to write", __FUNCTION__);
    return BAD_VALUE;
  }

  uint32_t size = get_camera_metadata_size(metadata);
  bool success = result_metadata_queue->write(
      reinterpret_cast<const uint8_t*>(metadata), size);
  if (!success) {
    ALOGW("%s: Writing to result metadata queue failed. (size=%u)",
          __FUNCTION__, size);
    return INVALID_OPERATION;
  }

  return OK;
}

// Try writing result metadata to result metadata queue. If failed, return
// the metadata to the caller in out_hal_metadata.
status_t TryWritingToResultMetadataQueue(
    std::unique_ptr<google_camera_hal::HalCameraMetadata> hal_metadata,
    MessageQueue<uint8_t, kSynchronizedReadWrite>* result_metadata_queue,
    uint64_t* fmq_result_size,
    std::unique_ptr<google_camera_hal::HalCameraMetadata>* out_hal_metadata) {
  if (out_hal_metadata == nullptr) {
    ALOGE("%s: out_hal_metadata is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  *out_hal_metadata = std::move(hal_metadata);

  if (fmq_result_size == nullptr) {
    ALOGE("%s: fmq_result_size is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  *fmq_result_size = 0;
  if (*out_hal_metadata == nullptr) {
    return OK;
  }

  camera_metadata_t* metadata = (*out_hal_metadata)->ReleaseCameraMetadata();
  // Temporarily use the raw metadata to write to metadata queue.
  status_t res = WriteToResultMetadataQueue(metadata, result_metadata_queue);
  *out_hal_metadata = google_camera_hal::HalCameraMetadata::Create(metadata);

  if (res != OK) {
    ALOGW("%s: Writing to result metadata queue failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  *fmq_result_size = (*out_hal_metadata)->GetCameraMetadataSize();
  *out_hal_metadata = nullptr;
  return OK;
}

status_t ConverToHidlResultMetadata(
    MessageQueue<uint8_t, kSynchronizedReadWrite>* result_metadata_queue,
    std::unique_ptr<google_camera_hal::HalCameraMetadata> hal_metadata,
    CameraMetadata* hidl_metadata, uint64_t* fmq_result_size) {
  if (TryWritingToResultMetadataQueue(std::move(hal_metadata),
                                      result_metadata_queue, fmq_result_size,
                                      &hal_metadata) == OK) {
    return OK;
  }

  // If writing to metadata queue failed, attach the metadata to hidl_metadata.
  if (hidl_metadata == nullptr) {
    ALOGE("%s: hidl_metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  uint32_t metadata_size = hal_metadata->GetCameraMetadataSize();
  hidl_metadata->setToExternal(
      reinterpret_cast<uint8_t*>(hal_metadata->ReleaseCameraMetadata()),
      metadata_size, /*shouldOwn=*/true);

  return OK;
}

status_t ConvertToHidlBufferStatus(google_camera_hal::BufferStatus hal_status,
                                   BufferStatus* hidl_status) {
  if (hidl_status == nullptr) {
    ALOGE("%s: hidl_status is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hal_status) {
    case google_camera_hal::BufferStatus::kOk:
      *hidl_status = BufferStatus::OK;
      break;
    case google_camera_hal::BufferStatus::kError:
      *hidl_status = BufferStatus::ERROR;
      break;
    default:
      ALOGE("%s: Unknown HAL buffer status: %u", __FUNCTION__, hal_status);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHidlStreamBuffer(
    const google_camera_hal::StreamBuffer& hal_buffer,
    StreamBuffer* hidl_buffer) {
  if (hidl_buffer == nullptr) {
    ALOGE("%s: hidl_buffer is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_buffer->streamId = hal_buffer.stream_id;
  hidl_buffer->bufferId = hal_buffer.buffer_id;
  hidl_buffer->buffer = nullptr;

  status_t res =
      ConvertToHidlBufferStatus(hal_buffer.status, &hidl_buffer->status);
  if (res != OK) {
    ALOGE("%s: Converting to HIDL buffer status failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  hidl_buffer->acquireFence = nullptr;
  hidl_buffer->releaseFence = hal_buffer.release_fence;
  return OK;
}

status_t ConvertToHidlCaptureResult_V3_2(
    MessageQueue<uint8_t, kSynchronizedReadWrite>* result_metadata_queue,
    google_camera_hal::CaptureResult* hal_result,
    device::V3_2::CaptureResult* hidl_result) {
  if (hidl_result == nullptr) {
    ALOGE("%s: hidl_result is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (hal_result == nullptr) {
    ALOGE("%s: hal_result is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_result->frameNumber = hal_result->frame_number;

  status_t res = ConverToHidlResultMetadata(
      result_metadata_queue, std::move(hal_result->result_metadata),
      &hidl_result->result, &hidl_result->fmqResultSize);
  if (res != OK) {
    ALOGE("%s: Converting to HIDL result metadata failed: %s(%d).",
          __FUNCTION__, strerror(-res), res);
    return res;
  }

  hidl_result->outputBuffers.resize(hal_result->output_buffers.size());
  for (uint32_t i = 0; i < hidl_result->outputBuffers.size(); i++) {
    res = ConvertToHidlStreamBuffer(hal_result->output_buffers[i],
                                    &hidl_result->outputBuffers[i]);
    if (res != OK) {
      ALOGE("%s: Converting to HIDL output stream buffer failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  }

  uint32_t num_input_buffers = hal_result->input_buffers.size();
  if (num_input_buffers > 0) {
    if (num_input_buffers > 1) {
      ALOGW("%s: HAL result should not have more than 1 input buffer. (=%u)",
            __FUNCTION__, num_input_buffers);
    }

    res = ConvertToHidlStreamBuffer(hal_result->input_buffers[0],
                                    &hidl_result->inputBuffer);
    if (res != OK) {
      ALOGE("%s: Converting to HIDL input stream buffer failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  } else {
    hidl_result->inputBuffer.streamId = -1;
  }

  hidl_result->partialResult = hal_result->partial_result;
  return OK;
}

status_t ConvertToHidlCaptureResult(
    MessageQueue<uint8_t, kSynchronizedReadWrite>* result_metadata_queue,
    std::unique_ptr<google_camera_hal::CaptureResult> hal_result,
    CaptureResult* hidl_result) {
  if (hidl_result == nullptr) {
    ALOGE("%s: hidl_result is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (hal_result == nullptr) {
    ALOGE("%s: hal_result is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res = ConvertToHidlCaptureResult_V3_2(
      result_metadata_queue, hal_result.get(), &hidl_result->v3_2);
  if (res != OK) {
    ALOGE("%s: Converting to V3.2 HIDL result failed: %s(%d).", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  uint32_t num_physical_metadata = hal_result->physical_metadata.size();
  hidl_result->physicalCameraMetadata.resize(num_physical_metadata);

  for (uint32_t i = 0; i < num_physical_metadata; i++) {
    hidl_result->physicalCameraMetadata[i].physicalCameraId =
        std::to_string(hal_result->physical_metadata[i].physical_camera_id);

    res = ConverToHidlResultMetadata(
        result_metadata_queue,
        std::move(hal_result->physical_metadata[i].metadata),
        &hidl_result->physicalCameraMetadata[i].metadata,
        &hidl_result->physicalCameraMetadata[i].fmqMetadataSize);
    if (res != OK) {
      ALOGE("%s: Converting to HIDL physical metadata failed: %s(%d).",
            __FUNCTION__, strerror(-res), res);
      return res;
    }
  }

  return OK;
}

status_t ConvertToHidlErrorMessage(
    const google_camera_hal::ErrorMessage& hal_error, ErrorMsg* hidl_error) {
  if (hidl_error == nullptr) {
    ALOGE("%s: hidl_error is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_error->frameNumber = hal_error.frame_number;
  hidl_error->errorStreamId = hal_error.error_stream_id;

  switch (hal_error.error_code) {
    case google_camera_hal::ErrorCode::kErrorDevice:
      hidl_error->errorCode = ErrorCode::ERROR_DEVICE;
      break;
    case google_camera_hal::ErrorCode::kErrorRequest:
      hidl_error->errorCode = ErrorCode::ERROR_REQUEST;
      break;
    case google_camera_hal::ErrorCode::kErrorResult:
      hidl_error->errorCode = ErrorCode::ERROR_RESULT;
      break;
    case google_camera_hal::ErrorCode::kErrorBuffer:
      hidl_error->errorCode = ErrorCode::ERROR_BUFFER;
      break;
    default:
      ALOGE("%s: Unknown error code: %u", __FUNCTION__, hal_error.error_code);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHidlShutterMessage(
    const google_camera_hal::ShutterMessage& hal_shutter,
    ShutterMsg* hidl_shutter) {
  if (hidl_shutter == nullptr) {
    ALOGE("%s: hidl_shutter is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_shutter->frameNumber = hal_shutter.frame_number;
  hidl_shutter->timestamp = hal_shutter.timestamp_ns;
  return OK;
}

status_t ConverToHidlNotifyMessage(
    const google_camera_hal::NotifyMessage& hal_message,
    NotifyMsg* hidl_message) {
  if (hidl_message == nullptr) {
    ALOGE("%s: hidl_message is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res;
  switch (hal_message.type) {
    case google_camera_hal::MessageType::kError:
      hidl_message->type = MsgType::ERROR;
      res = ConvertToHidlErrorMessage(hal_message.message.error,
                                      &hidl_message->msg.error);
      if (res != OK) {
        ALOGE("%s: Converting to HIDL error message failed: %s(%d)",
              __FUNCTION__, strerror(-res), res);
        return res;
      }
      break;
    case google_camera_hal::MessageType::kShutter:
      hidl_message->type = MsgType::SHUTTER;
      res = ConvertToHidlShutterMessage(hal_message.message.shutter,
                                        &hidl_message->msg.shutter);
      if (res != OK) {
        ALOGE("%s: Converting to HIDL shutter message failed: %s(%d)",
              __FUNCTION__, strerror(-res), res);
        return res;
      }
      break;
    default:
      ALOGE("%s: Unknown message type: %u", __FUNCTION__, hal_message.type);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHidlCameraDeviceStatus(
    google_camera_hal::CameraDeviceStatus hal_camera_device_status,
    CameraDeviceStatus* hidl_camera_device_status) {
  if (hidl_camera_device_status == nullptr) {
    ALOGE("%s: hidl_camera_device_status is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hal_camera_device_status) {
    case google_camera_hal::CameraDeviceStatus::kNotPresent:
      *hidl_camera_device_status = CameraDeviceStatus::NOT_PRESENT;
      break;
    case google_camera_hal::CameraDeviceStatus::kPresent:
      *hidl_camera_device_status = CameraDeviceStatus::PRESENT;
      break;
    case google_camera_hal::CameraDeviceStatus::kEnumerating:
      *hidl_camera_device_status = CameraDeviceStatus::ENUMERATING;
      break;
    default:
      ALOGE("%s: Unknown HAL camera device status: %u", __FUNCTION__,
            hal_camera_device_status);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHidlTorchModeStatus(
    google_camera_hal::TorchModeStatus hal_torch_status,
    TorchModeStatus* hidl_torch_status) {
  if (hidl_torch_status == nullptr) {
    ALOGE("%s: hidl_torch_status is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hal_torch_status) {
    case google_camera_hal::TorchModeStatus::kNotAvailable:
      *hidl_torch_status = TorchModeStatus::NOT_AVAILABLE;
      break;
    case google_camera_hal::TorchModeStatus::kAvailableOff:
      *hidl_torch_status = TorchModeStatus::AVAILABLE_OFF;
      break;
    case google_camera_hal::TorchModeStatus::kAvailableOn:
      *hidl_torch_status = TorchModeStatus::AVAILABLE_ON;
      break;
    default:
      ALOGE("%s: Unknown HAL torch mode status: %u", __FUNCTION__,
            hal_torch_status);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHidlBufferRequest(
    const std::vector<google_camera_hal::BufferRequest>& hal_buffer_requests,
    hidl_vec<BufferRequest>* hidl_buffer_requests) {
  if (hidl_buffer_requests == nullptr) {
    ALOGE("%s: hidl_buffer_request is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hidl_buffer_requests->resize(hal_buffer_requests.size());
  for (uint32_t i = 0; i < hal_buffer_requests.size(); i++) {
    (*hidl_buffer_requests)[i].streamId = hal_buffer_requests[i].stream_id;
    (*hidl_buffer_requests)[i].numBuffersRequested =
        hal_buffer_requests[i].num_buffers_requested;
  }
  return OK;
}

status_t ConvertToHalBufferStatus(BufferStatus hidl_status,
                                  google_camera_hal::BufferStatus* hal_status) {
  if (hal_status == nullptr) {
    ALOGE("%s: hal_status is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_status) {
    case BufferStatus::OK:
      *hal_status = google_camera_hal::BufferStatus::kOk;
      break;
    case BufferStatus::ERROR:
      *hal_status = google_camera_hal::BufferStatus::kError;
      break;
    default:
      ALOGE("%s: Unknown HIDL buffer status: %u", __FUNCTION__, hidl_status);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHalStreamBuffer(const StreamBuffer& hidl_buffer,
                                  google_camera_hal::StreamBuffer* hal_buffer) {
  if (hal_buffer == nullptr) {
    ALOGE("%s: hal_buffer is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hal_buffer->stream_id = hidl_buffer.streamId;
  hal_buffer->buffer_id = hidl_buffer.bufferId;
  hal_buffer->buffer = hidl_buffer.buffer.getNativeHandle();

  status_t res =
      ConvertToHalBufferStatus(hidl_buffer.status, &hal_buffer->status);
  if (res != OK) {
    ALOGE("%s: Converting to HAL buffer status failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  hal_buffer->acquire_fence = hidl_buffer.acquireFence.getNativeHandle();
  hal_buffer->release_fence = hidl_buffer.releaseFence.getNativeHandle();

  return OK;
}

status_t ConvertToHalMetadata(
    uint32_t message_queue_setting_size,
    MessageQueue<uint8_t, kSynchronizedReadWrite>* request_metadata_queue,
    const CameraMetadata& request_settings,
    std::unique_ptr<google_camera_hal::HalCameraMetadata>* hal_metadata) {
  if (hal_metadata == nullptr) {
    ALOGE("%s: hal_metadata is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  const camera_metadata_t* metadata = nullptr;
  CameraMetadata metadata_queue_settings;

  if (message_queue_setting_size == 0) {
    // Use the settings in the request.
    if (request_settings.size() != 0) {
      metadata =
          reinterpret_cast<const camera_metadata_t*>(request_settings.data());
    }
  } else {
    // Read the settings from request metadata queue.
    if (request_metadata_queue == nullptr) {
      ALOGE("%s: request_metadata_queue is nullptr", __FUNCTION__);
      return BAD_VALUE;
    }

    metadata_queue_settings.resize(message_queue_setting_size);
    bool success = request_metadata_queue->read(metadata_queue_settings.data(),
                                                message_queue_setting_size);
    if (!success) {
      ALOGE("%s: Failed to read from request metadata queue.", __FUNCTION__);
      return BAD_VALUE;
    }

    metadata = reinterpret_cast<const camera_metadata_t*>(
        metadata_queue_settings.data());
  }

  if (metadata == nullptr) {
    *hal_metadata = nullptr;
    return OK;
  }

  *hal_metadata = google_camera_hal::HalCameraMetadata::Clone(metadata);
  return OK;
}

status_t ConvertToHalCaptureRequest(
    const CaptureRequest& hidl_request,
    MessageQueue<uint8_t, kSynchronizedReadWrite>* request_metadata_queue,
    google_camera_hal::CaptureRequest* hal_request) {
  if (hal_request == nullptr) {
    ALOGE("%s: hal_request is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  hal_request->frame_number = hidl_request.v3_2.frameNumber;

  status_t res = ConvertToHalMetadata(
      hidl_request.v3_2.fmqSettingsSize, request_metadata_queue,
      hidl_request.v3_2.settings, &hal_request->settings);
  if (res != OK) {
    ALOGE("%s: Converting metadata failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  google_camera_hal::StreamBuffer hal_buffer = {};
  if (hidl_request.v3_2.inputBuffer.buffer != nullptr) {
    res = ConvertToHalStreamBuffer(hidl_request.v3_2.inputBuffer, &hal_buffer);
    if (res != OK) {
      ALOGE("%s: Converting hal stream buffer failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    hal_request->input_buffers.push_back(hal_buffer);
  }

  for (auto& buffer : hidl_request.v3_2.outputBuffers) {
    hal_buffer = {};
    status_t res = ConvertToHalStreamBuffer(buffer, &hal_buffer);
    if (res != OK) {
      ALOGE("%s: Converting hal stream buffer failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    hal_request->output_buffers.push_back(hal_buffer);
  }

  for (auto hidl_physical_settings : hidl_request.physicalCameraSettings) {
    std::unique_ptr<google_camera_hal::HalCameraMetadata> hal_physical_settings;
    res = ConvertToHalMetadata(
        hidl_physical_settings.fmqSettingsSize, request_metadata_queue,
        hidl_physical_settings.settings, &hal_physical_settings);
    if (res != OK) {
      ALOGE("%s: Converting to HAL metadata failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    uint32_t camera_id = std::stoul(hidl_physical_settings.physicalCameraId);
    hal_request->physical_camera_settings.emplace(
        camera_id, std::move(hal_physical_settings));
  }

  return OK;
}

status_t ConvertToHalBufferCaches(
    const hidl_vec<BufferCache>& hidl_buffer_caches,
    std::vector<google_camera_hal::BufferCache>* hal_buffer_caches) {
  if (hal_buffer_caches == nullptr) {
    ALOGE("%s: hal_buffer_caches is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  for (auto hidl_cache : hidl_buffer_caches) {
    google_camera_hal::BufferCache hal_cache;
    hal_cache.stream_id = hidl_cache.streamId;
    hal_cache.buffer_id = hidl_cache.bufferId;

    hal_buffer_caches->push_back(hal_cache);
  }

  return OK;
}

status_t ConvertToHalStreamConfigurationMode(
    StreamConfigurationMode hidl_mode,
    google_camera_hal::StreamConfigurationMode* hal_mode) {
  if (hal_mode == nullptr) {
    ALOGE("%s: hal_mode is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_mode) {
    case StreamConfigurationMode::NORMAL_MODE:
      *hal_mode = google_camera_hal::StreamConfigurationMode::kNormal;
      break;
    case StreamConfigurationMode::CONSTRAINED_HIGH_SPEED_MODE:
      *hal_mode =
          google_camera_hal::StreamConfigurationMode::kConstrainedHighSpeed;
      break;
    default:
      ALOGE("%s: Unknown configuration mode %u", __FUNCTION__, hidl_mode);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConverToHalStreamConfig(
    const StreamConfiguration& hidl_stream_config,
    google_camera_hal::StreamConfiguration* hal_stream_config) {
  if (hal_stream_config == nullptr) {
    ALOGE("%s: hal_stream_config is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res;

  for (auto hidl_stream : hidl_stream_config.v3_4.streams) {
    google_camera_hal::Stream hal_stream;
    res = ConvertToHalStream(hidl_stream, &hal_stream);
    if (res != OK) {
      ALOGE("%s: Converting to HAL stream failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }

    hal_stream_config->streams.push_back(hal_stream);
  }

  res = ConvertToHalStreamConfigurationMode(
      hidl_stream_config.v3_4.operationMode, &hal_stream_config->operation_mode);
  if (res != OK) {
    ALOGE("%s: Converting to HAL opeation mode failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  res = ConvertToHalMetadata(0, nullptr, hidl_stream_config.v3_4.sessionParams,
                             &hal_stream_config->session_params);
  if (res != OK) {
    ALOGE("%s: Converting to HAL metadata failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  hal_stream_config->stream_config_counter =
      hidl_stream_config.streamConfigCounter;

  return OK;
}

status_t ConverToHalStreamConfig(
    const device::V3_4::StreamConfiguration& hidl_stream_config,
    google_camera_hal::StreamConfiguration* hal_stream_config) {
  if (hal_stream_config == nullptr) {
    ALOGE("%s: hal_stream_config is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res;
  for (auto hidl_stream : hidl_stream_config.streams) {
    google_camera_hal::Stream hal_stream;
    res = ConvertToHalStream(hidl_stream, &hal_stream);
    if (res != OK) {
      ALOGE("%s: Converting to HAL stream failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return res;
    }
    hal_stream_config->streams.push_back(hal_stream);
  }

  res = ConvertToHalStreamConfigurationMode(hidl_stream_config.operationMode,
                                            &hal_stream_config->operation_mode);
  if (res != OK) {
    ALOGE("%s: Converting to HAL opeation mode failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  res = ConvertToHalMetadata(0, nullptr, hidl_stream_config.sessionParams,
                             &hal_stream_config->session_params);
  if (res != OK) {
    ALOGE("%s: Converting to HAL metadata failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  return OK;
}

status_t ConvertToHalStreamType(StreamType hidl_stream_type,
                                google_camera_hal::StreamType* hal_stream_type) {
  if (hal_stream_type == nullptr) {
    ALOGE("%s: hal_stream_type is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_stream_type) {
    case StreamType::OUTPUT:
      *hal_stream_type = google_camera_hal::StreamType::kOutput;
      break;
    case StreamType::INPUT:
      *hal_stream_type = google_camera_hal::StreamType::kInput;
      break;
    default:
      ALOGE("%s: Unknown stream type: %u", __FUNCTION__, hidl_stream_type);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHalStreamRotation(
    StreamRotation hidl_stream_rotation,
    google_camera_hal::StreamRotation* hal_stream_rotation) {
  if (hal_stream_rotation == nullptr) {
    ALOGE("%s: hal_stream_rotation is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_stream_rotation) {
    case StreamRotation::ROTATION_0:
      *hal_stream_rotation = google_camera_hal::StreamRotation::kRotation0;
      break;
    case StreamRotation::ROTATION_90:
      *hal_stream_rotation = google_camera_hal::StreamRotation::kRotation90;
      break;
    case StreamRotation::ROTATION_180:
      *hal_stream_rotation = google_camera_hal::StreamRotation::kRotation180;
      break;
    case StreamRotation::ROTATION_270:
      *hal_stream_rotation = google_camera_hal::StreamRotation::kRotation270;
      break;
    default:
      ALOGE("%s: Unknown stream rotation: %u", __FUNCTION__,
            hidl_stream_rotation);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHalStream(const Stream& hidl_stream,
                            google_camera_hal::Stream* hal_stream) {
  if (hal_stream == nullptr) {
    ALOGE("%s: hal_stream is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  *hal_stream = {};

  hal_stream->id = hidl_stream.v3_2.id;

  status_t res = ConvertToHalStreamType(hidl_stream.v3_2.streamType,
                                        &hal_stream->stream_type);
  if (res != OK) {
    ALOGE("%s: Converting to HAL stream type failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  hal_stream->width = hidl_stream.v3_2.width;
  hal_stream->height = hidl_stream.v3_2.height;
  hal_stream->format = (android_pixel_format_t)hidl_stream.v3_2.format;
  hal_stream->usage = (uint64_t)hidl_stream.v3_2.usage;
  hal_stream->data_space = (android_dataspace_t)hidl_stream.v3_2.dataSpace;

  res = ConvertToHalStreamRotation(hidl_stream.v3_2.rotation,
                                   &hal_stream->rotation);
  if (res != OK) {
    ALOGE("%s: Converting to HAL stream rotation failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  if (hidl_stream.physicalCameraId.empty()) {
    hal_stream->is_physical_camera_stream = false;
  } else {
    hal_stream->is_physical_camera_stream = true;
    hal_stream->physical_camera_id = std::stoul(hidl_stream.physicalCameraId);
  }

  hal_stream->buffer_size = hidl_stream.bufferSize;

  return OK;
}

status_t ConvertToHalTorchMode(TorchMode hidl_torch_mode,
                               google_camera_hal::TorchMode* hal_torch_mode) {
  if (hal_torch_mode == nullptr) {
    ALOGE("%s: hal_torch_mode is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_torch_mode) {
    case TorchMode::ON:
      *hal_torch_mode = google_camera_hal::TorchMode::kOn;
      break;
    case TorchMode::OFF:
      *hal_torch_mode = google_camera_hal::TorchMode::kOff;
      break;
    default:
      ALOGE("%s: Unknown torch mode: %u", __FUNCTION__, hidl_torch_mode);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHalBufferRequestStatus(
    const BufferRequestStatus& hidl_buffer_request_status,
    google_camera_hal::BufferRequestStatus* hal_buffer_request_status) {
  if (hal_buffer_request_status == nullptr) {
    ALOGE("%s: hal_buffer_request_status is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  switch (hidl_buffer_request_status) {
    case BufferRequestStatus::OK:
      *hal_buffer_request_status = google_camera_hal::BufferRequestStatus::kOk;
      break;
    case BufferRequestStatus::FAILED_PARTIAL:
      *hal_buffer_request_status =
          google_camera_hal::BufferRequestStatus::kFailedPartial;
      break;
    case BufferRequestStatus::FAILED_CONFIGURING:
      *hal_buffer_request_status =
          google_camera_hal::BufferRequestStatus::kFailedConfiguring;
      break;
    case BufferRequestStatus::FAILED_ILLEGAL_ARGUMENTS:
      *hal_buffer_request_status =
          google_camera_hal::BufferRequestStatus::kFailedIllegalArgs;
      break;
    case BufferRequestStatus::FAILED_UNKNOWN:
      *hal_buffer_request_status =
          google_camera_hal::BufferRequestStatus::kFailedUnknown;
      break;
    default:
      ALOGE("%s: Failed unknown buffer request error code %d", __FUNCTION__,
            hidl_buffer_request_status);
      return BAD_VALUE;
  }

  return OK;
}

status_t ConvertToHalBufferReturnStatus(
    const StreamBufferRet& hidl_stream_buffer_return,
    google_camera_hal::BufferReturn* hal_buffer_return) {
  if (hal_buffer_return == nullptr) {
    ALOGE("%s: hal_buffer_return is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  if (hidl_stream_buffer_return.val.getDiscriminator() ==
      StreamBuffersVal::hidl_discriminator::error) {
    switch (hidl_stream_buffer_return.val.error()) {
      case StreamBufferRequestError::NO_BUFFER_AVAILABLE:
        hal_buffer_return->val.error =
            google_camera_hal::StreamBufferRequestError::kNoBufferAvailable;
        break;
      case StreamBufferRequestError::MAX_BUFFER_EXCEEDED:
        hal_buffer_return->val.error =
            google_camera_hal::StreamBufferRequestError::kMaxBufferExceeded;
        break;
      case StreamBufferRequestError::STREAM_DISCONNECTED:
        hal_buffer_return->val.error =
            google_camera_hal::StreamBufferRequestError::kStreamDisconnected;
        break;
      case StreamBufferRequestError::UNKNOWN_ERROR:
        hal_buffer_return->val.error =
            google_camera_hal::StreamBufferRequestError::kUnknownError;
        break;
      default:
        ALOGE("%s: Unknown StreamBufferRequestError %d", __FUNCTION__,
              hidl_stream_buffer_return.val.error());
        return BAD_VALUE;
    }
  } else {
    hal_buffer_return->val.error =
        google_camera_hal::StreamBufferRequestError::kOk;
  }

  return OK;
}
}  // namespace hidl_utils
}  // namespace implementation
}  // namespace camera
}  // namespace hardware
}  // namespace android
