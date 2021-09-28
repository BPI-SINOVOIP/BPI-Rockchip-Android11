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

#include "chre/core/debug_dump_manager.h"

#include <cstring>

#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"

namespace chre {

void DebugDumpManager::trigger() {
  auto callback = [](uint16_t /*eventType*/, void * /*eventData*/) {
    DebugDumpManager &debugDumpManager =
        EventLoopManagerSingleton::get()->getDebugDumpManager();
    debugDumpManager.collectFrameworkDebugDumps();
    debugDumpManager.sendFrameworkDebugDumps();
  };

  // Collect CHRE framework debug dumps.
  EventLoopManagerSingleton::get()->deferCallback(
      SystemCallbackType::PerformDebugDump, nullptr /*data*/, callback);

  auto nappCallback = [](uint16_t /*eventType*/, void * /*eventData*/) {
    EventLoopManagerSingleton::get()
        ->getDebugDumpManager()
        .sendNanoappDebugDumps();
  };

  // Notify nanoapps to collect debug dumps.
  EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
      CHRE_EVENT_DEBUG_DUMP, nullptr /*eventData*/, nappCallback);
}

void DebugDumpManager::appendNanoappLog(const Nanoapp &nanoapp,
                                        const char *formatStr, va_list args) {
  uint32_t instanceId = nanoapp.getInstanceId();

  // Note this check isn't exact as it's possible that the nanoapp isn't
  // handling CHRE_EVENT_DEBUG_DUMP. This approximate check is used for its low
  // complexity as it doesn't introduce any real harms.
  if (!mCollectingNanoappDebugDumps) {
    LOGW("Nanoapp instance %" PRIu32
         " logging debug data while not in an active debug dump session",
         instanceId);
  } else if (formatStr != nullptr) {
    // Log nanoapp info the first time it adds debug data in this session.
    if (!mLastNanoappId.has_value() || mLastNanoappId.value() != instanceId) {
      mLastNanoappId = instanceId;
      mDebugDump.print("\n\n %s 0x%016" PRIx64 ":\n", nanoapp.getAppName(),
                       nanoapp.getAppId());
    }

    mDebugDump.print(formatStr, args);
  }
}

void DebugDumpManager::collectFrameworkDebugDumps() {
  auto *eventLoopManager = EventLoopManagerSingleton::get();
  eventLoopManager->getMemoryManager().logStateToBuffer(mDebugDump);
  eventLoopManager->getEventLoop().handleNanoappWakeupBuckets();
  eventLoopManager->getEventLoop().logStateToBuffer(mDebugDump);
  eventLoopManager->getSensorRequestManager().logStateToBuffer(mDebugDump);
#ifdef CHRE_GNSS_SUPPORT_ENABLED
  eventLoopManager->getGnssManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_GNSS_SUPPORT_ENABLED
#ifdef CHRE_WIFI_SUPPORT_ENABLED
  eventLoopManager->getWifiRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_WIFI_SUPPORT_ENABLED
#ifdef CHRE_WWAN_SUPPORT_ENABLED
  eventLoopManager->getWwanRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_WWAN_SUPPORT_ENABLED
#ifdef CHRE_AUDIO_SUPPORT_ENABLED
  eventLoopManager->getAudioRequestManager().logStateToBuffer(mDebugDump);
#endif  // CHRE_AUDIO_SUPPORT_ENABLED
  logSettingStateToBuffer(mDebugDump);
}

void DebugDumpManager::sendFrameworkDebugDumps() {
  for (size_t i = 0; i < mDebugDump.getBuffers().size(); i++) {
    const auto &buff = mDebugDump.getBuffers()[i];
    sendDebugDump(buff.get(), false /*complete*/);
  }

  // Clear out buffers before nanoapp debug dumps to reduce peak memory usage.
  mDebugDump.clear();

  // Mark the beginning of nanoapp debug dumps
  mDebugDump.print("\n\nNanoapp debug dumps:");
  mCollectingNanoappDebugDumps = true;
}

void DebugDumpManager::sendNanoappDebugDumps() {
  // Avoid buffer underflow when mDebugDump failed to allocate buffers.
  size_t numBuffers = mDebugDump.getBuffers().size();
  if (numBuffers > 0) {
    for (size_t i = 0; i < numBuffers - 1; i++) {
      const auto &buff = mDebugDump.getBuffers()[i];
      sendDebugDump(buff.get(), false /*complete*/);
    }
  }

  const char *debugStr =
      (numBuffers > 0) ? mDebugDump.getBuffers().back().get() : "";
  sendDebugDump(debugStr, true /*complete*/);

  // Clear current session debug dumps and release memory.
  mDebugDump.clear();
  mLastNanoappId.reset();
  mCollectingNanoappDebugDumps = false;
}

}  // namespace chre
