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

#define LOG_TAG "EmulatedRequestState"
#define ATRACE_TAG ATRACE_TAG_CAMERA

#include "EmulatedRequestState.h"

#include <inttypes.h>
#include <log/log.h>
#include <utils/HWLUtils.h>

#include "EmulatedRequestProcessor.h"

namespace android {

using google_camera_hal::HwlPipelineResult;

const std::set<uint8_t> EmulatedRequestState::kSupportedCapabilites = {
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_DEPTH_OUTPUT,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING,
    ANDROID_REQUEST_AVAILABLE_CAPABILITIES_LOGICAL_MULTI_CAMERA,
};

const std::set<uint8_t> EmulatedRequestState::kSupportedHWLevels = {
    ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED,
    ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL,
    ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_3,
};

template <typename T>
T GetClosestValue(T val, T min, T max) {
  if ((min > max) || ((val >= min) && (val <= max))) {
    return val;
  } else if (val > max) {
    return max;
  } else {
    return min;
  }
}

status_t EmulatedRequestState::Update3AMeteringRegion(
    uint32_t tag, const HalCameraMetadata& settings, int32_t* region /*out*/) {
  if ((region == nullptr) || ((tag != ANDROID_CONTROL_AE_REGIONS) &&
                              (tag != ANDROID_CONTROL_AF_REGIONS) &&
                              (tag != ANDROID_CONTROL_AWB_REGIONS))) {
    return BAD_VALUE;
  }

  camera_metadata_ro_entry_t entry;
  auto ret = settings.Get(ANDROID_SCALER_CROP_REGION, &entry);
  if ((ret == OK) && (entry.count > 0)) {
    int32_t crop_region[4];
    crop_region[0] = entry.data.i32[0];
    crop_region[1] = entry.data.i32[1];
    crop_region[2] = entry.data.i32[2] + crop_region[0];
    crop_region[3] = entry.data.i32[3] + crop_region[1];
    ret = settings.Get(tag, &entry);
    if ((ret == OK) && (entry.count > 0)) {
      const int32_t* a_region = entry.data.i32;
      // calculate the intersection of 3A and CROP regions
      if (a_region[0] < crop_region[2] && crop_region[0] < a_region[2] &&
          a_region[1] < crop_region[3] && crop_region[1] < a_region[3]) {
        region[0] = std::max(a_region[0], crop_region[0]);
        region[1] = std::max(a_region[1], crop_region[1]);
        region[2] = std::min(a_region[2], crop_region[2]);
        region[3] = std::min(a_region[3], crop_region[3]);
        region[4] = entry.data.i32[4];
      }
    }
  }

  return OK;
}

status_t EmulatedRequestState::CompensateAE() {
  if (!exposure_compensation_supported_) {
    sensor_exposure_time_ = current_exposure_time_;
    return OK;
  }

  camera_metadata_ro_entry_t entry;
  auto ret =
      request_settings_->Get(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    exposure_compensation_ = entry.data.i32[0];
  } else {
    ALOGW("%s: AE compensation absent from request,  re-using previous value!",
          __FUNCTION__);
  }

  float ae_compensation = ::powf(
      2, exposure_compensation_ *
             ((static_cast<float>(exposure_compensation_step_.numerator) /
               exposure_compensation_step_.denominator)));

  sensor_exposure_time_ = GetClosestValue(
      static_cast<nsecs_t>(ae_compensation * current_exposure_time_),
      sensor_exposure_time_range_.first, sensor_exposure_time_range_.second);

  return OK;
}

status_t EmulatedRequestState::DoFakeAE() {
  camera_metadata_ro_entry_t entry;
  auto ret = request_settings_->Get(ANDROID_CONTROL_AE_LOCK, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    ae_lock_ = entry.data.u8[0];
  } else {
    ae_lock_ = ANDROID_CONTROL_AE_LOCK_OFF;
  }

  if (ae_lock_ == ANDROID_CONTROL_AE_LOCK_ON) {
    ae_state_ = ANDROID_CONTROL_AE_STATE_LOCKED;
    return OK;
  }

  FPSRange fps_range;
  ret = request_settings_->Get(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, &entry);
  if ((ret == OK) && (entry.count == 2)) {
    for (const auto& it : available_fps_ranges_) {
      if ((it.min_fps == entry.data.i32[0]) &&
          (it.max_fps == entry.data.i32[1])) {
        fps_range = {entry.data.i32[0], entry.data.i32[1]};
        break;
      }
    }
    if (fps_range.max_fps == 0) {
      ALOGE("%s: Unsupported framerate range [%d, %d]", __FUNCTION__,
            entry.data.i32[0], entry.data.i32[1]);
      return BAD_VALUE;
    }
  } else {
    fps_range = *available_fps_ranges_.begin();
  }

  ret = request_settings_->Get(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    ae_trigger_ = entry.data.u8[0];
  } else {
    ae_trigger_ = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
  }

  nsecs_t min_frame_duration =
      GetClosestValue(ms2ns(1000 / fps_range.max_fps),
                      EmulatedSensor::kSupportedFrameDurationRange[0],
                      sensor_max_frame_duration_);
  nsecs_t max_frame_duration =
      GetClosestValue(ms2ns(1000 / fps_range.min_fps),
                      EmulatedSensor::kSupportedFrameDurationRange[0],
                      sensor_max_frame_duration_);
  sensor_frame_duration_ = (max_frame_duration + min_frame_duration) / 2;

  // Face priority mode usually changes the AE algorithm behavior by
  // using the regions of interest associated with detected faces.
  // Try to emulate this behavior by slightly increasing the target exposure
  // time compared to normal operation.
  if (exposure_compensation_supported_) {
    float max_ae_compensation = ::powf(
        2, exposure_compensation_range_[1] *
               ((static_cast<float>(exposure_compensation_step_.numerator) /
                 exposure_compensation_step_.denominator)));
    ae_target_exposure_time_ = GetClosestValue(
        static_cast<nsecs_t>(sensor_frame_duration_ / max_ae_compensation),
        sensor_exposure_time_range_.first, sensor_exposure_time_range_.second);
  } else if (scene_mode_ == ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY) {
    ae_target_exposure_time_ = GetClosestValue(
        sensor_frame_duration_ / 4, sensor_exposure_time_range_.first,
        sensor_exposure_time_range_.second);
  } else {
    ae_target_exposure_time_ = GetClosestValue(
        sensor_frame_duration_ / 5, sensor_exposure_time_range_.first,
        sensor_exposure_time_range_.second);
  }

  if ((ae_trigger_ == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START) ||
      (ae_state_ == ANDROID_CONTROL_AE_STATE_PRECAPTURE)) {
    if (ae_state_ != ANDROID_CONTROL_AE_STATE_PRECAPTURE) {
      ae_frame_counter_ = 0;
    }

    if (ae_trigger_ == ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL) {
      // Done with precapture
      ae_frame_counter_ = 0;
      ae_state_ = ANDROID_CONTROL_AE_STATE_CONVERGED;
      ae_trigger_ = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_CANCEL;
    } else if ((ae_frame_counter_ > kAEPrecaptureMinFrames) &&
               (abs(ae_target_exposure_time_ - current_exposure_time_) <
                ae_target_exposure_time_ / kAETargetThreshold)) {
      // Done with precapture
      ae_frame_counter_ = 0;
      ae_state_ = ANDROID_CONTROL_AE_STATE_CONVERGED;
      ae_trigger_ = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
    } else {
      // Converge some more
      current_exposure_time_ +=
          (ae_target_exposure_time_ - current_exposure_time_) *
          kExposureTrackRate;
      ae_frame_counter_++;
      ae_state_ = ANDROID_CONTROL_AE_STATE_PRECAPTURE;
    }
  } else {
    switch (ae_state_) {
      case ANDROID_CONTROL_AE_STATE_INACTIVE:
        ae_state_ = ANDROID_CONTROL_AE_STATE_SEARCHING;
        break;
      case ANDROID_CONTROL_AE_STATE_CONVERGED:
        ae_frame_counter_++;
        if (ae_frame_counter_ > kStableAeMaxFrames) {
          float exposure_step = ((double)rand_r(&rand_seed_) / RAND_MAX) *
                                    (kExposureWanderMax - kExposureWanderMin) +
                                kExposureWanderMin;
          ae_target_exposure_time_ =
              GetClosestValue(static_cast<nsecs_t>(ae_target_exposure_time_ *
                                                   std::pow(2, exposure_step)),
                              sensor_exposure_time_range_.first,
                              sensor_exposure_time_range_.second);
          ae_state_ = ANDROID_CONTROL_AE_STATE_SEARCHING;
        }
        break;
      case ANDROID_CONTROL_AE_STATE_SEARCHING:
        current_exposure_time_ +=
            (ae_target_exposure_time_ - current_exposure_time_) *
            kExposureTrackRate;
        if (abs(ae_target_exposure_time_ - current_exposure_time_) <
            ae_target_exposure_time_ / kAETargetThreshold) {
          // Close enough
          ae_state_ = ANDROID_CONTROL_AE_STATE_CONVERGED;
          ae_frame_counter_ = 0;
        }
        break;
      case ANDROID_CONTROL_AE_STATE_LOCKED:
        ae_state_ = ANDROID_CONTROL_AE_STATE_CONVERGED;
        ae_frame_counter_ = 0;
        break;
      default:
        ALOGE("%s: Unexpected AE state %d!", __FUNCTION__, ae_state_);
        return INVALID_OPERATION;
    }
  }

  return OK;
}

status_t EmulatedRequestState::ProcessAWB() {
  if (max_awb_regions_ > 0) {
    auto ret = Update3AMeteringRegion(ANDROID_CONTROL_AWB_REGIONS,
                                      *request_settings_, awb_metering_region_);
    if (ret != OK) {
      return ret;
    }
  }
  if (((awb_mode_ == ANDROID_CONTROL_AWB_MODE_OFF) ||
       (control_mode_ == ANDROID_CONTROL_MODE_OFF)) &&
      supports_manual_post_processing_) {
    // TODO: Add actual manual support
  } else if (is_backward_compatible_) {
    camera_metadata_ro_entry_t entry;
    auto ret = request_settings_->Get(ANDROID_CONTROL_AWB_LOCK, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      awb_lock_ = entry.data.u8[0];
    } else {
      awb_lock_ = ANDROID_CONTROL_AWB_LOCK_OFF;
    }

    if (awb_lock_ == ANDROID_CONTROL_AWB_LOCK_ON) {
      awb_state_ = ANDROID_CONTROL_AWB_STATE_LOCKED;
    } else {
      awb_state_ = ANDROID_CONTROL_AWB_STATE_CONVERGED;
    }
  } else {
    // No color output support no need for AWB
  }

  return OK;
}

status_t EmulatedRequestState::ProcessAF() {
  camera_metadata_ro_entry entry;

  if (max_af_regions_ > 0) {
    auto ret = Update3AMeteringRegion(ANDROID_CONTROL_AF_REGIONS,
                                      *request_settings_, af_metering_region_);
    if (ret != OK) {
      return ret;
    }
  }
  if (af_mode_ == ANDROID_CONTROL_AF_MODE_OFF) {
    camera_metadata_ro_entry_t entry;
    auto ret = request_settings_->Get(ANDROID_LENS_FOCUS_DISTANCE, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if ((entry.data.f[0] >= 0.f) &&
          (entry.data.f[0] <= minimum_focus_distance_)) {
        focus_distance_ = entry.data.f[0];
      } else {
        ALOGE(
            "%s: Unsupported focus distance, It should be within "
            "[%5.2f, %5.2f]",
            __FUNCTION__, 0.f, minimum_focus_distance_);
      }
    }

    af_state_ = ANDROID_CONTROL_AF_STATE_INACTIVE;
    return OK;
  }

  auto ret = request_settings_->Get(ANDROID_CONTROL_AF_TRIGGER, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    af_trigger_ = entry.data.u8[0];
  } else {
    af_trigger_ = ANDROID_CONTROL_AF_TRIGGER_IDLE;
  }

  /**
   * Simulate AF triggers. Transition at most 1 state per frame.
   * - Focusing always succeeds (goes into locked, or PASSIVE_SCAN).
   */

  bool af_trigger_start = false;
  switch (af_trigger_) {
    case ANDROID_CONTROL_AF_TRIGGER_IDLE:
      break;
    case ANDROID_CONTROL_AF_TRIGGER_START:
      af_trigger_start = true;
      break;
    case ANDROID_CONTROL_AF_TRIGGER_CANCEL:
      // Cancel trigger always transitions into INACTIVE
      af_state_ = ANDROID_CONTROL_AF_STATE_INACTIVE;

      // Stay in 'inactive' until at least next frame
      return OK;
    default:
      ALOGE("%s: Unknown AF trigger value", __FUNCTION__);
      return BAD_VALUE;
  }

  // If we get down here, we're either in ANDROID_CONTROL_AF_MODE_AUTO,
  // ANDROID_CONTROL_AF_MODE_MACRO, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
  // ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE and no other modes like
  // ANDROID_CONTROL_AF_MODE_OFF or ANDROID_CONTROL_AF_MODE_EDOF
  switch (af_state_) {
    case ANDROID_CONTROL_AF_STATE_INACTIVE:
      if (af_trigger_start) {
        switch (af_mode_) {
          case ANDROID_CONTROL_AF_MODE_AUTO:
            // fall-through
          case ANDROID_CONTROL_AF_MODE_MACRO:
            af_state_ = ANDROID_CONTROL_AF_STATE_ACTIVE_SCAN;
            break;
          case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
            // fall-through
          case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            af_state_ = ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED;
            break;
        }
      } else {
        // At least one frame stays in INACTIVE
        if (!af_mode_changed_) {
          switch (af_mode_) {
            case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
              // fall-through
            case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
              af_state_ = ANDROID_CONTROL_AF_STATE_PASSIVE_SCAN;
              break;
          }
        }
      }
      break;
    case ANDROID_CONTROL_AF_STATE_PASSIVE_SCAN:
      /**
       * When the AF trigger is activated, the algorithm should finish
       * its PASSIVE_SCAN if active, and then transition into AF_FOCUSED
       * or AF_NOT_FOCUSED as appropriate
       */
      if (af_trigger_start) {
        // Randomly transition to focused or not focused
        if (rand_r(&rand_seed_) % 3) {
          af_state_ = ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
        } else {
          af_state_ = ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED;
        }
      }
      /**
       * When the AF trigger is not involved, the AF algorithm should
       * start in INACTIVE state, and then transition into PASSIVE_SCAN
       * and PASSIVE_FOCUSED states
       */
      else {
        // Randomly transition to passive focus
        if (rand_r(&rand_seed_) % 3 == 0) {
          af_state_ = ANDROID_CONTROL_AF_STATE_PASSIVE_FOCUSED;
        }
      }

      break;
    case ANDROID_CONTROL_AF_STATE_PASSIVE_FOCUSED:
      if (af_trigger_start) {
        // Randomly transition to focused or not focused
        if (rand_r(&rand_seed_) % 3) {
          af_state_ = ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
        } else {
          af_state_ = ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED;
        }
      }
      // TODO: initiate passive scan (PASSIVE_SCAN)
      break;
    case ANDROID_CONTROL_AF_STATE_ACTIVE_SCAN:
      // Simulate AF sweep completing instantaneously

      // Randomly transition to focused or not focused
      if (rand_r(&rand_seed_) % 3) {
        af_state_ = ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED;
      } else {
        af_state_ = ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED;
      }
      break;
    case ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED:
      if (af_trigger_start) {
        switch (af_mode_) {
          case ANDROID_CONTROL_AF_MODE_AUTO:
            // fall-through
          case ANDROID_CONTROL_AF_MODE_MACRO:
            af_state_ = ANDROID_CONTROL_AF_STATE_ACTIVE_SCAN;
            break;
          case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
            // fall-through
          case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            // continuous autofocus => trigger start has no effect
            break;
        }
      }
      break;
    case ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED:
      if (af_trigger_start) {
        switch (af_mode_) {
          case ANDROID_CONTROL_AF_MODE_AUTO:
            // fall-through
          case ANDROID_CONTROL_AF_MODE_MACRO:
            af_state_ = ANDROID_CONTROL_AF_STATE_ACTIVE_SCAN;
            break;
          case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
            // fall-through
          case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            // continuous autofocus => trigger start has no effect
            break;
        }
      }
      break;
    default:
      ALOGE("%s: Bad af state %d", __FUNCTION__, af_state_);
  }

  return OK;
}

status_t EmulatedRequestState::ProcessAE() {
  if (max_ae_regions_ > 0) {
    auto ret = Update3AMeteringRegion(ANDROID_CONTROL_AE_REGIONS,
                                      *request_settings_, ae_metering_region_);
    if (ret != OK) {
      ALOGE("%s: Failed updating the 3A metering regions: %d, (%s)",
            __FUNCTION__, ret, strerror(-ret));
    }
  }

  camera_metadata_ro_entry_t entry;
  bool auto_ae_mode = false;
  bool auto_ae_flash_mode = false;
  switch (ae_mode_) {
    case ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH:
    case ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH:
    case ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE:
      auto_ae_flash_mode = true;
      [[fallthrough]];
    case ANDROID_CONTROL_AE_MODE_ON:
      auto_ae_mode = true;
  };
  if (((ae_mode_ == ANDROID_CONTROL_AE_MODE_OFF) ||
       (control_mode_ == ANDROID_CONTROL_MODE_OFF)) &&
      supports_manual_sensor_) {
    auto ret = request_settings_->Get(ANDROID_SENSOR_EXPOSURE_TIME, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if ((entry.data.i64[0] >= sensor_exposure_time_range_.first) &&
          (entry.data.i64[0] <= sensor_exposure_time_range_.second)) {
        sensor_exposure_time_ = entry.data.i64[0];
      } else {
        ALOGE(
            "%s: Sensor exposure time"
            " not within supported range[%" PRId64 ", %" PRId64 "]",
            __FUNCTION__, sensor_exposure_time_range_.first,
            sensor_exposure_time_range_.second);
        // Use last valid value
      }
    }

    ret = request_settings_->Get(ANDROID_SENSOR_FRAME_DURATION, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if ((entry.data.i64[0] >=
           EmulatedSensor::kSupportedFrameDurationRange[0]) &&
          (entry.data.i64[0] <= sensor_max_frame_duration_)) {
        sensor_frame_duration_ = entry.data.i64[0];
      } else {
        ALOGE(
            "%s: Sensor frame duration "
            " not within supported range[%" PRId64 ", %" PRId64 "]",
            __FUNCTION__, EmulatedSensor::kSupportedFrameDurationRange[0],
            sensor_max_frame_duration_);
        // Use last valid value
      }
    }

    if (sensor_frame_duration_ < sensor_exposure_time_) {
      sensor_frame_duration_ = sensor_exposure_time_;
    }

    ret = request_settings_->Get(ANDROID_SENSOR_SENSITIVITY, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if ((entry.data.i32[0] >= sensor_sensitivity_range_.first) &&
          (entry.data.i32[0] <= sensor_sensitivity_range_.second)) {
        sensor_sensitivity_ = entry.data.i32[0];
      } else {
        ALOGE("%s: Sensor sensitivity not within supported range[%d, %d]",
              __FUNCTION__, sensor_sensitivity_range_.first,
              sensor_sensitivity_range_.second);
        // Use last valid value
      }
    }
    ae_state_ = ANDROID_CONTROL_AE_STATE_INACTIVE;
  } else if (is_backward_compatible_ && auto_ae_mode) {
    auto ret = DoFakeAE();
    if (ret != OK) {
      ALOGE("%s: Failed fake AE: %d, (%s)", __FUNCTION__, ret, strerror(-ret));
    }

    // Do AE compensation on the results of the AE
    ret = CompensateAE();
    if (ret != OK) {
      ALOGE("%s: Failed during AE compensation: %d, (%s)", __FUNCTION__, ret,
            strerror(-ret));
    }
  } else {
    ALOGI(
        "%s: No emulation for current AE mode using previous sensor settings!",
        __FUNCTION__);
  }

  if (is_flash_supported_) {
    flash_state_ = ANDROID_FLASH_STATE_READY;
    // Flash fires only if the request manually enables it (SINGLE/TORCH)
    // and the appropriate AE mode is set or during still capture with auto
    // flash AE modes.
    bool manual_flash_mode = false;
    auto ret = request_settings_->Get(ANDROID_FLASH_MODE, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if ((entry.data.u8[0] == ANDROID_FLASH_MODE_SINGLE) ||
          (entry.data.u8[0] == ANDROID_FLASH_MODE_TORCH)) {
        manual_flash_mode = true;
      }
    }
    if (manual_flash_mode && !auto_ae_flash_mode) {
      flash_state_ = ANDROID_FLASH_STATE_FIRED;
    } else {
      bool is_still_capture = false;
      ret = request_settings_->Get(ANDROID_CONTROL_CAPTURE_INTENT, &entry);
      if ((ret == OK) && (entry.count == 1)) {
        if (entry.data.u8[0] == ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE) {
          is_still_capture = true;
        }
      }
      if (is_still_capture && auto_ae_flash_mode) {
        flash_state_ = ANDROID_FLASH_STATE_FIRED;
      }
    }
  } else {
    flash_state_ = ANDROID_FLASH_STATE_UNAVAILABLE;
  }

  return OK;
}

status_t EmulatedRequestState::InitializeSensorSettings(
    std::unique_ptr<HalCameraMetadata> request_settings,
    EmulatedSensor::SensorSettings* sensor_settings /*out*/) {
  if ((sensor_settings == nullptr) || (request_settings.get() == nullptr)) {
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(request_state_mutex_);
  request_settings_ = std::move(request_settings);
  camera_metadata_ro_entry_t entry;
  auto ret = request_settings_->Get(ANDROID_CONTROL_MODE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (available_control_modes_.find(entry.data.u8[0]) !=
        available_control_modes_.end()) {
      control_mode_ = entry.data.u8[0];
    } else {
      ALOGE("%s: Unsupported control mode!", __FUNCTION__);
      return BAD_VALUE;
    }
  }

  ret = request_settings_->Get(ANDROID_CONTROL_SCENE_MODE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    // Disabled scene is not expected to be among the available scene list
    if ((entry.data.u8[0] == ANDROID_CONTROL_SCENE_MODE_DISABLED) ||
        (available_scenes_.find(entry.data.u8[0]) != available_scenes_.end())) {
      scene_mode_ = entry.data.u8[0];
    } else {
      ALOGE("%s: Unsupported scene mode!", __FUNCTION__);
      return BAD_VALUE;
    }
  }

  float min_zoom = min_zoom_, max_zoom = max_zoom_;
  ret = request_settings_->Get(ANDROID_CONTROL_EXTENDED_SCENE_MODE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    bool extended_scene_mode_valid = false;
    for (const auto& cap : available_extended_scene_mode_caps_) {
      if (cap.mode == entry.data.u8[0]) {
        extended_scene_mode_ = entry.data.u8[0];
        min_zoom = cap.min_zoom;
        max_zoom = cap.max_zoom;
        extended_scene_mode_valid = true;
        break;
      }
    }
    if (!extended_scene_mode_valid) {
      ALOGE("%s: Unsupported extended scene mode %d!", __FUNCTION__,
            entry.data.u8[0]);
      return BAD_VALUE;
    }
    if (extended_scene_mode_ != ANDROID_CONTROL_EXTENDED_SCENE_MODE_DISABLED) {
      scene_mode_ = ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY;
    }
  }

  // Check zoom ratio range and override to supported range
  ret = request_settings_->Get(ANDROID_CONTROL_ZOOM_RATIO, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    zoom_ratio_ = std::min(std::max(entry.data.f[0], min_zoom), max_zoom);
  }

  // Check rotate_and_crop setting
  ret = request_settings_->Get(ANDROID_SCALER_ROTATE_AND_CROP, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (available_rotate_crop_modes_.find(entry.data.u8[0]) !=
        available_rotate_crop_modes_.end()) {
      rotate_and_crop_ = entry.data.u8[0];
    } else {
      ALOGE("%s: Unsupported rotate and crop mode: %u", __FUNCTION__, entry.data.u8[0]);
      return BAD_VALUE;
    }
  }

  // Check video stabilization parameter
  uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
  ret = request_settings_->Get(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (available_vstab_modes_.find(entry.data.u8[0]) !=
      available_vstab_modes_.end()) {
      vstab_mode = entry.data.u8[0];
    } else {
      ALOGE("%s: Unsupported video stabilization mode: %u! Video stabilization will be disabled!",
            __FUNCTION__, entry.data.u8[0]);
    }
  }

  // Check video stabilization parameter
  uint8_t edge_mode = ANDROID_EDGE_MODE_OFF;
  ret = request_settings_->Get(ANDROID_EDGE_MODE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (available_edge_modes_.find(entry.data.u8[0]) !=
      available_edge_modes_.end()) {
      edge_mode = entry.data.u8[0];
    } else {
      ALOGE("%s: Unsupported edge mode: %u", __FUNCTION__, entry.data.u8[0]);
      return BAD_VALUE;
    }
  }

  // 3A modes are active in case the scene is disabled or set to face priority
  // or the control mode is not using scenes
  if ((scene_mode_ == ANDROID_CONTROL_SCENE_MODE_DISABLED) ||
      (scene_mode_ == ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY) ||
      (control_mode_ != ANDROID_CONTROL_MODE_USE_SCENE_MODE)) {
    ret = request_settings_->Get(ANDROID_CONTROL_AE_MODE, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if (available_ae_modes_.find(entry.data.u8[0]) !=
          available_ae_modes_.end()) {
        ae_mode_ = entry.data.u8[0];
      } else {
        ALOGE("%s: Unsupported AE mode! Using last valid mode!", __FUNCTION__);
      }
    }

    ret = request_settings_->Get(ANDROID_CONTROL_AWB_MODE, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if (available_awb_modes_.find(entry.data.u8[0]) !=
          available_awb_modes_.end()) {
        awb_mode_ = entry.data.u8[0];
      } else {
        ALOGE("%s: Unsupported AWB mode! Using last valid mode!", __FUNCTION__);
      }
    }

    ret = request_settings_->Get(ANDROID_CONTROL_AF_MODE, &entry);
    if ((ret == OK) && (entry.count == 1)) {
      if (available_af_modes_.find(entry.data.u8[0]) !=
          available_af_modes_.end()) {
        af_mode_changed_ = af_mode_ != entry.data.u8[0];
        af_mode_ = entry.data.u8[0];
      } else {
        ALOGE("%s: Unsupported AF mode! Using last valid mode!", __FUNCTION__);
      }
    }
  } else {
    auto it = scene_overrides_.find(scene_mode_);
    if (it != scene_overrides_.end()) {
      ae_mode_ = it->second.ae_mode;
      awb_mode_ = it->second.awb_mode;
      af_mode_changed_ = af_mode_ != entry.data.u8[0];
      af_mode_ = it->second.af_mode;
    } else {
      ALOGW(
          "%s: Current scene has no overrides! Using the currently active 3A "
          "modes!",
          __FUNCTION__);
    }
  }

  ret = ProcessAE();
  if (ret != OK) {
    return ret;
  }

  ret = ProcessAWB();
  if (ret != OK) {
    return ret;
  }

  ret = ProcessAF();
  if (ret != OK) {
    return ret;
  }

  ret = request_settings_->Get(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (available_lens_shading_map_modes_.find(entry.data.u8[0]) !=
        available_lens_shading_map_modes_.end()) {
      sensor_settings->lens_shading_map_mode = entry.data.u8[0];
    } else {
      ALOGE("%s: Unsupported lens shading map mode!", __FUNCTION__);
    }
  }

  sensor_settings->exposure_time = sensor_exposure_time_;
  sensor_settings->frame_duration = sensor_frame_duration_;
  sensor_settings->gain = sensor_sensitivity_;
  sensor_settings->report_neutral_color_point = report_neutral_color_point_;
  sensor_settings->report_green_split = report_green_split_;
  sensor_settings->report_noise_profile = report_noise_profile_;
  sensor_settings->zoom_ratio = zoom_ratio_;
  sensor_settings->report_rotate_and_crop = report_rotate_and_crop_;
  sensor_settings->rotate_and_crop = rotate_and_crop_;
  sensor_settings->report_video_stab = !available_vstab_modes_.empty();
  sensor_settings->video_stab = vstab_mode;
  sensor_settings->report_edge_mode = report_edge_mode_;
  sensor_settings->edge_mode = edge_mode;

  return OK;
}

std::unique_ptr<HwlPipelineResult> EmulatedRequestState::InitializeResult(
    uint32_t pipeline_id, uint32_t frame_number) {
  std::lock_guard<std::mutex> lock(request_state_mutex_);
  auto result = std::make_unique<HwlPipelineResult>();
  result->camera_id = camera_id_;
  result->pipeline_id = pipeline_id;
  result->frame_number = frame_number;
  result->result_metadata = HalCameraMetadata::Clone(request_settings_.get());
  result->partial_result = partial_result_count_;

  // Results supported on all emulated devices
  result->result_metadata->Set(ANDROID_REQUEST_PIPELINE_DEPTH,
                               &max_pipeline_depth_, 1);
  result->result_metadata->Set(ANDROID_CONTROL_MODE, &control_mode_, 1);
  result->result_metadata->Set(ANDROID_CONTROL_AF_MODE, &af_mode_, 1);
  result->result_metadata->Set(ANDROID_CONTROL_AF_STATE, &af_state_, 1);
  result->result_metadata->Set(ANDROID_CONTROL_AWB_MODE, &awb_mode_, 1);
  result->result_metadata->Set(ANDROID_CONTROL_AWB_STATE, &awb_state_, 1);
  result->result_metadata->Set(ANDROID_CONTROL_AE_MODE, &ae_mode_, 1);
  result->result_metadata->Set(ANDROID_CONTROL_AE_STATE, &ae_state_, 1);
  int32_t fps_range[] = {ae_target_fps_.min_fps, ae_target_fps_.max_fps};
  result->result_metadata->Set(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range,
                               ARRAY_SIZE(fps_range));
  result->result_metadata->Set(ANDROID_FLASH_STATE, &flash_state_, 1);
  result->result_metadata->Set(ANDROID_LENS_STATE, &lens_state_, 1);

  // Results depending on device capability and features
  if (is_backward_compatible_) {
    result->result_metadata->Set(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                                 &ae_trigger_, 1);
    result->result_metadata->Set(ANDROID_CONTROL_AF_TRIGGER, &af_trigger_, 1);
    uint8_t vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    result->result_metadata->Set(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                                 &vstab_mode, 1);
    if (exposure_compensation_supported_) {
      result->result_metadata->Set(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                                   &exposure_compensation_, 1);
    }
  }
  if (ae_lock_available_ && report_ae_lock_) {
    result->result_metadata->Set(ANDROID_CONTROL_AE_LOCK, &ae_lock_, 1);
  }
  if (awb_lock_available_ && report_awb_lock_) {
    result->result_metadata->Set(ANDROID_CONTROL_AWB_LOCK, &awb_lock_, 1);
  }
  if (scenes_supported_) {
    result->result_metadata->Set(ANDROID_CONTROL_SCENE_MODE, &scene_mode_, 1);
  }
  if (max_ae_regions_ > 0) {
    result->result_metadata->Set(ANDROID_CONTROL_AE_REGIONS, ae_metering_region_,
                                 ARRAY_SIZE(ae_metering_region_));
  }
  if (max_awb_regions_ > 0) {
    result->result_metadata->Set(ANDROID_CONTROL_AWB_REGIONS,
                                 awb_metering_region_,
                                 ARRAY_SIZE(awb_metering_region_));
  }
  if (max_af_regions_ > 0) {
    result->result_metadata->Set(ANDROID_CONTROL_AF_REGIONS, af_metering_region_,
                                 ARRAY_SIZE(af_metering_region_));
  }
  if (report_exposure_time_) {
    result->result_metadata->Set(ANDROID_SENSOR_EXPOSURE_TIME,
                                 &sensor_exposure_time_, 1);
  }
  if (report_frame_duration_) {
    result->result_metadata->Set(ANDROID_SENSOR_FRAME_DURATION,
                                 &sensor_frame_duration_, 1);
  }
  if (report_sensitivity_) {
    result->result_metadata->Set(ANDROID_SENSOR_SENSITIVITY,
                                 &sensor_sensitivity_, 1);
  }
  if (report_rolling_shutter_skew_) {
    result->result_metadata->Set(
        ANDROID_SENSOR_ROLLING_SHUTTER_SKEW,
        &EmulatedSensor::kSupportedFrameDurationRange[0], 1);
  }
  if (report_post_raw_boost_) {
    result->result_metadata->Set(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST,
                                 &post_raw_boost_, 1);
  }
  if (report_focus_distance_) {
    result->result_metadata->Set(ANDROID_LENS_FOCUS_DISTANCE, &focus_distance_,
                                 1);
  }
  if (report_focus_range_) {
    float focus_range[2] = {0.f};
    if (minimum_focus_distance_ > .0f) {
      focus_range[0] = 1 / minimum_focus_distance_;
    }
    result->result_metadata->Set(ANDROID_LENS_FOCUS_RANGE, focus_range,
                                 ARRAY_SIZE(focus_range));
  }
  if (report_filter_density_) {
    result->result_metadata->Set(ANDROID_LENS_FILTER_DENSITY, &filter_density_,
                                 1);
  }
  if (report_ois_mode_) {
    result->result_metadata->Set(ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                                 &ois_mode_, 1);
  }
  if (report_pose_rotation_) {
    result->result_metadata->Set(ANDROID_LENS_POSE_ROTATION, pose_rotation_,
                                 ARRAY_SIZE(pose_rotation_));
  }
  if (report_pose_translation_) {
    result->result_metadata->Set(ANDROID_LENS_POSE_TRANSLATION,
                                 pose_translation_,
                                 ARRAY_SIZE(pose_translation_));
  }
  if (report_intrinsic_calibration_) {
    result->result_metadata->Set(ANDROID_LENS_INTRINSIC_CALIBRATION,
                                 intrinsic_calibration_,
                                 ARRAY_SIZE(intrinsic_calibration_));
  }
  if (report_distortion_) {
    result->result_metadata->Set(ANDROID_LENS_DISTORTION, distortion_,
                                 ARRAY_SIZE(distortion_));
  }
  if (report_black_level_lock_) {
    result->result_metadata->Set(ANDROID_BLACK_LEVEL_LOCK, &black_level_lock_,
                                 1);
  }
  if (report_scene_flicker_) {
    result->result_metadata->Set(ANDROID_STATISTICS_SCENE_FLICKER,
                                 &current_scene_flicker_, 1);
  }
  if (zoom_ratio_supported_) {
    result->result_metadata->Set(ANDROID_CONTROL_ZOOM_RATIO, &zoom_ratio_, 1);
  }
  if (report_extended_scene_mode_) {
    result->result_metadata->Set(ANDROID_CONTROL_EXTENDED_SCENE_MODE,
                                 &extended_scene_mode_, 1);
  }
  return result;
}

bool EmulatedRequestState::SupportsCapability(uint8_t cap) {
  return available_capabilities_.find(cap) != available_capabilities_.end();
}

status_t EmulatedRequestState::InitializeSensorDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret =
      static_metadata_->Get(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE, &entry);
  if ((ret == OK) && (entry.count == 2)) {
    sensor_sensitivity_range_ =
        std::make_pair(entry.data.i32[0], entry.data.i32[1]);
  } else if (!supports_manual_sensor_) {
    sensor_sensitivity_range_ =
        std::make_pair(EmulatedSensor::kSupportedSensitivityRange[0],
                       EmulatedSensor::kSupportedSensitivityRange[1]);
  } else {
    ALOGE("%s: Manual sensor devices must advertise sensor sensitivity range!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE, &entry);
  if ((ret == OK) && (entry.count == 2)) {
    sensor_exposure_time_range_ =
        std::make_pair(entry.data.i64[0], entry.data.i64[1]);
  } else if (!supports_manual_sensor_) {
    sensor_exposure_time_range_ =
        std::make_pair(EmulatedSensor::kSupportedExposureTimeRange[0],
                       EmulatedSensor::kSupportedExposureTimeRange[1]);
  } else {
    ALOGE(
        "%s: Manual sensor devices must advertise sensor exposure time range!",
        __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    sensor_max_frame_duration_ = entry.data.i64[0];
  } else if (!supports_manual_sensor_) {
    sensor_max_frame_duration_ = EmulatedSensor::kSupportedFrameDurationRange[1];
  } else {
    ALOGE("%s: Manual sensor devices must advertise sensor max frame duration!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  if (supports_manual_sensor_) {
    if (available_requests_.find(ANDROID_SENSOR_SENSITIVITY) ==
        available_requests_.end()) {
      ALOGE(
          "%s: Sensor sensitivity must be configurable on manual sensor "
          "devices!",
          __FUNCTION__);
      return BAD_VALUE;
    }

    if (available_requests_.find(ANDROID_SENSOR_EXPOSURE_TIME) ==
        available_requests_.end()) {
      ALOGE(
          "%s: Sensor exposure time must be configurable on manual sensor "
          "devices!",
          __FUNCTION__);
      return BAD_VALUE;
    }

    if (available_requests_.find(ANDROID_SENSOR_FRAME_DURATION) ==
        available_requests_.end()) {
      ALOGE(
          "%s: Sensor frame duration must be configurable on manual sensor "
          "devices!",
          __FUNCTION__);
      return BAD_VALUE;
    }
  }

  report_rolling_shutter_skew_ =
      available_results_.find(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW) !=
      available_results_.end();
  report_sensitivity_ = available_results_.find(ANDROID_SENSOR_SENSITIVITY) !=
                        available_results_.end();
  report_exposure_time_ =
      available_results_.find(ANDROID_SENSOR_EXPOSURE_TIME) !=
      available_results_.end();
  report_frame_duration_ =
      available_results_.find(ANDROID_SENSOR_FRAME_DURATION) !=
      available_results_.end();
  report_neutral_color_point_ =
      available_results_.find(ANDROID_SENSOR_NEUTRAL_COLOR_POINT) !=
      available_results_.end();
  report_green_split_ = available_results_.find(ANDROID_SENSOR_GREEN_SPLIT) !=
                        available_results_.end();
  report_noise_profile_ =
      available_results_.find(ANDROID_SENSOR_NOISE_PROFILE) !=
      available_results_.end();

  if (is_raw_capable_ && !report_green_split_) {
    ALOGE("%s: RAW capable devices must be able to report the noise profile!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  if (is_raw_capable_ && !report_neutral_color_point_) {
    ALOGE(
        "%s: RAW capable devices must be able to report the neutral color "
        "point!",
        __FUNCTION__);
    return BAD_VALUE;
  }

  if (is_raw_capable_ && !report_green_split_) {
    ALOGE("%s: RAW capable devices must be able to report the green split!",
          __FUNCTION__);
    return BAD_VALUE;
  }
  if (available_results_.find(ANDROID_SENSOR_TIMESTAMP) ==
      available_results_.end()) {
    ALOGE("%s: Sensor timestamp must always be part of the results!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES,
                              &entry);
  if (ret == OK) {
    available_test_pattern_modes_.insert(entry.data.i32,
                                         entry.data.i32 + entry.count);
  } else {
    ALOGE("%s: No available test pattern modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  sensor_exposure_time_ = GetClosestValue(EmulatedSensor::kDefaultExposureTime,
                                          sensor_exposure_time_range_.first,
                                          sensor_exposure_time_range_.second);
  sensor_frame_duration_ =
      GetClosestValue(EmulatedSensor::kDefaultFrameDuration,
                      EmulatedSensor::kSupportedFrameDurationRange[0],
                      sensor_max_frame_duration_);
  sensor_sensitivity_ = GetClosestValue(EmulatedSensor::kDefaultSensitivity,
                                        sensor_sensitivity_range_.first,
                                        sensor_sensitivity_range_.second);

  bool off_test_pattern_mode_supported =
      available_test_pattern_modes_.find(ANDROID_SENSOR_TEST_PATTERN_MODE_OFF) !=
      available_test_pattern_modes_.end();
  int32_t test_pattern_mode = (off_test_pattern_mode_supported)
                                  ? ANDROID_SENSOR_TEST_PATTERN_MODE_OFF
                                  : *available_test_pattern_modes_.begin();
  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    default_requests_[idx]->Set(ANDROID_SENSOR_EXPOSURE_TIME,
                                &sensor_exposure_time_, 1);
    default_requests_[idx]->Set(ANDROID_SENSOR_FRAME_DURATION,
                                &sensor_frame_duration_, 1);
    default_requests_[idx]->Set(ANDROID_SENSOR_SENSITIVITY,
                                &sensor_sensitivity_, 1);
    default_requests_[idx]->Set(ANDROID_SENSOR_TEST_PATTERN_MODE,
                                &test_pattern_mode, 1);
  }

  return OK;
}

status_t EmulatedRequestState::InitializeStatisticsDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(
      ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES, &entry);
  if (ret == OK) {
    available_face_detect_modes_.insert(entry.data.u8,
                                        entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available face detect modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(
      ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES, &entry);
  if (ret == OK) {
    available_lens_shading_map_modes_.insert(entry.data.u8,
                                             entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available lens shading modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(
      ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES, &entry);
  if (ret == OK) {
    available_hot_pixel_map_modes_.insert(entry.data.u8,
                                          entry.data.u8 + entry.count);
  } else if (is_raw_capable_) {
    ALOGE("%s: RAW capable device must support hot pixel map modes!",
          __FUNCTION__);
    return BAD_VALUE;
  } else {
    available_hot_pixel_map_modes_.emplace(
        ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF);
  }

  bool hot_pixel_mode_off_supported =
      available_hot_pixel_map_modes_.find(
          ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF) !=
      available_hot_pixel_map_modes_.end();
  bool face_detect_mode_off_supported =
      available_face_detect_modes_.find(
          ANDROID_STATISTICS_FACE_DETECT_MODE_OFF) !=
      available_face_detect_modes_.end();
  bool lens_shading_map_mode_off_supported =
      available_lens_shading_map_modes_.find(
          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_ON) !=
      available_lens_shading_map_modes_.end();
  bool lens_shading_map_mode_on_supported =
      available_lens_shading_map_modes_.find(
          ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_ON) !=
      available_lens_shading_map_modes_.end();
  if (is_raw_capable_ && !lens_shading_map_mode_on_supported) {
    ALOGE("%s: RAW capable device must support lens shading map reporting!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  if (lens_shading_map_mode_on_supported &&
      (available_results_.find(ANDROID_STATISTICS_LENS_SHADING_MAP) ==
       available_results_.end())) {
    ALOGE(
        "%s: Lens shading map reporting available but corresponding result key "
        "is absent!",
        __FUNCTION__);
    return BAD_VALUE;
  }

  if (lens_shading_map_mode_on_supported &&
      ((shading_map_size_[0] == 0) || (shading_map_size_[1] == 0))) {
    ALOGE(
        "%s: Lens shading map reporting available but without valid shading "
        "map size!",
        __FUNCTION__);
    return BAD_VALUE;
  }

  report_scene_flicker_ =
      available_results_.find(ANDROID_STATISTICS_SCENE_FLICKER) !=
      available_results_.end();

  uint8_t face_detect_mode = face_detect_mode_off_supported
                                 ? ANDROID_STATISTICS_FACE_DETECT_MODE_OFF
                                 : *available_face_detect_modes_.begin();
  uint8_t hot_pixel_map_mode = hot_pixel_mode_off_supported
                                   ? ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE_OFF
                                   : *available_hot_pixel_map_modes_.begin();
  uint8_t lens_shading_map_mode =
      lens_shading_map_mode_off_supported
          ? ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF
          : *available_lens_shading_map_modes_.begin();
  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    if ((static_cast<RequestTemplate>(idx) == RequestTemplate::kStillCapture) &&
        is_raw_capable_ && lens_shading_map_mode_on_supported) {
      uint8_t lens_shading_map_on = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_ON;
      default_requests_[idx]->Set(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                                  &lens_shading_map_on, 1);
    } else {
      default_requests_[idx]->Set(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
                                  &lens_shading_map_mode, 1);
    }

    default_requests_[idx]->Set(ANDROID_STATISTICS_FACE_DETECT_MODE,
                                &face_detect_mode, 1);
    default_requests_[idx]->Set(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
                                &hot_pixel_map_mode, 1);
  }

  return InitializeBlackLevelDefaults();
}

status_t EmulatedRequestState::InitializeControlSceneDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret =
      static_metadata_->Get(ANDROID_CONTROL_AVAILABLE_SCENE_MODES, &entry);
  if (ret == OK) {
    available_scenes_.insert(entry.data.u8, entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available scene modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  if ((entry.count == 1) &&
      (entry.data.u8[0] == ANDROID_CONTROL_SCENE_MODE_DISABLED)) {
    scenes_supported_ = false;
    return OK;
  } else {
    scenes_supported_ = true;
  }

  if (available_requests_.find(ANDROID_CONTROL_SCENE_MODE) ==
      available_requests_.end()) {
    ALOGE("%s: Scene mode cannot be set!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_SCENE_MODE) ==
      available_results_.end()) {
    ALOGE("%s: Scene mode cannot be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry_t overrides_entry;
  ret = static_metadata_->Get(ANDROID_CONTROL_SCENE_MODE_OVERRIDES,
                              &overrides_entry);
  if ((ret == OK) && ((overrides_entry.count / 3) == available_scenes_.size()) &&
      ((overrides_entry.count % 3) == 0)) {
    for (size_t i = 0; i < entry.count; i += 3) {
      SceneOverride scene(overrides_entry.data.u8[i],
                          overrides_entry.data.u8[i + 1],
                          overrides_entry.data.u8[i + 2]);
      if (available_ae_modes_.find(scene.ae_mode) == available_ae_modes_.end()) {
        ALOGE("%s: AE scene mode override: %d not supported!", __FUNCTION__,
              scene.ae_mode);
        return BAD_VALUE;
      }
      if (available_awb_modes_.find(scene.awb_mode) ==
          available_awb_modes_.end()) {
        ALOGE("%s: AWB scene mode override: %d not supported!", __FUNCTION__,
              scene.awb_mode);
        return BAD_VALUE;
      }
      if (available_af_modes_.find(scene.af_mode) == available_af_modes_.end()) {
        ALOGE("%s: AF scene mode override: %d not supported!", __FUNCTION__,
              scene.af_mode);
        return BAD_VALUE;
      }
      scene_overrides_.emplace(entry.data.u8[i], scene);
    }
  } else {
    ALOGE("%s: No available scene overrides!", __FUNCTION__);
    return BAD_VALUE;
  }

  return OK;
}

status_t EmulatedRequestState::InitializeControlAFDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(ANDROID_CONTROL_AF_AVAILABLE_MODES, &entry);
  if (ret == OK) {
    available_af_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available AF modes!", __FUNCTION__);
    return BAD_VALUE;
  }
  // Off mode must always be present
  if (available_af_modes_.find(ANDROID_CONTROL_AF_MODE_OFF) ==
      available_af_modes_.end()) {
    ALOGE("%s: AF off control mode must always be present!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_requests_.find(ANDROID_CONTROL_AF_MODE) ==
      available_requests_.end()) {
    ALOGE("%s: Clients must be able to set AF mode!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_requests_.find(ANDROID_CONTROL_AF_TRIGGER) ==
      available_requests_.end()) {
    ALOGE("%s: Clients must be able to set AF trigger!", __FUNCTION__);
    return BAD_VALUE;
  }
  if (available_results_.find(ANDROID_CONTROL_AF_TRIGGER) ==
      available_results_.end()) {
    ALOGE("%s: AF trigger must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AF_MODE) ==
      available_results_.end()) {
    ALOGE("%s: AF mode must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AF_STATE) ==
      available_results_.end()) {
    ALOGE("%s: AF state must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  bool auto_mode_present =
      available_af_modes_.find(ANDROID_CONTROL_AF_MODE_AUTO) !=
      available_af_modes_.end();
  bool picture_caf_mode_present =
      available_af_modes_.find(ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE) !=
      available_af_modes_.end();
  bool video_caf_mode_present =
      available_af_modes_.find(ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO) !=
      available_af_modes_.end();
  af_supported_ = auto_mode_present && (minimum_focus_distance_ > .0f);
  picture_caf_supported_ =
      picture_caf_mode_present && (minimum_focus_distance_ > .0f);
  video_caf_supported_ =
      video_caf_mode_present && (minimum_focus_distance_ > .0f);

  return OK;
}

status_t EmulatedRequestState::InitializeControlAWBDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(ANDROID_CONTROL_AWB_AVAILABLE_MODES, &entry);
  if (ret == OK) {
    available_awb_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available AWB modes!", __FUNCTION__);
    return BAD_VALUE;
  }
  // Auto mode must always be present
  if (available_awb_modes_.find(ANDROID_CONTROL_AWB_MODE_AUTO) ==
      available_awb_modes_.end()) {
    ALOGE("%s: AWB auto control mode must always be present!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AWB_MODE) ==
      available_results_.end()) {
    ALOGE("%s: AWB mode must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AWB_STATE) ==
      available_results_.end()) {
    ALOGE("%s: AWB state must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_CONTROL_AWB_LOCK_AVAILABLE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    awb_lock_available_ =
        entry.data.u8[0] == ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
  } else {
    ALOGV("%s: No available AWB lock!", __FUNCTION__);
    awb_lock_available_ = false;
  }
  report_awb_lock_ = available_results_.find(ANDROID_CONTROL_AWB_LOCK) !=
                     available_results_.end();

  return OK;
}

status_t EmulatedRequestState::InitializeBlackLevelDefaults() {
  if (is_level_full_or_higher_) {
    if (available_requests_.find(ANDROID_BLACK_LEVEL_LOCK) ==
        available_requests_.end()) {
      ALOGE(
          "%s: Full or above capable devices must be able to set the black "
          "level lock!",
          __FUNCTION__);
      return BAD_VALUE;
    }

    if (available_results_.find(ANDROID_BLACK_LEVEL_LOCK) ==
        available_results_.end()) {
      ALOGE(
          "%s: Full or above capable devices must be able to report the black "
          "level lock!",
          __FUNCTION__);
      return BAD_VALUE;
    }

    report_black_level_lock_ = true;
    uint8_t blackLevelLock = ANDROID_BLACK_LEVEL_LOCK_OFF;
    for (size_t idx = 0; idx < kTemplateCount; idx++) {
      if (default_requests_[idx].get() == nullptr) {
        continue;
      }

      default_requests_[idx]->Set(ANDROID_BLACK_LEVEL_LOCK, &blackLevelLock, 1);
    }
  }

  return InitializeEdgeDefaults();
}

status_t EmulatedRequestState::InitializeControlAEDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(ANDROID_CONTROL_AE_AVAILABLE_MODES, &entry);
  if (ret == OK) {
    available_ae_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available AE modes!", __FUNCTION__);
    return BAD_VALUE;
  }
  // On mode must always be present
  if (available_ae_modes_.find(ANDROID_CONTROL_AE_MODE_ON) ==
      available_ae_modes_.end()) {
    ALOGE("%s: AE on control mode must always be present!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AE_MODE) ==
      available_results_.end()) {
    ALOGE("%s: AE mode must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AE_STATE) ==
      available_results_.end()) {
    ALOGE("%s: AE state must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_CONTROL_AE_LOCK_AVAILABLE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    ae_lock_available_ =
        entry.data.u8[0] == ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
  } else {
    ALOGV("%s: No available AE lock!", __FUNCTION__);
    ae_lock_available_ = false;
  }
  report_ae_lock_ = available_results_.find(ANDROID_CONTROL_AE_LOCK) !=
                    available_results_.end();

  if (supports_manual_sensor_) {
    if (!ae_lock_available_) {
      ALOGE("%s: AE lock must always be available for manual sensors!",
            __FUNCTION__);
      return BAD_VALUE;
    }
    auto off_mode = available_control_modes_.find(ANDROID_CONTROL_MODE_OFF);
    if (off_mode == available_control_modes_.end()) {
      ALOGE("%s: Off control mode must always be present for manual sensors!",
            __FUNCTION__);
      return BAD_VALUE;
    }

    off_mode = available_ae_modes_.find(ANDROID_CONTROL_AE_MODE_OFF);
    if (off_mode == available_ae_modes_.end()) {
      ALOGE(
          "%s: AE off control mode must always be present for manual sensors!",
          __FUNCTION__);
      return BAD_VALUE;
    }
  }

  if (available_requests_.find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER) ==
      available_requests_.end()) {
    ALOGE("%s: Clients must be able to set AE pre-capture trigger!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER) ==
      available_results_.end()) {
    ALOGE("%s: AE pre-capture trigger must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                              &entry);
  if (ret == OK) {
    available_antibanding_modes_.insert(entry.data.u8,
                                        entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available antibanding modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_CONTROL_AE_COMPENSATION_RANGE, &entry);
  if ((ret == OK) && (entry.count == 2)) {
    exposure_compensation_range_[0] = entry.data.i32[0];
    exposure_compensation_range_[1] = entry.data.i32[1];
  } else {
    ALOGE("%s: No available exposure compensation range!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_CONTROL_AE_COMPENSATION_STEP, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    exposure_compensation_step_ = entry.data.r[0];
  } else {
    ALOGE("%s: No available exposure compensation step!", __FUNCTION__);
    return BAD_VALUE;
  }

  bool ae_comp_requests =
      available_requests_.find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION) !=
      available_requests_.end();
  bool ae_comp_results =
      available_results_.find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION) !=
      available_results_.end();
  exposure_compensation_supported_ =
      ((exposure_compensation_range_[0] < 0) &&
       (exposure_compensation_range_[1] > 0) &&
       (exposure_compensation_step_.denominator > 0) &&
       (exposure_compensation_step_.numerator > 0)) &&
      ae_comp_results && ae_comp_requests;

  return OK;
}

status_t EmulatedRequestState::InitializeMeteringRegionDefault(
    uint32_t tag, int32_t* region /*out*/) {
  if (region == nullptr) {
    return BAD_VALUE;
  }
  if (available_requests_.find(tag) == available_requests_.end()) {
    ALOGE("%s: %d metering region configuration must be supported!",
          __FUNCTION__, tag);
    return BAD_VALUE;
  }
  if (available_results_.find(tag) == available_results_.end()) {
    ALOGE("%s: %d metering region must be reported!", __FUNCTION__, tag);
    return BAD_VALUE;
  }

  region[0] = scaler_crop_region_default_[0];
  region[1] = scaler_crop_region_default_[1];
  region[2] = scaler_crop_region_default_[2];
  region[3] = scaler_crop_region_default_[3];
  region[4] = 0;

  return OK;
}

status_t EmulatedRequestState::InitializeControlDefaults() {
  camera_metadata_ro_entry_t entry;
  int32_t metering_area[5] = {0};  // (top, left, width, height, wight)
  auto ret = static_metadata_->Get(ANDROID_CONTROL_AVAILABLE_MODES, &entry);
  if (ret == OK) {
    available_control_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available control modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  // Auto mode must always be present
  if (available_control_modes_.find(ANDROID_CONTROL_MODE_AUTO) ==
      available_control_modes_.end()) {
    ALOGE("%s: Auto control modes must always be present!", __FUNCTION__);
    return BAD_VALUE;
  }

  // Capture intent must always be user configurable
  if (available_requests_.find(ANDROID_CONTROL_CAPTURE_INTENT) ==
      available_requests_.end()) {
    ALOGE("%s: Clients must be able to set the capture intent!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                              &entry);
  if ((ret == OK) && ((entry.count % 2) == 0)) {
    available_fps_ranges_.reserve(entry.count / 2);
    for (size_t i = 0; i < entry.count; i += 2) {
      FPSRange range(entry.data.i32[i], entry.data.i32[i + 1]);
      if (range.min_fps > range.max_fps) {
        ALOGE("%s: Mininum framerate: %d bigger than maximum framerate: %d",
              __FUNCTION__, range.min_fps, range.max_fps);
        return BAD_VALUE;
      }
      if ((range.max_fps >= kMinimumStreamingFPS) &&
          (range.max_fps == range.min_fps) && (ae_target_fps_.max_fps == 0)) {
        ae_target_fps_ = range;
      }
      available_fps_ranges_.push_back(range);
    }
  } else {
    ALOGE("%s: No available framerate ranges!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (ae_target_fps_.max_fps == 0) {
    ALOGE("%s: No minimum streaming capable framerate range available!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_requests_.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE) ==
      available_requests_.end()) {
    ALOGE("%s: Clients must be able to set the target framerate range!",
          __FUNCTION__);
    return BAD_VALUE;
  }

  if (available_results_.find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE) ==
      available_results_.end()) {
    ALOGE("%s: Target framerate must be reported!", __FUNCTION__);
    return BAD_VALUE;
  }

  report_extended_scene_mode_ =
      available_results_.find(ANDROID_CONTROL_EXTENDED_SCENE_MODE) !=
      available_results_.end();

  if (is_backward_compatible_) {
    ret = static_metadata_->Get(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST,
                                &entry);
    if (ret == OK) {
      post_raw_boost_ = entry.data.i32[0];
    } else {
      ALOGW("%s: No available post RAW boost! Setting default!", __FUNCTION__);
      post_raw_boost_ = 100;
    }
    report_post_raw_boost_ =
        available_results_.find(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST) !=
        available_results_.end();

    ret = static_metadata_->Get(ANDROID_CONTROL_AVAILABLE_EFFECTS, &entry);
    if ((ret == OK) && (entry.count > 0)) {
      available_effects_.insert(entry.data.u8, entry.data.u8 + entry.count);
      if (available_effects_.find(ANDROID_CONTROL_EFFECT_MODE_OFF) ==
          available_effects_.end()) {
        ALOGE("%s: Off color effect mode not supported!", __FUNCTION__);
        return BAD_VALUE;
      }
    } else {
      ALOGE("%s: No available effects!", __FUNCTION__);
      return BAD_VALUE;
    }

    ret = static_metadata_->Get(
        ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES, &entry);
    if ((ret == OK) && (entry.count > 0)) {
      available_vstab_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
      if (available_vstab_modes_.find(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF) ==
        available_vstab_modes_.end()) {
        ALOGE("%s: Off video stabilization mode not supported!", __FUNCTION__);
        return BAD_VALUE;
      }
      if (available_vstab_modes_.find(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON) !=
        available_vstab_modes_.end()) {
        vstab_available_ = true;
      }
    } else {
      ALOGE("%s: No available video stabilization modes!", __FUNCTION__);
      return BAD_VALUE;
    }

    ret = static_metadata_->Get(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM,
                                &entry);
    if ((ret == OK) && (entry.count > 0)) {
      if (entry.count != 1) {
        ALOGE("%s: Invalid max digital zoom capability!", __FUNCTION__);
        return BAD_VALUE;
      }
      max_zoom_ = entry.data.f[0];
    } else {
      ALOGE("%s: No available max digital zoom", __FUNCTION__);
      return BAD_VALUE;
    }

    ret = static_metadata_->Get(ANDROID_CONTROL_ZOOM_RATIO_RANGE, &entry);
    if ((ret == OK) && (entry.count > 0)) {
      if (entry.count != 2) {
        ALOGE("%s: Invalid zoom ratio range capability!", __FUNCTION__);
        return BAD_VALUE;
      }

      if (entry.data.f[1] != max_zoom_) {
        ALOGE("%s: Max zoom ratio must be equal to max digital zoom",
              __FUNCTION__);
        return BAD_VALUE;
      }

      if (entry.data.f[1] < entry.data.f[0]) {
        ALOGE("%s: Max zoom ratio must be larger than min zoom ratio",
              __FUNCTION__);
        return BAD_VALUE;
      }

      // Sanity check request and result keys
      if (available_requests_.find(ANDROID_CONTROL_ZOOM_RATIO) ==
          available_requests_.end()) {
        ALOGE("%s: Zoom ratio tag must be available in available request keys",
              __FUNCTION__);
        return BAD_VALUE;
      }
      if (available_results_.find(ANDROID_CONTROL_ZOOM_RATIO) ==
          available_results_.end()) {
        ALOGE("%s: Zoom ratio tag must be available in available result keys",
              __FUNCTION__);
        return BAD_VALUE;
      }

      zoom_ratio_supported_ = true;
      min_zoom_ = entry.data.f[0];
    }

    ret = static_metadata_->Get(
        ANDROID_CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_MAX_SIZES, &entry);
    if ((ret == OK) && (entry.count > 0)) {
      if (entry.count % 3 != 0) {
        ALOGE("%s: Invalid bokeh capabilities!", __FUNCTION__);
        return BAD_VALUE;
      }

      camera_metadata_ro_entry_t zoom_ratio_ranges_entry;
      ret = static_metadata_->Get(
          ANDROID_CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_ZOOM_RATIO_RANGES,
          &zoom_ratio_ranges_entry);
      if (ret != OK ||
          zoom_ratio_ranges_entry.count / 2 != entry.count / 3 - 1) {
        ALOGE("%s: Invalid bokeh mode zoom ratio ranges.", __FUNCTION__);
        return BAD_VALUE;
      }

      // Sanity check request and characteristics keys
      if (available_requests_.find(ANDROID_CONTROL_EXTENDED_SCENE_MODE) ==
          available_requests_.end()) {
        ALOGE("%s: Extended scene mode must be configurable for this device",
              __FUNCTION__);
        return BAD_VALUE;
      }
      if (available_characteristics_.find(
              ANDROID_CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_MAX_SIZES) ==
              available_characteristics_.end() ||
          available_characteristics_.find(
              ANDROID_CONTROL_AVAILABLE_EXTENDED_SCENE_MODE_ZOOM_RATIO_RANGES) ==
              available_characteristics_.end()) {
        ALOGE(
            "%s: ExtendedSceneMode maxSizes and zoomRatioRanges "
            "characteristics keys must "
            "be available",
            __FUNCTION__);
        return BAD_VALUE;
      }

      // Derive available bokeh caps.
      StreamConfigurationMap stream_configuration_map(*static_metadata_);
      std::set<StreamSize> yuv_sizes = stream_configuration_map.GetOutputSizes(
          HAL_PIXEL_FORMAT_YCBCR_420_888);
      bool has_extended_scene_mode_off = false;
      for (size_t i = 0, j = 0; i < entry.count; i += 3) {
        int32_t mode = entry.data.i32[i];
        int32_t max_width = entry.data.i32[i + 1];
        int32_t max_height = entry.data.i32[i + 2];
        float min_zoom_ratio, max_zoom_ratio;

        if (mode < ANDROID_CONTROL_EXTENDED_SCENE_MODE_DISABLED ||
            mode > ANDROID_CONTROL_EXTENDED_SCENE_MODE_BOKEH_CONTINUOUS) {
          ALOGE("%s: Invalid extended scene mode %d", __FUNCTION__, mode);
          return BAD_VALUE;
        }

        if (mode == ANDROID_CONTROL_EXTENDED_SCENE_MODE_DISABLED) {
          has_extended_scene_mode_off = true;
          if (max_width != 0 || max_height != 0) {
            ALOGE(
                "%s: Invalid max width or height for "
                "EXTENDED_SCENE_MODE_DISABLED",
                __FUNCTION__);
            return BAD_VALUE;
          }
          min_zoom_ratio = min_zoom_;
          max_zoom_ratio = max_zoom_;
        } else if (yuv_sizes.find({max_width, max_height}) == yuv_sizes.end()) {
          ALOGE("%s: Invalid max width or height for extended scene mode %d",
                __FUNCTION__, mode);
          return BAD_VALUE;
        } else {
          min_zoom_ratio = zoom_ratio_ranges_entry.data.f[j];
          max_zoom_ratio = zoom_ratio_ranges_entry.data.f[j + 1];
          j += 2;
        }

        ExtendedSceneModeCapability cap(mode, max_width, max_height,
                                        min_zoom_ratio, max_zoom_ratio);
        available_extended_scene_mode_caps_.push_back(cap);
      }
      if (!has_extended_scene_mode_off) {
        ALOGE("%s: Off extended scene mode not supported!", __FUNCTION__);
        return BAD_VALUE;
      }
    }

    ret = static_metadata_->Get(ANDROID_CONTROL_MAX_REGIONS, &entry);
    if ((ret == OK) && (entry.count == 3)) {
      max_ae_regions_ = entry.data.i32[0];
      max_awb_regions_ = entry.data.i32[1];
      max_af_regions_ = entry.data.i32[2];
    } else {
      ALOGE(
          "%s: Metering regions must be available for backward compatible "
          "devices!",
          __FUNCTION__);
      return BAD_VALUE;
    }

    if ((is_level_full_or_higher_) &&
        ((max_ae_regions_ == 0) || (max_af_regions_ == 0))) {
      ALOGE(
          "%s: Full and higher level cameras must support at AF and AE "
          "metering regions",
          __FUNCTION__);
      return BAD_VALUE;
    }

    if (max_ae_regions_ > 0) {
      ret = InitializeMeteringRegionDefault(ANDROID_CONTROL_AE_REGIONS,
                                            ae_metering_region_);
      if (ret != OK) {
        return ret;
      }
    }

    if (max_awb_regions_ > 0) {
      ret = InitializeMeteringRegionDefault(ANDROID_CONTROL_AWB_REGIONS,
                                            awb_metering_region_);
      if (ret != OK) {
        return ret;
      }
    }

    if (max_af_regions_ > 0) {
      ret = InitializeMeteringRegionDefault(ANDROID_CONTROL_AF_REGIONS,
                                            af_metering_region_);
      if (ret != OK) {
        return ret;
      }
    }

    ret = InitializeControlAEDefaults();
    if (ret != OK) {
      return ret;
    }

    ret = InitializeControlAWBDefaults();
    if (ret != OK) {
      return ret;
    }

    ret = InitializeControlAFDefaults();
    if (ret != OK) {
      return ret;
    }

    ret = InitializeControlSceneDefaults();
    if (ret != OK) {
      return ret;
    }
  }

  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    auto template_idx = static_cast<RequestTemplate>(idx);
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    uint8_t intent = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
    uint8_t control_mode, ae_mode, awb_mode, af_mode, scene_mode, vstab_mode;
    control_mode = ANDROID_CONTROL_MODE_AUTO;
    ae_mode = ANDROID_CONTROL_AE_MODE_ON;
    awb_mode = ANDROID_CONTROL_AWB_MODE_AUTO;
    af_mode = af_supported_ ? ANDROID_CONTROL_AF_MODE_AUTO
                            : ANDROID_CONTROL_AF_MODE_OFF;
    scene_mode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    uint8_t effect_mode = ANDROID_CONTROL_EFFECT_MODE_OFF;
    uint8_t ae_lock = ANDROID_CONTROL_AE_LOCK_OFF;
    uint8_t awb_lock = ANDROID_CONTROL_AWB_LOCK_OFF;
    int32_t ae_target_fps[] = {ae_target_fps_.min_fps, ae_target_fps_.max_fps};
    float zoom_ratio = 1.0f;
    switch (template_idx) {
      case RequestTemplate::kManual:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_MANUAL;
        control_mode = ANDROID_CONTROL_MODE_OFF;
        ae_mode = ANDROID_CONTROL_AE_MODE_OFF;
        awb_mode = ANDROID_CONTROL_AWB_MODE_OFF;
        af_mode = ANDROID_CONTROL_AF_MODE_OFF;
        break;
      case RequestTemplate::kZeroShutterLag:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG;
        if (picture_caf_supported_) {
          af_mode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        }
        break;
      case RequestTemplate::kPreview:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        if (picture_caf_supported_) {
          af_mode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        }
        break;
      case RequestTemplate::kStillCapture:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
        if (picture_caf_supported_) {
          af_mode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        }
        break;
      case RequestTemplate::kVideoRecord:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
        if (video_caf_supported_) {
          af_mode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        }
        if (vstab_available_) {
          vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
        }
        break;
      case RequestTemplate::kVideoSnapshot:
        intent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
        if (video_caf_supported_) {
          af_mode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        }
        if (vstab_available_) {
          vstab_mode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON;
        }
        break;
      default:
        // Noop
        break;
    }

    if (intent != ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM) {
      default_requests_[idx]->Set(ANDROID_CONTROL_CAPTURE_INTENT, &intent, 1);
      default_requests_[idx]->Set(ANDROID_CONTROL_MODE, &control_mode, 1);
      default_requests_[idx]->Set(ANDROID_CONTROL_AE_MODE, &ae_mode, 1);
      default_requests_[idx]->Set(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                                  ae_target_fps, ARRAY_SIZE(ae_target_fps));
      default_requests_[idx]->Set(ANDROID_CONTROL_AWB_MODE, &awb_mode, 1);
      default_requests_[idx]->Set(ANDROID_CONTROL_AF_MODE, &af_mode, 1);
      if (is_backward_compatible_) {
        default_requests_[idx]->Set(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST,
                                    &post_raw_boost_, 1);
        if (vstab_available_) {
          default_requests_[idx]->Set(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                                      &vstab_mode, 1);
        }
        if (ae_lock_available_) {
          default_requests_[idx]->Set(ANDROID_CONTROL_AE_LOCK, &ae_lock, 1);
        }
        if (awb_lock_available_) {
          default_requests_[idx]->Set(ANDROID_CONTROL_AWB_LOCK, &awb_lock, 1);
        }
        if (scenes_supported_) {
          default_requests_[idx]->Set(ANDROID_CONTROL_SCENE_MODE, &scene_mode,
                                      1);
        }
        if (max_ae_regions_ > 0) {
          default_requests_[idx]->Set(ANDROID_CONTROL_AE_REGIONS, metering_area,
                                      ARRAY_SIZE(metering_area));
        }
        if (max_awb_regions_ > 0) {
          default_requests_[idx]->Set(ANDROID_CONTROL_AWB_REGIONS,
                                      metering_area, ARRAY_SIZE(metering_area));
        }
        if (max_af_regions_ > 0) {
          default_requests_[idx]->Set(ANDROID_CONTROL_AF_REGIONS, metering_area,
                                      ARRAY_SIZE(metering_area));
        }
        if (exposure_compensation_supported_) {
          default_requests_[idx]->Set(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
                                      &exposure_compensation_, 1);
        }
        if (zoom_ratio_supported_) {
          default_requests_[idx]->Set(ANDROID_CONTROL_ZOOM_RATIO, &zoom_ratio,
                                      1);
        }
        bool is_auto_antbanding_supported =
            available_antibanding_modes_.find(
                ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO) !=
            available_antibanding_modes_.end();
        uint8_t antibanding_mode = is_auto_antbanding_supported
                                       ? ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO
                                       : *available_antibanding_modes_.begin();
        default_requests_[idx]->Set(ANDROID_CONTROL_AE_ANTIBANDING_MODE,
                                    &antibanding_mode, 1);
        default_requests_[idx]->Set(ANDROID_CONTROL_EFFECT_MODE, &effect_mode,
                                    1);
        uint8_t ae_trigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
        default_requests_[idx]->Set(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
                                    &ae_trigger, 1);
        uint8_t af_trigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
        default_requests_[idx]->Set(ANDROID_CONTROL_AF_TRIGGER, &af_trigger, 1);
      }
    }
  }

  return InitializeHotPixelDefaults();
}

status_t EmulatedRequestState::InitializeTonemapDefaults() {
  if (is_backward_compatible_) {
    camera_metadata_ro_entry_t entry;
    auto ret =
        static_metadata_->Get(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES, &entry);
    if (ret == OK) {
      available_tonemap_modes_.insert(entry.data.u8,
                                      entry.data.u8 + entry.count);
    } else {
      ALOGE("%s: No available tonemap modes!", __FUNCTION__);
      return BAD_VALUE;
    }

    if ((is_level_full_or_higher_) && (available_tonemap_modes_.size() < 3)) {
      ALOGE(
          "%s: Full and higher level cameras must support at least three or "
          "more tonemap modes",
          __FUNCTION__);
      return BAD_VALUE;
    }

    bool fast_mode_supported =
        available_tonemap_modes_.find(ANDROID_TONEMAP_MODE_FAST) !=
        available_tonemap_modes_.end();
    bool hq_mode_supported =
        available_tonemap_modes_.find(ANDROID_TONEMAP_MODE_HIGH_QUALITY) !=
        available_tonemap_modes_.end();
    uint8_t tonemap_mode = *available_tonemap_modes_.begin();
    for (size_t idx = 0; idx < kTemplateCount; idx++) {
      if (default_requests_[idx].get() == nullptr) {
        continue;
      }

      switch (static_cast<RequestTemplate>(idx)) {
        case RequestTemplate::kVideoRecord:  // Pass-through
        case RequestTemplate::kPreview:
          if (fast_mode_supported) {
            tonemap_mode = ANDROID_TONEMAP_MODE_FAST;
          }
          break;
        case RequestTemplate::kVideoSnapshot:  // Pass-through
        case RequestTemplate::kStillCapture:
          if (hq_mode_supported) {
            tonemap_mode = ANDROID_TONEMAP_MODE_HIGH_QUALITY;
          }
          break;
        default:
          // Noop
          break;
      }

      default_requests_[idx]->Set(ANDROID_TONEMAP_MODE, &tonemap_mode, 1);
      default_requests_[idx]->Set(
          ANDROID_TONEMAP_CURVE_RED, EmulatedSensor::kDefaultToneMapCurveRed,
          ARRAY_SIZE(EmulatedSensor::kDefaultToneMapCurveRed));
      default_requests_[idx]->Set(
          ANDROID_TONEMAP_CURVE_GREEN, EmulatedSensor::kDefaultToneMapCurveGreen,
          ARRAY_SIZE(EmulatedSensor::kDefaultToneMapCurveGreen));
      default_requests_[idx]->Set(
          ANDROID_TONEMAP_CURVE_BLUE, EmulatedSensor::kDefaultToneMapCurveBlue,
          ARRAY_SIZE(EmulatedSensor::kDefaultToneMapCurveBlue));
    }
  }

  return InitializeStatisticsDefaults();
}

status_t EmulatedRequestState::InitializeEdgeDefaults() {
  if (is_backward_compatible_) {
    camera_metadata_ro_entry_t entry;
    auto ret = static_metadata_->Get(ANDROID_EDGE_AVAILABLE_EDGE_MODES, &entry);
    if (ret == OK) {
      available_edge_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
    } else {
      ALOGE("%s: No available edge modes!", __FUNCTION__);
      return BAD_VALUE;
    }

    report_edge_mode_ = available_results_.find(ANDROID_EDGE_MODE) != available_results_.end();
    bool is_fast_mode_supported =
        available_edge_modes_.find(ANDROID_EDGE_MODE_FAST) !=
        available_edge_modes_.end();
    bool is_hq_mode_supported =
        available_edge_modes_.find(ANDROID_EDGE_MODE_HIGH_QUALITY) !=
        available_edge_modes_.end();
    bool is_zsl_mode_supported =
        available_edge_modes_.find(ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG) !=
        available_edge_modes_.end();
    uint8_t edge_mode = *available_ae_modes_.begin();
    for (size_t idx = 0; idx < kTemplateCount; idx++) {
      if (default_requests_[idx].get() == nullptr) {
        continue;
      }

      switch (static_cast<RequestTemplate>(idx)) {
        case RequestTemplate::kVideoRecord:  // Pass-through
        case RequestTemplate::kPreview:
          if (is_fast_mode_supported) {
            edge_mode = ANDROID_EDGE_MODE_FAST;
          }
          break;
        case RequestTemplate::kVideoSnapshot:  // Pass-through
        case RequestTemplate::kStillCapture:
          if (is_hq_mode_supported) {
            edge_mode = ANDROID_EDGE_MODE_HIGH_QUALITY;
          }
          break;
        case RequestTemplate::kZeroShutterLag:
          if (is_zsl_mode_supported) {
            edge_mode = ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG;
          }
          break;
        default:
          // Noop
          break;
      }

      default_requests_[idx]->Set(ANDROID_EDGE_MODE, &edge_mode, 1);
    }
  }

  return InitializeShadingDefaults();
}

status_t EmulatedRequestState::InitializeColorCorrectionDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(
      ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES, &entry);
  if (ret == OK) {
    available_color_aberration_modes_.insert(entry.data.u8,
                                             entry.data.u8 + entry.count);
  } else if (supports_manual_post_processing_) {
    ALOGE(
        "%s: Devices capable of manual post-processing must support color "
        "abberation!",
        __FUNCTION__);
    return BAD_VALUE;
  }

  if (!available_color_aberration_modes_.empty()) {
    bool is_fast_mode_supported =
        available_color_aberration_modes_.find(
            ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST) !=
        available_color_aberration_modes_.end();
    bool is_hq_mode_supported =
        available_color_aberration_modes_.find(
            ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY) !=
        available_color_aberration_modes_.end();
    uint8_t color_aberration = *available_color_aberration_modes_.begin();
    uint8_t color_correction_mode = ANDROID_COLOR_CORRECTION_MODE_FAST;
    for (size_t idx = 0; idx < kTemplateCount; idx++) {
      if (default_requests_[idx].get() == nullptr) {
        continue;
      }

      switch (static_cast<RequestTemplate>(idx)) {
        case RequestTemplate::kVideoRecord:  // Pass-through
        case RequestTemplate::kPreview:
          if (is_fast_mode_supported) {
            color_aberration = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_FAST;
          }
          break;
        case RequestTemplate::kVideoSnapshot:  // Pass-through
        case RequestTemplate::kStillCapture:
          if (is_hq_mode_supported) {
            color_aberration =
                ANDROID_COLOR_CORRECTION_ABERRATION_MODE_HIGH_QUALITY;
          }
          break;
        default:
          // Noop
          break;
      }

      default_requests_[idx]->Set(ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
                                  &color_aberration, 1);
      if (is_backward_compatible_) {
        default_requests_[idx]->Set(ANDROID_COLOR_CORRECTION_MODE,
                                    &color_correction_mode, 1);
        default_requests_[idx]->Set(
            ANDROID_COLOR_CORRECTION_TRANSFORM,
            EmulatedSensor::kDefaultColorTransform,
            ARRAY_SIZE(EmulatedSensor::kDefaultColorTransform));
        default_requests_[idx]->Set(
            ANDROID_COLOR_CORRECTION_GAINS,
            EmulatedSensor::kDefaultColorCorrectionGains,
            ARRAY_SIZE(EmulatedSensor::kDefaultColorCorrectionGains));
      }
    }
  }

  return InitializeSensorDefaults();
}

status_t EmulatedRequestState::InitializeScalerDefaults() {
  if (is_backward_compatible_) {
    camera_metadata_ro_entry_t entry;
    auto ret =
        static_metadata_->Get(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, &entry);
    if ((ret == OK) && (entry.count == 4)) {
      scaler_crop_region_default_[0] = entry.data.i32[0];
      scaler_crop_region_default_[1] = entry.data.i32[1];
      scaler_crop_region_default_[2] = entry.data.i32[2];
      scaler_crop_region_default_[3] = entry.data.i32[3];
    } else {
      ALOGE("%s: Sensor pixel array size is not available!", __FUNCTION__);
      return BAD_VALUE;
    }

    if (available_requests_.find(ANDROID_SCALER_CROP_REGION) ==
        available_requests_.end()) {
      ALOGE(
          "%s: Backward compatible devices must support scaler crop "
          "configuration!",
          __FUNCTION__);
      return BAD_VALUE;
    }
    if (available_results_.find(ANDROID_SCALER_CROP_REGION) ==
        available_results_.end()) {
      ALOGE("%s: Scaler crop must reported on backward compatible devices!",
            __FUNCTION__);
      return BAD_VALUE;
    }
    ret = static_metadata_->Get(ANDROID_SCALER_AVAILABLE_ROTATE_AND_CROP_MODES,
                                &entry);
    if ((ret == OK) && (entry.count > 0)) {
      // Listing rotate and crop, so need to make sure it's consistently reported
      if (available_requests_.find(ANDROID_SCALER_ROTATE_AND_CROP) ==
          available_requests_.end()) {
        ALOGE(
            "%s: Rotate and crop must be listed in request keys if supported!",
            __FUNCTION__);
        return BAD_VALUE;
      }
      if (available_results_.find(ANDROID_SCALER_ROTATE_AND_CROP) ==
          available_results_.end()) {
        ALOGE("%s: Rotate and crop must be listed in result keys if supported!",
              __FUNCTION__);
        return BAD_VALUE;
      }
      if (available_characteristics_.find(
              ANDROID_SCALER_AVAILABLE_ROTATE_AND_CROP_MODES) ==
          available_characteristics_.end()) {
        ALOGE(
            "%s: Rotate and crop must be listed in characteristics keys if "
            "supported!",
            __FUNCTION__);
        return BAD_VALUE;
      }
      report_rotate_and_crop_ = true;
      for (size_t i = 0; i < entry.count; i++) {
        if (entry.data.u8[i] == ANDROID_SCALER_ROTATE_AND_CROP_AUTO) {
          rotate_and_crop_ = ANDROID_SCALER_ROTATE_AND_CROP_AUTO;
        }
        available_rotate_crop_modes_.insert(entry.data.u8[i]);
      }
    }

    for (size_t idx = 0; idx < kTemplateCount; idx++) {
      if (default_requests_[idx].get() == nullptr) {
        continue;
      }

      default_requests_[idx]->Set(ANDROID_SCALER_CROP_REGION,
                                  scaler_crop_region_default_,
                                  ARRAY_SIZE(scaler_crop_region_default_));
      if (report_rotate_and_crop_) {
        default_requests_[idx]->Set(ANDROID_SCALER_ROTATE_AND_CROP,
                                    &rotate_and_crop_, 1);
      }
    }
  }

  return InitializeControlDefaults();
}

status_t EmulatedRequestState::InitializeShadingDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(ANDROID_SHADING_AVAILABLE_MODES, &entry);
  if (ret == OK) {
    available_shading_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available lens shading modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (supports_manual_post_processing_ &&
      (available_shading_modes_.size() < 2)) {
    ALOGE(
        "%s: Devices capable of manual post-processing need to support at "
        "least "
        "two"
        " lens shading modes!",
        __FUNCTION__);
    return BAD_VALUE;
  }

  bool is_fast_mode_supported =
      available_shading_modes_.find(ANDROID_SHADING_MODE_FAST) !=
      available_shading_modes_.end();
  bool is_hq_mode_supported =
      available_shading_modes_.find(ANDROID_SHADING_MODE_HIGH_QUALITY) !=
      available_shading_modes_.end();
  uint8_t shading_mode = *available_shading_modes_.begin();
  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    switch (static_cast<RequestTemplate>(idx)) {
      case RequestTemplate::kVideoRecord:  // Pass-through
      case RequestTemplate::kPreview:
        if (is_fast_mode_supported) {
          shading_mode = ANDROID_SHADING_MODE_FAST;
        }
        break;
      case RequestTemplate::kVideoSnapshot:  // Pass-through
      case RequestTemplate::kStillCapture:
        if (is_hq_mode_supported) {
          shading_mode = ANDROID_SHADING_MODE_HIGH_QUALITY;
        }
        break;
      default:
        // Noop
        break;
    }

    default_requests_[idx]->Set(ANDROID_SHADING_MODE, &shading_mode, 1);
  }

  return InitializeNoiseReductionDefaults();
}

status_t EmulatedRequestState::InitializeNoiseReductionDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(
      ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES, &entry);
  if (ret == OK) {
    available_noise_reduction_modes_.insert(entry.data.u8,
                                            entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available noise reduction modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  if ((is_level_full_or_higher_) &&
      (available_noise_reduction_modes_.size() < 2)) {
    ALOGE(
        "%s: Full and above device must support at least two noise reduction "
        "modes!",
        __FUNCTION__);
    return BAD_VALUE;
  }

  bool is_fast_mode_supported =
      available_noise_reduction_modes_.find(ANDROID_NOISE_REDUCTION_MODE_FAST) !=
      available_noise_reduction_modes_.end();
  bool is_hq_mode_supported = available_noise_reduction_modes_.find(
                                  ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY) !=
                              available_noise_reduction_modes_.end();
  bool is_zsl_mode_supported =
      available_noise_reduction_modes_.find(
          ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG) !=
      available_noise_reduction_modes_.end();
  uint8_t noise_reduction_mode = *available_noise_reduction_modes_.begin();
  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    switch (static_cast<RequestTemplate>(idx)) {
      case RequestTemplate::kVideoRecord:  // Pass-through
      case RequestTemplate::kVideoSnapshot:  // Pass-through
      case RequestTemplate::kPreview:
        if (is_fast_mode_supported) {
          noise_reduction_mode = ANDROID_NOISE_REDUCTION_MODE_FAST;
        }
        break;
      case RequestTemplate::kStillCapture:
        if (is_hq_mode_supported) {
          noise_reduction_mode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        }
        break;
      case RequestTemplate::kZeroShutterLag:
        if (is_zsl_mode_supported) {
          noise_reduction_mode = ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG;
        }
        break;
      default:
        // Noop
        break;
    }

    default_requests_[idx]->Set(ANDROID_NOISE_REDUCTION_MODE,
                                &noise_reduction_mode, 1);
  }

  return InitializeColorCorrectionDefaults();
}

status_t EmulatedRequestState::InitializeHotPixelDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
                                   &entry);
  if (ret == OK) {
    available_hot_pixel_modes_.insert(entry.data.u8,
                                      entry.data.u8 + entry.count);
  } else {
    ALOGE("%s: No available hotpixel modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  if ((is_level_full_or_higher_) && (available_hot_pixel_modes_.size() < 2)) {
    ALOGE(
        "%s: Full and higher level cameras must support at least fast and hq "
        "hotpixel modes",
        __FUNCTION__);
    return BAD_VALUE;
  }

  bool fast_mode_supported =
      available_hot_pixel_modes_.find(ANDROID_HOT_PIXEL_MODE_FAST) !=
      available_hot_pixel_modes_.end();
  bool hq_mode_supported =
      available_hot_pixel_modes_.find(ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY) !=
      available_hot_pixel_modes_.end();
  uint8_t hotpixel_mode = *available_hot_pixel_modes_.begin();
  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    switch (static_cast<RequestTemplate>(idx)) {
      case RequestTemplate::kVideoRecord:  // Pass-through
      case RequestTemplate::kPreview:
        if (fast_mode_supported) {
          hotpixel_mode = ANDROID_HOT_PIXEL_MODE_FAST;
        }
        break;
      case RequestTemplate::kVideoSnapshot:  // Pass-through
      case RequestTemplate::kStillCapture:
        if (hq_mode_supported) {
          hotpixel_mode = ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY;
        }
        break;
      default:
        // Noop
        break;
    }

    default_requests_[idx]->Set(ANDROID_HOT_PIXEL_MODE, &hotpixel_mode, 1);
  }

  return InitializeTonemapDefaults();
}

status_t EmulatedRequestState::InitializeFlashDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret = static_metadata_->Get(ANDROID_FLASH_INFO_AVAILABLE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    is_flash_supported_ = entry.data.u8[0];
  } else {
    ALOGE("%s: No available flash info!", __FUNCTION__);
    return BAD_VALUE;
  }

  if (is_flash_supported_) {
    flash_state_ = ANDROID_FLASH_STATE_READY;
  } else {
    flash_state_ = ANDROID_FLASH_STATE_UNAVAILABLE;
  }

  uint8_t flash_mode = ANDROID_FLASH_MODE_OFF;
  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    default_requests_[idx]->Set(ANDROID_FLASH_MODE, &flash_mode, 1);
  }

  return InitializeScalerDefaults();
}

status_t EmulatedRequestState::InitializeLensDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret =
      static_metadata_->Get(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    minimum_focus_distance_ = entry.data.f[0];
  } else {
    ALOGW("%s: No available minimum focus distance assuming fixed focus!",
          __FUNCTION__);
    minimum_focus_distance_ = .0f;
  }

  ret = static_metadata_->Get(ANDROID_LENS_INFO_AVAILABLE_APERTURES, &entry);
  if ((ret == OK) && (entry.count > 0)) {
    // TODO: add support for multiple apertures
    aperture_ = entry.data.f[0];
  } else {
    ALOGE("%s: No available aperture!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS, &entry);
  if ((ret == OK) && (entry.count > 0)) {
    focal_length_ = entry.data.f[0];
  } else {
    ALOGE("%s: No available focal length!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_LENS_INFO_SHADING_MAP_SIZE, &entry);
  if ((ret == OK) && (entry.count == 2)) {
    shading_map_size_[0] = entry.data.i32[0];
    shading_map_size_[1] = entry.data.i32[1];
  } else if (is_raw_capable_) {
    ALOGE("%s: No available shading map size!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
                              &entry);
  if ((ret == OK) && (entry.count > 0)) {
    // TODO: add support for multiple filter densities
    filter_density_ = entry.data.f[0];
  } else {
    ALOGE("%s: No available filter density!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
                              &entry);
  if ((ret == OK) && (entry.count > 0)) {
    // TODO: add support for multiple OIS modes
    available_ois_modes_.insert(entry.data.u8, entry.data.u8 + entry.count);
    if (available_ois_modes_.find(ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF) ==
        available_ois_modes_.end()) {
      ALOGE("%s: OIS off mode not supported!", __FUNCTION__);
      return BAD_VALUE;
    }
  } else {
    ALOGE("%s: No available OIS modes!", __FUNCTION__);
    return BAD_VALUE;
  }

  ret = static_metadata_->Get(ANDROID_LENS_POSE_ROTATION, &entry);
  if ((ret == OK) && (entry.count == ARRAY_SIZE(pose_rotation_))) {
    memcpy(pose_rotation_, entry.data.f, sizeof(pose_rotation_));
  }
  ret = static_metadata_->Get(ANDROID_LENS_POSE_TRANSLATION, &entry);
  if ((ret == OK) && (entry.count == ARRAY_SIZE(pose_translation_))) {
    memcpy(pose_translation_, entry.data.f, sizeof(pose_translation_));
  }
  ret = static_metadata_->Get(ANDROID_LENS_INTRINSIC_CALIBRATION, &entry);
  if ((ret == OK) && (entry.count == ARRAY_SIZE(intrinsic_calibration_))) {
    memcpy(intrinsic_calibration_, entry.data.f, sizeof(intrinsic_calibration_));
  }

  ret = static_metadata_->Get(ANDROID_LENS_DISTORTION, &entry);
  if ((ret == OK) && (entry.count == ARRAY_SIZE(distortion_))) {
    memcpy(distortion_, entry.data.f, sizeof(distortion_));
  }

  report_focus_distance_ =
      available_results_.find(ANDROID_LENS_FOCUS_DISTANCE) !=
      available_results_.end();
  report_focus_range_ = available_results_.find(ANDROID_LENS_FOCUS_RANGE) !=
                        available_results_.end();
  report_filter_density_ =
      available_results_.find(ANDROID_LENS_FILTER_DENSITY) !=
      available_results_.end();
  report_ois_mode_ =
      available_results_.find(ANDROID_LENS_OPTICAL_STABILIZATION_MODE) !=
      available_results_.end();
  report_pose_rotation_ = available_results_.find(ANDROID_LENS_POSE_ROTATION) !=
                          available_results_.end();
  report_pose_translation_ =
      available_results_.find(ANDROID_LENS_POSE_TRANSLATION) !=
      available_results_.end();
  report_intrinsic_calibration_ =
      available_results_.find(ANDROID_LENS_INTRINSIC_CALIBRATION) !=
      available_results_.end();
  report_distortion_ = available_results_.find(ANDROID_LENS_DISTORTION) !=
                       available_results_.end();

  focus_distance_ = minimum_focus_distance_;
  for (size_t idx = 0; idx < kTemplateCount; idx++) {
    if (default_requests_[idx].get() == nullptr) {
      continue;
    }

    default_requests_[idx]->Set(ANDROID_LENS_APERTURE, &aperture_, 1);
    default_requests_[idx]->Set(ANDROID_LENS_FOCAL_LENGTH, &focal_length_, 1);
    default_requests_[idx]->Set(ANDROID_LENS_FOCUS_DISTANCE, &focus_distance_,
                                1);
    default_requests_[idx]->Set(ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                                &ois_mode_, 1);
  }

  return InitializeFlashDefaults();
}

status_t EmulatedRequestState::InitializeInfoDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret =
      static_metadata_->Get(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (kSupportedHWLevels.find(entry.data.u8[0]) ==
        kSupportedCapabilites.end()) {
      ALOGE("%s: HW Level: %u not supported", __FUNCTION__, entry.data.u8[0]);
      return BAD_VALUE;
    }
  } else {
    ALOGE("%s: No available hardware level!", __FUNCTION__);
    return BAD_VALUE;
  }

  supported_hw_level_ = entry.data.u8[0];
  is_level_full_or_higher_ =
      (supported_hw_level_ == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL) ||
      (supported_hw_level_ == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_3);

  return InitializeReprocessDefaults();
}

status_t EmulatedRequestState::InitializeReprocessDefaults() {
  if (supports_private_reprocessing_ || supports_yuv_reprocessing_) {
    StreamConfigurationMap config_map(*static_metadata_);
    if (!config_map.SupportsReprocessing()) {
      ALOGE(
          "%s: Reprocess capability present but InputOutput format map is "
          "absent!",
          __FUNCTION__);
      return BAD_VALUE;
    }

    auto input_formats = config_map.GetInputFormats();
    for (const auto& input_format : input_formats) {
      auto output_formats =
          config_map.GetValidOutputFormatsForInput(input_format);
      for (const auto& output_format : output_formats) {
        if (!EmulatedSensor::IsReprocessPathSupported(
                EmulatedSensor::OverrideFormat(input_format),
                EmulatedSensor::OverrideFormat(output_format))) {
          ALOGE(
              "%s: Input format: 0x%x to output format: 0x%x reprocess is"
              " currently not supported!",
              __FUNCTION__, input_format, output_format);
          return BAD_VALUE;
        }
      }
    }
  }

  return InitializeLensDefaults();
}

status_t EmulatedRequestState::InitializeRequestDefaults() {
  camera_metadata_ro_entry_t entry;
  auto ret =
      static_metadata_->Get(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, &entry);
  if ((ret == OK) && (entry.count > 0)) {
    for (size_t i = 0; i < entry.count; i++) {
      if (kSupportedCapabilites.find(entry.data.u8[i]) ==
          kSupportedCapabilites.end()) {
        ALOGE("%s: Capability: %u not supported", __FUNCTION__,
              entry.data.u8[i]);
        return BAD_VALUE;
      }
    }
  } else {
    ALOGE("%s: No available capabilities!", __FUNCTION__);
    return BAD_VALUE;
  }
  available_capabilities_.insert(entry.data.u8, entry.data.u8 + entry.count);

  ret = static_metadata_->Get(ANDROID_REQUEST_PIPELINE_MAX_DEPTH, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (entry.data.u8[0] == 0) {
      ALOGE("%s: Maximum request pipeline depth must have a non zero value!",
            __FUNCTION__);
      return BAD_VALUE;
    }
  } else {
    ALOGE("%s: Maximum request pipeline depth absent!", __FUNCTION__);
    return BAD_VALUE;
  }
  max_pipeline_depth_ = entry.data.u8[0];

  ret = static_metadata_->Get(ANDROID_REQUEST_PARTIAL_RESULT_COUNT, &entry);
  if ((ret == OK) && (entry.count == 1)) {
    if (entry.data.i32[0] != 1) {
      ALOGW("%s: Partial results not supported!", __FUNCTION__);
    }
  }

  ret = static_metadata_->Get(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
                              &entry);
  if ((ret != OK) || (entry.count == 0)) {
    ALOGE("%s: No available characteristic keys!", __FUNCTION__);
    return BAD_VALUE;
  }
  available_characteristics_.insert(entry.data.i32,
                                    entry.data.i32 + entry.count);

  ret = static_metadata_->Get(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, &entry);
  if ((ret != OK) || (entry.count == 0)) {
    ALOGE("%s: No available result keys!", __FUNCTION__);
    return BAD_VALUE;
  }
  available_results_.insert(entry.data.i32, entry.data.i32 + entry.count);

  ret = static_metadata_->Get(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, &entry);
  if ((ret != OK) || (entry.count == 0)) {
    ALOGE("%s: No available request keys!", __FUNCTION__);
    return BAD_VALUE;
  }
  available_requests_.insert(entry.data.i32, entry.data.i32 + entry.count);

  supports_manual_sensor_ =
      SupportsCapability(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR);
  supports_manual_post_processing_ = SupportsCapability(
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING);
  supports_private_reprocessing_ = SupportsCapability(
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING);
  supports_yuv_reprocessing_ = SupportsCapability(
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING);
  is_backward_compatible_ = SupportsCapability(
      ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE);
  is_raw_capable_ =
      SupportsCapability(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW);

  if (supports_manual_sensor_) {
    auto templateIdx = static_cast<size_t>(RequestTemplate::kManual);
    default_requests_[templateIdx] = HalCameraMetadata::Create(1, 10);
  }

  for (size_t templateIdx = 0; templateIdx < kTemplateCount; templateIdx++) {
    switch (static_cast<RequestTemplate>(templateIdx)) {
      case RequestTemplate::kPreview:
      case RequestTemplate::kStillCapture:
      case RequestTemplate::kVideoRecord:
      case RequestTemplate::kVideoSnapshot:
        default_requests_[templateIdx] = HalCameraMetadata::Create(1, 10);
        break;
      default:
        // Noop
        break;
    }
  }

  if (supports_yuv_reprocessing_ || supports_private_reprocessing_) {
    auto templateIdx = static_cast<size_t>(RequestTemplate::kZeroShutterLag);
    default_requests_[templateIdx] = HalCameraMetadata::Create(1, 10);
  }

  return InitializeInfoDefaults();
}

status_t EmulatedRequestState::Initialize(
    std::unique_ptr<HalCameraMetadata> staticMeta) {
  std::lock_guard<std::mutex> lock(request_state_mutex_);
  static_metadata_ = std::move(staticMeta);

  return InitializeRequestDefaults();
}

status_t EmulatedRequestState::GetDefaultRequest(
    RequestTemplate type, std::unique_ptr<HalCameraMetadata>* default_settings) {
  if (default_settings == nullptr) {
    ALOGE("%s default_settings is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  std::lock_guard<std::mutex> lock(request_state_mutex_);
  auto idx = static_cast<size_t>(type);
  if (idx >= kTemplateCount) {
    ALOGE("%s: Unexpected request type: %d", __FUNCTION__, type);
    return BAD_VALUE;
  }

  if (default_requests_[idx].get() == nullptr) {
    ALOGE("%s: Unsupported request type: %d", __FUNCTION__, type);
    return BAD_VALUE;
  }

  *default_settings =
      HalCameraMetadata::Clone(default_requests_[idx]->GetRawCameraMetadata());

  return OK;
}

}  // namespace android
