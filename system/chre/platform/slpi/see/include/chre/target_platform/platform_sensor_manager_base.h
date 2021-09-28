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

#ifndef CHRE_PLATFORM_SLPI_SEE_PLATFORM_SENSOR__MANAGER_BASE_H_
#define CHRE_PLATFORM_SLPI_SEE_PLATFORM_SENSOR__MANAGER_BASE_H_

#include "chre/core/sensor_request.h"
#include "chre/platform/slpi/see/see_helper.h"

namespace chre {

/**
 * Contains additional methods needed by the SLPI SEE implementation of
 * PlatformSensorManager. Specifically, implements the
 * SeeHelperCallbackInterface to allow the manager to receive callbacks from
 * SEE.
 */
class PlatformSensorManagerBase : public SeeHelperCallbackInterface {
 public:
#ifdef CHRE_SLPI_UIMG_ENABLED
  PlatformSensorManagerBase() : mBigImageSeeHelper(mSeeHelper.getCalHelper()) {}
#endif  // CHRE_SLPI_UIMG_ENABLED

  /**
   * Helper function to retrieve the SeeHelper for a given sensor type.
   * @param sensorType the sensor type
   * @return A reference to the appropriate (bimg or uimg) SeeHelper
   */
  SeeHelper &getSeeHelperForSensorType(uint8_t sensorType);

  const SeeHelper &getSeeHelperForSensorType(uint8_t sensorType) const {
    // The following cast is done to share code between this method and
    // getSeeHelperForSensorType so we can expose either a const or non-const
    // SeeHelper depending on the requirements of the caller. The non-const
    // version of getSeeHelperForSensorType will not modify
    // PlatformSensorManagerBase so this should be safe.
    return const_cast<PlatformSensorManagerBase *>(this)
        ->getSeeHelperForSensorType(sensorType);
  }

#ifdef CHRE_SLPI_UIMG_ENABLED
  /**
   * Registers alternate sensor(s) to be used separately by big image nanoapps.
   */
  void getBigImageSensors(DynamicVector<Sensor> *sensors);
#endif  // CHRE_SLPI_UIMG_ENABLED

  void onSamplingStatusUpdate(
      UniquePtr<SeeHelperCallbackInterface::SamplingStatusData> &&status)
      override;

  void onSensorDataEvent(uint8_t sensorType,
                         UniquePtr<uint8_t> &&eventData) override;

  void onHostWakeSuspendEvent(bool awake) override;

  void onSensorBiasEvent(
      uint8_t sensorHandle,
      UniquePtr<struct chreSensorThreeAxisData> &&biasData) override;

  void onFlushCompleteEvent(uint8_t sensorType) override;

 protected:
  SeeHelper mSeeHelper;

#ifdef CHRE_SLPI_UIMG_ENABLED
  BigImageSeeHelper mBigImageSeeHelper;
#endif  // CHRE_SLPI_UIMG_ENABLED
};

}  // namespace chre

#endif  // CHRE_PLATFORM_SLPI_SEE_PLATFORM_SENSOR__MANAGER_BASE_H_
