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

#include "sns_std_sensor.pb.h"
#include "stringl.h"

#include <cmath>

#include "chre/core/event_loop_manager.h"
#include "chre/core/sensor.h"
#include "chre/core/sensor_type_helpers.h"
#include "chre/platform/log.h"
#include "chre/platform/slpi/power_control_util.h"
#include "chre/platform/system_time.h"
#include "chre_api/chre/sensor.h"

#ifdef CHREX_SENSOR_SUPPORT
#include "chre/extensions/platform/slpi/see/vendor_data_types.h"
#endif  // CHREX_SENSOR_SUPPORT

#ifdef CHRE_VARIANT_SUPPLIES_SEE_SENSORS_LIST
#include "see_sensors.h"
#endif  // CHRE_VARIANT_SUPPLIES_SEE_SENSORS_LIST

#ifndef CHRE_SEE_NUM_TEMP_SENSORS
// There are usually more than one 'sensor_temperature' sensors in SEE.
// Define this in the variant-specific makefile to avoid missing sensors in
// sensor discovery.
#error "CHRE_SEE_NUM_TEMP_SENSORS is not defined"
#endif

namespace chre {
namespace {

//! A struct to facilitate sensor discovery
struct SuidAttr {
  sns_std_suid suid;
  SeeAttributes attr;
};

#ifndef CHRE_VARIANT_SUPPLIES_SEE_SENSORS_LIST

//! The list of SEE platform sensor data types that CHRE intends to support.
//! The standardized strings are defined in sns_xxx.proto.
const char *kSeeDataTypes[] = {
    "accel",
    "gyro",
    "mag",
    "pressure",
    "ambient_light",
    "proximity",
#ifdef CHRE_SLPI_DEFAULT_BUILD
    // Both instant motion and stationary detect share the same data type.
    "amd",
    "amd",
#else
    "motion_detect", "stationary_detect",
#endif
};

#endif  // CHRE_VARIANT_SUPPLIES_SEE_SENSORS_LIST

#ifdef CHRE_SLPI_UIMG_ENABLED
#ifndef CHREX_SENSOR_SUPPORT
// The current implementation uses vendor sensor type 3 to remap into accel,
// with requests made through QMI instead of QSockets, as SEE does not support
// micro-image batching in QCM.
#error "CHRE extensions are required for micro-image SEE support"
#endif  // CHREX_SENSOR_SUPPORT

bool isBigImageSensorType(uint8_t sensorType) {
  return (sensorType == CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_ACCEL ||
          sensorType == CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_ACCEL ||
          sensorType == CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_GYRO ||
          sensorType == CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_MAG ||
          sensorType == CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_LIGHT);
}

/**
 * Obtains the big-image sensor type given the specified data type and whether
 * the sensor is runtime-calibrated or not.
 */
bool getBigImageSensorTypeFromDataType(const char *dataType, bool calibrated,
                                       uint8_t *sensorType) {
  bool found = true;
  if (strcmp(dataType, "accel") == 0) {
    if (calibrated) {
      *sensorType = CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_ACCEL;
    } else {
      *sensorType = CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_ACCEL;
    }
  } else if (strcmp(dataType, "gyro") == 0 && !calibrated) {
    *sensorType = CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_GYRO;
  } else if (strcmp(dataType, "mag") == 0 && !calibrated) {
    *sensorType = CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_UNCAL_MAG;
  } else if (strcmp(dataType, "ambient_light") == 0 && calibrated) {
    *sensorType = CHRE_SLPI_SENSOR_TYPE_BIG_IMAGE_LIGHT;
  } else {
    found = false;
  }
  return found;
}

/**
 * Obtains the micro-image sensor type given the specified sensor type.
 *
 * @param sensorType The sensor type to convert from.
 * @return The associated micro-image sensor type, or the input sensor type
 *     if not associated with one
 */
uint8_t getUimgSensorType(uint8_t sensorType) {
  switch (sensorType) {
    case (CHRE_SENSOR_TYPE_VENDOR_START + 3):
      return CHRE_SENSOR_TYPE_ACCELEROMETER;
    case (CHRE_SENSOR_TYPE_VENDOR_START + 6):
      return CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER;
    case (CHRE_SENSOR_TYPE_VENDOR_START + 7):
      return CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE;
    case (CHRE_SENSOR_TYPE_VENDOR_START + 8):
      return CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD;
    case (CHRE_SENSOR_TYPE_VENDOR_START + 9):
      return CHRE_SENSOR_TYPE_LIGHT;
    default:
      return sensorType;
  }
}

#endif  // CHRE_SLPI_UIMG_ENABLED

#ifndef CHRE_LOG_ONLY_NO_SENSOR
/**
 * Callback function which will run after a delay if a required sensor is not
 * found.
 */
void crashAfterSensorNotFoundCallback(uint16_t /* eventType */,
                                      void * /* eventData */) {
  FATAL_ERROR("Missing required sensor(s)");
}
#endif

void handleMissingSensor() {
  // Try rebooting if a sensor is missing, which might help recover from a
  // transient failure/race condition at startup. But to avoid endless crashes,
  // only do this within 15 seconds of the timeout on initializing SEE - we rely
  // on knowledge that getMonotonicTime() maps into QTimer here, and QTimer only
  // resets when the entire system is rebooted (it continues increasing after
  // SLPI SSR).
#ifndef CHRE_LOG_ONLY_NO_SENSOR
  if (SystemTime::getMonotonicTime() < (kDefaultSeeWaitTimeout + Seconds(15))) {
    Nanoseconds delay(5 * Seconds(60).toRawNanoseconds());  // 5 minutes
    EventLoopManagerSingleton::get()->setDelayedCallback(
        SystemCallbackType::DelayedFatalError, nullptr,
        crashAfterSensorNotFoundCallback, delay);
  }
#endif
  LOGE("Missing required sensor(s)");
}

/**
 * Obtains the sensor type given the specified data type and whether the sensor
 * is runtime-calibrated or not.
 *
 * @return Whether a sensor type was found for the given data type.
 */
bool getSensorTypeFromDataType(const char *dataType, bool calibrated,
                               uint8_t *sensorType) {
  bool success = true;
  if (strcmp(dataType, "accel") == 0) {
    if (calibrated) {
      *sensorType = CHRE_SENSOR_TYPE_ACCELEROMETER;
    } else {
      *sensorType = CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER;
    }
  } else if (strcmp(dataType, "gyro") == 0) {
    if (calibrated) {
      *sensorType = CHRE_SENSOR_TYPE_GYROSCOPE;
    } else {
      *sensorType = CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE;
    }
  } else if (strcmp(dataType, "mag") == 0) {
    if (calibrated) {
      *sensorType = CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD;
    } else {
      *sensorType = CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD;
    }
  } else if (strcmp(dataType, "pressure") == 0) {
    *sensorType = CHRE_SENSOR_TYPE_PRESSURE;
  } else if (strcmp(dataType, "ambient_light") == 0) {
    *sensorType = CHRE_SENSOR_TYPE_LIGHT;
  } else if (strcmp(dataType, "proximity") == 0) {
    *sensorType = CHRE_SENSOR_TYPE_PROXIMITY;
  } else if (strcmp(dataType, "motion_detect") == 0 ||
             strcmp(dataType, "amd") == 0) {
    *sensorType = CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT;
  } else if (strcmp(dataType, "stationary_detect") == 0) {
    *sensorType = CHRE_SENSOR_TYPE_STATIONARY_DETECT;
  } else if (strcmp(dataType, "step_detect") == 0) {
    *sensorType = CHRE_SENSOR_TYPE_STEP_DETECT;
  } else {
#ifdef CHREX_SENSOR_SUPPORT
    success = extension::vendorSensorTypeFromDataType(dataType, calibrated,
                                                      sensorType);
  }
#else
    success = false;
  }
#endif
  return success;
}

/**
 * Merges a status update with an existing sampling status.
 */
void mergeUpdatedStatus(
    uint32_t sensorHandle,
    const SeeHelperCallbackInterface::SamplingStatusData &update,
    struct chreSensorSamplingStatus *mergedStatus) {
  Sensor *sensor = getSensorRequestManager().getSensor(sensorHandle);
  sensor->getSamplingStatus(mergedStatus);

