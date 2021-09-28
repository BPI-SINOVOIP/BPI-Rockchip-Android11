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

#define LOG_TAG "CpuExecutor"

#include "CpuExecutor.h"

#include <android/hardware_buffer.h>
#include <sys/mman.h>
#include <vndk/hardware_buffer.h>

#include <Eigen/Core>
#include <memory>
#include <utility>
#include <vector>

// b/109953668, disable OpenMP
#ifdef NNAPI_OPENMP
#include <omp.h>
#endif  // NNAPI_OPENMP

#include "ControlFlow.h"
#include "NeuralNetworks.h"
#include "OperationResolver.h"
#include "Operations.h"
#include "OperationsUtils.h"
#include "Tracing.h"

namespace android {
namespace nn {

namespace {

using namespace hal;

class OperationExecutionContext : public IOperationExecutionContext {
    DISALLOW_IMPLICIT_CONSTRUCTORS(OperationExecutionContext);

   public:
    OperationExecutionContext(const Operation* operation, RunTimeOperandInfo* operands)
        : operation(operation), operands(operands) {}

    uint32_t getNumInputs() const override;
    OperandType getInputType(uint32_t index) const override;
    Shape getInputShape(uint32_t index) const override;
    const void* getInputBuffer(uint32_t index) const override;
    const OperandExtraParams getInputExtraParams(uint32_t index) const override;

    uint32_t getNumOutputs() const override;
    OperandType getOutputType(uint32_t index) const override;
    Shape getOutputShape(uint32_t index) const override;
    void* getOutputBuffer(uint32_t index) override;

    // Return false on failure and store the result code.
    // Use getResultCode() to retrieve it at the end of the operation execution.
    bool setOutputShape(uint32_t index, const Shape& shape) override;
    int getResultCode() const;

    bool isOmittedInput(uint32_t index) const override;
    bool isOmittedOutput(uint32_t index) const override;

    // Return false if any of inputs or outputs is omitted, i.e. has lifetime of NO_VALUE.
    bool checkNoOmittedOperand() const;
    // Return false if any of inputs has dimension 0.
    bool checkNoZeroSizedInput() const;

   private:
    const RunTimeOperandInfo* getInputInfo(uint32_t index) const;
    const RunTimeOperandInfo* getOutputInfo(uint32_t index) const;
    RunTimeOperandInfo* getOutputInfo(uint32_t index);

    const Operation* operation;
    RunTimeOperandInfo* operands;

    int result = ANEURALNETWORKS_NO_ERROR;
};

const RunTimeOperandInfo* OperationExecutionContext::getInputInfo(uint32_t index) const {
    CHECK(index < operation->inputs.size());
    return &operands[operation->inputs[index]];
}

const RunTimeOperandInfo* OperationExecutionContext::getOutputInfo(uint32_t index) const {
    CHECK(index < operation->outputs.size());
    return &operands[operation->outputs[index]];
}

RunTimeOperandInfo* OperationExecutionContext::getOutputInfo(uint32_t index) {
    CHECK(index < operation->outputs.size());
    return &operands[operation->outputs[index]];
}

OperandType OperationExecutionContext::getInputType(uint32_t index) const {
    return getInputInfo(index)->type;
}

Shape OperationExecutionContext::getInputShape(uint32_t index) const {
    return getInputInfo(index)->shape();
}

const void* OperationExecutionContext::getInputBuffer(uint32_t index) const {
    return getInputInfo(index)->buffer;
}

const OperandExtraParams OperationExecutionContext::getInputExtraParams(uint32_t index) const {
    return getInputInfo(index)->extraParams;
}

OperandType OperationExecutionContext::getOutputType(uint32_t index) const {
    return getOutputInfo(index)->type;
}

Shape OperationExecutionContext::getOutputShape(uint32_t index) const {
    return getOutputInfo(index)->shape();
}

void* OperationExecutionContext::getOutputBuffer(uint32_t index) {
    return getOutputInfo(index)->buffer;
}

uint32_t OperationExecutionContext::getNumInputs() const {
    return operation->inputs.size();
}

uint32_t OperationExecutionContext::getNumOutputs() const {
    return operation->outputs.size();
}

int OperationExecutionContext::getResultCode() const {
    return result;
}

// TODO: Return error code directly once we've fully integrated OperationResolver with all ops.
// Updates the RunTimeOperandInfo with the newly calculated shape.
// Allocate the buffer if we need to.
//
// TODO(b/153081229): This function currently cannot handle extension operands well. We need to
//                    propagate the extension type info into this function.
bool setInfoAndAllocateIfNeeded(RunTimeOperandInfo* info, const Shape& shape, int* result) {
    // For user-provided model output operands, the parameters must match the Shape
    // calculated from the preparation step.
    if (info->lifetime == OperandLifeTime::SUBGRAPH_OUTPUT) {
        if (info->type != shape.type) {
            LOG(ERROR) << "Invalid type for model output";
            *result = ANEURALNETWORKS_OP_FAILED;
            return false;
        }
        if (info->scale != shape.scale) {
            LOG(ERROR) << "Invalid scale for model output";
            *result = ANEURALNETWORKS_OP_FAILED;
            return false;
        }
        if (info->zeroPoint != shape.offset) {
            LOG(ERROR) << "Invalid zeroPoint for model output";
            *result = ANEURALNETWORKS_OP_FAILED;
            return false;
        }
        if (info->extraParams != shape.extraParams) {
            LOG(ERROR) << "Invalid extraParams for model output";
            *result = ANEURALNETWORKS_OP_FAILED;
            return false;
        }
    }

    auto combined = combineDimensions(shape.dimensions, info->dimensions);
    if (!combined.has_value()) {
        LOG(ERROR) << "Invalid dimensions for model operand";
        *result = ANEURALNETWORKS_OP_FAILED;
        return false;
    }
    info->dimensions = std::move(combined.value());
    info->type = shape.type;
    info->scale = shape.scale;
    info->zeroPoint = shape.offset;
    info->extraParams = shape.extraParams;

    // TODO(b/153081229): We bypass the overflow check on extension operands because we do not know
    //                    the sizes of extension types.
    if (!isExtensionOperandType(info->type) &&
        nonExtensionOperandSizeOfDataOverflowsUInt32(info->type, info->dimensions)) {
        LOG(ERROR) << "Operand data size overflows uint32_t";
        *result = ANEURALNETWORKS_OP_FAILED;
        return false;
    }

    // Allocate the buffer only if the combined dimension is fully specified
    if (info->buffer == nullptr && (info->lifetime == OperandLifeTime::TEMPORARY_VARIABLE ||
                                    info->lifetime == OperandLifeTime::SUBGRAPH_OUTPUT)) {
        if (isExtensionOperandType(info->type)) {
            LOG(ERROR) << "Cannot allocate a variable of an extension type";
            *result = ANEURALNETWORKS_OP_FAILED;
            return false;
        }
        uint32_t length = nonExtensionOperandSizeOfData(info->type, info->dimensions);
        if (length > 0) {
            info->buffer = new uint8_t[length];
            if (info->buffer == nullptr) {
                *result = ANEURALNETWORKS_OUT_OF_MEMORY;
                return false;
            }
            info->length = length;
        }
    }
    if (!info->isSufficient()) {
        uint32_t length = nonExtensionOperandSizeOfData(info->type, info->dimensions);
        LOG(ERROR) << "Insufficient size for model operand: require = " << length
                   << ", provided = " << info->length;
        *result = ANEURALNETWORKS_OUTPUT_INSUFFICIENT_SIZE;
        return false;
    }
    *result = ANEURALNETWORKS_NO_ERROR;
    return true;
}

bool OperationExecutionContext::setOutputShape(uint32_t index, const Shape& shape) {
    return setInfoAndAllocateIfNeeded(getOutputInfo(index), shape, &result);
}

bool OperationExecutionContext::isOmittedInput(uint32_t index) const {
    return getInputInfo(index)->lifetime == OperandLifeTime::NO_VALUE;
}

bool OperationExecutionContext::isOmittedOutput(uint32_t index) const {
    return getOutputInfo(index)->lifetime == OperandLifeTime::NO_VALUE;
}

bool OperationExecutionContext::checkNoOmittedOperand() const {
    for (uint32_t i = 0; i < operation->inputs.size(); i++) {
        NN_RET_CHECK(!isOmittedInput(i)) << getOperationName(operation->type) << " input operand "
                                         << i << " is required but missing.";
    }
    for (uint32_t i = 0; i < operation->outputs.size(); i++) {
        NN_RET_CHECK(!isOmittedOutput(i)) << getOperationName(operation->type) << " output operand "
                                          << i << " is required but missing.";
    }
    return true;
}

bool OperationExecutionContext::checkNoZeroSizedInput() const {
    for (uint32_t i = 0; i < operation->inputs.size(); i++) {
        if (isOmittedInput(i)) continue;
        for (uint32_t j = 0; j < getInputInfo(i)->dimensions.size(); j++) {
            NN_RET_CHECK_NE(getInputInfo(i)->dimensions[j], 0)
                    << getOperationName(operation->type)
                    << " does not support zero-sized tensor, but input " << i << " dimension " << j
                    << " is 0.";
        }
    }
    return true;
}

}  // namespace

// Used to keep a pointer to a memory pool.
//
// In the case of an "mmap_fd" pool, owns the mmap region
// returned by getBuffer() -- i.e., that region goes away
// when the RunTimePoolInfo is destroyed or is assigned to.
class RunTimePoolInfo::RunTimePoolInfoImpl {
   public:
    RunTimePoolInfoImpl(const hidl_memory& hidlMemory, uint8_t* buffer, const sp<IMemory>& memory,
                        AHardwareBuffer* hardwareBuffer, uint32_t size);

    // rule of five...
    ~RunTimePoolInfoImpl();
    RunTimePoolInfoImpl(const RunTimePoolInfoImpl&) = delete;
    RunTimePoolInfoImpl(RunTimePoolInfoImpl&&) noexcept = delete;
    RunTimePoolInfoImpl& operator=(const RunTimePoolInfoImpl&) = delete;
    RunTimePoolInfoImpl& operator=(RunTimePoolInfoImpl&&) noexcept = delete;

