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

#define LOG_TAG "GCH_HidlCameraDeviceSession"
#define ATRACE_TAG ATRACE_TAG_CAMERA
//#define LOG_NDEBUG 0
#include <log/log.h>

#include <cutils/properties.h>
#include <cutils/trace.h>
#include <malloc.h>

#include "hidl_camera_device_session.h"
#include "hidl_profiler.h"
#include "hidl_utils.h"

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_5 {
namespace implementation {

namespace hidl_utils = ::android::hardware::camera::implementation::hidl_utils;
namespace hidl_profiler =
    ::android::hardware::camera::implementation::hidl_profiler;

using ::android::hardware::camera::device::V3_2::NotifyMsg;
using ::android::hardware::camera::device::V3_2::StreamBuffer;
using ::android::hardware::camera::device::V3_4::CaptureResult;
using ::android::hardware::camera::device::V3_4::HalStreamConfiguration;
using ::android::hardware::camera::device::V3_5::BufferRequestStatus;
using ::android::hardware::camera::device::V3_5::StreamBufferRet;
using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;
using ::android::hardware::thermal::V2_0::Temperature;
using ::android::hardware::thermal::V2_0::TemperatureType;

std::unique_ptr<HidlCameraDeviceSession> HidlCameraDeviceSession::Create(
    const sp<V3_2::ICameraDeviceCallback>& callback,
    std::unique_ptr<google_camera_hal::CameraDeviceSession> device_session) {
  auto session =
      std::unique_ptr<HidlCameraDeviceSession>(new HidlCameraDeviceSession());
  if (session == nullptr) {
    ALOGE("%s: Cannot create a HidlCameraDeviceSession.", __FUNCTION__);
    return nullptr;
  }

  status_t res = session->Initialize(callback, std::move(device_session));
  if (res != OK) {
    ALOGE("%s: Initializing HidlCameraDeviceSession failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return nullptr;
  }

  return session;
}

HidlCameraDeviceSession::~HidlCameraDeviceSession() {
  close();
  // camera's closing, so flush any unused malloc pages
  mallopt(M_PURGE, 0);
}

void HidlCameraDeviceSession::ProcessCaptureResult(
    std::unique_ptr<google_camera_hal::CaptureResult> hal_result) {
  std::shared_lock lock(hidl_device_callback_lock_);
  if (hidl_device_callback_ == nullptr) {
    ALOGE("%s: hidl_device_callback_ is nullptr", __FUNCTION__);
    return;
  }

  {
    std::lock_guard<std::mutex> pending_lock(pending_first_frame_buffers_mutex_);
    if (!hal_result->output_buffers.empty() &&
        num_pending_first_frame_buffers_ > 0) {
      num_pending_first_frame_buffers_ -= hal_result->output_buffers.size();
      if (num_pending_first_frame_buffers_ == 0) {
        hidl_profiler::OnFirstFrameResult();
      }
    }
  }

  hidl_vec<CaptureResult> hidl_results(1);
  status_t res = hidl_utils::ConvertToHidlCaptureResult(
      result_metadata_queue_.get(), std::move(hal_result), &hidl_results[0]);
  if (res != OK) {
    ALOGE("%s: Converting to HIDL result failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return;
  }

  auto hidl_res = hidl_device_callback_->processCaptureResult_3_4(hidl_results);
  if (!hidl_res.isOk()) {
    ALOGE("%s: processCaptureResult transaction failed: %s.", __FUNCTION__,
          hidl_res.description().c_str());
    return;
  }
}

void HidlCameraDeviceSession::NotifyHalMessage(
    const google_camera_hal::NotifyMessage& hal_message) {
  std::shared_lock lock(hidl_device_callback_lock_);
  if (hidl_device_callback_ == nullptr) {
    ALOGE("%s: hidl_device_callback_ is nullptr", __FUNCTION__);
    return;
  }

  hidl_vec<NotifyMsg> hidl_messages(1);
  status_t res =
      hidl_utils::ConverToHidlNotifyMessage(hal_message, &hidl_messages[0]);
  if (res != OK) {
    ALOGE("%s: Converting to HIDL message failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return;
  }

  auto hidl_res = hidl_device_callback_->notify(hidl_messages);
  if (!hidl_res.isOk()) {
    ALOGE("%s: notify transaction failed: %s.", __FUNCTION__,
          hidl_res.description().c_str());
    return;
  }
}

google_camera_hal::BufferRequestStatus
HidlCameraDeviceSession::RequestStreamBuffers(
    const std::vector<google_camera_hal::BufferRequest>& hal_buffer_requests,
    std::vector<google_camera_hal::BufferReturn>* hal_buffer_returns) {
  std::shared_lock lock(hidl_device_callback_lock_);
  if (hidl_device_callback_ == nullptr) {
    ALOGE("%s: hidl_device_callback_ is nullptr", __FUNCTION__);
    return google_camera_hal::BufferRequestStatus::kFailedUnknown;
  }

  if (hal_buffer_returns == nullptr) {
    ALOGE("%s: hal_buffer_returns is nullptr", __FUNCTION__);
    return google_camera_hal::BufferRequestStatus::kFailedUnknown;
  }

  hidl_vec<BufferRequest> hidl_buffer_requests;
  status_t res = hidl_utils::ConvertToHidlBufferRequest(hal_buffer_requests,
                                                        &hidl_buffer_requests);
  if (res != OK) {
    ALOGE("%s: Converting to Hidl buffer request failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return google_camera_hal::BufferRequestStatus::kFailedUnknown;
  }

  BufferRequestStatus hidl_status;
  hidl_vec<StreamBufferRet> stream_buffer_returns;
  auto cb_status = hidl_device_callback_->requestStreamBuffers(
      hidl_buffer_requests, [&hidl_status, &stream_buffer_returns](
                                BufferRequestStatus status_ret,
                                const hidl_vec<StreamBufferRet>& buffer_ret) {
        hidl_status = status_ret;
        stream_buffer_returns = std::move(buffer_ret);
      });
  if (!cb_status.isOk()) {
    ALOGE("%s: Transaction request stream buffers error: %s", __FUNCTION__,
          cb_status.description().c_str());
    return google_camera_hal::BufferRequestStatus::kFailedUnknown;
  }

  google_camera_hal::BufferRequestStatus hal_buffer_request_status;
  res = hidl_utils::ConvertToHalBufferRequestStatus(hidl_status,
                                                    &hal_buffer_request_status);
  if (res != OK) {
    ALOGE("%s: Converting to Hal buffer request status failed: %s(%d)",
          __FUNCTION__, strerror(-res), res);
    return google_camera_hal::BufferRequestStatus::kFailedUnknown;
  }

  hal_buffer_returns->clear();
  // Converting HIDL stream buffer returns to HAL stream buffer returns.
  for (auto& stream_buffer_return : stream_buffer_returns) {
    google_camera_hal::BufferReturn hal_buffer_return;
    res = hidl_utils::ConvertToHalBufferReturnStatus(stream_buffer_return,
                                                     &hal_buffer_return);
    if (res != OK) {
      ALOGE("%s: Converting to Hal buffer return status failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      return google_camera_hal::BufferRequestStatus::kFailedUnknown;
    }

    if (stream_buffer_return.val.getDiscriminator() ==
        StreamBuffersVal::hidl_discriminator::buffers) {
      const hidl_vec<StreamBuffer>& hidl_buffers =
          stream_buffer_return.val.buffers();
      for (auto& hidl_buffer : hidl_buffers) {
        google_camera_hal::StreamBuffer hal_buffer = {};
        hidl_utils::ConvertToHalStreamBuffer(hidl_buffer, &hal_buffer);
        if (res != OK) {
          ALOGE("%s: Converting to Hal stream buffer failed: %s(%d)",
                __FUNCTION__, strerror(-res), res);
          return google_camera_hal::BufferRequestStatus::kFailedUnknown;
        }

        if (hidl_buffer.acquireFence.getNativeHandle() != nullptr) {
          hal_buffer.acquire_fence =
              native_handle_clone(hidl_buffer.acquireFence.getNativeHandle());
          if (hal_buffer.acquire_fence == nullptr) {
            ALOGE("%s: Cloning Hal stream buffer acquire fence failed",
                  __FUNCTION__);
          }
        }

        hal_buffer.release_fence = nullptr;
        // If buffer handle is not null, we need to import buffer handle and
        // return to the caller.
        if (hidl_buffer.buffer.getNativeHandle() != nullptr) {
          if (buffer_mapper_v4_ != nullptr) {
            hal_buffer.buffer = ImportBufferHandle<
                android::hardware::graphics::mapper::V4_0::IMapper,
                android::hardware::graphics::mapper::V4_0::Error>(
                buffer_mapper_v4_, hidl_buffer.buffer);
          } else if (buffer_mapper_v3_ != nullptr) {
            hal_buffer.buffer = ImportBufferHandle<
                android::hardware::graphics::mapper::V3_0::IMapper,
                android::hardware::graphics::mapper::V3_0::Error>(
                buffer_mapper_v3_, hidl_buffer.buffer);
          } else {
            hal_buffer.buffer = ImportBufferHandle<
                android::hardware::graphics::mapper::V2_0::IMapper,
                android::hardware::graphics::mapper::V2_0::Error>(
                buffer_mapper_v2_, hidl_buffer.buffer);
          }
        }

        hal_buffer_return.val.buffers.push_back(hal_buffer);
      }
    }

    hal_buffer_returns->push_back(hal_buffer_return);
  }

  return hal_buffer_request_status;
}

template <class T, class U>
buffer_handle_t HidlCameraDeviceSession::ImportBufferHandle(
    const sp<T> buffer_mapper_, const hidl_handle& buffer_hidl_handle) {
  U mapper_error;
  buffer_handle_t imported_buffer_handle;

  auto hidl_res = buffer_mapper_->importBuffer(
      buffer_hidl_handle, [&](const auto& error, const auto& buffer_handle) {
        mapper_error = error;
        imported_buffer_handle = static_cast<buffer_handle_t>(buffer_handle);
      });
  if (!hidl_res.isOk() || mapper_error != U::NONE) {
    ALOGE("%s: Importing buffer failed: %s, mapper error %u", __FUNCTION__,
          hidl_res.description().c_str(), mapper_error);
    return nullptr;
  }

  return imported_buffer_handle;
}

void HidlCameraDeviceSession::ReturnStreamBuffers(
    const std::vector<google_camera_hal::StreamBuffer>& return_hal_buffers) {
  std::shared_lock lock(hidl_device_callback_lock_);
  if (hidl_device_callback_ == nullptr) {
    ALOGE("%s: hidl_device_callback_ is nullptr", __FUNCTION__);
    return;
  }

  status_t res = OK;
  hidl_vec<StreamBuffer> hidl_return_buffers;
  hidl_return_buffers.resize(return_hal_buffers.size());
  for (uint32_t i = 0; i < return_hal_buffers.size(); i++) {
    res = hidl_utils::ConvertToHidlStreamBuffer(return_hal_buffers[i],
                                                &hidl_return_buffers[i]);
    if (res != OK) {
      ALOGE("%s: Converting to Hidl stream buffer failed: %s(%d)", __FUNCTION__,
            strerror(-res), res);
      return;
    }
  }

  hidl_device_callback_->returnStreamBuffers(hidl_return_buffers);
}

status_t HidlCameraDeviceSession::InitializeBufferMapper() {
  buffer_mapper_v4_ =
      android::hardware::graphics::mapper::V4_0::IMapper::getService();
  if (buffer_mapper_v4_ != nullptr) {
    return OK;
  }

  buffer_mapper_v3_ =
      android::hardware::graphics::mapper::V3_0::IMapper::getService();
  if (buffer_mapper_v3_ != nullptr) {
    return OK;
  }

  buffer_mapper_v2_ =
      android::hardware::graphics::mapper::V2_0::IMapper::getService();
  if (buffer_mapper_v2_ != nullptr) {
    return OK;
  }

  ALOGE("%s: Getting buffer mapper failed.", __FUNCTION__);
  return UNKNOWN_ERROR;
}

status_t HidlCameraDeviceSession::Initialize(
    const sp<V3_2::ICameraDeviceCallback>& callback,
    std::unique_ptr<google_camera_hal::CameraDeviceSession> device_session) {
  if (device_session == nullptr) {
    ALOGE("%s: device_session is nullptr.", __FUNCTION__);
    return BAD_VALUE;
  }

  status_t res = CreateMetadataQueue(&request_metadata_queue_,
                                     kRequestMetadataQueueSizeBytes,
                                     "ro.vendor.camera.req.fmq.size");
  if (res != OK) {
    ALOGE("%s: Creating request metadata queue failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  res = CreateMetadataQueue(&result_metadata_queue_,
                            kResultMetadataQueueSizeBytes,
                            "ro.vendor.camera.res.fmq.size");
  if (res != OK) {
    ALOGE("%s: Creating result metadata queue failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  // Cast V3.2 callback to V3.5
  auto cast_res = ICameraDeviceCallback::castFrom(callback);
  if (!cast_res.isOk()) {
    ALOGE("%s: Cannot convert to V3.5 device callback.", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  // Initialize buffer mapper
  res = InitializeBufferMapper();
  if (res != OK) {
    ALOGE("%s: Initialize buffer mapper failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  thermal_ = android::hardware::thermal::V2_0::IThermal::getService();
  if (thermal_ == nullptr) {
    ALOGE("%s: Getting thermal failed.", __FUNCTION__);
    // Continue without getting thermal information.
  }

  hidl_device_callback_ = cast_res;
  device_session_ = std::move(device_session);

  SetSessionCallbacks();
  return OK;
}

void HidlCameraDeviceSession::SetSessionCallbacks() {
  google_camera_hal::CameraDeviceSessionCallback session_callback = {
      .process_capture_result = google_camera_hal::ProcessCaptureResultFunc(
          [this](std::unique_ptr<google_camera_hal::CaptureResult> result) {
            ProcessCaptureResult(std::move(result));
          }),
      .notify = google_camera_hal::NotifyFunc(
          [this](const google_camera_hal::NotifyMessage& message) {
            NotifyHalMessage(message);
          }),
      .request_stream_buffers = google_camera_hal::RequestStreamBuffersFunc(
          [this](
              const std::vector<google_camera_hal::BufferRequest>&
                  hal_buffer_requests,
              std::vector<google_camera_hal::BufferReturn>* hal_buffer_returns) {
            return RequestStreamBuffers(hal_buffer_requests, hal_buffer_returns);
          }),
      .return_stream_buffers = google_camera_hal::ReturnStreamBuffersFunc(
          [this](const std::vector<google_camera_hal::StreamBuffer>&
                     return_hal_buffers) {
            ReturnStreamBuffers(return_hal_buffers);
          }),
  };

  google_camera_hal::ThermalCallback thermal_callback = {
      .register_thermal_changed_callback =
          google_camera_hal::RegisterThermalChangedCallbackFunc(
              [this](google_camera_hal::NotifyThrottlingFunc notify_throttling,
                     bool filter_type, google_camera_hal::TemperatureType type) {
                return RegisterThermalChangedCallback(notify_throttling,
                                                      filter_type, type);
              }),
      .unregister_thermal_changed_callback =
          google_camera_hal::UnregisterThermalChangedCallbackFunc(
              [this]() { UnregisterThermalChangedCallback(); }),
  };

  device_session_->SetSessionCallback(session_callback, thermal_callback);
}

status_t HidlCameraDeviceSession::RegisterThermalChangedCallback(
    google_camera_hal::NotifyThrottlingFunc notify_throttling, bool filter_type,
    google_camera_hal::TemperatureType type) {
  std::lock_guard<std::mutex> lock(hidl_thermal_mutex_);
  if (thermal_ == nullptr) {
    ALOGE("%s: thermal was not initialized.", __FUNCTION__);
    return NO_INIT;
  }

  if (thermal_changed_callback_ != nullptr) {
    ALOGE("%s: thermal changed callback is already registered.", __FUNCTION__);
    return ALREADY_EXISTS;
  }

  TemperatureType hidl_type;
  status_t res =
      hidl_thermal_utils::ConvertToHidlTemperatureType(type, &hidl_type);
  if (res != OK) {
    ALOGE("%s: Converting to HIDL type failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    return res;
  }

  std::unique_ptr<hidl_thermal_utils::HidlThermalChangedCallback> callback =
      hidl_thermal_utils::HidlThermalChangedCallback::Create(notify_throttling);
  thermal_changed_callback_ = callback.release();
  ThermalStatus thermal_status;
  auto hidl_res = thermal_->registerThermalChangedCallback(
      thermal_changed_callback_, filter_type, hidl_type,
      [&](ThermalStatus status) { thermal_status = status; });
  if (!hidl_res.isOk() || thermal_status.code != ThermalStatusCode::SUCCESS) {
    thermal_changed_callback_ = nullptr;
    return UNKNOWN_ERROR;
  }

  return OK;
}

void HidlCameraDeviceSession::UnregisterThermalChangedCallback() {
  std::lock_guard<std::mutex> lock(hidl_thermal_mutex_);
  if (thermal_changed_callback_ == nullptr) {
    // no-op if no thermal changed callback is registered.
    return;
  }

  if (thermal_ == nullptr) {
    ALOGE("%s: thermal was not initialized.", __FUNCTION__);
    return;
  }

  ThermalStatus thermal_status;
  auto hidl_res = thermal_->unregisterThermalChangedCallback(
      thermal_changed_callback_,
      [&](ThermalStatus status) { thermal_status = status; });
  if (!hidl_res.isOk() || thermal_status.code != ThermalStatusCode::SUCCESS) {
    ALOGW("%s: Unregstering thermal callback failed: %s", __FUNCTION__,
          thermal_status.debugMessage.c_str());
  }

  thermal_changed_callback_ = nullptr;
}

status_t HidlCameraDeviceSession::CreateMetadataQueue(
    std::unique_ptr<MetadataQueue>* metadata_queue, uint32_t default_size_bytes,
    const char* override_size_property) {
  if (metadata_queue == nullptr) {
    ALOGE("%s: metadata_queue is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  int32_t size = default_size_bytes;
  if (override_size_property != nullptr) {
    // Try to read the override size from the system property.
    size = property_get_int32(override_size_property, default_size_bytes);
    ALOGV("%s: request metadata queue size overridden to %d", __FUNCTION__,
          size);
  }

  *metadata_queue = std::make_unique<MetadataQueue>(
      static_cast<size_t>(size), /*configureEventFlagWord=*/false);
  if (!(*metadata_queue)->isValid()) {
    ALOGE("%s: Creating metadata queue (size %d) failed.", __FUNCTION__, size);
    return NO_INIT;
  }

  return OK;
}

Return<void> HidlCameraDeviceSession::constructDefaultRequestSettings(
    RequestTemplate type,
    ICameraDeviceSession::constructDefaultRequestSettings_cb _hidl_cb) {
  V3_2::CameraMetadata hidl_metadata;

  if (device_session_ == nullptr) {
    _hidl_cb(Status::INTERNAL_ERROR, hidl_metadata);
    return Void();
  }

  google_camera_hal::RequestTemplate hal_type;
  status_t res = hidl_utils::ConvertToHalTemplateType(type, &hal_type);
  if (res != OK) {
    _hidl_cb(Status::ILLEGAL_ARGUMENT, hidl_metadata);
    return Void();
  }

  std::unique_ptr<google_camera_hal::HalCameraMetadata> settings = nullptr;
  res = device_session_->ConstructDefaultRequestSettings(hal_type, &settings);
  if (res != OK) {
    _hidl_cb(hidl_utils::ConvertToHidlStatus(res), hidl_metadata);
    return Void();
  }

  uint32_t metadata_size = settings->GetCameraMetadataSize();
  hidl_metadata.setToExternal((uint8_t*)settings->ReleaseCameraMetadata(),
                              metadata_size, /*shouldOwn=*/true);
  _hidl_cb(Status::OK, hidl_metadata);

  return Void();
}

Return<void> HidlCameraDeviceSession::configureStreams_3_5(
    const StreamConfiguration& requestedConfiguration,
    ICameraDeviceSession::configureStreams_3_5_cb _hidl_cb) {
  HalStreamConfiguration hidl_hal_configs;
  if (device_session_ == nullptr) {
    _hidl_cb(Status::ILLEGAL_ARGUMENT, hidl_hal_configs);
    return Void();
  }

  auto profiler_item = hidl_profiler::OnCameraStreamConfigure();
  first_frame_requested_ = false;
  num_pending_first_frame_buffers_ = 0;

  google_camera_hal::StreamConfiguration hal_stream_config;
  status_t res = hidl_utils::ConverToHalStreamConfig(requestedConfiguration,
                                                     &hal_stream_config);
  if (res != OK) {
    _hidl_cb(Status::ILLEGAL_ARGUMENT, hidl_hal_configs);
    return Void();
  }

  std::vector<google_camera_hal::HalStream> hal_configured_streams;
  res = device_session_->ConfigureStreams(hal_stream_config,
                                          &hal_configured_streams);
  if (res != OK) {
    ALOGE("%s: Configuring streams failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(hidl_utils::ConvertToHidlStatus(res), hidl_hal_configs);
    return Void();
  }

  res = hidl_utils::ConvertToHidlHalStreamConfig(hal_configured_streams,
                                                 &hidl_hal_configs);
  _hidl_cb(hidl_utils::ConvertToHidlStatus(res), hidl_hal_configs);

  return Void();
}

Return<void> HidlCameraDeviceSession::getCaptureRequestMetadataQueue(
    ICameraDeviceSession::getCaptureRequestMetadataQueue_cb _hidl_cb) {
  _hidl_cb(*request_metadata_queue_->getDesc());
  return Void();
}

Return<void> HidlCameraDeviceSession::getCaptureResultMetadataQueue(
    ICameraDeviceSession::getCaptureResultMetadataQueue_cb _hidl_cb) {
  _hidl_cb(*result_metadata_queue_->getDesc());
  return Void();
}

Return<void> HidlCameraDeviceSession::processCaptureRequest_3_4(
    const hidl_vec<CaptureRequest>& requests,
    const hidl_vec<BufferCache>& cachesToRemove,
    processCaptureRequest_3_4_cb _hidl_cb) {
  if (device_session_ == nullptr) {
    _hidl_cb(Status::ILLEGAL_ARGUMENT, 0);
    return Void();
  }

  if (!first_frame_requested_) {
    first_frame_requested_ = true;
    num_pending_first_frame_buffers_ = requests[0].v3_2.outputBuffers.size();
    hidl_profiler::OnFirstFrameRequest();
  }

  std::vector<google_camera_hal::BufferCache> hal_buffer_caches;

  status_t res =
      hidl_utils::ConvertToHalBufferCaches(cachesToRemove, &hal_buffer_caches);
  if (res != OK) {
    _hidl_cb(Status::ILLEGAL_ARGUMENT, 0);
    return Void();
  }

  device_session_->RemoveBufferCache(hal_buffer_caches);

  // Converting HIDL requests to HAL requests.
  std::vector<google_camera_hal::CaptureRequest> hal_requests;
  for (auto& request : requests) {
    google_camera_hal::CaptureRequest hal_request = {};
    res = hidl_utils::ConvertToHalCaptureRequest(
        request, request_metadata_queue_.get(), &hal_request);
    if (res != OK) {
      ALOGE("%s: Converting to HAL capture request failed: %s(%d)",
            __FUNCTION__, strerror(-res), res);
      _hidl_cb(hidl_utils::ConvertToHidlStatus(res), 0);
      return Void();
    }

    hal_requests.push_back(std::move(hal_request));
  }

  uint32_t num_processed_requests = 0;
  res = device_session_->ProcessCaptureRequest(hal_requests,
                                               &num_processed_requests);
  if (res != OK) {
    ALOGE(
        "%s: Processing capture request failed: %s(%d). Only processed %u"
        " out of %zu.",
        __FUNCTION__, strerror(-res), res, num_processed_requests,
        hal_requests.size());
  }

  _hidl_cb(hidl_utils::ConvertToHidlStatus(res), num_processed_requests);
  return Void();
}

Return<void> HidlCameraDeviceSession::signalStreamFlush(
    const hidl_vec<int32_t>& /*streamIds*/, uint32_t /*streamConfigCounter*/) {
  // TODO(b/143902312): Implement this.
  return Void();
}

Return<Status> HidlCameraDeviceSession::flush() {
  if (device_session_ == nullptr) {
    return Status::INTERNAL_ERROR;
  }

  auto profiler_item = hidl_profiler::OnCameraFlush();

  status_t res = device_session_->Flush();
  if (res != OK) {
    ALOGE("%s: Flushing device failed: %s(%d).", __FUNCTION__, strerror(-res),
          res);
    return Status::INTERNAL_ERROR;
  }

  return Status::OK;
}

Return<void> HidlCameraDeviceSession::close() {
  if (device_session_ != nullptr) {
    auto profiler_item = hidl_profiler::OnCameraClose();
    device_session_ = nullptr;
  }
  return Void();
}

Return<void> HidlCameraDeviceSession::isReconfigurationRequired(
    const V3_2::CameraMetadata& oldSessionParams,
    const V3_2::CameraMetadata& newSessionParams,
    ICameraDeviceSession::isReconfigurationRequired_cb _hidl_cb) {
  std::unique_ptr<google_camera_hal::HalCameraMetadata> old_hal_session_metadata;
  status_t res = hidl_utils::ConvertToHalMetadata(0, nullptr, oldSessionParams,
                                                  &old_hal_session_metadata);
  if (res != OK) {
    ALOGE("%s: Converting to old session metadata failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, true);
    return Void();
  }

  std::unique_ptr<google_camera_hal::HalCameraMetadata> new_hal_session_metadata;
  res = hidl_utils::ConvertToHalMetadata(0, nullptr, newSessionParams,
                                         &new_hal_session_metadata);
  if (res != OK) {
    ALOGE("%s: Converting to new session metadata failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, true);
    return Void();
  }

  bool reconfiguration_required = true;
  res = device_session_->IsReconfigurationRequired(
      old_hal_session_metadata.get(), new_hal_session_metadata.get(),
      &reconfiguration_required);

  if (res != OK) {
    ALOGE("%s: IsReconfigurationRequired failed: %s(%d)", __FUNCTION__,
          strerror(-res), res);
    _hidl_cb(Status::INTERNAL_ERROR, true);
    return Void();
  }

  _hidl_cb(Status::OK, reconfiguration_required);
  return Void();
}

Return<void> HidlCameraDeviceSession::configureStreams(
    const V3_2::StreamConfiguration&, configureStreams_cb _hidl_cb) {
  _hidl_cb(Status::ILLEGAL_ARGUMENT, V3_2::HalStreamConfiguration());
  return Void();
}
Return<void> HidlCameraDeviceSession::configureStreams_3_3(
    const V3_2::StreamConfiguration&, configureStreams_3_3_cb _hidl_cb) {
  _hidl_cb(Status::ILLEGAL_ARGUMENT, V3_3::HalStreamConfiguration());
  return Void();
}
Return<void> HidlCameraDeviceSession::configureStreams_3_4(
    const V3_4::StreamConfiguration&, configureStreams_3_4_cb _hidl_cb) {
  _hidl_cb(Status::ILLEGAL_ARGUMENT, V3_4::HalStreamConfiguration());
  return Void();
}
Return<void> HidlCameraDeviceSession::processCaptureRequest(
    const hidl_vec<V3_2::CaptureRequest>& requests,
    const hidl_vec<BufferCache>& cachesToRemove,
    processCaptureRequest_cb _hidl_cb) {
  hidl_vec<CaptureRequest> requests_3_4;
  requests_3_4.resize(requests.size());
  for (uint32_t i = 0; i < requests_3_4.size(); i++) {
    requests_3_4[i].v3_2 = requests[i];
  }

  return processCaptureRequest_3_4(requests_3_4, cachesToRemove, _hidl_cb);
}

}  // namespace implementation
}  // namespace V3_5
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