  if (update.enabledValid) {
    mergedStatus->enabled = update.status.enabled;
  }
  if (update.intervalValid) {
    mergedStatus->interval = update.status.interval;
  }
  if (update.latencyValid) {
    mergedStatus->latency = update.status.latency;
  }
}

/**
 * Helper function to post a bias event given the bias data.
 *
 * @param sensorType The sensor type to post the event for.
 * @param bias The bias data.
 */
void postSensorBiasEvent(uint8_t sensorType,
                         const chreSensorThreeAxisData &bias) {
  uint16_t eventType;
  if (SensorTypeHelpers::getBiasEventType(sensorType, &eventType)) {
    auto *event = memoryAlloc<struct chreSensorThreeAxisData>();
    if (event == nullptr) {
      LOG_OOM();
    } else {
      *event = bias;
      getSensorRequestManager().getSensorHandle(sensorType,
                                                &event->header.sensorHandle);
      EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
          eventType, event, freeEventDataCallback);
    }
  }
}

/**
 * Compares the given status updates and returns true if they are the same.
 *
 * A simple memcmp cannot be done because if a given field is not valid, then
 * the field may be different across updates, but doesn't indicate the update
 * is different.
 */
bool isSameStatusUpdate(
    const SeeHelperCallbackInterface::SamplingStatusData &status1,
    const SeeHelperCallbackInterface::SamplingStatusData &status2) {
  bool sameStatus = status1.enabledValid == status2.enabledValid;
  if (sameStatus && status1.enabledValid) {
    sameStatus &= status1.status.enabled == status2.status.enabled;
  }
  // Only check interval / latency fields if both status updates say the sensor
  // is enabled since CHRE doesn't care what the fields are set to if the sensor
  // is disabled.
  if (sameStatus && status1.status.enabled) {
    sameStatus &= status1.intervalValid == status2.intervalValid;
    if (sameStatus && status1.intervalValid) {
      sameStatus &= status1.status.interval == status2.status.interval;
    }
    sameStatus &= status1.latencyValid == status2.latencyValid;
    if (sameStatus && status1.latencyValid) {
      sameStatus &= status1.status.latency == status2.status.latency;
    }
  }
  return sameStatus;
}

