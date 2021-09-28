/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "GeneratedTestUtils.h"

#include <android-base/logging.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "TestHarness.h"
#include "TestNeuralNetworksWrapper.h"

namespace android::nn::generated_tests {
using namespace test_wrapper;
using namespace test_helper;

static OperandType getOperandType(const TestOperand& op, bool testDynamicOutputShape) {
    auto dims = op.dimensions;
    if (testDynamicOutputShape && op.lifetime == TestOperandLifeTime::SUBGRAPH_OUTPUT) {
        dims.assign(dims.size(), 0);
    }
    if (op.type == TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        return OperandType(
                static_cast<Type>(op.type), dims,
                SymmPerChannelQuantParams(op.channelQuant.scales, op.channelQuant.channelDim));
    } else {
        return OperandType(static_cast<Type>(op.type), dims, op.scale, op.zeroPoint);
    }
}

// A Memory object that owns AHardwareBuffer
class MemoryAHWB : public Memory {
   public:
    static std::unique_ptr<MemoryAHWB> create(uint32_t size) {
        const uint64_t usage =
                AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN | AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN;
        AHardwareBuffer_Desc desc = {
                .width = size,
                .height = 1,
                .layers = 1,
                .format = AHARDWAREBUFFER_FORMAT_BLOB,
                .usage = usage,
        };
        AHardwareBuffer* ahwb = nullptr;
        EXPECT_EQ(AHardwareBuffer_allocate(&desc, &ahwb), 0);
        EXPECT_NE(ahwb, nullptr);

        void* buffer = nullptr;
        EXPECT_EQ(AHardwareBuffer_lock(ahwb, usage, -1, nullptr, &buffer), 0);
        EXPECT_NE(buffer, nullptr);

        return std::unique_ptr<MemoryAHWB>(new MemoryAHWB(ahwb, buffer));
    }

    ~MemoryAHWB() override {
        EXPECT_EQ(AHardwareBuffer_unlock(mAhwb, nullptr), 0);
        AHardwareBuffer_release(mAhwb);
    }

    void* getPointer() const { return mBuffer; }

   private:
    MemoryAHWB(AHardwareBuffer* ahwb, void* buffer) : Memory(ahwb), mAhwb(ahwb), mBuffer(buffer) {}

