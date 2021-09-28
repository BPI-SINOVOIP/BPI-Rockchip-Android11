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

#ifndef EMULATOR_CAMERA_HAL_HWL_REQUEST_STATE_H
#define EMULATOR_CAMERA_HAL_HWL_REQUEST_STATE_H

#include <mutex>
#include <unordered_map>

#include "EmulatedSensor.h"
#include "hwl_types.h"

namespace android {

using google_camera_hal::HalCameraMetadata;
using google_camera_hal::HalStream;
using google_camera_hal::HwlPipelineCallback;
using google_camera_hal::HwlPipelineRequest;
using google_camera_hal::RequestTemplate;
using google_camera_hal::StreamBuffer;

struct PendingRequest;

class EmulatedRequestState {
 public:
  EmulatedRequestState(uint32_t camera_id) : camera_id_(camera_id) {
  }
  virtual ~EmulatedRequestState() {
  }

  status_t Initialize(std::unique_ptr<HalCameraMetadata> static_meta);

  status_t GetDefaultRequest(
      RequestTemplate type,
      std::unique_ptr<HalCameraMetadata>* default_settings /*out*/);

  std::unique_ptr<HwlPipelineResult> InitializeResult(uint32_t pipeline_id,
                                                      uint32_t frame_number);

  status_t InitializeSensorSettings(
      std::unique_ptr<HalCameraMetadata> request_settings,
      EmulatedSensor::SensorSettings* sensor_settings /*out*/);

 private:
  bool SupportsCapability(uint8_t cap);

  status_t InitializeRequestDefaults();
  status_t InitializeSensorDefaults();
  status_t InitializeFlashDefaults();
  status_t InitializeControlDefaults();
  status_t InitializeControlAEDefaults();
  status_t InitializeControlAWBDefaults();
  status_t InitializeControlAFDefaults();
  status_t InitializeControlSceneDefaults();
  status_t InitializeHotPixelDefaults();
  status_t InitializeStatisticsDefaults();
  status_t InitializeTonemapDefaults();
  status_t InitializeBlackLevelDefaults();
  status_t InitializeEdgeDefaults();
  status_t InitializeShadingDefaults();
  status_t InitializeNoiseReductionDefaults();
  status_t InitializeColorCorrectionDefaults();
  status_t InitializeScalerDefaults();
  status_t InitializeReprocessDefaults();
  status_t InitializeMeteringRegionDefault(uint32_t tag,
                                           int32_t* region /*out*/);
  status_t InitializeControlefaults();
  status_t InitializeInfoDefaults();
  status_t InitializeLensDefaults();

  status_t ProcessAE();
  status_t ProcessAF();
  status_t ProcessAWB();
  status_t DoFakeAE();
  status_t CompensateAE();
  status_t Update3AMeteringRegion(uint32_t tag,
                                  const HalCameraMetadata& settings,
                                  int32_t* region /*out*/);

  std::mutex request_state_mutex_;
  std::unique_ptr<HalCameraMetadata> request_settings_;

  // Supported capabilities and features
  static const std::set<uint8_t> kSupportedCapabilites;
  static const std::set<uint8_t> kSupportedHWLevels;
  std::unique_ptr<HalCameraMetadata> static_metadata_;

  // android.blacklevel.*
  uint8_t black_level_lock_ = ANDROID_BLACK_LEVEL_LOCK_ON;
  bool report_black_level_lock_ = false;

  // android.colorcorrection.*
  std::set<uint8_t> available_color_aberration_modes_;

  // android.edge.*
  std::set<uint8_t> available_edge_modes_;
  bool report_edge_mode_ = false;

  // android.shading.*
  std::set<uint8_t> available_shading_modes_;

  // android.noiseReduction.*
  std::set<uint8_t> available_noise_reduction_modes_;

  // android.request.*
  std::set<uint8_t> available_capabilities_;
  std::set<int32_t> available_characteristics_;
  std::set<int32_t> available_results_;
  std::set<int32_t> available_requests_;
  uint8_t max_pipeline_depth_ = 0;
  int32_t partial_result_count_ = 1;  // TODO: add support for partial results
  bool supports_manual_sensor_ = false;
  bool supports_manual_post_processing_ = false;
  bool is_backward_compatible_ = false;
  bool is_raw_capable_ = false;
  bool supports_private_reprocessing_ = false;
  bool supports_yuv_reprocessing_ = false;

