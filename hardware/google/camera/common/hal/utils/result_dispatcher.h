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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_UTILS_RESULT_DISPATCHER_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_UTILS_RESULT_DISPATCHER_H_

#include <map>
#include <thread>

#include "hal_types.h"

namespace android {
namespace google_camera_hal {

// ResultDispatcher dispatches capture results in the order of frame numbers,
// including result metadata, shutters, and stream buffers.
//
// The client can add results and shutters via AddResult() and AddShutter() in
// any order. ResultDispatcher will invoke ProcessCaptureResultFunc and
// NotifyFunc to notify result metadata, shutters, and stream buffers in the
// in the order of increasing frame numbers.
class ResultDispatcher {
 public:
  // Create a ResultDispatcher.
  // partial_result_count is the partial result count.
  // process_capture_result is the function to notify capture results.
  // notify is the function to notify shutter messages.
  static std::unique_ptr<ResultDispatcher> Create(
      uint32_t partial_result_count,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify);

  virtual ~ResultDispatcher();

  // Add a pending request. This tells ResultDispatcher to watch for
  // the shutter, result metadata, and stream buffers for this request,
  // that will be added later via AddResult() and AddShutter().
  status_t AddPendingRequest(const CaptureRequest& pending_request);

  // Add a ready result. If the result doesn't belong to a pending request that
  // was previously added via AddPendingRequest(), an error will be returned.
  status_t AddResult(std::unique_ptr<CaptureResult> result);

  // Add a shutter for a frame number. If the frame number doesn't belong to a
  // pending request that was previously added via AddPendingRequest(), an error
  // will be returned.
  status_t AddShutter(uint32_t frame_number, int64_t timestamp_ns);

  // Add an error notification for a frame number. When this is called, we no
  // longer wait for a shutter message or result metadata for the given frame.
  status_t AddError(const ErrorMessage& error);

  // Remove a pending request.
  void RemovePendingRequest(uint32_t frame_number);

 protected:
  ResultDispatcher(uint32_t partial_result_count,
                   ProcessCaptureResultFunc process_capture_result,
                   NotifyFunc notify);

 private:
  static constexpr uint32_t kCallbackThreadTimeoutMs = 500;
  const uint32_t kPartialResultCount;

  // Define a pending shutter that will be ready later when AddShutter() is
  // called.
  struct PendingShutter {
    int64_t timestamp_ns = 0;
    bool ready = false;
  };

  // Define a pending buffer that will be ready later when AddResult() is called.
  struct PendingBuffer {
    StreamBuffer buffer = {};
    bool is_input = false;
    bool ready = false;
  };

  // Define a pending final result metadata that will be ready later when
  // AddResult() is called.
  struct PendingFinalResultMetadata {
    std::unique_ptr<HalCameraMetadata> metadata;
    std::vector<PhysicalCameraMetadata> physical_metadata;
    bool ready = false;
  };

  // Add a pending request for a frame. Must be protected with result_lock_.
  status_t AddPendingRequestLocked(const CaptureRequest& pending_request);

  // Add a pending shutter for a frame. Must be protected with result_lock_.
  status_t AddPendingShutterLocked(uint32_t frame_number);

  // Add a pending final metadata for a frame. Must be protected with
  // result_lock_.
  status_t AddPendingFinalResultMetadataLocked(uint32_t frame_number);

  // Add a pending buffer for a frame. Must be protected with result_lock_.
  status_t AddPendingBufferLocked(uint32_t frame_number,
                                  const StreamBuffer& buffer, bool is_input);

  // Remove pending shutter, result metadata, and buffers for a frame number.
  void RemovePendingRequestLocked(uint32_t frame_number);

  // Invoke process_capture_result_ to notify metadata.
  void NotifyResultMetadata(uint32_t frame_number,
                            std::unique_ptr<HalCameraMetadata> metadata,
                            std::vector<PhysicalCameraMetadata> physical_metadata,
                            uint32_t partial_result);

  status_t AddFinalResultMetadata(
      uint32_t frame_number, std::unique_ptr<HalCameraMetadata> final_metadata,
      std::vector<PhysicalCameraMetadata> physical_metadata);

  status_t AddResultMetadata(
      uint32_t frame_number, std::unique_ptr<HalCameraMetadata> metadata,
      std::vector<PhysicalCameraMetadata> physical_metadata,
      uint32_t partial_result);

  status_t AddBuffer(uint32_t frame_number, StreamBuffer buffer);

  // Get a shutter message that is ready to be notified via notify_.
  status_t GetReadyShutterMessage(NotifyMessage* message);

  // Get a final metadata that is ready to be notified via
  // process_capture_result_.
  status_t GetReadyFinalMetadata(
      uint32_t* frame_number, std::unique_ptr<HalCameraMetadata>* final_metadata,
      std::vector<PhysicalCameraMetadata>* physical_metadata);

  // Get a result with a buffer that is ready to be notified via
  // process_capture_result_.
  status_t GetReadyBufferResult(std::unique_ptr<CaptureResult>* result);

  // Check all pending shutters and invoke notify_ with shutters that are ready.
  void NotifyShutters();

  // Check all pending final result metadata and invoke process_capture_result_
  // with final result metadata that are ready.
  void NotifyFinalResultMetadata();

  // Check all pending buffers and invoke notify_ with buffers that are ready.
  void NotifyBuffers();

  // Thread loop to check pending shutters, result metadata, and buffers. It
  // notifies the client when one is ready.
  void NotifyCallbackThreadLoop();

  void PrintTimeoutMessages();

  std::mutex result_lock_;

  // Maps from frame numbers to pending shutters.
  // Protected by result_lock_.
  std::map<uint32_t, PendingShutter> pending_shutters_;

  // Maps from a stream ID to "a map from a frame number to a pending buffer."
  // Protected by result_lock_.
  std::map<uint32_t, std::map<uint32_t, PendingBuffer>> stream_pending_buffers_map_;

  // Maps from a stream ID to pending result metadata.
  // Protected by result_lock_.
  std::map<uint32_t, PendingFinalResultMetadata> pending_final_metadata_;

  ProcessCaptureResultFunc process_capture_result_;
  NotifyFunc notify_;

  // A thread to run NotifyCallbackThreadLoop().
  std::thread notify_callback_thread_;

  std::mutex notify_callback_lock;

  // Condition to wake up notify_callback_thread_. Used with notify_callback_lock.
  std::condition_variable notify_callback_condition_;

  // Protected by notify_callback_lock.
  bool notify_callback_thread_exiting = false;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_UTILS_RESULT_DISPATCHER_H_
