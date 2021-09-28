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

#include "chre/platform/platform_sensor_manager.h"

#include <cinttypes>

#include "chre/core/event_loop_manager.h"
#include "chre/platform/log.h"
#include "chre/platform/shared/pal_system_api.h"

namespace chre {

const chrePalSensorCallbacks PlatformSensorManagerBase::sSensorCallbacks = {
    PlatformSensorManager::samplingStatusUpdateCallback,
    PlatformSensorManager::dataEventCallback,
    PlatformSensorManager::biasEventCallback,
    PlatformSensorManager::flushCompleteCallback,
};

PlatformSensorManager::~PlatformSensorManager() {
  if (mSensorApi != nullptr) {
    LOGD("Platform sensor manager closing");
    mSensorApi->close();
    LOGD("Platform sensor manager closed");
  }
}

void PlatformSensorManager::init() {
  mSensorApi = chrePalSensorGetApi(CHRE_PAL_SENSOR_API_CURRENT_VERSION);
  if (mSensorApi != nullptr) {
    if (!mSensorApi->open(&gChrePalSystemApi, &sSensorCallbacks)) {
      LOGE("Sensor PAL open returned false");
      mSensorApi = nullptr;
    } else {
      LOGD("Opened Sensor PAL version 0x%08" PRIx32, mSensorApi->moduleVersion);
    }
  } else {
    LOGW("Requested Sensor PAL (version 0x%08" PRIx32 ") not found",
         CHRE_PAL_SENSOR_API_CURRENT_VERSION);
  }
}

DynamicVector<Sensor> PlatformSensorManager::getSensors() {
  DynamicVector<Sensor> sensors;
  struct chreSensorInfo *palSensors = nullptr;
  uint32_t arraySize;
  if (mSensorApi != nullptr) {
    if (!mSensorApi->getSensors(&palSensors, &arraySize) || arraySize == 0) {
      LOGE("Failed to query the platform for sensors");
    } else if (!sensors.reserve(arraySize)) {
      LOG_OOM();
    } else {
      for (uint32_t i = 0; i < arraySize; i++) {
        struct chreSensorInfo *sensor = &palSensors[i];
        sensors.push_back(Sensor());
        sensors[i].initBase(sensor, i /* sensorHandle */);
        if (sensor->sensorName != nullptr) {
          LOGD("Found sensor: %s", sensor->sensorName);
        } else {
          LOGD("Sensor at index %" PRIu32 " has type %" PRIu8, i,
               sensor->sensorType);
        }
      }
    }
  }
  return sensors;
}

bool PlatformSensorManager::configureSensor(Sensor &sensor,
                                            const SensorRequest &request) {
  bool success = false;
  if (mSensorApi != nullptr) {
    success = mSensorApi->configureSensor(
        sensor.getSensorHandle(),
        getConfigureModeFromSensorMode(request.getMode()),
        request.getInterval().toRawNanoseconds(),
        request.getLatency().toRawNanoseconds());
  }
  return success;
}

bool PlatformSensorManager::configureBiasEvents(const Sensor &sensor,
                                                bool enable,
                                                uint64_t latencyNs) {
  bool success = false;
  if (mSensorApi != nullptr) {
    success = mSensorApi->configureBiasEvents(sensor.getSensorHandle(), enable,
                                              latencyNs);
  }
  return success;
}

bool PlatformSensorManager::getThreeAxisBias(
    const Sensor &sensor, struct chreSensorThreeAxisData *bias) const {
  bool success = false;
  if (mSensorApi != nullptr) {
    success = mSensorApi->getThreeAxisBias(sensor.getSensorHandle(), bias);
  }
  return success;
}

bool PlatformSensorManager::flush(const Sensor &sensor,
                                  uint32_t *flushRequestId) {
  bool success = false;
  if (mSensorApi != nullptr) {
    success = mSensorApi->flush(sensor.getSensorHandle(), flushRequestId);
  }
  return success;
}

void PlatformSensorManagerBase::samplingStatusUpdateCallback(
    uint32_t sensorHandle, struct chreSensorSamplingStatus *status) {
  EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .handleSamplingStatusUpdate(sensorHandle, status);
}

void PlatformSensorManagerBase::dataEventCallback(uint32_t sensorHandle,
                                                  void *data) {
  EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .handleSensorDataEvent(sensorHandle, data);
}

void PlatformSensorManagerBase::biasEventCallback(uint32_t sensorHandle,
                                                  void *biasData) {
  EventLoopManagerSingleton::get()->getSensorRequestManager().handleBiasEvent(
      sensorHandle, biasData);
}

void PlatformSensorManagerBase::flushCompleteCallback(uint32_t sensorHandle,
                                                      uint32_t flushRequestId,
                                                      uint8_t errorCode) {
  EventLoopManagerSingleton::get()
      ->getSensorRequestManager()
      .handleFlushCompleteEvent(sensorHandle, flushRequestId, errorCode);
}

void PlatformSensorManager::releaseSamplingStatusUpdate(
    struct chreSensorSamplingStatus *status) {
  mSensorApi->releaseSamplingStatusEvent(status);
}

void PlatformSensorManager::releaseSensorDataEvent(void *data) {
  mSensorApi->releaseSensorDataEvent(data);
}

void PlatformSensorManager::releaseBiasEvent(void *biasData) {
  mSensorApi->releaseBiasEvent(biasData);
}

}  // namespace chre