/**
 * Constructs and initializes a sensor, and adds it to the sensor list.
 *
 * @param seeHelper SeeHelper instance to register sensor with
 * @param sensorType The sensor type of the sensor.
 * @param suid The SUID of the sensor as provided by SEE.
 * @param attr A reference to SeeAttrbutes.
 * @param sensors The sensor list.
 */
void addSensor(SeeHelper &seeHelper, uint8_t sensorType,
               const sns_std_suid &suid, const SeeAttributes &attr,
               DynamicVector<Sensor> *sensors) {
  // Concatenate vendor and name with a space in between.
  char sensorName[kSensorNameMaxLen];
  strlcpy(sensorName, attr.vendor, sizeof(sensorName));
  strlcat(sensorName, " ", sizeof(sensorName));
  strlcat(sensorName, attr.name, sizeof(sensorName));

  // Some sensors have a max sample rate of 0 which makes ceilf return infinity
  // for on-change or continuous sensors when that's not the correct
  // minInterval.
  float maxSampleRate = (attr.maxSampleRate == 0.0f) ? 10 : attr.maxSampleRate;

  // Override one-shot sensor's minInterval to default
  uint64_t minInterval =
      SensorTypeHelpers::isOneShot(sensorType)
          ? CHRE_SENSOR_INTERVAL_DEFAULT
          : static_cast<uint64_t>(
                ceilf(Seconds(1).toRawNanoseconds() / maxSampleRate));

  if (!sensors->emplace_back()) {
    FATAL_ERROR("Failed to allocate new sensor: out of memory");
  }

  // The sensor base class must be initialized before the main Sensor init()
  // can be invoked as init() is allowed to invoke base class methods.
  sensors->back().initBase(sensorType, minInterval, sensorName,
                           attr.passiveRequest);
  sensors->back().init();

  // Resample big image sensors to reduce system load during sw flush.
#ifdef CHRE_SLPI_UIMG_ENABLED
  bool resample = isBigImageSensorType(sensorType);
#else
  bool resample = false;
#endif
  bool prevRegistered;
  bool registered =
      seeHelper.registerSensor(sensorType, suid, resample, &prevRegistered);
  if (!registered && prevRegistered) {
    LOGW("SUID has been previously registered");
  } else if (!registered) {
    FATAL_ERROR("Failed to register SUID/SensorType mapping.");
  }
}

