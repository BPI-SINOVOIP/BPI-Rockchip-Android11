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

#define LOG_TAG "Manager"

#include "Manager.h"

#include <android/hidl/manager/1.2/IServiceManager.h>
#include <android/sync.h>
#include <build/version.h>
#include <cutils/native_handle.h>
#include <hidl/HidlTransportSupport.h>
#include <hidl/ServiceManagement.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Callbacks.h"
#include "CpuExecutor.h"
#include "ExecutionBurstController.h"
#include "HalInterfaces.h"
#include "Memory.h"
#include "MetaModel.h"
#include "ModelArgumentInfo.h"
#include "Tracing.h"
#include "TypeManager.h"
#include "Utils.h"
#include "VersionedInterfaces.h"

namespace android {
namespace nn {

using namespace hal;

const Timing kNoTiming = {.timeOnDevice = UINT64_MAX, .timeInDriver = UINT64_MAX};

// A Device with actual underlying driver
class DriverDevice : public Device {
   public:
    // Create a DriverDevice from a name and a DeviceFactory function.
    // Returns nullptr on failure.
    static std::shared_ptr<DriverDevice> create(const std::string& name,
                                                const DeviceFactory& makeDevice);

    // Prefer using DriverDevice::create
    DriverDevice(std::shared_ptr<VersionedIDevice> device);

    const std::string& getName() const override { return kInterface->getName(); }
    const std::string& getVersionString() const override { return kInterface->getVersionString(); }
    int64_t getFeatureLevel() const override { return kInterface->getFeatureLevel(); }
    int32_t getType() const override { return kInterface->getType(); }
    const std::vector<Extension>& getSupportedExtensions() const override {
        return kInterface->getSupportedExtensions();
    }
    std::vector<bool> getSupportedOperations(const MetaModel& metaModel) const override;
    PerformanceInfo getPerformance(OperandType type) const override {
        const auto& capabilities = kInterface->getCapabilities();
        return lookup(capabilities.operandPerformance, type);
    }
    PerformanceInfo getRelaxedFloat32toFloat16PerformanceScalar() const override {
        const auto& capabilities = kInterface->getCapabilities();
        return capabilities.relaxedFloat32toFloat16PerformanceScalar;
    }
    PerformanceInfo getRelaxedFloat32toFloat16PerformanceTensor() const override {
        const auto& capabilities = kInterface->getCapabilities();
        return capabilities.relaxedFloat32toFloat16PerformanceTensor;
    }
    PerformanceInfo getIfPerformance() const override {
        const auto& capabilities = kInterface->getCapabilities();
        return capabilities.ifPerformance;
    }
    PerformanceInfo getWhilePerformance() const override {
        const auto& capabilities = kInterface->getCapabilities();
        return capabilities.whilePerformance;
    }
    bool isCachingSupported() const override {
        // Caching is supported if either of numModelCache or numDataCache is greater than 0.
        const auto [numModelCacheFiles, numDataCacheFiles] =
                kInterface->getNumberOfCacheFilesNeeded();
        return numModelCacheFiles > 0 || numDataCacheFiles > 0;
    }
    int wait() const override { return kInterface->wait(); }

    std::pair<int, std::shared_ptr<PreparedModel>> prepareModel(
            const ModelFactory& makeModel, ExecutionPreference preference, Priority priority,
            const std::optional<Deadline>& deadline, const std::string& cacheDir,
            const std::optional<CacheToken>& maybeToken) const override;

    std::pair<int, std::unique_ptr<Memory>> allocate(const MemoryDescriptor& desc,
                                                     hal::OperandType) const override;

   private:
    const std::shared_ptr<VersionedIDevice> kInterface;

#ifdef NN_DEBUGGABLE
    // For debugging: behavior of IDevice::getSupportedOperations for SampleDriver.
    // 0 - all operations reported by IDevice::getSupportedOperations() supported
    // 1 - some operations reported by IDevice::getSupportedOperations() supported
    uint32_t mSupported = 0;
#endif  // NN_DEBUGGABLE
};

// A PreparedModel with underlying IPreparedModel instance return by actual driver.
class DriverPreparedModel : public PreparedModel {
   public:
    DriverPreparedModel(const Device* device,
                        const std::shared_ptr<VersionedIPreparedModel>& preparedModel)
        : mDevice(device), mPreparedModel(preparedModel) {
        CHECK(mDevice != nullptr);
        CHECK(mPreparedModel != nullptr);
    }

    const Device* getDevice() const override { return mDevice; }
    std::shared_ptr<VersionedIPreparedModel> getInterface() const override {
        return mPreparedModel;
    }
    std::tuple<int, std::vector<OutputShape>, Timing> execute(
            const std::vector<ModelArgumentInfo>& inputs,
            const std::vector<ModelArgumentInfo>& outputs,
            const std::vector<const Memory*>& memories,
            const std::shared_ptr<ExecutionBurstController>& burstController, MeasureTiming measure,
            const std::optional<Deadline>& deadline,
            const OptionalTimeoutDuration& loopTimeoutDuration) const override;

