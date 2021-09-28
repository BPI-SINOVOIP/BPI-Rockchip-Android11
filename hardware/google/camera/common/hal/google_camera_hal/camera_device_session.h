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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_DEVICE__SESSION_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_DEVICE__SESSION_H_

#include <android/hardware/graphics/mapper/2.0/IMapper.h>
#include <android/hardware/graphics/mapper/3.0/IMapper.h>
#include <android/hardware/graphics/mapper/4.0/IMapper.h>
#include <memory>
#include <set>
#include <shared_mutex>

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_session_hwl.h"
#include "capture_session.h"
#include "hal_camera_metadata.h"
#include "hal_types.h"
#include "pending_requests_tracker.h"
#include "stream_buffer_cache_manager.h"
#include "thermal_types.h"
#include "zoom_ratio_mapper.h"

namespace android {
namespace google_camera_hal {

// Defines callbacks to be invoked by a CameraDeviceSession.
struct CameraDeviceSessionCallback {
  // Callback to notify when a camera device produces a capture result.
  ProcessCaptureResultFunc process_capture_result;

  // Callback to notify shutters or errors.
  NotifyFunc notify;

  // Callback to request stream buffers.
  RequestStreamBuffersFunc request_stream_buffers;

  // Callback to return stream buffers.
  ReturnStreamBuffersFunc return_stream_buffers;
};

// Defines callbacks to get thermal information.
struct ThermalCallback {
  // Register a thermal changed callback.
  RegisterThermalChangedCallbackFunc register_thermal_changed_callback;

  // Unregister the thermal changed callback.
  UnregisterThermalChangedCallbackFunc unregister_thermal_changed_callback;
};

// Session function invoked to query if particular stream config supported
using StreamConfigSupportedFunc =
    std::function<bool(CameraDeviceSessionHwl* device_session_hwl,
                       const StreamConfiguration& stream_config)>;

// Session function invoked to create session instance
using CaptureSessionCreateFunc = std::function<std::unique_ptr<CaptureSession>(
    CameraDeviceSessionHwl* device_session_hwl,
    const StreamConfiguration& stream_config,
    ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
    HwlRequestBuffersFunc request_stream_buffers,
    std::vector<HalStream>* hal_configured_streams,
    CameraBufferAllocatorHwl* camera_allocator_hwl)>;

// define entry points to capture session
struct CaptureSessionEntryFuncs {
  StreamConfigSupportedFunc IsStreamConfigurationSupported;
  CaptureSessionCreateFunc CreateSession;
};

// Entry point for getting an external capture session.
using GetCaptureSessionFactoryFunc = ExternalCaptureSessionFactory* (*)();

// CameraDeviceSession implements functions needed for the HIDL camera device
// session interface, ICameraDeviceSession. It contains the methods to configure
// and request captures from an active camera device.
class CameraDeviceSession {
 public:
  // Create a CameraDeviceSession.
  // device_session_hwl is a CameraDeviceSessionHwl that will be managed by this
  // class.
  // If device_session_hwl is nullptr, this method will return nullptr.
  // camera_allocator_hwl is owned by the caller and must be valid during the
  // lifetime of CameraDeviceSession
  static std::unique_ptr<CameraDeviceSession> Create(
      std::unique_ptr<CameraDeviceSessionHwl> device_session_hwl,
      std::vector<GetCaptureSessionFactoryFunc> external_session_factory_entries,
      CameraBufferAllocatorHwl* camera_allocator_hwl = nullptr);

  virtual ~CameraDeviceSession();

  // Set session callbacks
  // Must be called before ConfigureStreams().
  // session_callback will be invoked for capture results and messages.
  // thermal_callback will be invoked for getting thermal information.
  void SetSessionCallback(const CameraDeviceSessionCallback& session_callback,
                          const ThermalCallback& thermal_callback);

  // Construct the default request settings for a request template type.
  status_t ConstructDefaultRequestSettings(
      RequestTemplate type,
      std::unique_ptr<HalCameraMetadata>* default_settings);