/**
 * Compare SEE reported stream type attribute to the expected one. Some SEE
 * sensors may support more than one stream type.
 */
bool isStreamTypeCorrect(uint8_t sensorType, uint8_t streamType) {
  bool success = true;
  if ((SensorTypeHelpers::isContinuous(sensorType) &&
       streamType != SNS_STD_SENSOR_STREAM_TYPE_STREAMING) ||
      (SensorTypeHelpers::isOnChange(sensorType) &&
       streamType != SNS_STD_SENSOR_STREAM_TYPE_ON_CHANGE)
// The default SLPI build exposes instant motion / stationary sensors as
// on-change, but CHRE uses them as one-shot
#ifndef CHRE_SLPI_DEFAULT_BUILD
      || (SensorTypeHelpers::isOneShot(sensorType) &&
          streamType != SNS_STD_SENSOR_STREAM_TYPE_SINGLE_OUTPUT)
#endif
  ) {
    success = false;
    LOGW("Inconsistent sensor type %" PRIu8 " and stream type %" PRIu8,
         static_cast<uint8_t>(sensorType), streamType);
  }
  return success;
}

/**
 * Obtains the list of SUIDs and their attributes that support the specified
 * data type.
 */
bool getSuidAndAttrs(SeeHelper &seeHelper, const char *dataType,
                     DynamicVector<SuidAttr> *suidAttrs, uint8_t minNumSuids) {
  DynamicVector<sns_std_suid> suids;
  bool success = seeHelper.findSuidSync(dataType, &suids, minNumSuids);
  if (!success) {
    LOGE("Failed to find sensor '%s'", dataType);
  } else {
    LOGD("Num of SUIDs found for '%s': %zu", dataType, suids.size());

    for (const auto &suid : suids) {
      SeeAttributes attr;
      if (!seeHelper.getAttributesSync(suid, &attr)) {
        success = false;
        LOGE("Failed to get attributes of SUID 0x%" PRIx64 " %" PRIx64,
             suid.suid_high, suid.suid_low);
      } else {
        LOGI("%s %s, hw id %" PRId64 ", max ODR %f Hz, stream type %" PRIu8
             " passive %d",
             attr.vendor, attr.name, attr.hwId, attr.maxSampleRate,
             attr.streamType, attr.passiveRequest);
        SuidAttr sensor = {
            .suid = suid,
            .attr = attr,
        };
        if (!suidAttrs->push_back(sensor)) {
          success = false;
          LOG_OOM();
        }
      }
    }
  }
  return success;
}

#ifndef CHRE_SLPI_DEFAULT_BUILD
//! Check whether two sensors with the specified attrtibutes belong to the same
//! sensor hardware module.
bool sensorHwMatch(const SeeAttributes &attr0, const SeeAttributes &attr1) {
  // When HW ID is absent, its default to 0 and won't be a factor.
  return ((strncmp(attr0.vendor, attr1.vendor, kSeeAttrStrValLen) == 0) &&
          (strncmp(attr0.name, attr1.name, kSeeAttrStrValLen) == 0) &&
          (attr0.hwId == attr1.hwId));
}
#endif

/**
 * Looks up SUID(s) associated with a given sensor data type string and sensor
 * type enum, registers them with SeeHelper, and adds a Sensor instance to the
 * supplied vector for use in CHRE. When given an uncalibrated sensor type, will
 * also look for and add the calibrated sensor type.
 *
 * @param seeHelper SeeHelper instance to use for lookup/registration
 * @param temperatureSensors List of previously discovered temperature sensor
 *        info to use for adding temp sensors associated with this sensor type
 * @param dataType SEE data type string
 * @param sensorType CHRE sensor type associated with dataType
 * @param skipAdditionalTypes if true, don't attempt to add
 *        calibrated/temperature sensor types associated with this sensorType
 * @param sensors Vector to append found sensor(s) to
 */
