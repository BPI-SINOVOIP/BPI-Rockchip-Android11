/*
 * Copyright (C) 2012 The Android Open Source Project
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

//#define LOG_NDEBUG 0
//#define LOG_NNDEBUG 0
#define LOG_TAG "EmulatedSensor"
#define ATRACE_TAG ATRACE_TAG_CAMERA

#ifdef LOG_NNDEBUG
#define ALOGVV(...) ALOGV(__VA_ARGS__)
#else
#define ALOGVV(...) ((void)0)
#endif

#include "EmulatedSensor.h"

#include <inttypes.h>
#include <libyuv.h>
#include <system/camera_metadata.h>
#include <utils/Log.h>
#include <utils/Trace.h>

#include <cmath>
#include <cstdlib>

#include "utils/ExifUtils.h"
#include "utils/HWLUtils.h"

namespace android {

using google_camera_hal::HalCameraMetadata;
using google_camera_hal::MessageType;
using google_camera_hal::NotifyMessage;

const uint32_t EmulatedSensor::kRegularSceneHandshake = 1; // Scene handshake divider
const uint32_t EmulatedSensor::kReducedSceneHandshake = 2; // Scene handshake divider

// 1 us - 30 sec
const nsecs_t EmulatedSensor::kSupportedExposureTimeRange[2] = {1000LL,
                                                                30000000000LL};

// ~1/30 s - 30 sec
const nsecs_t EmulatedSensor::kSupportedFrameDurationRange[2] = {33331760LL,
                                                                 30000000000LL};

const int32_t EmulatedSensor::kSupportedSensitivityRange[2] = {100, 1600};
const int32_t EmulatedSensor::kDefaultSensitivity = 100;  // ISO
const nsecs_t EmulatedSensor::kDefaultExposureTime = ms2ns(15);
const nsecs_t EmulatedSensor::kDefaultFrameDuration = ms2ns(33);
// Deadline within we should return the results as soon as possible to
// avoid skewing the frame cycle due to external delays.
const nsecs_t EmulatedSensor::kReturnResultThreshod = 3 * kDefaultFrameDuration;

// Sensor defaults
const uint8_t EmulatedSensor::kSupportedColorFilterArrangement =
    ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
const uint32_t EmulatedSensor::kDefaultMaxRawValue = 4000;
const uint32_t EmulatedSensor::kDefaultBlackLevelPattern[4] = {1000, 1000, 1000,
                                                               1000};

const nsecs_t EmulatedSensor::kMinVerticalBlank = 10000L;

// Sensor sensitivity
const float EmulatedSensor::kSaturationVoltage = 0.520f;
const uint32_t EmulatedSensor::kSaturationElectrons = 2000;
const float EmulatedSensor::kVoltsPerLuxSecond = 0.100f;

const float EmulatedSensor::kElectronsPerLuxSecond =
    EmulatedSensor::kSaturationElectrons / EmulatedSensor::kSaturationVoltage *
    EmulatedSensor::kVoltsPerLuxSecond;

const float EmulatedSensor::kReadNoiseStddevBeforeGain = 1.177;  // in electrons
const float EmulatedSensor::kReadNoiseStddevAfterGain =
    2.100;  // in digital counts
const float EmulatedSensor::kReadNoiseVarBeforeGain =
    EmulatedSensor::kReadNoiseStddevBeforeGain *
    EmulatedSensor::kReadNoiseStddevBeforeGain;
const float EmulatedSensor::kReadNoiseVarAfterGain =
    EmulatedSensor::kReadNoiseStddevAfterGain *
    EmulatedSensor::kReadNoiseStddevAfterGain;

const uint32_t EmulatedSensor::kMaxRAWStreams = 1;
const uint32_t EmulatedSensor::kMaxProcessedStreams = 3;
const uint32_t EmulatedSensor::kMaxStallingStreams = 2;
const uint32_t EmulatedSensor::kMaxInputStreams = 1;

const uint32_t EmulatedSensor::kMaxLensShadingMapSize[2]{64, 64};
const int32_t EmulatedSensor::kFixedBitPrecision = 64;  // 6-bit
// In fixed-point math, saturation point of sensor after gain
const int32_t EmulatedSensor::kSaturationPoint = kFixedBitPrecision * 255;
const camera_metadata_rational EmulatedSensor::kNeutralColorPoint[3] = {
    {255, 1}, {255, 1}, {255, 1}};
const float EmulatedSensor::kGreenSplit = 1.f;  // No divergence
// Reduce memory usage by allowing only one buffer in sensor, one in jpeg
// compressor and one pending request to avoid stalls.
const uint8_t EmulatedSensor::kPipelineDepth = 3;

const camera_metadata_rational EmulatedSensor::kDefaultColorTransform[9] = {
    {1, 1}, {0, 1}, {0, 1}, {0, 1}, {1, 1}, {0, 1}, {0, 1}, {0, 1}, {1, 1}};
const float EmulatedSensor::kDefaultColorCorrectionGains[4] = {1.0f, 1.0f, 1.0f,
                                                               1.0f};

const float EmulatedSensor::kDefaultToneMapCurveRed[4] = {.0f, .0f, 1.f, 1.f};
const float EmulatedSensor::kDefaultToneMapCurveGreen[4] = {.0f, .0f, 1.f, 1.f};
const float EmulatedSensor::kDefaultToneMapCurveBlue[4] = {.0f, .0f, 1.f, 1.f};

/** A few utility functions for math, normal distributions */

// Take advantage of IEEE floating-point format to calculate an approximate
// square root. Accurate to within +-3.6%
float sqrtf_approx(float r) {
  // Modifier is based on IEEE floating-point representation; the
  // manipulations boil down to finding approximate log2, dividing by two, and
  // then inverting the log2. A bias is added to make the relative error
  // symmetric about the real answer.
  const int32_t modifier = 0x1FBB4000;

  int32_t r_i = *(int32_t*)(&r);
  r_i = (r_i >> 1) + modifier;

  return *(float*)(&r_i);
}

EmulatedSensor::EmulatedSensor() : Thread(false), got_vsync_(false) {
  gamma_table_.resize(kSaturationPoint + 1);
  for (int32_t i = 0; i <= kSaturationPoint; i++) {
    gamma_table_[i] = ApplysRGBGamma(i, kSaturationPoint);
  }
}

EmulatedSensor::~EmulatedSensor() {
  ShutDown();
}

