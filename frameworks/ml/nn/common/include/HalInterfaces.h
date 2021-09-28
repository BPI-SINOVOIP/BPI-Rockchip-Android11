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

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_HAL_INTERFACES_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_HAL_INTERFACES_H

#include <android/hardware/neuralnetworks/1.0/IDevice.h>
#include <android/hardware/neuralnetworks/1.0/IExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.0/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.0/IPreparedModelCallback.h>
#include <android/hardware/neuralnetworks/1.0/types.h>
#include <android/hardware/neuralnetworks/1.1/IDevice.h>
#include <android/hardware/neuralnetworks/1.1/types.h>
#include <android/hardware/neuralnetworks/1.2/IDevice.h>
#include <android/hardware/neuralnetworks/1.2/IExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.2/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.2/IPreparedModelCallback.h>
#include <android/hardware/neuralnetworks/1.2/types.h>
#include <android/hardware/neuralnetworks/1.3/IDevice.h>
#include <android/hardware/neuralnetworks/1.3/IExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.3/IFencedExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.3/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.3/IPreparedModelCallback.h>
#include <android/hardware/neuralnetworks/1.3/types.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <hidlmemory/mapping.h>

#include <functional>

namespace android::nn::hal {

using android::sp;

using hardware::hidl_death_recipient;
using hardware::hidl_enum_range;
using hardware::hidl_handle;
using hardware::hidl_memory;
using hardware::hidl_string;
using hardware::hidl_vec;
using hardware::Return;
using hardware::Void;

using hidl::memory::V1_0::IMemory;

namespace V1_0 = hardware::neuralnetworks::V1_0;
namespace V1_1 = hardware::neuralnetworks::V1_1;
namespace V1_2 = hardware::neuralnetworks::V1_2;
namespace V1_3 = hardware::neuralnetworks::V1_3;

using V1_0::DataLocation;
using V1_0::DeviceStatus;
using V1_0::FusedActivationFunc;
using V1_0::PerformanceInfo;
using V1_0::RequestArgument;
using V1_1::ExecutionPreference;
using V1_2::Constant;
using V1_2::DeviceType;
using V1_2::Extension;
using V1_2::MeasureTiming;
using V1_2::OutputShape;
using V1_2::SymmPerChannelQuantParams;
using V1_2::Timing;
using V1_3::BufferDesc;
using V1_3::BufferRole;
using V1_3::Capabilities;
using V1_3::ErrorStatus;
using V1_3::IBuffer;
using V1_3::IDevice;
using V1_3::IExecutionCallback;
using V1_3::IFencedExecutionCallback;
using V1_3::IPreparedModel;
using V1_3::IPreparedModelCallback;
using V1_3::LoopTimeoutDurationNs;
using V1_3::Model;
using V1_3::Operand;
using V1_3::OperandLifeTime;
using V1_3::OperandType;
using V1_3::OperandTypeRange;
using V1_3::Operation;
using V1_3::OperationType;
using V1_3::OperationTypeRange;
using V1_3::OptionalTimeoutDuration;
using V1_3::OptionalTimePoint;
using V1_3::Priority;
using V1_3::Request;
using V1_3::Subgraph;
using ExtensionNameAndPrefix = V1_2::Model::ExtensionNameAndPrefix;
using ExtensionTypeEncoding = V1_2::Model::ExtensionTypeEncoding;
using OperandExtraParams = V1_2::Operand::ExtraParams;

using CacheToken =
        hardware::hidl_array<uint8_t, static_cast<uint32_t>(Constant::BYTE_SIZE_OF_CACHE_TOKEN)>;
using DeviceFactory = std::function<sp<V1_0::IDevice>(bool blocking)>;
using ModelFactory = std::function<Model()>;

inline constexpr Priority kDefaultPriority = Priority::MEDIUM;

}  // namespace android::nn::hal

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_HAL_INTERFACES_H
