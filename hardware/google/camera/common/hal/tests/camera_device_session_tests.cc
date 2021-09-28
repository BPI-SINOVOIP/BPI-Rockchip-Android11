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

#define LOG_TAG "CameraDeviceSessionTests"
#include <dlfcn.h>
#include <log/log.h>
#include <sys/stat.h>

#include <gtest/gtest.h>

#include <algorithm>

#include "gralloc_buffer_allocator.h"
#include "hwl_types.h"
#include "mock_device_session_hwl.h"
#include "test_utils.h"

namespace android {
namespace google_camera_hal {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;

// HAL external capture session library path
#if defined(_LP64)
constexpr char kExternalCaptureSessionDir[] =
    "/vendor/lib64/camera/capture_sessions/";
#else  // defined(_LP64)
constexpr char kExternalCaptureSessionDir[] =
    "/vendor/lib/camera/capture_sessions/";
#endif

// Returns an array of regular files under dir_path.
static std::vector<std::string> FindLibraryPaths(const char* dir_path) {
  std::vector<std::string> libs;

  errno = 0;
  DIR* dir = opendir(dir_path);
  if (!dir) {
    ALOGD("%s: Unable to open directory %s (%s)", __FUNCTION__, dir_path,
          strerror(errno));
    return libs;
  }

  struct dirent* entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    std::string lib_path(dir_path);
    lib_path += entry->d_name;
    struct stat st;
    if (stat(lib_path.c_str(), &st) == 0) {
      if (S_ISREG(st.st_mode)) {
        libs.push_back(lib_path);
      }
    }
  }

  return libs;
}

class CameraDeviceSessionTests : public ::testing::Test {
 protected:
  static constexpr uint32_t kCaptureTimeoutMs = 3000;
  std::vector<GetCaptureSessionFactoryFunc> external_session_factory_entries_;
  std::vector<void*> external_capture_session_lib_handles_;

  CameraDeviceSessionTests() {
    LoadExternalCaptureSession();
  }

  ~CameraDeviceSessionTests() {
    for (auto lib_handle : external_capture_session_lib_handles_) {
      dlclose(lib_handle);
    }
  }

  status_t LoadExternalCaptureSession() {
    if (external_session_factory_entries_.size() > 0) {
      ALOGI("%s: External capture session libraries already loaded; skip.",
            __FUNCTION__);
      return OK;
    }

    for (const auto& lib_path : FindLibraryPaths(kExternalCaptureSessionDir)) {
      ALOGI("%s: Loading %s", __FUNCTION__, lib_path.c_str());
      void* lib_handle = nullptr;
      lib_handle = dlopen(lib_path.c_str(), RTLD_NOW);
      if (lib_handle == nullptr) {
        ALOGW("Failed loading %s.", lib_path.c_str());
        continue;
      }

      GetCaptureSessionFactoryFunc external_session_factory_t =
          reinterpret_cast<GetCaptureSessionFactoryFunc>(
              dlsym(lib_handle, "GetCaptureSessionFactory"));
      if (external_session_factory_t == nullptr) {
        ALOGE("%s: dlsym failed (%s) when loading %s.", __FUNCTION__,
              "GetCaptureSessionFactory", lib_path.c_str());
        dlclose(lib_handle);
        lib_handle = nullptr;
        continue;
      }

      external_session_factory_entries_.push_back(external_session_factory_t);
    }

    return OK;
  }

  void CreateMockSessionHwlAndCheck(
      std::unique_ptr<MockDeviceSessionHwl>* session_hwl) {
    ASSERT_NE(session_hwl, nullptr);

    *session_hwl = std::make_unique<MockDeviceSessionHwl>();
    ASSERT_NE(*session_hwl, nullptr);
  }

  void CreateSessionAndCheck(std::unique_ptr<MockDeviceSessionHwl> session_hwl,
                             std::unique_ptr<CameraDeviceSession>* session) {
    ASSERT_NE(session, nullptr);

    *session = CameraDeviceSession::Create(std::move(session_hwl),
                                           external_session_factory_entries_);
    ASSERT_NE(*session, nullptr);
  }

  void TestInvalidDefaultRequestSettingsForType(RequestTemplate type) {
    std::unique_ptr<MockDeviceSessionHwl> session_hwl;
    CreateMockSessionHwlAndCheck(&session_hwl);
    session_hwl->DelegateCallsToFakeSession();

    EXPECT_CALL(*session_hwl, ConstructDefaultRequestSettings(
                                  /*type=*/_, /*default_settings=*/nullptr))
        .WillRepeatedly(Return(BAD_VALUE));

    std::unique_ptr<CameraDeviceSession> session;
    CreateSessionAndCheck(std::move(session_hwl), &session);

    status_t res =
        session->ConstructDefaultRequestSettings(type,
                                                 /*default_settings=*/nullptr);
    EXPECT_EQ(res, BAD_VALUE);
  }