bool EmulatedSensor::AreCharacteristicsSupported(
    const SensorCharacteristics& characteristics) {
  if ((characteristics.width == 0) || (characteristics.height == 0)) {
    ALOGE("%s: Invalid sensor size %zux%zu", __FUNCTION__,
          characteristics.width, characteristics.height);
    return false;
  }

  if ((characteristics.exposure_time_range[0] >=
       characteristics.exposure_time_range[1]) ||
      ((characteristics.exposure_time_range[0] < kSupportedExposureTimeRange[0]) ||
       (characteristics.exposure_time_range[1] >
        kSupportedExposureTimeRange[1]))) {
    ALOGE("%s: Unsupported exposure range", __FUNCTION__);
    return false;
  }

  if ((characteristics.frame_duration_range[0] >=
       characteristics.frame_duration_range[1]) ||
      ((characteristics.frame_duration_range[0] <
        kSupportedFrameDurationRange[0]) ||
       (characteristics.frame_duration_range[1] >
        kSupportedFrameDurationRange[1]))) {
    ALOGE("%s: Unsupported frame duration range", __FUNCTION__);
    return false;
  }

  if ((characteristics.sensitivity_range[0] >=
       characteristics.sensitivity_range[1]) ||
      ((characteristics.sensitivity_range[0] < kSupportedSensitivityRange[0]) ||
       (characteristics.sensitivity_range[1] > kSupportedSensitivityRange[1])) ||
      (!((kDefaultSensitivity >= characteristics.sensitivity_range[0]) &&
         (kDefaultSensitivity <= characteristics.sensitivity_range[1])))) {
    ALOGE("%s: Unsupported sensitivity range", __FUNCTION__);
    return false;
  }

  if (characteristics.color_arangement != kSupportedColorFilterArrangement) {
    ALOGE("%s: Unsupported color arrangement!", __FUNCTION__);
    return false;
  }

  for (const auto& blackLevel : characteristics.black_level_pattern) {
    if (blackLevel >= characteristics.max_raw_value) {
      ALOGE("%s: Black level matches or exceeds max RAW value!", __FUNCTION__);
      return false;
    }
  }

  if ((characteristics.frame_duration_range[0] / characteristics.height) == 0) {
    ALOGE("%s: Zero row readout time!", __FUNCTION__);
    return false;
  }

  if (characteristics.max_raw_streams > kMaxRAWStreams) {
    ALOGE("%s: RAW streams maximum %u exceeds supported maximum %u",
          __FUNCTION__, characteristics.max_raw_streams, kMaxRAWStreams);
    return false;
  }

  if (characteristics.max_processed_streams > kMaxProcessedStreams) {
    ALOGE("%s: Processed streams maximum %u exceeds supported maximum %u",
          __FUNCTION__, characteristics.max_processed_streams,
          kMaxProcessedStreams);
    return false;
  }

  if (characteristics.max_stalling_streams > kMaxStallingStreams) {
    ALOGE("%s: Stalling streams maximum %u exceeds supported maximum %u",
          __FUNCTION__, characteristics.max_stalling_streams,
          kMaxStallingStreams);
    return false;
  }

  if (characteristics.max_input_streams > kMaxInputStreams) {
    ALOGE("%s: Input streams maximum %u exceeds supported maximum %u",
          __FUNCTION__, characteristics.max_input_streams, kMaxInputStreams);
    return false;
  }

  if ((characteristics.lens_shading_map_size[0] > kMaxLensShadingMapSize[0]) ||
      (characteristics.lens_shading_map_size[1] > kMaxLensShadingMapSize[1])) {
    ALOGE("%s: Lens shading map [%dx%d] exceeds supprorted maximum [%dx%d]",
          __FUNCTION__, characteristics.lens_shading_map_size[0],
          characteristics.lens_shading_map_size[1], kMaxLensShadingMapSize[0],
          kMaxLensShadingMapSize[1]);
    return false;
  }

  if (characteristics.max_pipeline_depth < kPipelineDepth) {
    ALOGE("%s: Pipeline depth %d smaller than supprorted minimum %d",
          __FUNCTION__, characteristics.max_pipeline_depth, kPipelineDepth);
    return false;
  }

  return true;
}

bool EmulatedSensor::IsStreamCombinationSupported(
    const StreamConfiguration& config, StreamConfigurationMap& map,
    const SensorCharacteristics& sensor_chars) {
  uint32_t raw_stream_count = 0;
  uint32_t input_stream_count = 0;
  uint32_t processed_stream_count = 0;
  uint32_t stalling_stream_count = 0;

  for (const auto& stream : config.streams) {
    if (stream.rotation != google_camera_hal::StreamRotation::kRotation0) {
      ALOGE("%s: Stream rotation: 0x%x not supported!", __FUNCTION__,
            stream.rotation);
      return false;
    }

    if (stream.stream_type == google_camera_hal::StreamType::kInput) {
      if (sensor_chars.max_input_streams == 0) {
        ALOGE("%s: Input streams are not supported on this device!",
              __FUNCTION__);
        return false;
      }

      auto supported_outputs = map.GetValidOutputFormatsForInput(stream.format);
      if (supported_outputs.empty()) {
        ALOGE("%s: Input stream with format: 0x%x no supported on this device!",
              __FUNCTION__, stream.format);
        return false;
      }

      input_stream_count++;
    } else {
      switch (stream.format) {
        case HAL_PIXEL_FORMAT_BLOB:
          if ((stream.data_space != HAL_DATASPACE_V0_JFIF) &&
              (stream.data_space != HAL_DATASPACE_UNKNOWN)) {
            ALOGE("%s: Unsupported Blob dataspace 0x%x", __FUNCTION__,
                  stream.data_space);
            return false;
          }
          stalling_stream_count++;
          break;
        case HAL_PIXEL_FORMAT_RAW16:
          raw_stream_count++;
          break;
        default:
          processed_stream_count++;
      }
    }

    auto output_sizes = map.GetOutputSizes(stream.format);
    if (output_sizes.empty()) {
      ALOGE("%s: Unsupported format: 0x%x", __FUNCTION__, stream.format);
      return false;
    }

    auto stream_size = std::make_pair(stream.width, stream.height);
    if (output_sizes.find(stream_size) == output_sizes.end()) {
      ALOGE("%s: Stream with size %dx%d and format 0x%x is not supported!",
            __FUNCTION__, stream.width, stream.height, stream.format);
      return false;
    }
  }

  if (raw_stream_count > sensor_chars.max_raw_streams) {
    ALOGE("%s: RAW streams maximum %u exceeds supported maximum %u",
          __FUNCTION__, raw_stream_count, sensor_chars.max_raw_streams);
    return false;
  }

  if (processed_stream_count > sensor_chars.max_processed_streams) {
    ALOGE("%s: Processed streams maximum %u exceeds supported maximum %u",
          __FUNCTION__, processed_stream_count,
          sensor_chars.max_processed_streams);
    return false;
  }

  if (stalling_stream_count > sensor_chars.max_stalling_streams) {
    ALOGE("%s: Stalling streams maximum %u exceeds supported maximum %u",
          __FUNCTION__, stalling_stream_count,
          sensor_chars.max_stalling_streams);
    return false;
  }

  if (input_stream_count > sensor_chars.max_input_streams) {
    ALOGE("%s: Input stream maximum %u exceeds supported maximum %u",
          __FUNCTION__, input_stream_count, sensor_chars.max_input_streams);
    return false;
  }

  return true;
}

