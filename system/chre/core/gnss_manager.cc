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

#include "chre/core/gnss_manager.h"

#include "chre/core/event_loop_manager.h"
#include "chre/core/settings.h"
#include "chre/platform/assert.h"
#include "chre/platform/fatal_error.h"
#include "chre/util/system/debug_dump.h"

namespace chre {

namespace {

bool getCallbackType(uint16_t eventType, SystemCallbackType *callbackType) {
  bool success = true;
  switch (eventType) {
    case CHRE_EVENT_GNSS_LOCATION: {
      *callbackType = SystemCallbackType::GnssLocationReportEvent;
      break;
    }
    case CHRE_EVENT_GNSS_DATA: {
      *callbackType = SystemCallbackType::GnssMeasurementReportEvent;
      break;
    }
    default: {
      LOGE("Unknown event type %" PRIu16, eventType);
      success = false;
    }
  }

  return success;
}

bool getReportEventType(SystemCallbackType callbackType, uint16_t *eventType) {
  bool success = true;
  switch (callbackType) {
    case SystemCallbackType::GnssLocationReportEvent: {
      *eventType = CHRE_EVENT_GNSS_LOCATION;
      break;
    }
    case SystemCallbackType::GnssMeasurementReportEvent: {
      *eventType = CHRE_EVENT_GNSS_DATA;
      break;
    }
    default: {
      LOGE("Unknown callback type %" PRIu16,
           static_cast<uint16_t>(callbackType));
      success = false;
    }
  }

  return success;
}

}  // anonymous namespace

GnssManager::GnssManager()
    : mLocationSession(CHRE_EVENT_GNSS_LOCATION),
      mMeasurementSession(CHRE_EVENT_GNSS_DATA) {}

void GnssManager::init() {
  mPlatformGnss.init();
}

uint32_t GnssManager::getCapabilities() {
  return mPlatformGnss.getCapabilities();
}

void GnssManager::onSettingChanged(Setting setting, SettingState state) {
  mLocationSession.onSettingChanged(setting, state);
  mMeasurementSession.onSettingChanged(setting, state);
}

void GnssManager::logStateToBuffer(DebugDumpWrapper &debugDump) const {
  debugDump.print("\nGNSS:");
  mLocationSession.logStateToBuffer(debugDump);
  mMeasurementSession.logStateToBuffer(debugDump);
}

GnssSession::GnssSession(uint16_t reportEventType)
    : mReportEventType(reportEventType) {
  switch (mReportEventType) {
    case CHRE_EVENT_GNSS_LOCATION:
      mStartRequestType = CHRE_GNSS_REQUEST_TYPE_LOCATION_SESSION_START;
      mStopRequestType = CHRE_GNSS_REQUEST_TYPE_LOCATION_SESSION_STOP;
      mName = "Location";
      break;

    case CHRE_EVENT_GNSS_DATA:
      mStartRequestType = CHRE_GNSS_REQUEST_TYPE_MEASUREMENT_SESSION_START;
      mStopRequestType = CHRE_GNSS_REQUEST_TYPE_MEASUREMENT_SESSION_STOP;
      mName = "Measurement";
      break;

    default:
      CHRE_ASSERT_LOG(false, "Unsupported eventType %" PRIu16, reportEventType);
  }

  if (!mRequests.reserve(1)) {
    FATAL_ERROR_OOM();
  }
}

bool GnssSession::addRequest(Nanoapp *nanoapp, Milliseconds minInterval,
                             Milliseconds minTimeToNext, const void *cookie) {
  CHRE_ASSERT(nanoapp);
  return configure(nanoapp, true /* enable */, minInterval, minTimeToNext,
                   cookie);
}

bool GnssSession::removeRequest(Nanoapp *nanoapp, const void *cookie) {
  CHRE_ASSERT(nanoapp);
  return configure(nanoapp, false /* enable */, Milliseconds(UINT64_MAX),
                   Milliseconds(UINT64_MAX), cookie);
}

void GnssSession::handleStatusChange(bool enabled, uint8_t errorCode) {
  struct CallbackState {
    bool enabled;
    uint8_t errorCode;
    GnssSession *session;
  };

  auto *cbState = memoryAlloc<CallbackState>();
  if (cbState == nullptr) {
    LOG_OOM();
  } else {
    cbState->enabled = enabled;
    cbState->errorCode = errorCode;
    cbState->session = this;

    auto callback = [](uint16_t /* eventType */, void *eventData) {
      auto *state = static_cast<CallbackState *>(eventData);
      state->session->handleStatusChangeSync(state->enabled, state->errorCode);
      memoryFree(state);
    };

    EventLoopManagerSingleton::get()->deferCallback(
        SystemCallbackType::GnssSessionStatusChange, cbState, callback);
  }
}

void GnssSession::handleReportEvent(void *event) {
  auto callback = [](uint16_t type, void *eventData) {
    uint16_t reportEventType;
    if (!getReportEventType(static_cast<SystemCallbackType>(type),
                            &reportEventType) ||
        (getSettingState(Setting::LOCATION) == SettingState::DISABLED)) {
      freeReportEventCallback(reportEventType, eventData);
    } else {
      EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
          reportEventType, eventData, freeReportEventCallback);
    }
  };

