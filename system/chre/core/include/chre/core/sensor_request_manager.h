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

#ifndef CHRE_CORE_SENSOR_REQUEST_MANAGER_H_
#define CHRE_CORE_SENSOR_REQUEST_MANAGER_H_

#include "chre/core/sensor.h"
#include "chre/core/sensor_request.h"
#include "chre/core/sensor_request_multiplexer.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/platform_sensor_manager.h"
#include "chre/platform/system_time.h"
#include "chre/platform/system_timer.h"
#include "chre/util/non_copyable.h"
#include "chre/util/optional.h"
#include "chre/util/system/debug_dump.h"

namespace chre {

/**
 * Handles requests from nanoapps for sensor data and information. This includes
 * multiplexing multiple requests into one for the platform to handle.
 *
 * This class is effectively a singleton as there can only be one instance of
 * the PlatformSensorManager instance.
 */
class SensorRequestManager : public NonCopyable {
 public:
  /**
   * Destructs the sensor request manager and releases platform sensor resources
   * if requested.
   */
  ~SensorRequestManager();

  /**
   * Initializes the underlying platform-specific sensors. Must be called
   * prior to invoking any other methods in this class.
   */
  void init();

  /**
   * Determines whether the runtime is aware of a given sensor type. The
   * supplied sensorHandle is only populated if the sensor type is known.
   *
   * @param sensorType The type of the sensor.
   * @param sensorHandle A non-null pointer to a uint32_t to use as a sensor
   *                     handle for nanoapps.
   * @return true if the supplied sensor type is available for use.
   */
  bool getSensorHandle(uint8_t sensorType, uint32_t *sensorHandle) const;

  /**
   * Sets a sensor request for the given nanoapp for the provided sensor handle.
   * If the nanoapp has made a previous request, it is replaced by this request.
   * If the request changes the mode to SensorMode::Off the request is removed.
   *
   * @param nanoapp A non-null pointer to the nanoapp requesting this change.
   * @param sensorHandle The sensor handle for which this sensor request is
   *        directed at.
   * @param request The new sensor request for this nanoapp.
   * @return true if the request was set successfully. If the sensorHandle is
   *         out of range or the platform sensor fails to update to the new
   *         request false will be returned.
   */
  bool setSensorRequest(Nanoapp *nanoapp, uint32_t sensorHandle,
                        const SensorRequest &sensorRequest);

  /**
   * Populates the supplied info struct if the sensor handle exists.
   *
   * @param sensorHandle The handle of the sensor.
   * @param nanoapp The nanoapp requesting this change.
   * @param info A non-null pointer to a chreSensorInfo struct.
   * @return true if the supplied sensor handle exists.
   */
  bool getSensorInfo(uint32_t sensorHandle, const Nanoapp &nanoapp,
                     struct chreSensorInfo *info) const;
  /*
   * Removes all requests of a sensorType and unregisters all nanoapps for its
   * events.
   *
   * @param sensorHandle The handle of the sensor.
   * @return true if all requests of the sensor type have been successfully
   *         removed.
   */
  bool removeAllRequests(uint32_t sensorHandle);

  /**
   * Obtains a pointer to the Sensor of the specified sensorHandle.
   *
   * NOTE: Some platform implementations invoke this method from different
   * threads assuming the underlying list of sensors doesn't change after
   * initialization.
   *
   * @param sensorHandle The sensor handle corresponding to the sensor.
   * @return A pointer to the Sensor, or nullptr if sensorHandle is invalid.
   */
  Sensor *getSensor(uint32_t sensorHandle);

  /**
   * Populates the supplied sampling status struct if the sensor handle exists.
   *
   * @param sensorHandle The handle of the sensor.
   * @param status A non-null pointer to a chreSensorSamplingStatus struct.
   * @return true if the supplied sensor handle exists.
   */
  bool getSensorSamplingStatus(uint32_t sensorHandle,
                               struct chreSensorSamplingStatus *status) const;