  void TestDefaultRequestSettingsForType(RequestTemplate type) {
    std::unique_ptr<MockDeviceSessionHwl> session_hwl;
    CreateMockSessionHwlAndCheck(&session_hwl);
    session_hwl->DelegateCallsToFakeSession();

    EXPECT_CALL(*session_hwl, ConstructDefaultRequestSettings(
                                  /*type=*/_, /*default_settings=*/_))
        .Times(AtLeast(1))
        .WillRepeatedly([](RequestTemplate /*type*/,
                           std::unique_ptr<HalCameraMetadata>* default_settings) {
          uint32_t num_entries = 128;
          uint32_t data_bytes = 512;

          *default_settings = HalCameraMetadata::Create(num_entries, data_bytes);
          return OK;
        });

    std::unique_ptr<CameraDeviceSession> session;
    CreateSessionAndCheck(std::move(session_hwl), &session);

    std::unique_ptr<HalCameraMetadata> default_settings;
    status_t res =
        session->ConstructDefaultRequestSettings(type, &default_settings);
    EXPECT_EQ(res, OK);
    ASSERT_NE(default_settings, nullptr);
    EXPECT_GT(default_settings->GetCameraMetadataSize(), static_cast<size_t>(0));
  }

  // Invoked when CameraDeviceSession produces a result.
  void ProcessCaptureResult(std::unique_ptr<CaptureResult> result) {
    EXPECT_NE(result, nullptr);
    if (result == nullptr) {
      return;
    }

    std::lock_guard<std::mutex> lock(callback_lock_);
    auto pending_result = received_results_.find(result->frame_number);
    if (pending_result == received_results_.end()) {
      ALOGE("%s: frame %u result_metadata %p", __FUNCTION__,
            result->frame_number, result->result_metadata.get());
      received_results_.emplace(result->frame_number, std::move(result));
    } else {
      if (result->result_metadata != nullptr) {
        // TODO(b/143902331): support partial results.
        ASSERT_NE(pending_result->second->result_metadata, nullptr);
        pending_result->second->result_metadata =
            std::move(result->result_metadata);
      }

      pending_result->second->input_buffers.insert(
          pending_result->second->input_buffers.end(),
          result->input_buffers.begin(), result->input_buffers.end());

      pending_result->second->output_buffers.insert(
          pending_result->second->output_buffers.end(),
          result->output_buffers.begin(), result->output_buffers.end());

      pending_result->second->partial_result = result->partial_result;
    }

    callback_condition_.notify_one();
  }

  // Invoked when CameraDeviceSession notify a message.
  void Notify(const NotifyMessage& message) {
    std::lock_guard<std::mutex> lock(callback_lock_);
    received_messages_.push_back(message);
    callback_condition_.notify_one();
  }

  void ClearResultsAndMessages() {
    std::lock_guard<std::mutex> lock(callback_lock_);
    received_results_.clear();
    received_messages_.clear();
  }

  bool ContainsTheSameBuffers(std::vector<StreamBuffer> buffers,
                              std::vector<StreamBuffer> other_buffers) {
    // Set of pairs of stream ID and buffer ID.
    std::set<std::pair<uint32_t, uint32_t>> stream_buffer_set;
    std::set<std::pair<uint32_t, uint32_t>> other_stream_buffer_set;

    for (auto& buffer : buffers) {
      stream_buffer_set.emplace(buffer.stream_id, buffer.buffer_id);
    }

    for (auto& buffer : other_buffers) {
      other_stream_buffer_set.emplace(buffer.stream_id, buffer.buffer_id);
    }

    return stream_buffer_set == other_stream_buffer_set;
  }

  // Caller must lock callback_lock_
  bool IsResultReceivedLocked(const CaptureRequest& request) {
    auto result = received_results_.find(request.frame_number);
    if (result == received_results_.end()) {
      return false;
    }

    if (result->second->result_metadata == nullptr) {
      return false;
    }

    if (!ContainsTheSameBuffers(result->second->output_buffers,
                                request.output_buffers)) {
      return false;
    }

    if (!ContainsTheSameBuffers(result->second->input_buffers,
                                request.input_buffers)) {
      return false;
    }

    return true;
  }