  // android.control.*
  struct SceneOverride {
    uint8_t ae_mode, awb_mode, af_mode;
    SceneOverride()
        : ae_mode(ANDROID_CONTROL_AE_MODE_OFF),
          awb_mode(ANDROID_CONTROL_AWB_MODE_OFF),
          af_mode(ANDROID_CONTROL_AF_MODE_OFF) {
    }
    SceneOverride(uint8_t ae, uint8_t awb, uint8_t af)
        : ae_mode(ae), awb_mode(awb), af_mode(af) {
    }
  };

  struct FPSRange {
    int32_t min_fps, max_fps;
    FPSRange() : min_fps(-1), max_fps(-1) {
    }
    FPSRange(int32_t min, int32_t max) : min_fps(min), max_fps(max) {
    }
  };

  struct ExtendedSceneModeCapability {
    int32_t mode, max_width, max_height;
    float min_zoom, max_zoom;
    ExtendedSceneModeCapability()
        : mode(ANDROID_CONTROL_EXTENDED_SCENE_MODE_DISABLED),
          max_width(-1),
          max_height(-1),
          min_zoom(1.0f),
          max_zoom(1.0f) {
    }
    ExtendedSceneModeCapability(int32_t m, int32_t w, int32_t h, float min_z,
                                float max_z)
        : mode(m), max_width(w), max_height(h), min_zoom(min_z), max_zoom(max_z) {
    }
  };

  std::set<uint8_t> available_control_modes_;
  std::set<uint8_t> available_ae_modes_;
  std::set<uint8_t> available_af_modes_;
  std::set<uint8_t> available_awb_modes_;
  std::set<uint8_t> available_scenes_;
  std::set<uint8_t> available_antibanding_modes_;
  std::set<uint8_t> available_effects_;
  std::set<uint8_t> available_vstab_modes_;
  std::vector<ExtendedSceneModeCapability> available_extended_scene_mode_caps_;
  std::unordered_map<uint8_t, SceneOverride> scene_overrides_;
  std::vector<FPSRange> available_fps_ranges_;
  int32_t exposure_compensation_range_[2] = {0, 0};
  float max_zoom_ = 1.0f;
  bool zoom_ratio_supported_ = false;
  float min_zoom_ = 1.0f;
  camera_metadata_rational exposure_compensation_step_ = {0, 1};
  bool exposure_compensation_supported_ = false;
  int32_t exposure_compensation_ = 0;
  int32_t ae_metering_region_[5] = {0, 0, 0, 0, 0};
  int32_t awb_metering_region_[5] = {0, 0, 0, 0, 0};
  int32_t af_metering_region_[5] = {0, 0, 0, 0, 0};
  size_t max_ae_regions_ = 0;
  size_t max_awb_regions_ = 0;
  size_t max_af_regions_ = 0;
  uint8_t control_mode_ = ANDROID_CONTROL_MODE_AUTO;
  uint8_t scene_mode_ = ANDROID_CONTROL_SCENE_MODE_DISABLED;
  uint8_t ae_mode_ = ANDROID_CONTROL_AE_MODE_ON;
  uint8_t awb_mode_ = ANDROID_CONTROL_AWB_MODE_AUTO;
  uint8_t af_mode_ = ANDROID_CONTROL_AF_MODE_AUTO;
  uint8_t ae_lock_ = ANDROID_CONTROL_AE_LOCK_OFF;
  uint8_t ae_state_ = ANDROID_CONTROL_AE_STATE_INACTIVE;
  uint8_t awb_state_ = ANDROID_CONTROL_AWB_STATE_INACTIVE;
  uint8_t awb_lock_ = ANDROID_CONTROL_AWB_LOCK_OFF;
  uint8_t af_state_ = ANDROID_CONTROL_AF_STATE_INACTIVE;
  uint8_t af_trigger_ = ANDROID_CONTROL_AF_TRIGGER_IDLE;
  uint8_t ae_trigger_ = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
  FPSRange ae_target_fps_ = {0, 0};
  float zoom_ratio_ = 1.0f;
  uint8_t extended_scene_mode_ = ANDROID_CONTROL_EXTENDED_SCENE_MODE_DISABLED;
  static const int32_t kMinimumStreamingFPS = 20;
  bool ae_lock_available_ = false;
  bool report_ae_lock_ = false;
  bool scenes_supported_ = false;
  size_t ae_frame_counter_ = 0;
  bool vstab_available_ = false;
  const size_t kAEPrecaptureMinFrames = 10;
  // Fake AE related constants
  const float kExposureTrackRate = .2f;  // This is the rate at which the fake
                                         // AE will reach the calculated target
  const size_t kStableAeMaxFrames =
      100;  // The number of frames the fake AE will stay in converged state
  // After fake AE switches to state searching the exposure
  // time will wander randomly in region defined by min/max below.
  const float kExposureWanderMin = -2;
  const float kExposureWanderMax = 1;
  const uint32_t kAETargetThreshold =
      10;  // Defines a threshold for reaching the AE target
  int32_t post_raw_boost_ = 100;
  bool report_post_raw_boost_ = false;
  nsecs_t ae_target_exposure_time_ = EmulatedSensor::kDefaultExposureTime;
  nsecs_t current_exposure_time_ = EmulatedSensor::kDefaultExposureTime;
  bool awb_lock_available_ = false;
  bool report_awb_lock_ = false;
  bool af_mode_changed_ = false;
  bool af_supported_ = false;
  bool picture_caf_supported_ = false;
  bool video_caf_supported_ = false;