    std::tuple<int, int, sp<hal::IFencedExecutionCallback>, hal::Timing> executeFenced(
            const std::vector<ModelArgumentInfo>& inputs,
            const std::vector<ModelArgumentInfo>& outputs,
            const std::vector<const Memory*>& memories, const std::vector<int>& waitFor,
            MeasureTiming measure, const std::optional<Deadline>& deadline,
            const OptionalTimeoutDuration& loopTimeoutDuration,
            const hal::OptionalTimeoutDuration& timeoutDurationAfterFence) const override;

    std::shared_ptr<ExecutionBurstController> configureExecutionBurst(
            bool preferPowerOverLatency) const override {
        return mPreparedModel->configureExecutionBurst(preferPowerOverLatency);
    }

   private:
    const Device* mDevice;
    const std::shared_ptr<VersionedIPreparedModel> mPreparedModel;
};

DriverDevice::DriverDevice(std::shared_ptr<VersionedIDevice> device)
    : kInterface(std::move(device)) {
    CHECK(kInterface != nullptr);
#ifdef NN_DEBUGGABLE
    static const char samplePrefix[] = "sample";
    if (getName().substr(0, sizeof(samplePrefix) - 1) == samplePrefix) {
        mSupported = getProp("debug.nn.sample.supported");
    }
#endif  // NN_DEBUGGABLE
}

std::shared_ptr<DriverDevice> DriverDevice::create(const std::string& name,
                                                   const DeviceFactory& makeDevice) {
    CHECK(makeDevice != nullptr);
    std::shared_ptr<VersionedIDevice> device = VersionedIDevice::create(name, makeDevice);
    if (device == nullptr) {
        LOG(ERROR) << "DriverDevice::create failed to create VersionedIDevice object for service "
                   << name;
        return nullptr;
    }

    return std::make_shared<DriverDevice>(std::move(device));
}

std::vector<bool> DriverDevice::getSupportedOperations(const MetaModel& metaModel) const {
    // Query the driver for what it can do.
    ErrorStatus status = ErrorStatus::GENERAL_FAILURE;
    std::vector<bool> supportedOperations;
    std::tie(status, supportedOperations) = kInterface->getSupportedOperations(metaModel);

    const Model& hidlModel = metaModel.getModel();
    const uint32_t operationCount = hidlModel.main.operations.size();
    if (status != ErrorStatus::NONE) {
        LOG(ERROR) << "IDevice::getSupportedOperations returned the error " << toString(status);
        // Set the supported operation vectors to all false, so we won't use this driver.
        return std::vector<bool>(operationCount, false);
    }
    if (supportedOperations.size() != operationCount) {
        LOG(ERROR) << "IDevice::getSupportedOperations returned a vector of length "
                   << supportedOperations.size() << " when expecting " << operationCount;
        // Set the supported operation vectors to all false, so we won't use this driver.
        return std::vector<bool>(operationCount, false);
    }

#ifdef NN_DEBUGGABLE
    if (mSupported != 1) {
        return supportedOperations;
    }

    const uint32_t baseAccumulator = std::hash<std::string>{}(getName());
    for (size_t operationIndex = 0; operationIndex < supportedOperations.size(); operationIndex++) {
        if (!supportedOperations[operationIndex]) {
            continue;
        }

        uint32_t accumulator = baseAccumulator;
        const Operation& operation = hidlModel.main.operations[operationIndex];
        accumulator ^= static_cast<uint32_t>(operation.type);
        auto accumulateOperands = [&hidlModel, &accumulator](const hidl_vec<uint32_t>& operands) {
            for (uint32_t operandIndex : operands) {
                const Operand& operand = hidlModel.main.operands[operandIndex];
                accumulator ^= static_cast<uint32_t>(operand.type);
                accumulator ^= operand.dimensions.size();
                for (uint32_t dimension : operand.dimensions) {
                    accumulator ^= dimension;
                    if (operand.lifetime == OperandLifeTime::CONSTANT_COPY ||
                        operand.lifetime == OperandLifeTime::CONSTANT_REFERENCE) {
                        accumulator ^= 1;
                    }
                }
            }
        };
        accumulateOperands(operation.inputs);
        accumulateOperands(operation.outputs);
        if (accumulator & 1) {
            supportedOperations[operationIndex] = false;
        }
    }
#endif  // NN_DEBUGGABLE

    return supportedOperations;
}

std::pair<int, std::shared_ptr<PreparedModel>> DriverDevice::prepareModel(
        const ModelFactory& makeModel, ExecutionPreference preference, Priority priority,
        const std::optional<Deadline>& deadline, const std::string& cacheDir,
        const std::optional<CacheToken>& maybeToken) const {
    const auto [n, preparedModel] = kInterface->prepareModel(makeModel, preference, priority,
                                                             deadline, cacheDir, maybeToken);
    if (n != ANEURALNETWORKS_NO_ERROR) {
        return {n, nullptr};
    }
    CHECK(preparedModel != nullptr) << "prepareModel returned nullptr without error code";
    return {ANEURALNETWORKS_NO_ERROR, std::make_shared<DriverPreparedModel>(this, preparedModel)};
}

std::pair<int, std::unique_ptr<Memory>> DriverDevice::allocate(const MemoryDescriptor& desc,
                                                               hal::OperandType) const {
    const BufferDesc hidlDesc = {.dimensions = desc.dimensions};
    std::vector<std::shared_ptr<VersionedIPreparedModel>> preparedModels(
            desc.preparedModels.size());
    std::transform(desc.preparedModels.begin(), desc.preparedModels.end(), preparedModels.begin(),
                   [](const auto* preparedModel) {
                       const auto versionedPreparedModel = preparedModel->getInterface();
                       CHECK(versionedPreparedModel != nullptr);
                       return versionedPreparedModel;
                   });
    auto [status, buffer, token] =
            kInterface->allocate(hidlDesc, preparedModels, desc.inputRoles, desc.outputRoles);
    if (status != ErrorStatus::NONE) {
        LOG(ERROR) << "DriverDevice::allocate -- memory allocation on device " << getName()
                   << " failed!";
        return {convertErrorStatusToResultCode(status), nullptr};
    }
    return MemoryFromDevice::create(std::move(buffer), token);
}

// Figures out how to place each of the input or outputs in a buffer. This just
// does the layout and memory allocation, it does not copy data.  Aligns each
// input a bit.
static std::tuple<int, std::unique_ptr<MemoryAshmem>, std::vector<DataLocation>>
allocatePointerArgumentsToPool(const std::vector<ModelArgumentInfo>& args,
                               std::vector<const Memory*>* memories) {
    CHECK(memories != nullptr);
    std::vector<DataLocation> ptrArgsLocations;
    const uint32_t nextPoolIndex = memories->size();
    int64_t total = 0;
    for (const auto& info : args) {
        if (info.state() == ModelArgumentInfo::POINTER) {
            // TODO Good enough alignment?
            total += alignBytesNeeded(static_cast<uint32_t>(total), info.length());
            ptrArgsLocations.push_back({.poolIndex = nextPoolIndex,
                                        .offset = static_cast<uint32_t>(total),
                                        .length = info.length()});
            total += info.length();
        }
    };
    if (total > 0xFFFFFFFF) {
        LOG(ERROR) << "allocatePointerArgumentsToPool: ANeuralNetworksExecution: Size of all "
                      "inputs or outputs exceeds 2^32.";
        return {ANEURALNETWORKS_BAD_DATA, nullptr, std::vector<DataLocation>{}};
    }
    if (total <= 0) {
        return {ANEURALNETWORKS_NO_ERROR, nullptr, std::vector<DataLocation>{}};
    }
    auto [n, memory] = MemoryAshmem::create(total);
    if (n != ANEURALNETWORKS_NO_ERROR) {
        return {n, nullptr, std::vector<DataLocation>{}};
    }
    memories->push_back(memory.get());
    return {ANEURALNETWORKS_NO_ERROR, std::move(memory), std::move(ptrArgsLocations)};
}

// Perform computation on an actual HIDL driver.
//
// Because HIDL cannot take raw pointers, two separate memory pools will be allocated for inputs and
// outputs specified by pointers. The input pointer data will be copied to the input pool prior to
// execution, and the output pointer data will be copied out from the output pool after the
// execution.
//
// The HIDL invocation will choose between sync/async execution according to
// DeviceManager::mSyncExecHal.
std::tuple<int, std::vector<OutputShape>, Timing> DriverPreparedModel::execute(
        const std::vector<ModelArgumentInfo>& inputs, const std::vector<ModelArgumentInfo>& outputs,
        const std::vector<const Memory*>& memories,
        const std::shared_ptr<ExecutionBurstController>& burstController, MeasureTiming measure,
        const std::optional<Deadline>& deadline,
        const OptionalTimeoutDuration& loopTimeoutDuration) const {
    NNTRACE_RT(NNTRACE_PHASE_INPUTS_AND_OUTPUTS, "DriverPreparedModel::execute");

    // Make a copy of the memory tracker as we will append memory pools for pointer arguments.
    std::vector<const Memory*> localMemories = memories;

    // We separate the input & output pools so accelerators only need to copy
    // the contents of the input pools. We could also use it to set protection
    // on read only memory but that's not currently done.

    // Layout the input and output data
    const auto [n1, inputPtrArgsMemory, inputPtrArgsLocations] =
            allocatePointerArgumentsToPool(inputs, &localMemories);
    if (n1 != ANEURALNETWORKS_NO_ERROR) {
        return {n1, {}, kNoTiming};
    }
    const auto [n2, outputPtrArgsMemory, outputPtrArgsLocations] =
            allocatePointerArgumentsToPool(outputs, &localMemories);
    if (n2 != ANEURALNETWORKS_NO_ERROR) {
        return {n2, {}, kNoTiming};
    }

    // Copy the input data that was specified via a pointer.
    if (inputPtrArgsMemory != nullptr) {
        uint32_t ptrInputIndex = 0;
        for (const auto& info : inputs) {
            if (info.state() == ModelArgumentInfo::POINTER) {
                const DataLocation& loc = inputPtrArgsLocations[ptrInputIndex++];
                uint8_t* const data = inputPtrArgsMemory->getPointer();
                memcpy(data + loc.offset, info.buffer(), loc.length);
            }
        }
    }

    Request request;
    request.inputs = createRequestArguments(inputs, inputPtrArgsLocations);
    request.outputs = createRequestArguments(outputs, outputPtrArgsLocations);
    uint32_t count = localMemories.size();
    request.pools.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        request.pools[i] = localMemories[i]->getMemoryPool();
    }

