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

#ifndef HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_SESSION_HWL_H_
#define HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_SESSION_HWL_H_

#include <camera_device_session.h>
#include <gmock/gmock.h>

#include "session_data_defs.h"

namespace android {
namespace google_camera_hal {

// Defines a fake CameraDeviceSessionHwl to be called by MockDeviceSessionHwl.
class FakeCameraDeviceSessionHwl : public CameraDeviceSessionHwl {
 public:
  // Initialize a fake camera device session HWL for a camera ID.
  // If physical_camera_ids is not empty, it will consist of the physical camera
  // IDs.
  FakeCameraDeviceSessionHwl(uint32_t camera_id,
                             const std::vector<uint32_t>& physical_camera_ids);

  virtual ~FakeCameraDeviceSessionHwl() = default;

  status_t ConstructDefaultRequestSettings(
      RequestTemplate type,
      std::unique_ptr<HalCameraMetadata>* default_settings) override;

  status_t PrepareConfigureStreams(
      const StreamConfiguration& request_config) override;

  status_t ConfigurePipeline(uint32_t camera_id,
                             HwlPipelineCallback hwl_pipeline_callback,
                             const StreamConfiguration& request_config,
                             const StreamConfiguration& overall_config,
                             uint32_t* pipeline_id) override;

  status_t BuildPipelines() override;

  // This fake method fills a few dummy streams to streams.
  // Currently only supports kOfflineSmoothTransitionRole.
  status_t GetRequiredIntputStreams(const StreamConfiguration& overall_config,
                                    HwlOfflinePipelineRole pipeline_role,
                                    std::vector<Stream>* streams) override;

  status_t PreparePipeline(uint32_t /*pipeline_id*/, uint32_t /*frame_number*/) {
    return OK;
  }

  status_t GetConfiguredHalStream(
      uint32_t pipeline_id, std::vector<HalStream>* hal_streams) const override;

  void DestroyPipelines() override;

  status_t SubmitRequests(
      uint32_t frame_number,
      const std::vector<HwlPipelineRequest>& requests) override;

  status_t Flush() override;

  uint32_t GetCameraId() const override;

  std::vector<uint32_t> GetPhysicalCameraIds() const override;

  status_t GetCameraCharacteristics(
      std::unique_ptr<HalCameraMetadata>* characteristics) const override;

  status_t GetPhysicalCameraCharacteristics(
      uint32_t physical_camera_id,
      std::unique_ptr<HalCameraMetadata>* characteristics) const override;

  void SetPhysicalCameraIds(const std::vector<uint32_t>& physical_camera_ids);

  status_t SetSessionData(SessionDataKey key, void* value) override;

  status_t GetSessionData(SessionDataKey key, void** value) const override;

  void SetSessionCallback(
      const HwlSessionCallback& hwl_session_callback) override;

  status_t FilterResultMetadata(HalCameraMetadata* metadata) const override;

  std::unique_ptr<IMulticamCoordinatorHwl> CreateMulticamCoordinatorHwl();

  status_t IsReconfigurationRequired(
      const HalCameraMetadata* old_session, const HalCameraMetadata* new_session,
      bool* reconfiguration_required) const override;

  std::unique_ptr<ZoomRatioMapperHwl> GetZoomRatioMapperHwl() override;

 private:
  const uint32_t kCameraId;
  const std::vector<uint32_t> kPhysicalCameraIds;

  mutable std::mutex hwl_pipeline_lock_;

  // Maps from pipeline ID to HWL pipeline callback.
  // Protected by hwl_pipeline_lock_.
  std::unordered_map<uint32_t, HwlPipelineCallback> hwl_pipeline_callbacks_;

  // Maps from pipeline ID to HAL streams. Protected by hwl_pipeline_lock_.
  std::unordered_map<uint32_t, std::vector<HalStream>> pipeline_hal_streams_map_;
};

// Defines a CameraDeviceSessionHwl mock using gmock.
class MockDeviceSessionHwl : public CameraDeviceSessionHwl {
 public:
  // Initialize a mock camera device session HWL for a camera ID.
  // If physical_camera_ids is not empty, it will consist of the physical camera
  // IDs.
  MockDeviceSessionHwl(uint32_t camera_id = 3,  // Dummy camera ID
                       const std::vector<uint32_t>& physical_camera_ids =
                           std::vector<uint32_t>());

  MOCK_METHOD2(ConstructDefaultRequestSettings,
               status_t(RequestTemplate type,
                        std::unique_ptr<HalCameraMetadata>* default_settings));

  MOCK_METHOD5(ConfigurePipeline,
               status_t(uint32_t camera_id,
                        HwlPipelineCallback hwl_pipeline_callback,
                        const StreamConfiguration& request_config,
                        const StreamConfiguration& overall_config,
                        uint32_t* pipeline_id));

  MOCK_METHOD0(BuildPipelines, status_t());

  MOCK_METHOD2(PreparePipeline,
               status_t(uint32_t pipeline_id, uint32_t frame_number));

  MOCK_METHOD3(GetRequiredIntputStreams,
               status_t(const StreamConfiguration& overall_config,
                        HwlOfflinePipelineRole pipeline_role,
                        std::vector<Stream>* streams));

  MOCK_CONST_METHOD2(GetConfiguredHalStream,
                     status_t(uint32_t pipeline_id,
                              std::vector<HalStream>* hal_streams));

  MOCK_METHOD0(DestroyPipelines, void());

  MOCK_METHOD2(SubmitRequests,
               status_t(uint32_t frame_number,
                        const std::vector<HwlPipelineRequest>& requests));

  MOCK_METHOD0(Flush, status_t());

  MOCK_CONST_METHOD0(GetCameraId, uint32_t());

  MOCK_CONST_METHOD0(GetPhysicalCameraIds, std::vector<uint32_t>());

  MOCK_CONST_METHOD1(
      GetCameraCharacteristics,
      status_t(std::unique_ptr<HalCameraMetadata>* characteristics));

  MOCK_CONST_METHOD2(
      GetPhysicalCameraCharacteristics,
      status_t(uint32_t physical_camera_id,
               std::unique_ptr<HalCameraMetadata>* characteristics));

  MOCK_METHOD2(SetSessionData, status_t(SessionDataKey key, void* value));

  MOCK_CONST_METHOD2(GetSessionData, status_t(SessionDataKey key, void** value));

  MOCK_CONST_METHOD1(FilterResultMetadata,
                     status_t(HalCameraMetadata* metadata));

  MOCK_METHOD1(PrepareConfigureStreams, status_t(const StreamConfiguration&));

  MOCK_METHOD1(SetSessionCallback,
               void(const HwlSessionCallback& hwl_session_callback));

  MOCK_METHOD0(CreateMulticamCoordinatorHwl,
               std::unique_ptr<IMulticamCoordinatorHwl>());

  MOCK_CONST_METHOD3(IsReconfigurationRequired,
                     status_t(const HalCameraMetadata* old_session,
                              const HalCameraMetadata* new_session,
                              bool* reconfiguration_required));

  MOCK_METHOD0(GetZoomRatioMapperHwl, std::unique_ptr<ZoomRatioMapperHwl>());

  // Delegate all calls to FakeCameraDeviceSessionHwl.
  void DelegateCallsToFakeSession();

 private:
  FakeCameraDeviceSessionHwl fake_session_hwl_;
};

}  // namespace google_camera_hal
}  // namespace android

#endif  // HARDWARE_GOOGLE_CAMERA_HAL_TESTS_MOCK_DEVICE_SESSION_HWL_H_
