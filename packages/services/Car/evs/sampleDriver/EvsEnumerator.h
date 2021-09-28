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

#ifndef ANDROID_HARDWARE_AUTOMOTIVE_EVS_V1_1_EVSCAMERAENUMERATOR_H
#define ANDROID_HARDWARE_AUTOMOTIVE_EVS_V1_1_EVSCAMERAENUMERATOR_H

#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/automotive/evs/1.1/IEvsCamera.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android/frameworks/automotive/display/1.0/IAutomotiveDisplayProxyService.h>

#include <unordered_map>
#include <thread>
#include <atomic>

#include "ConfigManager.h"

using ::android::hardware::camera::device::V3_2::Stream;
using EvsDisplayState = ::android::hardware::automotive::evs::V1_0::DisplayState;
using IEvsCamera_1_0  = ::android::hardware::automotive::evs::V1_0::IEvsCamera;
using IEvsCamera_1_1  = ::android::hardware::automotive::evs::V1_1::IEvsCamera;
using IEvsDisplay_1_0  = ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using IEvsDisplay_1_1  = ::android::hardware::automotive::evs::V1_1::IEvsDisplay;
using android::frameworks::automotive::display::V1_0::IAutomotiveDisplayProxyService;

namespace android {
namespace hardware {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {


class EvsV4lCamera;    // from EvsCamera.h
class EvsGlDisplay;    // from EvsGlDisplay.h

class EvsEnumerator : public IEvsEnumerator {
public:
    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsEnumerator follow.
    Return<void>                getCameraList(getCameraList_cb _hidl_cb)  override;
    Return<sp<IEvsCamera_1_0>>  openCamera(const hidl_string& cameraId) override;
    Return<void>                closeCamera(const ::android::sp<IEvsCamera_1_0>& pCamera)  override;
    Return<sp<IEvsDisplay_1_0>> openDisplay()  override;
    Return<void>                closeDisplay(const ::android::sp<IEvsDisplay_1_0>& display)  override;
    Return<EvsDisplayState>     getDisplayState()  override;

    // Methods from ::android::hardware::automotive::evs::V1_1::IEvsEnumerator follow.
    Return<void>                getCameraList_1_1(getCameraList_1_1_cb _hidl_cb) override;
    Return<sp<IEvsCamera_1_1>>  openCamera_1_1(const hidl_string& cameraId,
                                               const Stream& streamCfg) override;
    Return<bool>                isHardware() override { return true; }
    Return<void>                getDisplayIdList(getDisplayIdList_cb _list_cb) override;
    Return<sp<IEvsDisplay_1_1>> openDisplay_1_1(uint8_t port) override;
    Return<void> getUltrasonicsArrayList(getUltrasonicsArrayList_cb _hidl_cb) override;
    Return<sp<IEvsUltrasonicsArray>> openUltrasonicsArray(
            const hidl_string& ultrasonicsArrayId) override;
    Return<void> closeUltrasonicsArray(
            const ::android::sp<IEvsUltrasonicsArray>& evsUltrasonicsArray) override;

    // Implementation details
    EvsEnumerator(sp<IAutomotiveDisplayProxyService> proxyService = nullptr);

    // Listen to video device uevents
    static void EvsUeventThread(std::atomic<bool>& running);

private:
    struct CameraRecord {
        CameraDesc          desc;
        wp<EvsV4lCamera>    activeInstance;

        CameraRecord(const char *cameraId) : desc() { desc.v1.cameraId = cameraId; }
    };

    bool checkPermission();

    static bool qualifyCaptureDevice(const char* deviceName);
    static CameraRecord* findCameraById(const std::string& cameraId);
    static void enumerateCameras();
    static void enumerateDisplays();

    void closeCamera_impl(const sp<IEvsCamera_1_0>& pCamera, const std::string& cameraId);

    // NOTE:  All members values are static so that all clients operate on the same state
    //        That is to say, this is effectively a singleton despite the fact that HIDL
    //        constructs a new instance for each client.
    //        Because our server has a single thread in the thread pool, these values are
    //        never accessed concurrently despite potentially having multiple instance objects
    //        using them.
    static std::unordered_map<std::string,
                              CameraRecord> sCameraList;

    static wp<EvsGlDisplay>                 sActiveDisplay; // Weak pointer.
                                                            // Object destructs if client dies.

    static std::mutex                       sLock;          // Mutex on shared camera device list.
    static std::condition_variable          sCameraSignal;  // Signal on camera device addition.

    static std::unique_ptr<ConfigManager>   sConfigManager; // ConfigManager

    static sp<IAutomotiveDisplayProxyService> sDisplayProxy;
    static std::unordered_map<uint8_t,
                              uint64_t>       sDisplayPortList;
    static uint64_t                           sInternalDisplayId;
};

} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace hardware
} // namespace android

#endif  // ANDROID_HARDWARE_AUTOMOTIVE_EVS_V1_1_EVSCAMERAENUMERATOR_H