status_t EmulatedSensor::StartUp(
    uint32_t logical_camera_id,
    std::unique_ptr<LogicalCharacteristics> logical_chars) {
  if (isRunning()) {
    return OK;
  }

  if (logical_chars.get() == nullptr) {
    return BAD_VALUE;
  }

  chars_ = std::move(logical_chars);
  auto device_chars = chars_->find(logical_camera_id);
  if (device_chars == chars_->end()) {
    ALOGE(
        "%s: Logical camera id: %u absent from logical camera characteristics!",
        __FUNCTION__, logical_camera_id);
    return BAD_VALUE;
  }

  for (const auto& it : *chars_) {
    if (!AreCharacteristicsSupported(it.second)) {
      ALOGE("%s: Sensor characteristics for camera id: %u not supported!",
            __FUNCTION__, it.first);
      return BAD_VALUE;
    }
  }

  logical_camera_id_ = logical_camera_id;
  scene_ = new EmulatedScene(
      device_chars->second.width, device_chars->second.height,
      kElectronsPerLuxSecond, device_chars->second.orientation,
      device_chars->second.is_front_facing);
  scene_->InitializeSensorQueue();
  jpeg_compressor_ = std::make_unique<JpegCompressor>();

  auto res = run(LOG_TAG, ANDROID_PRIORITY_URGENT_DISPLAY);
  if (res != OK) {
    ALOGE("Unable to start up sensor capture thread: %d", res);
  }

  return res;
}

status_t EmulatedSensor::ShutDown() {
  int res;
  res = requestExitAndWait();
  if (res != OK) {
    ALOGE("Unable to shut down sensor capture thread: %d", res);
  }
  return res;
}

void EmulatedSensor::SetCurrentRequest(
    std::unique_ptr<LogicalCameraSettings> logical_settings,
    std::unique_ptr<HwlPipelineResult> result,
    std::unique_ptr<Buffers> input_buffers,
    std::unique_ptr<Buffers> output_buffers) {
  Mutex::Autolock lock(control_mutex_);
  current_settings_ = std::move(logical_settings);
  current_result_ = std::move(result);
  current_input_buffers_ = std::move(input_buffers);
  current_output_buffers_ = std::move(output_buffers);
}

bool EmulatedSensor::WaitForVSyncLocked(nsecs_t reltime) {
  got_vsync_ = false;
  while (!got_vsync_) {
    auto res = vsync_.waitRelative(control_mutex_, reltime);
    if (res != OK && res != TIMED_OUT) {
      ALOGE("%s: Error waiting for VSync signal: %d", __FUNCTION__, res);
      return false;
    }
  }

  return got_vsync_;
}

bool EmulatedSensor::WaitForVSync(nsecs_t reltime) {
  Mutex::Autolock lock(control_mutex_);

  return WaitForVSyncLocked(reltime);
}

status_t EmulatedSensor::Flush() {
  Mutex::Autolock lock(control_mutex_);
  auto ret = WaitForVSyncLocked(kSupportedFrameDurationRange[1]);

  // First recreate the jpeg compressor. This will abort any ongoing processing
  // and flush any pending jobs.
  jpeg_compressor_ = std::make_unique<JpegCompressor>();

  // Then return any pending frames here
  if ((current_input_buffers_.get() != nullptr) &&
      (!current_input_buffers_->empty())) {
    current_input_buffers_->clear();
  }
  if ((current_output_buffers_.get() != nullptr) &&
      (!current_output_buffers_->empty())) {
    for (const auto& buffer : *current_output_buffers_) {
      buffer->stream_buffer.status = BufferStatus::kError;
    }

    if ((current_result_.get() != nullptr) &&
        (current_result_->result_metadata.get() != nullptr)) {
      if (current_output_buffers_->at(0)->callback.notify != nullptr) {
        NotifyMessage msg{
            .type = MessageType::kError,
            .message.error = {
                .frame_number = current_output_buffers_->at(0)->frame_number,
                .error_stream_id = -1,
                .error_code = ErrorCode::kErrorResult,
            }};

        current_output_buffers_->at(0)->callback.notify(
            current_result_->pipeline_id, msg);
      }
    }

    current_output_buffers_->clear();
  }

  return ret ? OK : TIMED_OUT;
}