  SystemCallbackType type;
  if (!getCallbackType(mReportEventType, &type)) {
    freeReportEventCallback(mReportEventType, event);
  } else {
    EventLoopManagerSingleton::get()->deferCallback(type, event, callback);
  }
}

void GnssSession::onSettingChanged(Setting setting, SettingState state) {
  if (setting == Setting::LOCATION) {
    if (!mStateTransitions.empty()) {
      // A request is in progress, so we wait until the async response arrives
      // to handle the state change.
      mSettingChangePending = true;
    } else {
      handleLocationSettingChange(state);
      mSettingChangePending = false;
    }
  }
}

void GnssSession::handleLocationSettingChange(SettingState state) {
  bool chreDisable = ((state == SettingState::DISABLED) && mPlatformEnabled);
  bool chreEnable = ((state == SettingState::ENABLED) && !mPlatformEnabled &&
                     !mRequests.empty());

  if (chreEnable || chreDisable) {
    if (controlPlatform(chreEnable, mCurrentInterval,
                        Milliseconds(0) /* minTimeToNext */)) {
      LOGD("Configured GNSS %s: setting state %" PRIu8, mName,
           static_cast<uint8_t>(state));
      addSessionRequestLog(CHRE_INSTANCE_ID, mCurrentInterval, chreEnable);
      mInternalRequestPending = true;
    } else {
      LOGE("Failed to configure GNSS %s: setting state %" PRIu8, mName,
           static_cast<uint8_t>(state));
    }
  }
}

void GnssSession::logStateToBuffer(DebugDumpWrapper &debugDump) const {
  // TODO: have all interval values print as INVALID if they are the max
  // unsigned value
  debugDump.print("\n %s: Curr int(ms)=%" PRIu64 "\n", mName,
                  mCurrentInterval.getMilliseconds());
  debugDump.print("  Requests:\n");
  for (const auto &request : mRequests) {
    debugDump.print("   minInt(ms)=%" PRIu64 " nappId=%" PRIu32 "\n",
                    request.minInterval.getMilliseconds(),
                    request.nanoappInstanceId);
  }

  if (!mStateTransitions.empty()) {
    debugDump.print("  Transition queue:\n");
    for (const auto &transition : mStateTransitions) {
      debugDump.print("   minInt(ms)=%" PRIu64 " enable=%d nappId=%" PRIu32
                      "\n",
                      transition.minInterval.getMilliseconds(),
                      transition.enable, transition.nanoappInstanceId);
    }
  }

  debugDump.print("  Last %zu session requests:\n", mSessionRequestLogs.size());
  static_assert(kNumSessionRequestLogs <= INT8_MAX,
                "kNumSessionRequestLogs must be less than INT8_MAX.");
  for (int8_t i = static_cast<int8_t>(mSessionRequestLogs.size()) - 1; i >= 0;
       i--) {
    const auto &log = mSessionRequestLogs[static_cast<size_t>(i)];
    debugDump.print("   ts=%" PRIu64 " nappId=%" PRIu32 " %s",
                    log.timestamp.toRawNanoseconds(), log.instanceId,
                    log.start ? "start" : "stop\n");
    if (log.start) {
      debugDump.print(" int(ms)=%" PRIu64 "\n", log.interval.getMilliseconds());
    }
  }
}