void findAndAddSensorsForType(SeeHelper &seeHelper,
                              const DynamicVector<SuidAttr> &temperatureSensors,
                              const char *dataType, uint8_t sensorType,
                              bool skipAdditionalTypes,
                              DynamicVector<Sensor> *sensors) {
  DynamicVector<SuidAttr> primarySensors;
  if (!getSuidAndAttrs(seeHelper, dataType, &primarySensors,
                       1 /* minNumSuids */)) {
    handleMissingSensor();
  }

  for (const auto &primarySensor : primarySensors) {
    sns_std_suid suid = primarySensor.suid;
    SeeAttributes attr = primarySensor.attr;

    // Some sensors support both continuous and on-change streams.
    // If there are more than one SUIDs that support the data type,
    // choose the first one that has the expected stream type.
    if (isStreamTypeCorrect(sensorType, attr.streamType)) {
      addSensor(seeHelper, sensorType, suid, attr, sensors);

      if (!skipAdditionalTypes) {
        // Check if this sensor has a runtime-calibrated version.
        uint8_t calibratedType;
        if (getSensorTypeFromDataType(dataType, true /* calibrated */,
                                      &calibratedType) &&
            calibratedType != sensorType) {
          addSensor(seeHelper, calibratedType, suid, attr, sensors);
        }

        // Check if this sensor has a secondary temperature sensor.
        uint8_t temperatureType =
            PlatformSensorTypeHelpers::getTempSensorType(sensorType);
        if (temperatureType != CHRE_SENSOR_TYPE_INVALID) {
          bool tempFound = false;
          for (const auto &tempSensor : temperatureSensors) {
            sns_std_suid tempSuid = tempSensor.suid;
            SeeAttributes tempAttr = tempSensor.attr;

#ifdef CHRE_SLPI_DEFAULT_BUILD
            // The default build exposes a single temp sensor to be used for
            // all temperature sensors that doesn't have the same attributes
            // as the primarySensor.
            if (true) {
#else
            if (sensorHwMatch(attr, tempAttr)) {
#endif
              LOGD("Found matching temperature sensor type");
              tempFound = true;
              addSensor(seeHelper, temperatureType, tempSuid, tempAttr,
                        sensors);
              break;
            }
          }
          if (!tempFound) {
            LOGW("Temperature sensor type %" PRIu8 " not found!",
                 static_cast<uint8_t>(temperatureType));
          }
        }
      }
      break;
    }
  }
}

}  // namespace

PlatformSensorManager::~PlatformSensorManager() {}

void PlatformSensorManager::init() {
  if (!mSeeHelper.init(this)) {
    FATAL_ERROR("Failed to initialize SEE helper");
  }

#ifdef CHRE_SLPI_UIMG_ENABLED
  if (!mBigImageSeeHelper.init(this, kDefaultSeeWaitTimeout,
                               true /* skipDefaultSensorInit */)) {
    FATAL_ERROR("Failed to init bimg SEE helper");
  }
#endif  // CHRE_SLPI_UIMG_ENABLED
}

SeeHelper &PlatformSensorManagerBase::getSeeHelperForSensorType(
    uint8_t sensorType) {
#ifdef CHRE_SLPI_UIMG_ENABLED
  if (isBigImageSensorType(sensorType)) {
    slpiForceBigImage();
    return mBigImageSeeHelper;
  } else
#endif
  {
    return mSeeHelper;
  }
}

#ifdef CHRE_SLPI_UIMG_ENABLED
void PlatformSensorManagerBase::getBigImageSensors(
    DynamicVector<Sensor> *sensors) {
  CHRE_ASSERT(sensors);

  // Currently, just adding calibrated accel, uncal accel/gyro/mag and als as
  // they are the ones we know that big image nanoapps will need at a different
  // batching rate compared to uimg.
  const char *kBigImageDataTypes[] = {
      "accel",
      "gyro",
      "mag",
      "ambient_light",
  };

  DynamicVector<SuidAttr> nullTemperatureSensorList;

  for (size_t i = 0; i < ARRAY_SIZE(kBigImageDataTypes); i++) {
    const char *dataType = kBigImageDataTypes[i];
    // Loop through potential cal/uncal sensors.
    for (size_t j = 0; j < 2; j++) {
      uint8_t sensorType;
      if (getBigImageSensorTypeFromDataType(dataType, (j == 0) /* calibrated */,
                                            &sensorType)) {
        findAndAddSensorsForType(mBigImageSeeHelper, nullTemperatureSensorList,
                                 dataType, sensorType,
                                 true /* skipAdditionalTypes */, sensors);
      }
    }
  }
}
#endif  // CHRE_SLPI_UIMG_ENABLED

