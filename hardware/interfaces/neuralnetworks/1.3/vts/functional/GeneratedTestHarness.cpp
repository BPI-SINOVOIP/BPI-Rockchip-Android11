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

#include "GeneratedTestHarness.h"

#include <android-base/logging.h>
#include <android/hardware/neuralnetworks/1.0/IDevice.h>
#include <android/hardware/neuralnetworks/1.0/IExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.0/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.0/IPreparedModelCallback.h>
#include <android/hardware/neuralnetworks/1.0/types.h>
#include <android/hardware/neuralnetworks/1.1/IDevice.h>
#include <android/hardware/neuralnetworks/1.2/IDevice.h>
#include <android/hardware/neuralnetworks/1.2/IExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.2/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.2/IPreparedModelCallback.h>
#include <android/hardware/neuralnetworks/1.2/types.h>
#include <android/hardware/neuralnetworks/1.3/IDevice.h>
#include <android/hardware/neuralnetworks/1.3/IFencedExecutionCallback.h>
#include <android/hardware/neuralnetworks/1.3/IPreparedModel.h>
#include <android/hardware/neuralnetworks/1.3/IPreparedModelCallback.h>
#include <android/hardware/neuralnetworks/1.3/types.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <android/hidl/memory/1.0/IMemory.h>
#include <android/sync.h>
#include <gtest/gtest.h>
#include <hidlmemory/mapping.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>

#include "1.0/Utils.h"
#include "1.3/Callbacks.h"
#include "1.3/Utils.h"
#include "ExecutionBurstController.h"
#include "MemoryUtils.h"
#include "TestHarness.h"
#include "Utils.h"
#include "VtsHalNeuralnetworks.h"

namespace android::hardware::neuralnetworks::V1_3::vts::functional {

using namespace test_helper;
using hidl::memory::V1_0::IMemory;
using implementation::ExecutionCallback;
using implementation::PreparedModelCallback;
using V1_0::DataLocation;
using V1_0::RequestArgument;
using V1_1::ExecutionPreference;
using V1_2::Constant;
using V1_2::MeasureTiming;
using V1_2::OutputShape;
using V1_2::SymmPerChannelQuantParams;
using V1_2::Timing;
using HidlToken = hidl_array<uint8_t, static_cast<uint32_t>(Constant::BYTE_SIZE_OF_CACHE_TOKEN)>;

namespace {

enum class OutputType { FULLY_SPECIFIED, UNSPECIFIED, INSUFFICIENT, MISSED_DEADLINE };

enum class IOType { INPUT, OUTPUT };

struct TestConfig {
    Executor executor;
    MeasureTiming measureTiming;
    OutputType outputType;
    MemoryType memoryType;
    // `reportSkipping` indicates if a test should print an info message in case
    // it is skipped. The field is set to true by default and is set to false in
    // quantization coupling tests to suppress skipping a test
    bool reportSkipping;
    TestConfig(Executor executor, MeasureTiming measureTiming, OutputType outputType,
               MemoryType memoryType)
        : executor(executor),
          measureTiming(measureTiming),
          outputType(outputType),
          memoryType(memoryType),
          reportSkipping(true) {}
    TestConfig(Executor executor, MeasureTiming measureTiming, OutputType outputType,
               MemoryType memoryType, bool reportSkipping)
        : executor(executor),
          measureTiming(measureTiming),
          outputType(outputType),
          memoryType(memoryType),
          reportSkipping(reportSkipping) {}
};

class DeviceMemoryAllocator {
  public:
    DeviceMemoryAllocator(const sp<IDevice>& device, const sp<IPreparedModel>& preparedModel,
                          const TestModel& testModel)
        : kDevice(device), kPreparedModel(preparedModel), kTestModel(testModel) {}

    // Allocate device memory for a target input/output operand.
    // Return {IBuffer object, token} if successful.
    // Return {nullptr, 0} if device memory is not supported.
    template <IOType ioType>
    std::pair<sp<IBuffer>, uint32_t> allocate(uint32_t index) {
        std::pair<sp<IBuffer>, uint32_t> buffer;
        allocateInternal<ioType>(index, &buffer);
        return buffer;
    }

  private:
    template <IOType ioType>
    void allocateInternal(uint32_t index, std::pair<sp<IBuffer>, uint32_t>* result) {
        ASSERT_NE(result, nullptr);

        // Prepare arguments.
        BufferRole role = {.modelIndex = 0, .ioIndex = index, .frequency = 1.0f};
        hidl_vec<BufferRole> inputRoles, outputRoles;
        if constexpr (ioType == IOType::INPUT) {
            inputRoles = {role};
        } else {
            outputRoles = {role};
        }

        // Allocate device memory.
        ErrorStatus status;
        sp<IBuffer> buffer;
        uint32_t token;
        auto cb = [&status, &buffer, &token](ErrorStatus error, const sp<IBuffer>& buf,
                                             uint32_t tok) {
            status = error;
            buffer = buf;
            token = tok;
        };
        const auto ret = kDevice->allocate({}, {kPreparedModel}, inputRoles, outputRoles, cb);

        // Check allocation results.
        ASSERT_TRUE(ret.isOk());
        if (status == ErrorStatus::NONE) {
            ASSERT_NE(buffer, nullptr);
            ASSERT_GT(token, 0);
        } else {
            ASSERT_EQ(status, ErrorStatus::GENERAL_FAILURE);
            ASSERT_EQ(buffer, nullptr);
            ASSERT_EQ(token, 0);
        }

        // Initialize input data from TestBuffer.
        if constexpr (ioType == IOType::INPUT) {
            if (buffer != nullptr) {
                // TestBuffer -> Shared memory.
                const auto& testBuffer =
                        kTestModel.main.operands[kTestModel.main.inputIndexes[index]].data;
                ASSERT_GT(testBuffer.size(), 0);
                hidl_memory tmp = nn::allocateSharedMemory(testBuffer.size());
                sp<IMemory> inputMemory = mapMemory(tmp);
                ASSERT_NE(inputMemory.get(), nullptr);
                uint8_t* inputPtr =
                        static_cast<uint8_t*>(static_cast<void*>(inputMemory->getPointer()));
                ASSERT_NE(inputPtr, nullptr);
                const uint8_t* begin = testBuffer.get<uint8_t>();
                const uint8_t* end = begin + testBuffer.size();
                std::copy(begin, end, inputPtr);

                // Shared memory -> IBuffer.
                auto ret = buffer->copyFrom(tmp, {});
                ASSERT_TRUE(ret.isOk());
                ASSERT_EQ(static_cast<ErrorStatus>(ret), ErrorStatus::NONE);
            }
        }
        *result = {std::move(buffer), token};
    }