bool GnssSession::configure(Nanoapp *nanoapp, bool enable,
                            Milliseconds minInterval,
                            Milliseconds minTimeToNext, const void *cookie) {
  bool success = false;
  uint32_t instanceId = nanoapp->getInstanceId();
  size_t requestIndex = 0;
  bool hasRequest = nanoappHasRequest(instanceId, &requestIndex);
  if (!mStateTransitions.empty()) {
    success = addRequestToQueue(instanceId, enable, minInterval, cookie);
  } else if (stateTransitionIsRequired(enable, minInterval, hasRequest,
                                       requestIndex)) {
    if (enable &&
        getSettingState(Setting::LOCATION) == SettingState::DISABLED) {
      // Treat as success but post async failure per API.
      success = postAsyncResultEvent(instanceId, false /* success */, enable,
                                     minInterval, CHRE_ERROR_FUNCTION_DISABLED,
                                     cookie);
    } else if (addRequestToQueue(instanceId, enable, minInterval, cookie)) {
      success = controlPlatform(enable, minInterval, minTimeToNext);
      if (!success) {
        mStateTransitions.pop_back();
        LOGE("Failed to request a GNSS session for nanoapp instance %" PRIu32
             " enable %d",
             instanceId, enable);
      }
    }
  } else {
    success = postAsyncResultEvent(instanceId, true /* success */, enable,
                                   minInterval, CHRE_ERROR_NONE, cookie);
  }

  if (success) {
    addSessionRequestLog(nanoapp->getInstanceId(), minInterval, enable);
  }

  return success;
}

bool GnssSession::nanoappHasRequest(uint32_t instanceId,
                                    size_t *requestIndex) const {
  bool hasRequest = false;
  for (size_t i = 0; i < mRequests.size(); i++) {
    if (mRequests[i].nanoappInstanceId == instanceId) {
      hasRequest = true;
      if (requestIndex != nullptr) {
        *requestIndex = i;
      }

      break;
    }
  }

  return hasRequest;
}

bool GnssSession::addRequestToQueue(uint32_t instanceId, bool enable,
                                    Milliseconds minInterval,
                                    const void *cookie) {
  StateTransition stateTransition;
  stateTransition.nanoappInstanceId = instanceId;
  stateTransition.enable = enable;
  stateTransition.minInterval = minInterval;
  stateTransition.cookie = cookie;

  bool success = mStateTransitions.push(stateTransition);
  if (!success) {
    LOGW("Too many session state transitions");
  }

  return success;
}

bool GnssSession::isEnabled() const {
  return !mRequests.empty();
}

bool GnssSession::stateTransitionIsRequired(bool requestedState,
                                            Milliseconds minInterval,
                                            bool nanoappHasRequest,
                                            size_t requestIndex) const {
  bool requestToEnable = (requestedState && !isEnabled());
  bool requestToIncreaseRate =
      (requestedState && isEnabled() && minInterval < mCurrentInterval);
  bool requestToDisable =
      (!requestedState && nanoappHasRequest && mRequests.size() == 1);

  // An effective rate decrease for the session can only occur if the nanoapp
  // has an existing request.
  bool requestToDecreaseRate = false;
  if (nanoappHasRequest) {
    // The nanoapp has an existing request. Check that the request does not
    // result in a rate decrease by checking if no other nanoapps have the
    // same request, the nanoapp's existing request is not equal to the current
    // requested interval and the new request is slower than the current
    // requested rate.
    size_t requestCount = 0;
    const auto &currentRequest = mRequests[requestIndex];
    for (size_t i = 0; i < mRequests.size(); i++) {
      const Request &request = mRequests[i];
      if (i != requestIndex &&
          request.minInterval == currentRequest.minInterval) {
        requestCount++;
      }
    }

    requestToDecreaseRate =
        (minInterval > mCurrentInterval &&
         currentRequest.minInterval == mCurrentInterval && requestCount == 0);
  }

  return (requestToEnable || requestToDisable || requestToIncreaseRate ||
          requestToDecreaseRate);
}