    NNTRACE_FULL_SWITCH(NNTRACE_LAYER_IPC, NNTRACE_PHASE_EXECUTION,
                        "DriverPreparedModel::execute::execute");

    int n = ANEURALNETWORKS_OP_FAILED;
    std::vector<OutputShape> outputShapes;
    Timing timing = kNoTiming;

    // compute using burst if present
    const bool burstCompute = (burstController != nullptr);
    bool burstFallback = true;
    if (burstCompute) {
        const bool compliant = compliantWithV1_2(request);
        if (compliant) {
            V1_0::Request request12 = convertToV1_2(request);
            std::vector<intptr_t> memoryIds;
            memoryIds.reserve(localMemories.size());
            for (const Memory* memory : localMemories) {
                memory->usedBy(burstController);
                memoryIds.push_back(memory->getKey());
            }

            VLOG(EXECUTION) << "Before ExecutionBurstController->compute() "
                            << SHOW_IF_DEBUG(toString(request12));
            std::tie(n, outputShapes, timing, burstFallback) =
                    burstController->compute(request12, measure, memoryIds);
        }
    }

    // compute from IPreparedModel if either:
    // (1) burst was not supplied, or
    // (2) the burst execution failed and requested a fallback execution
    if (!burstCompute || burstFallback) {
        const bool preferSynchronous = DeviceManager::get()->syncExecHal();
        std::tie(n, outputShapes, timing) = mPreparedModel->execute(
                request, measure, deadline, loopTimeoutDuration, preferSynchronous);
    }

