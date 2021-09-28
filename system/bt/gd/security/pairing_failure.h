/******************************************************************************
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include <string>

#include "security/smp_packets.h"

namespace bluetooth {
namespace security {

/* This structure holds the information about the failure in case of airing failure */
struct PairingFailure {
  /* A place in code that triggered this failure. It can be modified by functions that pass the error to a location that
   * better reflect the current state of flow. i.e. instead of generic location responsible for waiting for packet,
   * replace it with location of receiving specific packet in a specific flow */
  // base::Location location;

  /* This is the failure message, that will be passed, either into upper layers,
   * or to the metrics in future */
  std::string message;

  /* If failure is due to mismatch of received code, this contains the received opcode */
  Code received_code_;

  /* if the failure is due to "SMP failure", this field contains the reson code
   */
  PairingFailedReason reason;

  PairingFailure(/*const base::Location& location, */ const std::string& message)
      : /*location(location), */ message(message) {}

  PairingFailure(/*const base::Location& location, */ const std::string& message, Code received_code)
      : /*location(location), */ message(message), received_code_(received_code) {}

  PairingFailure(/*const base::Location& location, */ const std::string& message, PairingFailedReason reason)
      : /*location(location),*/ message(message), reason(reason) {}
};

}  // namespace security
}  // namespace bluetooth