    uint8_t* getBuffer() const { return mBuffer; }
    uint32_t getSize() const { return mSize; }

    bool flush() const;

    const hidl_memory& getHidlMemory() const { return mHidlMemory; }

   private:
    const hidl_memory mHidlMemory;     // always used
    uint8_t* const mBuffer = nullptr;  // always used
    const sp<IMemory> mMemory;         // only used when hidlMemory.name() == "ashmem"
    AHardwareBuffer*
            mAHardwareBuffer;  // only used when hidlMemory.name() == "hardware_buffer_blob"
    const uint32_t mSize;
};

RunTimePoolInfo::RunTimePoolInfoImpl::RunTimePoolInfoImpl(const hidl_memory& hidlMemory,
                                                          uint8_t* buffer,
                                                          const sp<IMemory>& memory,
                                                          AHardwareBuffer* hardwareBuffer,
                                                          uint32_t size)
    : mHidlMemory(hidlMemory),
      mBuffer(buffer),
      mMemory(memory),
      mAHardwareBuffer(hardwareBuffer),
      mSize(size) {}

RunTimePoolInfo::RunTimePoolInfoImpl::~RunTimePoolInfoImpl() {
    if (mBuffer == nullptr) {
        return;
    }

    const auto& memType = mHidlMemory.name();
    if (memType == "ashmem") {
        // nothing to do
    } else if (memType == "mmap_fd") {
        const size_t size = mHidlMemory.size();
        if (munmap(mBuffer, size)) {
            LOG(ERROR) << "RunTimePoolInfoImpl::~RunTimePoolInfo(): Can't munmap";
        }
    } else if (memType == "hardware_buffer_blob") {
        AHardwareBuffer_unlock(mAHardwareBuffer, nullptr);
    } else if (memType == "") {
        // Represents a POINTER argument; nothing to do
    } else {
        LOG(ERROR) << "RunTimePoolInfoImpl::~RunTimePoolInfoImpl(): unsupported hidl_memory type";
    }

    if (mAHardwareBuffer != nullptr) {
        AHardwareBuffer_release(mAHardwareBuffer);
    }
}

// Making sure the output data are correctly updated after execution.
bool RunTimePoolInfo::RunTimePoolInfoImpl::flush() const {
    const auto& memType = mHidlMemory.name();
    if (memType == "mmap_fd") {
        const int prot = mHidlMemory.handle()->data[1];
        if (prot & PROT_WRITE) {
            const size_t size = mHidlMemory.size();
            return msync(mBuffer, size, MS_SYNC) == 0;
        }
    }
    // No-op for other types of memory.
    return true;
}

// TODO: short term, make share memory mapping and updating a utility function.
// TODO: long term, implement mmap_fd as a hidl IMemory service.
std::optional<RunTimePoolInfo> RunTimePoolInfo::createFromHidlMemory(
        const hidl_memory& hidlMemory) {
    uint8_t* buffer = nullptr;
    sp<IMemory> memory;
    AHardwareBuffer* hardwareBuffer = nullptr;

    const auto& memType = hidlMemory.name();
    if (memType == "ashmem") {
        memory = mapMemory(hidlMemory);
        if (memory == nullptr) {
            LOG(ERROR) << "Can't map shared memory.";
            return std::nullopt;
        }
        buffer = static_cast<uint8_t*>(static_cast<void*>(memory->getPointer()));
        if (buffer == nullptr) {
            LOG(ERROR) << "Can't access shared memory.";
            return std::nullopt;
        }
    } else if (memType == "mmap_fd") {
        size_t size = hidlMemory.size();
        int fd = hidlMemory.handle()->data[0];
        int prot = hidlMemory.handle()->data[1];
        size_t offset = getSizeFromInts(hidlMemory.handle()->data[2], hidlMemory.handle()->data[3]);
        buffer = static_cast<uint8_t*>(mmap(nullptr, size, prot, MAP_SHARED, fd, offset));
        if (buffer == MAP_FAILED) {
            LOG(ERROR) << "RunTimePoolInfo::set(): Can't mmap the file descriptor.";
            return std::nullopt;
        }
    } else if (memType == "hardware_buffer_blob") {
        auto handle = hidlMemory.handle();
        auto format = AHARDWAREBUFFER_FORMAT_BLOB;
        auto usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
        const uint32_t width = hidlMemory.size();
        const uint32_t height = 1;  // height is always 1 for BLOB mode AHardwareBuffer.
        const uint32_t layers = 1;  // layers is always 1 for BLOB mode AHardwareBuffer.
        const uint32_t stride = hidlMemory.size();

        AHardwareBuffer_Desc desc{
                .width = width,
                .format = format,
                .height = height,
                .layers = layers,
                .usage = usage,
                .stride = stride,
        };
        status_t status = AHardwareBuffer_createFromHandle(
                &desc, handle, AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_CLONE, &hardwareBuffer);
        if (status != NO_ERROR) {
            LOG(ERROR) << "RunTimePoolInfo Can't create AHardwareBuffer from handle. Error: "
                       << status;
            return std::nullopt;
        }
        void* gBuffer = nullptr;
        status = AHardwareBuffer_lock(hardwareBuffer, usage, -1, nullptr, &gBuffer);
        if (status != NO_ERROR) {
            LOG(ERROR) << "RunTimePoolInfo Can't lock the AHardwareBuffer. Error: " << status;
            return std::nullopt;
        }
        buffer = static_cast<uint8_t*>(gBuffer);
    } else {
        LOG(ERROR) << "RunTimePoolInfo::set(): unsupported hidl_memory type";
        return std::nullopt;
    }

    const auto impl = std::make_shared<const RunTimePoolInfoImpl>(
            hidlMemory, buffer, memory, hardwareBuffer, hidlMemory.size());
    return {RunTimePoolInfo(impl)};
}

RunTimePoolInfo RunTimePoolInfo::createFromExistingBuffer(uint8_t* buffer, uint32_t size) {
    const auto impl = std::make_shared<const RunTimePoolInfoImpl>(hidl_memory{}, buffer, nullptr,
                                                                  nullptr, size);
    return {impl};
}

RunTimePoolInfo::RunTimePoolInfo(const std::shared_ptr<const RunTimePoolInfoImpl>& impl)
    : mImpl(impl) {}

uint8_t* RunTimePoolInfo::getBuffer() const {
    return mImpl->getBuffer();
}

uint32_t RunTimePoolInfo::getSize() const {
    return mImpl->getSize();
}

bool RunTimePoolInfo::flush() const {
    return mImpl->flush();
}

const hidl_memory& RunTimePoolInfo::getHidlMemory() const {
    return mImpl->getHidlMemory();
}

bool setRunTimePoolInfosFromHidlMemories(std::vector<RunTimePoolInfo>* poolInfos,
                                         const hidl_vec<hidl_memory>& pools) {
    CHECK(poolInfos != nullptr);
    poolInfos->clear();
    poolInfos->reserve(pools.size());
    for (const auto& pool : pools) {
        if (std::optional<RunTimePoolInfo> poolInfo = RunTimePoolInfo::createFromHidlMemory(pool)) {
            poolInfos->push_back(*poolInfo);
        } else {
            LOG(ERROR) << "Could not map pools";
            poolInfos->clear();
            return false;
        }
    }
    return true;
}

bool setRunTimePoolInfosFromMemoryPools(std::vector<RunTimePoolInfo>* poolInfos,
                                        const hidl_vec<Request::MemoryPool>& pools) {
    CHECK(poolInfos != nullptr);
    poolInfos->clear();
    poolInfos->reserve(pools.size());
    for (const auto& pool : pools) {
        if (pool.getDiscriminator() != Request::MemoryPool::hidl_discriminator::hidlMemory) {
            LOG(ERROR) << "Unknown memory token";
            poolInfos->clear();
            return false;
        }
        if (std::optional<RunTimePoolInfo> poolInfo =
                    RunTimePoolInfo::createFromHidlMemory(pool.hidlMemory())) {
            poolInfos->push_back(*poolInfo);
        } else {
            LOG(ERROR) << "Could not map pools";
            poolInfos->clear();
            return false;
        }
    }
    return true;
}

template <typename T>
inline bool convertToNhwcImpl(T* to, const T* from, const std::vector<uint32_t>& fromDim) {
    uint32_t spatialSize = fromDim[2] * fromDim[3];
    for (uint32_t n = 0; n < fromDim[0]; n++) {
        for (uint32_t hw = 0; hw < spatialSize; hw++) {
            for (uint32_t c = 0; c < fromDim[1]; c++) {
                uint32_t fromIndex = n * fromDim[1] * spatialSize + c * spatialSize + hw;
                *to++ = from[fromIndex];
            }
        }
    }
    return true;
}

template <typename T>
inline bool convertFromNhwcImpl(T* to, const T* from, const std::vector<uint32_t>& fromDim) {
    uint32_t spatialSize = fromDim[1] * fromDim[2];
    for (uint32_t n = 0; n < fromDim[0]; n++) {
        for (uint32_t c = 0; c < fromDim[3]; c++) {
            for (uint32_t hw = 0; hw < spatialSize; hw++) {
                uint32_t fromIndex = n * spatialSize * fromDim[3] + hw * fromDim[3] + c;
                *to++ = from[fromIndex];
            }
        }
    }
    return true;
}

static bool convertToNhwc(RunTimeOperandInfo& to, const RunTimeOperandInfo& from,
                          std::unique_ptr<uint8_t[]>& ptr_guard, bool data_layout) {
    int result;
    if (from.dimensions.size() != 4) {
        LOG(ERROR) << "Error converting a non-4-D tensor to NHWC layout";
        return false;
    }
    to.lifetime = OperandLifeTime::TEMPORARY_VARIABLE;
    if (data_layout) {
        // convert dimensions
        Shape inShape = from.shape();
        auto& fromDim = from.dimensions;
        inShape.dimensions = {fromDim[0], fromDim[2], fromDim[3], fromDim[1]};
        // allocate buffer
        to.buffer = nullptr;
        if (!setInfoAndAllocateIfNeeded(&to, inShape, &result)) {
            return false;
        }
        ptr_guard.reset(to.buffer);
        // convert value
        if (from.type == OperandType::TENSOR_FLOAT32) {
            return convertToNhwcImpl<float>(reinterpret_cast<float*>(to.buffer),
                                            reinterpret_cast<const float*>(from.buffer), fromDim);
        } else if (from.type == OperandType::TENSOR_FLOAT16) {
            return convertToNhwcImpl<_Float16>(reinterpret_cast<_Float16*>(to.buffer),
                                               reinterpret_cast<const _Float16*>(from.buffer),
                                               fromDim);
        } else if (from.type == OperandType::TENSOR_QUANT8_ASYMM) {
            return convertToNhwcImpl<uint8_t>(reinterpret_cast<uint8_t*>(to.buffer),
                                              reinterpret_cast<const uint8_t*>(from.buffer),
                                              fromDim);
        } else if (from.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
            return convertToNhwcImpl<int8_t>(reinterpret_cast<int8_t*>(to.buffer),
                                             reinterpret_cast<const int8_t*>(from.buffer), fromDim);
        } else {
            LOG(ERROR) << "Unsupported data type";
            return false;
        }
    } else {
        to = from;
    }
    return true;
}

static bool convertFromNhwc(RunTimeOperandInfo& to, const RunTimeOperandInfo& from,
                            bool data_layout, int* result) {
    if (from.dimensions.size() != 4) {
        LOG(ERROR) << "Error converting a non-4-D tensor from NHWC layout";
        return false;
    }
    if (data_layout) {
        // convert dimensions
        Shape outShape = from.shape();
        auto& fromDim = from.dimensions;
        outShape.dimensions = {fromDim[0], fromDim[3], fromDim[1], fromDim[2]};
        // allocate buffer
        if (!setInfoAndAllocateIfNeeded(&to, outShape, result)) {
            return false;
        }
        // convert value
        if (from.type == OperandType::TENSOR_FLOAT32) {
            return convertFromNhwcImpl<float>(reinterpret_cast<float*>(to.buffer),
                                              reinterpret_cast<const float*>(from.buffer), fromDim);
        } else if (from.type == OperandType::TENSOR_FLOAT16) {
            return convertFromNhwcImpl<_Float16>(reinterpret_cast<_Float16*>(to.buffer),
                                                 reinterpret_cast<const _Float16*>(from.buffer),
                                                 fromDim);
        } else if (from.type == OperandType::TENSOR_QUANT8_ASYMM) {
            return convertFromNhwcImpl<uint8_t>(reinterpret_cast<uint8_t*>(to.buffer),
                                                reinterpret_cast<const uint8_t*>(from.buffer),
                                                fromDim);
        } else if (from.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
            return convertFromNhwcImpl<int8_t>(reinterpret_cast<int8_t*>(to.buffer),
                                               reinterpret_cast<const int8_t*>(from.buffer),
                                               fromDim);
        } else {
            LOG(ERROR) << "Unsupported data type";
            return false;
        }
    } else {
        Shape outShape = from.shape();
        to.buffer = from.buffer;
        to.length = from.length;
        if (!setInfoAndAllocateIfNeeded(&to, outShape, result)) {
            return false;
        }
    }
    return true;
}

// Decrements the usage count for the operands listed.  Frees the memory
// allocated for any temporary variable with a count of zero.
static void consumeOperationInputs(const std::vector<uint32_t>& inputs,
                                   RunTimeOperandInfo* operands) {
    for (uint32_t i : inputs) {
        auto& info = operands[i];
        // Check if it's a static or model input/output.
        if (info.numberOfUsesLeft == 0) {
            continue;
        }
        info.numberOfUsesLeft--;
        if (info.numberOfUsesLeft == 0 && info.buffer != nullptr) {
            delete[] info.buffer;
            info.buffer = nullptr;
        }
    }
}

// This function only frees TEMPORARY_VARIABLE operands that are unused
// outputs because consumeOperationInputs takes care of any operands
// that are inputs to an operation.
static void freeUnusedSubgraphOperands(std::vector<RunTimeOperandInfo>* operands) {
    for (auto& info : *operands) {
        if (info.lifetime == OperandLifeTime::TEMPORARY_VARIABLE && info.numberOfUsesLeft == 0 &&
            info.buffer != nullptr) {
            delete[] info.buffer;
            info.buffer = nullptr;
        }
    }
}

// Ignore the .pools entry in model and request.  This will have been taken care of
// by the caller.
int CpuExecutor::run(const Model& model, const Request& request,
                     const std::vector<RunTimePoolInfo>& modelPoolInfos,
                     const std::vector<RunTimePoolInfo>& requestPoolInfos) {
    NNTRACE_CPU(NNTRACE_PHASE_EXECUTION, "run");
    VLOG(CPUEXE) << "CpuExecutor::run() with request(" << SHOW_IF_DEBUG(toString(request)) << ")";
    mModelOperandValues = &model.operandValues;
    mModelPoolInfos = &modelPoolInfos;
    mReferencedSubgraphs = &model.referenced;

    // b/109953668, disable OpenMP
#ifdef NNAPI_OPENMP
    ScopedOpenmpSettings openMpSettings;
#endif  // NNAPI_OPENMP

    std::vector<RunTimeOperandInfo> operands = initializeRunTimeInfo(model.main);
    updateForArguments(model.main.inputIndexes, request.inputs, requestPoolInfos, operands.data());
    updateForArguments(model.main.outputIndexes, request.outputs, requestPoolInfos,
                       operands.data());
    int result = executeSubgraph(model.main, operands.data());
    freeUnusedSubgraphOperands(&operands);

    if (result == ANEURALNETWORKS_NO_ERROR) {
        VLOG(CPUEXE) << "Completed run normally";
        for (auto& runtimeInfo : requestPoolInfos) {
            runtimeInfo.flush();
        }
    }

    // Only report the output shapes when the result code is NO_ERROR or OUTPUT_INSUFFICIENT_SIZE.
    if (result == ANEURALNETWORKS_NO_ERROR || result == ANEURALNETWORKS_OUTPUT_INSUFFICIENT_SIZE) {
        setOutputShapes(model.main.outputIndexes, operands);
    } else {
        mOutputShapes.clear();
    }

    mFinished = true;
    mModelOperandValues = nullptr;
    mModelPoolInfos = nullptr;
    mReferencedSubgraphs = nullptr;
    return result;
}

int CpuExecutor::executeSubgraph(const Subgraph& subgraph, RunTimeOperandInfo* operands) {
    VLOG(CPUEXE) << "CpuExecutor::executeSubgraph " << toString(subgraph);
    // The graph has serialized the operation in execution order.
    for (const auto& operation : subgraph.operations) {
        NN_RETURN_IF_ERROR(executeOperation(operation, operands));
    }
    return ANEURALNETWORKS_NO_ERROR;
}

std::vector<RunTimeOperandInfo> CpuExecutor::initializeRunTimeInfo(const Subgraph& subgraph) {
    VLOG(CPUEXE) << "CpuExecutor::initializeRunTimeInfo";
    const size_t count = subgraph.operands.size();
    std::vector<RunTimeOperandInfo> operands(count);
    for (size_t i = 0; i < count; i++) {
        const Operand& from = subgraph.operands[i];
        RunTimeOperandInfo& to = operands[i];
        to.type = from.type;
        to.dimensions = from.dimensions;
        to.scale = from.scale;
        to.zeroPoint = from.zeroPoint;
        to.length = from.location.length;
        to.lifetime = from.lifetime;
        to.extraParams = from.extraParams;
        switch (from.lifetime) {
            case OperandLifeTime::TEMPORARY_VARIABLE:
                to.buffer = nullptr;
                to.numberOfUsesLeft = from.numberOfConsumers;
                break;
            case OperandLifeTime::CONSTANT_COPY:
                to.buffer = const_cast<uint8_t*>(&(*mModelOperandValues)[from.location.offset]);
                to.numberOfUsesLeft = 0;
                break;
            case OperandLifeTime::CONSTANT_REFERENCE: {
                auto poolIndex = from.location.poolIndex;
                CHECK_LT(poolIndex, mModelPoolInfos->size());
                auto& r = (*mModelPoolInfos)[poolIndex];
                to.buffer = r.getBuffer() + from.location.offset;
                to.numberOfUsesLeft = 0;
                break;
            }
            case OperandLifeTime::SUBGRAPH: {
                auto subgraphIndex = from.location.offset;
                CHECK_LT(subgraphIndex, mReferencedSubgraphs->size());
                to.buffer = reinterpret_cast<uint8_t*>(
                        const_cast<Subgraph*>(&(*mReferencedSubgraphs)[subgraphIndex]));
                to.numberOfUsesLeft = 0;
            } break;
            case OperandLifeTime::SUBGRAPH_INPUT:
            case OperandLifeTime::SUBGRAPH_OUTPUT:
            case OperandLifeTime::NO_VALUE:
                to.buffer = nullptr;
                to.numberOfUsesLeft = 0;
                break;
        }
    }
    return operands;
}

void CpuExecutor::updateForArguments(const std::vector<uint32_t>& indexes,
                                     const hal::hidl_vec<hal::RequestArgument>& arguments,
                                     const std::vector<RunTimePoolInfo>& requestPoolInfos,
                                     RunTimeOperandInfo* operands) {
    CHECK_EQ(indexes.size(), arguments.size());
    for (size_t i = 0; i < indexes.size(); i++) {
        const uint32_t operandIndex = indexes[i];
        const RequestArgument& from = arguments[i];
        RunTimeOperandInfo& to = operands[operandIndex];
        if (from.dimensions.size() > 0) {
            // It's the responsibility of the caller to validate that
            // from.dimensions only modifies the dimensions that were
            // unspecified in the model.  That's the case in SampleDriver.cpp
            // with the call to validateRequest().
            // TODO make sure that's the case for the default CPU path.
            to.dimensions = from.dimensions;
        }
        if (from.hasNoValue) {
            to.lifetime = OperandLifeTime::NO_VALUE;
            CHECK(to.buffer == nullptr);
            to.length = 0;
        } else {
            auto poolIndex = from.location.poolIndex;
            CHECK_LT(poolIndex, requestPoolInfos.size());
            auto& r = requestPoolInfos[poolIndex];
            to.buffer = r.getBuffer() + from.location.offset;
            if (from.location.offset == 0 && from.location.length == 0) {
                // Use the entire memory region.
                to.length = r.getSize();
            } else {
                to.length = from.location.length;
            }
        }
    }
}

int CpuExecutor::executeOperation(const Operation& operation, RunTimeOperandInfo* operands) {
    if (hasDeadlinePassed(mDeadline)) {
        return ANEURALNETWORKS_MISSED_DEADLINE_TRANSIENT;
    }
    if (operation.type == OperationType::IF) {
        int result = executeIfOperation(operation, operands);
        if (result != ANEURALNETWORKS_NO_ERROR) {
            LOG(ERROR) << "IF failed.";
        }
        return result;
    }
    if (operation.type == OperationType::WHILE) {
        int result = executeWhileOperation(operation, operands);
        if (result != ANEURALNETWORKS_NO_ERROR) {
            LOG(ERROR) << "WHILE failed.";
        }
        return result;
    }

    // VLOG(CPUEXE) << "CpuExecutor::executeOperation(" << toString(operation) << ")";
    const hidl_vec<uint32_t>& ins = operation.inputs;
    const hidl_vec<uint32_t>& outs = operation.outputs;
    bool success = false;
    int result = ANEURALNETWORKS_NO_ERROR;

    // Function to verify that the number of input and output parameters
    // matches what is expected.  Also checks that all the parameters have
    // values. This function is to be used only for operations that do not
    // accept optional arguments.
    // TODO Have a version that works for optional arguments.
    auto allParametersPresent = [&operation, &operands, &ins, &outs](size_t requiredIns,
                                                                     size_t requiredOuts) -> bool {
        auto verify = [&operation, &operands](size_t requiredCount,
                                              const hidl_vec<uint32_t>& indexes,
                                              const char* type) -> bool {
            size_t actualCount = indexes.size();
            if (actualCount != requiredCount) {
                LOG(ERROR) << getOperationName(operation.type) << ": Invalid number of " << type
                           << " operands. Got " << actualCount << " of " << requiredCount;
                return false;
            }
            for (size_t i = 0; i < actualCount; i++) {
                if (operands[indexes[i]].lifetime == OperandLifeTime::NO_VALUE) {
                    LOG(ERROR) << getOperationName(operation.type) << " " << type << " operand "
                               << i << " is required but missing.";
                    return false;
                }
            }
            return true;
        };

        auto verifyNoZeroSizedInputs = [&operation, &operands](const hidl_vec<uint32_t>& indexes) {
            for (size_t i = 0; i < indexes.size(); i++) {
                for (size_t j = 0; j < operands[indexes[i]].dimensions.size(); j++) {
                    if (operands[indexes[i]].dimensions[j] == 0) {
                        LOG(ERROR) << getOperationName(operation.type)
                                   << " does not support zero-sized tensor, but input " << i
                                   << " dimension " << j << " is zero.";
                        return false;
                    }
                }
            }
            return true;
        };

        return verify(requiredIns, ins, "in") && verify(requiredOuts, outs, "out") &&
               verifyNoZeroSizedInputs(ins);
    };

    switch (operation.type) {
        case OperationType::OEM_OPERATION: {
            LOG(ERROR) << "OEM operation not supported for CPU execution";
            success = false;
        } break;
        case OperationType::RESHAPE: {
            if (!allParametersPresent(2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            const RunTimeOperandInfo& targetShape = operands[ins[1]];

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            success = reshapePrepare(input.shape(),
                                     reinterpret_cast<const int32_t*>(targetShape.buffer),
                                     getNumberOfElements(targetShape.shape()), &outShape) &&
                      setInfoAndAllocateIfNeeded(&output, outShape, &result) &&
                      copyData(input.buffer, input.shape(), output.buffer, outShape);
        } break;
        case OperationType::DEPTH_TO_SPACE: {
            const size_t inCount = ins.size();
            if ((inCount != 3 && inCount != 2) || !allParametersPresent(inCount, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            int32_t blockSize = getScalarData<int32_t>(operands[ins[1]]);
            bool data_layout = inCount == 3 ? getScalarData<bool>(operands[ins[2]]) : false;

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            RunTimeOperandInfo input_tmp, output_tmp;
            std::unique_ptr<uint8_t[]> input_tmp_guard, output_tmp_guard;
            if (!convertToNhwc(input_tmp, input, input_tmp_guard, data_layout)) {
                success = false;
                break;
            }
            output_tmp.lifetime = OperandLifeTime::TEMPORARY_VARIABLE;
            output_tmp.buffer = data_layout ? nullptr : output.buffer;
            output_tmp.length = data_layout ? 0 : output.length;
            if (!depthToSpacePrepare(input_tmp.shape(), blockSize, &outShape) ||
                !setInfoAndAllocateIfNeeded(&output_tmp, outShape, &result)) {
                if (!data_layout) output.dimensions = output_tmp.dimensions;
                break;
            }
            switch (input_tmp.type) {
                case OperandType::TENSOR_FLOAT32: {
                    success = depthToSpaceGeneric(
                            reinterpret_cast<const float*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<float*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_FLOAT16: {
                    success = depthToSpaceGeneric(
                            reinterpret_cast<const _Float16*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<_Float16*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM: {
                    success = depthToSpaceGeneric(
                            reinterpret_cast<const uint8_t*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<uint8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM_SIGNED: {
                    success = depthToSpaceGeneric(
                            reinterpret_cast<const int8_t*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<int8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                default: {
                    LOG(ERROR) << "Unsupported data type";
                    success = false;
                }
            }
            if (data_layout) {
                output_tmp_guard.reset(output_tmp.buffer);
            }
            if (!success || !convertFromNhwc(output, output_tmp, data_layout, &result)) {
                success = false;
                break;
            }
        } break;
        case OperationType::SPACE_TO_DEPTH: {
            const size_t inCount = ins.size();
            if ((inCount != 3 && inCount != 2) || !allParametersPresent(inCount, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            int32_t blockSize = getScalarData<int32_t>(operands[ins[1]]);
            bool data_layout = inCount == 3 ? getScalarData<bool>(operands[ins[2]]) : false;

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            RunTimeOperandInfo input_tmp, output_tmp;
            std::unique_ptr<uint8_t[]> input_tmp_guard, output_tmp_guard;
            if (!convertToNhwc(input_tmp, input, input_tmp_guard, data_layout)) {
                success = false;
                break;
            }
            output_tmp.lifetime = OperandLifeTime::TEMPORARY_VARIABLE;
            output_tmp.buffer = data_layout ? nullptr : output.buffer;
            output_tmp.length = data_layout ? 0 : output.length;

            if (!spaceToDepthPrepare(input_tmp.shape(), blockSize, &outShape) ||
                !setInfoAndAllocateIfNeeded(&output_tmp, outShape, &result)) {
                if (!data_layout) output.dimensions = output_tmp.dimensions;
                break;
            }
            switch (input_tmp.type) {
                case OperandType::TENSOR_FLOAT32: {
                    success = spaceToDepthGeneric(
                            reinterpret_cast<const float*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<float*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_FLOAT16: {
                    success = spaceToDepthGeneric(
                            reinterpret_cast<const _Float16*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<_Float16*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM: {
                    success = spaceToDepthGeneric(
                            reinterpret_cast<const uint8_t*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<uint8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM_SIGNED: {
                    success = spaceToDepthGeneric(
                            reinterpret_cast<const int8_t*>(input_tmp.buffer), input_tmp.shape(),
                            blockSize, reinterpret_cast<int8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                default: {
                    LOG(ERROR) << "Unsupported data type";
                    success = false;
                }
            }
            if (data_layout) {
                output_tmp_guard.reset(output_tmp.buffer);
            }
            if (!success || !convertFromNhwc(output, output_tmp, data_layout, &result)) {
                success = false;
                break;
            }
        } break;
        case OperationType::EMBEDDING_LOOKUP: {
            if (!allParametersPresent(2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& values = operands[ins[EmbeddingLookup::kValueTensor]];
            const RunTimeOperandInfo& lookups = operands[ins[EmbeddingLookup::kLookupTensor]];
            RunTimeOperandInfo& output = operands[outs[EmbeddingLookup::kOutputTensor]];

            Shape outputShape;
            EmbeddingLookup lookup(operation, operands);

            success = embeddingLookupPrepare(values.shape(), lookups.shape(), &outputShape) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) && lookup.Eval();
        } break;
        case OperationType::HASHTABLE_LOOKUP: {
            if (!allParametersPresent(3, 2)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& lookups = operands[ins[HashtableLookup::kLookupTensor]];
            const RunTimeOperandInfo& keys = operands[ins[HashtableLookup::kKeyTensor]];
            const RunTimeOperandInfo& values = operands[ins[HashtableLookup::kValueTensor]];

            RunTimeOperandInfo& output = operands[outs[HashtableLookup::kOutputTensor]];
            RunTimeOperandInfo& hits = operands[outs[HashtableLookup::kHitsTensor]];

            Shape outputShape, hitShape;
            HashtableLookup lookup(operation, operands);

            success = hashtableLookupPrepare(lookups.shape(), keys.shape(), values.shape(),
                                             &outputShape, &hitShape) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) &&
                      setInfoAndAllocateIfNeeded(&hits, hitShape, &result) && lookup.Eval();
        } break;
        case OperationType::LSH_PROJECTION: {
            RunTimeOperandInfo& output = operands[outs[LSHProjection::kOutputTensor]];
            Shape outputShape;
            if (!LSHProjection::Prepare(operation, operands, &outputShape) ||
                !setInfoAndAllocateIfNeeded(&output, outputShape, &result)) {
                break;
            }

            LSHProjection lsh(operation, operands);
            const RunTimeOperandInfo& hash = operands[ins[LSHProjection::kHashTensor]];
            switch (hash.type) {
                case OperandType::TENSOR_FLOAT32: {
                    success = lsh.Eval<float>();
                    break;
                }
                case OperandType::TENSOR_FLOAT16: {
                    success = lsh.Eval<_Float16>();
                    break;
                }
                default: {
                    success = false;
                    LOG(ERROR) << "Unsupported data type";
                }
            }
        } break;
        case OperationType::BIDIRECTIONAL_SEQUENCE_LSTM: {
            const auto merge_outputs = getScalarData<bool>(
                    operands[ins[BidirectionalSequenceLSTM::kMergeOutputsParam]]);
            const bool output_state = (outs.size() == 5 || outs.size() == 6);
            RunTimeOperandInfo& fwOutput =
                    operands[outs[BidirectionalSequenceLSTM::kFwOutputTensor]];
            Shape fwOutputShape, bwOutputShape, fwOutputActivationStateShape,
                    fwOutputCellStateShape, bwOutputActivationStateShape, bwOutputCellStateShape;

            BidirectionalSequenceLSTM lstm(operation, operands);
            success = lstm.Prepare(operation, operands, &fwOutputShape, &bwOutputShape,
                                   &fwOutputActivationStateShape, &fwOutputCellStateShape,
                                   &bwOutputActivationStateShape, &bwOutputCellStateShape) &&
                      setInfoAndAllocateIfNeeded(&fwOutput, fwOutputShape, &result);
            if (!merge_outputs) {
                RunTimeOperandInfo& bwOutput =
                        operands[outs[BidirectionalSequenceLSTM::kBwOutputTensor]];
                success = success && setInfoAndAllocateIfNeeded(&bwOutput, bwOutputShape, &result);
            }
            if (output_state) {
                uint32_t delta = merge_outputs ? 1 : 0;
                RunTimeOperandInfo& fwOutputActivationState =
                        operands[outs[BidirectionalSequenceLSTM::kFwOutputActivationStateTensor -
                                      delta]];
                RunTimeOperandInfo& fwOutputCellState =
                        operands[outs[BidirectionalSequenceLSTM::kFwOutputCellStateTensor - delta]];
                RunTimeOperandInfo& bwOutputActivationState =
                        operands[outs[BidirectionalSequenceLSTM::kBwOutputActivationStateTensor -
                                      delta]];
                RunTimeOperandInfo& bwOutputCellState =
                        operands[outs[BidirectionalSequenceLSTM::kBwOutputCellStateTensor - delta]];
                success = success &&
                          setInfoAndAllocateIfNeeded(&fwOutputActivationState,
                                                     fwOutputActivationStateShape, &result) &&
                          setInfoAndAllocateIfNeeded(&fwOutputCellState, fwOutputCellStateShape,
                                                     &result) &&
                          setInfoAndAllocateIfNeeded(&bwOutputActivationState,
                                                     bwOutputActivationStateShape, &result) &&
                          setInfoAndAllocateIfNeeded(&bwOutputCellState, bwOutputCellStateShape,
                                                     &result);
            }
            success = success && lstm.Eval();
        } break;
        case OperationType::LSTM: {
            RunTimeOperandInfo& scratch = operands[outs[LSTMCell::kScratchBufferTensor]];
            RunTimeOperandInfo& outputStateOut = operands[outs[LSTMCell::kOutputStateOutTensor]];
            RunTimeOperandInfo& cellStateOut = operands[outs[LSTMCell::kCellStateOutTensor]];
            RunTimeOperandInfo& output = operands[outs[LSTMCell::kOutputTensor]];

            Shape scratchShape, outputStateShape, cellStateShape, outputShape;
            LSTMCell lstm_cell(operation, operands);

            success = lstm_cell.Prepare(operation, operands, &scratchShape, &outputStateShape,
                                        &cellStateShape, &outputShape) &&
                      setInfoAndAllocateIfNeeded(&scratch, scratchShape, &result) &&
                      setInfoAndAllocateIfNeeded(&outputStateOut, outputStateShape, &result) &&
                      setInfoAndAllocateIfNeeded(&cellStateOut, cellStateShape, &result) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) && lstm_cell.Eval();
        } break;
        case OperationType::RANDOM_MULTINOMIAL: {
            if (!allParametersPresent(3, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& lookups = operands[ins[HashtableLookup::kLookupTensor]];
            const RunTimeOperandInfo& keys = operands[ins[HashtableLookup::kKeyTensor]];
            const RunTimeOperandInfo& values = operands[ins[HashtableLookup::kValueTensor]];
            RunTimeOperandInfo& output = operands[outs[Multinomial::kOutputTensor]];

            Shape outputShape;
            Multinomial multinomial(operation, operands);

            success = Multinomial::Prepare(operation, operands, &outputShape) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) &&
                      multinomial.Eval();
        } break;
        case OperationType::RNN: {
            if (!allParametersPresent(6, 2)) {
                return ANEURALNETWORKS_BAD_DATA;
            }

            RunTimeOperandInfo& hiddenStateOut = operands[outs[RNN::kHiddenStateOutTensor]];
            RunTimeOperandInfo& output = operands[outs[RNN::kOutputTensor]];

            Shape hiddenStateShape, outputShape;
            RNN rnn_cell(operation, operands);

            success = RNN::Prepare(operation, operands, &hiddenStateShape, &outputShape) &&
                      setInfoAndAllocateIfNeeded(&hiddenStateOut, hiddenStateShape, &result) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) && rnn_cell.Eval();
        } break;
        case OperationType::SVDF: {
            RunTimeOperandInfo& stateOut = operands[outs[SVDF::kStateOutTensor]];
            RunTimeOperandInfo& output = operands[outs[SVDF::kOutputTensor]];

            Shape stateShape, outputShape;
            SVDF svdf(operation, operands);

            success = SVDF::Prepare(operation, operands, &stateShape, &outputShape) &&
                      setInfoAndAllocateIfNeeded(&stateOut, stateShape, &result) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) && svdf.Eval();
        } break;
        case OperationType::BATCH_TO_SPACE_ND: {
            const size_t inCount = ins.size();
            if ((inCount != 3 && inCount != 2) || !allParametersPresent(inCount, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            const RunTimeOperandInfo& blockSize = operands[ins[1]];
            bool data_layout = inCount == 3 ? getScalarData<bool>(operands[ins[2]]) : false;

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            RunTimeOperandInfo input_tmp, output_tmp;
            std::unique_ptr<uint8_t[]> input_tmp_guard, output_tmp_guard;
            if (!convertToNhwc(input_tmp, input, input_tmp_guard, data_layout)) {
                success = false;
                break;
            }
            output_tmp.lifetime = OperandLifeTime::TEMPORARY_VARIABLE;
            output_tmp.buffer = data_layout ? nullptr : output.buffer;
            output_tmp.length = data_layout ? 0 : output.length;

            if (!batchToSpacePrepare(input_tmp.shape(),
                                     reinterpret_cast<const int32_t*>(blockSize.buffer),
                                     blockSize.shape(), &outShape) ||
                !setInfoAndAllocateIfNeeded(&output_tmp, outShape, &result)) {
                if (!data_layout) output.dimensions = output_tmp.dimensions;
                break;
            }
            switch (input_tmp.type) {
                case OperandType::TENSOR_FLOAT32: {
                    success = batchToSpaceGeneric(
                            reinterpret_cast<const float*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<float*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_FLOAT16: {
                    success = batchToSpaceGeneric(
                            reinterpret_cast<const _Float16*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<_Float16*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM: {
                    success = batchToSpaceGeneric(
                            reinterpret_cast<const uint8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<uint8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM_SIGNED: {
                    success = batchToSpaceGeneric(
                            reinterpret_cast<const int8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<int8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                default: {
                    LOG(ERROR) << "Unsupported data type";
                    success = false;
                }
            }
            if (data_layout) {
                output_tmp_guard.reset(output_tmp.buffer);
            }
            if (!success || !convertFromNhwc(output, output_tmp, data_layout, &result)) {
                success = false;
                break;
            }
        } break;
        case OperationType::SPACE_TO_BATCH_ND: {
            const size_t inCount = ins.size();
            if ((inCount != 4 && inCount != 3) || !allParametersPresent(inCount, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            const RunTimeOperandInfo& blockSize = operands[ins[1]];
            const RunTimeOperandInfo& paddings = operands[ins[2]];
            bool data_layout = inCount == 4 ? getScalarData<bool>(operands[ins[3]]) : false;

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            RunTimeOperandInfo input_tmp, output_tmp;
            std::unique_ptr<uint8_t[]> input_tmp_guard, output_tmp_guard;
            if (!convertToNhwc(input_tmp, input, input_tmp_guard, data_layout)) {
                success = false;
                break;
            }
            output_tmp.lifetime = OperandLifeTime::TEMPORARY_VARIABLE;
            output_tmp.buffer = data_layout ? nullptr : output.buffer;
            output_tmp.length = data_layout ? 0 : output.length;

            if (!spaceToBatchPrepare(
                        input_tmp.shape(), reinterpret_cast<const int32_t*>(blockSize.buffer),
                        blockSize.shape(), reinterpret_cast<const int32_t*>(paddings.buffer),
                        paddings.shape(), &outShape) ||
                !setInfoAndAllocateIfNeeded(&output_tmp, outShape, &result)) {
                if (!data_layout) output.dimensions = output_tmp.dimensions;
                break;
            }
            switch (input_tmp.type) {
                case OperandType::TENSOR_FLOAT32: {
                    success = spaceToBatchGeneric(
                            reinterpret_cast<const float*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<const int32_t*>(paddings.buffer), paddings.shape(),
                            reinterpret_cast<float*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_FLOAT16: {
                    success = spaceToBatchGeneric(
                            reinterpret_cast<const _Float16*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<const int32_t*>(paddings.buffer), paddings.shape(),
                            reinterpret_cast<_Float16*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM: {
                    success = spaceToBatchGeneric(
                            reinterpret_cast<const uint8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<const int32_t*>(paddings.buffer), paddings.shape(),
                            reinterpret_cast<uint8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                case OperandType::TENSOR_QUANT8_ASYMM_SIGNED: {
                    success = spaceToBatchGeneric(
                            reinterpret_cast<const int8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int32_t*>(blockSize.buffer),
                            reinterpret_cast<const int32_t*>(paddings.buffer), paddings.shape(),
                            reinterpret_cast<int8_t*>(output_tmp.buffer), outShape);
                    break;
                }
                default: {
                    LOG(ERROR) << "Unsupported data type";
                    success = false;
                }
            }
            if (data_layout) {
                output_tmp_guard.reset(output_tmp.buffer);
            }
            if (!success || !convertFromNhwc(output, output_tmp, data_layout, &result)) {
                success = false;
                break;
            }
        } break;
        case OperationType::PAD:
        case OperationType::PAD_V2: {
            const bool isV2 = operation.type == OperationType::PAD_V2;
            if (!allParametersPresent(isV2 ? 3 : 2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            const RunTimeOperandInfo& paddings = operands[ins[1]];

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            if (!padPrepare(input.shape(), reinterpret_cast<const int32_t*>(paddings.buffer),
                            paddings.shape(), &outShape) ||
                !setInfoAndAllocateIfNeeded(&output, outShape, &result)) {
                break;
            }
            if (input.type == OperandType::TENSOR_FLOAT32) {
                float pad_value = isV2 ? getScalarData<float>(operands[ins[2]]) : 0;
                success = padGeneric(reinterpret_cast<const float*>(input.buffer), input.shape(),
                                     reinterpret_cast<const int32_t*>(paddings.buffer), pad_value,
                                     reinterpret_cast<float*>(output.buffer), outShape);
            } else if (input.type == OperandType::TENSOR_FLOAT16) {
                _Float16 pad_value = isV2 ? getScalarData<_Float16>(operands[ins[2]]) : 0;
                success = padGeneric(reinterpret_cast<const _Float16*>(input.buffer), input.shape(),
                                     reinterpret_cast<const int32_t*>(paddings.buffer),
                                     static_cast<_Float16>(pad_value),
                                     reinterpret_cast<_Float16*>(output.buffer), outShape);
            } else if (input.type == OperandType::TENSOR_QUANT8_ASYMM) {
                uint8_t pad_value =
                        isV2 ? getScalarData<uint8_t>(operands[ins[2]]) : outShape.offset;
                success = padGeneric(input.buffer, input.shape(),
                                     reinterpret_cast<const int32_t*>(paddings.buffer), pad_value,
                                     output.buffer, outShape);
            } else if (input.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
                uint8_t pad_value =
                        isV2 ? getScalarData<int8_t>(operands[ins[2]]) : outShape.offset;
                success = padGeneric(input.buffer, input.shape(),
                                     reinterpret_cast<const int32_t*>(paddings.buffer), pad_value,
                                     output.buffer, outShape);
            }
        } break;
        case OperationType::CAST: {
            if (!allParametersPresent(1, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            success = cast::prepare(input.shape(), &outShape) &&
                      setInfoAndAllocateIfNeeded(&output, outShape, &result) &&
                      cast::eval(input.buffer, input.shape(), output.buffer, outShape);
        } break;
        case OperationType::MEAN: {
            if (!allParametersPresent(3, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            const RunTimeOperandInfo& axis = operands[ins[1]];
            int32_t keepDims = getScalarData<int32_t>(operands[ins[2]]);

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            if (!meanPrepare(input.shape(), reinterpret_cast<const int32_t*>(axis.buffer),
                             axis.shape(), keepDims > 0, &outShape) ||
                !setInfoAndAllocateIfNeeded(&output, outShape, &result)) {
                break;
            }
            if (input.type == OperandType::TENSOR_FLOAT16) {
                success = meanFloat16(reinterpret_cast<_Float16*>(input.buffer), input.shape(),
                                      reinterpret_cast<const int32_t*>(axis.buffer), axis.shape(),
                                      keepDims > 0, reinterpret_cast<_Float16*>(output.buffer),
                                      outShape);
            } else if (input.type == OperandType::TENSOR_FLOAT32) {
                success = meanGeneric<float, float>(
                        reinterpret_cast<float*>(input.buffer), input.shape(),
                        reinterpret_cast<const int32_t*>(axis.buffer), axis.shape(), keepDims > 0,
                        reinterpret_cast<float*>(output.buffer), outShape);
            } else if (input.type == OperandType::TENSOR_QUANT8_ASYMM) {
                success = meanGeneric<uint8_t, int32_t>(
                        reinterpret_cast<uint8_t*>(input.buffer), input.shape(),
                        reinterpret_cast<const int32_t*>(axis.buffer), axis.shape(), keepDims > 0,
                        reinterpret_cast<uint8_t*>(output.buffer), outShape);
            } else if (input.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
                success = meanGeneric<int8_t, int32_t>(
                        reinterpret_cast<int8_t*>(input.buffer), input.shape(),
                        reinterpret_cast<const int32_t*>(axis.buffer), axis.shape(), keepDims > 0,
                        reinterpret_cast<int8_t*>(output.buffer), outShape);
            }
        } break;
        case OperationType::ARGMAX:
        case OperationType::ARGMIN: {
            if (!allParametersPresent(2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            int32_t axis = getScalarData<int32_t>(operands[ins[1]]);

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            const bool isArgMin = operation.type == OperationType::ARGMIN;
            success = argMinMaxPrepare(input.shape(), axis, &outShape) &&
                      setInfoAndAllocateIfNeeded(&output, outShape, &result) &&
                      argMinMaxGeneric(input.buffer, input.shape(), axis, isArgMin, output.buffer,
                                       outShape);
        } break;
        case OperationType::EXPAND_DIMS: {
            if (!allParametersPresent(2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            int32_t axis = getScalarData<int32_t>(operands[ins[1]]);

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            success = expand_dims::prepare(input.shape(), axis, &outShape) &&
                      setInfoAndAllocateIfNeeded(&output, outShape, &result) &&
                      expand_dims::eval(input.buffer, input.shape(), axis, output.buffer, outShape);
        } break;
        case OperationType::SPLIT: {
            const size_t outCount = outs.size();
            if (!allParametersPresent(3, outCount)) {
                return ANEURALNETWORKS_BAD_DATA;
            }

            const RunTimeOperandInfo& input = operands[ins[0]];
            const int32_t axis = getScalarData<int32_t>(operands[ins[1]]);
            const int32_t numOutputs = getScalarData<int32_t>(operands[ins[2]]);

            if (numOutputs != outs.size()) {
                return ANEURALNETWORKS_BAD_DATA;
            }

            std::vector<Shape> outputShapes(numOutputs);
            for (int i = 0; i < numOutputs; ++i) {
                outputShapes[i] = operands[outs[i]].shape();
            }

            success = splitPrepare(input.shape(), axis, numOutputs, &outputShapes);
            for (int i = 0; i < numOutputs; ++i) {
                success = success && setInfoAndAllocateIfNeeded(&(operands[outs[i]]),
                                                                outputShapes[i], &result);
            }
            switch (input.type) {
                case OperandType::TENSOR_FLOAT16: {
                    std::vector<_Float16*> outputDataPtrs(numOutputs);
                    for (int i = 0; i < numOutputs; ++i) {
                        outputDataPtrs[i] = reinterpret_cast<_Float16*>(operands[outs[i]].buffer);
                    }
                    success = success &&
                              splitFloat16(reinterpret_cast<const _Float16*>(input.buffer),
                                           input.shape(), axis, &outputDataPtrs, outputShapes);
                } break;
                case OperandType::TENSOR_FLOAT32: {
                    std::vector<float*> outputDataPtrs(numOutputs);
                    for (int i = 0; i < numOutputs; ++i) {
                        outputDataPtrs[i] = reinterpret_cast<float*>(operands[outs[i]].buffer);
                    }
                    success = success &&
                              splitFloat32(reinterpret_cast<const float*>(input.buffer),
                                           input.shape(), axis, &outputDataPtrs, outputShapes);
                } break;
                case OperandType::TENSOR_INT32: {
                    std::vector<int32_t*> outputDataPtrs(numOutputs);
                    for (int i = 0; i < numOutputs; ++i) {
                        outputDataPtrs[i] = reinterpret_cast<int32_t*>(operands[outs[i]].buffer);
                    }
                    success = success &&
                              splitInt32(reinterpret_cast<const int32_t*>(input.buffer),
                                         input.shape(), axis, &outputDataPtrs, outputShapes);
                } break;
                case OperandType::TENSOR_QUANT8_ASYMM: {
                    std::vector<uint8_t*> outputDataPtrs(numOutputs);
                    for (int i = 0; i < numOutputs; ++i) {
                        outputDataPtrs[i] = reinterpret_cast<uint8_t*>(operands[outs[i]].buffer);
                    }
                    success = success &&
                              splitQuant8(reinterpret_cast<const uint8_t*>(input.buffer),
                                          input.shape(), axis, &outputDataPtrs, outputShapes);
                } break;
                case OperandType::TENSOR_QUANT8_ASYMM_SIGNED: {
                    std::vector<int8_t*> outputDataPtrs(numOutputs);
                    for (int i = 0; i < numOutputs; ++i) {
                        outputDataPtrs[i] = reinterpret_cast<int8_t*>(operands[outs[i]].buffer);
                    }
                    success = success &&
                              splitQuant8Signed(reinterpret_cast<const int8_t*>(input.buffer),
                                                input.shape(), axis, &outputDataPtrs, outputShapes);
                } break;
                default: {
                    return ANEURALNETWORKS_BAD_DATA;
                }
            }
        } break;
        case OperationType::MAXIMUM:
        case OperationType::MINIMUM: {
            if (!allParametersPresent(2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& in1 = operands[ins[0]];
            const RunTimeOperandInfo& in2 = operands[ins[1]];

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outputShape = output.shape();

            const bool isMinimum = operation.type == OperationType::MINIMUM;
            success = maximum_minimum::prepare(in1.shape(), in2.shape(), &outputShape) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) &&
                      maximum_minimum::eval(in1.buffer, in1.shape(), in2.buffer, in2.shape(),
                                            isMinimum, output.buffer, outputShape);
        } break;
        case OperationType::GROUPED_CONV_2D: {
            const size_t inCount = ins.size();
            if ((inCount != 12 && inCount != 9) || !allParametersPresent(inCount, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            const RunTimeOperandInfo& filter = operands[ins[1]];
            const RunTimeOperandInfo& bias = operands[ins[2]];

            int32_t padding_left, padding_right;
            int32_t padding_top, padding_bottom;
            int32_t padding_implicit = 0;
            int32_t stride_width, stride_height;
            int32_t numGroups;
            int32_t activation;
            bool data_layout = false;

            if (inCount == 12) {
                padding_left = getScalarData<int32_t>(operands[ins[3]]);
                padding_right = getScalarData<int32_t>(operands[ins[4]]);
                padding_top = getScalarData<int32_t>(operands[ins[5]]);
                padding_bottom = getScalarData<int32_t>(operands[ins[6]]);
                stride_width = getScalarData<int32_t>(operands[ins[7]]);
                stride_height = getScalarData<int32_t>(operands[ins[8]]);
                numGroups = getScalarData<int32_t>(operands[ins[9]]);
                activation = getScalarData<int32_t>(operands[ins[10]]);
                data_layout = getScalarData<bool>(operands[ins[11]]);
            } else {
                padding_implicit = getScalarData<int32_t>(operands[ins[3]]);
                stride_width = getScalarData<int32_t>(operands[ins[4]]);
                stride_height = getScalarData<int32_t>(operands[ins[5]]);
                numGroups = getScalarData<int32_t>(operands[ins[6]]);
                activation = getScalarData<int32_t>(operands[ins[7]]);
                data_layout = getScalarData<bool>(operands[ins[8]]);
            }

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            RunTimeOperandInfo input_tmp, output_tmp;
            std::unique_ptr<uint8_t[]> input_tmp_guard, output_tmp_guard;
            if (!convertToNhwc(input_tmp, input, input_tmp_guard, data_layout)) {
                success = false;
                break;
            }
            output_tmp.lifetime = OperandLifeTime::TEMPORARY_VARIABLE;
            output_tmp.buffer = data_layout ? nullptr : output.buffer;
            output_tmp.length = data_layout ? 0 : output.length;

            if (inCount == 9) {
                Shape inputShape = input_tmp.shape();
                Shape filterShape = filter.shape();
                int32_t input_width = getSizeOfDimension(inputShape, 2);
                int32_t input_height = getSizeOfDimension(inputShape, 1);
                int32_t filter_width = getSizeOfDimension(filterShape, 2);
                int32_t filter_height = getSizeOfDimension(filterShape, 1);
                calculateExplicitPadding(input_width, stride_width, filter_width, padding_implicit,
                                         &padding_left, &padding_right);
                calculateExplicitPadding(input_height, stride_height, filter_height,
                                         padding_implicit, &padding_top, &padding_bottom);
            }

            if (!groupedConvPrepare(input_tmp.shape(), filter.shape(), bias.shape(), padding_left,
                                    padding_right, padding_top, padding_bottom, stride_width,
                                    stride_height, numGroups, &outShape) ||
                !setInfoAndAllocateIfNeeded(&output_tmp, outShape, &result)) {
                if (!data_layout) output.dimensions = output_tmp.dimensions;
                success = false;
                break;
            }

            if (input_tmp.type == OperandType::TENSOR_FLOAT32) {
                success = groupedConvFloat32(
                        reinterpret_cast<const float*>(input_tmp.buffer), input_tmp.shape(),
                        reinterpret_cast<const float*>(filter.buffer), filter.shape(),
                        reinterpret_cast<const float*>(bias.buffer), bias.shape(), padding_left,
                        padding_right, padding_top, padding_bottom, stride_width, stride_height,
                        numGroups, activation, reinterpret_cast<float*>(output_tmp.buffer),
                        outShape);
            } else if (input_tmp.type == OperandType::TENSOR_FLOAT16) {
                success = groupedConvFloat16(
                        reinterpret_cast<const _Float16*>(input_tmp.buffer), input_tmp.shape(),
                        reinterpret_cast<const _Float16*>(filter.buffer), filter.shape(),
                        reinterpret_cast<const _Float16*>(bias.buffer), bias.shape(), padding_left,
                        padding_right, padding_top, padding_bottom, stride_width, stride_height,
                        numGroups, activation, reinterpret_cast<_Float16*>(output_tmp.buffer),
                        outShape);
            } else if (input_tmp.type == OperandType::TENSOR_QUANT8_ASYMM) {
                if (filter.type == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
                    success = groupedConvQuant8PerChannel(
                            reinterpret_cast<const uint8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int8_t*>(filter.buffer), filter.shape(),
                            filter.extraParams.channelQuant().scales.data(),
                            reinterpret_cast<const int32_t*>(bias.buffer), bias.shape(),
                            padding_left, padding_right, padding_top, padding_bottom, stride_width,
                            stride_height, numGroups, activation,
                            reinterpret_cast<uint8_t*>(output_tmp.buffer), outShape);
                } else if (filter.type == OperandType::TENSOR_QUANT8_ASYMM) {
                    success = groupedConvQuant8(
                            reinterpret_cast<const uint8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const uint8_t*>(filter.buffer), filter.shape(),
                            reinterpret_cast<const int32_t*>(bias.buffer), bias.shape(),
                            padding_left, padding_right, padding_top, padding_bottom, stride_width,
                            stride_height, numGroups, activation,
                            reinterpret_cast<uint8_t*>(output_tmp.buffer), outShape);
                }
            } else if (input_tmp.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
                if (filter.type == OperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
                    success = groupedConvQuant8PerChannel(
                            reinterpret_cast<const int8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int8_t*>(filter.buffer), filter.shape(),
                            filter.extraParams.channelQuant().scales.data(),
                            reinterpret_cast<const int32_t*>(bias.buffer), bias.shape(),
                            padding_left, padding_right, padding_top, padding_bottom, stride_width,
                            stride_height, numGroups, activation,
                            reinterpret_cast<int8_t*>(output_tmp.buffer), outShape);
                } else if (filter.type == OperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
                    success = groupedConvQuant8(
                            reinterpret_cast<const int8_t*>(input_tmp.buffer), input_tmp.shape(),
                            reinterpret_cast<const int8_t*>(filter.buffer), filter.shape(),
                            reinterpret_cast<const int32_t*>(bias.buffer), bias.shape(),
                            padding_left, padding_right, padding_top, padding_bottom, stride_width,
                            stride_height, numGroups, activation,
                            reinterpret_cast<int8_t*>(output_tmp.buffer), outShape);
                }
            }

            if (data_layout) {
                output_tmp_guard.reset(output_tmp.buffer);
            }
            if (!success || !convertFromNhwc(output, output_tmp, data_layout, &result)) {
                success = false;
                break;
            }
        } break;
        case OperationType::TILE: {
            if (!allParametersPresent(2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& input = operands[ins[0]];
            const RunTimeOperandInfo& multiples = operands[ins[1]];

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            success =
                    tile::prepare(input.shape(), reinterpret_cast<const int32_t*>(multiples.buffer),
                                  multiples.shape(), &outShape) &&
                    setInfoAndAllocateIfNeeded(&output, outShape, &result) &&
                    tile::eval(input.buffer, input.shape(),
                               reinterpret_cast<const int32_t*>(multiples.buffer), output.buffer,
                               outShape);
        } break;
        case OperationType::QUANTIZED_16BIT_LSTM: {
            if (!allParametersPresent(15, 2)) {
                return ANEURALNETWORKS_BAD_DATA;
            }

            RunTimeOperandInfo& cellStateOut =
                    operands[outs[QuantizedLSTMCell::kCellStateOutTensor]];
            RunTimeOperandInfo& output = operands[outs[QuantizedLSTMCell::kOutputTensor]];

            Shape cellStateOutShape, outputShape;
            QuantizedLSTMCell quantizedLSTMCell(operation, operands);

            success = QuantizedLSTMCell::prepare(operation, operands, &cellStateOutShape,
                                                 &outputShape) &&
                      setInfoAndAllocateIfNeeded(&cellStateOut, cellStateOutShape, &result) &&
                      setInfoAndAllocateIfNeeded(&output, outputShape, &result) &&
                      quantizedLSTMCell.eval();
        } break;
        case OperationType::POW: {
            if (!allParametersPresent(2, 1)) {
                return ANEURALNETWORKS_BAD_DATA;
            }
            const RunTimeOperandInfo& base = operands[ins[0]];
            const RunTimeOperandInfo& exponent = operands[ins[1]];

            RunTimeOperandInfo& output = operands[outs[0]];
            Shape outShape = output.shape();

            success = pow::prepare(base.shape(), exponent.shape(), &outShape) &&
                      setInfoAndAllocateIfNeeded(&output, outShape, &result) &&
                      pow::eval(base.buffer, base.shape(), exponent.buffer, exponent.shape(),
                                output.buffer, outShape);
        } break;
        default: {
            const OperationRegistration* operationRegistration =
                    mOperationResolver->findOperation(operation.type);
            if (operationRegistration == nullptr) {
                LOG(ERROR) << getOperationName(operation.type) << " not registered";
            } else if (operationRegistration->prepare == nullptr ||
                       operationRegistration->execute == nullptr) {
                LOG(ERROR) << "Incomplete operation registration: "
                           << getOperationName(operation.type);
            } else {
                OperationExecutionContext context(&operation, operands);
                success = operationRegistration->flags.allowOmittedOperand ||
                          context.checkNoOmittedOperand();
                success = success && (operationRegistration->flags.allowZeroSizedInput ||
                                      context.checkNoZeroSizedInput());
                success = success && operationRegistration->prepare(&context) &&
                          operationRegistration->execute(&context);
                result = context.getResultCode();
            }
        }
    }
    if (!success && result == ANEURALNETWORKS_NO_ERROR) {
        result = ANEURALNETWORKS_OP_FAILED;
    }
    if (result != ANEURALNETWORKS_NO_ERROR) {
        LOG(ERROR) << getOperationName(operation.type) << " failed.";
        return result;
    }

    consumeOperationInputs(ins, operands);
    return ANEURALNETWORKS_NO_ERROR;
}

// Copies RunTimeOperandInfo, preserving the original lifetime and numberOfUsesLeft
// to prevent deallocation of subgraph inputs and outputs.
static void setInfoExceptLifetime(RunTimeOperandInfo* to, const RunTimeOperandInfo& from) {
    auto originalLifetime = to->lifetime;
    auto originalNumberOfUsesLeft = to->numberOfUsesLeft;
    *to = from;
    to->lifetime = originalLifetime;
    to->numberOfUsesLeft = originalNumberOfUsesLeft;
}

int CpuExecutor::executeIfOperation(const Operation& operation, RunTimeOperandInfo* operands) {
    namespace op = operation_if;
    const RunTimeOperandInfo& condOperand = operands[operation.inputs[op::kCondBoolOperand]];
    if (condOperand.buffer == nullptr) {
        LOG(ERROR) << "Cannot read IF condition operand value";
        return ANEURALNETWORKS_OP_FAILED;
    }
    const bool condValue = *reinterpret_cast<const bool8*>(condOperand.buffer);
    VLOG(CPUEXE) << "CpuExecutor::executeIfOperation: condition value: " << condValue;

    const uint32_t branchInputIndex = condValue ? op::kThenModelOperand : op::kElseModelOperand;
    const RunTimeOperandInfo& branchOperand = operands[operation.inputs[branchInputIndex]];
    const Subgraph& branchSubgraph = *reinterpret_cast<const Subgraph*>(branchOperand.buffer);
    std::vector<RunTimeOperandInfo> branchOperands = initializeRunTimeInfo(branchSubgraph);

    // Initialize inner input and output operands from outer operands.
    for (uint32_t i = 0, n = branchSubgraph.inputIndexes.size(); i < n; ++i) {
        setInfoExceptLifetime(&branchOperands[branchSubgraph.inputIndexes[i]],
                              operands[operation.inputs[op::kFirstInput + i]]);
    }
    for (uint32_t i = 0, n = branchSubgraph.outputIndexes.size(); i < n; ++i) {
        setInfoExceptLifetime(&branchOperands[branchSubgraph.outputIndexes[i]],
                              operands[operation.outputs[i]]);
    }

    NN_RETURN_IF_ERROR(executeSubgraph(branchSubgraph, branchOperands.data()));
    freeUnusedSubgraphOperands(&branchOperands);

    // Update outer outputs.
    for (uint32_t i = 0, n = operation.outputs.size(); i < n; ++i) {
        setInfoExceptLifetime(&operands[operation.outputs[i]],
                              branchOperands[branchSubgraph.outputIndexes[i]]);
    }

    consumeOperationInputs(operation.inputs, operands);
    return ANEURALNETWORKS_NO_ERROR;
}

int CpuExecutor::executeWhileOperation(const Operation& operation, RunTimeOperandInfo* operands) {
    namespace op = operation_while;
    const RunTimeOperandInfo& condModelOperand = operands[operation.inputs[op::kCondModelOperand]];
    const RunTimeOperandInfo& bodyModelOperand = operands[operation.inputs[op::kBodyModelOperand]];
    const Subgraph& condSubgraph = *reinterpret_cast<const Subgraph*>(condModelOperand.buffer);
    const Subgraph& bodySubgraph = *reinterpret_cast<const Subgraph*>(bodyModelOperand.buffer);
    std::vector<RunTimeOperandInfo> condOperands = initializeRunTimeInfo(condSubgraph);
    std::vector<RunTimeOperandInfo> bodyOperands = initializeRunTimeInfo(bodySubgraph);

    // The code below implements the following sequence of subgraph input and output buffer
    // assignments:
    // iteration = 0   cond inputs = body inputs = outer inputs   body outputs = tmp1
    // iteration = 1   cond inputs = body inputs = tmp1           body outputs = tmp2
    // iteration = 2   cond inputs = body inputs = tmp2           body outputs = tmp1
    // iteration = 3   cond inputs = body inputs = ...            body outputs = ...

    // For body output double buffering.
    std::vector<uint8_t*> tmp1(bodySubgraph.outputIndexes.size());
    std::vector<uint8_t*> tmp2(bodySubgraph.outputIndexes.size());

    // For body outputs with unknown shape, we skip double buffering and
    // allocate on each iteration instead. This allows growing output tensors
    // inside a WHILE loop.
    std::vector<bool> bodyOutputHasUnknownShape(bodySubgraph.outputIndexes.size());
    for (uint32_t i = 0, n = bodySubgraph.outputIndexes.size(); i < n; ++i) {
        const Operand& operand = bodySubgraph.operands[bodySubgraph.outputIndexes[i]];
        bodyOutputHasUnknownShape[i] = nonExtensionOperandSizeOfData(operand) == 0;
    }

    // Initialize condition inputs from outer operands.
    for (uint32_t i = 0, n = condSubgraph.inputIndexes.size(); i < n; ++i) {
        setInfoExceptLifetime(&condOperands[condSubgraph.inputIndexes[i]],
                              operands[operation.inputs[op::kFirstInput + i]]);
    }

    // Store condition output on the stack.
    RunTimeOperandInfo& condOutput = condOperands[condSubgraph.outputIndexes[0]];
    bool8 condValue = {/* initialized memory */};
    condOutput.buffer = &condValue;
    condOutput.length = sizeof(condValue);

    std::chrono::nanoseconds timeoutDuration(mLoopTimeoutDuration);
    const auto startTime = std::chrono::steady_clock::now();
    for (uint32_t iteration = 0;; ++iteration) {
        VLOG(CPUEXE) << "CpuExecutor::executeWhileOperation: iteration " << iteration;
        if (iteration != 0) {
            // Set condition inputs from previous iteration outputs.
            for (uint32_t i = 0, n = bodySubgraph.outputIndexes.size(); i < n; ++i) {
                setInfoExceptLifetime(&condOperands[condSubgraph.inputIndexes[i]],
                                      bodyOperands[bodySubgraph.outputIndexes[i]]);
            }
        }
        NN_RETURN_IF_ERROR(executeSubgraph(condSubgraph, condOperands.data()));
        VLOG(CPUEXE) << "CpuExecutor::executeWhileOperation: condition value: "
                     << static_cast<int>(condValue);
        if (!condValue) {
            break;
        }

        const auto duration = std::chrono::steady_clock::now() - startTime;
        if (duration > timeoutDuration) {
            LOG(ERROR) << "CpuExecutor::executeWhileOperation: timed out after "
                       << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
                       << " ms";
            return ANEURALNETWORKS_MISSED_DEADLINE_TRANSIENT;
        }

        // Set body inputs from condition inputs.
        for (uint32_t i = 0, n = bodySubgraph.inputIndexes.size(); i < n; ++i) {
            bodyOperands[bodySubgraph.inputIndexes[i]] = condOperands[condSubgraph.inputIndexes[i]];
        }
        // Set body outputs.
        auto& outputBuffer = iteration % 2 == 0 ? tmp1 : tmp2;
        for (uint32_t i = 0, n = bodySubgraph.outputIndexes.size(); i < n; ++i) {
            RunTimeOperandInfo& info = bodyOperands[bodySubgraph.outputIndexes[i]];
            if (bodyOutputHasUnknownShape[i]) {
                // Reset dimensions and buffer.
                info.dimensions = bodySubgraph.operands[bodySubgraph.outputIndexes[i]].dimensions;
                if (outputBuffer[i] != nullptr) {
                    delete[] outputBuffer[i];
                    outputBuffer[i] = nullptr;
                }
            }
            info.buffer = outputBuffer[i];
        }

        NN_RETURN_IF_ERROR(executeSubgraph(bodySubgraph, bodyOperands.data()));

        // Update output buffer information in case we have allocated new buffers.
        for (uint32_t i = 0, n = bodySubgraph.outputIndexes.size(); i < n; ++i) {
            outputBuffer[i] = bodyOperands[bodySubgraph.outputIndexes[i]].buffer;
        }
    }

    // Copy body outputs to outer outputs.
    for (uint32_t i = 0, n = operation.outputs.size(); i < n; ++i) {
        RunTimeOperandInfo& outerOperand = operands[operation.outputs[i]];
        RunTimeOperandInfo& innerOperand = condOperands[condSubgraph.inputIndexes[i]];
        if (int error; !setInfoAndAllocateIfNeeded(&outerOperand, innerOperand.shape(), &error)) {
            return error;
        }
        CHECK_EQ(outerOperand.length, innerOperand.length);
        // TODO: Use the outer buffer as tmp1 to avoid copies.
        std::memcpy(outerOperand.buffer, innerOperand.buffer, innerOperand.length);
    }

    auto freeLoopOutputs = [](const std::vector<uint8_t*>& tmp) {
        for (auto buffer : tmp) {
            if (buffer != nullptr) {
                delete[] buffer;
            }
        }
    };
    freeLoopOutputs(tmp1);
    freeLoopOutputs(tmp2);
    freeUnusedSubgraphOperands(&condOperands);
    freeUnusedSubgraphOperands(&bodyOperands);
    consumeOperationInputs(operation.inputs, operands);

    return ANEURALNETWORKS_NO_ERROR;
}

void CpuExecutor::setOutputShapes(const std::vector<uint32_t>& outputIndexes,
                                  const std::vector<RunTimeOperandInfo>& operands) {
    mOutputShapes.resize(outputIndexes.size());
    for (uint32_t i = 0; i < outputIndexes.size(); i++) {
        const uint32_t operandIndex = outputIndexes[i];
        const RunTimeOperandInfo& from = operands[operandIndex];
        mOutputShapes[i].dimensions = from.dimensions;
        mOutputShapes[i].isSufficient = from.isSufficient();
    }
}

// b/109953668, disable OpenMP
#ifdef NNAPI_OPENMP
ScopedOpenmpSettings::ScopedOpenmpSettings() {
    mBlocktimeInitial = kmp_get_blocktime();
    kmp_set_blocktime(20);  // ms, see b/109645291

#if NNAPI_LIMIT_CPU_THREADS
    // Code not yet enabled. Choosing the number of threads to be based on
    // benchmarking. See longer comment by the class declaration.
    mMaxThreadsInitial = Eigen::nbThreads();
    const int nProcs = omp_get_num_procs();
    int threads = nProcs;
    if (nProcs >= 8) {
        threads = nProcs - 4;
    } else if (nProcs >= 4) {
        threads = nProcs - 2;
    }
    Eigen::setNbThreads(threads);
#endif
}

ScopedOpenmpSettings::~ScopedOpenmpSettings() {
    kmp_set_blocktime(mBlocktimeInitial);
#if NNAPI_LIMIT_CPU_THREADS
    Eigen::setNbThreads(mMaxThreadsInitial);
#endif
}
#endif  // NNAPI_OPENMP

}  // namespace nn
}  // namespace android
