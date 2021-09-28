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

// A fake ECOServiceInfoListener for testing ECOService and ECOSession.

#include <android-base/unique_fd.h>
#include <android/media/eco/BnECOServiceInfoListener.h>
#include <android/media/eco/IECOSession.h>
#include <binder/BinderService.h>
#include <binder/Parcel.h>
#include <binder/Parcelable.h>
#include <cutils/ashmem.h>
#include <gtest/gtest.h>
#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <utils/Log.h>

#include "eco/ECOData.h"
#include "eco/ECODataKey.h"

namespace android {
namespace media {
namespace eco {

using ::android::sp;
using ::android::binder::Status;

/**
 * A fake ECOServiceInfoListener.
 *
 * FakeECOServiceInfoListener is a fake ECOServiceInfoListener that used for testing.
 */
class FakeECOServiceInfoListener : public BnECOServiceInfoListener {
public:
    // Method called by the FakeECOServiceInfoListener when there is new info from ECOService.
    // This is used by the test to verify the information is sent by the ECOService correctly.
    using InfoAvailableCallback =
            std::function<void(const ::android::media::eco::ECOData& newInfo)>;

    FakeECOServiceInfoListener(int32_t width, int32_t height, bool isCameraRecording,
                               sp<IECOSession> session);

    FakeECOServiceInfoListener(int32_t width, int32_t height, bool isCameraRecording);

    void setECOSession(android::sp<IECOSession> session) { mECOSession = session; }

    virtual ~FakeECOServiceInfoListener();

    virtual Status getType(int32_t* _aidl_return);
    virtual Status getName(::android::String16* _aidl_return);
    virtual Status getECOSession(::android::sp<::android::IBinder>* _aidl_return);
    virtual Status onNewInfo(const ::android::media::eco::ECOData& newInfo);

    // Helper callback to send the info to the test.
    void setInfoAvailableCallback(InfoAvailableCallback callback) {
        mInfoAvaiableCallback = callback;
    }

    // IBinder::DeathRecipient implementation
    virtual void binderDied(const wp<IBinder>& who);

private:
    int32_t mWidth;
    int32_t mHeight;
    bool mIsCameraRecording;
    android::sp<IECOSession> mECOSession;
    InfoAvailableCallback mInfoAvaiableCallback;
};

}  // namespace eco
}  // namespace media
}  // namespace android
