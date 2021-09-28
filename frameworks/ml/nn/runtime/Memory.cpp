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

#define LOG_TAG "Memory"

#include "Memory.h"

#include <android-base/scopeguard.h>
#include <android/hardware_buffer.h>
#include <cutils/native_handle.h>
#include <vndk/hardware_buffer.h>

#include <algorithm>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "CompilationBuilder.h"
#include "CpuExecutor.h"
#include "ExecutionBurstController.h"
#include "Manager.h"
#include "MemoryUtils.h"
#include "TypeManager.h"
#include "Utils.h"

namespace android {
namespace nn {

using namespace hal;

namespace {

// The validator for a client-managed single-dimensional memory pool with a known size.
// The memory may be used for request inputs, request outputs, or model constants.
class SizedMemoryValidator : public MemoryValidatorBase {
   public:
    SizedMemoryValidator(uint32_t size) : kSize(size) {}

    bool validate(const CompilationBuilder*, IOType, uint32_t, const ANeuralNetworksOperandType*,
                  uint32_t offset, uint32_t length) const override {
        NN_RET_CHECK(offset + length <= kSize) << "request size larger than the memory size.";
        NN_RET_CHECK(offset != 0 || length != 0) << "memory size cannot be implied.";
        return true;
    }

    Metadata getMetadata() const override { return {.logicalSize = kSize}; }
    bool updateMetadata(const Metadata& metadata) override {
        return metadata.logicalSize == 0 || metadata.logicalSize == kSize;
    }

   private:
    const uint32_t kSize;
};

// The validator for an AHardwareBuffer with Non-BLOB format.
// We require the memory only used for request inputs or request outputs,
// with both offset and length set to zero.
class AHardwareBufferNonBlobValidator : public MemoryValidatorBase {
   public:
    AHardwareBufferNonBlobValidator() = default;

    bool validate(const CompilationBuilder* compilation, IOType, uint32_t,
                  const ANeuralNetworksOperandType*, uint32_t offset,
                  uint32_t length) const override {
        NN_RET_CHECK(compilation != nullptr)
                << "cannot use Non-BLOB AHardwareBuffer as model constant";
        NN_RET_CHECK(offset == 0 && length == 0)
                << "non-zero offset (" << offset << ") and/or length (" << length
                << ") for Non-BLOB format AHardwareBuffer.";
        return true;
    }

    Metadata getMetadata() const override { return {}; }
    bool updateMetadata(const Metadata&) override { return true; }
};

// The validator for a memory created from ANNMemory_createFromDesc.
// We require the memory only used as one of the pre-specified roles,
// with both offset and length set to zero.
class DeviceMemoryValidator : public MemoryValidatorBase {
   public:
    DeviceMemoryValidator(std::set<CompilationRole> roles, Operand operand,
                          std::vector<uint32_t> dimensions)
        : kCompilationRoles(std::move(roles)),
          kOperand(std::move(operand)),
          kInitialDimensions(std::move(dimensions)),
          mUpdatedDimensions(kInitialDimensions) {}

    bool validate(const CompilationBuilder* compilation, IOType ioType, uint32_t index,
                  const ANeuralNetworksOperandType* type, uint32_t offset,
                  uint32_t length) const override {
        NN_RET_CHECK(kCompilationRoles.count({compilation, ioType, index}) > 0)
                << "invalid compilation role.";
        NN_RET_CHECK(offset == 0 && length == 0)
                << "non-zero offset and/or length for driver-allocated memory.";
        if (type) {
            const bool isTensor = TypeManager::get()->isTensorType(kOperand.type);
            NN_RET_CHECK(isTensor || type->dimensionCount == 0)
                    << "invalid dimensions for scalar memory.";
            std::vector<uint32_t> dimensions(type->dimensions,
                                             type->dimensions + type->dimensionCount);
            // We only check against kInitialDimensions here.
            // For input memories, mUpdatedDimensions will be checked in validateInputDimensions
            // at the beginning of a computation.
            const auto combined = combineDimensions(dimensions, kInitialDimensions);
            NN_RET_CHECK(combined.has_value())
                    << "incompatible dimensions between request and memory. (request: "
                    << toString(dimensions) << ", memory: " << toString(kInitialDimensions) << ")";
        }
        return true;
    }

