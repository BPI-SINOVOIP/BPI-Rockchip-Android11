/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "chre/core/nanoapp.h"

#include "chre/core/event_loop_manager.h"
#include "chre/platform/assert.h"
#include "chre/platform/fatal_error.h"
#include "chre/platform/log.h"
#include "chre/util/system/debug_dump.h"
#include "chre_api/chre/version.h"

#include <algorithm>

namespace chre {

constexpr size_t Nanoapp::kMaxSizeWakeupBuckets;

Nanoapp::Nanoapp() {
  // Push first bucket onto wakeup bucket queue
  cycleWakeupBuckets(1);
}

Nanoapp::~Nanoapp() {
  const size_t totalAllocatedBytes = getTotalAllocatedBytes();

  if (totalAllocatedBytes > 0) {
    // TODO: Consider asserting here
    LOGE("Nanoapp ID=0x%016" PRIx64 " still has %zu allocated bytes!",
         getAppId(), totalAllocatedBytes);
  }
}

bool Nanoapp::isRegisteredForBroadcastEvent(uint16_t eventType) const {
  return (mRegisteredEvents.find(eventType) != mRegisteredEvents.size());
}

bool Nanoapp::registerForBroadcastEvent(uint16_t eventId) {
  if (isRegisteredForBroadcastEvent(eventId)) {
    return false;
  }

  if (!mRegisteredEvents.push_back(eventId)) {
    FATAL_ERROR_OOM();
  }

  return true;
}

bool Nanoapp::unregisterForBroadcastEvent(uint16_t eventId) {
  size_t registeredEventIndex = mRegisteredEvents.find(eventId);
  if (registeredEventIndex == mRegisteredEvents.size()) {
    return false;
  }

  mRegisteredEvents.erase(registeredEventIndex);
  return true;
}

void Nanoapp::configureNanoappInfoEvents(bool enable) {
  if (enable) {
    registerForBroadcastEvent(CHRE_EVENT_NANOAPP_STARTED);
    registerForBroadcastEvent(CHRE_EVENT_NANOAPP_STOPPED);
  } else {
    unregisterForBroadcastEvent(CHRE_EVENT_NANOAPP_STARTED);
    unregisterForBroadcastEvent(CHRE_EVENT_NANOAPP_STOPPED);
  }
}

void Nanoapp::configureHostSleepEvents(bool enable) {
  if (enable) {
    registerForBroadcastEvent(CHRE_EVENT_HOST_AWAKE);
    registerForBroadcastEvent(CHRE_EVENT_HOST_ASLEEP);
  } else {
    unregisterForBroadcastEvent(CHRE_EVENT_HOST_AWAKE);
    unregisterForBroadcastEvent(CHRE_EVENT_HOST_ASLEEP);
  }
}

void Nanoapp::configureDebugDumpEvent(bool enable) {
  if (enable) {
    registerForBroadcastEvent(CHRE_EVENT_DEBUG_DUMP);
  } else {
    unregisterForBroadcastEvent(CHRE_EVENT_DEBUG_DUMP);
  }
}

Event *Nanoapp::processNextEvent() {
  Event *event = mEventQueue.pop();

  CHRE_ASSERT_LOG(event != nullptr, "Tried delivering event, but queue empty");
  if (event != nullptr) {
    handleEvent(event->senderInstanceId, event->eventType, event->eventData);
  }

  return event;
}

void Nanoapp::blameHostWakeup() {
  if (mWakeupBuckets.back() < UINT16_MAX) ++mWakeupBuckets.back();
}

void Nanoapp::cycleWakeupBuckets(size_t numBuckets) {
  numBuckets = std::min(numBuckets, kMaxSizeWakeupBuckets);
  for (size_t i = 0; i < numBuckets; ++i) {
    if (mWakeupBuckets.full()) {
      mWakeupBuckets.erase(0);
    }
    mWakeupBuckets.push_back(0);
  }
}

void Nanoapp::logStateToBuffer(DebugDumpWrapper &debugDump) const {
  debugDump.print(" Id=%" PRIu32 " 0x%016" PRIx64 " ", getInstanceId(),
                  getAppId());
  PlatformNanoapp::logStateToBuffer(debugDump);
  debugDump.print(" v%" PRIu32 ".%" PRIu32 ".%" PRIu32 " tgtAPI=%" PRIu32
                  ".%" PRIu32 " curAlloc=%zu peakAlloc=%zu",
                  CHRE_EXTRACT_MAJOR_VERSION(getAppVersion()),
                  CHRE_EXTRACT_MINOR_VERSION(getAppVersion()),
                  CHRE_EXTRACT_PATCH_VERSION(getAppVersion()),
                  CHRE_EXTRACT_MAJOR_VERSION(getTargetApiVersion()),
                  CHRE_EXTRACT_MINOR_VERSION(getTargetApiVersion()),
                  getTotalAllocatedBytes(), getPeakAllocatedBytes());
  debugDump.print(" hostWakeups=[ cur->");
  // Get buckets latest -> earliest except last one
  for (size_t i = mWakeupBuckets.size() - 1; i > 0; --i) {
    debugDump.print("%" PRIu16 ", ", mWakeupBuckets[i]);
  }
  // Earliest bucket gets no comma
  debugDump.print("%" PRIu16 " ]\n", mWakeupBuckets.front());
}

}  // namespace chre