    AHardwareBuffer* mAhwb;
    void* mBuffer;
};

static std::unique_ptr<MemoryAHWB> createConstantReferenceMemory(const TestModel& testModel) {
    uint32_t size = 0;

    auto processSubgraph = [&size](const TestSubgraph& subgraph) {
        for (const TestOperand& operand : subgraph.operands) {
            if (operand.lifetime == TestOperandLifeTime::CONSTANT_REFERENCE) {
                size += operand.data.alignedSize();
            }
        }
    };

    processSubgraph(testModel.main);
    for (const TestSubgraph& subgraph : testModel.referenced) {
        processSubgraph(subgraph);
    }
    return size == 0 ? nullptr : MemoryAHWB::create(size);
}

static void createModelFromSubgraph(const TestSubgraph& subgraph, bool testDynamicOutputShape,
                                    const std::vector<TestSubgraph>& refSubgraphs,
                                    const std::unique_ptr<MemoryAHWB>& memory,
                                    uint32_t* memoryOffset, Model* model, Model* refModels) {
    // Operands.
    for (const auto& operand : subgraph.operands) {
        auto type = getOperandType(operand, testDynamicOutputShape);
        auto index = model->addOperand(&type);

        switch (operand.lifetime) {
            case TestOperandLifeTime::CONSTANT_COPY: {
                model->setOperandValue(index, operand.data.get<void>(), operand.data.size());
            } break;
            case TestOperandLifeTime::CONSTANT_REFERENCE: {
                const uint32_t length = operand.data.size();
                std::memcpy(static_cast<uint8_t*>(memory->getPointer()) + *memoryOffset,
                            operand.data.get<void>(), length);
                model->setOperandValueFromMemory(index, memory.get(), *memoryOffset, length);
                *memoryOffset += operand.data.alignedSize();
            } break;
            case TestOperandLifeTime::NO_VALUE: {
                model->setOperandValue(index, nullptr, 0);
            } break;
            case TestOperandLifeTime::SUBGRAPH: {
                uint32_t refIndex = *operand.data.get<uint32_t>();
                CHECK_LT(refIndex, refSubgraphs.size());
                const TestSubgraph& refSubgraph = refSubgraphs[refIndex];
                Model* refModel = &refModels[refIndex];
                if (!refModel->isFinished()) {
                    createModelFromSubgraph(refSubgraph, testDynamicOutputShape, refSubgraphs,
                                            memory, memoryOffset, refModel, refModels);
                    ASSERT_EQ(refModel->finish(), Result::NO_ERROR);
                    ASSERT_TRUE(refModel->isValid());
                }
                model->setOperandValueFromModel(index, refModel);
            } break;
            case TestOperandLifeTime::SUBGRAPH_INPUT:
            case TestOperandLifeTime::SUBGRAPH_OUTPUT:
            case TestOperandLifeTime::TEMPORARY_VARIABLE: {
                // Nothing to do here.
            } break;
        }
    }

    // Operations.
    for (const auto& operation : subgraph.operations) {
        model->addOperation(static_cast<int>(operation.type), operation.inputs, operation.outputs);
    }

    // Inputs and outputs.
    model->identifyInputsAndOutputs(subgraph.inputIndexes, subgraph.outputIndexes);
}

void createModel(const TestModel& testModel, bool testDynamicOutputShape, GeneratedModel* model) {
    ASSERT_NE(nullptr, model);

    std::unique_ptr<MemoryAHWB> memory = createConstantReferenceMemory(testModel);
    uint32_t memoryOffset = 0;
    std::vector<Model> refModels(testModel.referenced.size());
    createModelFromSubgraph(testModel.main, testDynamicOutputShape, testModel.referenced, memory,
                            &memoryOffset, model, refModels.data());
    model->setRefModels(std::move(refModels));
    model->setConstantReferenceMemory(std::move(memory));

    // Relaxed computation.
    model->relaxComputationFloat32toFloat16(testModel.isRelaxed);

    if (!testModel.expectFailure) {
        ASSERT_TRUE(model->isValid());
    }
}

void createRequest(const TestModel& testModel, Execution* execution,
                   std::vector<TestBuffer>* outputs) {
    ASSERT_NE(nullptr, execution);
    ASSERT_NE(nullptr, outputs);

    // Model inputs.
    for (uint32_t i = 0; i < testModel.main.inputIndexes.size(); i++) {
        const auto& operand = testModel.main.operands[testModel.main.inputIndexes[i]];
        ASSERT_EQ(Result::NO_ERROR,
                  execution->setInput(i, operand.data.get<void>(), operand.data.size()));
    }

    // Model outputs.
    for (uint32_t i = 0; i < testModel.main.outputIndexes.size(); i++) {
        const auto& operand = testModel.main.operands[testModel.main.outputIndexes[i]];

        // In the case of zero-sized output, we should at least provide a one-byte buffer.
        // This is because zero-sized tensors are only supported internally to the runtime, or
        // reported in output shapes. It is illegal for the client to pre-specify a zero-sized
        // tensor as model output. Otherwise, we will have two semantic conflicts:
        // - "Zero dimension" conflicts with "unspecified dimension".
        // - "Omitted operand buffer" conflicts with "zero-sized operand buffer".
        const size_t bufferSize = std::max<size_t>(operand.data.size(), 1);

        outputs->emplace_back(bufferSize);
        ASSERT_EQ(Result::NO_ERROR,
                  execution->setOutput(i, outputs->back().getMutable<void>(), bufferSize));
    }
}

}  // namespace android::nn::generated_tests
