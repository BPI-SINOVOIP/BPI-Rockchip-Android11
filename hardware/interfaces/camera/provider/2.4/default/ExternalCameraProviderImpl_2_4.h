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

#ifndef ANDROID_HARDWARE_CAMERA_PROVIDER_V2_4_EXTCAMERAPROVIDER_H
#define ANDROID_HARDWARE_CAMERA_PROVIDER_V2_4_EXTCAMERAPROVIDER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <hidl/Status.h>
#include <hidl/MQDescriptor.h>
#include "ExternalCameraUtils_3.4.h"

#include "CameraProvider_2_4.h"

namespace android {
namespace hardware {
namespace camera {
namespace provider {
namespace V2_4 {
namespace implementation {

using ::android::hardware::camera::common::V1_0::CameraDeviceStatus;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::hardware::camera::common::V1_0::VendorTagSection;
using ::android::hardware::camera::external::common::ExternalCameraConfig;
using ::android::hardware::camera::provider::V2_4::ICameraProvider;
using ::android::hardware::camera::provider::V2_4::ICameraProviderCallback;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::sp;
using ::android::Mutex;

/**
 * The implementation of external webcam CameraProvider 2.4, separated
 * from the HIDL interface layer to allow for implementation reuse by later
 * provider versions.
 *
 * This camera provider supports standard UVC webcameras via the Linux V4L2
 * UVC driver.
 */
struct ExternalCameraProviderImpl_2_4 {
    ExternalCameraProviderImpl_2_4();
    ~ExternalCameraProviderImpl_2_4();

    // Caller must use this method to check if CameraProvider ctor failed
    bool isInitFailed() { return false;}

    // Methods from ::android::hardware::camera::provider::V2_4::ICameraProvider follow.
    Return<Status> setCallback(const sp<ICameraProviderCallback>& callback);
    Return<void> getVendorTags(ICameraProvider::getVendorTags_cb _hidl_cb);
    Return<void> getCameraIdList(ICameraProvider::getCameraIdList_cb _hidl_cb);
    Return<void> isSetTorchModeSupported(ICameraProvider::isSetTorchModeSupported_cb _hidl_cb);
    Return<void> getCameraDeviceInterface_V1_x(
            const hidl_string&,
            ICameraProvider::getCameraDeviceInterface_V1_x_cb);
    Return<void> getCameraDeviceInterface_V3_x(
            const hidl_string&,
            ICameraProvider::getCameraDeviceInterface_V3_x_cb);

private:

    void addExternalCamera(const char* devName);

    void deviceAdded(const char* devName);

    void deviceRemoved(const char* devName);

    class HotplugThread : public android::Thread {
    public:
        HotplugThread(ExternalCameraProviderImpl_2_4* parent);
        ~HotplugThread();

        virtual bool threadLoop() override;

    private:
        ExternalCameraProviderImpl_2_4* mParent = nullptr;
        const std::unordered_set<std::string> mInternalDevices;

        int mINotifyFD = -1;
        int mWd = -1;
    };

    Mutex mLock;
    sp<ICameraProviderCallback> mCallbacks = nullptr;
    std::unordered_map<std::string, CameraDeviceStatus> mCameraStatusMap; // camera id -> status
    const ExternalCameraConfig mCfg;
    HotplugThread mHotPlugThread;
    int mPreferredHal3MinorVersion;
};



}  // namespace implementation
}  // namespace V2_4
}  // namespace provider
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_CAMERA_PROVIDER_V2_4_EXTCAMERAPROVIDER_H
