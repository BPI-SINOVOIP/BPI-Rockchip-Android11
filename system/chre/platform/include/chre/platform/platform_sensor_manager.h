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

#ifndef CHRE_PLATFORM_PLATFORM_SENSOR_MANAGER_H_
#define CHRE_PLATFORM_PLATFORM_SENSOR_MANAGER_H_

#include "chre/core/sensor.h"
#include "chre/pal/sensor.h"
#include "chre/target_platform/platform_sensor_manager_base.h"
#include "chre/util/dynamic_vector.h"

namespace chre {

/**
 * Handles communicating with all CHRE-supported sensors in the system at the
 * behest of the core framework while also managing the receipt of various
 * sensor events that CHRE is able to process.
 */
class PlatformSensorManager : public PlatformSensorManagerBase {
 public:
  ~PlatformSensorManager();

  /**
   * Initializes the manager implementation. This is called at a later stage of
   * initialization than the constructor, so implementations are encouraged to
   * put any blocking initialization here.
   */
  void init();

  /**
   * Constructs Sensor objects for every CHRE-supported sensor in the system,
   * and returns them in a DynamicVector. This method is only invoked once
   * during initialization of the CHRE framework.
   *
   * @return A DynamicVector to populate with the list of sensors the framework
   *     can send requests to.
   */
  DynamicVector<Sensor> getSensors();

  /**
   * Sends the sensor request to the provided sensor. The request issued through
   * this method must be a valid request based on the properties of the given
   * sensor.
   *
   * If setting this new request fails due to a transient failure (example:
   * inability to communicate with the sensor) false will be returned.
   *
   * If a request's latency is lower than its interval, the request is assumed
   * to have a latency of 0 and samples should be delivered to CHRE as soon
   * as they become available.
   * TODO(b/142958445): Make the above modification to the request before it
   * reaches the platform code.
   *
   * @param sensor One of the sensors provided by getSensors().
   * @param request The new request that contains the details about how the
   *     sensor should be configured.
   * @return true if the sensor was successfully configured with the supplied
   *     request.
   */
  bool configureSensor(Sensor &sensor, const SensorRequest &request);

  /**
   * Configures the reception of bias events for a specified sensor.
   *
   * It is recommended that the platform deliver the bias data at the same
   * interval that sensor data is delivered, in order to optimize for power,
   * with the bias data being delivered first so that nanoapps are easily able
   * to translate sensor data if necessary. If bias events are not delivered at
   * the same interval as the sensor data, they should be delivered as close to
   * the corresponding sensor data as possible to reduce the amount of time
   * nanoapps need to remember multiple bias updates. Additonally, an enable
   * request must only be issued if a sensor has already been enabled through
   * configureSensor().
   *
   * @param sensor One of the sensors provided by getSensors().
   * @param enable whether to enable or disable bias event delivery
   * @param latencyNs The maximum latency, in nanoseconds, allowed before the
   *     PAL begins delivery of events. This will control how many events can be
   *     queued before requiring a delivery event. This value will match
   *     the latency requested for sensor data through configureSensor()
   * @return true if the sensor was successfully configured with the supplied
   *     parameters.
   */
  bool configureBiasEvents(const Sensor &sensor, bool enable,
                           uint64_t latencyNs);

  /**
   * Synchronously retrieves the current bias for a sensor that supports
   * data in the chreSensorThreeAxisData format. If the current bias hasn't been
   * received for the given sensor, this method will store data with a bias of 0
   * and the accuracy field in chreSensorDataHeader set to
   * CHRE_SENSOR_ACCURACY_UNKNOWN per the CHRE API requirements.
   *
   * @param sensor One of the sensors provided by getSensors().
   * @param bias A non-null pointer to store the current bias data.
   * @return false if sensor does not report bias data in the
   *     chreSensorThreeAxisData format.
   */
  bool getThreeAxisBias(const Sensor &sensor,
                        struct chreSensorThreeAxisData *bias) const;

  /**
   * Makes a flush request for the given sensor. When a flush request made by
   * this method is completed (i.e. all pending samples are posted to the CHRE
   * event queue), PlatformSensorManager must invoke
   * SensorRequestManager::handleFlushCompleteEvent().
   *
   * @param sensor One of the sensors provided by getSensors().
   * @param flushRequestId A pointer where a UID will be stored identify this
   *     flush request. Must be set to UINT32_MAX if request IDs are not
   *     supported by this platform. This value must be passed into
   *     flushCompleteCallback() when the flush request is completed.
   * @return true if the request was accepted.
   */
  bool flush(const Sensor &sensor, uint32_t *flushRequestId);

  //! Methods that allow the platform to free the data given via the below
  //! event handlers
  void releaseSamplingStatusUpdate(struct chreSensorSamplingStatus *status);
  void releaseSensorDataEvent(void *data);
  void releaseBiasEvent(void *biasData);
};

}  // namespace chre

#endif  // CHRE_PLATFORM_PLATFORM_SENSOR_MANAGER_H_
