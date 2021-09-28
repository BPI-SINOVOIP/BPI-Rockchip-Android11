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

#ifndef ANDROID_MEDIA_ECO_SERVICE_H_
#define ANDROID_MEDIA_ECO_SERVICE_H_

#include <android/media/eco/BnECOService.h>
#include <binder/BinderService.h>

#include <list>

#include "eco/ECODebug.h"
#include "eco/ECOSession.h"

namespace android {
namespace media {
namespace eco {

using android::sp;
using android::binder::Status;
using android::media::eco::ECOSession;

/**
 * ECO (Encoder Camera Optimization) service.
 *
 * ECOService creates and manages EcoSession to relay feedback information between one or multiple
 * ECOServiceStatsProvider and ECOServiceInfoListener. The relationship can be many-to-many. In
 * general, ECOServiceStatsProvider extracts information from an encoder for a given encoding
 * session. EcoSession then relays the encoder information to any subscribed
 * ECOServiceInfoListener.
 *
 * Internally, ECOService creates an ECOSession for each encoding session. Upon start, both
 * ECOServiceStatsProvider and ECOServiceInfoListener should call obtainSession to get the
 * ECOSession instance. After that, ECOServiceStatsProvider should push Stats to ECOSession and
 * ECOServiceInfoListener should listen to the info from ECOSession. Upon finish, both
 * ECOServiceStatsProvider and ECOServiceInfoListener should remove themselves from ECOSession.
 * Then ECOService will safely destroy the ECOSession.
 */
class ECOService : public BinderService<ECOService>,
                   public BnECOService,
                   public virtual IBinder::DeathRecipient {
    friend class BinderService<ECOService>;

public:
    ECOService();

    virtual ~ECOService() = default;

    virtual Status obtainSession(int32_t width, int32_t height, bool isCameraRecording,
                                 sp<IECOSession>* _aidl_return);

    virtual Status getNumOfSessions(int32_t* _aidl_return);

    virtual Status getSessions(::std::vector<sp<IBinder>>* _aidl_return);

    // Implementation of BinderService<T>
    static char const* getServiceName() { return "media.ecoservice"; }

    // IBinder::DeathRecipient implementation
    virtual void binderDied(const wp<IBinder>& who);

    virtual status_t dump(int fd, const Vector<String16>& args);

private:
    // Lock guarding ECO service state
    Mutex mServiceLock;

    struct SessionConfig {
        int32_t mWidth;
        int32_t mHeight;
        bool mIsCameraRecording;

        SessionConfig(int w, int h, bool isCameraRecording)
              : mWidth(w), mHeight(h), mIsCameraRecording(isCameraRecording) {}

        bool operator==(const SessionConfig& cfg) {
            return mWidth == cfg.mWidth && mHeight == cfg.mHeight &&
                   mIsCameraRecording == cfg.mIsCameraRecording;
        }
    };

    friend bool operator==(const SessionConfig& p1, const SessionConfig& p2) {
        return p1.mWidth == p2.mWidth && p1.mHeight == p2.mHeight &&
               p1.mIsCameraRecording == p2.mIsCameraRecording;
    }

    // Hash function for mSessionConfigToSessionMap.
    // TODO(hkuang): Add test for this hash function.
    struct SessionConfigHash {
        size_t operator()(const SessionConfig& cfg) const {
            // Generate a hash by bit shifting and concatenation.
            return cfg.mWidth | (cfg.mHeight << 16) | ((int32_t)cfg.mIsCameraRecording << 31);
        }
    };

    // Map from SessionConfig to session.
    std::unordered_map<SessionConfig, wp<ECOSession>, SessionConfigHash> mSessionConfigToSessionMap;

    using MapIterType =
            std::unordered_map<SessionConfig, wp<ECOSession>, SessionConfigHash>::iterator;

    // A helpful function to traverse the mSessionConfigToSessionMap, remove the entry that
    // does not exist any more and call |callback| when the entry is valid.
    void SanitizeSession(const std::function<void(MapIterType it)>& callback);
};

}  // namespace eco
}  // namespace media
}  // namespace android

#endif  // ANDROID_MEDIA_ECO_SERVICE_H_
