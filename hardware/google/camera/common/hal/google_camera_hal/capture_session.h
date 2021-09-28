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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAPTURE_SESSION_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAPTURE_SESSION_H_

#include <utils/Errors.h>

#include "camera_buffer_allocator_hwl.h"
#include "camera_device_session_hwl.h"
#include "hal_types.h"
#include "hwl_types.h"

namespace android {
namespace google_camera_hal {

// CaptureSession defines the interface of a capture session. Each capture
// session is associated with a certain stream configuration.
// Classes that inherit this interface class should provide a
//   1. IsStreamConfigurationSupported() for the client to query whether a
//      stream configuration is supported by this capture session.
//   2. Create() for the client to create a capture session and get a unique
//      pointer to the capture session.
//
// A capture session can use RequestProcessor, ProcessBlock, and ResultProcessor
// to form chains of process blocks. A simple capture session can create a
// simple chain like
//
//   RequestProcessor -> ProcessBlock -> ResultProcessor
//
// If additional post-processing is needed, more ProcessBlock can be added to
// the process chain like
//
//   RequestProcessor -> ProcessBlock_0 -> Result/RequestProcessor ->
//   ProcessBlock_1 -> ResultProcessor
//
// Each implementation of RequestProcess, ProcessBlock, and ResultProcessor must
// clearly define their capabilities.
class CaptureSession {
 public:
  virtual ~CaptureSession() = default;

  // Process a capture request.
  virtual status_t ProcessRequest(const CaptureRequest& request) = 0;

  // Flush all pending capture requests.
  virtual status_t Flush() = 0;
};

// ExternalCaptureSessionFactory defines the interface of an external capture session,
// in addition to `class CaptureSession`.
class ExternalCaptureSessionFactory {
 public:
  virtual ~ExternalCaptureSessionFactory() = default;

  // IsStreamConfigurationSupported is called by the client to query whether a
  // stream configuration is supported by this capture session.
  virtual bool IsStreamConfigurationSupported(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config) = 0;

  // Create is called by the client to create a capture session and get a unique
  // pointer to the capture session.
  virtual std::unique_ptr<CaptureSession> CreateSession(
      CameraDeviceSessionHwl* device_session_hwl,
      const StreamConfiguration& stream_config,
      ProcessCaptureResultFunc process_capture_result, NotifyFunc notify,
      HwlRequestBuffersFunc request_stream_buffers,
      std::vector<HalStream>* hal_configured_streams,
      CameraBufferAllocatorHwl* camera_allocator_hwl) = 0;
};

// GetCaptureSessionFactory should be called by the client to discover an external
// capture session via dlsym.
extern "C" ExternalCaptureSessionFactory* GetCaptureSessionFactory();

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_GOOGLE_CAMERA_HAL_CAPTURE_SESSION_H_