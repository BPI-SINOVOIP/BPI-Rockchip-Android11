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

#ifndef CHRE_POWER_TEST_REQUEST_MANAGER_H_
#define CHRE_POWER_TEST_REQUEST_MANAGER_H_

#include <chre.h>
#include <cinttypes>

#include "chre/util/singleton.h"
#include "chre/util/time.h"
#include "common.h"

namespace chre {

//! Handles requests coming in from the power test host app, enabling /
//! disabling various sensors as necessary.
class RequestManager {
 public:
  /**
   * Processes a message from the host, performing the requested action(s).
   *
   * @param hostMessage the message data received from the host AP
   * @return whether the message was processed successfully
   */
  bool handleMessageFromHost(const chreMessageFromHostData &hostMessage);

  /**
   * Handles a timer event using the cookie to determine what action should be
   * performed.
   *
   * @param cookie if non-null, contains an enum value corresponding to whatever
   *     action should be performed when the timer fires
   */
  void handleTimerEvent(const void *cookie) const;

 private:
  //! Indicates the source that initially set up the timer.
  enum TimerType {
    WAKEUP,
    WIFI,
    CELL,
    NUM_TYPES,
  };

  //! Holds the timer ID for each of the timer types.
  uint32_t mTimerIds[TimerType::NUM_TYPES] = {CHRE_TIMER_INVALID};

  /**
   * Enables or disables break-it mode. When enabled, requests WiFi / GNSS /
   * Cell data at one second intervals, buffers audio at the fastest possible
   * rate and enables all sensors at their fastest sampling rates.
   *
   * @param enable whether to enable the break-it mode
   * @return whether the request was successful
   */
  bool requestBreakIt(bool enable);

  /**
   * Enables / disables audio sampling. If enabled, identifies the primary audio
   * source's minimum buffer duration and requests audio at that rate.
   *
   * @param enable whether to enable audio sampling
   * @return whether the request was successful
   */
  bool requestAudioAtFastestRate(bool enable) const;

  /**
   * Enables / disables a repeating wakeup timer set to fire at the given rate.
   *
   * @param enable whether to enable the wakeup timer
   * @param type the source of the timer request. Used as the cookie that is
   *     given to the nanoapp when the timer fires
   * @param delay amount of time, in nanoseconds, between each timer event
   */
  bool requestTimer(bool enable, TimerType type, Nanoseconds delay);

  /**
   * Performs a Wifi scan. Should be invoked when a timer of TimerType::WIFI
   * fires.
   */
  void wifiTimerCallback() const;

  /**
   * Enables / disables GNSS location sampling.
   *
   * @param enable whether to enable GNSS location sampling
   * @param scanIntervalMillis amount of time, in milliseconds, between each
   *     GNSS location sample
   * @param minTimeToNextFixMillis amount of time, in milliseconds, to wait
   *     before generating the first location fix
   * @return whether the request was successful
   */
  bool requestGnssLocation(bool enable, uint32_t scanIntervalMillis,
                           uint32_t minTimeToNextFixMillis) const;

  /**
   * Requests cell info. Should be invoked when a timer of TimerType::CELL
   * fires.
   */
  void cellTimerCallback() const;

  /**
   * Enables / disables sampling of audio.
   *
   * @param enable whether to enable audio sampling
   * @param bufferDurationNs amount of time, in nanoseconds, to buffer audio
   *     data before delivering to the nanoapp
   * @return whether the request was successful
   */
  bool requestAudio(bool enable, uint64_t bufferDurationNs) const;

  /**
   * Enables / disables sampling of a particular sensor.
   *
   * @param enable whether to enable sensor sampling
   * @param sensorType the type of the sensor to configure
   * @param samplingIntervalNs The sampling rate, in nanoseconds, to configure
   *    the sensor to sample at
   * @param latencyNs The latency, in nanoseconds, between batches of sensor
   *    data. This controls how much data is batched before requiring a delivery
   *    to the nanoapp
   * @return whether the request was successful
   */
  bool requestSensor(bool enable, uint8_t sensorType,
                     uint64_t samplingIntervalNs, uint64_t latencyNs) const;

  /**
   * Enables / disables sampling of all sensors. If enabled, samples all
   * available sensors at their fastest rate.
   *
   * @param enable whether to enable sensor sampling
   * @return whether the request was successful
   */
  bool requestAllSensors(bool enable) const;
};

}  // namespace chre

typedef chre::Singleton<chre::RequestManager> RequestManagerSingleton;

#endif  // CHRE_POWER_TEST_REQUEST_MANAGER_H_