/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once
#include <android/hardware/sensors/2.1/types.h>

namespace goldfish {

namespace ahs = ::android::hardware::sensors;
using SensorInfo = ahs::V2_1::SensorInfo;

constexpr int kSensorHandleAccelerometer = 0;
constexpr int kSensorHandleGyroscope = 1;
constexpr int kSensorHandleMagneticField = 2;
constexpr int kSensorHandleOrientation = 3;
constexpr int kSensorHandleAmbientTemperature = 4;
constexpr int kSensorHandleProximity = 5;
constexpr int kSensorHandleLight = 6;
constexpr int kSensorHandlePressure = 7;
constexpr int kSensorHandleRelativeHumidity = 8;
constexpr int kSensorHandleMagneticFieldUncalibrated = 9;
constexpr int kSensorHandleGyroscopeFieldUncalibrated = 10;
constexpr int kSensorHandleHingeAngle0 = 11;
constexpr int kSensorHandleHingeAngle1 = 12;
constexpr int kSensorHandleHingeAngle2 = 13;

int getSensorNumber();
bool isSensorHandleValid(int h);
const SensorInfo* getSensorInfoByHandle(int h);
const char* getQemuSensorNameByHandle(int h);

}  // namespace goldfish
