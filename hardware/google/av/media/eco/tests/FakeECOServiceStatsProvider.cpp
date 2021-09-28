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
#define LOG_TAG "FakeECOServiceStatsProvider"

#include "FakeECOServiceStatsProvider.h"

#include <android-base/unique_fd.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/Parcelable.h>
#include <cutils/ashmem.h>
#include <gtest/gtest.h>
#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <utils/Log.h>

namespace android {
namespace media {
namespace eco {

FakeECOServiceStatsProvider::FakeECOServiceStatsProvider(int32_t width, int32_t height,
                                                         bool isCameraRecording, float frameRate,
                                                         android::sp<IECOSession> session)
      : mWidth(width),
        mHeight(height),
        mIsCameraRecording(isCameraRecording),
        mFrameRate(frameRate),
        mFrameNumber(0),
        mECOSession(session) {
    ALOGD("FakeECOServiceStatsProvider construct with w: %d, h: %d, isCameraRecording: %d, "
          "frameRate: %f",
          mWidth, mHeight, mIsCameraRecording, mFrameRate);
}

FakeECOServiceStatsProvider::FakeECOServiceStatsProvider(int32_t width, int32_t height,
                                                         bool isCameraRecording, float frameRate)
      : mWidth(width),
        mHeight(height),
        mIsCameraRecording(isCameraRecording),
        mFrameRate(frameRate),
        mFrameNumber(0) {
    ALOGD("FakeECOServiceStatsProvider construct with w: %d, h: %d, isCameraRecording: %d, "
          "frameRate: %f",
          mWidth, mHeight, mIsCameraRecording, mFrameRate);
}

FakeECOServiceStatsProvider::~FakeECOServiceStatsProvider() {
    ALOGD("FakeECOServiceStatsProvider destructor");
}

Status FakeECOServiceStatsProvider::getType(int32_t* /*_aidl_return*/) {
    return binder::Status::ok();
}

Status FakeECOServiceStatsProvider::getName(::android::String16* _aidl_return) {
    *_aidl_return = String16("FakeECOServiceStatsProvider");
    return binder::Status::ok();
}

Status FakeECOServiceStatsProvider::getECOSession(sp<::android::IBinder>* _aidl_return) {
    *_aidl_return = IInterface::asBinder(mECOSession);
    return binder::Status::ok();
}

bool FakeECOServiceStatsProvider::injectSessionStats(const ECOData& stats) {
    if (mECOSession == nullptr) return false;
    ALOGD("injectSessionStats");
    bool res;
    mECOSession->pushNewStats(stats, &res);
    return res;
}

bool FakeECOServiceStatsProvider::injectFrameStats(const ECOData& stats) {
    if (mECOSession == nullptr) return false;
    ALOGD("injectPerFrameStats");
    bool res;
    mECOSession->pushNewStats(stats, &res);
    return res;
}

// IBinder::DeathRecipient implementation
void FakeECOServiceStatsProvider::binderDied(const wp<IBinder>& /*who*/) {}

}  // namespace eco
}  // namespace media
}  // namespace android