bool EmulatedSensor::threadLoop() {
  ATRACE_CALL();
  /**
   * Sensor capture operation main loop.
   *
   */

  /**
   * Stage 1: Read in latest control parameters
   */
  std::unique_ptr<Buffers> next_buffers;
  std::unique_ptr<Buffers> next_input_buffer;
  std::unique_ptr<HwlPipelineResult> next_result;
  std::unique_ptr<LogicalCameraSettings> settings;
  HwlPipelineCallback callback = {nullptr, nullptr};
  {
    Mutex::Autolock lock(control_mutex_);
    std::swap(settings, current_settings_);
    std::swap(next_buffers, current_output_buffers_);
    std::swap(next_input_buffer, current_input_buffers_);
    std::swap(next_result, current_result_);

    // Signal VSync for start of readout
    ALOGVV("Sensor VSync");
    got_vsync_ = true;
    vsync_.signal();
  }

  auto frame_duration = EmulatedSensor::kSupportedFrameDurationRange[0];
  // Frame duration must always be the same among all physical devices
  if ((settings.get() != nullptr) && (!settings->empty())) {
    frame_duration = settings->begin()->second.frame_duration;
  }

  nsecs_t start_real_time = systemTime();
  // Stagefright cares about system time for timestamps, so base simulated
  // time on that.
  nsecs_t frame_end_real_time = start_real_time + frame_duration;

  /**
   * Stage 2: Capture new image
   */
  next_capture_time_ = frame_end_real_time;

  bool reprocess_request = false;
  if ((next_input_buffer.get() != nullptr) && (!next_input_buffer->empty())) {
    if (next_input_buffer->size() > 1) {
      ALOGW("%s: Reprocess supports only single input!", __FUNCTION__);
    }
    if (next_input_buffer->at(0)->format != HAL_PIXEL_FORMAT_YCBCR_420_888) {
      ALOGE(
          "%s: Reprocess input format: 0x%x not supported! Skipping reprocess!",
          __FUNCTION__, next_input_buffer->at(0)->format);
    } else {
      camera_metadata_ro_entry_t entry;
      auto ret =
          next_result->result_metadata->Get(ANDROID_SENSOR_TIMESTAMP, &entry);
      if ((ret == OK) && (entry.count == 1)) {
        next_capture_time_ = entry.data.i64[0];
      } else {
        ALOGW("%s: Reprocess timestamp absent!", __FUNCTION__);
      }

      reprocess_request = true;
    }
  }

  if ((next_buffers != nullptr) && (settings != nullptr)) {
    callback = next_buffers->at(0)->callback;
    if (callback.notify != nullptr) {
      NotifyMessage msg{
          .type = MessageType::kShutter,
          .message.shutter = {
              .frame_number = next_buffers->at(0)->frame_number,
              .timestamp_ns = static_cast<uint64_t>(next_capture_time_)}};
      callback.notify(next_result->pipeline_id, msg);
    }
    auto b = next_buffers->begin();
    while (b != next_buffers->end()) {
      auto device_settings = settings->find((*b)->camera_id);
      if (device_settings == settings->end()) {
        ALOGE("%s: Sensor settings absent for device: %d", __func__,
              (*b)->camera_id);
        b = next_buffers->erase(b);
        continue;
      }

      auto device_chars = chars_->find((*b)->camera_id);
      if (device_chars == chars_->end()) {
        ALOGE("%s: Sensor characteristics absent for device: %d", __func__,
              (*b)->camera_id);
        b = next_buffers->erase(b);
        continue;
      }

      ALOGVV("Starting next capture: Exposure: %" PRIu64 " ms, gain: %d",
             ns2ms(device_settings->second.exposure_time),
             device_settings->second.gain);

      scene_->Initialize(device_chars->second.width,
                         device_chars->second.height, kElectronsPerLuxSecond);
      scene_->SetExposureDuration((float)device_settings->second.exposure_time /
                                  1e9);
      scene_->SetColorFilterXYZ(device_chars->second.color_filter.rX,
                                device_chars->second.color_filter.rY,
                                device_chars->second.color_filter.rZ,
                                device_chars->second.color_filter.grX,
                                device_chars->second.color_filter.grY,
                                device_chars->second.color_filter.grZ,
                                device_chars->second.color_filter.gbX,
                                device_chars->second.color_filter.gbY,
                                device_chars->second.color_filter.gbZ,
                                device_chars->second.color_filter.bX,
                                device_chars->second.color_filter.bY,
                                device_chars->second.color_filter.bZ);
      uint32_t handshake_divider =
        (device_settings->second.video_stab == ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON) ?
        kReducedSceneHandshake : kRegularSceneHandshake;
      scene_->CalculateScene(next_capture_time_, handshake_divider);

      (*b)->stream_buffer.status = BufferStatus::kOk;
      switch ((*b)->format) {
        case HAL_PIXEL_FORMAT_RAW16:
          if (!reprocess_request) {
            CaptureRaw((*b)->plane.img.img, device_settings->second.gain,
                       (*b)->width, device_chars->second);
          } else {
            ALOGE("%s: Reprocess requests with output format %x no supported!",
                  __FUNCTION__, (*b)->format);
            (*b)->stream_buffer.status = BufferStatus::kError;
          }
          break;
        case HAL_PIXEL_FORMAT_RGB_888:
          if (!reprocess_request) {
            CaptureRGB((*b)->plane.img.img, (*b)->width, (*b)->height,
                       (*b)->plane.img.stride, RGBLayout::RGB,
                       device_settings->second.gain, device_chars->second);
          } else {
            ALOGE("%s: Reprocess requests with output format %x no supported!",
                  __FUNCTION__, (*b)->format);
            (*b)->stream_buffer.status = BufferStatus::kError;
          }
          break;
        case HAL_PIXEL_FORMAT_RGBA_8888:
          if (!reprocess_request) {
            CaptureRGB((*b)->plane.img.img, (*b)->width, (*b)->height,
                       (*b)->plane.img.stride, RGBLayout::RGBA,
                       device_settings->second.gain, device_chars->second);
          } else {
            ALOGE("%s: Reprocess requests with output format %x no supported!",
                  __FUNCTION__, (*b)->format);
            (*b)->stream_buffer.status = BufferStatus::kError;
          }
          break;
        case HAL_PIXEL_FORMAT_BLOB:
          if ((*b)->dataSpace == HAL_DATASPACE_V0_JFIF) {
            YUV420Frame yuv_input{
                .width =
                    reprocess_request ? (*next_input_buffer->begin())->width : 0,
                .height = reprocess_request
                              ? (*next_input_buffer->begin())->height
                              : 0,
                .planes = reprocess_request
                              ? (*next_input_buffer->begin())->plane.img_y_crcb
                              : YCbCrPlanes{}};
            auto jpeg_input = std::make_unique<JpegYUV420Input>();
            jpeg_input->width = (*b)->width;
            jpeg_input->height = (*b)->height;
            auto img =
                new uint8_t[(jpeg_input->width * jpeg_input->height * 3) / 2];
            jpeg_input->yuv_planes = {
                .img_y = img,
                .img_cb = img + jpeg_input->width * jpeg_input->height,
                .img_cr = img + (jpeg_input->width * jpeg_input->height * 5) / 4,
                .y_stride = jpeg_input->width,
                .cbcr_stride = jpeg_input->width / 2,
                .cbcr_step = 1};
            jpeg_input->buffer_owner = true;
            YUV420Frame yuv_output{.width = jpeg_input->width,
                                   .height = jpeg_input->height,
                                   .planes = jpeg_input->yuv_planes};

            bool rotate =
                device_settings->second.rotate_and_crop == ANDROID_SCALER_ROTATE_AND_CROP_90;
            ProcessType process_type = reprocess_request ? REPROCESS :
              (device_settings->second.edge_mode == ANDROID_EDGE_MODE_HIGH_QUALITY) ?
              HIGH_QUALITY : REGULAR;
            auto ret = ProcessYUV420(
                yuv_input, yuv_output, device_settings->second.gain,
                process_type, device_settings->second.zoom_ratio,
                rotate, device_chars->second);
            if (ret != 0) {
              (*b)->stream_buffer.status = BufferStatus::kError;
              break;
            }

            auto jpeg_job = std::make_unique<JpegYUV420Job>();
            jpeg_job->exif_utils = std::unique_ptr<ExifUtils>(
                ExifUtils::Create(device_chars->second));
            jpeg_job->input = std::move(jpeg_input);
            // If jpeg compression is successful, then the jpeg compressor
            // must set the corresponding status.
            (*b)->stream_buffer.status = BufferStatus::kError;
            std::swap(jpeg_job->output, *b);
            jpeg_job->result_metadata =
                HalCameraMetadata::Clone(next_result->result_metadata.get());

            Mutex::Autolock lock(control_mutex_);
            jpeg_compressor_->QueueYUV420(std::move(jpeg_job));
          } else {
            ALOGE("%s: Format %x with dataspace %x is TODO", __FUNCTION__,
                  (*b)->format, (*b)->dataSpace);
            (*b)->stream_buffer.status = BufferStatus::kError;
          }
          break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_YCbCr_420_888: {
          YUV420Frame yuv_input{
              .width =
                  reprocess_request ? (*next_input_buffer->begin())->width : 0,
              .height =
                  reprocess_request ? (*next_input_buffer->begin())->height : 0,
              .planes = reprocess_request
                            ? (*next_input_buffer->begin())->plane.img_y_crcb
                            : YCbCrPlanes{}};
          YUV420Frame yuv_output{.width = (*b)->width,
                                 .height = (*b)->height,
                                 .planes = (*b)->plane.img_y_crcb};
          bool rotate =
              device_settings->second.rotate_and_crop == ANDROID_SCALER_ROTATE_AND_CROP_90;
          ProcessType process_type = reprocess_request ? REPROCESS :
            (device_settings->second.edge_mode == ANDROID_EDGE_MODE_HIGH_QUALITY) ?
            HIGH_QUALITY : REGULAR;
          auto ret = ProcessYUV420(
              yuv_input, yuv_output, device_settings->second.gain,
              process_type, device_settings->second.zoom_ratio,
              rotate, device_chars->second);
          if (ret != 0) {
            (*b)->stream_buffer.status = BufferStatus::kError;
          }
        } break;
        case HAL_PIXEL_FORMAT_Y16:
          if (!reprocess_request) {
            if ((*b)->dataSpace == HAL_DATASPACE_DEPTH) {
              CaptureDepth((*b)->plane.img.img, device_settings->second.gain,
                           (*b)->width, (*b)->height, (*b)->plane.img.stride,
                           device_chars->second);
            } else {
              ALOGE("%s: Format %x with dataspace %x is TODO", __FUNCTION__,
                    (*b)->format, (*b)->dataSpace);
              (*b)->stream_buffer.status = BufferStatus::kError;
            }
          } else {
            ALOGE("%s: Reprocess requests with output format %x no supported!",
                  __FUNCTION__, (*b)->format);
            (*b)->stream_buffer.status = BufferStatus::kError;
          }
          break;
        default:
          ALOGE("%s: Unknown format %x, no output", __FUNCTION__, (*b)->format);
          (*b)->stream_buffer.status = BufferStatus::kError;
          break;
      }

      b = next_buffers->erase(b);
    }
  }

  if (reprocess_request) {
    auto input_buffer = next_input_buffer->begin();
    while (input_buffer != next_input_buffer->end()) {
      (*input_buffer++)->stream_buffer.status = BufferStatus::kOk;
    }
    next_input_buffer->clear();
  }

  nsecs_t work_done_real_time = systemTime();
  // Returning the results at this point is not entirely correct from timing
  // perspective. Under ideal conditions where 'ReturnResults' completes
  // in less than 'time_accuracy' we need to return the results after the
  // frame cycle expires. However under real conditions various system
  // components like SurfaceFlinger, Encoder, LMK etc. could be consuming most
  // of the resources and the duration of "ReturnResults" can get comparable to
  // 'kDefaultFrameDuration'. This will skew the frame cycle and can result in
  // potential frame drops. To avoid this scenario when we are running under
  // tight deadlines (less than 'kReturnResultThreshod') try to return the
  // results immediately. In all other cases with more relaxed deadlines
  // the occasional bump during 'ReturnResults' should not have any
  // noticeable effect.
  if ((work_done_real_time + kReturnResultThreshod) > frame_end_real_time) {
    ReturnResults(callback, std::move(settings), std::move(next_result));
  }

  work_done_real_time = systemTime();
  ALOGVV("Sensor vertical blanking interval");
  const nsecs_t time_accuracy = 2e6;  // 2 ms of imprecision is ok
  if (work_done_real_time < frame_end_real_time - time_accuracy) {
    timespec t;
    t.tv_sec = (frame_end_real_time - work_done_real_time) / 1000000000L;
    t.tv_nsec = (frame_end_real_time - work_done_real_time) % 1000000000L;

    int ret;
    do {
      ret = nanosleep(&t, &t);
    } while (ret != 0);
  }
  nsecs_t end_real_time __unused = systemTime();
  ALOGVV("Frame cycle took %" PRIu64 "  ms, target %" PRIu64 " ms",
         ns2ms(end_real_time - start_real_time), ns2ms(frame_duration));

  ReturnResults(callback, std::move(settings), std::move(next_result));

  return true;
};

