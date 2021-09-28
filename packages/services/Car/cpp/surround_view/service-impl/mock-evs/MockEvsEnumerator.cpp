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

#include "MockEvsEnumerator.h"
#include "MockEvsCamera.h"

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

using ::android::sp;

using CameraDesc_1_0 = ::android::hardware::automotive::evs::V1_0::CameraDesc;
using CameraDesc_1_1 = ::android::hardware::automotive::evs::V1_1::CameraDesc;

MockEvsEnumerator::MockEvsEnumerator() {
    mConfigManager = ConfigManager::Create();
}

Return<void> MockEvsEnumerator::getCameraList(getCameraList_cb _hidl_cb) {
    // Not implemented.

    (void)_hidl_cb;
    return {};
}

Return<sp<IEvsCamera_1_0>> MockEvsEnumerator::openCamera(
        const hidl_string& cameraId) {
    // Not implemented.

    (void)cameraId;
    return nullptr;
}

Return<void> MockEvsEnumerator::closeCamera(
        const sp<IEvsCamera_1_0>& virtualCamera) {
    // Not implemented.

    (void)virtualCamera;
    return {};
}

Return<sp<IEvsDisplay_1_0>> MockEvsEnumerator::openDisplay() {
    // Not implemented.

    return nullptr;
}

Return<void> MockEvsEnumerator::closeDisplay(
        const sp<IEvsDisplay_1_0>& display) {
    // Not implemented.

    (void)display;
    return {};
}

Return<EvsDisplayState> MockEvsEnumerator::getDisplayState() {
    // Not implemented.

    return EvsDisplayState::DEAD;
}

Return<void> MockEvsEnumerator::getCameraList_1_1(
        getCameraList_1_1_cb _hidl_cb) {
    // We only take camera group into consideration here.
    vector<string> cameraGroupIdList = mConfigManager->getCameraGroupIdList();
    LOG(INFO) << "getCameraGroupIdList: " << cameraGroupIdList.size();
    for (int i = 0; i < cameraGroupIdList.size(); i++) {
        LOG(INFO) << "Camera[" << i << "]: " << cameraGroupIdList[i];
    }

    vector<CameraDesc_1_1> hidlCameras;
    for (int i = 0; i < cameraGroupIdList.size(); i++) {
        CameraDesc_1_1 desc = {};
        desc.v1.cameraId = cameraGroupIdList[i];
        unique_ptr<ConfigManager::CameraGroupInfo>& cameraGroupInfo =
                mConfigManager->getCameraGroupInfo(cameraGroupIdList[i]);
        if (cameraGroupInfo != nullptr) {
            desc.metadata.setToExternal(
                    (uint8_t*)cameraGroupInfo->characteristics,
                    get_camera_metadata_size(cameraGroupInfo->characteristics));
        } else {
            LOG(WARNING) << "Cannot find camera info for "
                         << cameraGroupIdList[i];
        }
        hidlCameras.emplace_back(desc);
    }
    _hidl_cb(hidlCameras);

    return {};
}

Return<sp<IEvsCamera_1_1>> MockEvsEnumerator::openCamera_1_1(
        const hidl_string& cameraId, const Stream& streamCfg) {
    LOG(INFO) << __FUNCTION__ << ": " << streamCfg.width << ", " << streamCfg.height;
    return new MockEvsCamera(cameraId, streamCfg);
}

Return<void> MockEvsEnumerator::getDisplayIdList(getDisplayIdList_cb _list_cb) {
    // Not implemented.

    (void)_list_cb;
    return {};
}

Return<sp<IEvsDisplay_1_1>> MockEvsEnumerator::openDisplay_1_1(uint8_t id) {
    // Not implemented.

    (void)id;
    return nullptr;
}

Return<void> MockEvsEnumerator::getUltrasonicsArrayList(
        getUltrasonicsArrayList_cb _hidl_cb) {
    // Not implemented.

    (void)_hidl_cb;
    return {};
}

Return<sp<IEvsUltrasonicsArray>> MockEvsEnumerator::openUltrasonicsArray(
        const hidl_string& ultrasonicsArrayId) {
    // Not implemented.

    (void)ultrasonicsArrayId;
    return nullptr;
}

Return<void> MockEvsEnumerator::closeUltrasonicsArray(
        const ::android::sp<IEvsUltrasonicsArray>& evsUltrasonicsArray) {
    // Not implemented.

    (void)evsUltrasonicsArray;
    return {};
}

Return<void> MockEvsEnumerator::debug(
        const hidl_handle& fd, const hidl_vec<hidl_string>& options) {
    // Not implemented.

    (void)fd;
    (void)options;
    return {};
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
