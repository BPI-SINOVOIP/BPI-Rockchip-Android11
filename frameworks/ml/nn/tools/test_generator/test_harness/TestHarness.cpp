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

#include "TestHarness.h"

#include <android-base/logging.h>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

namespace test_helper {

namespace {

template <typename T>
constexpr bool nnIsFloat = std::is_floating_point_v<T> || std::is_same_v<T, _Float16>;

constexpr uint32_t kMaxNumberOfPrintedErrors = 10;

// TODO(b/139442217): Allow passing accuracy criteria from spec.
// Currently we only need relaxed accuracy criteria on mobilenet tests, so we return the quant8
// tolerance simply based on the current test name.
int getQuant8AllowedError() {
    const ::testing::TestInfo* const testInfo =
            ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string testCaseName = testInfo->test_case_name();
    const std::string testName = testInfo->name();
    // We relax the quant8 precision for all tests with mobilenet:
    // - CTS/VTS GeneratedTest and DynamicOutputShapeTest with mobilenet
    // - VTS CompilationCachingTest and CompilationCachingSecurityTest except for TOCTOU tests
    if (testName.find("mobilenet") != std::string::npos ||
        (testCaseName.find("CompilationCaching") != std::string::npos &&
         testName.find("TOCTOU") == std::string::npos)) {
        return 3;
    } else {
        return 1;
    }
}

uint32_t getNumberOfElements(const TestOperand& op) {
    return std::reduce(op.dimensions.begin(), op.dimensions.end(), 1u, std::multiplies<uint32_t>());
}

// Check if the actual results meet the accuracy criterion.
template <typename T>
void expectNear(const TestOperand& op, const TestBuffer& result, const AccuracyCriterion& criterion,
                bool allowInvalid = false) {
    constexpr uint32_t kMinNumberOfElementsToTestBiasMSE = 10;
    const T* actualBuffer = result.get<T>();
    const T* expectedBuffer = op.data.get<T>();
    uint32_t len = getNumberOfElements(op), numErrors = 0, numSkip = 0;
    double bias = 0.0f, mse = 0.0f;
    for (uint32_t i = 0; i < len; i++) {
        // Compare all data types in double for precision and signed arithmetic.
        double actual = static_cast<double>(actualBuffer[i]);
        double expected = static_cast<double>(expectedBuffer[i]);
        double tolerableRange = criterion.atol + criterion.rtol * std::fabs(expected);
        EXPECT_FALSE(std::isnan(expected));

        // Skip invalid floating point values.
        if (allowInvalid &&
            (std::isinf(expected) || (std::is_same_v<T, float> && std::fabs(expected) > 1e3) ||
             (std::is_same_v<T, _Float16> || std::fabs(expected) > 1e2))) {
            numSkip++;
            continue;
        }

        // Accumulate bias and MSE. Use relative bias and MSE for floating point values.
        double diff = actual - expected;
        if constexpr (nnIsFloat<T>) {
            diff /= std::max(1.0, std::abs(expected));
        }
        bias += diff;
        mse += diff * diff;

        // Print at most kMaxNumberOfPrintedErrors errors by EXPECT_NEAR.
        if (numErrors < kMaxNumberOfPrintedErrors) {
            EXPECT_NEAR(expected, actual, tolerableRange) << "When comparing element " << i;
        }
        if (std::fabs(actual - expected) > tolerableRange) numErrors++;
    }
    EXPECT_EQ(numErrors, 0u);

    // Test bias and MSE.
    if (len < numSkip + kMinNumberOfElementsToTestBiasMSE) return;
    bias /= static_cast<double>(len - numSkip);
    mse /= static_cast<double>(len - numSkip);
    EXPECT_LE(std::fabs(bias), criterion.bias);
    EXPECT_LE(mse, criterion.mse);
}

// For boolean values, we expect the number of mismatches does not exceed a certain ratio.
void expectBooleanNearlyEqual(const TestOperand& op, const TestBuffer& result,
                              float allowedErrorRatio) {
    const bool8* actualBuffer = result.get<bool8>();
    const bool8* expectedBuffer = op.data.get<bool8>();
    uint32_t len = getNumberOfElements(op), numErrors = 0;
    std::stringstream errorMsg;
    for (uint32_t i = 0; i < len; i++) {
        if (expectedBuffer[i] != actualBuffer[i]) {
            if (numErrors < kMaxNumberOfPrintedErrors)
                errorMsg << "    Expected: " << expectedBuffer[i] << ", actual: " << actualBuffer[i]
                         << ", when comparing element " << i << "\n";
            numErrors++;
        }
    }
    // When |len| is small, the allowedErrorCount will intentionally ceil at 1, which allows for
    // greater tolerance.
    uint32_t allowedErrorCount = static_cast<uint32_t>(std::ceil(allowedErrorRatio * len));
    EXPECT_LE(numErrors, allowedErrorCount) << errorMsg.str();
}

// Calculates the expected probability from the unnormalized log-probability of
// each class in the input and compares it to the actual occurrence of that class
// in the output.
void expectMultinomialDistributionWithinTolerance(const TestModel& model,
                                                  const std::vector<TestBuffer>& buffers) {
    // This function is only for RANDOM_MULTINOMIAL single-operation test.
    CHECK_EQ(model.referenced.size(), 0u) << "Subgraphs not supported";
    ASSERT_EQ(model.main.operations.size(), 1u);
    ASSERT_EQ(model.main.operations[0].type, TestOperationType::RANDOM_MULTINOMIAL);
    ASSERT_EQ(model.main.inputIndexes.size(), 1u);
    ASSERT_EQ(model.main.outputIndexes.size(), 1u);
    ASSERT_EQ(buffers.size(), 1u);

    const auto& inputOperand = model.main.operands[model.main.inputIndexes[0]];
    const auto& outputOperand = model.main.operands[model.main.outputIndexes[0]];
    ASSERT_EQ(inputOperand.dimensions.size(), 2u);
    ASSERT_EQ(outputOperand.dimensions.size(), 2u);

    const int kBatchSize = inputOperand.dimensions[0];
    const int kNumClasses = inputOperand.dimensions[1];
    const int kNumSamples = outputOperand.dimensions[1];

    const uint32_t outputLength = getNumberOfElements(outputOperand);
    const int32_t* outputData = buffers[0].get<int32_t>();
    std::vector<int> classCounts(kNumClasses);
    for (uint32_t i = 0; i < outputLength; i++) {
        classCounts[outputData[i]]++;
    }

    const uint32_t inputLength = getNumberOfElements(inputOperand);
    std::vector<float> inputData(inputLength);
    if (inputOperand.type == TestOperandType::TENSOR_FLOAT32) {
        const float* inputRaw = inputOperand.data.get<float>();
        std::copy(inputRaw, inputRaw + inputLength, inputData.begin());
    } else if (inputOperand.type == TestOperandType::TENSOR_FLOAT16) {
        const _Float16* inputRaw = inputOperand.data.get<_Float16>();
        std::transform(inputRaw, inputRaw + inputLength, inputData.begin(),
                       [](_Float16 fp16) { return static_cast<float>(fp16); });
    } else {
        FAIL() << "Unknown input operand type for RANDOM_MULTINOMIAL.";
    }

    for (int b = 0; b < kBatchSize; ++b) {
        float probabilitySum = 0;
        const int batchIndex = kBatchSize * b;
        for (int i = 0; i < kNumClasses; ++i) {
            probabilitySum += expf(inputData[batchIndex + i]);
        }
        for (int i = 0; i < kNumClasses; ++i) {
            float probability =
                    static_cast<float>(classCounts[i]) / static_cast<float>(kNumSamples);
            float probabilityExpected = expf(inputData[batchIndex + i]) / probabilitySum;
            EXPECT_THAT(probability,
                        ::testing::FloatNear(probabilityExpected,
                                             model.expectedMultinomialDistributionTolerance));
        }
    }
}

}  // namespace

void checkResults(const TestModel& model, const std::vector<TestBuffer>& buffers,
                  const AccuracyCriteria& criteria) {
    ASSERT_EQ(model.main.outputIndexes.size(), buffers.size());
    for (uint32_t i = 0; i < model.main.outputIndexes.size(); i++) {
        const uint32_t outputIndex = model.main.outputIndexes[i];
        SCOPED_TRACE(testing::Message()
                     << "When comparing output " << i << " (op" << outputIndex << ")");
        const auto& operand = model.main.operands[outputIndex];
        const auto& result = buffers[i];
        if (operand.isIgnored) continue;

        switch (operand.type) {
            case TestOperandType::TENSOR_FLOAT32:
                expectNear<float>(operand, result, criteria.float32, criteria.allowInvalidFpValues);
                break;
            case TestOperandType::TENSOR_FLOAT16:
                expectNear<_Float16>(operand, result, criteria.float16,
                                     criteria.allowInvalidFpValues);
                break;
            case TestOperandType::TENSOR_INT32:
            case TestOperandType::INT32:
                expectNear<int32_t>(operand, result, criteria.int32);
                break;
            case TestOperandType::TENSOR_QUANT8_ASYMM:
                expectNear<uint8_t>(operand, result, criteria.quant8Asymm);
                break;
            case TestOperandType::TENSOR_QUANT8_SYMM:
                expectNear<int8_t>(operand, result, criteria.quant8Symm);
                break;
            case TestOperandType::TENSOR_QUANT16_ASYMM:
                expectNear<uint16_t>(operand, result, criteria.quant16Asymm);
                break;
            case TestOperandType::TENSOR_QUANT16_SYMM:
                expectNear<int16_t>(operand, result, criteria.quant16Symm);
                break;
            case TestOperandType::TENSOR_BOOL8:
                expectBooleanNearlyEqual(operand, result, criteria.bool8AllowedErrorRatio);
                break;
            case TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED:
                expectNear<int8_t>(operand, result, criteria.quant8AsymmSigned);
                break;
            default:
                FAIL() << "Data type not supported.";
        }
    }
}

void checkResults(const TestModel& model, const std::vector<TestBuffer>& buffers) {
    // For RANDOM_MULTINOMIAL test only.
    if (model.expectedMultinomialDistributionTolerance > 0.0f) {
        expectMultinomialDistributionWithinTolerance(model, buffers);
        return;
    }

    // Decide the default tolerable range.
    //
    // For floating-point models, we use the relaxed precision if either
    // - relaxed computation flag is set
    // - the model has at least one TENSOR_FLOAT16 operand
    //
    // The bias and MSE criteria are implicitly set to the maximum -- we do not enforce these
    // criteria in normal generated tests.
    //
    // TODO: Adjust the error limit based on testing.
    //
    AccuracyCriteria criteria = {
            // The relative tolerance is 5ULP of FP32.
            .float32 = {.atol = 1e-5, .rtol = 5.0f * 1.1920928955078125e-7},
            // Both the absolute and relative tolerance are 5ULP of FP16.
            .float16 = {.atol = 5.0f * 0.0009765625, .rtol = 5.0f * 0.0009765625},
            .int32 = {.atol = 1},
            .quant8Asymm = {.atol = 1},
            .quant8Symm = {.atol = 1},
            .quant16Asymm = {.atol = 1},
            .quant16Symm = {.atol = 1},
            .bool8AllowedErrorRatio = 0.0f,
            // Since generated tests are hand-calculated, there should be no invalid FP values.
            .allowInvalidFpValues = false,
    };
    bool hasFloat16Inputs = false;
    model.forEachSubgraph([&hasFloat16Inputs](const TestSubgraph& subgraph) {
        if (!hasFloat16Inputs) {
            hasFloat16Inputs = std::any_of(subgraph.operands.begin(), subgraph.operands.end(),
                                           [](const TestOperand& op) {
                                               return op.type == TestOperandType::TENSOR_FLOAT16;
                                           });
        }
    });
    if (model.isRelaxed || hasFloat16Inputs) {
        criteria.float32 = criteria.float16;
    }
    const double quant8AllowedError = getQuant8AllowedError();
    criteria.quant8Asymm.atol = quant8AllowedError;
    criteria.quant8AsymmSigned.atol = quant8AllowedError;
    criteria.quant8Symm.atol = quant8AllowedError;

    checkResults(model, buffers, criteria);
}

TestModel convertQuant8AsymmOperandsToSigned(const TestModel& testModel) {
    auto processSubgraph = [](TestSubgraph* subgraph) {
        for (TestOperand& operand : subgraph->operands) {
            if (operand.type == test_helper::TestOperandType::TENSOR_QUANT8_ASYMM) {
                operand.type = test_helper::TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED;
                operand.zeroPoint -= 128;
                const uint8_t* inputOperandData = operand.data.get<uint8_t>();
                int8_t* outputOperandData = operand.data.getMutable<int8_t>();
                for (size_t i = 0; i < operand.data.size(); ++i) {
                    outputOperandData[i] =
                            static_cast<int8_t>(static_cast<int32_t>(inputOperandData[i]) - 128);
                }
            }
        }
    };
    TestModel converted(testModel.copy());
    processSubgraph(&converted.main);
    for (TestSubgraph& subgraph : converted.referenced) {
        processSubgraph(&subgraph);
    }
    return converted;
}

bool isQuantizedType(TestOperandType type) {
    static const std::set<TestOperandType> kQuantizedTypes = {
            TestOperandType::TENSOR_QUANT8_ASYMM,
            TestOperandType::TENSOR_QUANT8_SYMM,
            TestOperandType::TENSOR_QUANT16_ASYMM,
            TestOperandType::TENSOR_QUANT16_SYMM,
            TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL,
            TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED,
    };
    return kQuantizedTypes.count(type) > 0;
}

bool isFloatType(TestOperandType type) {
    static const std::set<TestOperandType> kFloatTypes = {
            TestOperandType::TENSOR_FLOAT32,
            TestOperandType::TENSOR_FLOAT16,
            TestOperandType::FLOAT32,
            TestOperandType::FLOAT16,
    };
    return kFloatTypes.count(type) > 0;
}

bool isConstant(TestOperandLifeTime lifetime) {
    return lifetime == TestOperandLifeTime::CONSTANT_COPY ||
           lifetime == TestOperandLifeTime::CONSTANT_REFERENCE;
}

namespace {

const char* kOperationTypeNames[] = {
        "ADD",
        "AVERAGE_POOL_2D",
        "CONCATENATION",
        "CONV_2D",
        "DEPTHWISE_CONV_2D",
        "DEPTH_TO_SPACE",
        "DEQUANTIZE",
        "EMBEDDING_LOOKUP",
        "FLOOR",
        "FULLY_CONNECTED",
        "HASHTABLE_LOOKUP",
        "L2_NORMALIZATION",
        "L2_POOL",
        "LOCAL_RESPONSE_NORMALIZATION",
        "LOGISTIC",
        "LSH_PROJECTION",
        "LSTM",
        "MAX_POOL_2D",
        "MUL",
        "RELU",
        "RELU1",
        "RELU6",
        "RESHAPE",
        "RESIZE_BILINEAR",
        "RNN",
        "SOFTMAX",
        "SPACE_TO_DEPTH",
        "SVDF",
        "TANH",
        "BATCH_TO_SPACE_ND",
        "DIV",
        "MEAN",
        "PAD",
        "SPACE_TO_BATCH_ND",
        "SQUEEZE",
        "STRIDED_SLICE",
        "SUB",
        "TRANSPOSE",
        "ABS",
        "ARGMAX",
        "ARGMIN",
        "AXIS_ALIGNED_BBOX_TRANSFORM",
        "BIDIRECTIONAL_SEQUENCE_LSTM",
        "BIDIRECTIONAL_SEQUENCE_RNN",
        "BOX_WITH_NMS_LIMIT",
        "CAST",
        "CHANNEL_SHUFFLE",
        "DETECTION_POSTPROCESSING",
        "EQUAL",
        "EXP",
        "EXPAND_DIMS",
        "GATHER",
        "GENERATE_PROPOSALS",
        "GREATER",
        "GREATER_EQUAL",
        "GROUPED_CONV_2D",
        "HEATMAP_MAX_KEYPOINT",
        "INSTANCE_NORMALIZATION",
        "LESS",
        "LESS_EQUAL",
        "LOG",
        "LOGICAL_AND",
        "LOGICAL_NOT",
        "LOGICAL_OR",
        "LOG_SOFTMAX",
        "MAXIMUM",
        "MINIMUM",
        "NEG",
        "NOT_EQUAL",
        "PAD_V2",
        "POW",
        "PRELU",
        "QUANTIZE",
        "QUANTIZED_16BIT_LSTM",
        "RANDOM_MULTINOMIAL",
        "REDUCE_ALL",
        "REDUCE_ANY",
        "REDUCE_MAX",
        "REDUCE_MIN",
        "REDUCE_PROD",
        "REDUCE_SUM",
        "ROI_ALIGN",
        "ROI_POOLING",
        "RSQRT",
        "SELECT",
        "SIN",
        "SLICE",
        "SPLIT",
        "SQRT",
        "TILE",
        "TOPK_V2",
        "TRANSPOSE_CONV_2D",
        "UNIDIRECTIONAL_SEQUENCE_LSTM",
        "UNIDIRECTIONAL_SEQUENCE_RNN",
        "RESIZE_NEAREST_NEIGHBOR",
        "QUANTIZED_LSTM",
        "IF",
        "WHILE",
        "ELU",
        "HARD_SWISH",
        "FILL",
        "RANK",
};

const char* kOperandTypeNames[] = {
        "FLOAT32",
        "INT32",
        "UINT32",
        "TENSOR_FLOAT32",
        "TENSOR_INT32",
        "TENSOR_QUANT8_ASYMM",
        "BOOL",
        "TENSOR_QUANT16_SYMM",
        "TENSOR_FLOAT16",
        "TENSOR_BOOL8",
        "FLOAT16",
        "TENSOR_QUANT8_SYMM_PER_CHANNEL",
        "TENSOR_QUANT16_ASYMM",
        "TENSOR_QUANT8_SYMM",
        "TENSOR_QUANT8_ASYMM_SIGNED",
};

bool isScalarType(TestOperandType type) {
    static const std::vector<bool> kIsScalarOperandType = {
            true,   // TestOperandType::FLOAT32
            true,   // TestOperandType::INT32
            true,   // TestOperandType::UINT32
            false,  // TestOperandType::TENSOR_FLOAT32
            false,  // TestOperandType::TENSOR_INT32
            false,  // TestOperandType::TENSOR_QUANT8_ASYMM
            true,   // TestOperandType::BOOL
            false,  // TestOperandType::TENSOR_QUANT16_SYMM
            false,  // TestOperandType::TENSOR_FLOAT16
            false,  // TestOperandType::TENSOR_BOOL8
            true,   // TestOperandType::FLOAT16
            false,  // TestOperandType::TENSOR_QUANT8_SYMM_PER_CHANNEL
            false,  // TestOperandType::TENSOR_QUANT16_ASYMM
            false,  // TestOperandType::TENSOR_QUANT8_SYMM
            false,  // TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED
    };
    return kIsScalarOperandType[static_cast<int>(type)];
}

std::string getOperandClassInSpecFile(TestOperandLifeTime lifetime) {
    switch (lifetime) {
        case TestOperandLifeTime::SUBGRAPH_INPUT:
            return "Input";
        case TestOperandLifeTime::SUBGRAPH_OUTPUT:
            return "Output";
        case TestOperandLifeTime::CONSTANT_COPY:
        case TestOperandLifeTime::CONSTANT_REFERENCE:
        case TestOperandLifeTime::NO_VALUE:
            return "Parameter";
        case TestOperandLifeTime::TEMPORARY_VARIABLE:
            return "Internal";
        default:
            CHECK(false);
            return "";
    }
}

template <typename T>
std::string defaultToStringFunc(const T& value) {
    return std::to_string(value);
};
template <>
std::string defaultToStringFunc<_Float16>(const _Float16& value) {
    return defaultToStringFunc(static_cast<float>(value));
};

// Dump floating point values in hex representation.
template <typename T>
std::string toHexFloatString(const T& value);
template <>
std::string toHexFloatString<float>(const float& value) {
    std::stringstream ss;
    ss << "\"" << std::hexfloat << value << "\"";
    return ss.str();
};
template <>
std::string toHexFloatString<_Float16>(const _Float16& value) {
    return toHexFloatString(static_cast<float>(value));
};

template <typename Iterator, class ToStringFunc>
std::string join(const std::string& joint, Iterator begin, Iterator end, ToStringFunc func) {
    std::stringstream ss;
    for (auto it = begin; it < end; it++) {
        ss << (it == begin ? "" : joint) << func(*it);
    }
    return ss.str();
}

template <typename T, class ToStringFunc>
std::string join(const std::string& joint, const std::vector<T>& range, ToStringFunc func) {
    return join(joint, range.begin(), range.end(), func);
}

template <typename T>
void dumpTestBufferToSpecFileHelper(const TestBuffer& buffer, bool useHexFloat, std::ostream& os) {
    const T* data = buffer.get<T>();
    const uint32_t length = buffer.size() / sizeof(T);
    if constexpr (nnIsFloat<T>) {
        if (useHexFloat) {
            os << "from_hex([" << join(", ", data, data + length, toHexFloatString<T>) << "])";
            return;
        }
    }
    os << "[" << join(", ", data, data + length, defaultToStringFunc<T>) << "]";
}

}  // namespace

const char* toString(TestOperandType type) {
    return kOperandTypeNames[static_cast<int>(type)];
}

const char* toString(TestOperationType type) {
    return kOperationTypeNames[static_cast<int>(type)];
}

// Dump a test buffer.
void SpecDumper::dumpTestBuffer(TestOperandType type, const TestBuffer& buffer, bool useHexFloat) {
    switch (type) {
        case TestOperandType::FLOAT32:
        case TestOperandType::TENSOR_FLOAT32:
            dumpTestBufferToSpecFileHelper<float>(buffer, useHexFloat, mOs);
            break;
        case TestOperandType::INT32:
        case TestOperandType::TENSOR_INT32:
            dumpTestBufferToSpecFileHelper<int32_t>(buffer, useHexFloat, mOs);
            break;
        case TestOperandType::TENSOR_QUANT8_ASYMM:
            dumpTestBufferToSpecFileHelper<uint8_t>(buffer, useHexFloat, mOs);
            break;
        case TestOperandType::TENSOR_QUANT8_SYMM:
        case TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            dumpTestBufferToSpecFileHelper<int8_t>(buffer, useHexFloat, mOs);
            break;
        case TestOperandType::TENSOR_QUANT16_ASYMM:
            dumpTestBufferToSpecFileHelper<uint16_t>(buffer, useHexFloat, mOs);
            break;
        case TestOperandType::TENSOR_QUANT16_SYMM:
            dumpTestBufferToSpecFileHelper<int16_t>(buffer, useHexFloat, mOs);
            break;
        case TestOperandType::BOOL:
        case TestOperandType::TENSOR_BOOL8:
            dumpTestBufferToSpecFileHelper<bool8>(buffer, useHexFloat, mOs);
            break;
        case TestOperandType::FLOAT16:
        case TestOperandType::TENSOR_FLOAT16:
            dumpTestBufferToSpecFileHelper<_Float16>(buffer, useHexFloat, mOs);
            break;
        default:
            CHECK(false) << "Unknown type when dumping the buffer";
    }
}

void SpecDumper::dumpTestOperand(const TestOperand& operand, uint32_t index) {
    mOs << "op" << index << " = " << getOperandClassInSpecFile(operand.lifetime) << "(\"op" << index
        << "\", [\"" << toString(operand.type) << "\", ["
        << join(", ", operand.dimensions, defaultToStringFunc<uint32_t>) << "]";
    if (operand.scale != 0.0f || operand.zeroPoint != 0) {
        mOs << ", float.fromhex(" << toHexFloatString(operand.scale) << "), " << operand.zeroPoint;
    }
    mOs << "]";
    if (operand.lifetime == TestOperandLifeTime::CONSTANT_COPY ||
        operand.lifetime == TestOperandLifeTime::CONSTANT_REFERENCE) {
        mOs << ", ";
        dumpTestBuffer(operand.type, operand.data, /*useHexFloat=*/true);
    } else if (operand.lifetime == TestOperandLifeTime::NO_VALUE) {
        mOs << ", value=None";
    }
    mOs << ")";
    // For quantized data types, append a human-readable scale at the end.
    if (operand.scale != 0.0f) {
        mOs << "  # scale = " << operand.scale;
    }
    // For float buffers, append human-readable values at the end.
    if (isFloatType(operand.type) &&
        (operand.lifetime == TestOperandLifeTime::CONSTANT_COPY ||
         operand.lifetime == TestOperandLifeTime::CONSTANT_REFERENCE)) {
        mOs << "  # ";
        dumpTestBuffer(operand.type, operand.data, /*useHexFloat=*/false);
    }
    mOs << "\n";
}

void SpecDumper::dumpTestOperation(const TestOperation& operation) {
    auto toOperandName = [](uint32_t index) { return "op" + std::to_string(index); };
    mOs << "model = model.Operation(\"" << toString(operation.type) << "\", "
        << join(", ", operation.inputs, toOperandName) << ").To("
        << join(", ", operation.outputs, toOperandName) << ")\n";
}

void SpecDumper::dumpTestModel() {
    CHECK_EQ(kTestModel.referenced.size(), 0u) << "Subgraphs not supported";
    mOs << "from_hex = lambda l: [float.fromhex(i) for i in l]\n\n";

    // Dump model operands.
    mOs << "# Model operands\n";
    for (uint32_t i = 0; i < kTestModel.main.operands.size(); i++) {
        dumpTestOperand(kTestModel.main.operands[i], i);
    }

    // Dump model operations.
    mOs << "\n# Model operations\nmodel = Model()\n";
    for (const auto& operation : kTestModel.main.operations) {
        dumpTestOperation(operation);
    }

    // Dump input/output buffers.
    mOs << "\n# Example\nExample({\n";
    for (uint32_t i = 0; i < kTestModel.main.operands.size(); i++) {
        const auto& operand = kTestModel.main.operands[i];
        if (operand.lifetime != TestOperandLifeTime::SUBGRAPH_INPUT &&
            operand.lifetime != TestOperandLifeTime::SUBGRAPH_OUTPUT) {
            continue;
        }
        // For float buffers, dump human-readable values as a comment.
        if (isFloatType(operand.type)) {
            mOs << "    # op" << i << ": ";
            dumpTestBuffer(operand.type, operand.data, /*useHexFloat=*/false);
            mOs << "\n";
        }
        mOs << "    op" << i << ": ";
        dumpTestBuffer(operand.type, operand.data, /*useHexFloat=*/true);
        mOs << ",\n";
    }
    mOs << "}).DisableLifeTimeVariation()\n";
}

void SpecDumper::dumpResults(const std::string& name, const std::vector<TestBuffer>& results) {
    CHECK_EQ(results.size(), kTestModel.main.outputIndexes.size());
    mOs << "\n# Results from " << name << "\n{\n";
    for (uint32_t i = 0; i < results.size(); i++) {
        const uint32_t outputIndex = kTestModel.main.outputIndexes[i];
        const auto& operand = kTestModel.main.operands[outputIndex];
        // For float buffers, dump human-readable values as a comment.
        if (isFloatType(operand.type)) {
            mOs << "    # op" << outputIndex << ": ";
            dumpTestBuffer(operand.type, results[i], /*useHexFloat=*/false);
            mOs << "\n";
        }
        mOs << "    op" << outputIndex << ": ";
        dumpTestBuffer(operand.type, results[i], /*useHexFloat=*/true);
        mOs << ",\n";
    }
    mOs << "}\n";
}

template <typename T>
static TestOperand convertOperandToFloat32(const TestOperand& op) {
    TestOperand converted = op;
    converted.type =
            isScalarType(op.type) ? TestOperandType::FLOAT32 : TestOperandType::TENSOR_FLOAT32;
    converted.scale = 0.0f;
    converted.zeroPoint = 0;

    const uint32_t numberOfElements = getNumberOfElements(converted);
    converted.data = TestBuffer(numberOfElements * sizeof(float));
    const T* data = op.data.get<T>();
    float* floatData = converted.data.getMutable<float>();

    if (op.scale != 0.0f) {
        std::transform(data, data + numberOfElements, floatData, [&op](T val) {
            return (static_cast<float>(val) - op.zeroPoint) * op.scale;
        });
    } else {
        std::transform(data, data + numberOfElements, floatData,
                       [](T val) { return static_cast<float>(val); });
    }
    return converted;
}

std::optional<TestModel> convertToFloat32Model(const TestModel& testModel) {
    // Only single-operation graphs are supported.
    if (testModel.referenced.size() > 0 || testModel.main.operations.size() > 1) {
        return std::nullopt;
    }

    // Check for unsupported operations.
    CHECK(!testModel.main.operations.empty());
    const auto& operation = testModel.main.operations[0];
    // Do not convert type-casting operations.
    if (operation.type == TestOperationType::DEQUANTIZE ||
        operation.type == TestOperationType::QUANTIZE ||
        operation.type == TestOperationType::CAST) {
        return std::nullopt;
    }
    // HASHTABLE_LOOKUP has different behavior in float and quant data types: float
    // HASHTABLE_LOOKUP will output logical zero when there is a key miss, while quant
    // HASHTABLE_LOOKUP will output byte zero.
    if (operation.type == TestOperationType::HASHTABLE_LOOKUP) {
        return std::nullopt;
    }

    auto convert = [&testModel, &operation](const TestOperand& op, uint32_t index) {
        switch (op.type) {
            case TestOperandType::TENSOR_FLOAT32:
            case TestOperandType::FLOAT32:
            case TestOperandType::TENSOR_BOOL8:
            case TestOperandType::BOOL:
            case TestOperandType::UINT32:
                return op;
            case TestOperandType::INT32:
                // The third input of PAD_V2 uses INT32 to specify the padded value.
                if (operation.type == TestOperationType::PAD_V2 && index == operation.inputs[2]) {
                    // The scale and zero point is inherited from the first input.
                    const uint32_t input0Index = operation.inputs[0];
                    const auto& input0 = testModel.main.operands[input0Index];
                    TestOperand scalarWithScaleAndZeroPoint = op;
                    scalarWithScaleAndZeroPoint.scale = input0.scale;
                    scalarWithScaleAndZeroPoint.zeroPoint = input0.zeroPoint;
                    return convertOperandToFloat32<int32_t>(scalarWithScaleAndZeroPoint);
                }
                return op;
            case TestOperandType::TENSOR_INT32:
                if (op.scale != 0.0f || op.zeroPoint != 0) {
                    return convertOperandToFloat32<int32_t>(op);
                }
                return op;
            case TestOperandType::TENSOR_FLOAT16:
            case TestOperandType::FLOAT16:
                return convertOperandToFloat32<_Float16>(op);
            case TestOperandType::TENSOR_QUANT8_ASYMM:
                return convertOperandToFloat32<uint8_t>(op);
            case TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED:
                return convertOperandToFloat32<int8_t>(op);
            case TestOperandType::TENSOR_QUANT16_ASYMM:
                return convertOperandToFloat32<uint16_t>(op);
            case TestOperandType::TENSOR_QUANT16_SYMM:
                return convertOperandToFloat32<int16_t>(op);
            default:
                CHECK(false) << "OperandType not supported";
                return TestOperand{};
        }
    };

    TestModel converted = testModel;
    for (uint32_t i = 0; i < testModel.main.operands.size(); i++) {
        converted.main.operands[i] = convert(testModel.main.operands[i], i);
    }
    return converted;
}

template <typename T>
static void setDataFromFloat32Buffer(const TestBuffer& fpBuffer, TestOperand* op) {
    const uint32_t numberOfElements = getNumberOfElements(*op);
    const float* floatData = fpBuffer.get<float>();
    T* data = op->data.getMutable<T>();

    if (op->scale != 0.0f) {
        std::transform(floatData, floatData + numberOfElements, data, [op](float val) {
            int32_t unclamped = std::round(val / op->scale) + op->zeroPoint;
            int32_t clamped = std::clamp<int32_t>(unclamped, std::numeric_limits<T>::min(),
                                                  std::numeric_limits<T>::max());
            return static_cast<T>(clamped);
        });
    } else {
        std::transform(floatData, floatData + numberOfElements, data,
                       [](float val) { return static_cast<T>(val); });
    }
}

void setExpectedOutputsFromFloat32Results(const std::vector<TestBuffer>& results,
                                          TestModel* model) {
    CHECK_EQ(model->referenced.size(), 0u) << "Subgraphs not supported";
    CHECK_EQ(model->main.operations.size(), 1u) << "Only single-operation graph is supported";

    for (uint32_t i = 0; i < results.size(); i++) {
        uint32_t outputIndex = model->main.outputIndexes[i];
        auto& op = model->main.operands[outputIndex];
        switch (op.type) {
            case TestOperandType::TENSOR_FLOAT32:
            case TestOperandType::FLOAT32:
            case TestOperandType::TENSOR_BOOL8:
            case TestOperandType::BOOL:
            case TestOperandType::INT32:
            case TestOperandType::UINT32:
                op.data = results[i];
                break;
            case TestOperandType::TENSOR_INT32:
                if (op.scale != 0.0f) {
                    setDataFromFloat32Buffer<int32_t>(results[i], &op);
                } else {
                    op.data = results[i];
                }
                break;
            case TestOperandType::TENSOR_FLOAT16:
            case TestOperandType::FLOAT16:
                setDataFromFloat32Buffer<_Float16>(results[i], &op);
                break;
            case TestOperandType::TENSOR_QUANT8_ASYMM:
                setDataFromFloat32Buffer<uint8_t>(results[i], &op);
                break;
            case TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED:
                setDataFromFloat32Buffer<int8_t>(results[i], &op);
                break;
            case TestOperandType::TENSOR_QUANT16_ASYMM:
                setDataFromFloat32Buffer<uint16_t>(results[i], &op);
                break;
            case TestOperandType::TENSOR_QUANT16_SYMM:
                setDataFromFloat32Buffer<int16_t>(results[i], &op);
                break;
            default:
                CHECK(false) << "OperandType not supported";
        }
    }
}

}  // namespace test_helper
