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

#define LOG_TAG "ModelArgumentInfo"

#include "ModelArgumentInfo.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "HalInterfaces.h"
#include "NeuralNetworks.h"
#include "TypeManager.h"
#include "Utils.h"

namespace android {
namespace nn {

using namespace hal;

static const std::pair<int, ModelArgumentInfo> kBadDataModelArgumentInfo{ANEURALNETWORKS_BAD_DATA,
                                                                         {}};

std::pair<int, ModelArgumentInfo> ModelArgumentInfo::createFromPointer(
        const Operand& operand, const ANeuralNetworksOperandType* type, void* data,
        uint32_t length) {
    if ((data == nullptr) != (length == 0)) {
        const char* dataPtrMsg = data ? "NOT_NULLPTR" : "NULLPTR";
        LOG(ERROR) << "Data pointer must be nullptr if and only if length is zero (data = "
                   << dataPtrMsg << ", length = " << length << ")";
        return kBadDataModelArgumentInfo;
    }

    ModelArgumentInfo ret;
    if (data == nullptr) {
        ret.mState = ModelArgumentInfo::HAS_NO_VALUE;
    } else {
        if (int n = ret.updateDimensionInfo(operand, type)) {
            return {n, ModelArgumentInfo()};
        }
        if (operand.type != OperandType::OEM) {
            uint32_t neededLength =
                    TypeManager::get()->getSizeOfData(operand.type, ret.mDimensions);
            if (neededLength != length && neededLength != 0) {
                LOG(ERROR) << "Setting argument with invalid length: " << length
                           << ", expected length: " << neededLength;
                return kBadDataModelArgumentInfo;
            }
        }
        ret.mState = ModelArgumentInfo::POINTER;
    }
    ret.mBuffer = data;
    ret.mLocationAndLength = {.poolIndex = 0, .offset = 0, .length = length};
    return {ANEURALNETWORKS_NO_ERROR, ret};
}

std::pair<int, ModelArgumentInfo> ModelArgumentInfo::createFromMemory(
        const Operand& operand, const ANeuralNetworksOperandType* type, uint32_t poolIndex,
        uint32_t offset, uint32_t length) {
    ModelArgumentInfo ret;
    if (int n = ret.updateDimensionInfo(operand, type)) {
        return {n, ModelArgumentInfo()};
    }
    const bool isMemorySizeKnown = offset != 0 || length != 0;
    if (isMemorySizeKnown && operand.type != OperandType::OEM) {
        const uint32_t neededLength =
                TypeManager::get()->getSizeOfData(operand.type, ret.mDimensions);
        if (neededLength != length && neededLength != 0) {
            LOG(ERROR) << "Setting argument with invalid length: " << length
                       << " (offset: " << offset << "), expected length: " << neededLength;
            return kBadDataModelArgumentInfo;
        }
    }

    ret.mState = ModelArgumentInfo::MEMORY;
    ret.mLocationAndLength = {.poolIndex = poolIndex, .offset = offset, .length = length};
    ret.mBuffer = nullptr;
    return {ANEURALNETWORKS_NO_ERROR, ret};
}

int ModelArgumentInfo::updateDimensionInfo(const Operand& operand,
                                           const ANeuralNetworksOperandType* newType) {
    if (newType == nullptr) {
        mDimensions = operand.dimensions;
    } else {
        const uint32_t count = newType->dimensionCount;
        mDimensions = hidl_vec<uint32_t>(count);
        std::copy(&newType->dimensions[0], &newType->dimensions[count], mDimensions.begin());
    }
    return ANEURALNETWORKS_NO_ERROR;
}

hidl_vec<RequestArgument> createRequestArguments(
        const std::vector<ModelArgumentInfo>& argumentInfos,
        const std::vector<DataLocation>& ptrArgsLocations) {
    const size_t count = argumentInfos.size();
    hidl_vec<RequestArgument> ioInfos(count);
    uint32_t ptrArgsIndex = 0;
    for (size_t i = 0; i < count; i++) {
        const auto& info = argumentInfos[i];
        switch (info.state()) {
            case ModelArgumentInfo::POINTER:
                ioInfos[i] = {.hasNoValue = false,
                              .location = ptrArgsLocations[ptrArgsIndex++],
                              .dimensions = info.dimensions()};
                break;
            case ModelArgumentInfo::MEMORY:
                ioInfos[i] = {.hasNoValue = false,
                              .location = info.locationAndLength(),
                              .dimensions = info.dimensions()};
                break;
            case ModelArgumentInfo::HAS_NO_VALUE:
                ioInfos[i] = {.hasNoValue = true};
                break;
            default:
                CHECK(false);
        };
    }
    return ioInfos;
}

}  // namespace nn
}  // namespace android
