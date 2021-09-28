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
#include <general_test/cell_info_nr.h>

#include <cinttypes>

namespace general_test {
namespace {
constexpr int32_t kInvalid = INT32_MAX;
}  // namespace

bool CellInfoNr::validateIdentity(const struct chreWwanCellIdentityNr &identity,
                                  bool registered) {
  bool valid = false;

  if (!isBoundedInt32(identity.mcc, 0, 999, kInvalid, !registered)) {
    sendFatalFailureInt32("Invalid NR Mobile Country Code: %d", identity.mcc);
  } else if (!isBoundedInt32(identity.mnc, 0, 999, kInvalid, !registered)) {
    sendFatalFailureInt32("Invalid NR Mobile Network Code: %d", identity.mnc);
  } else if (!isBoundedInt64(chreWwanUnpackNrNci(&identity), 0, 68719476735,
                             INT64_MAX)) {
    chreLog(CHRE_LOG_ERROR, "Invalid NR Cell Identity: %" PRId64,
            chreWwanUnpackNrNci(&identity));
    sendFatalFailure("Invalid NR Cell Identity");
  } else if (!isBoundedInt32(identity.pci, 0, 1007, kInvalid,
                             false /* invalidAllowed*/)) {
    sendFatalFailureInt32("Invalid NR Physical Cell Id: %d", identity.pci);
  } else if (!isBoundedInt32(identity.tac, 0, 16777215, kInvalid)) {
    sendFatalFailureInt32("Invalid NR Tracking Area Code: %d", identity.tac);
  } else if (!isBoundedInt32(identity.nrarfcn, 0, 3279165, kInvalid,
                             false /* invalidAllowed */)) {
    sendFatalFailureInt32("Invalid NR Absolute RF Channel Number: %d",
                          identity.nrarfcn);
  } else {
    valid = true;
  }

  return valid;
}

bool CellInfoNr::validateSignalStrength(
    const struct chreWwanSignalStrengthNr &strength) {
  bool valid = false;

  if (!isBoundedInt32(strength.ssRsrp, 44, 140, kInvalid)) {
    sendFatalFailureInt32("Invalid NR SS RSRP: %d", strength.ssRsrp);
  } else if (!isBoundedInt32(strength.ssRsrq, -86, 41, kInvalid)) {
    sendFatalFailureInt32("Invalid NR SS RSRQ: %d", strength.ssRsrq);
  } else if (!isBoundedInt32(strength.ssSinr, -46, 81, kInvalid)) {
    sendFatalFailureInt32("Invalid NR SS SINR: %d", strength.ssSinr);
  } else if (!isBoundedInt32(strength.csiRsrp, 44, 140, kInvalid)) {
    sendFatalFailureInt32("Invalid NR CSI RSRP: %d", strength.csiRsrp);
  } else if (!isBoundedInt32(strength.csiRsrq, -86, 41, kInvalid)) {
    sendFatalFailureInt32("Invalid NR CSI RSRQ: %d", strength.csiRsrq);
  } else if (!isBoundedInt32(strength.csiSinr, -46, 81, kInvalid)) {
    sendFatalFailureInt32("Invalid NR CSI SINR: %d", strength.csiSinr);
  } else {
    valid = true;
  }

  return valid;
}

bool CellInfoNr::validate(const struct chreWwanCellInfoNr &cell,
                          bool registered) {
  return (validateIdentity(cell.cellIdentityNr, registered) &&
          validateSignalStrength(cell.signalStrengthNr));
}

}  // namespace general_test
