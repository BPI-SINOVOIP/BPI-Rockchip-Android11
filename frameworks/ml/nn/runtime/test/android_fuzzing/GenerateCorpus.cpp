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

#include <google/protobuf/text_format.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Converter.h"

namespace android::nn::fuzz {
namespace {

using namespace test_helper;
using namespace android_nn_fuzz;

OperandType convert(TestOperandType type) {
    return static_cast<OperandType>(type);
}

OperationType convert(TestOperationType type) {
    return static_cast<OperationType>(type);
}

OperandLifeTime convert(TestOperandLifeTime lifetime) {
    return static_cast<OperandLifeTime>(lifetime);
}

Scales convert(const std::vector<float>& scales) {
    Scales protoScales;
    for (float scale : scales) {
        protoScales.add_scale(scale);
    }
    return protoScales;
}

SymmPerChannelQuantParams convert(const TestSymmPerChannelQuantParams& params) {
    SymmPerChannelQuantParams protoParams;
    *protoParams.mutable_scales() = convert(params.scales);
    protoParams.set_channel_dim(params.channelDim);
    return protoParams;
}

Dimensions convertDimensions(const std::vector<uint32_t>& dimensions) {
    Dimensions protoDimensions;
    for (uint32_t dimension : dimensions) {
        protoDimensions.add_dimension(dimension);
    }
    return protoDimensions;
}

uint32_t getHashValue(const TestBuffer& buffer) {
    const char* ptr = buffer.get<char>();
    const size_t size = buffer.size();
    const std::string_view view(ptr, size);
    const size_t value = std::hash<std::string_view>{}(view);
    return static_cast<uint32_t>(value & 0xFFFFFFFF);
}

Buffer convert(bool noValue, const TestBuffer& buffer) {
    Buffer protoBuffer;
    const uint32_t randomSeed = (noValue ? 0 : getHashValue(buffer));
    protoBuffer.set_random_seed(randomSeed);
    return protoBuffer;
}

Operand convert(const TestOperand& operand) {
    Operand protoOperand;
    protoOperand.set_type(convert(operand.type));
    *protoOperand.mutable_dimensions() = convertDimensions(operand.dimensions);
    protoOperand.set_scale(operand.scale);
    protoOperand.set_zero_point(operand.zeroPoint);
    protoOperand.set_lifetime(convert(operand.lifetime));
    *protoOperand.mutable_channel_quant() = convert(operand.channelQuant);
    const bool noValue = (operand.lifetime == TestOperandLifeTime::NO_VALUE);
    *protoOperand.mutable_data() = convert(noValue, operand.data);
    return protoOperand;
}

Operands convert(const std::vector<TestOperand>& operands) {
    Operands protoOperands;
    for (const auto& operand : operands) {
        *protoOperands.add_operand() = convert(operand);
    }
    return protoOperands;
}

Indexes convertIndexes(const std::vector<uint32_t>& indexes) {
    Indexes protoIndexes;
    for (uint32_t index : indexes) {
        protoIndexes.add_index(index);
    }
    return protoIndexes;
}

Operation convert(const TestOperation& operation) {
    Operation protoOperation;
    protoOperation.set_type(convert(operation.type));
    *protoOperation.mutable_inputs() = convertIndexes(operation.inputs);
    *protoOperation.mutable_outputs() = convertIndexes(operation.outputs);
    return protoOperation;
}

Operations convert(const std::vector<TestOperation>& operations) {
    Operations protoOperations;
    for (const auto& operation : operations) {
        *protoOperations.add_operation() = convert(operation);
    }
    return protoOperations;
}

Model convert(const TestModel& model) {
    Model protoModel;
    *protoModel.mutable_operands() = convert(model.operands);
    *protoModel.mutable_operations() = convert(model.operations);
    *protoModel.mutable_input_indexes() = convertIndexes(model.inputIndexes);
    *protoModel.mutable_output_indexes() = convertIndexes(model.outputIndexes);
    protoModel.set_is_relaxed(model.isRelaxed);
    return protoModel;
}

Test convertToTest(const TestModel& model) {
    Test protoTest;
    *protoTest.mutable_model() = convert(model);
    return protoTest;
}

std::string saveMessageAsText(const google::protobuf::Message& message) {
    std::string str;
    if (!google::protobuf::TextFormat::PrintToString(message, &str)) {
        return {};
    }
    return str;
}

void createCorpusEntry(const std::pair<std::string, const TestModel*>& testCase,
                       const std::string& genDir) {
    const auto& [testName, testModel] = testCase;
    const Test test = convertToTest(*testModel);
    const std::string contents = saveMessageAsText(test);
    const std::string fullName = genDir + "/" + testName;
    std::ofstream file(fullName.c_str());
    if (file.good()) {
        file << contents;
    }
}

}  // anonymous namespace
}  // namespace android::nn::fuzz

using ::android::nn::fuzz::createCorpusEntry;
using ::test_helper::TestModel;
using ::test_helper::TestModelManager;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "error: nnapi_fuzz_generate_corpus requires one argument" << std::endl;
        return -1;
    }
    const std::string genDir = argv[1];
    const auto filter = [](const TestModel& testModel) { return !testModel.expectFailure; };
    const auto testModels = TestModelManager::get().getTestModels(filter);
    std::for_each(testModels.begin(), testModels.end(),
                  [&genDir](const auto& testCase) { createCorpusEntry(testCase, genDir); });
    return EXIT_SUCCESS;
}
