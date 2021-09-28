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

//#define LOG_NDEBUG 0
#define LOG_TAG "ECOSession"
//#define DEBUG_ECO_SESSION
#include "eco/ECOSession.h"

#include <binder/BinderService.h>
#include <cutils/atomic.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <utils/Log.h>

#include <algorithm>
#include <climits>
#include <cstring>
#include <ctime>
#include <string>

#include "eco/ECODataKey.h"
#include "eco/ECODebug.h"

namespace android {
namespace media {
namespace eco {

using android::binder::Status;
using android::sp;

#define RETURN_IF_ERROR(expr)         \
    {                                 \
        status_t _errorCode = (expr); \
        if (_errorCode != true) {     \
            return _errorCode;        \
        }                             \
    }

// static
sp<ECOSession> ECOSession::createECOSession(int32_t width, int32_t height, bool isCameraRecording) {
    // Only support up to 720P.
    // TODO: Support the same resolution as in EAF.
    if (width <= 0 || height <= 0 || width > 5120 || height > 5120 ||
        width > 1280 * 720 / height) {
        ECOLOGE("Failed to create ECOSession with w: %d, h: %d, isCameraRecording: %d", width,
                height, isCameraRecording);
        return nullptr;
    }
    return new ECOSession(width, height, isCameraRecording);
}

ECOSession::ECOSession(int32_t width, int32_t height, bool isCameraRecording)
      : BnECOSession(),
        mStopThread(false),
        mLastReportedQp(0),
        mListener(nullptr),
        mProvider(nullptr),
        mWidth(width),
        mHeight(height),
        mIsCameraRecording(isCameraRecording) {
    ECOLOGI("ECOSession created with w: %d, h: %d, isCameraRecording: %d", mWidth, mHeight,
            mIsCameraRecording);
    mThread = std::thread(startThread, this);

    // Read the debug properies.
    mLogStats = property_get_bool(kDebugLogStats, false);
    mLogStatsEntries = mLogStats ? property_get_int32(kDebugLogStatsSize, 0) : 0;

    mLogInfo = property_get_bool(kDebugLogStats, false);
    mLogInfoEntries = mLogInfo ? property_get_int32(kDebugLogInfosSize, 0) : 0;

    ECOLOGI("ECOSession debug settings: logStats: %s, entries: %d, logInfo: %s entries: %d",
            mLogStats ? "true" : "false", mLogStatsEntries, mLogInfo ? "true" : "false",
            mLogInfoEntries);
}

ECOSession::~ECOSession() {
    mStopThread = true;

    mWorkerWaitCV.notify_all();
    if (mThread.joinable()) {
        ECOLOGD("ECOSession: join the thread");
        mThread.join();
    }
    ECOLOGI("ECOSession destroyed with w: %d, h: %d, isCameraRecording: %d", mWidth, mHeight,
            mIsCameraRecording);
}

// static
void ECOSession::startThread(ECOSession* session) {
    session->run();
}

void ECOSession::run() {
    ECOLOGD("ECOSession: starting main thread");

    while (!mStopThread) {
        std::unique_lock<std::mutex> runLock(mStatsQueueLock);

        mWorkerWaitCV.wait(runLock, [this] {
            return mStopThread == true || !mStatsQueue.empty() || mNewListenerAdded;
        });

        if (mStopThread) return;

        std::scoped_lock<std::mutex> lock(mSessionLock);
        if (mNewListenerAdded) {
            // Check if there is any session info available.
            ECOData sessionInfo = generateLatestSessionInfoEcoData();
            if (!sessionInfo.isEmpty()) {
                Status status = mListener->onNewInfo(sessionInfo);
                if (!status.isOk()) {
                    ECOLOGE("%s: Failed to publish info: %s due to binder error", __FUNCTION__,
                            sessionInfo.debugString().c_str());
                    // Remove the listener. The lock has been acquired outside this function.
                    mListener = nullptr;
                }
            }
            mNewListenerAdded = false;
        }

        if (!mStatsQueue.empty()) {
            ECOData stats = mStatsQueue.front();
            mStatsQueue.pop_front();
            processStats(stats);  // TODO: Handle the error from processStats
        }
    }

    ECOLOGD("ECOSession: exiting main thread");
}

bool ECOSession::processStats(const ECOData& stats) {
    ECOLOGV("%s: receive stats: %s", __FUNCTION__, stats.debugString().c_str());

    if (stats.getDataType() != ECOData::DATA_TYPE_STATS) {
        ECOLOGE("Invalid stats. ECOData with type: %s", stats.getDataTypeString().c_str());
        return false;
    }

    // Get the type of the stats.
    std::string statsType;
    if (stats.findString(KEY_STATS_TYPE, &statsType) != ECODataStatus::OK) {
        ECOLOGE("Invalid stats ECOData without statsType");
        return false;
    }

    if (statsType.compare(VALUE_STATS_TYPE_SESSION) == 0) {
        processSessionStats(stats);
    } else if (statsType.compare(VALUE_STATS_TYPE_FRAME) == 0) {
        processFrameStats(stats);
    } else {
        ECOLOGE("processStats:: Failed to process stats as ECOData contains unknown stats type");
        return false;
    }

    return true;
}

void ECOSession::processSessionStats(const ECOData& stats) {
    ECOLOGV("processSessionStats");

    ECOData info(ECOData::DATA_TYPE_INFO, systemTime(SYSTEM_TIME_BOOTTIME));
    info.setString(KEY_INFO_TYPE, VALUE_INFO_TYPE_SESSION);

    ECODataKeyValueIterator iter(stats);
    while (iter.hasNext()) {
        ECOData::ECODataKeyValuePair entry = iter.next();
        const std::string& key = entry.first;
        const ECOData::ECODataValueType value = entry.second;
        ECOLOGV("Processing key: %s", key.c_str());
        if (!key.compare(KEY_STATS_TYPE)) {
            // Skip the key KEY_STATS_TYPE as that has been parsed already.
            continue;
        } else if (!key.compare(ENCODER_TYPE)) {
            mCodecType = std::get<int32_t>(value);
            ECOLOGV("codec type is %d", mCodecType);
        } else if (!key.compare(ENCODER_PROFILE)) {
            mCodecProfile = std::get<int32_t>(value);
            ECOLOGV("codec profile is %d", mCodecProfile);
        } else if (!key.compare(ENCODER_LEVEL)) {
            mCodecLevel = std::get<int32_t>(value);
            ECOLOGV("codec level is %d", mCodecLevel);
        } else if (!key.compare(ENCODER_TARGET_BITRATE_BPS)) {
            mTargetBitrateBps = std::get<int32_t>(value);
            ECOLOGV("codec target bitrate is %d", mTargetBitrateBps);
        } else if (!key.compare(ENCODER_KFI_FRAMES)) {
            mKeyFrameIntervalFrames = std::get<int32_t>(value);
            ECOLOGV("codec kfi is %d", mKeyFrameIntervalFrames);
        } else if (!key.compare(ENCODER_FRAMERATE_FPS)) {
            mFramerateFps = std::get<float>(value);
            ECOLOGV("codec framerate is %f", mFramerateFps);
        } else if (!key.compare(ENCODER_INPUT_WIDTH)) {
            int32_t width = std::get<int32_t>(value);
            if (width != mWidth) {
                ECOLOGW("Codec width: %d, expected: %d", width, mWidth);
            }
            ECOLOGV("codec input width is %d", width);
        } else if (!key.compare(ENCODER_INPUT_HEIGHT)) {
            int32_t height = std::get<int32_t>(value);
            if (height != mHeight) {
                ECOLOGW("Codec height: %d, expected: %d", height, mHeight);
            }
            ECOLOGV("codec input height is %d", height);
        } else if (!key.compare(ENCODER_OUTPUT_WIDTH)) {
            mOutputWidth = std::get<int32_t>(value);
            if (mOutputWidth != mWidth) {
                ECOLOGW("Codec output width: %d, expected: %d", mOutputWidth, mWidth);
            }
            ECOLOGV("codec output width is %d", mOutputWidth);
        } else if (!key.compare(ENCODER_OUTPUT_HEIGHT)) {
            mOutputHeight = std::get<int32_t>(value);
            if (mOutputHeight != mHeight) {
                ECOLOGW("Codec output height: %d, expected: %d", mOutputHeight, mHeight);
            }
            ECOLOGV("codec output height is %d", mOutputHeight);
        } else {
            ECOLOGW("Unknown session stats key %s from provider.", key.c_str());
            continue;
        }
        info.set(key, value);
    }

    if (mListener != nullptr) {
        Status status = mListener->onNewInfo(info);
        if (!status.isOk()) {
            ECOLOGE("%s: Failed to publish info: %s due to binder error", __FUNCTION__,
                    info.debugString().c_str());
            // Remove the listener. The lock has been acquired outside this function.
            mListener = nullptr;
        }
    }
}

ECOData ECOSession::generateLatestSessionInfoEcoData() {
    bool hasInfo = false;

    ECOData info(ECOData::DATA_TYPE_INFO, systemTime(SYSTEM_TIME_BOOTTIME));

    if (mOutputWidth != -1) {
        info.setInt32(ENCODER_OUTPUT_WIDTH, mOutputWidth);
        hasInfo = true;
    }

    if (mOutputHeight != -1) {
        info.setInt32(ENCODER_OUTPUT_HEIGHT, mOutputHeight);
        hasInfo = true;
    }

    if (mCodecType != -1) {
        info.setInt32(ENCODER_TYPE, mCodecType);
        hasInfo = true;
    }

    if (mCodecProfile != -1) {
        info.setInt32(ENCODER_PROFILE, mCodecProfile);
        hasInfo = true;
    }

    if (mCodecLevel != -1) {
        info.setInt32(ENCODER_LEVEL, mCodecLevel);
        hasInfo = true;
    }

    if (mTargetBitrateBps != -1) {
        info.setInt32(ENCODER_TARGET_BITRATE_BPS, mTargetBitrateBps);
        hasInfo = true;
    }

    if (mKeyFrameIntervalFrames != -1) {
        info.setInt32(ENCODER_KFI_FRAMES, mKeyFrameIntervalFrames);
        hasInfo = true;
    }

    if (mFramerateFps > 0) {
        info.setFloat(ENCODER_FRAMERATE_FPS, mFramerateFps);
        hasInfo = true;
    }

    if (hasInfo) {
        info.setString(KEY_INFO_TYPE, VALUE_INFO_TYPE_SESSION);
    }
    return info;
}

void ECOSession::processFrameStats(const ECOData& stats) {
    ECOLOGD("processFrameStats");

    bool needToNotifyListener = false;
    ECOData info(ECOData::DATA_TYPE_INFO, systemTime(SYSTEM_TIME_BOOTTIME));
    info.setString(KEY_INFO_TYPE, VALUE_INFO_TYPE_FRAME);

    ECODataKeyValueIterator iter(stats);
    while (iter.hasNext()) {
        ECOData::ECODataKeyValuePair entry = iter.next();
        const std::string& key = entry.first;
        const ECOData::ECODataValueType value = entry.second;
        ECOLOGD("Processing %s key", key.c_str());

        if (!key.compare(KEY_STATS_TYPE)) {
            // Skip the key KEY_STATS_TYPE as that has been parsed already.
            continue;
        } else if (!key.compare(FRAME_NUM) || !key.compare(FRAME_PTS_US) ||
                   !key.compare(FRAME_TYPE) || !key.compare(FRAME_SIZE_BYTES) ||
                   !key.compare(ENCODER_ACTUAL_BITRATE_BPS) ||
                   !key.compare(ENCODER_FRAMERATE_FPS)) {
            // Only process the keys that are supported by ECOService 1.0.
            info.set(key, value);
        } else if (!key.compare(FRAME_AVG_QP)) {
            // Check the qp to see if need to notify the listener.
            const int32_t currAverageQp = std::get<int32_t>(value);

            // Check if the delta between current QP and last reported QP is larger than the
            // threshold specified by the listener.
            const bool largeQPChangeDetected =
                    abs(currAverageQp - mLastReportedQp) > mListenerQpCondition.mQpChangeThreshold;

            // Check if the qp is going from below threshold to beyond threshold.
            const bool exceedQpBlockinessThreshold =
                    (mLastReportedQp <= mListenerQpCondition.mQpBlocknessThreshold &&
                     currAverageQp > mListenerQpCondition.mQpBlocknessThreshold);

            // Check if the qp is going from beyond threshold to below threshold.
            const bool fallBelowQpBlockinessThreshold =
                    (mLastReportedQp > mListenerQpCondition.mQpBlocknessThreshold &&
                     currAverageQp <= mListenerQpCondition.mQpBlocknessThreshold);

            // Notify the listener if any of the above three conditions met.
            if (largeQPChangeDetected || exceedQpBlockinessThreshold ||
                fallBelowQpBlockinessThreshold) {
                mLastReportedQp = currAverageQp;
                needToNotifyListener = true;
            }

            info.set(key, value);
        } else {
            ECOLOGW("Unknown frame stats key %s from provider.", key.c_str());
        }
    }

    if (needToNotifyListener && mListener != nullptr) {
        Status status = mListener->onNewInfo(info);
        if (!status.isOk()) {
            ECOLOGE("%s: Failed to publish info: %s due to binder error", __FUNCTION__,
                    info.debugString().c_str());
            // Remove the listener. The lock has been acquired outside this function.
            mListener = nullptr;
        }
    }
}

Status ECOSession::getIsCameraRecording(bool* _aidl_return) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    *_aidl_return = mIsCameraRecording;
    return binder::Status::ok();
}

Status ECOSession::addStatsProvider(
        const sp<::android::media::eco::IECOServiceStatsProvider>& provider,
        const ::android::media::eco::ECOData& config, bool* status) {
    ::android::String16 name;
    Status result = provider->getName(&name);
    if (!result.isOk()) {
        // This binder transaction failure may due to permission issue.
        *status = false;
        ALOGE("Failed to get provider name");
        return STATUS_ERROR(ERROR_PERMISSION_DENIED, "Failed to get provider name");
    }

    ECOLOGV("Try to add stats provider name: %s uid: %d pid %d", ::android::String8(name).string(),
            IPCThreadState::self()->getCallingUid(), IPCThreadState::self()->getCallingPid());

    if (provider == nullptr) {
        ECOLOGE("%s: provider must not be null", __FUNCTION__);
        *status = false;
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "Null provider given to addStatsProvider");
    }