bool GnssSession::updateRequests(bool enable, Milliseconds minInterval,
                                 uint32_t instanceId) {
  bool success = true;
  Nanoapp *nanoapp =
      EventLoopManagerSingleton::get()->getEventLoop().findNanoappByInstanceId(
          instanceId);
  if (nanoapp == nullptr) {
    LOGW("Failed to update GNSS session request list for non-existent nanoapp");
  } else {
    size_t requestIndex;
    bool hasExistingRequest = nanoappHasRequest(instanceId, &requestIndex);
    if (enable) {
      if (hasExistingRequest) {
        // If the nanoapp has an open request ensure that the minInterval is
        // kept up to date.
        mRequests[requestIndex].minInterval = minInterval;
      } else {
        // The GNSS session was successfully enabled for this nanoapp and
        // there is no existing request. Add it to the list of GNSS session
        // nanoapps.
        Request request;
        request.nanoappInstanceId = instanceId;
        request.minInterval = minInterval;
        success = mRequests.push_back(request);
        if (!success) {
          LOG_OOM();
        } else {
          nanoapp->registerForBroadcastEvent(mReportEventType);
        }
      }
    } else if (hasExistingRequest) {
      // The session was successfully disabled for a previously enabled
      // nanoapp. Remove it from the list of requests.
      mRequests.erase(requestIndex);
      nanoapp->unregisterForBroadcastEvent(mReportEventType);
    }  // else disabling an inactive request, treat as success per CHRE API
  }

  return success;
}

bool GnssSession::postAsyncResultEvent(uint32_t instanceId, bool success,
                                       bool enable, Milliseconds minInterval,
                                       uint8_t errorCode, const void *cookie) {
  bool eventPosted = false;
  if (!success || updateRequests(enable, minInterval, instanceId)) {
    chreAsyncResult *event = memoryAlloc<chreAsyncResult>();
    if (event == nullptr) {
      LOG_OOM();
    } else {
      event->requestType = enable ? mStartRequestType : mStopRequestType;
      event->success = success;
      event->errorCode = errorCode;
      event->reserved = 0;
      event->cookie = cookie;

      eventPosted =
          EventLoopManagerSingleton::get()->getEventLoop().postEventOrDie(
              CHRE_EVENT_GNSS_ASYNC_RESULT, event, freeEventDataCallback,
              instanceId);

      if (!eventPosted) {
        memoryFree(event);
      }
    }
  }

  return eventPosted;
}

void GnssSession::postAsyncResultEventFatal(uint32_t instanceId, bool success,
                                            bool enable,
                                            Milliseconds minInterval,
                                            uint8_t errorCode,
                                            const void *cookie) {
  if (!postAsyncResultEvent(instanceId, success, enable, minInterval, errorCode,
                            cookie)) {
    FATAL_ERROR("Failed to send GNSS session request async result event");
  }
}

void GnssSession::handleStatusChangeSync(bool enabled, uint8_t errorCode) {
  bool success = (errorCode == CHRE_ERROR_NONE);

  CHRE_ASSERT_LOG(!mStateTransitions.empty() || mInternalRequestPending,
                  "handleStatusChangeSync called with no transitions");
  if (mInternalRequestPending) {
    // Silently handle internal requests from CHRE, since they are not pushed
    // to the mStateTransitions queue.
    mInternalRequestPending = false;
  } else if (!mStateTransitions.empty()) {
    const auto &stateTransition = mStateTransitions.front();

    if (success) {
      mCurrentInterval = stateTransition.minInterval;
    }

    success &= (stateTransition.enable == enabled);
    postAsyncResultEventFatal(
        stateTransition.nanoappInstanceId, success, stateTransition.enable,
        stateTransition.minInterval, errorCode, stateTransition.cookie);
    mStateTransitions.pop();
  }

  // If a previous setting change event is pending process, do that first.
  if (mSettingChangePending) {
    handleLocationSettingChange(getSettingState(Setting::LOCATION));
    mSettingChangePending = false;
  } else {
    // Dispatch pending state transition until first one succeeds
    dispatchQueuedStateTransitions();
  }
}