    bool validateInputDimensions(const std::vector<uint32_t>& dimensions) const override {
        NN_RET_CHECK(mInitialized) << "using an uninitialized memory as input";
        NN_RET_CHECK(dimensions == mUpdatedDimensions)
                << "incompatible input dimensions between request and memory. (request: "
                << toString(dimensions) << ", memory: " << toString(mUpdatedDimensions) << ")";
        return true;
    }

    Metadata getMetadata() const override {
        return {.logicalSize = TypeManager::get()->getSizeOfData(kOperand.type, mUpdatedDimensions),
                .dimensions = mUpdatedDimensions,
                .operand = kOperand};
    }

    bool updateMetadata(const Metadata& metadata) override {
        NN_RET_CHECK(!metadata.operand.has_value() ||
                     (metadata.operand->type == kOperand.type &&
                      metadata.operand->scale == kOperand.scale &&
                      metadata.operand->zeroPoint == kOperand.zeroPoint &&
                      metadata.operand->extraParams == kOperand.extraParams));

        NN_RET_CHECK(metadata.dimensions.empty() ||
                     TypeManager::get()->isTensorType(kOperand.type));
        auto combined = combineDimensions(metadata.dimensions, kInitialDimensions);
        NN_RET_CHECK(combined.has_value());
        NN_RET_CHECK(metadata.logicalSize == 0 ||
                     metadata.logicalSize ==
                             TypeManager::get()->getSizeOfData(kOperand.type, combined.value()));
        mUpdatedDimensions = std::move(combined.value());
        return true;
    }

    bool createdWithUnknownShape() const override {
        return TypeManager::get()->getSizeOfData(kOperand.type, kInitialDimensions) == 0;
    }

    void setInitialized(bool initialized) override { mInitialized = initialized; }
    bool isInitialized() const override { return mInitialized; }

   private:
    const std::set<CompilationRole> kCompilationRoles;

    // Keep track of the data type, scale, zero point, and extra parameters of the target operand.
    // Other fields will be ignored, including dimensions, lifetime, location, etc.
    const Operand kOperand;

    // The dimensions of the memory when the memory object is created.
    // May have unknown dimensions or rank.
    const std::vector<uint32_t> kInitialDimensions;

    // The updated dimensions after a successful execution or memory copying.
    std::vector<uint32_t> mUpdatedDimensions;

