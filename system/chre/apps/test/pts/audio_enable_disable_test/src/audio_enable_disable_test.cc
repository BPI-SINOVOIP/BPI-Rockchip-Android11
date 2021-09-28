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

/**
 * A simple nanoapp to test the CHRE audio feature.
 *
 * Test flow:
 * 1) Nanoapp waits for a TEST_START message from the host.
 * 2) Nanoapp finds a suitable audio source and requests for data.
 * 3) Upon receiving an audio data event, cancel the audio request.
 * 4) Verify that we do not receive any audio data events for a few seconds,
 *    and report test success.
 */

#include <cinttypes>

#include <chre.h>
#include <pb_encode.h>

#include "chre/util/nanoapp/callbacks.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/time.h"
#include "pts_chre.nanopb.h"

#define LOG_TAG "[PtsAudioEnableDisable]"

namespace chre {

namespace {

//! The audio handle to use, currently assuming only one source and we use the
//! first one available.
constexpr uint32_t kAudioHandle = 0;

//! The audio source to use during the test.
struct chreAudioSource gAudioSource;

//! The endpoint ID of the test app host.
uint16_t gHostEndpointId;

//! True if the nanoapp has enabled audio, expecting an audio data event.
//! TODO: Use TestSuccessMarker instead
bool gAudioEnabled = false;

//! True if the test is currently running.
bool gTestRunning = false;

//! Timer for validating no audio data when disabled.
uint32_t gTimerHandle = CHRE_TIMER_INVALID;

// TODO: Refactor this in a shared helper function.
void sendTestResult(uint16_t hostEndpointId, bool success) {
  pts_chre_TestResult result = pts_chre_TestResult_init_default;
  result.has_code = true;
  result.code = success ? pts_chre_TestResult_Code_TEST_PASSED
                        : pts_chre_TestResult_Code_TEST_FAILED;
  size_t size;
  if (!pb_get_encoded_size(&size, pts_chre_TestResult_fields, &result)) {
    LOGE("Failed to get message size");
  } else {
    pb_byte_t *bytes = static_cast<pb_byte_t *>(chreHeapAlloc(size));
    if (bytes == nullptr) {
      LOGE("Could not allocate message size %zu", size);
    } else {
      pb_ostream_t stream = pb_ostream_from_buffer(bytes, size);
      if (!pb_encode(&stream, pts_chre_TestResult_fields, &result)) {
        LOGE("Failed to encode protobuf error %s", PB_GET_ERROR(&stream));
        chreHeapFree(bytes);
      } else {
        chreSendMessageToHostEndpoint(bytes, size,
                                      pts_chre_MessageType_PTS_TEST_RESULT,
                                      hostEndpointId, heapFreeMessageCallback);
        gTestRunning = false;
      }
    }
  }
}

bool discoverAudioSource() {
  return chreAudioGetSource(kAudioHandle, &gAudioSource);
}

void handleMessageFromHost(const chreMessageFromHostData *message) {
  if (message->messageType != pts_chre_MessageType_PTS_TEST_START) {
    LOGE("Unexpected message from host: type %" PRIu32, message->messageType);
  } else {
    gTestRunning = true;
    gHostEndpointId = message->hostEndpoint;

    bool success = false;
    if (!discoverAudioSource()) {
      LOGE("Failed to find audio source");
    } else if (!chreAudioConfigureSource(kAudioHandle, true /* enable */,
                                         gAudioSource.minBufferDuration,
                                         gAudioSource.minBufferDuration)) {
      LOGE("Failed to enable audio source");
    } else {
      gAudioEnabled = true;
      // Start a timer to ensure we receive the first audio data event quickly.
      // Since it may take some time to load the sound model, choose a
      // reasonably long timeout.
      gTimerHandle = chreTimerSet(20 * kOneSecondInNanoseconds,
                                  nullptr /* cookie */, true /* oneShot */);
      if (gTimerHandle == CHRE_TIMER_INVALID) {
        LOGE("Failed to set audio enabled timer");
      } else {
        success = true;
      }
    }

    if (!success) {
      sendTestResult(gHostEndpointId, success);
    }
  }
}

void handleAudioDataEvent(const chreAudioDataEvent * /* data */) {
  bool success = false;
  if (!gAudioEnabled) {
    LOGE("Received unexpected audio data");
  } else if (!chreTimerCancel(gTimerHandle)) {
    LOGE("Failed to cancel audio enable timer");
  } else if (!chreAudioConfigureSource(kAudioHandle, false /* enable */,
                                       0 /* bufferDuration */,
                                       0 /* deliveryInterval */)) {
    LOGE("Failed to stop audio source");
  } else {
    gAudioEnabled = false;
    gTimerHandle = chreTimerSet(5 * kOneSecondInNanoseconds,
                                nullptr /* cookie */, true /* oneShot */);
    if (gTimerHandle == CHRE_TIMER_INVALID) {
      LOGE("Failed to set audio disabled timer");
    } else {
      success = true;
    }
  }

  if (!success) {
    sendTestResult(gHostEndpointId, success);
  }
}

void handleTimer() {
  bool success = false;
  if (gTimerHandle == CHRE_TIMER_INVALID) {
    LOGE("Unexpected timer event");
  } else if (gAudioEnabled) {
    LOGE("Did not receive audio data in time");
  } else {
    success = true;
  }

  sendTestResult(gHostEndpointId, success);
}

}  // anonymous namespace

extern "C" void nanoappHandleEvent(uint32_t senderInstanceId,
                                   uint16_t eventType, const void *eventData) {
  if (!gTestRunning && eventType != CHRE_EVENT_MESSAGE_FROM_HOST) {
    return;
  }

  switch (eventType) {
    case CHRE_EVENT_MESSAGE_FROM_HOST:
      handleMessageFromHost(
          static_cast<const chreMessageFromHostData *>(eventData));
      break;

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

extern "C" bool nanoappStart(void) {
  return true;
}

extern "C" void nanoappEnd(void) {
  if (gAudioEnabled) {
    chreAudioConfigureSource(kAudioHandle, false /* enable */,
                             0 /* bufferDuration */, 0 /* deliveryInterval */);
  }
}

}  // namespace chre