void EmulatedSensor::ReturnResults(
    HwlPipelineCallback callback,
    std::unique_ptr<LogicalCameraSettings> settings,
    std::unique_ptr<HwlPipelineResult> result) {
  if ((callback.process_pipeline_result != nullptr) &&
      (result.get() != nullptr) && (result->result_metadata.get() != nullptr)) {
    auto logical_settings = settings->find(logical_camera_id_);
    if (logical_settings == settings->end()) {
      ALOGE("%s: Logical camera id: %u not found in settings!", __FUNCTION__,
            logical_camera_id_);
      return;
    }
    auto device_chars = chars_->find(logical_camera_id_);
    if (device_chars == chars_->end()) {
      ALOGE("%s: Sensor characteristics absent for device: %d", __func__,
            logical_camera_id_);
      return;
    }

    result->result_metadata->Set(ANDROID_SENSOR_TIMESTAMP, &next_capture_time_,
                                 1);
    if (logical_settings->second.lens_shading_map_mode ==
        ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_ON) {
      if ((device_chars->second.lens_shading_map_size[0] > 0) &&
          (device_chars->second.lens_shading_map_size[1] > 0)) {
        // Perfect lens, no actual shading needed.
        std::vector<float> lens_shading_map(
            device_chars->second.lens_shading_map_size[0] *
                device_chars->second.lens_shading_map_size[1] * 4,
            1.f);

        result->result_metadata->Set(ANDROID_STATISTICS_LENS_SHADING_MAP,
                                     lens_shading_map.data(),
                                     lens_shading_map.size());
      }
    }
    if (logical_settings->second.report_video_stab) {
      result->result_metadata->Set(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                                   &logical_settings->second.video_stab, 1);
    }
    if (logical_settings->second.report_edge_mode) {
      result->result_metadata->Set(ANDROID_EDGE_MODE,
                                   &logical_settings->second.edge_mode, 1);
    }
    if (logical_settings->second.report_neutral_color_point) {
      result->result_metadata->Set(ANDROID_SENSOR_NEUTRAL_COLOR_POINT,
                                   kNeutralColorPoint,
                                   ARRAY_SIZE(kNeutralColorPoint));
    }
    if (logical_settings->second.report_green_split) {
      result->result_metadata->Set(ANDROID_SENSOR_GREEN_SPLIT, &kGreenSplit, 1);
    }
    if (logical_settings->second.report_noise_profile) {
      CalculateAndAppendNoiseProfile(
          logical_settings->second.gain,
          GetBaseGainFactor(device_chars->second.max_raw_value),
          result->result_metadata.get());
    }
    if (logical_settings->second.report_rotate_and_crop) {
      result->result_metadata->Set(ANDROID_SCALER_ROTATE_AND_CROP,
          &logical_settings->second.rotate_and_crop, 1);
    }

    if (!result->physical_camera_results.empty()) {
      for (auto& it : result->physical_camera_results) {
        auto physical_settings = settings->find(it.first);
        if (physical_settings == settings->end()) {
          ALOGE("%s: Physical settings for camera id: %u are absent!",
                __FUNCTION__, it.first);
          continue;
        }

        // Sensor timestamp for all physical devices must be the same.
        it.second->Set(ANDROID_SENSOR_TIMESTAMP, &next_capture_time_, 1);
        if (physical_settings->second.report_neutral_color_point) {
          it.second->Set(ANDROID_SENSOR_NEUTRAL_COLOR_POINT, kNeutralColorPoint,
                         ARRAY_SIZE(kNeutralColorPoint));
        }
        if (physical_settings->second.report_green_split) {
          it.second->Set(ANDROID_SENSOR_GREEN_SPLIT, &kGreenSplit, 1);
        }
        if (physical_settings->second.report_noise_profile) {
          auto device_chars = chars_->find(it.first);
          if (device_chars == chars_->end()) {
            ALOGE("%s: Sensor characteristics absent for device: %d", __func__,
                  it.first);
          }
          CalculateAndAppendNoiseProfile(
              physical_settings->second.gain,
              GetBaseGainFactor(device_chars->second.max_raw_value),
              it.second.get());
        }
      }
    }

    callback.process_pipeline_result(std::move(result));
  }
}