    if (n != ANEURALNETWORKS_NO_ERROR) {
        VLOG(EXECUTION) << "**Execution failed**";
        return {n, std::move(outputShapes), timing};
    }

    // Copy the output data from shared memory to the output buffers.
    NNTRACE_RT_SWITCH(NNTRACE_PHASE_RESULTS, "DriverPreparedModel::execute");
    if (outputPtrArgsMemory != nullptr) {
        uint32_t ptrOutputIndex = 0;
        for (const auto& info : outputs) {
            if (info.state() == ModelArgumentInfo::POINTER) {
                const DataLocation& loc = outputPtrArgsLocations[ptrOutputIndex++];
                const uint8_t* const data = outputPtrArgsMemory->getPointer();
                memcpy(info.buffer(), data + loc.offset, loc.length);
            }
        }
    }

    VLOG(EXECUTION) << "DriverPreparedModel::execute completed";
    return {ANEURALNETWORKS_NO_ERROR, std::move(outputShapes), timing};
}

std::tuple<int, int, sp<hal::IFencedExecutionCallback>, hal::Timing>
DriverPreparedModel::executeFenced(
        const std::vector<ModelArgumentInfo>& inputs, const std::vector<ModelArgumentInfo>& outputs,
        const std::vector<const Memory*>& memories, const std::vector<int>& waitFor,
        hal::MeasureTiming measure, const std::optional<Deadline>& deadline,
        const OptionalTimeoutDuration& loopTimeoutDuration,
        const hal::OptionalTimeoutDuration& timeoutDurationAfterFence) const {
    NNTRACE_RT(NNTRACE_PHASE_INPUTS_AND_OUTPUTS, "DriverPreparedModel::executeFenced");
    CHECK(std::all_of(waitFor.begin(), waitFor.end(), [](int fd) { return fd > 0; }));
    // Make a copy of the memory tracker as we will append memory pools for pointer arguments.
    std::vector<const Memory*> localMemories = memories;
    sp<hal::IFencedExecutionCallback> executeFencedCallback;
    hal::Timing timing = kNoTiming;

    // We separate the input & output pools so accelerators only need to copy
    // the contents of the input pools. We could also use it to set protection
    // on read only memory but that's not currently done.

    // Layout the input and output data
    const auto [n1, inputPtrArgsMemory, inputPtrArgsLocations] =
            allocatePointerArgumentsToPool(inputs, &localMemories);
    if (n1 != ANEURALNETWORKS_NO_ERROR) {
        return {n1, -1, nullptr, timing};
    }
    const auto [n2, outputPtrArgsMemory, outputPtrArgsLocations] =
            allocatePointerArgumentsToPool(outputs, &localMemories);
    if (n2 != ANEURALNETWORKS_NO_ERROR) {
        return {n2, -1, nullptr, timing};
    }

    // Copy the input data that was specified via a pointer.
    if (inputPtrArgsMemory != nullptr) {
        uint32_t ptrInputIndex = 0;
        for (const auto& info : inputs) {
            if (info.state() == ModelArgumentInfo::POINTER) {
                const DataLocation& loc = inputPtrArgsLocations[ptrInputIndex++];
                uint8_t* const data = inputPtrArgsMemory->getPointer();
                memcpy(data + loc.offset, info.buffer(), loc.length);
            }
        }
    }

    Request request;
    request.inputs = createRequestArguments(inputs, inputPtrArgsLocations);
    request.outputs = createRequestArguments(outputs, outputPtrArgsLocations);
    uint32_t count = localMemories.size();
    request.pools.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        request.pools[i] = localMemories[i]->getMemoryPool();
    }

