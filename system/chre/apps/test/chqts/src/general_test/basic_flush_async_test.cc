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

#include <general_test/basic_flush_async_test.h>

#include <cinttypes>

#include <shared/macros.h>
#include <shared/send_message.h>
#include <shared/time_util.h>

using nanoapp_testing::kOneMillisecondInNanoseconds;
using nanoapp_testing::kOneSecondInNanoseconds;
using nanoapp_testing::sendFatalFailureToHost;
using nanoapp_testing::sendSuccessToHost;

namespace general_test {

void BasicSensorFlushAsyncTest::setUp(uint32_t messageSize,
                                      const void *message) {
  constexpr uint64_t kFlushTestLatencyNs = 2 * kOneSecondInNanoseconds;
  constexpr uint64_t kFlushTestStartTimerValueNs =
      kFlushTestLatencyNs / 2;  // start the test at (now + 1/2*latency)

  if (messageSize != 0) {
    sendFatalFailureToHost("Expected 0 byte message, got more bytes:",
                           &messageSize);
  }

  // TODO: Generalize this test for all sensors by making
  // BasicSensorFlushAsyncTest a base class for sensor specific tests for the
  // FlushAsync API
  if (!chreSensorFindDefault(CHRE_SENSOR_TYPE_ACCELEROMETER, &mSensorHandle)) {
    sendFatalFailureToHost("Default Accelerometer not found");
  }

  // We set the sampling period of the sensor to 2x the min interval,
  // and set a variable to track that we get sensor samples within a
  // reasonable (a small order of magnitude greater than the min interval)
  // 'wiggle room' from when we start the flush request.
  struct chreSensorInfo info;
  if (!chreGetSensorInfo(mSensorHandle, &info)) {
    sendFatalFailureToHost("Failed to get sensor info");
  }
  mFlushTestTimeWiggleRoomNs = 20 * info.minInterval;

  if (!chreSensorConfigure(mSensorHandle, CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS,
                           2 * info.minInterval, kFlushTestLatencyNs)) {
    sendFatalFailureToHost("Failed to configure the accelerometer");
  }

  // To exercise the test, we need to confirm that we actually get sensor
  // samples from the flush request. to do this, set a timer to start a flush
  // request at around latency/2 time from now, and request the flush when it
  // expires, hoping to receive some of the data accumulated between configure
  // time and flush request time
  mFlushStartTimerHandle =
      chreTimerSet(kFlushTestStartTimerValueNs, &mFlushStartTimerHandle,
                   true /* one shot */);
  if (CHRE_TIMER_INVALID == mFlushStartTimerHandle) {
    sendFatalFailureToHost("Failed to set flush start timer");
  }
}

void BasicSensorFlushAsyncTest::handleEvent(uint32_t senderInstanceId,
                                            uint16_t eventType,
                                            const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_SENSOR_ACCELEROMETER_DATA:
      handleDataReceived(
          static_cast<const struct chreSensorThreeAxisData *>(eventData));
      break;

    case CHRE_EVENT_SENSOR_FLUSH_COMPLETE:
      handleFlushComplete(
          static_cast<const struct chreSensorFlushCompleteEvent *>(eventData));
      break;

    case CHRE_EVENT_TIMER:
      handleTimerExpired(static_cast<const uint32_t *>(eventData));
      break;

    default:
      break;
  }
}

void BasicSensorFlushAsyncTest::start() {
  mStarted = true;
  mFlushRequestTime = chreGetTime();

  if (!chreSensorFlushAsync(mSensorHandle, &mCookie)) {
    finish(false /* succeeded */, "Async flush failed");
  }

  mFlushTimeoutTimerHandle =
      chreTimerSet(CHRE_SENSOR_FLUSH_COMPLETE_TIMEOUT_NS,
                   &mFlushTimeoutTimerHandle, true /* oneShot */);
  if (CHRE_TIMER_INVALID == mFlushTimeoutTimerHandle) {
    sendFatalFailureToHost("Failed to set flush start timer");
  }
}

void BasicSensorFlushAsyncTest::finish(bool succeeded, const char *message) {
  mStarted = false;

  if (mFlushTimeoutTimerHandle != CHRE_TIMER_INVALID) {
    chreTimerCancel(mFlushTimeoutTimerHandle);
  }

  if (!chreSensorConfigureModeOnly(mSensorHandle,
                                   CHRE_SENSOR_CONFIGURE_MODE_DONE)) {
    sendFatalFailureToHost("Failed to release sensor handle");
  }

  if (!succeeded) {
    ASSERT_NE(message, nullptr, "message cannot be null when the test failed");
    sendFatalFailureToHost(message);
  } else {
    sendSuccessToHost();
  }
}

void BasicSensorFlushAsyncTest::handleDataReceived(
    const struct chreSensorThreeAxisData *eventData) {
  // we're only interested in storing the latest timestamp of the sensor data
  mLatestSensorDataTimestamp = eventData->header.baseTimestamp;
  for (int i = 0; i < eventData->header.readingCount; ++i) {
    mLatestSensorDataTimestamp += eventData->readings[i].timestampDelta;
  }
}

void BasicSensorFlushAsyncTest::handleFlushComplete(
    const struct chreSensorFlushCompleteEvent *eventData) {
  if (mStarted) {
    ASSERT_NE(mLatestSensorDataTimestamp, 0, "No sensor data was received");

    // we should fail the test if we receive too old a sensor sample.
    // ideally, we don't receive any samples that was sampled after
    // our flush request, but for this test, we'll be lenient and assume
    // that anything between [flushRequestTime - kFlushTestTimeWiggleRoomNs,
    // now] is OK.
    uint64_t oldestValidTimestamp =
        mFlushRequestTime - mFlushTestTimeWiggleRoomNs;

    ASSERT_GE(mLatestSensorDataTimestamp, oldestValidTimestamp,
              "Received very old data");

    chreLog(CHRE_LOG_INFO,
            "Flush test: flush request to complete time: %" PRIu64 " ms",
            (chreGetTime() - mFlushRequestTime) / kOneMillisecondInNanoseconds);

    // verify event data
    ASSERT_NE(eventData, nullptr, "null event data");
    ASSERT_EQ(eventData->sensorHandle, mSensorHandle,
              "Got flush event from a different sensor handle");
    ASSERT_EQ(eventData->errorCode, CHRE_ERROR_NONE,
              "Flush Error code was not CHRE_ERROR_NONE");
    ASSERT_NE(eventData->cookie, nullptr,
              "Null cookie in flush complete event");
    ASSERT_EQ(*(static_cast<const uint32_t *>(eventData->cookie)), mCookie,
              "unexpected cookie in flush complete event");

    finish(true /* succeeded */, nullptr /* message */);
  }
}

void BasicSensorFlushAsyncTest::handleTimerExpired(
    const uint32_t *timerHandle) {
  if (timerHandle != nullptr) {
    if (mFlushStartTimerHandle == *timerHandle) {
      start();
    } else if (mFlushTimeoutTimerHandle == *timerHandle) {
      finish(false /* succeeded */,
             "Did not receive flush complete event in time");
    } else {
      sendFatalFailureToHost("Unexpected timer handle received");
    }
  } else {
    sendFatalFailureToHost("Null timer handle received");
  }
}

}  // namespace general_test