DynamicVector<Sensor> PlatformSensorManager::getSensors() {
  DynamicVector<Sensor> sensors;
  DynamicVector<SuidAttr> tempSensors;
  if (!getSuidAndAttrs(mSeeHelper, "sensor_temperature", &tempSensors,
                       CHRE_SEE_NUM_TEMP_SENSORS)) {
    handleMissingSensor();
  }

#ifndef CHREX_SENSOR_SUPPORT
  const char *kVendorDataTypes[] = {};
#endif  // CHREX_SENSOR_SUPPORT
  constexpr size_t kNumSeeTypes = ARRAY_SIZE(kSeeDataTypes);
  constexpr size_t kNumVendorTypes = ARRAY_SIZE(kVendorDataTypes);
  for (size_t i = 0; i < kNumSeeTypes + kNumVendorTypes; i++) {
    const char *dataType = (i < kNumSeeTypes)
                               ? kSeeDataTypes[i]
                               : kVendorDataTypes[i - kNumSeeTypes];

    uint8_t sensorType;
    if (!getSensorTypeFromDataType(dataType, false /* calibrated */,
                                   &sensorType)) {
      LOGE("Unknown sensor type found for '%s'", dataType);
      continue;
    }

    bool skipAdditionalTypes = false;

#ifdef CHRE_SLPI_DEFAULT_BUILD
    // Stationary and motion detect share the same dataType on the default build
    if (sensorType == CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT &&
        i == kNumSeeTypes - 1) {
      sensorType = CHRE_SENSOR_TYPE_STATIONARY_DETECT;
      // Skip additional types or InstantMotion will be added to the sensor list
      // twice.
      skipAdditionalTypes = true;
    }
#endif

    findAndAddSensorsForType(mSeeHelper, tempSensors, dataType, sensorType,
                             skipAdditionalTypes, &sensors);
  }

#ifdef CHRE_SLPI_UIMG_ENABLED
  getBigImageSensors(&sensors);
#endif

  return sensors;
}

bool PlatformSensorManager::configureSensor(Sensor &sensor,
                                            const SensorRequest &request) {
  SeeSensorRequest req = {
      .sensorType = sensor.getSensorType(),
      .enable = (request.getMode() != SensorMode::Off),
      .passive = sensorModeIsPassive(request.getMode()),
      .samplingRateHz = static_cast<float>(
          kOneSecondInNanoseconds / request.getInterval().toRawNanoseconds()),
      // Override batch period to 0 for micro-image non-continuous sensors to
      // ensure one sample per batch so that nanoapps do not miss state changes.
      .batchPeriodUs =
#ifdef CHRE_SLPI_UIMG_ENABLED
          (!sensor.isContinuous() &&
           !isBigImageSensorType(sensor.getSensorType()))
              ? 0
              :
#endif
              static_cast<uint32_t>(request.getLatency().toRawNanoseconds() /
                                    kOneMicrosecondInNanoseconds),
  };

  SeeHelper &seeHelper = getSeeHelperForSensorType(sensor.getSensorType());
  bool wasInUImage = slpiInUImage();

  bool success = true;

  // TODO(b/150144912): Merge the two implementations to avoid having separate
  // code paths.
#ifdef CHRE_SLPI_DEFAULT_BUILD
  // Calibration updates are not enabled automatically in the default build.
  SeeCalHelper *calHelper = seeHelper.getCalHelper();

  const sns_std_suid *suid =
      calHelper->getCalSuidFromSensorType(sensor.getSensorType());
  bool wereCalUpdatesEnabled = false;
  if (suid != nullptr) {
    wereCalUpdatesEnabled = calHelper->areCalUpdatesEnabled(*suid);
    success = calHelper->configureCalUpdates(*suid, req.enable, seeHelper);
  }
#endif

  if (success) {
    success = seeHelper.makeRequest(req);
  }

#ifdef CHRE_SLPI_DEFAULT_BUILD
  // If any part of the configuration process failed, reset our subscription
  // for calibration updates to its previous value to attempt to restore state.
  if (suid != nullptr && !success) {
    bool areCalUpdatesEnabled = calHelper->areCalUpdatesEnabled(*suid);
    if (areCalUpdatesEnabled != wereCalUpdatesEnabled) {
      calHelper->configureCalUpdates(*suid, wereCalUpdatesEnabled, seeHelper);
    }
  }
#endif

  // If we dropped into micro-image during that blocking call to SEE, go back
  // to big image. This won't happen if the calling nanoapp is a big image one,
  // but other code paths currently assume that we will only transition from big
  // image to micro-image from CHRE's perspective while it's waiting for an
  // event to arrive in its empty queue.
  // TODO: transition back to big image only when needed, at the point of
  // invoking a nanoapp's free event/message callback
  if (!wasInUImage && slpiInUImage()) {
    LOGD("Restoring big image operating mode");
    slpiForceBigImage();
  }

  if (success) {
    // TODO: remove setSamplingStatus when .latency is available in status
    // update from SEE.
    struct chreSensorSamplingStatus status;
    if (sensor.getSamplingStatus(&status)) {
      // If passive request is not supported by this SEE sensor, it won't be
      // dynamically enabled/disabled and its status stays the same as set here.
      if (!sensor.supportsPassiveMode()) {
        status.enabled = req.enable;
      }
      status.latency = req.batchPeriodUs * kOneMicrosecondInNanoseconds;
      sensor.setSamplingStatus(status);
    }
  }
  return success;
}