    bool mInitialized = false;
};

}  // namespace

Memory::Memory(hal::hidl_memory memory)
    : kHidlMemory(std::move(memory)),
      mValidator(std::make_unique<SizedMemoryValidator>(kHidlMemory.size())) {}

Memory::Memory(hal::hidl_memory memory, std::unique_ptr<MemoryValidatorBase> validator)
    : kHidlMemory(std::move(memory)), mValidator(std::move(validator)) {}

Memory::Memory(sp<hal::IBuffer> buffer, uint32_t token)
    : kBuffer(std::move(buffer)), kToken(token) {}

Memory::~Memory() {
    for (const auto& [ptr, weakBurst] : mUsedBy) {
        if (const std::shared_ptr<ExecutionBurstController> burst = weakBurst.lock()) {
            burst->freeMemory(getKey());
        }
    }
}

Request::MemoryPool Memory::getMemoryPool() const {
    Request::MemoryPool pool;
    if (kToken > 0) {
        pool.token(kToken);
    } else {
        pool.hidlMemory(kHidlMemory);
    }
    return pool;
}

std::optional<RunTimePoolInfo> Memory::getRunTimePoolInfo() const {
    std::lock_guard<std::mutex> guard(mMutex);
    if (!mHasCachedRunTimePoolInfo) {
        mCachedRunTimePoolInfo = RunTimePoolInfo::createFromHidlMemory(kHidlMemory);
        mHasCachedRunTimePoolInfo = true;
    }
    return mCachedRunTimePoolInfo;
}

intptr_t Memory::getKey() const {
    return reinterpret_cast<intptr_t>(this);
}

void Memory::usedBy(const std::shared_ptr<ExecutionBurstController>& burst) const {
    std::lock_guard<std::mutex> guard(mMutex);
    mUsedBy.emplace(burst.get(), burst);
}

static int copyHidlMemories(const std::optional<RunTimePoolInfo>& src,
                            const std::optional<RunTimePoolInfo>& dst) {
    if (!src.has_value() || !dst.has_value()) {
        LOG(ERROR) << "ANeuralNetworksMemory_copy -- unable to map memory";
        return ANEURALNETWORKS_UNMAPPABLE;
    }
    if (src->getSize() != dst->getSize()) {
        LOG(ERROR) << "ANeuralNetworksMemory_copy -- incompatible memory size";
        return ANEURALNETWORKS_BAD_DATA;
    }
    CHECK(src->getBuffer() != nullptr);
    CHECK(dst->getBuffer() != nullptr);
    std::copy(src->getBuffer(), src->getBuffer() + src->getSize(), dst->getBuffer());
    dst->flush();
    return ANEURALNETWORKS_NO_ERROR;
}

int copyIBufferToHidlMemory(const sp<IBuffer>& src, const hidl_memory& dst) {
    const auto ret = src->copyTo(dst);
    if (!ret.isOk()) {
        LOG(ERROR) << "ANeuralNetworksMemory_copy failure: " << ret.description();
        return ANEURALNETWORKS_OP_FAILED;
    }
    return convertErrorStatusToResultCode(static_cast<ErrorStatus>(ret));
}

int copyHidlMemoryToIBuffer(const hidl_memory& src, const sp<IBuffer>& dst,
                            const std::vector<uint32_t>& dimensions) {
    const auto ret = dst->copyFrom(src, dimensions);
    if (!ret.isOk()) {
        LOG(ERROR) << "ANeuralNetworksMemory_copy failure: " << ret.description();
        return ANEURALNETWORKS_OP_FAILED;
    }
    return convertErrorStatusToResultCode(static_cast<ErrorStatus>(ret));
}

static int copyIBuffers(const sp<IBuffer>& src, const sp<IBuffer>& dst,
                        const MemoryValidatorBase::Metadata& srcMetadata) {
    const auto [n, memory] = MemoryRuntimeAHWB::create(srcMetadata.logicalSize);
    NN_RETURN_IF_ERROR(n);
    const hidl_memory& hidlMemory = memory->getHidlMemory();
    if (!hidlMemory.valid()) return ANEURALNETWORKS_OUT_OF_MEMORY;
    NN_RETURN_IF_ERROR(copyIBufferToHidlMemory(src, hidlMemory));
    NN_RETURN_IF_ERROR(copyHidlMemoryToIBuffer(hidlMemory, dst, srcMetadata.dimensions));
    return ANEURALNETWORKS_NO_ERROR;
}

static int copyInternal(const Memory& src, const Memory& dst) {
    if (&src == &dst) return ANEURALNETWORKS_NO_ERROR;

    if (!src.getValidator().isInitialized()) {
        LOG(ERROR) << "ANeuralNetworksMemory_copy -- uninitialized source memory";
        return ANEURALNETWORKS_BAD_DATA;
    }

    const auto srcMetadata = src.getValidator().getMetadata();
    if (!dst.getValidator().updateMetadata(srcMetadata)) {
        LOG(ERROR) << "ANeuralNetworksMemory_copy -- incompatible memories";
        return ANEURALNETWORKS_BAD_DATA;
    }

    bool srcHasHidlMemory = src.getHidlMemory().valid();
    bool dstHasHidlMemory = dst.getHidlMemory().valid();
    bool srcHasIBuffer = src.getIBuffer() != nullptr;
    bool dstHasIBuffer = dst.getIBuffer() != nullptr;
    if (srcHasIBuffer && dstHasIBuffer) {
        return copyIBuffers(src.getIBuffer(), dst.getIBuffer(), srcMetadata);
    } else if (srcHasHidlMemory && dstHasHidlMemory) {
        return copyHidlMemories(src.getRunTimePoolInfo(), dst.getRunTimePoolInfo());
    } else if (srcHasHidlMemory && dstHasIBuffer) {
        return copyHidlMemoryToIBuffer(src.getHidlMemory(), dst.getIBuffer(),
                                       srcMetadata.dimensions);
    } else if (srcHasIBuffer && dstHasHidlMemory) {
        return copyIBufferToHidlMemory(src.getIBuffer(), dst.getHidlMemory());
    }
    return ANEURALNETWORKS_OP_FAILED;
}

int Memory::copy(const Memory& src, const Memory& dst) {
    int n = copyInternal(src, dst);
    dst.getValidator().setInitialized(n == ANEURALNETWORKS_NO_ERROR);
    return n;
}

bool MemoryBuilder::badState(const char* name) const {
    if (mFinished) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_" << name << " can't modify after finished";
        return true;
    }
    return false;
}

int MemoryBuilder::addRole(const CompilationBuilder& compilation, IOType ioType, uint32_t index,
                           float freq) {
    const char* tag = ioType == IOType::INPUT ? "addInputRole" : "addOutputRole";
    if (badState(tag)) {
        return ANEURALNETWORKS_BAD_STATE;
    }
    if (mRoles.count({&compilation, ioType, index}) > 0) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_" << tag
                   << " -- the same operand is specified twice.";
        return ANEURALNETWORKS_BAD_DATA;
    }

