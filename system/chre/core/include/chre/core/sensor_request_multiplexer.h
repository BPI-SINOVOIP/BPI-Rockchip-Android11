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

#ifndef CHRE_CORE_SENSOR_REQUEST_MULTIPLEXER_H_
#define CHRE_CORE_SENSOR_REQUEST_MULTIPLEXER_H_

#include "chre/core/request_multiplexer.h"
#include "chre/core/sensor_request.h"

namespace chre {

/**
 * Provides methods on top of the RequestMultiplexer class specific for working
 * with SensorRequest objects.
 */
class SensorRequestMultiplexer : public RequestMultiplexer<SensorRequest> {
 public:
  /**
   * Searches through the list of sensor requests for a request owned by the
   * given nanoapp. The provided non-null index pointer is populated with the
   * index of the request if it is found.
   *
   * @param instanceId The instance ID of the nanoapp whose request is being
   *        searched for.
   * @param index A non-null pointer to an index that is populated if a
   *        request for this nanoapp is found.
   * @return A pointer to a SensorRequest that is owned by the provided
   *         nanoapp if one is found otherwise nullptr.
   */
  const SensorRequest *findRequest(uint32_t instanceId, size_t *index) const;
};

}  // namespace chre

#endif  // CHRE_CORE_SENSOR_REQUEST_MULTIPLEXER_H_