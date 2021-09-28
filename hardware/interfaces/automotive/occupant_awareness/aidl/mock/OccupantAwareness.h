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

#pragma once

#include <thread>

#include <aidl/android/hardware/automotive/occupant_awareness/BnOccupantAwareness.h>
#include <aidl/android/hardware/automotive/occupant_awareness/BnOccupantAwarenessClientCallback.h>
#include <utils/StrongPointer.h>

#include "DetectionGenerator.h"

namespace android {
namespace hardware {
namespace automotive {
namespace occupant_awareness {
namespace V1_0 {
namespace implementation {

using ::aidl::android::hardware::automotive::occupant_awareness::BnOccupantAwareness;
using ::aidl::android::hardware::automotive::occupant_awareness::IOccupantAwarenessClientCallback;
using ::aidl::android::hardware::automotive::occupant_awareness::OccupantAwarenessStatus;
using ::aidl::android::hardware::automotive::occupant_awareness::OccupantDetections;
using ::aidl::android::hardware::automotive::occupant_awareness::Role;

/**
 * The mock HAL can detect presence of Driver and front passenger, and driver awareness detection
 * for driver.
 **/
class OccupantAwareness : public BnOccupantAwareness {
  public:
    // Methods from ::android::hardware::automotive::occupant_awareness::IOccupantAwareness
    // follow.
    ndk::ScopedAStatus startDetection(OccupantAwarenessStatus* status) override;
    ndk::ScopedAStatus stopDetection(OccupantAwarenessStatus* status) override;
    ndk::ScopedAStatus getCapabilityForRole(Role occupantRole, int32_t* capabilities) override;
    ndk::ScopedAStatus getState(Role occupantRole, int detectionCapability,
                                OccupantAwarenessStatus* status) override;
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IOccupantAwarenessClientCallback>& callback) override;
    ndk::ScopedAStatus getLatestDetection(OccupantDetections* detections) override;

  private:
    bool isValidRole(Role occupantRole);
    bool isValidDetectionCapabilities(int detectionCapabilities);
    bool isSingularCapability(int detectionCapability);

    void workerThreadFunction();
    static void startWorkerThread(OccupantAwareness* occupantAwareness);

    std::mutex mMutex;
    std::shared_ptr<IOccupantAwarenessClientCallback> mCallback = nullptr;
    OccupantAwarenessStatus mStatus = OccupantAwarenessStatus::NOT_INITIALIZED;

    OccupantDetections mLatestDetections;
    std::thread mWorkerThread;

    DetectionGenerator mGenerator;

    // Generate a new detection every 1ms.
    const int64_t mDetectionDurationMs = 1;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace occupant_awareness
}  // namespace automotive
}  // namespace hardware
}  // namespace android