  /**
   * Obtains the list of open requests of the specified sensor handle.
   *
   * @param sensorHandle The handle of the sensor.
   * @return The list of open requests of this sensor in a DynamicVector.
   */
  const DynamicVector<SensorRequest> &getRequests(uint32_t sensorHandle) const;

  /**
   * Configures a nanoapp to receive bias events.
   *
   * @param nanoapp A non-null pointer to the nanoapp making this request.
   * @param sensorHandle The handle of the sensor to receive bias events for.
   * @param enable true to enable bias event reporting.
   *
   * @return true if the configuration was successful.
   */
  bool configureBiasEvents(Nanoapp *nanoapp, uint32_t sensorHandle,
                           bool enable);

  /**
   * Synchronously retrieves the current bias for a sensor that supports
   * data in the chreSensorThreeAxisData format.
   *
   * @param sensorHandle The handle of the sensor to retrieve bias data for.
   * @param bias A non-null pointer to store the current bias data.
   *
   * @return false if the sensor handle was invalid or the sensor does not
   *     report bias data in the chreSensorThreeAxisData format.
   */
  bool getThreeAxisBias(uint32_t sensorHandle,
                        struct chreSensorThreeAxisData *bias) const;

  /**
   * Makes a sensor flush request for a nanoapp asynchronously.
   *
   * @param nanoapp A non-null pointer to the nanoapp requesting this change.
   * @param sensorHandle The sensor handle for which this sensor request is
   *        directed at.
   * @param cookie An opaque pointer to data that will be used in the
   *        chreSensorFlushCompleteEvent.
   *
   * @return true if the request was accepted, false otherwise
   */
  bool flushAsync(Nanoapp *nanoapp, uint32_t sensorHandle, const void *cookie);

  /**
   * Invoked by the PlatformSensorManager when a flush complete event is
   * received for a given sensor for a request done through flushAsync(). This
   * method can be invoked from any thread, and defers processing the event to
   * the main CHRE event loop.
   *
   * @param sensorHandle The sensor handle that completed flushing.
   * @param flushRequestId The ID of the flush request that completed. Should
   *     be set to UINT32_MAX if request IDs are not supported by the platform.
   * @param errorCode An error code from enum chreError
   */
  void handleFlushCompleteEvent(uint32_t sensorHandle, uint32_t flushRequestId,
                                uint8_t errorCode);

  /**
   * Invoked by the PlatformSensorManager when a sensor event is received for a
   * given sensor. This method should be invoked from the same thread.
   *
   * @param sensorHandle The sensor handle this data event is from.
   * @param event the event data formatted as one of the chreSensorXXXData
   *     defined in the CHRE API, implicitly specified by sensorHandle.
   */
  void handleSensorDataEvent(uint32_t sensorHandle, void *event);

  /**
   * Invoked by the PlatformSensorManager when a sensor's sampling status
   * changes. This method can be invoked from any thread.
   *
   * @param sensorHandle The handle that corresponds to the sensor this update
   *     is for.
   * @param status The status update for the given sensor.
   */
  void handleSamplingStatusUpdate(uint32_t sensorHandle,
                                  struct chreSensorSamplingStatus *status);

  /**
   * Invoked by the PlatformSensorManager when a bias update has been received
   * for a sensor. This method can be invoked from any thread.
   *
   * @param sensorHandle The handle that corresponds to the sensor this update
   *     is for.
   * @param bias The bias update for the given sensor.
   */
  void handleBiasEvent(uint32_t sensorHandle, void *biasData);

  /**
   * Prints state in a string buffer. Must only be called from the context of
   * the main CHRE thread.
   *
   * @param debugDump The debug dump wrapper where a string can be printed
   *     into one of the buffers.
   */
  void logStateToBuffer(DebugDumpWrapper &debugDump) const;

  /**
   * Releases the sensor data event back to the platform. Also removes any
   * requests for a one-shot sensor if the sensor type corresponds to a one-shot
   * sensor.
   *
   * @param eventType the sensor event type that was sent and now needs to be
   *     released.
   * @param eventData the event data to be released back to the platform.
   */
  void releaseSensorDataEvent(uint16_t eventType, void *eventData);