    std::vector<std::tuple<const PreparedModel*, IOType, uint32_t>> roles;
    auto callback = [&roles](const auto* preparedModel, IOType type, uint32_t index) {
        roles.emplace_back(preparedModel, type, index);
    };
    if (ioType == IOType::INPUT) {
        if (compilation.forEachStepRoleOfInput(index, callback) != ANEURALNETWORKS_NO_ERROR) {
            return ANEURALNETWORKS_BAD_DATA;
        }
    } else {
        if (compilation.forEachStepRoleOfOutput(index, callback) != ANEURALNETWORKS_NO_ERROR) {
            return ANEURALNETWORKS_BAD_DATA;
        }
    }

    const ModelBuilder* model = compilation.getModel();
    CHECK(model != nullptr);
    Operand operand;
    if (ioType == IOType::INPUT) {
        if (index >= model->inputCount()) {
            LOG(ERROR) << "ANeuralNetworksMemoryDesc_addInputRole -- input index out of range.";
            return ANEURALNETWORKS_BAD_DATA;
        }
        operand = model->getInputOperand(index);
    } else {
        if (index >= model->outputCount()) {
            LOG(ERROR) << "ANeuralNetworksMemoryDesc_addOutputRole -- output index out of range.";
            return ANEURALNETWORKS_BAD_DATA;
        }
        operand = model->getOutputOperand(index);
    }
    if (mOperand.has_value()) {
        if (operand.type != mOperand->type || operand.scale != mOperand->scale ||
            operand.zeroPoint != mOperand->zeroPoint ||
            operand.extraParams != mOperand->extraParams) {
            LOG(ERROR) << "ANeuralNetworksMemoryDesc_" << tag
                       << " -- incompatible operand metadata.";
            return ANEURALNETWORKS_BAD_DATA;
        }
    }
    if (!TypeManager::get()->isTensorType(operand.type) && !mDesc.dimensions.empty()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_" << tag << " -- incompatible dimensions.";
        return ANEURALNETWORKS_BAD_DATA;
    }
    auto combined = combineDimensions(mDesc.dimensions, operand.dimensions);
    if (!combined.has_value()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_" << tag << " -- incompatible dimensions.";
        return ANEURALNETWORKS_BAD_DATA;
    }

    if (freq > 1.0f || freq <= 0.0f) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_" << tag << " -- invalid frequency " << freq;
        return ANEURALNETWORKS_BAD_DATA;
    }

    mRoles.emplace(&compilation, ioType, index);
    for (const auto [preparedModel, type, ind] : roles) {
        uint32_t modelIndex = mDesc.preparedModels.add(preparedModel);
        BufferRole role = {.modelIndex = modelIndex, .ioIndex = ind, .frequency = freq};
        if (type == IOType::INPUT) {
            mDesc.inputRoles.push_back(role);
        } else {
            mDesc.outputRoles.push_back(role);
        }
    }
    mOperand = std::move(operand);
    mDesc.dimensions = std::move(combined.value());
    return ANEURALNETWORKS_NO_ERROR;
}

int MemoryBuilder::setDimensions(const std::vector<uint32_t>& dimensions) {
    if (badState("setDimensions")) return ANEURALNETWORKS_BAD_STATE;
    if (mOperand.has_value() && !TypeManager::get()->isTensorType(mOperand->type) &&
        !dimensions.empty()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_setDimensions -- incompatible dimensions for "
                      "scalars.";
        return ANEURALNETWORKS_BAD_DATA;
    }
    auto combined = combineDimensions(mDesc.dimensions, dimensions);
    if (!combined.has_value()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_setDimensions -- incompatible dimensions.";
        return ANEURALNETWORKS_BAD_DATA;
    }
    mDesc.dimensions = std::move(combined.value());
    return ANEURALNETWORKS_NO_ERROR;
}