void EmulatedSensor::CalculateAndAppendNoiseProfile(
    float gain /*in ISO*/, float base_gain_factor,
    HalCameraMetadata* result /*out*/) {
  if (result != nullptr) {
    float total_gain = gain / 100.0 * base_gain_factor;
    float noise_var_gain = total_gain * total_gain;
    float read_noise_var =
        kReadNoiseVarBeforeGain * noise_var_gain + kReadNoiseVarAfterGain;
    // Noise profile is the same across all 4 CFA channels
    double noise_profile[2 * 4] = {
        noise_var_gain, read_noise_var, noise_var_gain, read_noise_var,
        noise_var_gain, read_noise_var, noise_var_gain, read_noise_var};
    result->Set(ANDROID_SENSOR_NOISE_PROFILE, noise_profile,
                ARRAY_SIZE(noise_profile));
  }
}

void EmulatedSensor::CaptureRaw(uint8_t* img, uint32_t gain, uint32_t width,
                                const SensorCharacteristics& chars) {
  ATRACE_CALL();
  float total_gain = gain / 100.0 * GetBaseGainFactor(chars.max_raw_value);
  float noise_var_gain = total_gain * total_gain;
  float read_noise_var =
      kReadNoiseVarBeforeGain * noise_var_gain + kReadNoiseVarAfterGain;
  //
  // RGGB
  int bayer_select[4] = {EmulatedScene::R, EmulatedScene::Gr, EmulatedScene::Gb,
                         EmulatedScene::B};
  scene_->SetReadoutPixel(0, 0);
  for (unsigned int y = 0; y < chars.height; y++) {
    int* bayer_row = bayer_select + (y & 0x1) * 2;
    uint16_t* px = (uint16_t*)img + y * width;
    for (unsigned int x = 0; x < chars.width; x++) {
      uint32_t electron_count;
      electron_count = scene_->GetPixelElectrons()[bayer_row[x & 0x1]];

      // TODO: Better pixel saturation curve?
      electron_count = (electron_count < kSaturationElectrons)
                           ? electron_count
                           : kSaturationElectrons;

      // TODO: Better A/D saturation curve?
      uint16_t raw_count = electron_count * total_gain;
      raw_count =
          (raw_count < chars.max_raw_value) ? raw_count : chars.max_raw_value;

      // Calculate noise value
      // TODO: Use more-correct Gaussian instead of uniform noise
      float photon_noise_var = electron_count * noise_var_gain;
      float noise_stddev = sqrtf_approx(read_noise_var + photon_noise_var);
      // Scaled to roughly match gaussian/uniform noise stddev
      float noise_sample = rand_r(&rand_seed_) * (2.5 / (1.0 + RAND_MAX)) - 1.25;

      raw_count += chars.black_level_pattern[bayer_row[x & 0x1]];
      raw_count += noise_stddev * noise_sample;

      *px++ = raw_count;
    }
    // TODO: Handle this better
    // simulatedTime += mRowReadoutTime;
  }
  ALOGVV("Raw sensor image captured");
}