  /**
   * Releases the bias data back to the platform.
   *
   * @param biasData the bias data to be released back to the platform.
   */
  void releaseBiasData(void *biasData) {
    mPlatformSensorManager.releaseBiasEvent(biasData);
  }

  /**
   * Releases the sampling status updated back to the platform.
   *
   * @param status the status to be released back to the platform.
   */
  void releaseSamplingStatusUpdate(struct chreSensorSamplingStatus *status) {
    mPlatformSensorManager.releaseSamplingStatusUpdate(status);
  }

 private:
  //! An internal structure to store incoming sensor flush requests
  struct FlushRequest {
    FlushRequest(uint32_t handle, uint32_t id, const void *cookiePtr) {
      sensorHandle = handle;
      nanoappInstanceId = id;
      cookie = cookiePtr;
    }

    //! The timestamp at which this request should complete.
    Nanoseconds deadlineTimestamp =
        SystemTime::getMonotonicTime() +
        Nanoseconds(CHRE_SENSOR_FLUSH_COMPLETE_TIMEOUT_NS);
    //! The sensor handle this flush request is for.
    uint32_t sensorHandle;
    //! The ID of the nanoapp that requested the flush.
    uint32_t nanoappInstanceId;
    //! The opaque pointer provided in flushAsync().
    const void *cookie;
    //! True if this flush request is active and is pending completion.
    bool isActive = false;
  };

  //! An internal structure to store sensor request logs
  struct SensorRequestLog {
    SensorRequestLog(Nanoseconds timestampIn, uint32_t instanceIdIn,
                     uint8_t sensorTypeIn, SensorMode modeIn,
                     Nanoseconds intervalIn, Nanoseconds latencyIn)
        : timestamp(timestampIn),
          interval(intervalIn),
          latency(latencyIn),
          instanceId(instanceIdIn),
          sensorType(sensorTypeIn),
          mode(modeIn) {}

    Nanoseconds timestamp;
    Nanoseconds interval;
    Nanoseconds latency;
    uint32_t instanceId;
    uint8_t sensorType;
    SensorMode mode;
  };

  //! The list of all sensors
  DynamicVector<Sensor> mSensors;

  //! The list of logged sensor requests
  static constexpr size_t kMaxSensorRequestLogs = 8;
  ArrayQueue<SensorRequestLog, kMaxSensorRequestLogs> mSensorRequestLogs;

  //! A queue of flush requests made by nanoapps.
  static constexpr size_t kMaxFlushRequests = 16;
  FixedSizeVector<FlushRequest, kMaxFlushRequests> mFlushRequestQueue;

  PlatformSensorManager mPlatformSensorManager;

  /**
   * Makes a specified flush request, and sets the timeout timer appropriately.
   * If there already is a pending flush request for the sensor specified in
   * the request, then this method does nothing.
   *
   * @param request the request to make
   *
   * @return An error code from enum chreError
   */
  uint8_t makeFlushRequest(FlushRequest &request);

  /**
   * Make a flush request through PlatformSensorManager.
   *
   * @param sensor The sensor to flush.
   * @return true if the flush request was successfully made.
   */
  bool doMakeFlushRequest(Sensor &sensor);

  /**
   * Removes all requests and consolidates all the maximal request changes
   * into one sensor configuration update.
   *
   * @param sensor The sensor to clear all requests for.
   * @return true if all the requests have been removed and sensor
   *         configuration successfully updated.
   */
  bool removeAllRequests(Sensor &sensor);

  /**
   * Removes a sensor request from the given lists of requests. The provided
   * index must fall in the range of the sensor requests available.
   *
   * @param sensor The sensor to remove the request from.
   * @param removeIndex The index to remove the request from.
   * @param requestChanged A non-null pointer to a bool to indicate that the
   *        net request made to the sensor has changed. This boolean is always
   *        assigned to the status of the request changing (true or false).
   * @return true if the remove operation was successful.
   */
  bool removeRequest(Sensor &sensor, size_t removeIndex, bool *requestChanged);