    const sp<IDevice> kDevice;
    const sp<IPreparedModel> kPreparedModel;
    const TestModel& kTestModel;
};

Subgraph createSubgraph(const TestSubgraph& testSubgraph, uint32_t* constCopySize,
                        std::vector<const TestBuffer*>* constCopies, uint32_t* constRefSize,
                        std::vector<const TestBuffer*>* constReferences) {
    CHECK(constCopySize != nullptr);
    CHECK(constCopies != nullptr);
    CHECK(constRefSize != nullptr);
    CHECK(constReferences != nullptr);

    // Operands.
    hidl_vec<Operand> operands(testSubgraph.operands.size());
    for (uint32_t i = 0; i < testSubgraph.operands.size(); i++) {
        const auto& op = testSubgraph.operands[i];

        DataLocation loc = {};
        if (op.lifetime == TestOperandLifeTime::CONSTANT_COPY) {
            loc = {
                    .poolIndex = 0,
                    .offset = *constCopySize,
                    .length = static_cast<uint32_t>(op.data.size()),
            };
            constCopies->push_back(&op.data);
            *constCopySize += op.data.alignedSize();
        } else if (op.lifetime == TestOperandLifeTime::CONSTANT_REFERENCE) {
            loc = {
                    .poolIndex = 0,
                    .offset = *constRefSize,
                    .length = static_cast<uint32_t>(op.data.size()),
            };
            constReferences->push_back(&op.data);
            *constRefSize += op.data.alignedSize();
        } else if (op.lifetime == TestOperandLifeTime::SUBGRAPH) {
            loc = {
                    .poolIndex = 0,
                    .offset = *op.data.get<uint32_t>(),
                    .length = 0,
            };
        }

        V1_2::Operand::ExtraParams extraParams;
        if (op.type == TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
            extraParams.channelQuant(SymmPerChannelQuantParams{
                    .scales = op.channelQuant.scales, .channelDim = op.channelQuant.channelDim});
        }

        operands[i] = {.type = static_cast<OperandType>(op.type),
                       .dimensions = op.dimensions,
                       .numberOfConsumers = op.numberOfConsumers,
                       .scale = op.scale,
                       .zeroPoint = op.zeroPoint,
                       .lifetime = static_cast<OperandLifeTime>(op.lifetime),
                       .location = loc,
                       .extraParams = std::move(extraParams)};
    }

    // Operations.
    hidl_vec<Operation> operations(testSubgraph.operations.size());
    std::transform(testSubgraph.operations.begin(), testSubgraph.operations.end(),
                   operations.begin(), [](const TestOperation& op) -> Operation {
                       return {.type = static_cast<OperationType>(op.type),
                               .inputs = op.inputs,
                               .outputs = op.outputs};
                   });

    return {.operands = std::move(operands),
            .operations = std::move(operations),
            .inputIndexes = testSubgraph.inputIndexes,
            .outputIndexes = testSubgraph.outputIndexes};
}

void copyTestBuffers(const std::vector<const TestBuffer*>& buffers, uint8_t* output) {
    uint32_t offset = 0;
    for (const TestBuffer* buffer : buffers) {
        const uint8_t* begin = buffer->get<uint8_t>();
        const uint8_t* end = begin + buffer->size();
        std::copy(begin, end, output + offset);
        offset += buffer->alignedSize();
    }
}

}  // namespace

void waitForSyncFence(int syncFd) {
    constexpr int kInfiniteTimeout = -1;
    ASSERT_GT(syncFd, 0);
    int r = sync_wait(syncFd, kInfiniteTimeout);
    ASSERT_GE(r, 0);
}

Model createModel(const TestModel& testModel) {
    uint32_t constCopySize = 0;
    uint32_t constRefSize = 0;
    std::vector<const TestBuffer*> constCopies;
    std::vector<const TestBuffer*> constReferences;

    Subgraph mainSubgraph = createSubgraph(testModel.main, &constCopySize, &constCopies,
                                           &constRefSize, &constReferences);
    hidl_vec<Subgraph> refSubgraphs(testModel.referenced.size());
    std::transform(testModel.referenced.begin(), testModel.referenced.end(), refSubgraphs.begin(),
                   [&constCopySize, &constCopies, &constRefSize,
                    &constReferences](const TestSubgraph& testSubgraph) {
                       return createSubgraph(testSubgraph, &constCopySize, &constCopies,
                                             &constRefSize, &constReferences);
                   });

    // Constant copies.
    hidl_vec<uint8_t> operandValues(constCopySize);
    copyTestBuffers(constCopies, operandValues.data());

    // Shared memory.
    hidl_vec<hidl_memory> pools = {};
    if (constRefSize > 0) {
        hidl_vec_push_back(&pools, nn::allocateSharedMemory(constRefSize));
        CHECK_NE(pools[0].size(), 0u);

        // load data
        sp<IMemory> mappedMemory = mapMemory(pools[0]);
        CHECK(mappedMemory.get() != nullptr);
        uint8_t* mappedPtr =
                reinterpret_cast<uint8_t*>(static_cast<void*>(mappedMemory->getPointer()));
        CHECK(mappedPtr != nullptr);

        copyTestBuffers(constReferences, mappedPtr);
    }

    return {.main = std::move(mainSubgraph),
            .referenced = std::move(refSubgraphs),
            .operandValues = std::move(operandValues),
            .pools = std::move(pools),
            .relaxComputationFloat32toFloat16 = testModel.isRelaxed};
}

static bool isOutputSizeGreaterThanOne(const TestModel& testModel, uint32_t index) {
    const auto byteSize = testModel.main.operands[testModel.main.outputIndexes[index]].data.size();
    return byteSize > 1u;
}

static void makeOutputInsufficientSize(uint32_t outputIndex, Request* request) {
    auto& length = request->outputs[outputIndex].location.length;
    ASSERT_GT(length, 1u);
    length -= 1u;
}

static void makeOutputDimensionsUnspecified(Model* model) {
    for (auto i : model->main.outputIndexes) {
        auto& dims = model->main.operands[i].dimensions;
        std::fill(dims.begin(), dims.end(), 0);
    }
}

class ExecutionContextV1_3 {
  public:
    ExecutionContextV1_3(sp<IDevice> device, sp<IPreparedModel> preparedModel)
        : kDevice(std::move(device)), kPreparedModel(std::move(preparedModel)) {}

