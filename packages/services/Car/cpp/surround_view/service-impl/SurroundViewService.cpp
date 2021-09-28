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

#include <android-base/logging.h>

#include "SurroundViewService.h"

using namespace android_auto::surround_view;

namespace android {
namespace hardware {
namespace automotive {
namespace sv {
namespace V1_0 {
namespace implementation {

std::mutex SurroundViewService::sLock;
sp<SurroundViewService> SurroundViewService::sService;
sp<SurroundView2dSession> SurroundViewService::sSurroundView2dSession;
sp<SurroundView3dSession> SurroundViewService::sSurroundView3dSession;

const std::string kCameraIds[] = {"0", "1", "2", "3"};
static const int kVhalUpdateRate = 10;

SurroundViewService::SurroundViewService() :
      mVhalHandler(nullptr), mAnimationModule(nullptr), mIOModule(nullptr) {
    mVhalHandler = new VhalHandler();
    mIOModule = new IOModule("/vendor/etc/automotive/sv/sv_sample_config.xml");
}

SurroundViewService::~SurroundViewService() {
    if (mVhalHandler != nullptr) {
        delete mVhalHandler;
    }

    if (mIOModule != nullptr) {
        delete mIOModule;
    }

    if (mAnimationModule != nullptr) {
        delete mAnimationModule;
    }
}

sp<SurroundViewService> SurroundViewService::getInstance() {
    std::scoped_lock<std::mutex> lock(sLock);
    if (sService == nullptr) {
        sService = new SurroundViewService();
        if (!sService->initialize()) {
            LOG(ERROR) << "Cannot initialize the service properly";
            sService = nullptr;
            return nullptr;
        }
    }
    return sService;
}

std::vector<uint64_t> getAnimationPropertiesToRead(const AnimationConfig& animationConfig) {
    std::set<uint64_t> propertiesSet;
    for(const auto& animation: animationConfig.animations) {
        for(const auto& opPair : animation.gammaOpsMap) {
            propertiesSet.insert(opPair.first);
        }

        for(const auto& opPair : animation.textureOpsMap) {
            propertiesSet.insert(opPair.first);
        }

        for(const auto& opPair : animation.rotationOpsMap) {
            propertiesSet.insert(opPair.first);
        }

        for(const auto& opPair : animation.translationOpsMap) {
            propertiesSet.insert(opPair.first);
        }
    }
    std::vector<uint64_t> propertiesToRead;
    propertiesToRead.assign(propertiesSet.begin(), propertiesSet.end());
    return propertiesToRead;
}

bool SurroundViewService::initialize() {
    // Get the EVS manager service
    LOG(INFO) << "Acquiring EVS Enumerator";
    mEvs = IEvsEnumerator::getService("default");
    if (mEvs == nullptr) {
        LOG(ERROR) << "getService returned NULL.  Exiting.";
        return false;
    }

    IOStatus status = mIOModule->initialize();
    if (status != IOStatus::OK) {
        LOG(ERROR) << "IO Module cannot be initialized properly";
        return false;
    }

    if (!mIOModule->getConfig(&mConfig)) {
        LOG(ERROR) << "Cannot parse Car Config file properly";
        return false;
    }

    // Since we only keep one instance of the SurroundViewService and initialize
    // method is always called after the constructor, it is safe to put the
    // allocation here and the de-allocation in service's constructor.
    mAnimationModule = new AnimationModule(
            mConfig.carModelConfig.carModel.partsMap,
            mConfig.carModelConfig.carModel.texturesMap,
            mConfig.carModelConfig.animationConfig.animations);

    // Initialize the VHal Handler with update method and rate.
    // TODO(b/157498592): The update rate should align with the EVS camera
    // update rate.
    if (mVhalHandler->initialize(VhalHandler::GET, kVhalUpdateRate)) {
        // Initialize the vhal handler properties to read.
        std::vector<uint64_t> propertiesToRead;

        // Add animation properties to read if 3d and animations are enabled.
        if (mConfig.sv3dConfig.sv3dEnabled && mConfig.sv3dConfig.sv3dAnimationsEnabled) {
            const std::vector<uint64_t> animationPropertiesToRead =
                    getAnimationPropertiesToRead(mConfig.carModelConfig.animationConfig);
            propertiesToRead.insert(propertiesToRead.end(), animationPropertiesToRead.begin(),
                    animationPropertiesToRead.end());
        }

        // Call vhal handler setPropertiesToRead with all properties.
        if (!mVhalHandler->setPropertiesToRead(propertiesToRead)) {
            LOG(WARNING) << "VhalHandler setPropertiesToRead failed.";
        }
    } else {
        LOG(WARNING) << "VhalHandler cannot be initialized properly";
    }

    return true;
}

Return<void> SurroundViewService::getCameraIds(getCameraIds_cb _hidl_cb) {
    hidl_vec<hidl_string> cameraIds = {kCameraIds[0], kCameraIds[1],
        kCameraIds[2], kCameraIds[3]};
    _hidl_cb(cameraIds);
    return {};
}

Return<void> SurroundViewService::start2dSession(start2dSession_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(sLock);

    if (sSurroundView2dSession != nullptr) {
        LOG(WARNING) << "Only one 2d session is supported at the same time";
        _hidl_cb(nullptr, SvResult::INTERNAL_ERROR);
    } else {
        sSurroundView2dSession = new SurroundView2dSession(mEvs, &mConfig);
        if (sSurroundView2dSession->initialize()) {
            _hidl_cb(sSurroundView2dSession, SvResult::OK);
        } else {
            _hidl_cb(nullptr, SvResult::INTERNAL_ERROR);
        }
    }
    return {};
}

Return<SvResult> SurroundViewService::stop2dSession(
    const sp<ISurroundView2dSession>& sv2dSession) {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(sLock);

    if (sv2dSession != nullptr && sv2dSession == sSurroundView2dSession) {
        sSurroundView2dSession = nullptr;
        return SvResult::OK;
    } else {
        LOG(ERROR) << __FUNCTION__ << ": Invalid argument";
        return SvResult::INVALID_ARG;
    }
}

Return<void> SurroundViewService::start3dSession(start3dSession_cb _hidl_cb) {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(sLock);

    if (sSurroundView3dSession != nullptr) {
        LOG(WARNING) << "Only one 3d session is supported at the same time";
        _hidl_cb(nullptr, SvResult::INTERNAL_ERROR);
    } else {
        sSurroundView3dSession = new SurroundView3dSession(mEvs,
                                                           mVhalHandler,
                                                           mAnimationModule,
                                                           &mConfig);
        if (sSurroundView3dSession->initialize()) {
            _hidl_cb(sSurroundView3dSession, SvResult::OK);
        } else {
            _hidl_cb(nullptr, SvResult::INTERNAL_ERROR);
        }
    }
    return {};
}

Return<SvResult> SurroundViewService::stop3dSession(
    const sp<ISurroundView3dSession>& sv3dSession) {
    LOG(DEBUG) << __FUNCTION__;
    std::scoped_lock<std::mutex> lock(sLock);

    if (sv3dSession != nullptr && sv3dSession == sSurroundView3dSession) {
        sSurroundView3dSession = nullptr;
        return SvResult::OK;
    } else {
        LOG(ERROR) << __FUNCTION__ << ": Invalid argument";
        return SvResult::INVALID_ARG;
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace sv
}  // namespace automotive
}  // namespace hardware
}  // namespace android
