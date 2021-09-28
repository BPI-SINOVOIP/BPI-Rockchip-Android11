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

#ifndef CHRE_PAL_SENSOR_H_
#define CHRE_PAL_SENSOR_H_

#include <cstdbool>
#include <cstdint>

#include "chre/pal/system.h"
#include "chre/pal/version.h"
#include "chre_api/chre/sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initial version of the CHRE sensor PAL, tied to CHRE API v1.3.
 */
#define CHRE_PAL_SENSOR_API_V1_3 CHRE_PAL_CREATE_API_VERSION(1, 3)

// v1.0-v1.2 skipped to avoid confusion with older versions of the CHRE API.
#define CHRE_PAL_SENSOR_API_CURRENT_VERSION CHRE_PAL_SENSOR_API_V1_3

/**
 * ID value that must be returned from flush() if request IDs are not
 * supported by this PAL.
 */
#define CHRE_PAL_SENSOR_FLUSH_UNSUPPORTED_REQUEST_ID UINT32_MAX

/**
 * The amount of time, in seconds, that the getSensors() method can block for
 * before returning.
 */
#define CHRE_PAL_SENSOR_SENSOR_INIT_TIMEOUT_SEC 45

struct chrePalSensorCallbacks {
  /**
   * Invoked whenever a sensor's sampling status changes or when
   * chrePalSensorApi.configureSensor is initially invoked for a given sensor.
   *
   * All fields in the provided status must represent the current sampling
   * status of the sensor.
   *
   * This function call passes ownership of the status memory to the core CHRE
   * system, i.e. the PAL module must not modify the referenced data until the
   * associated API function is called to release the memory.
   *
   * @param sensorInfoIndex The index into the array returned by
   *     chrePalSensorApi.getSensors indicating the sensor this status update
   *     corresponds to.
   * @param status The latest sampling status for the given sensor. The PAL must
   *     ensure this memory remains accessible until
   *     releaseSamplingStatusEvent() is invoked.
   *
   * @see chrePalSensorApi.configureSensor
   */
  void (*samplingStatusUpdateCallback)(uint32_t sensorInfoIndex,
                                       struct chreSensorSamplingStatus *status);

  /**
   * Callback used to pass new sensor data that's been generated for the sensor
   * specified by the sensorInfoIndex.
   *
   * The event data format is one of the chreSensorXXXData defined in the CHRE
   * API, implicitly specified by the sensor type.
   *
   * This function call passes ownership of the data memory to the core CHRE
   * system, i.e. the PAL module must not modify the referenced data until the
   * associated API function is called to release the memory.
   *
   * @param sensorInfoIndex The index into the array returned by
   *     chrePalSensorApi.getSensors indicating the sensor this sensor data
   *     corresponds to.
   * @param data The sensor data in one of the chreSensorXXXData formats as
   *     required by the sensor type for the given sensor. The PAL must ensure
   *     this memory remains accessible until releaseSensorDataEvent() is
   *     invoked.
   */
  void (*dataEventCallback)(uint32_t sensorInfoIndex, void *data);

  /**
   * Invoked whenever a sensor bias event is generated or when bias events have
   * been enabled for a given sensor and the latest bias values need to be
   * delivered.
   *
   * @param sensorInfoIndex The index into the array returned by
   *     chrePalSensorApi.getSensors indicating the sensor this bias data
   *     corresponds to.
   * @param biasData The bias information in one of the chreSensorXXXData
   *     formats as required by the sensor type for the given sensor. The PAL
   *     must ensure this memory remains accessible until releaseBiasEvent()
   *     is invoked.
   *
   * @see chrePalSensorApi.configureBiasEvents
   */
  void (*biasEventCallback)(uint32_t sensorInfoIndex, void *biasData);

  /**
   * Invoked whenever a request issued through chrePalSensorApi.flush has
   * completed.
   *
   * This callback must be invoked no later than
   * CHRE_SENSOR_FLUSH_COMPLETE_TIMEOUT_NS after chrePalSensorApi.flush was
   * called.
   *
   * @param sensorInfoIndex The index into the array returned by
   *     chrePalSensorApi.getSensors indicating the sensor this flush completion
   *     corresponds to.
   * @param flushRequestId UID corresponding to what the PAL returned when the
   *     flush was requested. Can be set to
   *     CHRE_PAL_SENSOR_FLUSH_UNSUPPORTED_REQUEST_ID if the PAL implementation
   *     doesn't support flush requests.
   * @param errorCode Value from the chreError enum specifying any error that
   *     occurred while processing the flush request. CHRE_ERROR_NONE if
   *     the request was successful.
   *
   * @see chrePalSensorApi.flush
   */
  void (*flushCompleteCallback)(uint32_t sensorInfoIndex,
                                uint32_t flushRequestId, uint8_t errorCode);
};

struct chrePalSensorApi {
  /**
   * Version of the module providing this API. This value must be
   * constructed from CHRE_PAL_CREATE_MODULE_VERSION using the supported
   * API version constant (CHRE_PAL_SENSOR_API_*) and the module-specific patch
   * version.
   */
  uint32_t moduleVersion;