bool PlatformSensorManager::configureBiasEvents(const Sensor & /* sensor */,
                                                bool /* enable */,
                                                uint64_t /* latencyNs */) {
  // TODO: Allow enabling / disabling bias events rather than enabling all
  // bias sensors at init.
  return true;
}

bool PlatformSensorManager::getThreeAxisBias(
    const Sensor &sensor, struct chreSensorThreeAxisData *bias) const {
  uint8_t sensorType = sensor.getSensorType();
  SeeCalHelper *calHelper =
      getSeeHelperForSensorType(sensorType).getCalHelper();

  bool success = sensor.reportsBiasEvents();
  if (success) {
    // We use the runtime-calibrated sensor type here, per documentation
    // of SeeCalHelper::getBias(), but overwrite the sensorHandle to that of
    // the current sensor, because the calibration data itself is equivalent
    // for both calibrated/uncalibrated sensor types.
#ifdef CHRE_SLPI_UIMG_ENABLED
    // Use the uimg runtime-calibrated sensor type to get the calibration
    // bias, since SeeCalHelper is unaware of the bimg/uimg differentiation.
    uint8_t calSensorType = PlatformSensorTypeHelpers::toCalibratedSensorType(
        getUimgSensorType(sensorType));
#else
    uint8_t calSensorType =
        PlatformSensorTypeHelpers::toCalibratedSensorType(sensorType);
#endif
    if (!calHelper->getBias(calSensorType, bias)) {
      // Set to zero bias + unknown accuracy per CHRE API requirements.
      memset(bias, 0, sizeof(chreSensorThreeAxisData));
      bias->header.readingCount = 1;
      bias->header.accuracy = CHRE_SENSOR_ACCURACY_UNKNOWN;
    }

    // Overwrite sensorHandle to match the request type.
    getSensorRequestManager().getSensorHandle(sensorType,
                                              &bias->header.sensorHandle);
  }

  return success;
}

bool PlatformSensorManager::flush(const Sensor &sensor,
                                  uint32_t *flushRequestId) {
  uint8_t sensorType = sensor.getSensorType();
  return getSeeHelperForSensorType(sensorType).flush(sensorType);
}

void PlatformSensorManager::releaseSamplingStatusUpdate(
    struct chreSensorSamplingStatus *status) {
  memoryFree(status);
}

void PlatformSensorManager::releaseSensorDataEvent(void *data) {
  memoryFree(data);
}

void PlatformSensorManager::releaseBiasEvent(void *biasData) {
  memoryFree(biasData);
}

