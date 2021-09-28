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

#ifndef _GTS_NANOAPPS_GENERAL_TEST_BASIC_FLUSH_ASYNC_TESTS_H_
#define _GTS_NANOAPPS_GENERAL_TEST_BASIC_FLUSH_ASYNC_TESTS_H_

#include <general_test/test.h>

#include <chre.h>

namespace general_test {

class BasicSensorFlushAsyncTest : public Test {
 public:
  BasicSensorFlushAsyncTest()
      : Test(CHRE_API_VERSION_1_3), mInstanceId(chreGetInstanceId()) {}

 protected:
  void handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                   const void *eventData) override;

  void setUp(uint32_t messageSize, const void *message) override;

 private:
  bool mStarted = false;
  uint32_t mCookie = 0xdeadbeef;
  uint32_t mInstanceId;
  uint32_t mSensorHandle = 0;
  uint32_t mFlushTimeoutTimerHandle = CHRE_TIMER_INVALID;
  uint32_t mFlushStartTimerHandle = CHRE_TIMER_INVALID;
  uint64_t mFlushRequestTime = 0;
  uint64_t mFlushTestTimeWiggleRoomNs = 0;
  uint64_t mLatestSensorDataTimestamp = 0;

  void start();
  void finish(bool succeeded, const char *message);
  void handleDataReceived(const struct chreSensorThreeAxisData *eventData);
  void handleFlushComplete(
      const struct chreSensorFlushCompleteEvent *eventData);
  void handleTimerExpired(const uint32_t *timerHandle);
};

}  // namespace general_test

#endif  // _GTS_NANOAPPS_GENERAL_TEST_BASIC_CHRE_API_TESTS_H_