  // android.flash.*
  bool is_flash_supported_ = false;
  uint8_t flash_state_ = ANDROID_FLASH_STATE_UNAVAILABLE;

  // android.sensor.*
  std::pair<int32_t, int32_t> sensor_sensitivity_range_;
  std::pair<nsecs_t, nsecs_t> sensor_exposure_time_range_;
  nsecs_t sensor_max_frame_duration_ =
      EmulatedSensor::kSupportedFrameDurationRange[1];
  nsecs_t sensor_exposure_time_ = EmulatedSensor::kDefaultExposureTime;
  nsecs_t sensor_frame_duration_ = EmulatedSensor::kDefaultFrameDuration;
  int32_t sensor_sensitivity_ = EmulatedSensor::kDefaultSensitivity;
  bool report_frame_duration_ = false;
  bool report_sensitivity_ = false;
  bool report_exposure_time_ = false;
  std::set<int32_t> available_test_pattern_modes_;
  bool report_rolling_shutter_skew_ = false;
  bool report_neutral_color_point_ = false;
  bool report_green_split_ = false;
  bool report_noise_profile_ = false;
  bool report_extended_scene_mode_ = false;

  // android.scaler.*
  bool report_rotate_and_crop_ = false;
  uint8_t rotate_and_crop_ = ANDROID_SCALER_ROTATE_AND_CROP_NONE;
  int32_t scaler_crop_region_default_[4] = {0, 0, 0, 0};
  std::set<uint8_t> available_rotate_crop_modes_;

  // android.statistics.*
  std::set<uint8_t> available_hot_pixel_map_modes_;
  std::set<uint8_t> available_lens_shading_map_modes_;
  std::set<uint8_t> available_face_detect_modes_;
  uint8_t current_scene_flicker_ = ANDROID_STATISTICS_SCENE_FLICKER_NONE;
  bool report_scene_flicker_ = false;

  // android.tonemap.*
  std::set<uint8_t> available_tonemap_modes_;

  // android.info.*
  uint8_t supported_hw_level_ = 0;
  static const size_t kTemplateCount =
      static_cast<size_t>(RequestTemplate::kManual) + 1;
  std::unique_ptr<HalCameraMetadata> default_requests_[kTemplateCount];
  // Set to true if the camera device has HW level FULL or LEVEL3
  bool is_level_full_or_higher_ = false;

  // android.lens.*
  float minimum_focus_distance_ = 0.f;
  float aperture_ = 0.f;
  float focal_length_ = 0.f;
  float focus_distance_ = 0.f;
  bool report_focus_distance_ = false;
  uint8_t lens_state_ = ANDROID_LENS_STATE_STATIONARY;
  bool report_focus_range_ = false;
  float filter_density_ = 0.f;
  bool report_filter_density_ = false;
  std::set<uint8_t> available_ois_modes_;
  uint8_t ois_mode_ = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
  bool report_ois_mode_ = false;
  float pose_rotation_[5] = {.0f};
  float pose_translation_[3] = {.0f};
  float distortion_[5] = {.0f};
  float intrinsic_calibration_[5] = {.0f};
  bool report_pose_rotation_ = false;
  bool report_pose_translation_ = false;
  bool report_distortion_ = false;
  bool report_intrinsic_calibration_ = false;
  int32_t shading_map_size_[2] = {0};

  unsigned int rand_seed_ = 1;

  // android.hotpixel.*
  std::set<uint8_t> available_hot_pixel_modes_;

  uint32_t camera_id_;

  EmulatedRequestState(const EmulatedRequestState&) = delete;
  EmulatedRequestState& operator=(const EmulatedRequestState&) = delete;
};

}  // namespace android

#endif  // EMULATOR_CAMERA_HAL_HWL_REQUEST_STATE_H
