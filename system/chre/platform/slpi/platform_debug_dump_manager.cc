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

#include "chre/platform/platform_debug_dump_manager.h"

#include "chre/core/event_loop_manager.h"
#include "chre/platform/log.h"
#include "chre/target_platform/host_link_base.h"
#include "chre/target_platform/platform_debug_dump_manager_base.h"

#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
#include "ash/debug.h"
#else  // CHRE_ENABLE_ASH_DEBUG_DUMP
#include <cstring>
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP

namespace chre {
namespace {

#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
void onDebugDumpTriggered(void * /*cookie*/, uint32_t handle) {
  auto &debugDumpManager =
      EventLoopManagerSingleton::get()->getDebugDumpManager();

  debugDumpManager.setHandle(handle);
  debugDumpManager.trigger();
}

void debugDumpReadyCb(void * /*cookie*/, const char *debugStr,
                      size_t debugStrSize, bool complete) {
  EventLoopManagerSingleton::get()->getDebugDumpManager().sendDebugDumpResult(
      debugStr, debugStrSize, complete);
}
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP

}  // namespace

void PlatformDebugDumpManager::sendDebugDump(const char *debugStr,
                                             bool complete) {
  // DDM is guaranteed to call complete=true at the end of a debug dump session.
  // However, sendDebugDumpResult may not get called with complete=true, for
  // example when ASH times out. Therefore, mDataCount has to be reset here
  // instead of in sendDebugDumpResult(), to properly start the next debug dump
  // session.
  if (mComplete) {
    mDataCount = 0;
  }
  mComplete = complete;

#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
  ashCommitDebugDump(mHandle, debugStr, complete);
#else   // CHRE_ENABLE_ASH_DEBUG_DUMP
  sendDebugDumpResult(debugStr, strlen(debugStr), complete);
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP
}

PlatformDebugDumpManagerBase::PlatformDebugDumpManagerBase() {
#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
  if (!ashRegisterDebugDumpCallback("CHRE", onDebugDumpTriggered,
                                    nullptr /* cookie */)) {
    LOGE("Failed to register ASH debug dump callback");
  }
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP
}

PlatformDebugDumpManagerBase::~PlatformDebugDumpManagerBase() {
#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
  ashUnregisterDebugDumpCallback(onDebugDumpTriggered);
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP
}

bool PlatformDebugDumpManagerBase::onDebugDumpRequested(uint16_t hostClientId) {
  mHostClientId = hostClientId;

#ifdef CHRE_ENABLE_ASH_DEBUG_DUMP
  return ashTriggerDebugDump(debugDumpReadyCb, nullptr /*cookie*/);
#else   // CHRE_ENABLE_ASH_DEBUG_DUMP
  EventLoopManagerSingleton::get()->getDebugDumpManager().trigger();
  return true;
#endif  // CHRE_ENABLE_ASH_DEBUG_DUMP
}

void PlatformDebugDumpManagerBase::sendDebugDumpResult(const char *debugStr,
                                                       size_t debugStrSize,
                                                       bool complete) {
  if (debugStrSize > 0) {
    mDataCount++;
  }
  sendDebugDumpResultToHost(mHostClientId, debugStr, debugStrSize, complete,
                            mDataCount);
}

}  // namespace chre