    std::scoped_lock<std::mutex> lock(mSessionLock);

    if (mProvider != nullptr) {
        ::android::String16 name;
        mProvider->getName(&name);
        String8 errorMsg = String8::format(
                "ECOService 1.0 only supports one stats provider, current provider: %s",
                ::android::String8(name).string());
        ECOLOGE("%s", errorMsg.string());
        *status = false;
        return STATUS_ERROR(ERROR_ALREADY_EXISTS, errorMsg.string());
    }

    // TODO: Handle the provider config.
    if (config.getDataType() != ECOData::DATA_TYPE_STATS_PROVIDER_CONFIG) {
        ECOLOGE("Provider config is invalid");
        *status = false;
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "Provider config is invalid");
    }

    mProvider = provider;
    mProviderName = name;
    *status = true;
    return binder::Status::ok();
}

Status ECOSession::removeStatsProvider(
        const sp<::android::media::eco::IECOServiceStatsProvider>& provider, bool* status) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    // Check if the provider is the same as current provider for the session.
    if (IInterface::asBinder(provider) != IInterface::asBinder(mProvider)) {
        *status = false;
        ECOLOGE("Failed to remove provider");
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "Provider does not match");
    }

    mProvider = nullptr;
    *status = true;
    return binder::Status::ok();
}