static void logMemoryDescriptorToInfo(const MemoryDescriptor& desc, const Operand& operand) {
    LOG(INFO) << "MemoryDescriptor start";
    LOG(INFO) << "    Data type: " << toString(operand.type);
    LOG(INFO) << "    Scale: " << toString(operand.scale);
    LOG(INFO) << "    Zero point: " << toString(operand.zeroPoint);
    LOG(INFO) << "    Extra params: " << toString(operand.extraParams);
    LOG(INFO) << "    Dimensions: " << toString(desc.dimensions);
    LOG(INFO) << "    Prepared models [" << desc.preparedModels.size() << "]:";
    for (const auto* preparedModel : desc.preparedModels) {
        LOG(INFO) << "        service = " << preparedModel->getDevice()->getName();
    }
    LOG(INFO) << "    Input roles [" << desc.inputRoles.size() << "]:";
    for (const auto& usage : desc.inputRoles) {
        LOG(INFO) << "        " << toString(usage);
    }
    LOG(INFO) << "    Output roles [" << desc.outputRoles.size() << "]:";
    for (const auto& usage : desc.outputRoles) {
        LOG(INFO) << "        " << toString(usage);
    }
    LOG(INFO) << "MemoryDescriptor end";
}

static std::set<const Device*> getDevices(const MemoryDescriptor& desc) {
    std::set<const Device*> devices;
    for (const auto* preparedModel : desc.preparedModels) {
        const auto* device = preparedModel->getDevice();
        devices.insert(device);
    }
    return devices;
}

int MemoryBuilder::finish() {
    if (badState("finish")) return ANEURALNETWORKS_BAD_STATE;
    if (mRoles.empty()) {
        LOG(ERROR) << "ANeuralNetworksMemoryDesc_finish -- no role has been specified.";
        return ANEURALNETWORKS_BAD_DATA;
    }
    CHECK(mOperand.has_value());
    if (VLOG_IS_ON(MEMORY)) {
        logMemoryDescriptorToInfo(mDesc, mOperand.value());
    }
    std::set<const Device*> devices = getDevices(mDesc);
    if (devices.empty()) {
        // This can happen with interpreted control flow.
        mAllocator = nullptr;
    } else if (devices.size() == 1) {
        mAllocator = *devices.begin();
        VLOG(MEMORY) << "Using " << mAllocator->getName() << " as allocator.";
    } else {
        LOG(INFO) << "MemoryBuilder::finish -- cannot handle multiple devices.";
        mAllocator = nullptr;
    }
    mSupportsAhwb = std::all_of(devices.begin(), devices.end(), [](const auto* device) {
        return device->getFeatureLevel() >= __ANDROID_API_R__;
    });
    mShouldFallback = std::none_of(mRoles.begin(), mRoles.end(), [](const auto& role) {
        const auto* cb = std::get<const CompilationBuilder*>(role);
        return cb->createdWithExplicitDeviceList();
    });
    const uint32_t size = TypeManager::get()->getSizeOfData(mOperand->type, mDesc.dimensions);
    mShouldFallback &= (size != 0);
    mFinished = true;
    return ANEURALNETWORKS_NO_ERROR;
}

std::pair<int, std::unique_ptr<Memory>> MemoryBuilder::allocate() const {
    if (!mFinished) {
        LOG(ERROR) << "ANeuralNetworksMemory_createFromDesc -- passed an unfinished descriptor";
        return {ANEURALNETWORKS_BAD_STATE, nullptr};
    }

    int n = ANEURALNETWORKS_OP_FAILED;
    std::unique_ptr<Memory> memory;
    CHECK(mOperand.has_value());

    // Try allocate the memory on device.
    if (mAllocator != nullptr) {
        std::tie(n, memory) = mAllocator->allocate(mDesc, mOperand->type);
    }

    // If failed, fallback to ashmem or BLOB mode AHWB.
    if (n != ANEURALNETWORKS_NO_ERROR && mShouldFallback) {
        const uint32_t size = TypeManager::get()->getSizeOfData(mOperand->type, mDesc.dimensions);
        if (mSupportsAhwb) {
            VLOG(MEMORY) << "MemoryBuilder::allocate -- fallback to BLOB mode AHWB.";
            std::tie(n, memory) = MemoryRuntimeAHWB::create(size);
        } else {
            VLOG(MEMORY) << "MemoryBuilder::allocate -- fallback to ashmem.";
            std::tie(n, memory) = MemoryAshmem::create(size);
        }
    }

    if (n == ANEURALNETWORKS_NO_ERROR) {
        CHECK(memory != nullptr);
        auto validator =
                std::make_unique<DeviceMemoryValidator>(mRoles, mOperand.value(), mDesc.dimensions);
        memory->setValidator(std::move(validator));
    }
    return {n, std::move(memory)};
}