void EmulatedSensor::CaptureRGB(uint8_t* img, uint32_t width, uint32_t height,
                                uint32_t stride, RGBLayout layout, uint32_t gain,
                                const SensorCharacteristics& chars) {
  ATRACE_CALL();
  float total_gain = gain / 100.0 * GetBaseGainFactor(chars.max_raw_value);
  // In fixed-point math, calculate total scaling from electrons to 8bpp
  int scale64x = 64 * total_gain * 255 / chars.max_raw_value;
  uint32_t inc_h = ceil((float)chars.width / width);
  uint32_t inc_v = ceil((float)chars.height / height);

  for (unsigned int y = 0, outy = 0; y < chars.height; y += inc_v, outy++) {
    scene_->SetReadoutPixel(0, y);
    uint8_t* px = img + outy * stride;
    for (unsigned int x = 0; x < chars.width; x += inc_h) {
      uint32_t r_count, g_count, b_count;
      // TODO: Perfect demosaicing is a cheat
      const uint32_t* pixel = scene_->GetPixelElectrons();
      r_count = pixel[EmulatedScene::R] * scale64x;
      g_count = pixel[EmulatedScene::Gr] * scale64x;
      b_count = pixel[EmulatedScene::B] * scale64x;

      uint8_t r = r_count < 255 * 64 ? r_count / 64 : 255;
      uint8_t g = g_count < 255 * 64 ? g_count / 64 : 255;
      uint8_t b = b_count < 255 * 64 ? b_count / 64 : 255;
      switch (layout) {
        case RGB:
          *px++ = r;
          *px++ = g;
          *px++ = b;
          break;
        case RGBA:
          *px++ = r;
          *px++ = g;
          *px++ = b;
          *px++ = 255;
          break;
        case ARGB:
          *px++ = 255;
          *px++ = r;
          *px++ = g;
          *px++ = b;
          break;
        default:
          ALOGE("%s: RGB layout: %d not supported", __FUNCTION__, layout);
          return;
      }
      for (unsigned int j = 1; j < inc_h; j++) scene_->GetPixelElectrons();
    }
  }
  ALOGVV("RGB sensor image captured");
}

void EmulatedSensor::CaptureYUV420(YCbCrPlanes yuv_layout, uint32_t width,
                                   uint32_t height, uint32_t gain,
                                   float zoom_ratio, bool rotate,
                                   const SensorCharacteristics& chars) {
  ATRACE_CALL();
  float total_gain = gain / 100.0 * GetBaseGainFactor(chars.max_raw_value);
  // Using fixed-point math with 6 bits of fractional precision.
  // In fixed-point math, calculate total scaling from electrons to 8bpp
  const int scale64x =
      kFixedBitPrecision * total_gain * 255 / chars.max_raw_value;
  // Fixed-point coefficients for RGB-YUV transform
  // Based on JFIF RGB->YUV transform.
  // Cb/Cr offset scaled by 64x twice since they're applied post-multiply
  const int rgb_to_y[] = {19, 37, 7};
  const int rgb_to_cb[] = {-10, -21, 32, 524288};
  const int rgb_to_cr[] = {32, -26, -5, 524288};
  // Scale back to 8bpp non-fixed-point
  const int scale_out = 64;
  const int scale_out_sq = scale_out * scale_out;  // after multiplies

  // inc = how many pixels to skip while reading every next pixel
  const float aspect_ratio = static_cast<float>(width) / height;

  // precalculate normalized coordinates and dimensions
  const float norm_left_top = 0.5f - 0.5f / zoom_ratio;
  const float norm_rot_top = norm_left_top;
  const float norm_width = 1 / zoom_ratio;
  const float norm_rot_width = norm_width / aspect_ratio;
  const float norm_rot_height = norm_width;
  const float norm_rot_left =
      norm_left_top + (norm_width + norm_rot_width) * 0.5f;

  for (unsigned int out_y = 0; out_y < height; out_y++) {
    uint8_t* px_y = yuv_layout.img_y + out_y * yuv_layout.y_stride;
    uint8_t* px_cb = yuv_layout.img_cb + (out_y / 2) * yuv_layout.cbcr_stride;
    uint8_t* px_cr = yuv_layout.img_cr + (out_y / 2) * yuv_layout.cbcr_stride;

    for (unsigned int out_x = 0; out_x < width; out_x++) {
      int x, y;
      float norm_x = out_x / (width * zoom_ratio);
      float norm_y = out_y / (height * zoom_ratio);
      if (rotate) {
        x = static_cast<int>(chars.width *
                             (norm_rot_left - norm_y * norm_rot_width));
        y = static_cast<int>(chars.height *
                             (norm_rot_top + norm_x * norm_rot_height));
      } else {
        x = static_cast<int>(chars.width * (norm_left_top + norm_x));
        y = static_cast<int>(chars.height * (norm_left_top + norm_y));
      }
      x = std::min(std::max(x, 0), (int)chars.width - 1);
      y = std::min(std::max(y, 0), (int)chars.height - 1);
      scene_->SetReadoutPixel(x, y);

      int32_t r_count, g_count, b_count;
      // TODO: Perfect demosaicing is a cheat
      const uint32_t* pixel = rotate ? scene_->GetPixelElectronsColumn()
                                     : scene_->GetPixelElectrons();
      r_count = pixel[EmulatedScene::R] * scale64x;
      r_count = r_count < kSaturationPoint ? r_count : kSaturationPoint;
      g_count = pixel[EmulatedScene::Gr] * scale64x;
      g_count = g_count < kSaturationPoint ? g_count : kSaturationPoint;
      b_count = pixel[EmulatedScene::B] * scale64x;
      b_count = b_count < kSaturationPoint ? b_count : kSaturationPoint;

      // Gamma correction
      r_count = gamma_table_[r_count];
      g_count = gamma_table_[g_count];
      b_count = gamma_table_[b_count];

      *px_y++ = (rgb_to_y[0] * r_count + rgb_to_y[1] * g_count +
                 rgb_to_y[2] * b_count) /
                scale_out_sq;
      if (out_y % 2 == 0 && out_x % 2 == 0) {
        *px_cb = (rgb_to_cb[0] * r_count + rgb_to_cb[1] * g_count +
                  rgb_to_cb[2] * b_count + rgb_to_cb[3]) /
                 scale_out_sq;
        *px_cr = (rgb_to_cr[0] * r_count + rgb_to_cr[1] * g_count +
                  rgb_to_cr[2] * b_count + rgb_to_cr[3]) /
                 scale_out_sq;
        px_cr += yuv_layout.cbcr_step;
        px_cb += yuv_layout.cbcr_step;
      }
    }
  }
  ALOGVV("YUV420 sensor image captured");
}