  status_t WaitForResult(const CaptureRequest& request, uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(callback_lock_);
    if (IsResultReceivedLocked(request)) {
      return OK;
    }

    bool received = callback_condition_.wait_for(
        lock, std::chrono::milliseconds(timeout_ms),
        [&] { return IsResultReceivedLocked(request); });

    return received ? OK : TIMED_OUT;
  }

  // Caller must lock callback_lock_
  bool IsShutterReceivedLocked(uint32_t frame_number) {
    for (auto& message : received_messages_) {
      if (message.type == MessageType::kShutter &&
          message.message.shutter.frame_number == frame_number) {
        return true;
      }
    }

    return false;
  }

  status_t WaitForShutter(uint32_t frame_number, uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(callback_lock_);
    if (IsShutterReceivedLocked(frame_number)) {
      return OK;
    }

    bool received = callback_condition_.wait_for(
        lock, std::chrono::milliseconds(timeout_ms),
        [&] { return IsShutterReceivedLocked(frame_number); });

    return received ? OK : TIMED_OUT;
  }

  std::mutex callback_lock_;
  std::condition_variable callback_condition_;  // Protected by callback_lock_.

  // Maps from a frame number to the received result from CameraDeviceSession.
  // Protected by callback_lock_.
  std::unordered_map<uint32_t, std::unique_ptr<CaptureResult>> received_results_;