void PlatformSensorManagerBase::onSamplingStatusUpdate(
    UniquePtr<SeeHelperCallbackInterface::SamplingStatusData> &&status) {
  uint32_t sensorHandle;
  getSensorRequestManager().getSensorHandle(status->sensorType, &sensorHandle);
  Sensor *sensor = getSensorRequestManager().getSensor(sensorHandle);

  // TODO: Once the latency field is actually filled in by SEE, modify this
  // logic to avoid reacting if the latency and interval of the sensor are
  // updated separately, but contain the same info as before.
  if (sensor != nullptr) {
    // Ignore the enabled flag from status update if this is not a passive mode
    // supported sensor because this may cause the sampling status in CHRE to
    // go out of sync with reality
    if (!sensor->supportsPassiveMode()) {
      status->status.enabled =
          sensor->mLastReceivedSamplingStatus.status.enabled;
      status->enabledValid = sensor->mLastReceivedSamplingStatus.enabledValid;
    }
    if (!isSameStatusUpdate(sensor->mLastReceivedSamplingStatus,
                            *status.get())) {
      sensor->mLastReceivedSamplingStatus = *status.get();

      auto callback = [](uint16_t /* type */, void *data) {
        auto cbData = UniquePtr<SeeHelperCallbackInterface::SamplingStatusData>(
            static_cast<SeeHelperCallbackInterface::SamplingStatusData *>(
                data));

        uint32_t sensorHandle;
        getSensorRequestManager().getSensorHandle(cbData->sensorType,
                                                  &sensorHandle);

        // Memory will be freed after core framework performs its updates.
        struct chreSensorSamplingStatus *status =
            memoryAlloc<struct chreSensorSamplingStatus>();
        mergeUpdatedStatus(sensorHandle, *cbData.get(), status);

        getSensorRequestManager().handleSamplingStatusUpdate(sensorHandle,
                                                             status);
      };
      // Schedule a deferred callback to handle sensor status change in the main
      // thread.
      EventLoopManagerSingleton::get()->deferCallback(
          SystemCallbackType::SensorStatusUpdate, status.release(), callback);
    }
  }
}

void PlatformSensorManagerBase::onSensorDataEvent(
    uint8_t sensorType, UniquePtr<uint8_t> &&eventData) {
  uint32_t sensorHandle;
  getSensorRequestManager().getSensorHandle(sensorType, &sensorHandle);
  auto *header =
      reinterpret_cast<struct chreSensorDataHeader *>(eventData.get());
  header->sensorHandle = sensorHandle;

  getSensorRequestManager().handleSensorDataEvent(sensorHandle,
                                                  eventData.release());
}

void PlatformSensorManagerBase::onHostWakeSuspendEvent(bool awake) {
  // Host wake events are sent as soon as SEE is up so verify the event loop is
  // up before attempting to post the event to avoid a crash.
  if (EventLoopManagerSingleton::isInitialized()) {
    EventLoopManagerSingleton::get()
        ->getEventLoop()
        .getPowerControlManager()
        .onHostWakeSuspendEvent(awake);
  }
}

void PlatformSensorManagerBase::onSensorBiasEvent(
    uint8_t sensorType, UniquePtr<struct chreSensorThreeAxisData> &&biasData) {
  uint32_t sensorHandle;
  if (getSensorRequestManager().getSensorHandle(sensorType, &sensorHandle)) {
    biasData->header.sensorHandle = sensorHandle;
    Sensor *sensor = getSensorRequestManager().getSensor(sensorHandle);
    if (!sensor->reportsBiasEvents()) {
      LOGE("Received bias event for unsupported sensor type %" PRIu8,
           sensorType);
    } else {
      // Posts newly allocated event for the uncalibrated type
      postSensorBiasEvent(
          PlatformSensorTypeHelpers::toUncalibratedSensorType(sensorType),
          *biasData);

      getSensorRequestManager().handleBiasEvent(sensorHandle,
                                                biasData.release());
    }
  }
}

void PlatformSensorManagerBase::onFlushCompleteEvent(uint8_t sensorType) {
  // Flush complete events are sent after any batch delivery so verify the
  // event loop is up before attempting to post the event to avoid a crash.
  if (EventLoopManagerSingleton::isInitialized()) {
    // TODO: Have SEE pass flush request IDs through the flush complete event
    uint32_t sensorHandle;
    getSensorRequestManager().getSensorHandle(sensorType, &sensorHandle);
    getSensorRequestManager().handleFlushCompleteEvent(
        sensorHandle, UINT32_MAX, /* invalid flush request ID */
        CHRE_ERROR_NONE);
  }
}

}  // namespace chre
