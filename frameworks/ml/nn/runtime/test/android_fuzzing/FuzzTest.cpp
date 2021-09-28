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

#include <android-base/logging.h>

#include <cstdlib>
#include <optional>
#include <utility>

#include "Converter.h"
#include "Model.pb.h"
#include "NeuralNetworksWrapper.h"
#include "TestHarness.h"
#include "src/libfuzzer/libfuzzer_macro.h"

namespace {

using ::android::nn::fuzz::convertToTestModel;
using ::android_nn_fuzz::Test;
using ::test_helper::TestModel;
using namespace ::android::nn::wrapper;
using namespace test_helper;

OperandType getOperandType(const TestOperand& op) {
    auto dims = op.dimensions;
    if (op.type == TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL) {
        return OperandType(
                static_cast<Type>(op.type), dims,
                SymmPerChannelQuantParams(op.channelQuant.scales, op.channelQuant.channelDim));
    } else {
        return OperandType(static_cast<Type>(op.type), dims, op.scale, op.zeroPoint);
    }
}

std::optional<Model> CreateModel(const TestModel& testModel) {
    Model model;

    // Operands.
    // TODO(b/148605565): Add control flow support
    CHECK_EQ(testModel.referenced.size(), 0u) << "Subgraphs not supported";
    for (const auto& operand : testModel.main.operands) {
        auto type = getOperandType(operand);
        auto index = model.addOperand(&type);

        switch (operand.lifetime) {
            case TestOperandLifeTime::CONSTANT_COPY:
            case TestOperandLifeTime::CONSTANT_REFERENCE:
                model.setOperandValue(index, operand.data.get<void>(), operand.data.size());
                break;
            case TestOperandLifeTime::NO_VALUE:
                model.setOperandValue(index, nullptr, 0);
                break;
            case TestOperandLifeTime::SUBGRAPH: {
                CHECK(false);
            } break;
            case TestOperandLifeTime::SUBGRAPH_INPUT:
            case TestOperandLifeTime::SUBGRAPH_OUTPUT:
            case TestOperandLifeTime::TEMPORARY_VARIABLE:
                // Nothing to do here.
                break;
        }
        if (!model.isValid()) return std::nullopt;
    }

    // Operations.
    CHECK_EQ(testModel.referenced.size(), 0u) << "Subgraphs not supported";
    for (const auto& operation : testModel.main.operations) {
        model.addOperation(static_cast<int>(operation.type), operation.inputs, operation.outputs);
        if (!model.isValid()) return std::nullopt;
    }

    // Inputs and outputs.
    model.identifyInputsAndOutputs(testModel.main.inputIndexes, testModel.main.outputIndexes);
    if (!model.isValid()) return std::nullopt;

    // Relaxed computation.
    model.relaxComputationFloat32toFloat16(testModel.isRelaxed);
    if (!model.isValid()) return std::nullopt;

    if (model.finish() != Result::NO_ERROR) {
        return std::nullopt;
    }

    return model;
}

std::optional<Compilation> CreateCompilation(const Model& model) {
    Compilation compilation(&model);
    if (compilation.finish() != Result::NO_ERROR) {
        return std::nullopt;
    }
    return compilation;
}

std::optional<Execution> CreateExecution(const Compilation& compilation,
                                         const TestModel& testModel) {
    Execution execution(&compilation);

    // Model inputs.
    for (uint32_t i = 0; i < testModel.main.inputIndexes.size(); i++) {
        const auto& operand = testModel.main.operands[testModel.main.inputIndexes[i]];
        if (execution.setInput(i, operand.data.get<void>(), operand.data.size()) !=
            Result::NO_ERROR) {
            return std::nullopt;
        }
    }

    // Model outputs.
    for (uint32_t i = 0; i < testModel.main.outputIndexes.size(); i++) {
        const auto& operand = testModel.main.operands[testModel.main.outputIndexes[i]];
        if (execution.setOutput(i, const_cast<void*>(operand.data.get<void>()),
                                operand.data.size()) != Result::NO_ERROR) {
            return std::nullopt;
        }
    }

    return execution;
}

void runTest(const TestModel& testModel) {
    // set up model
    auto model = CreateModel(testModel);
    if (!model.has_value()) {
        return;
    }

    // set up compilation
    auto compilation = CreateCompilation(*model);
    if (!compilation.has_value()) {
        return;
    }

    // set up execution
    auto execution = CreateExecution(*compilation, testModel);
    if (!execution.has_value()) {
        return;
    }

    // perform execution
    execution->compute();
}

}  // anonymous namespace

DEFINE_PROTO_FUZZER(const Test& model) {
    const TestModel testModel = convertToTestModel(model);
    runTest(testModel);
}