  /**
   * Initializes the sensor module. Initialization must complete synchronously.
   *
   * @param systemApi Structure containing CHRE system function pointers which
   *        the PAL implementation should prefer to use over equivalent
   *        functionality exposed by the underlying platform. The module does
   *        not need to deep-copy this structure; its memory remains
   *        accessible at least until after close() is called.
   * @param callbacks Structure containing entry points to the core CHRE
   *        system. The module does not need to deep-copy this structure; its
   *        memory remains accessible at least until after close() is called.
   *
   * @return true if initialization was successful, false otherwise
   */
  bool (*open)(const struct chrePalSystemApi *systemApi,
               const struct chrePalSensorCallbacks *callbacks);

  /**
   * Performs clean shutdown of the sensor module, usually done in preparation
   * for stopping the CHRE. The sensor module must end any active requests to
   * ensure that it will not invoke any callbacks past this point, and
   * complete any relevant teardown activities before returning from this
   * function. The PAL must also free any memory (e.g. the sensor array if it
   * was dynamically allocated) inside this function.
   */
  void (*close)();

  /**
   * Creates a chreSensorInfo struct for every CHRE-supported sensor that is
   * able to be communicated with via the PAL and places them into an array that
   * is passed to the framework via the sensors parameter. The memory used for
   * the array is owned by the PAL and may be statically or dynamically
   * allocated. If the PAL dynamically allocates the array, it must be freed
   * when close() is invoked.
   *
   * This function must block until all CHRE-supported sensors are able to be
   * communicated with via the PAL and all info required by the chreSensorInfo
   * struct has been retrieved. No more than
   * CHRE_PAL_SENSOR_SENSOR_INIT_TIMEOUT_SEC must pass between the time this
   * method is invoked and when it returns.
   *
   * If more than CHRE_PAL_SENSOR_SENSOR_INIT_TIMEOUT_SEC pass, this function
   * must return with an array that includes as many sensors as the PAL was able
   * to ensure could be communicated with during that time.
   *
   * If the PAL supports multiple sensors of the same sensor type, the default
   * sensor should be listed first in the returned array.
   *
   * This method will only be invoked once during the CHRE framework's
   * initialization process.
   *
   * @param sensors A non-null pointer to an array that must be initialized
   *     and populated with all sensors the PAL needs to make available to the
   *     CHRE framework on this device.
   * @param arraySize The size of the filled in sensors array.
   * @return false if any errors occurred while attempting to discover sensors.
   *     true if all sensors are available. If false, the sensors array may be
   *     partially filled.
   */
  bool (*getSensors)(struct chreSensorInfo *const *sensors,
                     uint32_t *arraySize);

  /**
   * Configures the sensor specified by the given index into the array returned
   * by getSensors() following the same set of requirements as
   * chreSensorConfigure().
   *
   * It's guaranteed that only one request from CHRE will be issued for any
   * sensor at a time. If a new request is issued for a sensor with an active
   * request, the CHRE framework expects it would override the existing one.
   *
   * Additionally, the CHRE framework will verify the issued request is valid
   * by checking against the chreSensorInfo for the issued sensor so the PAL
   * can and should avoid duplicating these checks to remove extra logic.
   *
   * Once the request is accepted by the PAL, any sensor data generated by the
   * sensor must be sent via the dataEventCallback(). Additionally, if this
   * request enables the sensor, the samplingStatusUpdateCallback() must be
   * invoked after processing CHRE's request to provide the latest sampling
   * status.
   *
   * As mentioned in chreSensorConfigure, bias event delivery must be enabled
   * for calibrated sensor types automatically. An added recommendation is that
   * the PAL should deliver the bias data at the same interval that sensor data
   * is delivered, in order to optimize for power, with the bias data being
   * delivered first so that nanoapps are easily able to translate sensor data
   * if necessary. If bias events are not delivered at the same interval as the
   * sensor data, they should be delivered as close to the corresponding sensor
   * data as possible to reduce the amount of time nanoapps need to remember
   * multiple bias updates.
   *
   * @param sensorInfoIndex The index into the array provided via getSensors()
   *     indicating the sensor that must be configured.
   * @param mode chreSensorConfigureMode enum value specifying which mode the
   *     sensor must be configured for.
   * @param intervalNs The interval, in nanoseconds, at which events must be
   *     generated by the sensor.
   * @param latencyNs The maximum latency, in nanoseconds, allowed before the
   *     PAL begins delivery of events. This will control how many events can be
   *     queued by the sensor before requiring a delivery event.
   * @return true if the configuration succeeded, false otherwise.
   */
  bool (*configureSensor)(uint32_t sensorInfoIndex,
                          enum chreSensorConfigureMode mode,
                          uint64_t intervalNs, uint64_t latencyNs);

