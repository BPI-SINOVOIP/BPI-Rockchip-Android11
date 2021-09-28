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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_DEVICE_SESSION_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_DEVICE_SESSION_H_

#include <android/hardware/camera/device/3.5/ICameraDevice.h>
#include <android/hardware/camera/device/3.5/ICameraDeviceCallback.h>
#include <android/hardware/camera/device/3.5/ICameraDeviceSession.h>
#include <android/hardware/thermal/2.0/IThermal.h>
#include <fmq/MessageQueue.h>

#include <shared_mutex>

#include "camera_device_session.h"
#include "hidl_thermal_utils.h"
#include "profiler.h"

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_5 {
namespace implementation {

using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::device::V3_2::BufferCache;
using ::android::hardware::camera::device::V3_2::RequestTemplate;
using ::android::hardware::camera::device::V3_4::CaptureRequest;
using ::android::hardware::camera::device::V3_5::ICameraDeviceCallback;
using ::android::hardware::camera::device::V3_5::ICameraDeviceSession;
using ::android::hardware::camera::device::V3_5::StreamConfiguration;

using MetadataQueue =
    ::android::hardware::MessageQueue<uint8_t, kSynchronizedReadWrite>;

// HidlCameraDeviceSession implements the HIDL camera device session interface,
// ICameraDeviceSession, that contains the methods to configure and request
// captures from an active camera device.
class HidlCameraDeviceSession : public ICameraDeviceSession {
 public:
  // Create a HidlCameraDeviceSession.
  // device_session is a google camera device session that
  // HidlCameraDeviceSession is going to manage. Creating a
  // HidlCameraDeviceSession will fail if device_session is
  // nullptr.
  static std::unique_ptr<HidlCameraDeviceSession> Create(
      const sp<V3_2::ICameraDeviceCallback>& callback,
      std::unique_ptr<google_camera_hal::CameraDeviceSession> device_session);

  virtual ~HidlCameraDeviceSession();

  // Override functions in ICameraDeviceSession
  Return<void> constructDefaultRequestSettings(
      RequestTemplate type,
      ICameraDeviceSession::constructDefaultRequestSettings_cb _hidl_cb) override;

  Return<void> configureStreams_3_5(
      const StreamConfiguration& requestedConfiguration,
      ICameraDeviceSession::configureStreams_3_5_cb _hidl_cb) override;

  Return<void> getCaptureRequestMetadataQueue(
      ICameraDeviceSession::getCaptureRequestMetadataQueue_cb _hidl_cb) override;

  Return<void> getCaptureResultMetadataQueue(
      ICameraDeviceSession::getCaptureResultMetadataQueue_cb _hidl_cb) override;

  Return<void> processCaptureRequest_3_4(
      const hidl_vec<CaptureRequest>& requests,
      const hidl_vec<BufferCache>& cachesToRemove,
      processCaptureRequest_3_4_cb _hidl_cb) override;

  Return<void> signalStreamFlush(const hidl_vec<int32_t>& streamIds,
                                 uint32_t streamConfigCounter) override;

  Return<void> isReconfigurationRequired(const V3_2::CameraMetadata& oldSessionParams,
      const V3_2::CameraMetadata& newSessionParams,
      ICameraDeviceSession::isReconfigurationRequired_cb _hidl_cb) override;

  Return<Status> flush() override;

  Return<void> close() override;

  // Legacy methods
  Return<void> configureStreams(const V3_2::StreamConfiguration&,
                                configureStreams_cb _hidl_cb) override;

  Return<void> configureStreams_3_3(const V3_2::StreamConfiguration&,
                                    configureStreams_3_3_cb _hidl_cb) override;

  Return<void> configureStreams_3_4(const V3_4::StreamConfiguration&,
                                    configureStreams_3_4_cb _hidl_cb) override;

  Return<void> processCaptureRequest(
      const hidl_vec<V3_2::CaptureRequest>& requests,
      const hidl_vec<BufferCache>& cachesToRemove,
      processCaptureRequest_cb _hidl_cb) override;
  // End of override functions in ICameraDeviceSession

 protected:
  HidlCameraDeviceSession() = default;

 private:
  static constexpr uint32_t kRequestMetadataQueueSizeBytes = 1 << 20;  // 1MB
  static constexpr uint32_t kResultMetadataQueueSizeBytes = 1 << 20;   // 1MB

  // Initialize the latest available gralloc buffer mapper.
  status_t InitializeBufferMapper();

  // Initialize HidlCameraDeviceSession with a CameraDeviceSession.
  status_t Initialize(
      const sp<V3_2::ICameraDeviceCallback>& callback,
      std::unique_ptr<google_camera_hal::CameraDeviceSession> device_session);

  // Create a metadata queue.
  // If override_size_property contains a valid size, it will create a metadata
  // queue of that size. If it override_size_property doesn't contain a valid
  // size, it will create a metadata queue of the default size.
  // default_size_bytes is the default size of the message queue in bytes.
  // override_size_property is the name of the system property that contains
  // the message queue size.
  status_t CreateMetadataQueue(std::unique_ptr<MetadataQueue>* metadata_queue,
                               uint32_t default_size_bytes,
                               const char* override_size_property);

  // Invoked when receiving a result from HAL.
  void ProcessCaptureResult(
      std::unique_ptr<google_camera_hal::CaptureResult> hal_result);

  // Invoked when reciving a message from HAL.
  void NotifyHalMessage(const google_camera_hal::NotifyMessage& hal_message);

  // Invoked when requesting stream buffers from HAL.
  google_camera_hal::BufferRequestStatus RequestStreamBuffers(
      const std::vector<google_camera_hal::BufferRequest>& hal_buffer_requests,
      std::vector<google_camera_hal::BufferReturn>* hal_buffer_returns);

  // Invoked when returning stream buffers from HAL.
  void ReturnStreamBuffers(
      const std::vector<google_camera_hal::StreamBuffer>& return_hal_buffers);

  // Import a buffer handle.
  template <class T, class U>
  buffer_handle_t ImportBufferHandle(const sp<T> buffer_mapper_,
                                     const hidl_handle& buffer_hidl_handle);

  // Set camera device session callbacks.
  void SetSessionCallbacks();

  // Register a thermal changed callback.
  // notify_throttling will be invoked when thermal status changes.
  // If filter_type is false, type will be ignored and all types will be
  // monitored.
  // If filter_type is true, only type will be monitored.
  status_t RegisterThermalChangedCallback(
      google_camera_hal::NotifyThrottlingFunc notify_throttling,
      bool filter_type, google_camera_hal::TemperatureType type);

  // Unregister thermal changed callback.
  void UnregisterThermalChangedCallback();

  std::unique_ptr<google_camera_hal::CameraDeviceSession> device_session_;

  // Metadata queue to read the request metadata from.
  std::unique_ptr<MetadataQueue> request_metadata_queue_;

  // Metadata queue to write the result metadata to.
  std::unique_ptr<MetadataQueue> result_metadata_queue_;

  // Assuming callbacks to framework is thread-safe, the shared mutex is only
  // used to protect member variable writing and reading.
  std::shared_mutex hidl_device_callback_lock_;
  // Protected by hidl_device_callback_lock_
  sp<ICameraDeviceCallback> hidl_device_callback_;

  sp<android::hardware::graphics::mapper::V2_0::IMapper> buffer_mapper_v2_;
  sp<android::hardware::graphics::mapper::V3_0::IMapper> buffer_mapper_v3_;
  sp<android::hardware::graphics::mapper::V4_0::IMapper> buffer_mapper_v4_;

  std::mutex hidl_thermal_mutex_;
  sp<android::hardware::thermal::V2_0::IThermal> thermal_;

  // Must be protected by hidl_thermal_mutex_.
  sp<android::hardware::thermal::V2_0::IThermalChangedCallback>
      thermal_changed_callback_;

  // Flag for profiling first frame processing time.
  bool first_frame_requested_ = false;

  std::mutex pending_first_frame_buffers_mutex_;
  // Profiling first frame process time. Stop timer when it become 0.
  // Must be protected by pending_first_frame_buffers_mutex_
  size_t num_pending_first_frame_buffers_ = 0;
};

}  // namespace implementation
}  // namespace V3_5
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_HIDL_SERVICE_HIDL_CAMERA_DEVICE_SESSION_H_