    std::optional<Request> createRequest(const TestModel& testModel, MemoryType memoryType);
    std::vector<TestBuffer> getOutputBuffers(const TestModel& testModel,
                                             const Request& request) const;

  private:
    // Get a TestBuffer with data copied from an IBuffer object.
    void getBuffer(const sp<IBuffer>& buffer, size_t size, TestBuffer* testBuffer) const;

    static constexpr uint32_t kInputPoolIndex = 0;
    static constexpr uint32_t kOutputPoolIndex = 1;
    static constexpr uint32_t kDeviceMemoryBeginIndex = 2;

    const sp<IDevice> kDevice;
    const sp<IPreparedModel> kPreparedModel;
    std::unique_ptr<TestMemoryBase> mInputMemory, mOutputMemory;
    std::vector<sp<IBuffer>> mBuffers;
};

std::optional<Request> ExecutionContextV1_3::createRequest(const TestModel& testModel,
                                                           MemoryType memoryType) {
    // Memory pools are organized as:
    // - 0: Input shared memory pool
    // - 1: Output shared memory pool
    // - [2, 2+i): Input device memories
    // - [2+i, 2+i+o): Output device memories
    DeviceMemoryAllocator allocator(kDevice, kPreparedModel, testModel);
    std::vector<uint32_t> tokens;
    mBuffers.clear();

    // Model inputs.
    hidl_vec<RequestArgument> inputs(testModel.main.inputIndexes.size());
    size_t inputSize = 0;
    for (uint32_t i = 0; i < testModel.main.inputIndexes.size(); i++) {
        const auto& op = testModel.main.operands[testModel.main.inputIndexes[i]];
        if (op.data.size() == 0) {
            // Omitted input.
            inputs[i] = {.hasNoValue = true};
            continue;
        } else if (memoryType == MemoryType::DEVICE) {
            SCOPED_TRACE("Input index = " + std::to_string(i));
            auto [buffer, token] = allocator.allocate<IOType::INPUT>(i);
            if (buffer != nullptr) {
                DataLocation loc = {.poolIndex = static_cast<uint32_t>(mBuffers.size() +
                                                                       kDeviceMemoryBeginIndex)};
                mBuffers.push_back(std::move(buffer));
                tokens.push_back(token);
                inputs[i] = {.hasNoValue = false, .location = loc, .dimensions = {}};
                continue;
            }
        }

        // Reserve shared memory for input.
        DataLocation loc = {.poolIndex = kInputPoolIndex,
                            .offset = static_cast<uint32_t>(inputSize),
                            .length = static_cast<uint32_t>(op.data.size())};
        inputSize += op.data.alignedSize();
        inputs[i] = {.hasNoValue = false, .location = loc, .dimensions = {}};
    }

    // Model outputs.
    hidl_vec<RequestArgument> outputs(testModel.main.outputIndexes.size());
    size_t outputSize = 0;
    for (uint32_t i = 0; i < testModel.main.outputIndexes.size(); i++) {
        const auto& op = testModel.main.operands[testModel.main.outputIndexes[i]];
        if (memoryType == MemoryType::DEVICE) {
            SCOPED_TRACE("Output index = " + std::to_string(i));
            auto [buffer, token] = allocator.allocate<IOType::OUTPUT>(i);
            if (buffer != nullptr) {
                DataLocation loc = {.poolIndex = static_cast<uint32_t>(mBuffers.size() +
                                                                       kDeviceMemoryBeginIndex)};
                mBuffers.push_back(std::move(buffer));
                tokens.push_back(token);
                outputs[i] = {.hasNoValue = false, .location = loc, .dimensions = {}};
                continue;
            }
        }

        // In the case of zero-sized output, we should at least provide a one-byte buffer.
        // This is because zero-sized tensors are only supported internally to the driver, or
        // reported in output shapes. It is illegal for the client to pre-specify a zero-sized
        // tensor as model output. Otherwise, we will have two semantic conflicts:
        // - "Zero dimension" conflicts with "unspecified dimension".
        // - "Omitted operand buffer" conflicts with "zero-sized operand buffer".
        size_t bufferSize = std::max<size_t>(op.data.size(), 1);

        // Reserve shared memory for output.
        DataLocation loc = {.poolIndex = kOutputPoolIndex,
                            .offset = static_cast<uint32_t>(outputSize),
                            .length = static_cast<uint32_t>(bufferSize)};
        outputSize += op.data.size() == 0 ? TestBuffer::kAlignment : op.data.alignedSize();
        outputs[i] = {.hasNoValue = false, .location = loc, .dimensions = {}};
    }

    if (memoryType == MemoryType::DEVICE && mBuffers.empty()) {
        return std::nullopt;
    }

    // Memory pools.
    hidl_vec<Request::MemoryPool> pools(kDeviceMemoryBeginIndex + mBuffers.size());
    if (memoryType == MemoryType::BLOB_AHWB) {
        mInputMemory = TestBlobAHWB::create(std::max<size_t>(inputSize, 1));
        mOutputMemory = TestBlobAHWB::create(std::max<size_t>(outputSize, 1));
    } else {
        mInputMemory = TestAshmem::create(std::max<size_t>(inputSize, 1));
        mOutputMemory = TestAshmem::create(std::max<size_t>(outputSize, 1));
    }
    EXPECT_NE(mInputMemory, nullptr);
    EXPECT_NE(mOutputMemory, nullptr);
    pools[kInputPoolIndex].hidlMemory(mInputMemory->getHidlMemory());
    pools[kOutputPoolIndex].hidlMemory(mOutputMemory->getHidlMemory());
    for (uint32_t i = 0; i < mBuffers.size(); i++) {
        pools[kDeviceMemoryBeginIndex + i].token(tokens[i]);
    }

    // Copy input data to the input shared memory pool.
    uint8_t* inputPtr = mInputMemory->getPointer();
    for (uint32_t i = 0; i < testModel.main.inputIndexes.size(); i++) {
        if (!inputs[i].hasNoValue && inputs[i].location.poolIndex == kInputPoolIndex) {
            const auto& op = testModel.main.operands[testModel.main.inputIndexes[i]];
            const uint8_t* begin = op.data.get<uint8_t>();
            const uint8_t* end = begin + op.data.size();
            std::copy(begin, end, inputPtr + inputs[i].location.offset);
        }
    }
    return Request{
            .inputs = std::move(inputs), .outputs = std::move(outputs), .pools = std::move(pools)};
}

std::vector<TestBuffer> ExecutionContextV1_3::getOutputBuffers(const TestModel& testModel,
                                                               const Request& request) const {
    // Copy out output results.
    uint8_t* outputPtr = mOutputMemory->getPointer();
    std::vector<TestBuffer> outputBuffers;
    for (uint32_t i = 0; i < request.outputs.size(); i++) {
        const auto& outputLoc = request.outputs[i].location;
        if (outputLoc.poolIndex == kOutputPoolIndex) {
            outputBuffers.emplace_back(outputLoc.length, outputPtr + outputLoc.offset);
        } else {
            const auto& op = testModel.main.operands[testModel.main.outputIndexes[i]];
            if (op.data.size() == 0) {
                outputBuffers.emplace_back(0, nullptr);
            } else {
                SCOPED_TRACE("Output index = " + std::to_string(i));
                const uint32_t bufferIndex = outputLoc.poolIndex - kDeviceMemoryBeginIndex;
                TestBuffer buffer;
                getBuffer(mBuffers[bufferIndex], op.data.size(), &buffer);
                outputBuffers.push_back(std::move(buffer));
            }
        }
    }
    return outputBuffers;
}

// Get a TestBuffer with data copied from an IBuffer object.
void ExecutionContextV1_3::getBuffer(const sp<IBuffer>& buffer, size_t size,
                                     TestBuffer* testBuffer) const {
    // IBuffer -> Shared memory.
    hidl_memory tmp = nn::allocateSharedMemory(size);
    const auto ret = buffer->copyTo(tmp);
    ASSERT_TRUE(ret.isOk());
    ASSERT_EQ(static_cast<ErrorStatus>(ret), ErrorStatus::NONE);

    // Shared memory -> TestBuffer.
    sp<IMemory> outputMemory = mapMemory(tmp);
    ASSERT_NE(outputMemory.get(), nullptr);
    uint8_t* outputPtr = static_cast<uint8_t*>(static_cast<void*>(outputMemory->getPointer()));
    ASSERT_NE(outputPtr, nullptr);
    ASSERT_NE(testBuffer, nullptr);
    *testBuffer = TestBuffer(size, outputPtr);
}

static bool hasZeroSizedOutput(const TestModel& testModel) {
    return std::any_of(testModel.main.outputIndexes.begin(), testModel.main.outputIndexes.end(),
                       [&testModel](uint32_t index) {
                           return testModel.main.operands[index].data.size() == 0;
                       });
}

static Return<ErrorStatus> ExecutePreparedModel(const sp<IPreparedModel>& preparedModel,
                                                const Request& request, MeasureTiming measure,
                                                const OptionalTimeoutDuration& loopTimeoutDuration,
                                                sp<ExecutionCallback>& callback) {
    return preparedModel->execute_1_3(request, measure, {}, loopTimeoutDuration, callback);
}
static Return<ErrorStatus> ExecutePreparedModel(const sp<IPreparedModel>& preparedModel,
                                                const Request& request, MeasureTiming measure,
                                                const OptionalTimeoutDuration& loopTimeoutDuration,
                                                hidl_vec<OutputShape>* outputShapes,
                                                Timing* timing) {
    ErrorStatus result;
    Return<void> ret = preparedModel->executeSynchronously_1_3(
            request, measure, {}, loopTimeoutDuration,
            [&result, outputShapes, timing](ErrorStatus error, const hidl_vec<OutputShape>& shapes,
                                            const Timing& time) {
                result = error;
                *outputShapes = shapes;
                *timing = time;
            });
    if (!ret.isOk()) {
        return ErrorStatus::GENERAL_FAILURE;
    }
    return result;
}
static std::shared_ptr<::android::nn::ExecutionBurstController> CreateBurst(
        const sp<IPreparedModel>& preparedModel) {
    return android::nn::ExecutionBurstController::create(preparedModel,
                                                         std::chrono::microseconds{0});
}

void EvaluatePreparedModel(const sp<IDevice>& device, const sp<IPreparedModel>& preparedModel,
                           const TestModel& testModel, const TestConfig& testConfig,
                           bool* skipped = nullptr) {
    if (skipped != nullptr) {
        *skipped = false;
    }
    // If output0 does not have size larger than one byte, we can not test with insufficient buffer.
    if (testConfig.outputType == OutputType::INSUFFICIENT &&
        !isOutputSizeGreaterThanOne(testModel, 0)) {
        return;
    }

    ExecutionContextV1_3 context(device, preparedModel);
    auto maybeRequest = context.createRequest(testModel, testConfig.memoryType);
    // Skip if testing memory domain but no device memory has been allocated.
    if (!maybeRequest.has_value()) {
        return;
    }

    Request request = std::move(maybeRequest.value());

    constexpr uint32_t kInsufficientOutputIndex = 0;
    if (testConfig.outputType == OutputType::INSUFFICIENT) {
        makeOutputInsufficientSize(kInsufficientOutputIndex, &request);
    }

    OptionalTimeoutDuration loopTimeoutDuration;
    // OutputType::MISSED_DEADLINE is only used by
    // TestKind::INTINITE_LOOP_TIMEOUT tests to verify that an infinite loop is
    // aborted after a timeout.
    if (testConfig.outputType == OutputType::MISSED_DEADLINE) {
        // Override the default loop timeout duration with a small value to
        // speed up test execution.
        constexpr uint64_t kMillisecond = 1'000'000;
        loopTimeoutDuration.nanoseconds(1 * kMillisecond);
    }

    ErrorStatus executionStatus;
    hidl_vec<OutputShape> outputShapes;
    Timing timing;
    switch (testConfig.executor) {
        case Executor::ASYNC: {
            SCOPED_TRACE("asynchronous");

            // launch execution
            sp<ExecutionCallback> executionCallback = new ExecutionCallback();
            Return<ErrorStatus> executionLaunchStatus =
                    ExecutePreparedModel(preparedModel, request, testConfig.measureTiming,
                                         loopTimeoutDuration, executionCallback);
            ASSERT_TRUE(executionLaunchStatus.isOk());
            EXPECT_EQ(ErrorStatus::NONE, static_cast<ErrorStatus>(executionLaunchStatus));

            // retrieve execution status
            executionCallback->wait();
            executionStatus = executionCallback->getStatus();
            outputShapes = executionCallback->getOutputShapes();
            timing = executionCallback->getTiming();

            break;
        }
        case Executor::SYNC: {
            SCOPED_TRACE("synchronous");

            // execute
            Return<ErrorStatus> executionReturnStatus =
                    ExecutePreparedModel(preparedModel, request, testConfig.measureTiming,
                                         loopTimeoutDuration, &outputShapes, &timing);
            ASSERT_TRUE(executionReturnStatus.isOk());
            executionStatus = static_cast<ErrorStatus>(executionReturnStatus);

            break;
        }
        case Executor::BURST: {
            // TODO(butlermichael): Check if we need to test burst in V1_3 if the interface remains
            //                      V1_2.
            SCOPED_TRACE("burst");

            // check compliance
            ASSERT_TRUE(nn::compliantWithV1_0(request));
            V1_0::Request request10 = nn::convertToV1_0(request);

            // create burst
            const std::shared_ptr<::android::nn::ExecutionBurstController> controller =
                    CreateBurst(preparedModel);
            ASSERT_NE(nullptr, controller.get());

            // create memory keys
            std::vector<intptr_t> keys(request10.pools.size());
            for (size_t i = 0; i < keys.size(); ++i) {
                keys[i] = reinterpret_cast<intptr_t>(&request10.pools[i]);
            }

            // execute burst
            int n;
            std::tie(n, outputShapes, timing, std::ignore) =
                    controller->compute(request10, testConfig.measureTiming, keys);
            executionStatus = nn::convertResultCodeToErrorStatus(n);

            break;
        }
        case Executor::FENCED: {
            SCOPED_TRACE("fenced");
            ErrorStatus result;
            hidl_handle syncFenceHandle;
            sp<IFencedExecutionCallback> fencedCallback;
            auto callbackFunc = [&result, &syncFenceHandle, &fencedCallback](
                                        ErrorStatus error, const hidl_handle& handle,
                                        const sp<IFencedExecutionCallback>& callback) {
                result = error;
                syncFenceHandle = handle;
                fencedCallback = callback;
            };
            Return<void> ret =
                    preparedModel->executeFenced(request, {}, testConfig.measureTiming, {},
                                                 loopTimeoutDuration, {}, callbackFunc);
            ASSERT_TRUE(ret.isOk());
            if (result != ErrorStatus::NONE) {
                ASSERT_EQ(syncFenceHandle.getNativeHandle(), nullptr);
                ASSERT_EQ(fencedCallback, nullptr);
                executionStatus = result;
                timing = {UINT64_MAX, UINT64_MAX};
            } else if (syncFenceHandle.getNativeHandle()) {
                // If a sync fence is returned, try start another run waiting for the sync fence.
                ret = preparedModel->executeFenced(request, {syncFenceHandle},
                                                   testConfig.measureTiming, {},
                                                   loopTimeoutDuration, {}, callbackFunc);
                ASSERT_TRUE(ret.isOk());
                ASSERT_EQ(result, ErrorStatus::NONE);
                waitForSyncFence(syncFenceHandle.getNativeHandle()->data[0]);
            }
            if (result == ErrorStatus::NONE) {
                ASSERT_NE(fencedCallback, nullptr);
                Return<void> ret = fencedCallback->getExecutionInfo(
                        [&executionStatus, &timing](ErrorStatus error, Timing t, Timing) {
                            executionStatus = error;
                            timing = t;
                        });
                ASSERT_TRUE(ret.isOk());
            }
            break;
        }
    }

    if (testConfig.outputType != OutputType::FULLY_SPECIFIED &&
        executionStatus == ErrorStatus::GENERAL_FAILURE) {
        if (skipped != nullptr) {
            *skipped = true;
        }
        if (!testConfig.reportSkipping) {
            return;
        }
        LOG(INFO) << "NN VTS: Early termination of test because vendor service cannot "
                     "execute model that it does not support.";
        std::cout << "[          ]   Early termination of test because vendor service cannot "
                     "execute model that it does not support."
                  << std::endl;
        GTEST_SKIP();
    }
    if (testConfig.measureTiming == MeasureTiming::NO) {
        EXPECT_EQ(UINT64_MAX, timing.timeOnDevice);
        EXPECT_EQ(UINT64_MAX, timing.timeInDriver);
    } else {
        if (timing.timeOnDevice != UINT64_MAX && timing.timeInDriver != UINT64_MAX) {
            EXPECT_LE(timing.timeOnDevice, timing.timeInDriver);
        }
    }

    switch (testConfig.outputType) {
        case OutputType::FULLY_SPECIFIED:
            if (testConfig.executor == Executor::FENCED && hasZeroSizedOutput(testModel)) {
                // Executor::FENCED does not support zero-sized output.
                ASSERT_EQ(ErrorStatus::INVALID_ARGUMENT, executionStatus);
                return;
            }
            // If the model output operands are fully specified, outputShapes must be either
            // either empty, or have the same number of elements as the number of outputs.
            ASSERT_EQ(ErrorStatus::NONE, executionStatus);
            ASSERT_TRUE(outputShapes.size() == 0 ||
                        outputShapes.size() == testModel.main.outputIndexes.size());
            break;
        case OutputType::UNSPECIFIED:
            if (testConfig.executor == Executor::FENCED) {
                // For Executor::FENCED, the output shape must be fully specified.
                ASSERT_EQ(ErrorStatus::INVALID_ARGUMENT, executionStatus);
                return;
            }
            // If the model output operands are not fully specified, outputShapes must have
            // the same number of elements as the number of outputs.
            ASSERT_EQ(ErrorStatus::NONE, executionStatus);
            ASSERT_EQ(outputShapes.size(), testModel.main.outputIndexes.size());
            break;
        case OutputType::INSUFFICIENT:
            if (testConfig.executor == Executor::FENCED) {
                // For Executor::FENCED, the output shape must be fully specified.
                ASSERT_EQ(ErrorStatus::INVALID_ARGUMENT, executionStatus);
                return;
            }
            ASSERT_EQ(ErrorStatus::OUTPUT_INSUFFICIENT_SIZE, executionStatus);
            ASSERT_EQ(outputShapes.size(), testModel.main.outputIndexes.size());
            // Check that all returned output dimensions are at least as fully specified as the
            // union of the information about the corresponding operand in the model and in the
            // request. In this test, all model outputs have known rank with all dimensions
            // unspecified, and no dimensional information is provided in the request.
            for (uint32_t i = 0; i < outputShapes.size(); i++) {
                ASSERT_EQ(outputShapes[i].isSufficient, i != kInsufficientOutputIndex);
                const auto& actual = outputShapes[i].dimensions;
                const auto& golden =
                        testModel.main.operands[testModel.main.outputIndexes[i]].dimensions;
                ASSERT_EQ(actual.size(), golden.size());
                for (uint32_t j = 0; j < actual.size(); j++) {
                    if (actual[j] == 0) continue;
                    EXPECT_EQ(actual[j], golden[j]) << "index: " << j;
                }
            }
            return;
        case OutputType::MISSED_DEADLINE:
            ASSERT_TRUE(executionStatus == ErrorStatus::MISSED_DEADLINE_TRANSIENT ||
                        executionStatus == ErrorStatus::MISSED_DEADLINE_PERSISTENT)
                    << "executionStatus = " << executionStatus;
            return;
    }

    // Go through all outputs, check returned output shapes.
    for (uint32_t i = 0; i < outputShapes.size(); i++) {
        EXPECT_TRUE(outputShapes[i].isSufficient);
        const auto& expect = testModel.main.operands[testModel.main.outputIndexes[i]].dimensions;
        const std::vector<uint32_t> actual = outputShapes[i].dimensions;
        EXPECT_EQ(expect, actual);
    }

    // Retrieve execution results.
    const std::vector<TestBuffer> outputs = context.getOutputBuffers(testModel, request);

    // We want "close-enough" results.
    checkResults(testModel, outputs);
}

void EvaluatePreparedModel(const sp<IDevice>& device, const sp<IPreparedModel>& preparedModel,
                           const TestModel& testModel, TestKind testKind) {
    std::vector<OutputType> outputTypesList;
    std::vector<MeasureTiming> measureTimingList;
    std::vector<Executor> executorList;
    std::vector<MemoryType> memoryTypeList;

    switch (testKind) {
        case TestKind::GENERAL: {
            outputTypesList = {OutputType::FULLY_SPECIFIED};
            measureTimingList = {MeasureTiming::NO, MeasureTiming::YES};
            executorList = {Executor::ASYNC, Executor::SYNC, Executor::BURST};
            memoryTypeList = {MemoryType::ASHMEM};
        } break;
        case TestKind::DYNAMIC_SHAPE: {
            outputTypesList = {OutputType::UNSPECIFIED, OutputType::INSUFFICIENT};
            measureTimingList = {MeasureTiming::NO, MeasureTiming::YES};
            executorList = {Executor::ASYNC, Executor::SYNC, Executor::BURST, Executor::FENCED};
            memoryTypeList = {MemoryType::ASHMEM};
        } break;
        case TestKind::MEMORY_DOMAIN: {
            outputTypesList = {OutputType::FULLY_SPECIFIED};
            measureTimingList = {MeasureTiming::NO};
            executorList = {Executor::ASYNC, Executor::SYNC, Executor::FENCED};
            memoryTypeList = {MemoryType::BLOB_AHWB, MemoryType::DEVICE};
        } break;
        case TestKind::FENCED_COMPUTE: {
            outputTypesList = {OutputType::FULLY_SPECIFIED};
            measureTimingList = {MeasureTiming::NO, MeasureTiming::YES};
            executorList = {Executor::FENCED};
            memoryTypeList = {MemoryType::ASHMEM};
        } break;
        case TestKind::QUANTIZATION_COUPLING: {
            LOG(FATAL) << "Wrong TestKind for EvaluatePreparedModel";
            return;
        } break;
        case TestKind::INTINITE_LOOP_TIMEOUT: {
            outputTypesList = {OutputType::MISSED_DEADLINE};
            measureTimingList = {MeasureTiming::NO, MeasureTiming::YES};
            // Burst does not support V1_3 loop timeout.
            executorList = {Executor::ASYNC, Executor::SYNC, Executor::FENCED};
            memoryTypeList = {MemoryType::ASHMEM};
        } break;
    }

    for (const OutputType outputType : outputTypesList) {
        for (const MeasureTiming measureTiming : measureTimingList) {
            for (const Executor executor : executorList) {
                for (const MemoryType memoryType : memoryTypeList) {
                    const TestConfig testConfig(executor, measureTiming, outputType, memoryType);
                    EvaluatePreparedModel(device, preparedModel, testModel, testConfig);
                }
            }
        }
    }
}

void EvaluatePreparedCoupledModels(const sp<IDevice>& device,
                                   const sp<IPreparedModel>& preparedModel,
                                   const TestModel& testModel,
                                   const sp<IPreparedModel>& preparedCoupledModel,
                                   const TestModel& coupledModel) {
    const std::vector<OutputType> outputTypesList = {OutputType::FULLY_SPECIFIED};
    const std::vector<MeasureTiming> measureTimingList = {MeasureTiming::NO, MeasureTiming::YES};
    const std::vector<Executor> executorList = {Executor::ASYNC, Executor::SYNC, Executor::BURST,
                                                Executor::FENCED};

    for (const OutputType outputType : outputTypesList) {
        for (const MeasureTiming measureTiming : measureTimingList) {
            for (const Executor executor : executorList) {
                const TestConfig testConfig(executor, measureTiming, outputType, MemoryType::ASHMEM,
                                            /*reportSkipping=*/false);
                bool baseSkipped = false;
                EvaluatePreparedModel(device, preparedModel, testModel, testConfig, &baseSkipped);
                bool coupledSkipped = false;
                EvaluatePreparedModel(device, preparedCoupledModel, coupledModel, testConfig,
                                      &coupledSkipped);
                ASSERT_EQ(baseSkipped, coupledSkipped);
                if (baseSkipped) {
                    LOG(INFO) << "NN VTS: Early termination of test because vendor service cannot "
                                 "execute model that it does not support.";
                    std::cout << "[          ]   Early termination of test because vendor service "
                                 "cannot "
                                 "execute model that it does not support."
                              << std::endl;
                    GTEST_SKIP();
                }
            }
        }
    }
}

void Execute(const sp<IDevice>& device, const TestModel& testModel, TestKind testKind) {
    Model model = createModel(testModel);
    if (testKind == TestKind::DYNAMIC_SHAPE) {
        makeOutputDimensionsUnspecified(&model);
    }

    sp<IPreparedModel> preparedModel;
    switch (testKind) {
        case TestKind::GENERAL:
        case TestKind::DYNAMIC_SHAPE:
        case TestKind::MEMORY_DOMAIN:
        case TestKind::FENCED_COMPUTE:
        case TestKind::INTINITE_LOOP_TIMEOUT: {
            createPreparedModel(device, model, &preparedModel);
            if (preparedModel == nullptr) return;
            EvaluatePreparedModel(device, preparedModel, testModel, testKind);
        } break;
        case TestKind::QUANTIZATION_COUPLING: {
            ASSERT_TRUE(testModel.hasQuant8CoupledOperands());
            createPreparedModel(device, model, &preparedModel,
                                /*reportSkipping*/ false);
            TestModel signedQuantizedModel = convertQuant8AsymmOperandsToSigned(testModel);
            sp<IPreparedModel> preparedCoupledModel;
            createPreparedModel(device, createModel(signedQuantizedModel), &preparedCoupledModel,
                                /*reportSkipping*/ false);
            // If we couldn't prepare a model with unsigned quantization, we must
            // fail to prepare a model with signed quantization as well.
            if (preparedModel == nullptr) {
                ASSERT_EQ(preparedCoupledModel, nullptr);
                // If we failed to prepare both of the models, we can safely skip
                // the test.
                LOG(INFO) << "NN VTS: Early termination of test because vendor service cannot "
                             "prepare model that it does not support.";
                std::cout
                        << "[          ]   Early termination of test because vendor service cannot "
                           "prepare model that it does not support."
                        << std::endl;
                GTEST_SKIP();
            }
            ASSERT_NE(preparedCoupledModel, nullptr);
            EvaluatePreparedCoupledModels(device, preparedModel, testModel, preparedCoupledModel,
                                          signedQuantizedModel);
        } break;
    }
}

void GeneratedTestBase::SetUp() {
    testing::TestWithParam<GeneratedTestParam>::SetUp();
    ASSERT_NE(kDevice, nullptr);
}

std::vector<NamedModel> getNamedModels(const FilterFn& filter) {
    return TestModelManager::get().getTestModels(filter);
}

std::vector<NamedModel> getNamedModels(const FilterNameFn& filter) {
    return TestModelManager::get().getTestModels(filter);
}

std::string printGeneratedTest(const testing::TestParamInfo<GeneratedTestParam>& info) {
    const auto& [namedDevice, namedModel] = info.param;
    return gtestCompliantName(getName(namedDevice) + "_" + getName(namedModel));
}

// Tag for the generated tests
class GeneratedTest : public GeneratedTestBase {};

// Tag for the dynamic output shape tests
class DynamicOutputShapeTest : public GeneratedTest {};

// Tag for the memory domain tests
class MemoryDomainTest : public GeneratedTest {};

// Tag for the fenced compute tests
class FencedComputeTest : public GeneratedTest {};

// Tag for the dynamic output shape tests
class QuantizationCouplingTest : public GeneratedTest {};

// Tag for the loop timeout tests
class InfiniteLoopTimeoutTest : public GeneratedTest {};

TEST_P(GeneratedTest, Test) {
    Execute(kDevice, kTestModel, TestKind::GENERAL);
}

TEST_P(DynamicOutputShapeTest, Test) {
    Execute(kDevice, kTestModel, TestKind::DYNAMIC_SHAPE);
}

TEST_P(MemoryDomainTest, Test) {
    Execute(kDevice, kTestModel, TestKind::MEMORY_DOMAIN);
}

TEST_P(FencedComputeTest, Test) {
    Execute(kDevice, kTestModel, TestKind::FENCED_COMPUTE);
}

TEST_P(QuantizationCouplingTest, Test) {
    Execute(kDevice, kTestModel, TestKind::QUANTIZATION_COUPLING);
}

TEST_P(InfiniteLoopTimeoutTest, Test) {
    Execute(kDevice, kTestModel, TestKind::INTINITE_LOOP_TIMEOUT);
}

INSTANTIATE_GENERATED_TEST(GeneratedTest,
                           [](const TestModel& testModel) { return !testModel.expectFailure; });

INSTANTIATE_GENERATED_TEST(DynamicOutputShapeTest, [](const TestModel& testModel) {
    return !testModel.expectFailure && !testModel.hasScalarOutputs();
});

INSTANTIATE_GENERATED_TEST(MemoryDomainTest,
                           [](const TestModel& testModel) { return !testModel.expectFailure; });

INSTANTIATE_GENERATED_TEST(FencedComputeTest,
                           [](const TestModel& testModel) { return !testModel.expectFailure; });

INSTANTIATE_GENERATED_TEST(QuantizationCouplingTest, [](const TestModel& testModel) {
    return !testModel.expectFailure && testModel.hasQuant8CoupledOperands() &&
           testModel.main.operations.size() == 1;
});

INSTANTIATE_GENERATED_TEST(InfiniteLoopTimeoutTest, [](const TestModel& testModel) {
    return testModel.isInfiniteLoopTimeoutTest();
});

}  // namespace android::hardware::neuralnetworks::V1_3::vts::functional