  /**
   * Issues a request to flush all samples stored for batching. The PAL
   * implementation is expected to blindly issue this request for the given
   * sensor. The CHRE framework will ensure that it must have an active,
   * powered, batching request issued through configureSensor before invoking
   * this method.
   *
   * It's strongly recommended that PAL implementations support request IDs per
   * flush request to allow the framework to issue multiple flush requests per
   * sensor and remove the need for queueing in CHRE.
   *
   * Once the PAL accepts the flush request, it's required that the earlier
   * provided flushCompleteCallback() will be invoked once the flush request
   * has completed. The PAL is responsible for not allowing more than
   * CHRE_SENSOR_FLUSH_COMPLETE_TIMEOUT_NS to pass after this flush request has
   * been issued before invoking the flushCompleteCallback(). If more than
   * CHRE_SENSOR_FLUSH_COMPLETE_TIMEOUT_NS pass, the PAL is required to invoke
   * flushCompleteCallback() with an errorCode value of CHRE_ERROR_TIMEOUT to
   * allow the CHRE framework to recover from the timeout.
   *
   * @param sensorInfoIndex The index into the array provided via getSensors()
   *     indicating the sensor that must have its samples flushed
   * @param flushRequestId A pointer where a UID will be stored identify this
   *     flush request. Must be set to
   *     CHRE_PAL_SENSOR_FLUSH_UNSUPPORTED_REQUEST_ID if request IDs are not
   *     supported by this PAL. This value must be passed into
   *     flushCompleteCallback() when the flush request is completed.
   * @return true if the request was accepted for processing, false otherwise.
   *
   * @see chreSensorFlushAsync
   */
  bool (*flush)(uint32_t sensorInfoIndex, uint32_t *flushRequestId);

  /**
   * Configures the reception of bias events for the specified sensor.
   *
   * It's required that this implementation meets the same requirements as
   * chreSensorConfigureBiasEvents() with one key difference:
   * - The framework will only enable bias events for a given sensor after it
   *   has placed an active request through configureSensor() and will ensure
   *   bias events are not enabled for sensors that don't have an active request
   *   in place.
   *
   * Additionally, as specified for configureSensor(), it is recommended that
   * the PAL should deliver the bias data at the same interval that sensor data
   * is delivered, in order to optimize for power, with the bias data being
   * delivered first so that nanoapps are easily able to translate sensor data
   * if necessary. If bias events are not delivered at the same interval as the
   * sensor data, they should be delivered as close to the corresponding sensor
   * data as possible to reduce the amount of time nanoapps need to remember
   * multiple bias updates.
   *
   * Once bias delivery is enabled for a given sensor, the PAL is required to
   * invoke biasEventCallback() with the latest bias and any future bias updates
   * received from the sensor.
   *
   * @param sensorInfoIndex The index into the array provided via getSensors()
   *     indicating the sensor that bias events must be enabled / disabled for
   * @param enable whether to enable or disable bias event delivery
   * @param latencyNs The maximum latency, in nanoseconds, allowed before the
   *     PAL begins delivery of events. This will control how many events can be
   *     queued before requiring a delivery event. This value will match
   *     the latency requested for sensor data through configureSensor()
   */
  bool (*configureBiasEvents)(uint32_t sensorInfoIndex, bool enable,
                              uint64_t latencyNs);

  /**
   * Synchronously provides the most recent bias info available for a sensor.
   *
   * The implementation of this function must meet the same requirements as
   * specified for chreSensorGetThreeAxisBias().
   *
   * @param sensorInfoIndex The index into the array provided via getSensors()
   *     indicating the sensor that bias info must be returned for
   * @param bias A pointer to where the bias will be stored
   * @return true if the bias was successfully stored, false if the
   *    sensorInfoIndex is invalid or the sensor doesn't support three axis bias
   *    delivery
   */
  bool (*getThreeAxisBias)(uint32_t sensorInfoIndex,
                           struct chreSensorThreeAxisData *bias);

  /**
   * Invoked when the core CHRE system no longer needs a status update event
   * that was provided via dataEventCallback()
   *
   * @param data Sensor data to release
   */
  void (*releaseSensorDataEvent)(void *data);

  /**
   * Invoked when the core CHRE system no longer needs a status update event
   * that was provided via samplingStatusUpdateCallback()
   *
   * @param status Sampling status update to release
   */
  void (*releaseSamplingStatusEvent)(struct chreSensorSamplingStatus *status);

  /**
   * Invoked when the core CHRE system no longer needs a bias event that was
   * provided via biasEventCallback()
   *
   * @param bias Bias data to release
   */
  void (*releaseBiasEvent)(void *bias);
};

/**
 * Retrieve a handle for the CHRE sensor PAL.
 *
 * @param requestedApiVersion The implementation of this function must return a
 *        pointer to a structure with the same major version as requested.
 * @return Pointer to API handle, or NULL if a compatible API version is not
 *         supported by the module, or the API as a whole is not implemented. If
 *         non-NULL, the returned API handle must be valid as long as this
 *         module is loaded.
 */
const struct chrePalSensorApi *chrePalSensorGetApi(
    uint32_t requestedApiVersion);

#ifdef __cplusplus
}
#endif

#endif  // CHRE_PAL_SENSOR_H_