    NNTRACE_FULL_SWITCH(NNTRACE_LAYER_IPC, NNTRACE_PHASE_EXECUTION,
                        "DriverPreparedModel::executeFenced");

    int n = ANEURALNETWORKS_OP_FAILED;
    hidl_vec<hidl_handle> waitForHandles;
    waitForHandles.resize(waitFor.size());
    for (uint32_t i = 0; i < waitFor.size(); i++) {
        native_handle_t* nativeHandle = native_handle_create(1, 0);
        if (nativeHandle == nullptr) {
            LOG(ERROR) << "Failed to create native_handle";
            return {n, -1, nullptr, timing};
        }
        int dupFd = dup(waitFor[i]);
        if (dupFd <= 0) {
            LOG(ERROR) << "Unable to dup the file descriptor";
            return {n, -1, nullptr, timing};
        }
        nativeHandle->data[0] = dupFd;
        hidl_handle hidlHandle;
        hidlHandle.setTo(nativeHandle, /*shouldOwn=*/true);
        waitForHandles[i] = std::move(hidlHandle);
    }

    hidl_handle syncFence;
    std::tie(n, syncFence, executeFencedCallback, timing) =
            mPreparedModel->executeFenced(request, waitForHandles, measure, deadline,
                                          loopTimeoutDuration, timeoutDurationAfterFence);

    if (n != ANEURALNETWORKS_NO_ERROR) {
        VLOG(EXECUTION) << "**executeFenced failed**";
        return {n, -1, nullptr, timing};
    }

    int syncFenceFd = -1;
    if (syncFence.getNativeHandle()) {
        syncFenceFd = dup(syncFence.getNativeHandle()->data[0]);
        if (syncFenceFd < 0) {
            LOG(ERROR) << "Failed to dup the file descriptor";
            return {ANEURALNETWORKS_OP_FAILED, -1, nullptr, timing};
        }
    }
    // If output buffer is provided as a malloc pointer, wait for the execution to finish.
    // Then copy the output data from shared memory to the output buffers.
    if (outputPtrArgsMemory != nullptr) {
        NNTRACE_RT_SWITCH(NNTRACE_PHASE_RESULTS, "DriverPreparedModel::executeFenced");
        if (syncFenceFd > 0) {
            auto r = syncWait(syncFenceFd, -1);
            if (r != FenceState::SIGNALED) {
                LOG(ERROR) << "syncWait failed, fd: " << syncFenceFd;
                return {ANEURALNETWORKS_OP_FAILED, syncFenceFd, nullptr, timing};
            }
        }
        uint32_t ptrOutputIndex = 0;
        for (const auto& info : outputs) {
            if (info.state() == ModelArgumentInfo::POINTER) {
                const DataLocation& loc = outputPtrArgsLocations[ptrOutputIndex++];
                const uint8_t* const data = outputPtrArgsMemory->getPointer();
                memcpy(info.buffer(), data + loc.offset, loc.length);
            }
        }
    }

    VLOG(EXECUTION) << "DriverPreparedModel::executeFenced completed";
    return {ANEURALNETWORKS_NO_ERROR, syncFenceFd, executeFencedCallback, timing};
}

// A special abstracted device for the CPU. Only one instance of this class will exist.
// Use get() to retrieve it.
class CpuDevice : public Device {
   public:
    // Returns the singleton CPU fallback device.
    static std::shared_ptr<CpuDevice> get() {
        static std::shared_ptr<CpuDevice> instance(new CpuDevice);
        return instance;
    }