  // Received messages from CameraDeviceSession. Protected by callback_lock_.
  std::vector<NotifyMessage> received_messages_;
};

TEST_F(CameraDeviceSessionTests, Create) {
  auto session = CameraDeviceSession::Create(/*device_session_hwl=*/nullptr,
                                             external_session_factory_entries_);
  EXPECT_EQ(session, nullptr);

  uint32_t num_sessions = 5;
  for (uint32_t i = 0; i < num_sessions; i++) {
    std::unique_ptr<MockDeviceSessionHwl> session_hwl;
    CreateMockSessionHwlAndCheck(&session_hwl);
    session_hwl->DelegateCallsToFakeSession();
    CreateSessionAndCheck(std::move(session_hwl), &session);
    session = nullptr;
  }
}

TEST_F(CameraDeviceSessionTests, ConstructDefaultRequestSettings) {
  std::vector<RequestTemplate> types = {
      RequestTemplate::kPreview,        RequestTemplate::kStillCapture,
      RequestTemplate::kVideoRecord,    RequestTemplate::kVideoSnapshot,
      RequestTemplate::kZeroShutterLag, RequestTemplate::kManual};

  for (auto type : types) {
    TestInvalidDefaultRequestSettingsForType(type);
    TestDefaultRequestSettingsForType(type);
  }
}

TEST_F(CameraDeviceSessionTests, ConfigurePreviewStream) {
  std::vector<std::pair<uint32_t, uint32_t>> preview_resolutions = {
      std::make_pair(640, 480), std::make_pair(1280, 720),
      std::make_pair(1920, 1080)};

  std::unique_ptr<MockDeviceSessionHwl> session_hwl;
  CreateMockSessionHwlAndCheck(&session_hwl);
  session_hwl->DelegateCallsToFakeSession();

  // Expect CreatePipeline() calls back to back.
  EXPECT_CALL(*session_hwl, ConfigurePipeline(/*camera_id=*/_,
                                              /*hwl_pipeline_callback=*/_,
                                              /*request_config=*/_,
                                              /*overall_config=*/_,
                                              /*pipeline_id=*/_))
      .Times(AtLeast(preview_resolutions.size()));

  // Expect BuildPipelines() calls back to back.
  EXPECT_CALL(*session_hwl, BuildPipelines())
      .Times(AtLeast(preview_resolutions.size()));

  // Expect DestroyPipelines() calls back to back except the first
  // stream configuration.
  EXPECT_CALL(*session_hwl, DestroyPipelines())
      .Times(AtLeast(preview_resolutions.size() - 1));

  std::unique_ptr<CameraDeviceSession> session;
  CreateSessionAndCheck(std::move(session_hwl), &session);

  std::vector<HalStream> hal_configured_streams;
  StreamConfiguration preview_config;
  status_t res;

  for (auto& resolution : preview_resolutions) {
    test_utils::GetPreviewOnlyStreamConfiguration(
        &preview_config, resolution.first, resolution.second);
    res = session->ConfigureStreams(preview_config, &hal_configured_streams);
    EXPECT_EQ(res, OK);
  }
}

TEST_F(CameraDeviceSessionTests, PreviewRequests) {
  std::unique_ptr<MockDeviceSessionHwl> session_hwl;
  CreateMockSessionHwlAndCheck(&session_hwl);
  session_hwl->DelegateCallsToFakeSession();

  // Set up mocking expections.
  static constexpr uint32_t kNumPreviewRequests = 5;
  EXPECT_CALL(*session_hwl, ConfigurePipeline(_, _, _, _, _)).Times(1);
  EXPECT_CALL(*session_hwl, SubmitRequests(_, _)).Times(kNumPreviewRequests);

  std::unique_ptr<CameraDeviceSession> session;
  CreateSessionAndCheck(std::move(session_hwl), &session);

  // Configure a preview stream.
  static const uint32_t kPreviewWidth = 640;
  static const uint32_t kPreviewHeight = 480;
  StreamConfiguration preview_config;
  std::vector<HalStream> hal_configured_streams;

  // Set up session callback.
  CameraDeviceSessionCallback session_callback = {
      .process_capture_result =
          [&](std::unique_ptr<CaptureResult> result) {
            ProcessCaptureResult(std::move(result));
          },
      .notify = [&](const NotifyMessage& message) { Notify(message); },
  };

  ThermalCallback thermal_callback = {
      .register_thermal_changed_callback =
          google_camera_hal::RegisterThermalChangedCallbackFunc(
              [](google_camera_hal::NotifyThrottlingFunc /*notify_throttling*/,
                 bool /*filter_type*/,
                 google_camera_hal::TemperatureType /*type*/) {
                return INVALID_OPERATION;
              }),
      .unregister_thermal_changed_callback =
          google_camera_hal::UnregisterThermalChangedCallbackFunc([]() {}),
  };

  session->SetSessionCallback(session_callback, thermal_callback);

  test_utils::GetPreviewOnlyStreamConfiguration(&preview_config, kPreviewWidth,
                                                kPreviewHeight);
  ASSERT_EQ(session->ConfigureStreams(preview_config, &hal_configured_streams),
            OK);
  ASSERT_EQ(hal_configured_streams.size(), static_cast<uint32_t>(1));

  // Allocate buffers.
  auto allocator = GrallocBufferAllocator::Create();
  ASSERT_NE(allocator, nullptr);

  HalBufferDescriptor buffer_descriptor = {
      .width = preview_config.streams[0].width,
      .height = preview_config.streams[0].height,
      .format = hal_configured_streams[0].override_format,
      .producer_flags = hal_configured_streams[0].producer_usage |
                        preview_config.streams[0].usage,
      .consumer_flags = hal_configured_streams[0].consumer_usage,
      .immediate_num_buffers =
          std::max(hal_configured_streams[0].max_buffers, kNumPreviewRequests),
      .max_num_buffers =
          std::max(hal_configured_streams[0].max_buffers, kNumPreviewRequests),
  };

  std::vector<buffer_handle_t> preview_buffers;
  ASSERT_EQ(allocator->AllocateBuffers(buffer_descriptor, &preview_buffers), OK);

  std::unique_ptr<HalCameraMetadata> preview_settings;
  ASSERT_EQ(session->ConstructDefaultRequestSettings(RequestTemplate::kPreview,
                                                     &preview_settings),
            OK);

  // Prepare preview requests.
  std::vector<CaptureRequest> requests;
  for (uint32_t i = 0; i < kNumPreviewRequests; i++) {
    StreamBuffer preview_buffer = {
        .stream_id = preview_config.streams[0].id,
        .buffer_id = i,
        .buffer = preview_buffers[i],
        .status = BufferStatus::kOk,
        .acquire_fence = nullptr,
        .release_fence = nullptr,
    };

    CaptureRequest request = {
        .frame_number = i,
        .settings = HalCameraMetadata::Clone(preview_settings.get()),
        .output_buffers = {preview_buffer},
    };

    requests.push_back(std::move(request));
  }

  ClearResultsAndMessages();
  uint32_t num_processed_requests = 0;
  ASSERT_EQ(session->ProcessCaptureRequest(requests, &num_processed_requests),
            OK);
  ASSERT_EQ(num_processed_requests, requests.size());

  // Verify shutters and results are received.
  for (auto& request : requests) {
    EXPECT_EQ(WaitForShutter(request.frame_number, kCaptureTimeoutMs), OK);
    EXPECT_EQ(WaitForResult(request, kCaptureTimeoutMs), OK);
  }

  allocator->FreeBuffers(&preview_buffers);
}

}  // namespace google_camera_hal
}  // namespace android
