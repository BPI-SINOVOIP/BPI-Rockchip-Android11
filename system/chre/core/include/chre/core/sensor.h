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

#ifndef CHRE_CORE_SENSOR_H_
#define CHRE_CORE_SENSOR_H_

#include "chre/core/sensor_request_multiplexer.h"
#include "chre/core/sensor_type_helpers.h"
#include "chre/core/timer_pool.h"
#include "chre/platform/atomic.h"
#include "chre/platform/platform_sensor.h"
#include "chre/util/optional.h"

namespace chre {

/**
 * Represents a sensor in the system that is exposed to nanoapps in CHRE.
 *
 * Note that like chre::Nanoapp, this class uses inheritance to separate the
 * common code (Sensor) from common interface with platform-specific
 * implementation (PlatformSensor). However, this inheritance relationship does
 * *not* imply polymorphism, and this object must only be referred to via the
 * most-derived type, i.e. chre::Sensor.
 */
class Sensor : public PlatformSensor {
 public:
  /**
   * Constructs a sensor in an unspecified state. Should not be called directly
   * by common code, as platform-specific initialization of the Sensor object is
   * required for it to be usable.
   *
   * @see PlatformSensorManager::getSensors
   */
  Sensor() : mFlushRequestPending(false) {}

  Sensor(Sensor &&other);
  Sensor &operator=(Sensor &&other);

  ~Sensor();

  /**
   * Initializes various Sensor class state. The platform implementation is
   * responsible for invoking this after any base class state necessary for
   * PlatformSensor methods to work is set up.
   */
  void init();

  /**
   * @return true if the sensor is currently enabled.
   */
  bool isSensorEnabled() const {
    return !mSensorRequests.getRequests().empty();
  }

  /**
   * @return A const reference to the maximal request.
   */
  const SensorRequest &getMaximalRequest() const {
    return mSensorRequests.getCurrentMaximalRequest();
  }

  /**
   * @return A reference to the list of all active requests for this sensor.
   */
  const DynamicVector<SensorRequest> &getRequests() const {
    return mSensorRequests.getRequests();
  }

  /**
   * @return A reference to the multiplexer for all active requests for this
   *     sensor.
   */
  SensorRequestMultiplexer &getRequestMultiplexer() {
    return mSensorRequests;
  }

  /**
   * @return Whether this sensor is a one-shot sensor.
   */
  bool isOneShot() const {
    return SensorTypeHelpers::isOneShot(getSensorType());
  }

  /**
   * @return Whether this sensor is an on-change sensor.
   */
  bool isOnChange() const {
    return SensorTypeHelpers::isOnChange(getSensorType());
  }

  /**
   * @return Whether this sensor is a continuous sensor.
   */
  bool isContinuous() const {
    return SensorTypeHelpers::isContinuous(getSensorType());
  }

  /**
   * @return Whether this sensor is calibrated.
   */
  bool isCalibrated() const {
    return SensorTypeHelpers::isCalibrated(getSensorType());
  }

  /**
   * @param eventType A non-null pointer to the event type to populate
   * @return true if this sensor has a bias event type.
   */
  bool getBiasEventType(uint16_t *eventType) const {
    return SensorTypeHelpers::getBiasEventType(getSensorType(), eventType);
  }

  /**
   * Gets the sensor info of this sensor in the CHRE API format.
   *
   * @param info A non-null pointer to the chreSensor info to populate.
   * @param targetApiVersion CHRE_API_VERSION_ value corresponding to the API
   *     that the info struct should be populated for.
   */
  void populateSensorInfo(struct chreSensorInfo *info,
                          uint32_t targetApiVersion) const;

  /**
   * Clears any states (e.g. timeout timer and relevant flags) associated
   * with a pending flush request.
   */
  void clearPendingFlushRequest();

  /**
   * Cancels the pending timeout timer associated with a flush request.
   */
  void cancelPendingFlushRequestTimer();

  /**
   * Sets the timer handle used to time out an active flush request for this
   * sensor.
   *
   * @param handle Timer handle for the current flush request.
   */
  void setFlushRequestTimerHandle(TimerHandle handle) {
    mFlushRequestTimerHandle = handle;
  }

  /**
   * Sets whether a flush request is pending or not.
   *
   * @param pending bool indicating whether a flush is pending.
   */
  void setFlushRequestPending(bool pending) {
    mFlushRequestPending = pending;
  }

  /**
   * @return true if a flush is pending.
   */
  bool isFlushRequestPending() const {
    return mFlushRequestPending;
  }

  /**
   * @return Pointer to this sensor's last data event. It returns a nullptr if
   *         the sensor doesn't provide it.
   */
  ChreSensorData *getLastEvent() const {
    return mLastEventValid ? mLastEvent : nullptr;
  }

  /**
   * Extracts the last sample from the supplied event to the sensor's last event
   * memory and marks last event valid. This method must be invoked within the
   * CHRE thread before the event containing the sensor data is delivered to
   * nanoapps.
   *
   * @param event The pointer to the event to update from. If nullptr, the last
   *      event is marked invalid.
   */
  void setLastEvent(ChreSensorData *event);

  /**
   * Marks the last event invalid.
   */
  void clearLastEvent() {
    mLastEventValid = false;
  }

  /**
   * Gets the current status of this sensor in the CHRE API format.
   *
   * @param status A non-null pointer to chreSensorSamplingStatus to populate
   * @return true if the sampling status has been successfully obtained.
   */
  bool getSamplingStatus(struct chreSensorSamplingStatus *status) const;

  /**
   * Sets the current status of this sensor in the CHRE API format.
   *
   * @param status The current sampling status.
   */
  void setSamplingStatus(const struct chreSensorSamplingStatus &status);

  const char *getSensorTypeName() const {
    return SensorTypeHelpers::getSensorTypeName(getSensorType());
  }

 private:
  size_t getLastEventSize() {
    return SensorTypeHelpers::getLastEventSize(getSensorType());
  }

  //! The latest sampling status provided by the sensor.
  struct chreSensorSamplingStatus mSamplingStatus = {};

  //! Set to true only when this sensor is currently active and we have a copy
  //! of the most recent event in lastEvent.
  bool mLastEventValid = false;

  //! The most recent event received for this sensor. Only enough memory is
  //! allocated to store the sensor data for this particular sensor (a.k.a.
  //! don't attempt to use other fields in this union).
  ChreSensorData *mLastEvent = nullptr;

  //! The multiplexer for all requests for this sensor.
  SensorRequestMultiplexer mSensorRequests;

  //! The timeout timer handle for the current flush request.
  TimerHandle mFlushRequestTimerHandle = CHRE_TIMER_INVALID;

  //! True if a flush request is pending for this sensor.
  AtomicBool mFlushRequestPending;
};

}  // namespace chre

#endif  // CHRE_CORE_SENSOR_H_
