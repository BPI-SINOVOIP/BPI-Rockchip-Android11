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

#ifndef ANDROID_HARDWARE_CAMERA_DEVICE_V3_5_CAMERADEVICE_H
#define ANDROID_HARDWARE_CAMERA_DEVICE_V3_5_CAMERADEVICE_H

#include "CameraDeviceSession.h"
#include <../../../../3.4/default/include/device_v3_4_impl/CameraDevice_3_4.h>

#include <android/hardware/camera/device/3.5/ICameraDevice.h>

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_5 {
namespace implementation {

using namespace ::android::hardware::camera::device;

using ::android::hardware::camera::common::V1_0::helper::CameraModule;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_string;
using ::android::hardware::camera::common::V1_0::TorchMode;
using ::android::hardware::camera::common::V1_0::helper::CameraModule;
using ::android::hardware::camera::common::V1_0::Status;
using ::android::sp;

struct CameraDevice : public V3_4::implementation::CameraDevice {
    // Called by provider HAL.
    // Provider HAL must ensure the uniqueness of CameraDevice object per cameraId, or there could
    // be multiple CameraDevice trying to access the same physical camera.  Also, provider will have
    // to keep track of all CameraDevice objects in order to notify CameraDevice when the underlying
    // camera is detached.
    // Delegates nearly all work to CameraDevice_3_4
    CameraDevice(sp<CameraModule> module,
                 const std::string& cameraId,
                 const SortedVector<std::pair<std::string, std::string>>& cameraDeviceNames);
    virtual ~CameraDevice();

    virtual sp<V3_2::ICameraDevice> getInterface() override {
        return new TrampolineDeviceInterface_3_5(this);
    }

protected:
    virtual sp<V3_2::implementation::CameraDeviceSession> createSession(camera3_device_t*,
            const camera_metadata_t* deviceInfo,
            const sp<V3_2::ICameraDeviceCallback>&) override;

    Return<void> getPhysicalCameraCharacteristics(const hidl_string& physicalCameraId,
            V3_5::ICameraDevice::getPhysicalCameraCharacteristics_cb _hidl_cb);

    Return<void> isStreamCombinationSupported(
            const V3_4::StreamConfiguration& streams,
            V3_5::ICameraDevice::isStreamCombinationSupported_cb _hidl_cb);

private:
    struct TrampolineDeviceInterface_3_5 : public ICameraDevice {
        TrampolineDeviceInterface_3_5(sp<CameraDevice> parent) :
            mParent(parent) {}

        virtual Return<void> getResourceCost(V3_2::ICameraDevice::getResourceCost_cb _hidl_cb)
                override {
            return mParent->getResourceCost(_hidl_cb);
        }

        virtual Return<void> getCameraCharacteristics(
                V3_2::ICameraDevice::getCameraCharacteristics_cb _hidl_cb) override {
            return mParent->getCameraCharacteristics(_hidl_cb);
        }

        virtual Return<Status> setTorchMode(TorchMode mode) override {
            return mParent->setTorchMode(mode);
        }

        virtual Return<void> open(const sp<V3_2::ICameraDeviceCallback>& callback,
                V3_2::ICameraDevice::open_cb _hidl_cb) override {
            return mParent->open(callback, _hidl_cb);
        }

        virtual Return<void> dumpState(const hidl_handle& fd) override {
            return mParent->dumpState(fd);
        }

        virtual Return<void> getPhysicalCameraCharacteristics(const hidl_string& physicalCameraId,
                V3_5::ICameraDevice::getPhysicalCameraCharacteristics_cb _hidl_cb) override {
            return mParent->getPhysicalCameraCharacteristics(physicalCameraId, _hidl_cb);
        }

        virtual Return<void> isStreamCombinationSupported(
                const V3_4::StreamConfiguration& streams,
                V3_5::ICameraDevice::isStreamCombinationSupported_cb _hidl_cb) override {
            return mParent->isStreamCombinationSupported(streams, _hidl_cb);
        }

    private:
        sp<CameraDevice> mParent;
    };
};

}  // namespace implementation
}  // namespace V3_5
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android

#endif // ANDROID_HARDWARE_CAMERA_DEVICE_V3_5_CAMERADEVICE_H