  // Configure streams.
  // stream_config is the requested stream configuration.
  // hal_configured_streams is filled by this method with configured stream.
  status_t ConfigureStreams(const StreamConfiguration& stream_config,
                            std::vector<HalStream>* hal_configured_streams);

  // Process a capture request.
  // num_processed_requests is filled by this method with the number of
  // processed requests.
  status_t ProcessCaptureRequest(const std::vector<CaptureRequest>& requests,
                                 uint32_t* num_processed_requests);

  // Remove the buffer caches kept in the camera device session.
  void RemoveBufferCache(const std::vector<BufferCache>& buffer_caches);

  // Flush all pending requests.
  status_t Flush();

  // Check reconfiguration is required or not
  // old_session is old session parameter
  // new_session is new session parameter
  // If reconfiguration is required, set reconfiguration_required to true
  // If reconfiguration is not required, set reconfiguration_required to false
  status_t IsReconfigurationRequired(const HalCameraMetadata* old_session,
                                     const HalCameraMetadata* new_session,
                                     bool* reconfiguration_required);

 protected:
  CameraDeviceSession() = default;

 private:
  // Define buffer cache hashing in order to use BufferCache as a key of an
  // unordered map.
  struct BufferCacheHashing {
    unsigned long operator()(const BufferCache& buffer_cache) const {
      std::string s = "s" + std::to_string(buffer_cache.stream_id) + "b" +
                      std::to_string(buffer_cache.buffer_id);
      return std::hash<std::string>{}(s);
    }
  };

  status_t Initialize(
      std::unique_ptr<CameraDeviceSessionHwl> device_session_hwl,
      CameraBufferAllocatorHwl* camera_allocator_hwl,
      std::vector<GetCaptureSessionFactoryFunc> external_session_factory_entries);

  // Initialize the latest available gralloc buffer mapper.
  status_t InitializeBufferMapper();

  // Initialize callbacks from HWL and callbacks to the client.
  void InitializeCallbacks();

  // Initialize buffer management support.
  status_t InitializeBufferManagement(HalCameraMetadata* characteristics);

  // Update all buffer handles in buffers with the imported buffer handles.
  // Must be protected by imported_buffer_handle_map_lock_.
  status_t UpdateBufferHandlesLocked(std::vector<StreamBuffer>* buffers);

  // Import the buffer handles in the request.
  status_t ImportRequestBufferHandles(const CaptureRequest& request);

  // Import the buffer handles of buffers.
  status_t ImportBufferHandles(const std::vector<StreamBuffer>& buffers);

  // Import the buffer handle of a buffer.
  // Must be protected by imported_buffer_handle_map_lock_.
  template <class T, class U>
  status_t ImportBufferHandleLocked(const sp<T> buffer_mapper,
                                    const StreamBuffer& buffer);

  // Create a request with updated buffer handles and modified settings.
  // Must be protected by session_lock_.
  status_t CreateCaptureRequestLocked(const CaptureRequest& request,
                                      CaptureRequest* updated_request);

  // Add a buffer handle to the imported buffer handle map. If the buffer cache
  // is already in the map but the buffer handle doesn't match, it will
  // return BAD_VALUE.
  // Must be protected by imported_buffer_handle_map_lock_.
  status_t AddImportedBufferHandlesLocked(const BufferCache& buffer_cache,
                                          buffer_handle_t buffer_handle);

  // Return if the buffer handle for a certain buffer ID is imported.
  // Must be protected by imported_buffer_handle_map_lock_.
  bool IsBufferImportedLocked(int32_t stream_id, uint32_t buffer_id);

  // Free all imported buffer handles belonging to the stream id.
  // Must be protected by imported_buffer_handle_map_lock_.
  template <class T>
  void FreeBufferHandlesLocked(const sp<T> buffer_mapper, int32_t stream_id);

  template <class T>
  void FreeImportedBufferHandles(const sp<T> buffer_mapper);

  // Clean up stale streams with new stream configuration.
  // Must be protected by session_lock_.
  void CleanupStaleStreamsLocked(const std::vector<Stream>& new_streams);

