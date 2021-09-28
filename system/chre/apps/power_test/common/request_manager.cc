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

#include "request_manager.h"

#include "chre/util/macros.h"
#include "chre/util/nanoapp/audio.h"
#include "chre/util/nested_data_ptr.h"
#include "generated/chre_power_test_generated.h"

namespace chre {
namespace {

//! List of all sensor types that can be interacted with from the nanoapp.
constexpr uint8_t kAllSensorTypes[] = {
    CHRE_SENSOR_TYPE_ACCELEROMETER,
    CHRE_SENSOR_TYPE_GYROSCOPE,
    CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE,
    CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD,
    CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD,
    CHRE_SENSOR_TYPE_PRESSURE,
    CHRE_SENSOR_TYPE_LIGHT,
    CHRE_SENSOR_TYPE_PROXIMITY,
    CHRE_SENSOR_TYPE_STEP_DETECT,
    CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER,
    CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE,
    CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE,
    CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE,
    CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT,
    CHRE_SENSOR_TYPE_STATIONARY_DETECT,
};

/**
 * Retrieve the configure mode for the given sensor type.
 *
 * @param sensorType The type of the sensor
 * @return The sensor configure mode for the given sensor type
 */
chreSensorConfigureMode getModeForSensorType(uint8_t sensorType) {
  chreSensorConfigureMode mode;
  switch (sensorType) {
    case CHRE_SENSOR_TYPE_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_GYROSCOPE:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD:
    case CHRE_SENSOR_TYPE_PRESSURE:
    case CHRE_SENSOR_TYPE_LIGHT:
    case CHRE_SENSOR_TYPE_PROXIMITY:
    case CHRE_SENSOR_TYPE_STEP_DETECT:
    case CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER:
    case CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE:
    case CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE:
    case CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD_TEMPERATURE:
      mode = CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS;
      break;
    case CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT:
    case CHRE_SENSOR_TYPE_STATIONARY_DETECT:
      mode = CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT;
      break;
    default:
      LOGE("Mode requested for unhandled sensor type %" PRIu8
           " defaulting to continuous",
           sensorType);
      mode = CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS;
  }
  return mode;
}

/**
 * Verifies a given message from the host is a valid message to the nanoapp.
 *
 * @param hostMessage message being delivered from the host
 * @param verifiedMessage if verification is successful, contains the decoded
 *     message from the host. Otherwise, is uninitialized.
 * @return true if the message was verified to be a valid.
 */
template <class MessageClass>
bool verifyMessage(const chreMessageFromHostData &hostMessage,
                   const MessageClass **verifiedMessage) {
  flatbuffers::Verifier verifier(
      static_cast<const uint8_t *>(hostMessage.message),
      hostMessage.messageSize);
  bool verified = verifier.VerifyBuffer<MessageClass>(nullptr);
  if (verified) {
    *verifiedMessage = flatbuffers::GetRoot<MessageClass>(hostMessage.message);
  } else {
    LOGE("Failed to verify %s message from host",
         power_test::EnumNameMessageType(
             static_cast<power_test::MessageType>(hostMessage.messageType)));
  }
  return verified;
}

}  // namespace

using power_test::AudioRequestMessage;
using power_test::BreakItMessage;
using power_test::CellQueryMessage;
using power_test::GnssLocationMessage;
using power_test::MessageType;
using power_test::NanoappResponseMessage;
using power_test::SensorRequestMessage;
using power_test::TimerMessage;
using power_test::WifiScanMessage;

bool RequestManager::requestTimer(bool enable, TimerType type,
                                  Nanoseconds delay) {
  bool success = false;
  if (enable) {
    NestedDataPtr<TimerType> timerType(type);
    uint32_t timerId = chreTimerSet(delay.toRawNanoseconds(), timerType.dataPtr,
                                    false /* oneShot */);
    if (timerId != CHRE_TIMER_INVALID) {
      success = true;
    }
    mTimerIds[type] = timerId;
  } else {
    success = chreTimerCancel(mTimerIds[type]);
  }
  LOGI("RequestTimer success %d, enable %d, type %d, delay %" PRIu64, success,
       enable, type, delay.toRawNanoseconds());
  return success;
}

void RequestManager::wifiTimerCallback() const {
  bool success = chreWifiRequestScanAsyncDefault(nullptr /* cookie */);
  LOGI("Requested WiFi - success %d", success);
}

bool RequestManager::requestGnssLocation(
    bool enable, uint32_t scanIntervalMillis,
    uint32_t minTimeToNextFixMillis) const {
  bool success;
  if (enable) {
    success = chreGnssLocationSessionStartAsync(
        scanIntervalMillis, minTimeToNextFixMillis, nullptr /* cookie */);
  } else {
    success = chreGnssLocationSessionStopAsync(nullptr /* cookie */);
  }
  LOGI("RequestGnss success %d, enable %d, scanIntervalMillis %" PRIu32
       " minTimeToNextFixMillis %" PRIu32,
       success, enable, scanIntervalMillis, minTimeToNextFixMillis);
  return success;
}

void RequestManager::cellTimerCallback() const {
  bool success = chreWwanGetCellInfoAsync(nullptr /* cookie */);
  LOGI("Requested Cell - success %d", success);
}

bool RequestManager::requestAudio(bool enable,
                                  uint64_t bufferDurationNs) const {
  bool success;

  if (enable) {
    // Only request audio data from the first source
    // TODO: Request audio data from all available sources (or allow configuring
    // which source to sample from)
    success = chreAudioConfigureSource(0 /* handle */, true /* enable */,
                                       bufferDurationNs, bufferDurationNs);
  } else {
    success = chreAudioConfigureSource(0 /* handle */, false /* enable */,
                                       0 /* bufferDuration */,
                                       0 /* deliveryInterval */);
  }
  LOGI("RequestAudio success %d, enable %d, bufferDurationNs %" PRIu64, success,
       enable, bufferDurationNs);
  return success;
}

bool RequestManager::requestSensor(bool enable, uint8_t sensorType,
                                   uint64_t samplingIntervalNs,
                                   uint64_t latencyNs) const {
  uint32_t sensorHandle;
  bool success = chreSensorFindDefault(sensorType, &sensorHandle);

  if (success) {
    if (enable) {
      success =
          chreSensorConfigure(sensorHandle, getModeForSensorType(sensorType),
                              samplingIntervalNs, latencyNs);
    } else {
      success = chreSensorConfigureModeOnly(sensorHandle,
                                            CHRE_SENSOR_CONFIGURE_MODE_DONE);
    }
  }

  LOGI("RequestSensor success %d, enable %d, sensorType %" PRIu8
       " samplingIntervalNs %" PRIu64 " latencyNs %" PRIu64,
       success, enable, sensorType, samplingIntervalNs, latencyNs);
  return success;
}

bool RequestManager::requestAllSensors(bool enable) const {
  bool success = true;
  uint32_t sensorHandle;
  struct chreSensorInfo sensorInfo;
  for (uint8_t i = 0; i < ARRAY_SIZE(kAllSensorTypes); i++) {
    success &= chreSensorFindDefault(kAllSensorTypes[i], &sensorHandle) &&
               chreGetSensorInfo(sensorHandle, &sensorInfo) &&
               requestSensor(enable, kAllSensorTypes[i], sensorInfo.minInterval,
                             CHRE_SENSOR_LATENCY_ASAP);
  }

  LOGI("requestAllSensors success %d enable %d", success, enable);
  return success;
}

bool RequestManager::requestAudioAtFastestRate(bool enable) const {
  struct chreAudioSource audioSource;
  bool success = chreAudioGetSource(0 /* handle */, &audioSource);
  if (success) {
    LOGI("Found audio source '%s' with %" PRIu32 "Hz %s data", audioSource.name,
         audioSource.sampleRate,
         chre::getChreAudioFormatString(audioSource.format));
    LOGI("  buffer duration: [%" PRIu64 "ns, %" PRIu64 "ns]",
         audioSource.minBufferDuration, audioSource.maxBufferDuration);
    success &= requestAudio(enable, audioSource.minBufferDuration);
  }

  LOGI("requestAudioAtFastestRate success %d enable %d", success, enable);
  return success;
}

bool RequestManager::requestBreakIt(bool enable) {
  bool success = requestTimer(enable, TimerType::WIFI, Seconds(1));
  success &= requestGnssLocation(enable, chre::kOneSecondInNanoseconds,
                                 0 /* minTimeToNextFixMillis */);
  success &= requestTimer(enable, TimerType::CELL, Seconds(1));
  success &= requestAudioAtFastestRate(enable);
  success &= requestAllSensors(enable);
  LOGI("RequestBreakIt success %d enable %d", success, enable);
  return success;
}

void RequestManager::handleTimerEvent(const void *cookie) const {
  if (cookie != nullptr) {
    NestedDataPtr<TimerType> timerType;
    timerType.dataPtr = const_cast<void *>(cookie);
    switch (timerType.data) {
      case TimerType::WAKEUP:
        LOGI("Received a wakeup timer event");
        break;
      case TimerType::WIFI:
        wifiTimerCallback();
        break;
      case TimerType::CELL:
        cellTimerCallback();
        break;
      default:
        LOGE("Invalid timer type received %d", timerType.data);
    }
  }
}

bool RequestManager::handleMessageFromHost(
    const chreMessageFromHostData &hostMessage) {
  bool success = false;
  if (hostMessage.message == nullptr) {
    LOGE("Host message from %" PRIu16 " has empty message",
         hostMessage.hostEndpoint);
  } else {
    switch (static_cast<MessageType>(hostMessage.messageType)) {
      case MessageType::TIMER_TEST: {
        const TimerMessage *msg;
        if (verifyMessage<TimerMessage>(hostMessage, &msg)) {
          success = requestTimer(msg->enable(), TimerType::WAKEUP,
                                 Nanoseconds(msg->wakeup_interval_ns()));
        }
        break;
      }
      case MessageType::WIFI_SCAN_TEST: {
        const WifiScanMessage *msg;
        if (verifyMessage<WifiScanMessage>(hostMessage, &msg)) {
          success = requestTimer(msg->enable(), TimerType::WIFI,
                                 Nanoseconds(msg->scan_interval_ns()));
        }
        break;
      }
      case MessageType::GNSS_LOCATION_TEST: {
        const GnssLocationMessage *msg;
        if (verifyMessage<GnssLocationMessage>(hostMessage, &msg)) {
          success =
              requestGnssLocation(msg->enable(), msg->scan_interval_millis(),
                                  msg->min_time_to_next_fix_millis());
        }
        break;
      }
      case MessageType::CELL_QUERY_TEST: {
        const CellQueryMessage *msg;
        if (verifyMessage<CellQueryMessage>(hostMessage, &msg)) {
          success = requestTimer(msg->enable(), TimerType::CELL,
                                 Nanoseconds(msg->query_interval_ns()));
        }
        break;
      }
      case MessageType::AUDIO_REQUEST_TEST: {
        const AudioRequestMessage *msg;
        if (verifyMessage<AudioRequestMessage>(hostMessage, &msg)) {
          success = requestAudio(msg->enable(), msg->buffer_duration_ns());
        }
        break;
      }
      case MessageType::SENSOR_REQUEST_TEST: {
        const SensorRequestMessage *msg;
        if (verifyMessage<SensorRequestMessage>(hostMessage, &msg)) {
          success =
              requestSensor(msg->enable(), static_cast<uint8_t>(msg->sensor()),
                            msg->sampling_interval_ns(), msg->latency_ns());
        }
        break;
      }
      case MessageType::BREAK_IT_TEST: {
        const BreakItMessage *msg;
        if (verifyMessage<BreakItMessage>(hostMessage, &msg)) {
          success = requestBreakIt(msg->enable());
        }
        break;
      }
      default:
        LOGE("Received unknown host message %" PRIu32, hostMessage.messageType);
    }
  }
  return success;
}

}  // namespace chre