Status ECOSession::addInfoListener(
        const sp<::android::media::eco::IECOServiceInfoListener>& listener,
        const ::android::media::eco::ECOData& config, bool* status) {
    ALOGV("%s: Add listener %p", __FUNCTION__, listener.get());
    std::scoped_lock<std::mutex> lock(mSessionLock);

    ::android::String16 name;
    Status result = listener->getName(&name);
    if (!result.isOk()) {
        // This binder transaction failure may due to permission issue.
        *status = false;
        ALOGE("Failed to get listener name");
        return STATUS_ERROR(ERROR_PERMISSION_DENIED, "Failed to get listener name");
    }

    if (mListener != nullptr) {
        ECOLOGE("ECOService 1.0 only supports one listener");
        *status = false;
        return STATUS_ERROR(ERROR_ALREADY_EXISTS, "ECOService 1.0 only supports one listener");
    }

    if (listener == nullptr) {
        ECOLOGE("%s: listener must not be null", __FUNCTION__);
        *status = false;
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "Null listener given to addInfoListener");
    }

    if (config.getDataType() != ECOData::DATA_TYPE_INFO_LISTENER_CONFIG) {
        *status = false;
        ECOLOGE("%s: listener config is invalid", __FUNCTION__);
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "listener config is invalid");
    }

    if (config.isEmpty()) {
        *status = false;
        ECOLOGE("Listener must provide listening criterion");
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "listener config is empty");
    }

    // For ECOService 1.0, listener must specify the two threshold in order to receive info.
    if (config.findInt32(KEY_LISTENER_QP_BLOCKINESS_THRESHOLD,
                         &mListenerQpCondition.mQpBlocknessThreshold) != ECODataStatus::OK ||
        config.findInt32(KEY_LISTENER_QP_CHANGE_THRESHOLD,
                         &mListenerQpCondition.mQpChangeThreshold) != ECODataStatus::OK ||
        mListenerQpCondition.mQpBlocknessThreshold < ENCODER_MIN_QP ||
        mListenerQpCondition.mQpBlocknessThreshold > ENCODER_MAX_QP) {
        *status = false;
        ECOLOGE("%s: listener config is invalid", __FUNCTION__);
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "listener config is not valid");
    }

    ECOLOGD("Info listener name: %s uid: %d pid %d", ::android::String8(name).string(),
            IPCThreadState::self()->getCallingUid(), IPCThreadState::self()->getCallingPid());

    mListener = listener;
    mListenerName = name;
    mNewListenerAdded = true;
    mWorkerWaitCV.notify_all();

    *status = true;
    return binder::Status::ok();
}

