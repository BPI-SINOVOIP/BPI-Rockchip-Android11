/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <general_test/basic_wifi_test.h>

#include <algorithm>
#include <cmath>

#include <chre.h>
#include <shared/macros.h>
#include <shared/send_message.h>
#include <shared/time_util.h>

#include "chre/util/time.h"
#include "chre/util/unique_ptr.h"

using nanoapp_testing::sendFatalFailureToHost;
using nanoapp_testing::sendFatalFailureToHostUint8;
using nanoapp_testing::sendSuccessToHost;

/*
 * Test to check expected functionality of the CHRE WiFi APIs.
 *
 * 1. If scan monitor is not supported, skip to 5;
 *    otherwise enables scan monitor.
 * 2. Checks async result of enabling scan monitor.
 * 3. Disables scan monitor.
 * 4. Checks async result of disabling scan monitor.
 * 5. If on demand WiFi scan is not supported, skip to end;
 *    otherwise sends default scan request.
 * 6. Checks the result of on demand WiFi scan.
 */
namespace general_test {

namespace {

//! A dummy cookie to pass into the enable configure scan monitoring async
//! request.
constexpr uint32_t kEnableScanMonitoringCookie = 0x1337;

//! A dummy cookie to pass into the disable configure scan monitoring async
//! request.
constexpr uint32_t kDisableScanMonitoringCookie = 0x1338;

//! A dummy cookie to pass into request ranging async.
constexpr uint32_t kRequestRangingCookie = 0xefac;

//! A dummy cookie to pass into request scan async.
constexpr uint32_t kOnDemandScanCookie = 0xcafe;

//! Starting frequency of band 2.4 GHz
constexpr uint32_t kWifiBandStartFreq_2_4_GHz = 2407;

//! Starting frequency of band 5 GHz
constexpr uint32_t kWifiBandStartFreq_5_GHz = 5000;

//! Frequency of channel 14
constexpr uint32_t kWifiBandFreqOfChannel_14 = 2484;

//! The amount of time to allow between an operation timing out and the event
//! being deliverd to the test.
constexpr uint32_t kTimeoutWiggleRoomNs = 2 * chre::kOneSecondInNanoseconds;

/**
 * Calls API testConfigureScanMonitorAsync. Sends fatal failure to host
 * if API call fails.
 *
 * @param enable Set to true to enable monitoring scan results,
 *        false to disable.
 * @param cookie An opaque value that will be included in the chreAsyncResult
 *        sent in relation to this request.
 */
void testConfigureScanMonitorAsync(bool enable, const void *cookie) {
  if (!chreWifiConfigureScanMonitorAsync(enable, cookie)) {
    if (enable) {
      sendFatalFailureToHost("Failed to request to enable scan monitor.");
    } else {
      sendFatalFailureToHost("Failed to request to disable scan monitor.");
    }
  }
}

/**
 * Calls API chreWifiRequestScanAsyncDefault. Sends fatal failure to host
 * if API call fails.
 */
void testRequestScanAsync() {
  if (!chreWifiRequestScanAsyncDefault(&kOnDemandScanCookie)) {
    sendFatalFailureToHost("Failed to request for on-demand WiFi scan.");
  }
}

/**
 * Calls API chreWifiRequestRangingAsync. Sends fatal failure to host if the
 * API call fails.
 */
void testRequestRangingAsync(const struct chreWifiScanResult *aps,
                             uint8_t length) {
  // Sending an array larger than CHRE_WIFI_RANGING_LIST_MAX_LEN will cause
  // an immediate failure.
  uint8_t targetLength =
      std::min(length, static_cast<uint8_t>(CHRE_WIFI_RANGING_LIST_MAX_LEN));

  void *array = chreHeapAlloc(sizeof(chreWifiRangingTarget) * targetLength);
  ASSERT_NE(array, nullptr,
            "Failed to allocate array for issuing a ranging request");

  chre::UniquePtr<struct chreWifiRangingTarget> targetList(
      static_cast<struct chreWifiRangingTarget *>(array));

  // Save the last spot for any available RTT APs in case they didn't make it
  // in the array earlier. This first loop allows non-RTT compatible APs as a
  // way to test that the driver implementation will return failure for only
  // those APs and success for valid RTT APs.
  for (uint8_t i = 0; i < targetLength - 1; i++) {
    chreWifiRangingTargetFromScanResult(&aps[i], &targetList[i]);
  }

  for (uint8_t i = targetLength - 1; i < length; i++) {
    if ((aps[i].flags & CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER) ==
            CHRE_WIFI_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER ||
        i == (length - 1)) {
      chreWifiRangingTargetFromScanResult(&aps[i],
                                          &targetList[targetLength - 1]);
      break;
    }
  }

  struct chreWifiRangingParams params = {.targetListLen = targetLength,
                                         .targetList = targetList.get()};
  if (!chreWifiRequestRangingAsync(&params, &kRequestRangingCookie)) {
    sendFatalFailureToHost(
        "Failed to request ranging for a list of WiFi scans.");
  }
}

/**
 * Validates primaryChannel and sends fatal failure to host if failing.
 * 1. (primaryChannel - start frequecny) is a multiple of 5.
 * 2. primaryChannelNumber is multiple of 5 and between [1, maxChannelNumber].
 *
 * @param primaryChannel primary channel of a WiFi scan result.
 * @param startFrequency start frequency of band 2.4/5 GHz.
 * @param maxChannelNumber max channel number of band 2.4/5 GHz.
 */
void validatePrimaryChannel(uint32_t primaryChannel, uint32_t startFrequency,
                            uint8_t maxChannelNumber) {
  if ((primaryChannel - startFrequency) % 5 != 0) {
    chreLog(CHRE_LOG_ERROR,
            "primaryChannel - %" PRIu32
            " must be a multiple of 5,"
            "got primaryChannel: %" PRIu32,
            startFrequency, primaryChannel);
  }

  uint32_t primaryChannelNumber = (primaryChannel - startFrequency) / 5;
  if (primaryChannelNumber < 1 || primaryChannelNumber > maxChannelNumber) {
    chreLog(CHRE_LOG_ERROR,
            "primaryChannelNumber must be between 1 and %" PRIu8
            ","
            "got primaryChannel: %" PRIu32,
            maxChannelNumber, primaryChannel);
  }
}

/**
 * Validates primaryChannel for band 2.4/5 GHz.
 *
 * primaryChannelNumber of band 2.4 GHz is between 1 and 13,
 * plus a special case for channel 14 (primaryChannel == 2484);
 * primaryChannelNumber of band 5 GHz is between 1 and 200,
 * ref: IEEE Std 802.11-2016, 19.3.15.2.
 * Also, (primaryChannel - start frequecny) is a multiple of 5,
 * except channel 14 of 2.4 GHz.
 *
 * @param result WiFi scan result.
 */
void validatePrimaryChannel(const chreWifiScanResult &result) {
  // channel 14 (primaryChannel = 2484) is not applicable for this test.
  if (result.band == CHRE_WIFI_BAND_2_4_GHZ &&
      result.primaryChannel != kWifiBandFreqOfChannel_14) {
    validatePrimaryChannel(result.primaryChannel, kWifiBandStartFreq_2_4_GHz,
                           13);
  } else if (result.band == CHRE_WIFI_BAND_5_GHZ) {
    validatePrimaryChannel(result.primaryChannel, kWifiBandStartFreq_5_GHz,
                           200);
  }
}

/**
 * Validates centerFreqPrimary and centerFreqSecondary
 * TODO (jacksun) add test when channelWidth is 20, 40, 80, or 160 MHz
 */
void validateCenterFreq(const chreWifiScanResult &result) {
  if (result.channelWidth != CHRE_WIFI_CHANNEL_WIDTH_80_PLUS_80_MHZ &&
      result.centerFreqSecondary != 0) {
    // TODO (jacksun) Format the centerFreqSecondary into the message
    // after redesigning of sendFatalFailureToHost()
    sendFatalFailureToHost(
        "centerFreqSecondary must be 0 if channelWidth is not 80+80MHZ");
  }
}

/**
 * Validates that RSSI is within sane limits.
 */
void validateRssi(int8_t rssi) {
  // It's possible for WiFi RSSI to be positive if the phone is placed
  // right next to a high-power AP (e.g. transmitting at 20 dBm),
  // in which case RSSI will be < 20 dBm. Place a high threshold to check
  // against values likely to be erroneous (36 dBm/4W).
  ASSERT_LT(rssi, 36, "RSSI is greater than 36");
}

/**
 * Validates that the amount of access points ranging was requested for matches
 * the number of ranging results returned. Also, verifies that the BSSID of
 * the each access point is present in the ranging results.
 */
void validateRangingEventArray(const struct chreWifiScanResult *results,
                               size_t resultsSize,
                               const struct chreWifiRangingEvent *event) {
  size_t expectedArraySize = std::min(
      resultsSize, static_cast<size_t>(CHRE_WIFI_RANGING_LIST_MAX_LEN));
  ASSERT_EQ(event->resultCount, expectedArraySize,
            "RTT ranging result count was not the same as the requested target "
            "list size");

  uint8_t matchesFound = 0;

  for (size_t i = 0; i < resultsSize; i++) {
    for (size_t j = 0; j < expectedArraySize; j++) {
      if (memcmp(results[i].bssid, event->results[j].macAddress,
                 CHRE_WIFI_BSSID_LEN) == 0) {
        matchesFound++;
        break;
      }
    }
  }

  ASSERT_EQ(
      matchesFound, expectedArraySize,
      "BSSID(s) from the ranging request were not found in the ranging result");
}

/**
 * Validates the location configuration information returned by a ranging result
 * is compliant with the formatting specified at @see chreWifiLci.
 */
void validateLci(const struct chreWifiRangingResult::chreWifiLci *lci) {
  // Per RFC 6225 2.3, there are 25 fractional bits and up to 9 integer bits
  // used for lat / lng so verify that no bits outside those are used.
  constexpr int64_t kMaxLat = INT64_C(90) << 25;
  constexpr int64_t kMaxLng = INT64_C(180) << 25;
  ASSERT_IN_RANGE(lci->latitude, -1 * kMaxLat, kMaxLat,
                  "LCI's latitude is outside the range of -90 to 90");
  ASSERT_IN_RANGE(lci->longitude, -1 * kMaxLng, kMaxLng,
                  "LCI's longitude is outside the range of -180 to 180");

  // According to RFC 6225, values greater than 34 are reserved
  constexpr uint8_t kMaxLatLngUncertainty = 34;
  ASSERT_LE(lci->latitudeUncertainty, kMaxLatLngUncertainty,
            "LCI's latitude uncertainty is greater than 34");
  ASSERT_LE(lci->longitudeUncertainty, kMaxLatLngUncertainty,
            "LCI's longitude uncertainty is greater than 34");

  if (lci->altitudeType == CHRE_WIFI_LCI_ALTITUDE_TYPE_METERS) {
    // Highest largely populated city in the world, El Alto, Bolivia, is 4300
    // meters and the tallest building in the world is 828 meters so the upper
    // bound for this range should be 5500 meters (contains some padding).
    constexpr int32_t kMaxAltitudeMeters = 5500 << 8;

    // Lowest largely populated city in the world, Baku, Azerbaijan, is 28
    // meters below sea level so -100 meters should be a good lower bound.
    constexpr int32_t kMinAltitudeMeters = (100 << 8) * -1;
    ASSERT_IN_RANGE(
        lci->altitude, kMinAltitudeMeters, kMaxAltitudeMeters,
        "LCI's altitude is outside of the range of -25 to 500 meters");

    // According to RFC 6225, values greater than 30 are reserved
    constexpr uint8_t kMaxAltitudeUncertainty = 30;
    ASSERT_LE(lci->altitudeUncertainty, kMaxAltitudeUncertainty,
              "LCI's altitude certainty is greater than 30");
  } else if (lci->altitudeType == CHRE_WIFI_LCI_ALTITUDE_TYPE_FLOORS) {
    // Tallest building has 163 floors. Assume -5 to 100 floors is a sane range.
    constexpr int32_t kMaxAltitudeFloors = 100 << 8;
    constexpr int32_t kMinAltitudeFloors = (5 << 8) * -1;
    ASSERT_IN_RANGE(
        lci->altitude, kMinAltitudeFloors, kMaxAltitudeFloors,
        "LCI's altitude is outside of the range of -5 to 100 floors");
  } else if (lci->altitudeType != CHRE_WIFI_LCI_ALTITUDE_TYPE_UNKNOWN) {
    sendFatalFailureToHost(
        "LCI's altitude type was not unknown, floors, or meters");
  }
}

}  // anonymous namespace

BasicWifiTest::BasicWifiTest() : Test(CHRE_API_VERSION_1_1) {}

void BasicWifiTest::setUp(uint32_t messageSize, const void * /* message */) {
  if (messageSize != 0) {
    sendFatalFailureToHost("Expected 0 byte message, got more bytes:",
                           &messageSize);
  } else {
    mWifiCapabilities = chreWifiGetCapabilities();
    startScanMonitorTestStage();
  }
}

void BasicWifiTest::handleEvent(uint32_t /* senderInstanceId */,
                                uint16_t eventType, const void *eventData) {
  ASSERT_NE(eventData, nullptr, "Received null eventData");
  switch (eventType) {
    case CHRE_EVENT_WIFI_ASYNC_RESULT:
      handleChreWifiAsyncEvent(static_cast<const chreAsyncResult *>(eventData));
      break;
    case CHRE_EVENT_WIFI_SCAN_RESULT: {
      if (!scanEventExpected()) {
        sendFatalFailureToHost("WiFi scan event received when not requested");
      }
      const auto *result = static_cast<const chreWifiScanEvent *>(eventData);
      if (isActiveWifiScanType(result)) {
        // The first chreWifiScanResult is expected to come immediately,
        // but a long delay is possible if it's implemented incorrectly,
        // e.g. the async result comes right away (before the scan is actually
        // completed), then there's a long delay to the scan result.
        if (mStartTimestampNs != 0 &&
            chreGetTime() - mStartTimestampNs >
                50 * chre::kOneMillisecondInNanoseconds) {
          sendFatalFailureToHost(
              "Did not receive chreWifiScanResult within 50 milliseconds.");
        }
        mStartTimestampNs = 0;
        validateWifiScanEvent(result);
      }
      break;
    }
    case CHRE_EVENT_WIFI_RANGING_RESULT: {
      if (!rangingEventExpected()) {
        sendFatalFailureToHost(
            "WiFi ranging event received when not requested");
      }
      const auto *result = static_cast<const chreWifiRangingEvent *>(eventData);
      // Allow some wiggle room between the expected timeout and when the event
      // would actually be delivered to the test.
      if (mStartTimestampNs != 0 &&
          chreGetTime() - mStartTimestampNs >
              CHRE_WIFI_RANGING_RESULT_TIMEOUT_NS + kTimeoutWiggleRoomNs) {
        sendFatalFailureToHost(
            "Did not receive chreWifiRangingEvent within the ranging timeout");
      }
      validateRangingEvent(result);
      // Ensure timestamp is reset after everything is validated as it's used to
      // validate the ranging event
      mStartTimestampNs = 0;
      mTestSuccessMarker.markStageAndSuccessOnFinish(
          BASIC_WIFI_TEST_STAGE_SCAN_RTT);
      break;
    }
    default:
      unexpectedEvent(eventType);
      break;
  }
}

void BasicWifiTest::handleChreWifiAsyncEvent(const chreAsyncResult *result) {
  if (!mCurrentWifiRequest.has_value()) {
    nanoapp_testing::sendFailureToHost("Unexpected async result");
  }

  validateChreAsyncResult(result, mCurrentWifiRequest.value());

  switch (result->requestType) {
    case CHRE_WIFI_REQUEST_TYPE_RANGING:
      // Reuse same start timestamp as the scan request since ranging fields
      // may be retrieved automatically as part of that scan.
      break;
    case CHRE_WIFI_REQUEST_TYPE_REQUEST_SCAN:
      mStartTimestampNs = chreGetTime();
      break;
    case CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR:
      if (mCurrentWifiRequest->cookie == &kDisableScanMonitoringCookie) {
        mTestSuccessMarker.markStageAndSuccessOnFinish(
            BASIC_WIFI_TEST_STAGE_SCAN_MONITOR);
        startScanAsyncTestStage();
      } else {
        testConfigureScanMonitorAsync(false /* enable */,
                                      &kDisableScanMonitoringCookie);
        resetCurrentWifiRequest(&kDisableScanMonitoringCookie,
                                CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR,
                                CHRE_ASYNC_RESULT_TIMEOUT_NS);
      }
      break;
    default:
      sendFatalFailureToHostUint8("Received unexpected requestType %d",
                                  result->requestType);
      break;
  }
}

bool BasicWifiTest::isActiveWifiScanType(const chreWifiScanEvent *eventData) {
  return (eventData->scanType == CHRE_WIFI_SCAN_TYPE_ACTIVE);
}

void BasicWifiTest::startScanMonitorTestStage() {
  if (mWifiCapabilities & CHRE_WIFI_CAPABILITIES_SCAN_MONITORING) {
    testConfigureScanMonitorAsync(true /* enable */,
                                  &kEnableScanMonitoringCookie);
    resetCurrentWifiRequest(&kEnableScanMonitoringCookie,
                            CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR,
                            CHRE_ASYNC_RESULT_TIMEOUT_NS);
  } else {
    mTestSuccessMarker.markStageAndSuccessOnFinish(
        BASIC_WIFI_TEST_STAGE_SCAN_MONITOR);
    startScanAsyncTestStage();
  }
}

void BasicWifiTest::startScanAsyncTestStage() {
  if (mWifiCapabilities & CHRE_WIFI_CAPABILITIES_ON_DEMAND_SCAN) {
    testRequestScanAsync();
    resetCurrentWifiRequest(&kOnDemandScanCookie,
                            CHRE_WIFI_REQUEST_TYPE_REQUEST_SCAN,
                            CHRE_WIFI_SCAN_RESULT_TIMEOUT_NS);
  } else {
    mTestSuccessMarker.markStageAndSuccessOnFinish(
        BASIC_WIFI_TEST_STAGE_SCAN_ASYNC);
    startRangingAsyncTestStage();
  }
}

void BasicWifiTest::startRangingAsyncTestStage() {
  // If no scans were received, the test has nothing to range with so simply
  // mark it as a success.
  if (mWifiCapabilities & CHRE_WIFI_CAPABILITIES_RTT_RANGING &&
      mLatestWifiScanResults.size() != 0) {
    testRequestRangingAsync(mLatestWifiScanResults.data(),
                            mLatestWifiScanResults.size());
    resetCurrentWifiRequest(&kRequestRangingCookie,
                            CHRE_WIFI_REQUEST_TYPE_RANGING,
                            CHRE_WIFI_RANGING_RESULT_TIMEOUT_NS);
  } else {
    mTestSuccessMarker.markStageAndSuccessOnFinish(
        BASIC_WIFI_TEST_STAGE_SCAN_RTT);
  }
}

void BasicWifiTest::resetCurrentWifiRequest(const void *cookie,
                                            uint8_t requestType,
                                            uint64_t timeoutNs) {
  chreAsyncRequest request = {.cookie = cookie,
                              .requestType = requestType,
                              .requestTimeNs = chreGetTime(),
                              .timeoutNs = timeoutNs};
  mCurrentWifiRequest = request;
}

void BasicWifiTest::validateWifiScanEvent(const chreWifiScanEvent *eventData) {
  if (eventData->version != CHRE_WIFI_SCAN_EVENT_VERSION) {
    sendFatalFailureToHostUint8("Got unexpected scan event version %d",
                                eventData->version);
  }

  if (mNextExpectedIndex != eventData->eventIndex) {
    chreLog(CHRE_LOG_ERROR,
            "Expected index: %" PRIu32 ", received index: %" PRIu8,
            mNextExpectedIndex, eventData->eventIndex);
    sendFatalFailureToHost("Received out-of-order events");
  }
  mNextExpectedIndex++;

  if (eventData->eventIndex == 0) {
    mWiFiScanResultRemaining = eventData->resultTotal;
  }
  if (mWiFiScanResultRemaining < eventData->resultCount) {
    chreLog(CHRE_LOG_ERROR,
            "Remaining scan results %" PRIu32 ", received %" PRIu8,
            mWiFiScanResultRemaining, eventData->resultCount);
    sendFatalFailureToHost("Received too many WiFi scan results");
  }
  mWiFiScanResultRemaining -= eventData->resultCount;

  validateWifiScanResult(eventData->resultCount, eventData->results);

  // Save the latest results for future tests retaining old data if the new
  // scan is empty (so the test has something to use).
  if (eventData->resultCount > 0) {
    mLatestWifiScanResults.copy_array(eventData->results,
                                      eventData->resultCount);
  }

  if (mWiFiScanResultRemaining == 0) {
    mNextExpectedIndex = 0;
    mTestSuccessMarker.markStageAndSuccessOnFinish(
        BASIC_WIFI_TEST_STAGE_SCAN_ASYNC);
    startRangingAsyncTestStage();
  }
}

void BasicWifiTest::validateWifiScanResult(uint8_t count,
                                           const chreWifiScanResult *results) {
  for (uint8_t i = 0; i < count; ++i) {
    if (results[i].ssidLen > CHRE_WIFI_SSID_MAX_LEN) {
      sendFatalFailureToHostUint8("Got unexpected ssidLen %d",
                                  results[i].ssidLen);
    }

    // TODO: Enable fatal failures on band, RSSI, and primary channel
    //       validations when proper error waiver is implemented in CHQTS.
    if (results[i].band != CHRE_WIFI_BAND_2_4_GHZ &&
        results[i].band != CHRE_WIFI_BAND_5_GHZ) {
      chreLog(CHRE_LOG_ERROR, "Got unexpected band %d", results[i].band);
    }

    validateRssi(results[i].rssi);

    validatePrimaryChannel(results[i]);
    validateCenterFreq(results[i]);
  }
}

void BasicWifiTest::validateRangingEvent(
    const chreWifiRangingEvent *eventData) {
  if (eventData->version != CHRE_WIFI_RANGING_EVENT_VERSION) {
    sendFatalFailureToHostUint8("Got unexpected ranging event version %d",
                                eventData->version);
  }

  validateRangingEventArray(mLatestWifiScanResults.data(),
                            mLatestWifiScanResults.size(), eventData);

  for (uint8_t i = 0; i < eventData->resultCount; i++) {
    auto &result = eventData->results[i];
    ASSERT_IN_RANGE(result.timestamp, mStartTimestampNs, chreGetTime(),
                    "Ranging result timestamp isn't between the ranging "
                    "request start time and the current time");

    if (result.status != CHRE_WIFI_RANGING_STATUS_SUCCESS) {
      if (result.rssi != 0 || result.distance != 0 ||
          result.distanceStdDev != 0) {
        sendFatalFailureToHost(
            "Ranging result with failure status had non-zero state");
      }
    } else {
      validateRssi(result.rssi);

      constexpr uint32_t kMaxDistanceMillimeters = 100 * 1000;
      if (result.distance > kMaxDistanceMillimeters) {
        sendFatalFailureToHost(
            "Ranging result was more than 100 meters away %" PRIu32,
            &result.distance);
      }

      constexpr uint32_t kMaxStdDevMillimeters = 10 * 1000;
      if (result.distanceStdDev > kMaxStdDevMillimeters) {
        sendFatalFailureToHost(
            "Ranging result distance stddev was more than 10 meters %" PRIu32,
            &result.distanceStdDev);
      }

      if (result.flags & CHRE_WIFI_RTT_RESULT_HAS_LCI) {
        validateLci(&result.lci);
      }
    }
  }
}

bool BasicWifiTest::rangingEventExpected() {
  return mTestSuccessMarker.isStageMarked(BASIC_WIFI_TEST_STAGE_SCAN_ASYNC) &&
         !mTestSuccessMarker.isStageMarked(BASIC_WIFI_TEST_STAGE_SCAN_RTT);
}

bool BasicWifiTest::scanEventExpected() {
  return mTestSuccessMarker.isStageMarked(BASIC_WIFI_TEST_STAGE_SCAN_MONITOR) &&
         !mTestSuccessMarker.isStageMarked(BASIC_WIFI_TEST_STAGE_SCAN_ASYNC);
}

}  // namespace general_test
