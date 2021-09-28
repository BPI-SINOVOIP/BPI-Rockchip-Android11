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

#include <cstdio>

#include "chre/core/event_loop_manager.h"
#include "chre/platform/system_time.h"
#include "chre/util/nested_data_ptr.h"

#ifdef CHRE_USE_TOKENIZED_LOGGING
namespace {

static constexpr size_t kLogBufferSize = 60;

}  // anonymous namespace

// The callback function that must be defined to handle an encoded
// tokenizer message.
void pw_TokenizerHandleEncodedMessageWithPayload(void *userPayload,
                                                 const uint8_t encodedMsg[],
                                                 size_t encodedMsgSize) {
  // The header encoding here follows the message definition in the
  // host_messages.fbs flatbuffers file.
  uint8_t logBuffer[kLogBufferSize];
  constexpr size_t kLogMessageHeaderSizeBytes = 1 + sizeof(uint64_t);
  uint8_t *pLogBuffer = &logBuffer[0];

  chre::NestedDataPtr<uint8_t> nestedLevel;
  nestedLevel.dataPtr = userPayload;
  *pLogBuffer = nestedLevel.data;
  ++pLogBuffer;

  uint64_t timestampNanos =
      chre::SystemTime::getMonotonicTime().toRawNanoseconds();
  memcpy(pLogBuffer, &timestampNanos, sizeof(uint64_t));
  pLogBuffer += sizeof(uint64_t);
  memcpy(pLogBuffer, encodedMsg, encodedMsgSize);

  // TODO (b/148873804): buffer log messages generated while the AP is asleep
  auto &hostCommsMgr =
      chre::EventLoopManagerSingleton::get()->getHostCommsManager();
  hostCommsMgr.sendLogMessage(reinterpret_cast<const char *>(logBuffer),
                              encodedMsgSize + kLogMessageHeaderSizeBytes);
}
#endif