std::pair<int, std::unique_ptr<MemoryAshmem>> MemoryAshmem::create(uint32_t size) {
    hidl_memory hidlMemory = allocateSharedMemory(size);
    sp<IMemory> mapped = mapMemory(hidlMemory);
    if (mapped == nullptr || mapped->getPointer() == nullptr) {
        LOG(ERROR) << "Memory::create failed";
        return {ANEURALNETWORKS_OUT_OF_MEMORY, nullptr};
    }
    return {ANEURALNETWORKS_NO_ERROR,
            std::make_unique<MemoryAshmem>(std::move(mapped), std::move(hidlMemory))};
}

uint8_t* MemoryAshmem::getPointer() const {
    return static_cast<uint8_t*>(static_cast<void*>(kMappedMemory->getPointer()));
}

MemoryAshmem::MemoryAshmem(sp<IMemory> mapped, hidl_memory memory)
    : Memory(std::move(memory)), kMappedMemory(std::move(mapped)) {}

std::pair<int, std::unique_ptr<MemoryFd>> MemoryFd::create(size_t size, int prot, int fd,
                                                           size_t offset) {
    if (size == 0 || fd < 0) {
        LOG(ERROR) << "Invalid size or fd";
        return {ANEURALNETWORKS_BAD_DATA, nullptr};
    }

    // Duplicate the file descriptor so MemoryFd owns its own version.
    int dupfd = dup(fd);
    if (dupfd == -1) {
        LOG(ERROR) << "Failed to dup the fd";
        // TODO(b/120417090): is ANEURALNETWORKS_UNEXPECTED_NULL the correct
        // error to return here?
        return {ANEURALNETWORKS_UNEXPECTED_NULL, nullptr};
    }

    // Create a temporary native handle to own the dupfd.
    native_handle_t* nativeHandle = native_handle_create(1, 3);
    if (nativeHandle == nullptr) {
        LOG(ERROR) << "Failed to create native_handle";
        // TODO(b/120417090): is ANEURALNETWORKS_UNEXPECTED_NULL the correct
        // error to return here?
        return {ANEURALNETWORKS_UNEXPECTED_NULL, nullptr};
    }
    nativeHandle->data[0] = dupfd;
    nativeHandle->data[1] = prot;
    const uint64_t bits = static_cast<uint64_t>(offset);
    nativeHandle->data[2] = (int32_t)(uint32_t)(bits & 0xffffffff);
    nativeHandle->data[3] = (int32_t)(uint32_t)(bits >> 32);

    // Create a hidl_handle which owns the native handle and fd so that we don't
    // have to manually clean either the native handle or the fd.
    hardware::hidl_handle hidlHandle;
    hidlHandle.setTo(nativeHandle, /*shouldOwn=*/true);

    // Push the hidl_handle into a hidl_memory object. The hidl_memory object is
    // responsible for cleaning the hidl_handle, the native handle, and the fd.
    hidl_memory hidlMemory = hidl_memory("mmap_fd", std::move(hidlHandle), size);

    return {ANEURALNETWORKS_NO_ERROR, std::make_unique<MemoryFd>(std::move(hidlMemory))};
}

MemoryFd::MemoryFd(hidl_memory memory) : Memory(std::move(memory)) {}