void EmulatedSensor::CaptureDepth(uint8_t* img, uint32_t gain, uint32_t width,
                                  uint32_t height, uint32_t stride,
                                  const SensorCharacteristics& chars) {
  ATRACE_CALL();
  float total_gain = gain / 100.0 * GetBaseGainFactor(chars.max_raw_value);
  // In fixed-point math, calculate scaling factor to 13bpp millimeters
  int scale64x = 64 * total_gain * 8191 / chars.max_raw_value;
  uint32_t inc_h = ceil((float)chars.width / width);
  uint32_t inc_v = ceil((float)chars.height / height);

  for (unsigned int y = 0, out_y = 0; y < chars.height; y += inc_v, out_y++) {
    scene_->SetReadoutPixel(0, y);
    uint16_t* px = (uint16_t*)(img + (out_y * stride));
    for (unsigned int x = 0; x < chars.width; x += inc_h) {
      uint32_t depth_count;
      // TODO: Make up real depth scene instead of using green channel
      // as depth
      const uint32_t* pixel = scene_->GetPixelElectrons();
      depth_count = pixel[EmulatedScene::Gr] * scale64x;

      *px++ = depth_count < 8191 * 64 ? depth_count / 64 : 0;
      for (unsigned int j = 1; j < inc_h; j++) scene_->GetPixelElectrons();
    }
    // TODO: Handle this better
    // simulatedTime += mRowReadoutTime;
  }
  ALOGVV("Depth sensor image captured");
}

status_t EmulatedSensor::ProcessYUV420(const YUV420Frame& input,
                                       const YUV420Frame& output, uint32_t gain,
                                       ProcessType process_type, float zoom_ratio,
                                       bool rotate_and_crop,
                                       const SensorCharacteristics& chars) {
  ATRACE_CALL();
  size_t input_width, input_height;
  YCbCrPlanes input_planes, output_planes;
  std::vector<uint8_t> temp_yuv, temp_output_uv, temp_input_uv;

  switch (process_type) {
    case HIGH_QUALITY:
      CaptureYUV420(output.planes, output.width, output.height, gain, zoom_ratio,
                    rotate_and_crop, chars);
      return OK;
    case REPROCESS:
      input_width = input.width;
      input_height = input.height;
      input_planes = input.planes;

      // libyuv only supports planar YUV420 during scaling.
      // Split the input U/V plane in separate planes if needed.
      if (input_planes.cbcr_step == 2) {
        temp_input_uv.resize(input_width * input_height / 2);
        auto temp_uv_buffer = temp_input_uv.data();
        input_planes.img_cb = temp_uv_buffer;
        input_planes.img_cr = temp_uv_buffer + (input_width * input_height) / 4;
        input_planes.cbcr_stride = input_width / 2;
        if (input.planes.img_cb < input.planes.img_cr) {
          libyuv::SplitUVPlane(input.planes.img_cb, input.planes.cbcr_stride,
                               input_planes.img_cb, input_planes.cbcr_stride,
                               input_planes.img_cr, input_planes.cbcr_stride,
                               input_width / 2, input_height / 2);
        } else {
          libyuv::SplitUVPlane(input.planes.img_cr, input.planes.cbcr_stride,
                               input_planes.img_cr, input_planes.cbcr_stride,
                               input_planes.img_cb, input_planes.cbcr_stride,
                               input_width / 2, input_height / 2);
        }
      }
      break;
    case REGULAR:
    default:
      // Generate the smallest possible frame with the expected AR and
      // then scale using libyuv.
      float aspect_ratio = static_cast<float>(output.width) / output.height;
      zoom_ratio = std::max(1.f, zoom_ratio);
      input_width = EmulatedScene::kSceneWidth * aspect_ratio;
      input_height = EmulatedScene::kSceneHeight;
      temp_yuv.reserve((input_width * input_height * 3) / 2);
      auto temp_yuv_buffer = temp_yuv.data();
      input_planes = {
          .img_y = temp_yuv_buffer,
          .img_cb = temp_yuv_buffer + input_width * input_height,
          .img_cr = temp_yuv_buffer + (input_width * input_height * 5) / 4,
          .y_stride = static_cast<uint32_t>(input_width),
          .cbcr_stride = static_cast<uint32_t>(input_width) / 2,
          .cbcr_step = 1};
      CaptureYUV420(input_planes, input_width, input_height, gain, zoom_ratio,
                    rotate_and_crop, chars);
  }

  output_planes = output.planes;
  // libyuv only supports planar YUV420 during scaling.
  // Treat the output UV space as planar first and then
  // interleave in the second step.
  if (output_planes.cbcr_step == 2) {
    temp_output_uv.resize(output.width * output.height / 2);
    auto temp_uv_buffer = temp_output_uv.data();
    output_planes.img_cb = temp_uv_buffer;
    output_planes.img_cr = temp_uv_buffer + output.width * output.height / 4;
    output_planes.cbcr_stride = output.width / 2;
  }

  auto ret = I420Scale(
      input_planes.img_y, input_planes.y_stride, input_planes.img_cb,
      input_planes.cbcr_stride, input_planes.img_cr, input_planes.cbcr_stride,
      input_width, input_height, output_planes.img_y, output_planes.y_stride,
      output_planes.img_cb, output_planes.cbcr_stride, output_planes.img_cr,
      output_planes.cbcr_stride, output.width, output.height,
      libyuv::kFilterNone);
  if (ret != 0) {
    ALOGE("%s: Failed during YUV scaling: %d", __FUNCTION__, ret);
    return ret;
  }

  // Merge U/V Planes for the interleaved case
  if (output_planes.cbcr_step == 2) {
    if (output.planes.img_cb < output.planes.img_cr) {
      libyuv::MergeUVPlane(output_planes.img_cb, output_planes.cbcr_stride,
                           output_planes.img_cr, output_planes.cbcr_stride,
                           output.planes.img_cb, output.planes.cbcr_stride,
                           output.width / 2, output.height / 2);
    } else {
      libyuv::MergeUVPlane(output_planes.img_cr, output_planes.cbcr_stride,
                           output_planes.img_cb, output_planes.cbcr_stride,
                           output.planes.img_cr, output.planes.cbcr_stride,
                           output.width / 2, output.height / 2);
    }
  }

  return ret;
}

int32_t EmulatedSensor::ApplysRGBGamma(int32_t value, int32_t saturation) {
  float n_value = (static_cast<float>(value) / saturation);
  n_value = (n_value <= 0.0031308f)
                ? n_value * 12.92f
                : 1.055f * pow(n_value, 0.4166667f) - 0.055f;
  return n_value * saturation;
}

}  // namespace android