Status ECOSession::removeInfoListener(
        const sp<::android::media::eco::IECOServiceInfoListener>& listener, bool* _aidl_return) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    // Check if the listener is the same as current listener for the session.
    if (IInterface::asBinder(listener) != IInterface::asBinder(mListener)) {
        *_aidl_return = false;
        ECOLOGE("Failed to remove listener");
        return STATUS_ERROR(ERROR_ILLEGAL_ARGUMENT, "Listener does not match");
    }

    mListener = nullptr;
    mNewListenerAdded = false;
    *_aidl_return = true;
    return binder::Status::ok();
}

Status ECOSession::pushNewStats(const ::android::media::eco::ECOData& stats, bool* _aidl_return) {
    ECOLOGV("ECOSession get new stats type: %s", stats.getDataTypeString().c_str());
    std::unique_lock<std::mutex> lock(mStatsQueueLock);
    mStatsQueue.push_back(stats);
    mWorkerWaitCV.notify_all();
    *_aidl_return = true;
    return binder::Status::ok();
}

Status ECOSession::getWidth(int32_t* _aidl_return) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    *_aidl_return = mWidth;
    return binder::Status::ok();
}

Status ECOSession::getHeight(int32_t* _aidl_return) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    *_aidl_return = mHeight;
    return binder::Status::ok();
}