std::pair<int, std::unique_ptr<MemoryAHWB>> MemoryAHWB::create(const AHardwareBuffer& ahwb) {
    AHardwareBuffer_Desc bufferDesc;
    AHardwareBuffer_describe(&ahwb, &bufferDesc);
    const native_handle_t* handle = AHardwareBuffer_getNativeHandle(&ahwb);
    hidl_memory hidlMemory;
    std::unique_ptr<MemoryValidatorBase> validator;
    if (bufferDesc.format == AHARDWAREBUFFER_FORMAT_BLOB) {
        hidlMemory = hidl_memory("hardware_buffer_blob", handle, bufferDesc.width);
        validator = std::make_unique<SizedMemoryValidator>(bufferDesc.width);
    } else {
        // memory size is not used.
        hidlMemory = hidl_memory("hardware_buffer", handle, 0);
        validator = std::make_unique<AHardwareBufferNonBlobValidator>();
    }
    auto memory = std::make_unique<MemoryAHWB>(std::move(hidlMemory), std::move(validator));
    return {ANEURALNETWORKS_NO_ERROR, std::move(memory)};
};

std::pair<int, std::unique_ptr<MemoryRuntimeAHWB>> MemoryRuntimeAHWB::create(uint32_t size) {
    AHardwareBuffer* ahwb = nullptr;
    const auto usage = AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
    const AHardwareBuffer_Desc desc = {
            .width = size,
            .height = 1,
            .layers = 1,
            .format = AHARDWAREBUFFER_FORMAT_BLOB,
            .usage = usage,
            .stride = size,
    };
    int err = AHardwareBuffer_allocate(&desc, &ahwb);
    if (err != 0 || ahwb == nullptr) {
        LOG(ERROR) << "Failed to allocate BLOB mode AHWB.";
        return {ANEURALNETWORKS_OP_FAILED, nullptr};
    }
    auto allocateGuard = base::make_scope_guard([&ahwb]() { AHardwareBuffer_release(ahwb); });

    void* buffer = nullptr;
    err = AHardwareBuffer_lock(ahwb, usage, -1, nullptr, &buffer);
    if (err != 0 || buffer == nullptr) {
        LOG(ERROR) << "Failed to lock BLOB mode AHWB.";
        return {ANEURALNETWORKS_OP_FAILED, nullptr};
    }
    auto lockGuard = base::make_scope_guard([&ahwb]() { AHardwareBuffer_unlock(ahwb, nullptr); });

    const native_handle_t* handle = AHardwareBuffer_getNativeHandle(ahwb);
    if (handle == nullptr) {
        LOG(ERROR) << "Failed to retrieve the native handle from the AHWB.";
        return {ANEURALNETWORKS_OP_FAILED, nullptr};
    }

    hidl_memory hidlMemory = hidl_memory("hardware_buffer_blob", handle, desc.width);
    auto memory = std::make_unique<MemoryRuntimeAHWB>(std::move(hidlMemory), ahwb,
                                                      static_cast<uint8_t*>(buffer));
    allocateGuard.Disable();
    lockGuard.Disable();
    return {ANEURALNETWORKS_NO_ERROR, std::move(memory)};
}

MemoryRuntimeAHWB::MemoryRuntimeAHWB(hal::hidl_memory memory, AHardwareBuffer* ahwb,
                                     uint8_t* buffer)
    : Memory(std::move(memory)), mAhwb(ahwb), mBuffer(buffer) {
    CHECK(mAhwb != nullptr);
    CHECK(mBuffer != nullptr);
}

MemoryRuntimeAHWB::~MemoryRuntimeAHWB() {
    AHardwareBuffer_unlock(mAhwb, nullptr);
    AHardwareBuffer_release(mAhwb);
}

std::pair<int, std::unique_ptr<MemoryFromDevice>> MemoryFromDevice::create(sp<hal::IBuffer> buffer,
                                                                           uint32_t token) {
    if (buffer == nullptr) {
        LOG(ERROR) << "nullptr IBuffer for device memory.";
        return {ANEURALNETWORKS_OP_FAILED, nullptr};
    }
    if (token <= 0) {
        LOG(ERROR) << "Invalid token for device memory: " << token;
        return {ANEURALNETWORKS_OP_FAILED, nullptr};
    }
    return {ANEURALNETWORKS_NO_ERROR, std::make_unique<MemoryFromDevice>(std::move(buffer), token)};
};

MemoryFromDevice::MemoryFromDevice(sp<hal::IBuffer> buffer, uint32_t token)
    : Memory(std::move(buffer), token) {}

}  // namespace nn
}  // namespace android