    const std::string& getName() const override { return kName; }
    const std::string& getVersionString() const override { return kVersionString; }
    int64_t getFeatureLevel() const override { return kFeatureLevel; }
    int32_t getType() const override { return ANEURALNETWORKS_DEVICE_CPU; }
    const std::vector<Extension>& getSupportedExtensions() const override {
        return kSupportedExtensions;
    }
    std::vector<bool> getSupportedOperations(const MetaModel& metaModel) const override;
    PerformanceInfo getPerformance(OperandType) const override { return kPerformance; }
    PerformanceInfo getRelaxedFloat32toFloat16PerformanceScalar() const override {
        return kPerformance;
    }
    PerformanceInfo getRelaxedFloat32toFloat16PerformanceTensor() const override {
        return kPerformance;
    }
    PerformanceInfo getIfPerformance() const override { return kPerformance; }
    PerformanceInfo getWhilePerformance() const override { return kPerformance; }
    bool isCachingSupported() const override { return false; }
    int wait() const override { return ANEURALNETWORKS_NO_ERROR; }

    std::pair<int, std::shared_ptr<PreparedModel>> prepareModel(
            const ModelFactory& makeModel, ExecutionPreference preference, Priority priority,
            const std::optional<Deadline>& deadline, const std::string& cacheDir,
            const std::optional<CacheToken>& maybeToken) const override;

    std::pair<int, std::unique_ptr<Memory>> allocate(const MemoryDescriptor& desc,
                                                     OperandType type) const override;

   private:
    CpuDevice() = default;
    const int64_t kFeatureLevel = __ANDROID_API__;
    const std::string kName = "nnapi-reference";
    const std::string kVersionString = build::GetBuildNumber();
    // Since the performance is a ratio compared to the CPU performance,
    // by definition the performance of the CPU is 1.0.
    const PerformanceInfo kPerformance = {.execTime = 1.0f, .powerUsage = 1.0f};
    const std::vector<Extension> kSupportedExtensions{/* No extensions. */};
};

// A special abstracted PreparedModel for the CPU, constructed by CpuDevice.
class CpuPreparedModel : public PreparedModel {
   public:
    // Factory method for CpuPreparedModel. Returns ANEURALNETWORKS_NO_ERROR and
    // a prepared model object if successfully created. Returns an error code
    // and nullptr otherwise.
    static std::pair<int, std::shared_ptr<PreparedModel>> create(Model hidlModel);

    const Device* getDevice() const override { return CpuDevice::get().get(); }
    std::shared_ptr<VersionedIPreparedModel> getInterface() const override { return nullptr; }

    std::tuple<int, std::vector<OutputShape>, Timing> execute(
            const std::vector<ModelArgumentInfo>& inputs,
            const std::vector<ModelArgumentInfo>& outputs,
            const std::vector<const Memory*>& memories,
            const std::shared_ptr<ExecutionBurstController>& burstController, MeasureTiming measure,
            const std::optional<Deadline>& deadline,
            const OptionalTimeoutDuration& loopTimeoutDuration) const override;

    std::shared_ptr<ExecutionBurstController> configureExecutionBurst(
            bool /*preferPowerOverLatency*/) const override {
        return nullptr;
    }

    std::tuple<int, int, sp<hal::IFencedExecutionCallback>, hal::Timing> executeFenced(
            const std::vector<ModelArgumentInfo>& inputs,
            const std::vector<ModelArgumentInfo>& outputs,
            const std::vector<const Memory*>& memories, const std::vector<int>& wait_for,
            MeasureTiming measure, const std::optional<Deadline>& deadline,
            const OptionalTimeoutDuration& loopTimeoutDuration,
            const hal::OptionalTimeoutDuration& timeoutDurationAfterFence) const override;

    // Prefer to use CpuPreparedModel::create.
    CpuPreparedModel(Model model, std::vector<RunTimePoolInfo> poolInfos)
        : mModel(std::move(model)), mModelPoolInfos(std::move(poolInfos)) {}

   private:
    const Model mModel;
    const std::vector<RunTimePoolInfo> mModelPoolInfos;
};

std::vector<bool> CpuDevice::getSupportedOperations(const MetaModel& metaModel) const {
    const Model& hidlModel = metaModel.getModel();
    const size_t count = hidlModel.main.operations.size();
    std::vector<bool> result(count, false);
    for (size_t i = 0; i < count; i++) {
        // TODO(b/119870033): Decide whether and how post-P operations would be supported on CPU.
        //                    We may want to use the slicer for CpuDevice just as we do for
        //                    DriverDevice.
        OperationType operationType = hidlModel.main.operations[i].type;
        result[i] = !isExtensionOperationType(operationType) &&
                    operationType != OperationType::OEM_OPERATION;
    }
    return result;
}

std::pair<int, std::shared_ptr<PreparedModel>> CpuDevice::prepareModel(
        const ModelFactory& makeModel, ExecutionPreference preference, Priority priority,
        const std::optional<Deadline>& deadline, const std::string& /*cacheDir*/,
        const std::optional<CacheToken>& maybeToken) const {
    CHECK(!maybeToken.has_value())
            << "Should never call prepareModel with cache information on CpuDevice";

    const Model model = makeModel();
    if (!validateModel(model, ValidationMode::RUNTIME) ||
        !validateExecutionPreference(preference) || !validatePriority(priority)) {
        return {ANEURALNETWORKS_OP_FAILED, nullptr};
    }
    if (hasDeadlinePassed(deadline)) {
        return {ANEURALNETWORKS_MISSED_DEADLINE_PERSISTENT, nullptr};
    }

    return CpuPreparedModel::create(model);
}