void GnssSession::freeReportEventCallback(uint16_t eventType, void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_GNSS_LOCATION:
      EventLoopManagerSingleton::get()
          ->getGnssManager()
          .mPlatformGnss.releaseLocationEvent(
              static_cast<chreGnssLocationEvent *>(eventData));
      break;

    case CHRE_EVENT_GNSS_DATA:
      EventLoopManagerSingleton::get()
          ->getGnssManager()
          .mPlatformGnss.releaseMeasurementDataEvent(
              static_cast<chreGnssDataEvent *>(eventData));
      break;

    default:
      CHRE_ASSERT_LOG(false, "Unhandled event type %" PRIu16, eventType);
  }
}

bool GnssSession::controlPlatform(bool enable, Milliseconds minInterval,
                                  Milliseconds /* minTimeToNext */) {
  bool success = false;

  switch (mReportEventType) {
    case CHRE_EVENT_GNSS_LOCATION:
      // TODO: Provide support for min time to next report. It is currently sent
      // to the platform as zero.
      success = EventLoopManagerSingleton::get()
                    ->getGnssManager()
                    .mPlatformGnss.controlLocationSession(enable, minInterval,
                                                          Milliseconds(0));
      break;

    case CHRE_EVENT_GNSS_DATA:
      success =
          EventLoopManagerSingleton::get()
              ->getGnssManager()
              .mPlatformGnss.controlMeasurementSession(enable, minInterval);
      break;

    default:
      CHRE_ASSERT_LOG(false, "Unhandled event type %" PRIu16, mReportEventType);
  }

  if (success) {
    mPlatformEnabled = enable;
  }

  return success;
}

void GnssSession::addSessionRequestLog(uint32_t nanoappInstanceId,
                                       Milliseconds interval, bool start) {
  mSessionRequestLogs.kick_push(SessionRequestLog(
      SystemTime::getMonotonicTime(), nanoappInstanceId, interval, start));
}

void GnssSession::dispatchQueuedStateTransitions() {
  while (!mStateTransitions.empty()) {
    const auto &stateTransition = mStateTransitions.front();

    size_t requestIndex;
    bool hasRequest =
        nanoappHasRequest(stateTransition.nanoappInstanceId, &requestIndex);

    if (stateTransitionIsRequired(stateTransition.enable,
                                  stateTransition.minInterval, hasRequest,
                                  requestIndex)) {
      if (getSettingState(Setting::LOCATION) == SettingState::DISABLED) {
        postAsyncResultEventFatal(
            stateTransition.nanoappInstanceId, false /* success */,
            stateTransition.enable, stateTransition.minInterval,
            CHRE_ERROR_FUNCTION_DISABLED, stateTransition.cookie);
        mStateTransitions.pop();
      } else if (controlPlatform(stateTransition.enable,
                                 stateTransition.minInterval,
                                 Milliseconds(0))) {
        break;
      } else {
        LOGE("Failed to enable a GNSS session for nanoapp instance %" PRIu32,
             stateTransition.nanoappInstanceId);
        postAsyncResultEventFatal(stateTransition.nanoappInstanceId,
                                  false /* success */, stateTransition.enable,
                                  stateTransition.minInterval, CHRE_ERROR,
                                  stateTransition.cookie);
        mStateTransitions.pop();
      }
    } else {
      postAsyncResultEventFatal(stateTransition.nanoappInstanceId,
                                true /* success */, stateTransition.enable,
                                stateTransition.minInterval, CHRE_ERROR_NONE,
                                stateTransition.cookie);
      mStateTransitions.pop();
    }
  }
}

}  // namespace chre
