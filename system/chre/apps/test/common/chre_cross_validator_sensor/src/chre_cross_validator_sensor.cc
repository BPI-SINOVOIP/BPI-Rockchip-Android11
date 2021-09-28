/*
 * Copyright (C) 2020 The Android Open Source Project
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

/**
 * The nanoapp that will request data from CHRE apis and send that data back to
 * the host so that it can be compared against host side data. The nanoapp will
 * request different chre apis (wifi, sensor, etc) depeding on the message type
 * given in start message.
 */

#include "chre_cross_validator_sensor_manager.h"

/* TODO(b/148481242): Send all errors to host as well as just logging them as
 * errors.
 *
 * TODO(b/146052784): Create a helper function to get string version of
 * sensorType for logging.
 */

namespace chre {

extern "C" void nanoappHandleEvent(uint32_t senderInstanceId,
                                   uint16_t eventType, const void *eventData) {
  cross_validator_sensor::ManagerSingleton::get()->handleEvent(
      senderInstanceId, eventType, eventData);
}

extern "C" bool nanoappStart(void) {
  cross_validator_sensor::ManagerSingleton::init();
  return true;
}

extern "C" void nanoappEnd(void) {
  cross_validator_sensor::ManagerSingleton::deinit();
}

}  // namespace chre
