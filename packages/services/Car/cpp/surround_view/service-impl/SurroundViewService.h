/*
 * Copyright 2020 The Android Open Source Project
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

#pragma once

#include "SurroundView2dSession.h"
#include "SurroundView3dSession.h"
#include "AnimationModule.h"
#include "IOModule.h"
#include "VhalHandler.h"

#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/automotive/sv/1.0/types.h>
#include <android/hardware/automotive/sv/1.0/ISurroundViewService.h>
#include <android/hardware/automotive/sv/1.0/ISurroundViewStream.h>
#include <android/hardware/automotive/sv/1.0/ISurroundView2dSession.h>
#include <android/hardware/automotive/sv/1.0/ISurroundView3dSession.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

#include <thread>

using namespace ::android::hardware::automotive::evs::V1_1;
using namespace ::android::hardware::automotive::sv::V1_0;
using ::android::hardware::Return;
using ::android::sp;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

class SurroundViewService : public ISurroundViewService {
public:
    // Methods from ::android::hardware::automotive::sv::V1_0::ISurroundViewService follow.
    Return<void> getCameraIds(getCameraIds_cb _hidl_cb) override;
    Return<void> start2dSession(start2dSession_cb _hidl_cb) override;
    Return<SvResult> stop2dSession(
        const sp<ISurroundView2dSession>& sv2dSession) override;

    Return<void> start3dSession(start3dSession_cb _hidl_cb) override;
    Return<SvResult> stop3dSession(
        const sp<ISurroundView3dSession>& sv3dSession) override;

    static sp<SurroundViewService> getInstance();
private:
    SurroundViewService();
    ~SurroundViewService();

    VhalHandler* mVhalHandler;
    AnimationModule* mAnimationModule;
    IOModule* mIOModule;
    IOModuleConfig mConfig;

    bool initialize();
    sp<IEvsEnumerator> mEvs;

    static std::mutex sLock;
    static sp<SurroundViewService> sService GUARDED_BY(sLock);

    static sp<SurroundView2dSession> sSurroundView2dSession GUARDED_BY(sLock);
    static sp<SurroundView3dSession> sSurroundView3dSession GUARDED_BY(sLock);
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