  /**
   * Adds a new sensor request to the given list of requests.
   *
   * @param sensor The sensor to add the request to.
   * @param request The request to add to the multiplexer.
   * @param requestChanged A non-null pointer to a bool to indicate that the
   *        net request made to the sensor has changed. This boolean is always
   *        assigned to the status of the request changing (true or false).
   * @return true if the add operation was successful.
   */
  bool addRequest(Sensor &sensor, const SensorRequest &request,
                  bool *requestChanged);

  /**
   * Updates a sensor request in the given list of requests. The provided index
   * must fall in range of the sensor requests managed by the multiplexer.
   *
   * @param sensor The sensor that will be updated.
   * @param updateIndex The index to update the request at.
   * @param request The new sensor request to replace the existing request
   *        with.
   * @param requestChanged A non-null pointer to a bool to indicate that the
   *        net request made to the sensor has changed. This boolean is always
   *        assigned to the status of the request changing (true or false).
   * @return true if the update operation was successful.
   */
  bool updateRequest(Sensor &sensor, size_t updateIndex,
                     const SensorRequest &request, bool *requestChanged);

  /**
   * Posts an event to a nanoapp indicating the completion of a flush request.
   *
   * @param sensorHandle The handle of the sensor for this event.
   * @param errorCode An error code from enum chreError
   * @param request The corresponding FlushRequest.
   */
  void postFlushCompleteEvent(uint32_t sensorHandle, uint8_t errorCode,
                              const FlushRequest &request);

  /**
   * Completes a flush request at the specified index by posting a
   * CHRE_EVENT_SENSOR_FLUSH_COMPLETE event with the specified errorCode,
   * removing the request from the queue, cleaning up states as necessary.
   *
   * @param index The index of the flush request.
   * @param errorCode The error code to send the completion event with.
   */
  void completeFlushRequestAtIndex(size_t index, uint8_t errorCode);

  /**
   * Dispatches the next flush request for the given sensor. If there are no
   * more pending flush requests, this method does nothing.
   *
   * @param sensorHandle The handle of the sensor to dispatch a new flush
   *     request for.
   */
  void dispatchNextFlushRequest(uint32_t sensorHandle);

  /**
   * A method that is called whenever a flush request times out.
   *
   * @param sensorHandle The sensor handle of the flush request.
   */
  void onFlushTimeout(uint32_t sensorHandle);

  /**
   * Handles a complete event for a sensor flush requested through flushAsync.
   * See handleFlushCompleteEvent which may be called from any thread. This
   * method is intended to be invoked on the CHRE event loop thread.
   *
   * @param errorCode An error code from enum chreError
   * @param sensorHandle The handle of the sensor that has completed the flush.
   */
  void handleFlushCompleteEventSync(uint8_t errorCode, uint32_t sensorHandle);

  /**
   * Cancels all pending flush requests for a given sensor and nanoapp.
   *
   * @param sensorHandle The sensor handle indicating the sensor to cancel flush
   *     requests for.
   * @param nanoappInstanceId The ID of the nanoapp to cancel requests for,
   *     kSystemInstanceId to remove requests for all nanoapps.
   */
  void cancelFlushRequests(uint32_t sensorHandle,
                           uint32_t nanoappInstanceId = kSystemInstanceId);

  /**
   * Adds a request log to the list of logs possibly pushing latest log
   * off if full.
   *
   * @param nanoappInstanceId Instance ID of the nanoapp that made the request.
   * @param sensorType The sensor type of requested sensor.
   * @param sensorRequest The SensorRequest object holding params about
   *    request.
   */
  void addSensorRequestLog(uint32_t nanoappInstanceId, uint8_t sensorType,
                           const SensorRequest &sensorRequest);

  /**
   * Helper function to make a sensor's maximal request to the platform, and
   * reset the last event if an on-change sensor is successfully turned off.
   *
   * @param sensor The sensor that will be making the request.
   * @return true if the platform accepted the request.
   */
  bool configurePlatformSensor(Sensor &sensor);
};

}  // namespace chre

#endif  // CHRE_CORE_SENSOR_REQUEST_MANAGER_H_
