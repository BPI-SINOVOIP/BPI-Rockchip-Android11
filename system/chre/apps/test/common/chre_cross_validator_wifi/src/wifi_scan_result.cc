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

#include "wifi_scan_result.h"

#include <chre.h>

#include "chre/util/nanoapp/log.h"

#include <stdio.h>
#include <cstring>

#define LOG_TAG "ChreCrossValidatorWifi"

WifiScanResult::WifiScanResult(pb_istream_t *apWifiScanResultStream) {
  memset(mSsid, 0, CHRE_WIFI_SSID_MAX_LEN);
  chre_cross_validation_wifi_WifiScanResult wifiScanResultProto =
      chre_cross_validation_wifi_WifiScanResult_init_default;
  wifiScanResultProto.ssid = {.funcs = {.decode = decodeString}, .arg = mSsid};
  wifiScanResultProto.bssid = {.funcs = {.decode = decodeString},
                               .arg = mBssid};
  if (!pb_decode(apWifiScanResultStream,
                 chre_cross_validation_wifi_WifiScanResult_fields,
                 &wifiScanResultProto)) {
    LOGE("AP wifi scan result proto message decode failed");
  }
  mTotalNumResults = wifiScanResultProto.totalNumResults;
  mResultIndex = wifiScanResultProto.resultIndex;
  LOGI("AP scan result mSsid = %s", mSsid);
}

WifiScanResult::WifiScanResult(const chreWifiScanResult &chreScanResult) {
  LOGI("CHRE scan result mSsid = %s", chreScanResult.ssid);
  memset(mSsid, 0, CHRE_WIFI_SSID_MAX_LEN);
  memcpy(mSsid, chreScanResult.ssid, chreScanResult.ssidLen);
  memcpy(mBssid, chreScanResult.bssid, CHRE_WIFI_BSSID_LEN);
}

bool WifiScanResult::areEqual(WifiScanResult result1, WifiScanResult result2) {
  // TODO(srok): Compare all fields that are shared between AP and CHRE scan
  // result.
  return strcmp(result1.mSsid, result2.mSsid) == 0 &&
         byteArraysAreEqual(result1.mBssid, result2.mBssid,
                            CHRE_WIFI_BSSID_LEN);
}

bool WifiScanResult::decodeString(pb_istream_t *stream,
                                  const pb_field_t * /*field*/, void **arg) {
  pb_byte_t *strPtr = reinterpret_cast<pb_byte_t *>(*arg);
  return pb_read(stream, strPtr, stream->bytes_left);
}

bool WifiScanResult::byteArraysAreEqual(uint8_t *arr1, uint8_t *arr2,
                                        uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    if (arr1[i] != arr2[i]) return false;
  }
  return true;
}
