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

#ifndef ANDROID_MEDIA_ECO_SERVICE_STATS_PROVIDER_H_
#define ANDROID_MEDIA_ECO_SERVICE_STATS_PROVIDER_H_

#include <android/media/eco/BnECOServiceStatsProvider.h>
#include <android/media/eco/IECOSession.h>
#include <binder/BinderService.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "ECOData.h"

namespace android {
namespace media {
namespace eco {

using ::android::binder::Status;

/**
 * ECOServiceStatsProvider interface class.
 */
class ECOServiceStatsProvider : public BinderService<IECOServiceStatsProvider>,
                                public BnECOServiceStatsProvider,
                                public virtual IBinder::DeathRecipient {
    friend class BinderService<IECOServiceStatsProvider>;

public:
    // Create a ECOServiceStatsProvider with specifed width, height and isCameraRecording.
    ECOServiceStatsProvider(int32_t width, int32_t height, bool isCameraRecording);

    virtual ~ECOServiceStatsProvider() {}

    virtual Status getType(int32_t* _aidl_return) = 0;
    virtual Status getName(::android::String16* _aidl_return) = 0;
    virtual Status getECOSession(::android::sp<::android::IBinder>* _aidl_return) = 0;
    virtual Status isCameraRecording(bool* _aidl_return) = 0;

    // IBinder::DeathRecipient implementation
    virtual void binderDied(const wp<IBinder>& who);

private:
};

}  // namespace eco
}  // namespace media
}  // namespace android

#endif  // ANDROID_MEDIA_ECO_SERVICE_STATS_PROVIDER_H_
