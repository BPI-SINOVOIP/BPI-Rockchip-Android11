/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "chre/core/sensor.h"

#include "chre/core/event_loop_manager.h"
#include "chre_api/chre/version.h"

namespace chre {

Sensor::Sensor(Sensor &&other)
    : PlatformSensor(std::move(other)), mFlushRequestPending(false) {
  *this = std::move(other);
}

Sensor &Sensor::operator=(Sensor &&other) {
  PlatformSensor::operator=(std::move(other));

  mSensorRequests = std::move(other.mSensorRequests);

  mFlushRequestTimerHandle = other.mFlushRequestTimerHandle;
  other.mFlushRequestTimerHandle = CHRE_TIMER_INVALID;

  mFlushRequestPending = other.mFlushRequestPending.load();
  other.mFlushRequestPending = false;

  mLastEvent = other.mLastEvent;
  other.mLastEvent = nullptr;

  mLastEventValid = other.mLastEventValid;
  other.mLastEventValid = false;

  return *this;
}

Sensor::~Sensor() {
  if (mLastEvent != nullptr) {
    LOGD("Releasing lastEvent: sensor %s, size %zu", getSensorName(),
         getLastEventSize());
    memoryFree(mLastEvent);
  }
}

void Sensor::init() {
  size_t lastEventSize = getLastEventSize();
  if (lastEventSize > 0) {
    mLastEvent = static_cast<ChreSensorData *>(memoryAlloc(lastEventSize));
    if (mLastEvent == nullptr) {
      FATAL_ERROR("Failed to allocate last event memory for %s",
                  getSensorName());
    }
  }
}

void Sensor::populateSensorInfo(struct chreSensorInfo *info,
                                uint32_t targetApiVersion) const {
  info->sensorType = getSensorType();
  info->isOnChange = isOnChange();
  info->isOneShot = isOneShot();
  info->reportsBiasEvents = reportsBiasEvents();
  info->supportsPassiveMode = supportsPassiveMode();
  info->unusedFlags = 0;
  info->sensorName = getSensorName();

  // minInterval was added in CHRE API v1.1 - do not attempt to populate for
  // nanoapps targeting v1.0 as their struct will not be large enough
  if (targetApiVersion >= CHRE_API_VERSION_1_1) {
    info->minInterval = getMinInterval();
  }
}

void Sensor::clearPendingFlushRequest() {
  cancelPendingFlushRequestTimer();
  mFlushRequestPending = false;
}

void Sensor::cancelPendingFlushRequestTimer() {
  if (mFlushRequestTimerHandle != CHRE_TIMER_INVALID) {
    EventLoopManagerSingleton::get()->cancelDelayedCallback(
        mFlushRequestTimerHandle);
    mFlushRequestTimerHandle = CHRE_TIMER_INVALID;
  }
}

void Sensor::setLastEvent(ChreSensorData *event) {
  if (event == nullptr) {
    mLastEventValid = false;
  } else {
    CHRE_ASSERT(event->header.readingCount > 0);

    SensorTypeHelpers::getLastSample(getSensorType(), event, mLastEvent);
    mLastEventValid = true;
  }
}

bool Sensor::getSamplingStatus(struct chreSensorSamplingStatus *status) const {
  CHRE_ASSERT(status != nullptr);

  memcpy(status, &mSamplingStatus, sizeof(*status));
  return true;
}

void Sensor::setSamplingStatus(const struct chreSensorSamplingStatus &status) {
  mSamplingStatus = status;
}

}  // namespace chre
