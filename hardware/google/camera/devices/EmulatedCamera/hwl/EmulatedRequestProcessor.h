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

#ifndef EMULATOR_CAMERA_HAL_HWL_REQUEST_PROCESSOR_H
#define EMULATOR_CAMERA_HAL_HWL_REQUEST_PROCESSOR_H

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "EmulatedLogicalRequestState.h"
#include "EmulatedSensor.h"
#include "hwl_types.h"

namespace android {

using google_camera_hal::HalCameraMetadata;
using google_camera_hal::HalStream;
using google_camera_hal::HwlPipelineCallback;
using google_camera_hal::HwlPipelineRequest;
using google_camera_hal::RequestTemplate;
using google_camera_hal::StreamBuffer;

struct EmulatedStream : public HalStream {
  uint32_t width, height;
  size_t buffer_size;
  bool is_input;
};

struct EmulatedPipeline {
  HwlPipelineCallback cb;
  // stream id -> stream map
  std::unordered_map<uint32_t, EmulatedStream> streams;
  uint32_t physical_camera_id, pipeline_id;
};

struct PendingRequest {
  std::unique_ptr<HalCameraMetadata> settings;
  std::unique_ptr<Buffers> input_buffers;
  std::unique_ptr<Buffers> output_buffers;
};

class EmulatedRequestProcessor {
 public:
  EmulatedRequestProcessor(uint32_t camera_id, sp<EmulatedSensor> sensor);
  virtual ~EmulatedRequestProcessor();

  // Process given pipeline requests and invoke the respective callback in a
  // separate thread
  status_t ProcessPipelineRequests(
      uint32_t frame_number, const std::vector<HwlPipelineRequest>& requests,
      const std::vector<EmulatedPipeline>& pipelines);

  status_t GetDefaultRequest(
      RequestTemplate type,
      std::unique_ptr<HalCameraMetadata>* default_settings);

  status_t Flush();

  status_t Initialize(std::unique_ptr<HalCameraMetadata> static_meta,
                      PhysicalDeviceMapPtr physical_devices);

 private:
  void RequestProcessorLoop();

  std::thread request_thread_;
  std::atomic_bool processor_done_ = false;

  // helper methods
  static uint32_t inline AlignTo(uint32_t value, uint32_t alignment) {
    uint32_t delta = value % alignment;
    return (delta == 0) ? value : (value + (alignment - delta));
  }

  status_t GetBufferSizeAndStride(const EmulatedStream& stream,
                                  uint32_t* size /*out*/,
                                  uint32_t* stride /*out*/);
  status_t LockSensorBuffer(const EmulatedStream& stream,
                            HandleImporter& importer /*in*/,
                            buffer_handle_t buffer,
                            SensorBuffer* sensor_buffer /*out*/);
  std::unique_ptr<Buffers> CreateSensorBuffers(
      uint32_t frame_number, const std::vector<StreamBuffer>& buffers,
      const std::unordered_map<uint32_t, EmulatedStream>& streams,
      uint32_t pipeline_id, HwlPipelineCallback cb);
  std::unique_ptr<SensorBuffer> CreateSensorBuffer(uint32_t frame_number,
                                                   const EmulatedStream& stream,
                                                   uint32_t pipeline_id,
                                                   HwlPipelineCallback callback,
                                                   StreamBuffer stream_buffer);
  std::unique_ptr<Buffers> AcquireBuffers(Buffers* buffers);
  void NotifyFailedRequest(const PendingRequest& request);

  std::mutex process_mutex_;
  std::condition_variable request_condition_;
  std::queue<PendingRequest> pending_requests_;
  uint32_t camera_id_;
  sp<EmulatedSensor> sensor_;
  std::unique_ptr<EmulatedLogicalRequestState>
      request_state_;  // Stores and handles 3A and related camera states.
  std::unique_ptr<HalCameraMetadata> last_settings_;

  EmulatedRequestProcessor(const EmulatedRequestProcessor&) = delete;
  EmulatedRequestProcessor& operator=(const EmulatedRequestProcessor&) = delete;
};

}  // namespace android

#endif  // EMULATOR_CAMERA_HAL_HWL_REQUEST_PROCESSOR_H