  // Append output intent to request settings.
  // Must be protected by session_lock_.
  void AppendOutputIntentToSettingsLocked(const CaptureRequest& request,
                                          CaptureRequest* updated_request);

  // Invoked by HWL to request stream buffers when buffer management is
  // supported.
  status_t RequestStreamBuffers(int32_t stream_id, uint32_t num_buffers,
                                std::vector<StreamBuffer>* buffers,
                                StreamBufferRequestError* request_status);

  // Invoked by HWL to return stream buffers when buffer management is
  // supported.
  void ReturnStreamBuffers(const std::vector<StreamBuffer>& buffers);

  // Update imported buffer handle map for the requested buffers and update
  // the buffer handle in requested buffers.
  status_t UpdateRequestedBufferHandles(std::vector<StreamBuffer>* buffers);

  // Request buffers from stream buffer cache manager
  status_t RequestBuffersFromStreamBufferCacheManager(
      int32_t stream_id, uint32_t num_buffers,
      std::vector<StreamBuffer>* buffers, uint32_t frame_number);

  // Register configured streams into stream buffer cache manager
  status_t RegisterStreamsIntoCacheManagerLocked(
      const StreamConfiguration& stream_config,
      const std::vector<HalStream>& hal_stream);

  // Update the inflight requests/streams and notify SBC for flushing if the
  // inflight requests/streams map is empty.
  status_t UpdatePendingRequest(CaptureResult* result);

  // Process the notification returned from the HWL
  void Notify(const NotifyMessage& result);

  // Process the capture result returned from the HWL
  void ProcessCaptureResult(std::unique_ptr<CaptureResult> result);

  // Notify error message with error code for stream of frame[frame_number].
  // Caller is responsible to make sure this function is called only once for any frame.
  void NotifyErrorMessage(uint32_t frame_number, int32_t stream_id,
                          ErrorCode error_code);

  // Notify buffer error for all output streams in request
  void NotifyBufferError(const CaptureRequest& request);

  // Notify buffer error for stream_id in frame_number
  void NotifyBufferError(uint32_t frame_number, int32_t stream_id,
                         uint64_t buffer_id);

  // Try to check if result contains dummy buffer or dummy buffer from this
  // result has been observed. If so, handle this result specifically. Set
  // result_handled as true.
  status_t TryHandleDummyResult(CaptureResult* result, bool* result_handled);

  // Check if all streams in the current session are active in SBC manager
  status_t HandleInactiveStreams(const CaptureRequest& request,
                                 bool* all_active);

  // Check the capture request before sending it to HWL. Only needed when HAL
  // Buffer Management is supported. The SBC manager determines if it is
  // necessasry to process the request still by checking if all streams are
  // still active for buffer requests.
  void CheckRequestForStreamBufferCacheManager(const CaptureRequest& request,
                                               bool* need_to_process);

  // Return true if a request is valid. Must be exclusively protected by
  // session_lock_.
  status_t ValidateRequestLocked(const CaptureRequest& request);

  // Invoked when thermal status changes.
  void NotifyThrottling(const Temperature& temperature);

  // Unregister thermal callback.
  void UnregisterThermalCallback();

  // Load HAL external capture session libraries.
  status_t LoadExternalCaptureSession(
      std::vector<GetCaptureSessionFactoryFunc> external_session_factory_entries);

  void InitializeZoomRatioMapper(HalCameraMetadata* characteristics);

  uint32_t camera_id_ = 0;
  std::unique_ptr<CameraDeviceSessionHwl> device_session_hwl_;

  // Graphics buffer mapper used to import and free buffers.
  sp<android::hardware::graphics::mapper::V2_0::IMapper> buffer_mapper_v2_;
  sp<android::hardware::graphics::mapper::V3_0::IMapper> buffer_mapper_v3_;
  sp<android::hardware::graphics::mapper::V4_0::IMapper> buffer_mapper_v4_;