std::pair<int, std::unique_ptr<Memory>> CpuDevice::allocate(const MemoryDescriptor& desc,
                                                            OperandType type) const {
    uint32_t size = TypeManager::get()->getSizeOfData(type, desc.dimensions);
    if (size == 0) {
        LOG(ERROR) << "CpuDevice::allocate -- does not support unknown dimensions.";
        return {ANEURALNETWORKS_OP_FAILED, nullptr};
    }
    return MemoryAshmem::create(size);
}

std::pair<int, std::shared_ptr<PreparedModel>> CpuPreparedModel::create(Model hidlModel) {
    std::vector<RunTimePoolInfo> poolInfos;
    if (!setRunTimePoolInfosFromHidlMemories(&poolInfos, hidlModel.pools)) {
        return {ANEURALNETWORKS_UNMAPPABLE, nullptr};
    }

    std::shared_ptr<PreparedModel> preparedModel =
            std::make_shared<CpuPreparedModel>(std::move(hidlModel), std::move(poolInfos));
    return {ANEURALNETWORKS_NO_ERROR, std::move(preparedModel)};
}

static std::tuple<int, std::vector<OutputShape>, Timing> computeOnCpu(
        const Model& model, const Request& request,
        const std::vector<RunTimePoolInfo>& modelPoolInfos,
        const std::vector<RunTimePoolInfo>& requestPoolInfos,
        const std::optional<Deadline>& deadline,
        const OptionalTimeoutDuration& loopTimeoutDuration) {
    NNTRACE_RT(NNTRACE_PHASE_EXECUTION, "computeOnCpu");
    CpuExecutor executor;
    if (loopTimeoutDuration.getDiscriminator() !=
        OptionalTimeoutDuration::hidl_discriminator::none) {
        executor.setLoopTimeout(loopTimeoutDuration.nanoseconds());
    }
    if (deadline.has_value()) {
        executor.setDeadline(*deadline);
    }
    int err = executor.run(model, request, modelPoolInfos, requestPoolInfos);
    const auto& outputShapes = executor.getOutputShapes();
    return {err, outputShapes, kNoTiming};
}

std::tuple<int, int, sp<hal::IFencedExecutionCallback>, hal::Timing>
CpuPreparedModel::executeFenced(const std::vector<ModelArgumentInfo>& inputs,
                                const std::vector<ModelArgumentInfo>& outputs,
                                const std::vector<const Memory*>& memories,
                                const std::vector<int>& waitFor, hal::MeasureTiming measure,
                                const std::optional<Deadline>& deadline,
                                const OptionalTimeoutDuration& loopTimeoutDuration,
                                const hal::OptionalTimeoutDuration& duration) const {
    VLOG(EXECUTION)
            << "CpuPreparedModel::executeFenced wait for sync fences to signal before execution";
    for (int syncFd : waitFor) {
        if (syncFd > 0) {
            auto r = syncWait(syncFd, -1);
            if (r != FenceState::SIGNALED) {
                LOG(ERROR) << "sync wait failed, fd: " << syncFd;
                return {ANEURALNETWORKS_OP_FAILED, -1, nullptr, {UINT64_MAX, UINT64_MAX}};
            }
        }
    }

    // Update deadline if the timeout duration is closer than the deadline.
    auto closestDeadline = deadline;
    if (duration.getDiscriminator() != OptionalTimeoutDuration::hidl_discriminator::none) {
        const auto timeoutDurationDeadline = makeDeadline(duration.nanoseconds());
        if (!closestDeadline.has_value() || *closestDeadline > timeoutDurationDeadline) {
            closestDeadline = timeoutDurationDeadline;
        }
    }

    const auto [result, outputShapes, timing] = execute(inputs, outputs, memories, nullptr, measure,
                                                        closestDeadline, loopTimeoutDuration);
    return {result, -1, nullptr, timing};
}

