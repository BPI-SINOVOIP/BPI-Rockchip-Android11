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

#include "chre_audio_concurrency_test_manager.h"

#include <pb_decode.h>

#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"
#include "chre_audio_concurrency_test.nanopb.h"
#include "send_message.h"

#define LOG_TAG "[ChreAudioConcurrencyTest]"

namespace chre {

using test_shared::sendEmptyMessageToHost;
using test_shared::sendTestResultToHost;

namespace audio_concurrency_test {

namespace {

//! The message type to use with sendTestResultToHost()
constexpr uint32_t kTestResultMessageType =
    chre_audio_concurrency_test_MessageType_TEST_RESULT;

bool isTestSupported() {
  // CHRE audio was supported in CHRE v1.2
  return chreGetVersion() >= CHRE_API_VERSION_1_2;
}

bool getTestStep(const chre_audio_concurrency_test_TestCommand &command,
                 Manager::TestStep *step) {
  bool success = true;
  switch (command.step) {
    case chre_audio_concurrency_test_TestCommand_Step_ENABLE_AUDIO:
      *step = Manager::TestStep::ENABLE_AUDIO;
      break;
    case chre_audio_concurrency_test_TestCommand_Step_VERIFY_AUDIO_RESUME:
      *step = Manager::TestStep::VERIFY_AUDIO_RESUME;
      break;
    default:
      LOGE("Unknown test step %d", command.step);
      success = false;
  }

  return success;
}

}  // anonymous namespace

Manager::~Manager() {
  if (mAudioEnabled) {
    chreAudioConfigureSource(kAudioHandle, false /* enable */,
                             0 /* bufferDuration */, 0 /* deliveryInterval */);
  }
  cancelTimeoutTimer();
}

bool Manager::handleTestCommandMessage(uint16_t hostEndpointId, TestStep step) {
  bool success = true;

  // Treat as success if CHRE audio is unsupported
  // TODO: Use all available audio sources
  if (!isTestSupported() || !chreAudioGetSource(kAudioHandle, &mAudioSource)) {
    sendTestResultToHost(hostEndpointId, kTestResultMessageType,
                         true /* success */);
  } else {
    success = false;
    if (step == TestStep::ENABLE_AUDIO) {
      if (!chreAudioConfigureSource(kAudioHandle, true /* enable */,
                                    mAudioSource.minBufferDuration,
                                    mAudioSource.minBufferDuration)) {
        LOGE("Failed to configure audio source");
      } else {
        mAudioEnabled = true;
        // Start a timer to ensure we receive the first audio data event
        // quickly. Since it may take some time to load the sound model, choose
        // a reasonably long timeout.
        success = setTimeoutTimer(20 /* durationSeconds */);
      }
    } else if (step == TestStep::VERIFY_AUDIO_RESUME) {
      success = setTimeoutTimer(20 /* durationSeconds */);
    }

    if (success) {
      mTestSession = TestSession(hostEndpointId, step);
      LOGI("Starting test step %" PRIu8, mTestSession->step);
    }
  }

  return success;
}

void Manager::handleMessageFromHost(uint32_t senderInstanceId,
                                    const chreMessageFromHostData *hostData) {
  bool success = false;
  uint32_t messageType = hostData->messageType;
  if (senderInstanceId != CHRE_INSTANCE_ID) {
    LOGE("Incorrect sender instance id: %" PRIu32, senderInstanceId);
  } else if (messageType !=
             chre_audio_concurrency_test_MessageType_TEST_COMMAND) {
    LOGE("Invalid message type %" PRIu32, messageType);
  } else {
    pb_istream_t istream = pb_istream_from_buffer(
        static_cast<const pb_byte_t *>(hostData->message),
        hostData->messageSize);
    chre_audio_concurrency_test_TestCommand testCommand =
        chre_audio_concurrency_test_TestCommand_init_default;

    TestStep step;
    if (!pb_decode(&istream, chre_audio_concurrency_test_TestCommand_fields,
                   &testCommand)) {
      LOGE("Failed to decode start command error %s", PB_GET_ERROR(&istream));
    } else if (getTestStep(testCommand, &step)) {
      success = handleTestCommandMessage(hostData->hostEndpoint, step);
    }
  }

  if (!success) {
    sendTestResultToHost(hostData->hostEndpoint, kTestResultMessageType,
                         false /* success */);
  }
}

void Manager::handleDataFromChre(uint16_t eventType, const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_AUDIO_DATA:
      handleAudioDataEvent(static_cast<const chreAudioDataEvent *>(eventData));
      break;

    case CHRE_EVENT_TIMER:
      handleTimer();
      break;

    case CHRE_EVENT_AUDIO_SAMPLING_CHANGE:
      /* ignore */
      break;

    default:
      LOGE("Unexpected event type %" PRIu16, eventType);
  }
}

void Manager::handleTimer() {
  if (mTimerHandle != CHRE_TIMER_INVALID && mTestSession.has_value()) {
    LOGE("Timed out during test: step %" PRIu8, mTestSession->step);
    sendTestResultToHost(mTestSession->hostEndpointId, kTestResultMessageType,
                         false /* success */);
    mTestSession.reset();
  }
}

bool Manager::setTimeoutTimer(uint32_t durationSeconds) {
  mTimerHandle = chreTimerSet(durationSeconds * kOneSecondInNanoseconds,
                              nullptr /* cookie */, true /* oneShot */);
  if (mTimerHandle == CHRE_TIMER_INVALID) {
    LOGE("Failed to set timeout timer");
  }

  return mTimerHandle != CHRE_TIMER_INVALID;
}

void Manager::cancelTimeoutTimer() {
  if (mTimerHandle != CHRE_TIMER_INVALID) {
    chreTimerCancel(mTimerHandle);
    mTimerHandle = CHRE_TIMER_INVALID;
  }
}

bool Manager::validateAudioDataEvent(const chreAudioDataEvent *data) {
  bool ulaw8 = false;
  if (data->format == CHRE_AUDIO_DATA_FORMAT_8_BIT_U_LAW) {
    ulaw8 = true;
  } else if (data->format != CHRE_AUDIO_DATA_FORMAT_16_BIT_SIGNED_PCM) {
    LOGE("Invalid format %" PRIu8, data->format);
    return false;
  }

  // Verify that the audio data is not all zeroes
  uint32_t numZeroes = 0;
  for (uint32_t i = 0; i < data->sampleCount; i++) {
    numZeroes +=
        ulaw8 ? (data->samplesULaw8[i] == 0) : (data->samplesS16[i] == 0);
  }
  bool dataValid = numZeroes != data->sampleCount;

  // Verify that timestamp increases
  static uint64_t lastTimestamp = 0;
  bool timestampValid = data->timestamp > lastTimestamp;
  lastTimestamp = data->timestamp;

  return dataValid && timestampValid;
}

void Manager::handleAudioDataEvent(const chreAudioDataEvent *data) {
  if (mTestSession.has_value()) {
    if (!validateAudioDataEvent(data)) {
      sendTestResultToHost(mTestSession->hostEndpointId, kTestResultMessageType,
                           false /* success */);
      mTestSession.reset();
    } else {
      switch (mTestSession->step) {
        case TestStep::ENABLE_AUDIO: {
          cancelTimeoutTimer();
          sendEmptyMessageToHost(
              mTestSession->hostEndpointId,
              chre_audio_concurrency_test_MessageType_TEST_AUDIO_ENABLED);

          // Reset the test session to avoid sending multiple TEST_AUDIO_ENABLED
          // messages to the host, while we wait for the next step.
          mTestSession.reset();
          break;
        }

        case TestStep::VERIFY_AUDIO_RESUME: {
          cancelTimeoutTimer();
          sendTestResultToHost(mTestSession->hostEndpointId,
                               kTestResultMessageType, true /* success */);
          mTestSession.reset();
          break;
        }

        default:
          LOGE("Unexpected test step %" PRIu8, mTestSession->step);
          break;
      }
    }
  }
}

void Manager::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                          const void *eventData) {
  if (eventType == CHRE_EVENT_MESSAGE_FROM_HOST) {
    handleMessageFromHost(
        senderInstanceId,
        static_cast<const chreMessageFromHostData *>(eventData));
  } else if (senderInstanceId == CHRE_INSTANCE_ID) {
    handleDataFromChre(eventType, eventData);
  } else {
    LOGW("Got unknown event type from senderInstanceId %" PRIu32
         " and with eventType %" PRIu16,
         senderInstanceId, eventType);
  }
}

}  // namespace audio_concurrency_test

}  // namespace chre
