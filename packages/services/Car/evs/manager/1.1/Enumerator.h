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

#ifndef ANDROID_AUTOMOTIVE_EVS_V1_1_EVSCAMERAENUMERATOR_H
#define ANDROID_AUTOMOTIVE_EVS_V1_1_EVSCAMERAENUMERATOR_H

#include "HalCamera.h"
#include "VirtualCamera.h"
#include "stats/StatsCollector.h"

#include <list>
#include <unordered_map>
#include <unordered_set>

#include <android/hardware/automotive/evs/1.1/IEvsEnumerator.h>
#include <android/hardware/automotive/evs/1.1/IEvsDisplay.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <system/camera_metadata.h>

using namespace ::android::hardware::automotive::evs::V1_1;
using ::android::hardware::Return;
using ::android::hardware::hidl_string;
using ::android::hardware::camera::device::V3_2::Stream;

namespace android {
namespace automotive {
namespace evs {
namespace V1_1 {
namespace implementation {

using IEvsCamera_1_0     = ::android::hardware::automotive::evs::V1_0::IEvsCamera;
using IEvsCamera_1_1     = ::android::hardware::automotive::evs::V1_1::IEvsCamera;
using IEvsEnumerator_1_0 = ::android::hardware::automotive::evs::V1_0::IEvsEnumerator;
using IEvsEnumerator_1_1 = ::android::hardware::automotive::evs::V1_1::IEvsEnumerator;
using IEvsDisplay_1_0    = ::android::hardware::automotive::evs::V1_0::IEvsDisplay;
using IEvsDisplay_1_1    = ::android::hardware::automotive::evs::V1_1::IEvsDisplay;
using EvsDisplayState    = ::android::hardware::automotive::evs::V1_0::DisplayState;

class Enumerator : public IEvsEnumerator {
public:
    // Methods from ::android::hardware::automotive::evs::V1_0::IEvsEnumerator follow.
    Return<void>                getCameraList(getCameraList_cb _hidl_cb)  override;
    Return<sp<IEvsCamera_1_0>>  openCamera(const hidl_string& cameraId)  override;
    Return<void>                closeCamera(const ::android::sp<IEvsCamera_1_0>& virtualCamera)  override;
    Return<sp<IEvsDisplay_1_0>> openDisplay()  override;
    Return<void>                closeDisplay(const ::android::sp<IEvsDisplay_1_0>& display)  override;
    Return<EvsDisplayState>     getDisplayState()  override;

    // Methods from ::android::hardware::automotive::evs::V1_1::IEvsEnumerator follow.
    Return<void>                getCameraList_1_1(getCameraList_1_1_cb _hidl_cb) override;
    Return<sp<IEvsCamera_1_1>>  openCamera_1_1(const hidl_string& cameraId,
                                               const Stream& streamCfg) override;
    Return<bool>                isHardware() override { return false; }
    Return<void>                getDisplayIdList(getDisplayIdList_cb _list_cb) override;
    Return<sp<IEvsDisplay_1_1>> openDisplay_1_1(uint8_t id) override;
    Return<void> getUltrasonicsArrayList(getUltrasonicsArrayList_cb _hidl_cb) override;
    Return<sp<IEvsUltrasonicsArray>> openUltrasonicsArray(
            const hidl_string& ultrasonicsArrayId) override;
    Return<void> closeUltrasonicsArray(
            const ::android::sp<IEvsUltrasonicsArray>& evsUltrasonicsArray) override;

    // Methods from ::android.hidl.base::V1_0::IBase follow.
    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) override;

    // Implementation details
    bool init(const char* hardwareServiceName);

    // Destructor
    virtual ~Enumerator();

private:
    bool inline                     checkPermission();
    bool                            isLogicalCamera(const camera_metadata_t *metadata);
    std::unordered_set<std::string> getPhysicalCameraIds(const std::string& id);

    sp<IEvsEnumerator_1_1>            mHwEnumerator;  // Hardware enumerator
    wp<IEvsDisplay_1_0>               mActiveDisplay; // Display proxy object warpping hw display

    // List of active camera proxy objects that wrap hw cameras
    std::unordered_map<std::string,
                       sp<HalCamera>> mActiveCameras;

    // List of camera descriptors of enumerated hw cameras
    std::unordered_map<std::string,
                       CameraDesc>    mCameraDevices;

    // List of available physical display devices
    std::list<uint8_t>                mDisplayPorts;

    // Display port the internal display is connected to.
    uint8_t                           mInternalDisplayPort;

    // Collecting camera usage statistics from clients
    sp<StatsCollector>                mClientsMonitor;

    // Boolean flag to tell whether the camera usages are being monitored or not
    bool                              mMonitorEnabled;

    // LSHAL dump
    void cmdDump(int fd, const hidl_vec<hidl_string>& options);
    void cmdHelp(int fd);
    void cmdList(int fd, const hidl_vec<hidl_string>& options);
    void cmdDumpDevice(int fd, const hidl_vec<hidl_string>& options);
};

} // namespace implementation
} // namespace V1_1
} // namespace evs
} // namespace automotive
} // namespace android

#endif  // ANDROID_AUTOMOTIVE_EVS_V1_1_EVSCAMERAENUMERATOR_H