Status ECOSession::getNumOfListeners(int32_t* _aidl_return) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    *_aidl_return = (mListener == nullptr ? 0 : 1);
    return binder::Status::ok();
}

Status ECOSession::getNumOfProviders(int32_t* _aidl_return) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    *_aidl_return = (mProvider == nullptr ? 0 : 1);
    return binder::Status::ok();
}

/*virtual*/ void ECOSession::binderDied(const wp<IBinder>& /*who*/) {
    ECOLOGV("binderDied");
}

status_t ECOSession::dump(int fd, const Vector<String16>& /*args*/) {
    std::scoped_lock<std::mutex> lock(mSessionLock);
    dprintf(fd, "\n== Session Info: ==\n\n");
    dprintf(fd,
            "Width: %d Height: %d isCameraRecording: %d, target-bitrate: %d bps codetype: %d "
            "profile: %d level: %d\n",
            mWidth, mHeight, mIsCameraRecording, mTargetBitrateBps, mCodecType, mCodecProfile,
            mCodecLevel);
    if (mProvider != nullptr) {
        dprintf(fd, "Provider: %s \n", ::android::String8(mProviderName).string());
    }
    if (mListener != nullptr) {
        dprintf(fd, "Listener: %s \n", ::android::String8(mListenerName).string());
    }
    dprintf(fd, "\n===================\n\n");

    return NO_ERROR;
}

void ECOSession::logStats(const ECOData& data) {
    // Check if mLogStats is true;
    if (!mLogStats || mLogStatsEntries == 0) return;

    // Check if we need to remove the old entry.
    if (mStatsDebugBuffer.size() >= mLogStatsEntries) {
        mStatsDebugBuffer.pop_front();
    }

    mStatsDebugBuffer.push_back(data);
}

void ECOSession::logInfos(const ECOData& data) {
    // Check if mLogInfo is true;
    if (!mLogInfo || mLogInfoEntries == 0) return;

    // Check if we need to remove the old entry.
    if (mInfosDebugBuffer.size() >= mLogInfoEntries) {
        mInfosDebugBuffer.pop_front();
    }

    mInfosDebugBuffer.push_back(data);
}

}  // namespace eco
}  // namespace media
}  // namespace android
