/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <chre.h>
#include <cinttypes>
#include <cmath>

#include "chre/util/macros.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/nanoapp/wifi.h"
#include "chre/util/time.h"

using chre::kOneMillisecondInNanoseconds;
using chre::Nanoseconds;
using chre::Seconds;

#define LOG_TAG "[WifiWorld]"

//#define WIFI_WORLD_VERBOSE_WIFI_RESULT_LOGS

#ifdef CHRE_NANOAPP_INTERNAL
namespace chre {
namespace {
#endif  // CHRE_NANOAPP_INTERNAL

namespace {

//! A dummy cookie to pass into the configure scan monitoring async request.
constexpr uint32_t kScanMonitoringCookie = 0x1337;

//! A dummy cookie to pass into on-demand scan async request.
constexpr uint32_t kOnDemandScanCookie = 0xcafe;

//! A dummy cookie to pass into ranging async request.
constexpr uint32_t kRangingCookie = 0xbeef;

//! The interval for on-demand wifi scans.
constexpr Nanoseconds kWifiScanInterval = Nanoseconds(Seconds(10));

//! A handle for the cyclic timer to request periodic on-demand wifi-scans.
uint32_t gWifiScanTimerHandle;

//! A global instance of wifi capabilities to use when reqeuesting wifi
//! functionality. This is populated at startup.
uint32_t gWifiCapabilities;

//! The last time in nanoseconds a wifi scan request was sucessfully made.
uint64_t gLastRequestTimeNs = 0;

//! True if CHRE_WIFI_REQUEST_TYPE_REQUEST_SCAN mode is requested.
bool gPendingOnDemandScan = false;

//! Accumulating count of the scan request results so far.
uint32_t gScanResultAcc = 0;

//! The currently requested on-demand wifi scan parameters.
chreWifiScanParams gWifiScanParams = {};

//! The sequence of on-demand wifi scan types to request for.
constexpr chreWifiScanType gWifiScanTypes[] = {
    CHRE_WIFI_SCAN_TYPE_ACTIVE, CHRE_WIFI_SCAN_TYPE_ACTIVE_PLUS_PASSIVE_DFS,
    CHRE_WIFI_SCAN_TYPE_PASSIVE};

//! The index of the next wifi scan type to request for.
uint8_t gScanTypeIndex = 0;

//! Whether to enable WiFi RTT ranging requests.
bool gEnableRanging = true;

//! The number of targets to make ranging request for.
uint8_t gTargetCount = 0;

//! The list of ranging targets.
chreWifiRangingTarget gTargetList[CHRE_WIFI_RANGING_LIST_MAX_LEN];

//! TIme last ranging request was made.
uint64_t gLastRangingTimeNs = 0;

//! Whether the app is awaiting any ranging event.
bool gPendingRanging = false;

/**
 * Logs a CHRE wifi scan result.
 *
 * @param result the scan result to log.
 */
void logChreWifiResult(const chreWifiScanResult &result) {
  const char *ssidStr = "<non-printable>";
  char ssidBuffer[chre::kMaxSsidStrLen];
  if (result.ssidLen == 0) {
    ssidStr = "<empty>";
  } else if (chre::parseSsidToStr(ssidBuffer, sizeof(ssidBuffer), result.ssid,
                                  result.ssidLen)) {
    ssidStr = ssidBuffer;
  }

  LOGI("Found network with SSID: %s", ssidStr);
#ifdef WIFI_WORLD_VERBOSE_WIFI_RESULT_LOGS
  const char *bssidStr = "<non-printable>";
  char bssidBuffer[chre::kBssidStrLen];
  if (chre::parseBssidToStr(result.bssid, bssidBuffer, sizeof(bssidBuffer))) {
    bssidStr = bssidBuffer;
  }

  LOGI("  age (ms): %" PRIu32, result.ageMs);
  LOGI("  capability info: %" PRIx16, result.capabilityInfo);
  LOGI("  bssid: %s", bssidStr);
  LOGI("  flags: %" PRIx8, result.flags);
  LOGI("  rssi: %" PRId8 "dBm", result.rssi);
  LOGI("  band: %s (%" PRIu8 ")", chre::parseChreWifiBand(result.band),
       result.band);
  LOGI("  primary channel: %" PRIu32, result.primaryChannel);
  LOGI("  center frequency primary: %" PRIu32, result.centerFreqPrimary);
  LOGI("  center frequency secondary: %" PRIu32, result.centerFreqSecondary);
  LOGI("  channel width: %" PRIu8, result.channelWidth);
  LOGI("  security mode: %" PRIx8, result.securityMode);
#endif  // WIFI_WORLD_VERBOSE_WIFI_RESULT_LOGS
}

/**
 * Logs a CHRE WiFi ranging result.
 *
 * @param result the ranging result to log.
 */
void logChreRangingResult(const chreWifiRangingResult &result) {
  const char *bssidStr = "<non-printable>";
  char bssidBuffer[chre::kBssidStrLen];
  if (chre::parseBssidToStr(result.macAddress, bssidBuffer,
                            sizeof(bssidBuffer))) {
    bssidStr = bssidBuffer;
  }
  LOGI("BSSID %s", bssidStr);
  LOGI("  age: %" PRIu64 " ms",
       (chreGetTime() - result.timestamp) / kOneMillisecondInNanoseconds);

  if (result.status != CHRE_WIFI_RANGING_STATUS_SUCCESS) {
    LOGE("  ranging failed");
  } else {
    LOGI("  rssi: %" PRId8 " dBm", result.rssi);
    LOGI("  distance: %" PRIu32 " mm", result.distance);
    LOGI("  distanceStdDev: %" PRIu32 " mm", result.distanceStdDev);

    if (result.flags & CHRE_WIFI_RTT_RESULT_HAS_LCI) {
      const chreWifiRangingResult::chreWifiLci lci = result.lci;
      LOGI("  latitude: 0x%" PRIx64 ", %f degs", lci.latitude,
           static_cast<float>(lci.latitude) / static_cast<float>(1 << 25));
      LOGI("  longitude: 0x%" PRIx64 ", %f degs", lci.longitude,
           static_cast<float>(lci.longitude) / static_cast<float>(1 << 25));

      float altitude =
          static_cast<float>(lci.altitude) / static_cast<float>(1 << 8);
      if (lci.altitudeType == CHRE_WIFI_LCI_UNCERTAINTY_UNKNOWN) {
        LOGI("  altitude: unknown");
      } else if (lci.altitudeType == CHRE_WIFI_LCI_ALTITUDE_TYPE_METERS) {
        LOGI("  altitude: 0x%" PRIx32 ", %f m", lci.altitude, altitude);
      } else if (lci.altitudeType == CHRE_WIFI_LCI_ALTITUDE_TYPE_FLOORS) {
        LOGI("  altitude: 0x%" PRIx32 ", %f floors", lci.altitude, altitude);
      } else {
        LOGE("  altitude: undefined");
      }

      if (lci.latitudeUncertainty == CHRE_WIFI_LCI_UNCERTAINTY_UNKNOWN) {
        LOGI("  latitude uncertainty: unknown");
      } else {
        LOGI("  latitude uncertainty: %f degs",
             powf(2, 8 - lci.latitudeUncertainty));
      }
      if (lci.longitudeUncertainty == CHRE_WIFI_LCI_UNCERTAINTY_UNKNOWN) {
        LOGI("  longitude uncertainty: unknown");
      } else {
        LOGI("  longitude uncertainty: %f degs",
             powf(2, 8 - lci.longitudeUncertainty));
      }
      if (lci.altitudeUncertainty == CHRE_WIFI_LCI_UNCERTAINTY_UNKNOWN ||
          lci.altitudeType != CHRE_WIFI_LCI_ALTITUDE_TYPE_METERS) {
        LOGI("  altitude uncertainty: unknown");
      } else {
        LOGI("  altitude uncertainty: %f m",
             powf(2, 21 - lci.altitudeUncertainty));
      }
    }
  }
}

/**
 * Requests a delayed WiFi scan using a one-shot timer. The interval is
 * specified as a constant at the top of this file.
 */
void requestDelayedWifiScan() {
  if (gWifiCapabilities & CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN) {
    // Schedule a timer to send an active WiFi scan.
    gWifiScanTimerHandle =
        chreTimerSet(kWifiScanInterval.toRawNanoseconds(),
                     &gWifiScanTimerHandle /* data */, true /* oneShot */);
    if (gWifiScanTimerHandle == CHRE_TIMER_INVALID) {
      LOGE("Failed to set timer for delayed WiFi scan");
    } else {
      LOGI("Set a timer to request a WiFi scan");
    }
  }
}

/**
 * Handles the result of an asynchronous request for a WiFi resource.
 *
 * @param result a pointer to the event structure containing the result of the
 * request.
 */
void handleWifiAsyncResult(const chreAsyncResult *result) {
  if (result->requestType == CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR) {
    if (result->success) {
      LOGI("Successfully requested WiFi scan monitoring");
    } else {
      LOGE("Error requesting WiFi scan monitoring with %" PRIu8,
           result->errorCode);
    }

    if (result->cookie != &kScanMonitoringCookie) {
      LOGE("Scan monitoring request cookie mismatch");
    }
  } else if (result->requestType == CHRE_WIFI_REQUEST_TYPE_REQUEST_SCAN) {
    uint64_t timeSinceRequest = chreGetTime() - gLastRequestTimeNs;
    if (result->success) {
      LOGI(
          "Successfully requested an on-demand WiFi scan (response time "
          "%" PRIu64 " ms)",
          timeSinceRequest / kOneMillisecondInNanoseconds);
      gPendingOnDemandScan = true;
    } else {
      LOGE("Error requesting an on-demand WiFi scan with %" PRIu8,
           result->errorCode);
    }

    if (result->cookie != &kOnDemandScanCookie) {
      LOGE("On-demand scan cookie mismatch");
    }

    requestDelayedWifiScan();
  } else if (result->requestType == CHRE_WIFI_REQUEST_TYPE_RANGING) {
    uint64_t timeSinceRequest = chreGetTime() - gLastRangingTimeNs;
    if (result->success) {
      LOGI("Successfully requested WiFi ranging (response time %" PRIu64 " ms)",
           timeSinceRequest / kOneMillisecondInNanoseconds);
    } else {
      gPendingRanging = false;
      LOGE("Error requesting a WiFi ranging with %" PRIu8, result->errorCode);
    }

    if (result->cookie != &kRangingCookie) {
      LOGE("Ranging cookie mismatch");
    }

  } else {
    LOGE("Received invalid async result");
  }
}

void prepareRanging(const chreWifiScanEvent *event) {
  if (gWifiCapabilities & CHRE_WIFI_CAPABILITIES_RTT_RANGING) {
    // Collect the first CHRE_WIFI_RANGING_LIST_MAX_LEN AP's that support the
    // capability.
    for (uint8_t i = 0; i < event->resultCount; i++) {
      if (gTargetCount < CHRE_WIFI_RANGING_LIST_MAX_LEN &&
          (event->results[i].flags &
           CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER)) {
        chreWifiRangingTargetFromScanResult(&event->results[i],
                                            &gTargetList[gTargetCount++]);
      }
    }

    // Make ranging request only when all scan events are received.
    if (!gPendingOnDemandScan) {
      if (gTargetCount == 0 && event->resultCount == 0) {
        LOGI("No AP to make ranging request to");
      } else if (gTargetCount == 0) {
        LOGI("No AP with RTT capability found");
        // Adding one AP to exercise ranging API.
        chreWifiRangingTargetFromScanResult(&event->results[0],
                                            &gTargetList[gTargetCount++]);
      }

      if (gTargetCount > 0) {
        struct chreWifiRangingParams params = {
            .targetListLen = gTargetCount,
            .targetList = &gTargetList[0],
        };

        gLastRangingTimeNs = chreGetTime();
        if (!chreWifiRequestRangingAsync(&params, &kRangingCookie)) {
          LOGE("Failed to request WiFi ranging");
        } else {
          gPendingRanging = true;
        }
        gTargetCount = 0;
      }
    }
  }
}

/**
 * Handles a WiFi scan event.
 *
 * @param event a pointer to the details of the WiFi scan event.
 */
void handleWifiScanEvent(const chreWifiScanEvent *event) {
  LOGI("Received Wifi scan event of type %" PRIu8 " with %" PRIu8
       " results at %" PRIu64 "ns",
       event->scanType, event->resultCount, event->referenceTime);

  if (gPendingOnDemandScan) {
    uint64_t timeSinceRequest = chreGetTime() - gLastRequestTimeNs;
    LOGI("Time since scan request = %" PRIu64 " ms",
         timeSinceRequest / kOneMillisecondInNanoseconds);

    if (event->scanType != gWifiScanParams.scanType) {
      LOGE("Invalid scan event type (expected %" PRIu8 ", received %" PRIu8 ")",
           gWifiScanParams.scanType, event->scanType);
    }

    gScanResultAcc += event->resultCount;
    if (gScanResultAcc >= event->resultTotal) {
      gPendingOnDemandScan = false;
      gScanResultAcc = 0;
    }

    if (gEnableRanging) {
      prepareRanging(event);
    }
  }

  for (uint8_t i = 0; i < event->resultCount; i++) {
    const chreWifiScanResult &result = event->results[i];
    logChreWifiResult(result);
  }
}

void handleWifiRangingEvent(const chreWifiRangingEvent *event) {
  LOGI("Received Wifi ranging event with %" PRIu8 " results",
       event->resultCount);

  if (!gPendingRanging) {
    LOGE("WiFi ranging event not expected");
  } else {
    gPendingRanging = false;
    for (uint8_t i = 0; i < event->resultCount; i++) {
      logChreRangingResult(event->results[i]);
    }
  }
}

/**
 * Handles a timer event.
 *
 * @param eventData The cookie passed to the timer request.
 */
void handleTimerEvent(const void *eventData) {
  const uint32_t *timerHandle = static_cast<const uint32_t *>(eventData);
  if (*timerHandle == gWifiScanTimerHandle) {
    gWifiScanParams.scanType = gWifiScanTypes[gScanTypeIndex];
    gWifiScanParams.maxScanAgeMs = 5000;  // 5 seconds
    gWifiScanParams.frequencyListLen = 0;
    gWifiScanParams.ssidListLen = 0;
    gScanTypeIndex = (gScanTypeIndex + 1) % ARRAY_SIZE(gWifiScanTypes);

    if (chreWifiRequestScanAsync(&gWifiScanParams, &kOnDemandScanCookie)) {
      LOGI("Requested a WiFi scan successfully");
      gLastRequestTimeNs = chreGetTime();
    } else {
      LOGE("Failed to request a WiFi scan");
    }
  } else {
    LOGE("Received invalid timer handle");
  }
}

}  // namespace

bool nanoappStart() {
  LOGI("App started as instance %" PRIu32, chreGetInstanceId());

  gWifiCapabilities = chreWifiGetCapabilities();
  LOGI("Detected WiFi support as: 0x%" PRIx32, gWifiCapabilities);

  if (gWifiCapabilities & CHRE_WIFI_CAPABILITIES_SCAN_MONITORING) {
    if (chreWifiConfigureScanMonitorAsync(true, &kScanMonitoringCookie)) {
      LOGI("Scan monitor enable request successful");
    } else {
      LOGE("Error sending scan monitoring request");
    }
  }

  requestDelayedWifiScan();
  return true;
}

void nanoappHandleEvent(uint32_t senderInstanceId, uint16_t eventType,
                        const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_WIFI_ASYNC_RESULT:
      handleWifiAsyncResult(static_cast<const chreAsyncResult *>(eventData));
      break;
    case CHRE_EVENT_WIFI_SCAN_RESULT:
      handleWifiScanEvent(static_cast<const chreWifiScanEvent *>(eventData));
      break;
    case CHRE_EVENT_WIFI_RANGING_RESULT:
      handleWifiRangingEvent(
          static_cast<const chreWifiRangingEvent *>(eventData));
      break;
    case CHRE_EVENT_TIMER:
      handleTimerEvent(eventData);
      break;
    default:
      LOGW("Unhandled event type %" PRIu16, eventType);
  }
}

void nanoappEnd() {
  LOGI("Wifi world app stopped");
}

#ifdef CHRE_NANOAPP_INTERNAL
}  // anonymous namespace
}  // namespace chre

#include "chre/platform/static_nanoapp_init.h"
#include "chre/util/nanoapp/app_id.h"

CHRE_STATIC_NANOAPP_INIT(WifiWorld, chre::kWifiWorldAppId, 0);
#endif  // CHRE_NANOAPP_INTERNAL
