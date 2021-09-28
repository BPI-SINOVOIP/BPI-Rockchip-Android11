/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "CompilationBuilder"

#include "CompilationBuilder.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "BurstBuilder.h"
#include "ExecutionBuilder.h"
#include "ExecutionBurstController.h"
#include "ExecutionPlan.h"
#include "Manager.h"
#include "ModelBuilder.h"
#include "Utils.h"

namespace android {
namespace nn {

using namespace hal;

CompilationBuilder::CompilationBuilder(const ModelBuilder* model,
                                       const std::vector<std::shared_ptr<Device>>& devices,
                                       bool explicitDeviceList)
    : mModel(model),
      mPartitioning(explicitDeviceList ? DeviceManager::kPartitioningWithoutFallback
                                       : DeviceManager::get()->getPartitioning()),
      mDevices(devices),
      mExplicitDeviceList(explicitDeviceList) {
    VLOG(COMPILATION) << "CompilationBuilder::CompilationBuilder";
}

int CompilationBuilder::finish() {
    if (mFinished) {
        LOG(ERROR) << "ANeuralNetworksCompilation_finish called more than once";
        return ANEURALNETWORKS_BAD_STATE;
    }
    // TODO validate the rest

    const auto deadline = makeDeadline(mTimeoutDuration);

    mFinished = true;
    if (mIsCacheInfoProvided) {
        mPlan.setCaching(&mCacheDir, mToken);
    }
    if (mPartitioning) {
        int n = mModel->partitionTheWork(mDevices, mPreference, mPriority, deadline, &mPlan);
        switch (n) {
            case ANEURALNETWORKS_NO_ERROR:
                return n;
            case ANEURALNETWORKS_UNEXPECTED_NULL:
            case ANEURALNETWORKS_BAD_DATA:
                // The two error codes above should only be used for errors in the user's
                // request. In case of a user error, we won't try any fallback.
                // TODO: Document this in NeuralNetworks.h and in the HAL. Make sure
                // driver writers know which code they can return.
                return n;
            default:
                // The error might be recoverable. Return the error only if falling back
                // is not allowed.
                if (!DeviceManager::partitioningAllowsFallback(mPartitioning)) {
                    return n;
                }
                if (mModel->hasOEMOperation()) {
                    LOG(ERROR) << "Cannot fall back to CPU because of an OEM operation";
                    return n;
                }
                if (mModel->hasExtensionOperation()) {
                    LOG(ERROR) << "Cannot fall back to CPU because of an extension operation";
                    return n;
                }
                break;
        }
    }

    // Fallback to CPU
    VLOG(COMPILATION) << "CompilationBuilder::finish with CPU fallback";
    mPlan.reset();
    mPlan.becomeSingleStep(DeviceManager::getCpuDevice(), mModel);
    return mPlan.finish(mPreference, mPriority, deadline);
}

int CompilationBuilder::setPreference(int32_t preference) {
    if (mFinished) {
        LOG(ERROR) << "ANeuralNetworksCompilation_setPreference can't modify after compilation "
                      "finished";
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (preference >= kNumberOfPreferences) {
        LOG(ERROR) << "ANeuralNetworksCompilation_setPreference invalid preference " << preference;
        return ANEURALNETWORKS_BAD_DATA;
    }

    mPreference = preference;
    return ANEURALNETWORKS_NO_ERROR;
}

int CompilationBuilder::setCaching(const std::string& cacheDir, const uint8_t* token) {
    if (mFinished) {
        LOG(ERROR)
                << "ANeuralNetworksCompilation_setCaching can't modify after compilation finished";
        return ANEURALNETWORKS_BAD_STATE;
    }
    mCacheDir = cacheDir;
    // Make sure the cache dir can concat with the filename.
    if (!mCacheDir.empty() && mCacheDir.back() != '/') {
        mCacheDir.push_back('/');
    }
    std::copy(token, token + ANEURALNETWORKS_BYTE_SIZE_OF_CACHE_TOKEN, mToken);
    mIsCacheInfoProvided = true;
    return ANEURALNETWORKS_NO_ERROR;
}

int CompilationBuilder::setPriority(int32_t priority) {
    if (mFinished) {
        LOG(ERROR) << "ANeuralNetworksCompilation_setPriority can't modify after compilation "
                      "finished";
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (priority != ANEURALNETWORKS_PRIORITY_LOW && priority != ANEURALNETWORKS_PRIORITY_MEDIUM &&
        priority != ANEURALNETWORKS_PRIORITY_HIGH) {
        LOG(ERROR) << "ANeuralNetworksCompilation_setPriority invalid priority " << priority;
        return ANEURALNETWORKS_BAD_DATA;
    }

    mPriority = priority;
    return ANEURALNETWORKS_NO_ERROR;
}

int CompilationBuilder::setTimeoutDuration(uint64_t duration) {
    if (mFinished) {
        LOG(ERROR) << "ANeuralNetworksCompilation_setTimeout can't modify after compilation "
                      "finished";
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (!mExplicitDeviceList || (mDevices.size() != 1)) {
        LOG(ERROR) << "ANeuralNetworksCompilation_setTimeout called on an "
                      "ANeuralNetworksCompilation that was not created by "
                      "ANeuralNetworksCompilation_createForDevices with numDevices = 1";
        return ANEURALNETWORKS_BAD_DATA;
    }
    if (duration > 0) {
        mTimeoutDuration = duration;
    } else {
        mTimeoutDuration.reset();
    }
    return ANEURALNETWORKS_NO_ERROR;
}

int CompilationBuilder::setPartitioning(uint32_t partitioning) {
    if (mFinished) {
        LOG(ERROR) << "ANeuralNetworksCompilation_setPartitioning can't modify after compilation "
                      "finished";
        return ANEURALNETWORKS_BAD_STATE;
    }

    mPartitioning = partitioning;
    return ANEURALNETWORKS_NO_ERROR;
}

int CompilationBuilder::createExecution(ExecutionBuilder** execution) {
    if (!mFinished) {
        LOG(ERROR) << "ANeuralNetworksExecution_create passed an unfinished compilation";
        *execution = nullptr;
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (!mPlan.isValid()) {
        LOG(ERROR) << "ANeuralNetworksExecution_create passed an invalid compilation";
        *execution = nullptr;
        return ANEURALNETWORKS_BAD_STATE;
    }
    *execution = new (std::nothrow) ExecutionBuilder(this);
    return (*execution ? ANEURALNETWORKS_NO_ERROR : ANEURALNETWORKS_OUT_OF_MEMORY);
}

int CompilationBuilder::createBurst(BurstBuilder** burst) {
    if (!mFinished) {
        LOG(ERROR) << "ANeuralNetworksBurst_create passed an unfinished compilation";
        *burst = nullptr;
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (!mPlan.isValid()) {
        LOG(ERROR) << "ANeuralNetworksBurst_create passed an invalid compilation";
        *burst = nullptr;
        return ANEURALNETWORKS_BAD_STATE;
    }
    std::vector<std::shared_ptr<ExecutionBurstController>> burstControllers =
            mPlan.makeBursts(mPreference);
    *burst = new (std::nothrow) BurstBuilder(this, std::move(burstControllers));
    return (*burst ? ANEURALNETWORKS_NO_ERROR : ANEURALNETWORKS_OUT_OF_MEMORY);
}

int CompilationBuilder::forEachStepRoleOfInput(uint32_t index,
                                               const StepRoleCallback& callback) const {
    if (!mFinished) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_addInputRole passed an unfinished compilation";
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (!mPlan.isValid()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_addInputRole passed an invalid compilation";
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (index >= mModel->inputCount()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_addInputRole passed an invalid input index "
                   << index;
        return ANEURALNETWORKS_BAD_DATA;
    }
    mPlan.forEachStepRoleOfInput(index, callback);
    return ANEURALNETWORKS_NO_ERROR;
}

int CompilationBuilder::forEachStepRoleOfOutput(uint32_t index,
                                                const StepRoleCallback& callback) const {
    if (!mFinished) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_addOutputRole passed an unfinished compilation";
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (!mPlan.isValid()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_addOutputRole passed an invalid compilation";
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (index >= mModel->outputCount()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_addOutputRole passed an invalid output index "
                   << index;
        return ANEURALNETWORKS_BAD_DATA;
    }
    mPlan.forEachStepRoleOfOutput(index, callback);
    return ANEURALNETWORKS_NO_ERROR;
}

}  // namespace nn
}  // namespace android