// Perform computation on NNAPI CPU reference implementation.
//
// Contrary to DriverPreparedModel::execute, the NNAPI CPU reference executor lives in the
// same process as the NNAPI runtime and can take raw pointers. We will create as many pools as
// there are input/output in this method to avoid data copying.
//
// Will choose between sync/async execution according to DeviceManager::mSyncExecCpu.
std::tuple<int, std::vector<OutputShape>, Timing> CpuPreparedModel::execute(
        const std::vector<ModelArgumentInfo>& inputs, const std::vector<ModelArgumentInfo>& outputs,
        const std::vector<const Memory*>& memories,
        const std::shared_ptr<ExecutionBurstController>& /*burstController*/,
        MeasureTiming /*measure*/, const std::optional<Deadline>& deadline,
        const OptionalTimeoutDuration& loopTimeoutDuration) const {
    if (hasDeadlinePassed(deadline)) {
        return {ANEURALNETWORKS_MISSED_DEADLINE_PERSISTENT, {}, kNoTiming};
    }

    std::vector<RunTimePoolInfo> requestPoolInfos;
    requestPoolInfos.reserve(memories.size());
    for (const Memory* mem : memories) {
        if (std::optional<RunTimePoolInfo> poolInfo = mem->getRunTimePoolInfo()) {
            requestPoolInfos.emplace_back(*poolInfo);
        } else {
            return {ANEURALNETWORKS_UNMAPPABLE, {}, kNoTiming};
        }
    }
    // Create as many pools as there are input / output.
    auto fixPointerArguments =
            [&requestPoolInfos](const std::vector<ModelArgumentInfo>& argumentInfos) {
                std::vector<DataLocation> ptrArgsLocations;
                for (const ModelArgumentInfo& argumentInfo : argumentInfos) {
                    if (argumentInfo.state() == ModelArgumentInfo::POINTER) {
                        ptrArgsLocations.push_back(
                                {.poolIndex = static_cast<uint32_t>(requestPoolInfos.size()),
                                 .offset = 0,
                                 .length = argumentInfo.length()});
                        requestPoolInfos.emplace_back(RunTimePoolInfo::createFromExistingBuffer(
                                static_cast<uint8_t*>(argumentInfo.buffer())));
                    }
                }
                return ptrArgsLocations;
            };
    const std::vector<DataLocation> inputPtrArgsLocations = fixPointerArguments(inputs);
    const std::vector<DataLocation> outputPtrArgsLocations = fixPointerArguments(outputs);

    Request request;
    request.inputs = createRequestArguments(inputs, inputPtrArgsLocations);
    request.outputs = createRequestArguments(outputs, outputPtrArgsLocations);

    if (!DeviceManager::get()->syncExecCpu()) {
        // TODO: use a thread pool
        // TODO(mikie): this could have NNTRACE so we could measure the overhead
        //              of spinning up a new thread.
        std::tuple<int, std::vector<OutputShape>, Timing> result = {};
        std::thread([this, &request, &requestPoolInfos, &deadline, &loopTimeoutDuration, &result] {
            result = computeOnCpu(mModel, request, mModelPoolInfos, requestPoolInfos, deadline,
                                  loopTimeoutDuration);
        }).join();
        return result;
    }

    return computeOnCpu(mModel, request, mModelPoolInfos, requestPoolInfos, deadline,
                        loopTimeoutDuration);
}

DeviceManager* DeviceManager::get() {
    static DeviceManager manager;
    return &manager;
}

std::shared_ptr<Device> DeviceManager::getCpuDevice() {
    return CpuDevice::get();
}

std::shared_ptr<Device> DeviceManager::forTest_makeDriverDevice(const std::string& name,
                                                                const sp<V1_0::IDevice>& device) {
    const DeviceFactory makeDevice = [device](bool /*blocking*/) { return device; };
    const auto driverDevice = DriverDevice::create(name, makeDevice);
    CHECK(driverDevice != nullptr);
    return driverDevice;
}

void DeviceManager::findAvailableDevices() {
    VLOG(MANAGER) << "findAvailableDevices";

    // register driver devices
    const auto names = hardware::getAllHalInstanceNames(V1_0::IDevice::descriptor);
    for (const auto& name : names) {
        VLOG(MANAGER) << "Found interface " << name;
        const DeviceFactory makeDevice = [name](bool blocking) {
            return blocking ? V1_0::IDevice::getService(name) : V1_0::IDevice::tryGetService(name);
        };
        registerDevice(name, makeDevice);
    }

    // register CPU fallback device
    mDevices.push_back(CpuDevice::get());
    mDevicesCpuOnly.push_back(CpuDevice::get());
}

void DeviceManager::registerDevice(const std::string& name, const DeviceFactory& makeDevice) {
    if (auto device = DriverDevice::create(name, makeDevice)) {
        mDevices.push_back(std::move(device));
    }
}

DeviceManager::DeviceManager() {
    VLOG(MANAGER) << "DeviceManager::DeviceManager";
    findAvailableDevices();
#ifdef NN_DEBUGGABLE
    mStrictSlicing = (getProp("debug.nn.strict-slicing") != 0);
    mPartitioning = getProp("debug.nn.partition", kPartitioningDefault);
    mDebugNNCpuOnly = (getProp("debug.nn.cpuonly") != 0);
    mSyncExecCpu = (getProp("debug.nn.syncexec-cpu", 1) != 0);
    if (!mSyncExecHalSetter) {
        mSyncExecHal = (getProp("debug.nn.syncexec-hal", 1) != 0);
    }
    mSyncExecRuntime = (getProp("debug.nn.syncexec-runtime") != 0);
#endif  // NN_DEBUGGABLE
}

}  // namespace nn
}  // namespace android
