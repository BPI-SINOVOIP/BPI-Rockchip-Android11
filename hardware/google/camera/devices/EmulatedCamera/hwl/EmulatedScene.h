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

/**
 * The Scene class implements a simple physical simulation of a scene, using the
 * CIE 1931 colorspace to represent light in physical units (lux).
 *
 * It's fairly approximate, but does provide a scene with realistic widely
 * variable illumination levels and colors over time.
 *
 */

#ifndef HW_EMULATOR_CAMERA2_SCENE_H
#define HW_EMULATOR_CAMERA2_SCENE_H

#include "android/frameworks/sensorservice/1.0/ISensorManager.h"
#include "android/frameworks/sensorservice/1.0/types.h"
#include "utils/Timers.h"

namespace android {

using ::android::frameworks::sensorservice::V1_0::IEventQueue;
using ::android::frameworks::sensorservice::V1_0::IEventQueueCallback;
using ::android::hardware::sensors::V1_0::Event;
using ::android::hardware::Return;
using ::android::hardware::Void;

class EmulatedScene : public RefBase {
 public:
  EmulatedScene(int sensor_width_px, int sensor_height_px,
                float sensor_sensitivity, int sensor_orientation,
                bool is_front_facing);
  ~EmulatedScene();

  void InitializeSensorQueue();

  void Initialize(int sensor_width_px, int sensor_height_px,
                  float sensor_sensitivity);

  // Set the filter coefficients for the red, green, and blue filters on the
  // sensor. Used as an optimization to pre-calculate various illuminance
  // values. Two different green filters can be provided, to account for
  // possible cross-talk on a Bayer sensor. Must be called before
  // calculateScene.
  void SetColorFilterXYZ(float rX, float rY, float rZ, float grX, float grY,
                         float grZ, float gbX, float gbY, float gbZ, float bX,
                         float bY, float bZ);

  // Set time of day (24-hour clock). This controls the general light levels
  // in the scene. Must be called before calculateScene.
  void SetHour(int hour);
  // Get current hour
  int GetHour() const;

  // Set the duration of exposure for determining luminous exposure.
  // Must be called before calculateScene
  void SetExposureDuration(float seconds);

  // Calculate scene information for current hour and the time offset since
  // the hour. Resets pixel readout location to 0,0
  void CalculateScene(nsecs_t time, int32_t handshake_divider);

  // Set sensor pixel readout location.
  void SetReadoutPixel(int x, int y);

  // Get sensor response in physical units (electrons) for light hitting the
  // current readout pixel, after passing through color filters. The readout
  // pixel will be auto-incremented horizontally. The returned array can be
  // indexed with ColorChannels.
  const uint32_t* GetPixelElectrons();

  // Get sensor response in physical units (electrons) for light hitting the
  // current readout pixel, after passing through color filters. The readout
  // pixel will be auto-incremented vertically. The returned array can be
  // indexed with ColorChannels.
  const uint32_t* GetPixelElectronsColumn();

  enum ColorChannels { R = 0, Gr, Gb, B, Y, Cb, Cr, NUM_CHANNELS };

  static const int kSceneWidth = 20;
  static const int kSceneHeight = 20;

 private:
  class SensorHandler : public IEventQueueCallback {
   public:
    SensorHandler(wp<EmulatedScene> scene) : scene_(scene) {
    }

    // IEventQueueCallback interface
    Return<void> onEvent(const Event& e) override;

   private:
    wp<EmulatedScene> scene_;
  };

  void InitiliazeSceneRotation(bool clock_wise);

  int32_t sensor_handle_;
  sp<IEventQueue> sensor_event_queue_;
  std::atomic_uint32_t screen_rotation_;
  uint8_t scene_rot0_[kSceneWidth*kSceneHeight];
  uint8_t scene_rot90_[kSceneWidth*kSceneHeight];
  uint8_t scene_rot180_[kSceneWidth*kSceneHeight];
  uint8_t scene_rot270_[kSceneWidth*kSceneHeight];
  uint8_t *current_scene_;
  int32_t sensor_orientation_;
  bool is_front_facing_;

  // Sensor color filtering coefficients in XYZ
  float filter_r_[3];
  float filter_gr_[3];
  float filter_gb_[3];
  float filter_b_[3];

  int offset_x_, offset_y_;
  int map_div_;

  int handshake_x_, handshake_y_;

  int sensor_width_;
  int sensor_height_;
  int current_x_;
  int current_y_;
  int sub_x_;
  int sub_y_;
  int scene_x_;
  int scene_y_;
  int scene_idx_;
  uint32_t* current_scene_material_;

  int hour_;
  float exposure_duration_;
  float sensor_sensitivity_;  // electrons per lux-second

  enum Materials {
    GRASS = 0,
    GRASS_SHADOW,
    HILL,
    WALL,
    ROOF,
    DOOR,
    CHIMNEY,
    WINDOW,
    SUN,
    SKY,
    MOON,
    NUM_MATERIALS
  };

  uint32_t current_colors_[NUM_MATERIALS * NUM_CHANNELS];

  /**
   * Constants for scene definition. These are various degrees of approximate.
   */

  // Fake handshake parameters. Two shake frequencies per axis, plus magnitude
  // as a fraction of a scene tile, and relative magnitudes for the frequencies
  static const float kHorizShakeFreq1;
  static const float kHorizShakeFreq2;
  static const float kVertShakeFreq1;
  static const float kVertShakeFreq2;
  static const float kFreq1Magnitude;
  static const float kFreq2Magnitude;

  static const float kShakeFraction;

  // Aperture of imaging lens
  static const float kAperture;

  // Sun, moon illuminance levels in 2-hour increments. These don't match any
  // real day anywhere.
  static const uint32_t kTimeStep = 2;
  static const float kSunlight[];
  static const float kMoonlight[];
  static const int kSunOverhead;
  static const int kMoonOverhead;

  // Illumination levels for various conditions, in lux
  static const float kDirectSunIllum;
  static const float kDaylightShadeIllum;
  static const float kSunsetIllum;
  static const float kTwilightIllum;
  static const float kFullMoonIllum;
  static const float kClearNightIllum;
  static const float kStarIllum;
  static const float kLivingRoomIllum;

  // Chromaticity of various illumination sources
  static const float kIncandescentXY[2];
  static const float kDirectSunlightXY[2];
  static const float kDaylightXY[2];
  static const float kNoonSkyXY[2];
  static const float kMoonlightXY[2];
  static const float kSunsetXY[2];

  static const uint8_t kSelfLit;
  static const uint8_t kShadowed;
  static const uint8_t kSky;

  static const float kMaterials_xyY[NUM_MATERIALS][3];
  static const uint8_t kMaterialsFlags[NUM_MATERIALS];

  static const uint8_t kScene[];
};

}  // namespace android

#endif  // HW_EMULATOR_CAMERA2_SCENE_H
