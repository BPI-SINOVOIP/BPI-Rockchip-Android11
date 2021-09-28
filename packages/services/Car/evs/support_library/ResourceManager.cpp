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

#include "ResourceManager.h"

namespace android {
namespace automotive {
namespace evs {
namespace support {

using ::std::lock_guard;

const string ResourceManager::kDefaultServiceName = "default";
sp<ResourceManager> ResourceManager::sInstance;
mutex ResourceManager::sLockSingleton;
mutex ResourceManager::sLockEvs;
sp<IEvsEnumerator> ResourceManager::sEvs;

sp<IEvsEnumerator> ResourceManager::getEvsEnumerator(string serviceName) {
    lock_guard<mutex> lock(sLockEvs);
    if (sEvs.get() == nullptr) {
        sEvs = IEvsEnumerator::getService(serviceName);
    }
    return sEvs;
}

sp<ResourceManager> ResourceManager::getInstance() {
    lock_guard<mutex> lock(sLockSingleton);
    if (sInstance == nullptr) {
        ALOGD("Creating new ResourceManager instance");
        sInstance = new ResourceManager();
    }
    return sInstance;
}

sp<StreamHandler> ResourceManager::obtainStreamHandler(string pCameraId) {
    ALOGD("ResourceManager::obtainStreamHandler");

    // Lock for stream handler related methods.
    lock_guard<mutex> lock(mLockStreamHandler);

    auto result = mCameraInstances.find(pCameraId);
    if (result == mCameraInstances.end()) {
        sp<CameraInstance> instance = new CameraInstance();

        // CameraInstance::useCaseCount
        instance->useCaseCount++;

        // CameraInstance::cameraId
        instance->cameraId = pCameraId;

        // CameraInstance::camera
        instance->camera = getEvsEnumerator()->openCamera(pCameraId);
        if (instance->camera.get() == nullptr) {
            ALOGE("Failed to allocate new EVS Camera interface for %s",
                  pCameraId.c_str());
            return nullptr;
        }

        // CameraInstance::handler
        instance->handler = new StreamHandler(instance->camera);
        if (instance->handler == nullptr) {
            ALOGE("Failed to create stream handler for %s",
                  pCameraId.c_str());
        }

        // Move the newly-created instance into vector, and the vector takes
        // ownership of the instance.
        mCameraInstances.emplace(pCameraId, instance);

        return instance->handler;
    } else {
        auto instance = result->second;
        instance->useCaseCount++;

        return instance->handler;
    }
}

void ResourceManager::releaseStreamHandler(string pCameraId) {
    ALOGD("ResourceManager::releaseStreamHandler");

    // Lock for stream handler related methods.
    lock_guard<mutex> lock(mLockStreamHandler);

    auto result = mCameraInstances.find(pCameraId);
    if (result == mCameraInstances.end()) {
        ALOGW("No stream handler is active with camera id %s", pCameraId.c_str());
    } else {
        auto instance = result->second;
        instance->useCaseCount--;

        if (instance->useCaseCount <= 0) {
            // The vector keeps the only strong reference to the camera
            // instance. Once the instance is erased from the vector, the
            // override onLastStrongRef method for CameraInstance class will
            // be called and clean up the resources.
            mCameraInstances.erase(result);
        }
    }
}

// TODO(b/130246434): have further discussion about how the display resource
// should be managed.
sp<IEvsDisplay> ResourceManager::openDisplay() {
    // Lock for display related methods.
    lock_guard<mutex> lock(mLockDisplay);

    if (mDisplay.get() == nullptr) {
        mDisplay = getEvsEnumerator()->openDisplay();
        if (mDisplay.get() != nullptr) {
            ALOGD("Evs display is opened");
        } else {
            ALOGE("Failed to open evs display.");
        }
    }

    return mDisplay;
}

void ResourceManager::closeDisplay(sp<IEvsDisplay> pDisplay) {
    // Lock for display related methods.
    lock_guard<mutex> lock(mLockDisplay);

    // Even though there are logics in evs manager to prevent errors from
    // unrecognized IEvsDisplay object, we still want to check whether the
    // incoming pDisplay is the one we opened earlier in resource manager. So
    // when developer make mistakes by passing in incorrect IEvsDisplay object,
    // we know that we should not proceed and the active display is still
    // opened.
    if (mDisplay.get() == pDisplay.get()) {
        getEvsEnumerator()->closeDisplay(mDisplay);
        mDisplay = nullptr;
        ALOGD("Evs display is closed");
    } else {
        ALOGW("Ignored! Unrecognized display object for closeDisplay method");
    }
}

bool ResourceManager::isDisplayOpened() {
    // Lock for display related methods.
    lock_guard<mutex> lock(mLockDisplay);

    return mDisplay.get() != nullptr;
}

}  // namespace support
}  // namespace evs
}  // namespace automotive
}  // namespace android