  // Assuming callbacks to framework is thread-safe, the shared mutex is only
  // used to protect member variable writing and reading.
  std::shared_mutex session_callback_lock_;
  // Session callback to the client. Protected by session_callback_lock_
  CameraDeviceSessionCallback session_callback_;

  // Camera Device Session callback to the camera device session. Protected by
  // session_callback_lock_
  CameraDeviceSessionCallback camera_device_session_callback_;

  // Callback to get thermal information. Protected by session_callback_lock_.
  ThermalCallback thermal_callback_;

  // Session callback from HWL session. Protected by session_callback_lock_
  HwlSessionCallback hwl_session_callback_;

  // imported_buffer_handle_map_lock_ protects the following variables as noted.
  std::mutex imported_buffer_handle_map_lock_;

  // Store the imported buffer handles from camera framework. Protected by
  // imported_buffer_handle_map_lock.
  std::unordered_map<BufferCache, buffer_handle_t, BufferCacheHashing>
      imported_buffer_handle_map_;

  // session_lock_ protects the following variables as noted.
  std::mutex session_lock_;

  // capture_session_lock_ protects the following variables as noted.
  std::shared_mutex capture_session_lock_;

  std::unique_ptr<CaptureSession>
      capture_session_;  // Protected by capture_session_lock_.

  // Map from a stream ID to the configured stream received from frameworks.
  // Protected by session_lock_.
  std::unordered_map<int32_t, Stream> configured_streams_map_;

  // Last valid settings in capture request. Must be protected by session_lock_.
  std::unique_ptr<HalCameraMetadata> last_request_settings_;

  // If thermal status has become >= ThrottlingSeverity::Severe since stream
  // configuration.
  // Must be protected by session_lock_.
  uint8_t thermal_throttling_ = false;

  // If device session has notified capture session about thermal throttling.
  // Must be protected by session_lock_.
  bool thermal_throttling_notified_ = false;

  // Predefined capture session entry points
  static std::vector<CaptureSessionEntryFuncs> kCaptureSessionEntries;

  // External capture session entry points
  std::vector<ExternalCaptureSessionFactory*> external_capture_session_entries_;

  // Opened library handles that should be closed on destruction
  std::vector<void*> external_capture_session_lib_handles_;

  // hwl allocator
  CameraBufferAllocatorHwl* camera_allocator_hwl_ = nullptr;

  // If buffer management API support.
  bool buffer_management_supported_ = false;

  // Pending requests tracker used when buffer management API is enabled.
  // Protected by session_lock_.
  std::unique_ptr<PendingRequestsTracker> pending_requests_tracker_;

  // Stream buffer cache manager supports the HAL Buffer Management by caching
  // buffers acquired from framework
  std::unique_ptr<StreamBufferCacheManager> stream_buffer_cache_manager_;

  // If we receives valid settings since stream configuration.
  // Protected by session_lock_.
  bool has_valid_settings_ = false;

  // request_record_lock_ protects the following variables as noted
  std::mutex request_record_lock_;

  // Map from frame number to a set of stream ids, which exist in
  // request[frame number]
  // Protected by request_record_lock_;
  std::map<uint32_t, std::set<int32_t>> pending_request_streams_;

  // Set of requests that have been notified for ERROR_REQUEST during buffer
  // request stage.
  // Protected by request_record_lock_;
  std::set<uint32_t> error_notified_requests_;

  // Set of dummy buffer observed
  std::set<buffer_handle_t> dummy_buffer_observed_;

  // The last shutter timestamp in nanoseconds if systrace is enabled. Reset
  // after stream configuration.
  int64_t last_timestamp_ns_for_trace_ = 0;

  // Operation mode of stream configuration
  StreamConfigurationMode operation_mode_ = StreamConfigurationMode::kNormal;

  // Flush is running or not
  std::atomic<bool> is_flushing_ = false;

  // Zoom ratio mapper
  ZoomRatioMapper zoom_ratio_mapper_;

  // Record the result metadata of pending request
  // Protected by request_record_lock_;
  std::set<uint32_t> pending_results_;

  static constexpr int32_t kInvalidStreamId = -1;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAMERA_DEVICE__SESSION_H_
