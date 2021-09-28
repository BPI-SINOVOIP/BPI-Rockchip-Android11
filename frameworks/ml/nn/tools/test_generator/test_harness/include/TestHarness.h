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

// This header file defines an unified structure for a model under test, and provides helper
// functions checking test results. Multiple instances of the test model structure will be
// generated from the model specification files under nn/runtime/test/specs directory.
// Both CTS and VTS will consume this test structure and convert into their own model and
// request format.

#ifndef ANDROID_FRAMEWORKS_ML_NN_TOOLS_TEST_GENERATOR_TEST_HARNESS_TEST_HARNESS_H
#define ANDROID_FRAMEWORKS_ML_NN_TOOLS_TEST_GENERATOR_TEST_HARNESS_TEST_HARNESS_H

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace test_helper {

// This class is a workaround for two issues our code relies on:
// 1. sizeof(bool) is implementation defined.
// 2. vector<bool> does not allow direct pointer access via the data() method.
class bool8 {
   public:
    bool8() : mValue() {}
    /* implicit */ bool8(bool value) : mValue(value) {}
    inline operator bool() const { return mValue != 0; }

   private:
    uint8_t mValue;
};

static_assert(sizeof(bool8) == 1, "size of bool8 must be 8 bits");

// We need the following enum classes since the test harness can neither depend on NDK nor HIDL
// definitions.

enum class TestOperandType {
    FLOAT32 = 0,
    INT32 = 1,
    UINT32 = 2,
    TENSOR_FLOAT32 = 3,
    TENSOR_INT32 = 4,
    TENSOR_QUANT8_ASYMM = 5,
    BOOL = 6,
    TENSOR_QUANT16_SYMM = 7,
    TENSOR_FLOAT16 = 8,
    TENSOR_BOOL8 = 9,
    FLOAT16 = 10,
    TENSOR_QUANT8_SYMM_PER_CHANNEL = 11,
    TENSOR_QUANT16_ASYMM = 12,
    TENSOR_QUANT8_SYMM = 13,
    TENSOR_QUANT8_ASYMM_SIGNED = 14,
    SUBGRAPH = 15,
};

enum class TestOperandLifeTime {
    TEMPORARY_VARIABLE = 0,
    SUBGRAPH_INPUT = 1,
    SUBGRAPH_OUTPUT = 2,
    CONSTANT_COPY = 3,
    CONSTANT_REFERENCE = 4,
    NO_VALUE = 5,
    SUBGRAPH = 6,
    // DEPRECATED. Use SUBGRAPH_INPUT.
    // This value is used in pre-1.3 VTS tests.
    MODEL_INPUT = SUBGRAPH_INPUT,
    // DEPRECATED. Use SUBGRAPH_OUTPUT.
    // This value is used in pre-1.3 VTS tests.
    MODEL_OUTPUT = SUBGRAPH_OUTPUT,
};

enum class TestOperationType {
    ADD = 0,
    AVERAGE_POOL_2D = 1,
    CONCATENATION = 2,
    CONV_2D = 3,
    DEPTHWISE_CONV_2D = 4,
    DEPTH_TO_SPACE = 5,
    DEQUANTIZE = 6,
    EMBEDDING_LOOKUP = 7,
    FLOOR = 8,
    FULLY_CONNECTED = 9,
    HASHTABLE_LOOKUP = 10,
    L2_NORMALIZATION = 11,
    L2_POOL_2D = 12,
    LOCAL_RESPONSE_NORMALIZATION = 13,
    LOGISTIC = 14,
    LSH_PROJECTION = 15,
    LSTM = 16,
    MAX_POOL_2D = 17,
    MUL = 18,
    RELU = 19,
    RELU1 = 20,
    RELU6 = 21,
    RESHAPE = 22,
    RESIZE_BILINEAR = 23,
    RNN = 24,
    SOFTMAX = 25,
    SPACE_TO_DEPTH = 26,
    SVDF = 27,
    TANH = 28,
    BATCH_TO_SPACE_ND = 29,
    DIV = 30,
    MEAN = 31,
    PAD = 32,
    SPACE_TO_BATCH_ND = 33,
    SQUEEZE = 34,
    STRIDED_SLICE = 35,
    SUB = 36,
    TRANSPOSE = 37,
    ABS = 38,
    ARGMAX = 39,
    ARGMIN = 40,
    AXIS_ALIGNED_BBOX_TRANSFORM = 41,
    BIDIRECTIONAL_SEQUENCE_LSTM = 42,
    BIDIRECTIONAL_SEQUENCE_RNN = 43,
    BOX_WITH_NMS_LIMIT = 44,
    CAST = 45,
    CHANNEL_SHUFFLE = 46,
    DETECTION_POSTPROCESSING = 47,
    EQUAL = 48,
    EXP = 49,
    EXPAND_DIMS = 50,
    GATHER = 51,
    GENERATE_PROPOSALS = 52,
    GREATER = 53,
    GREATER_EQUAL = 54,
    GROUPED_CONV_2D = 55,
    HEATMAP_MAX_KEYPOINT = 56,
    INSTANCE_NORMALIZATION = 57,
    LESS = 58,
    LESS_EQUAL = 59,
    LOG = 60,
    LOGICAL_AND = 61,
    LOGICAL_NOT = 62,
    LOGICAL_OR = 63,
    LOG_SOFTMAX = 64,
    MAXIMUM = 65,
    MINIMUM = 66,
    NEG = 67,
    NOT_EQUAL = 68,
    PAD_V2 = 69,
    POW = 70,
    PRELU = 71,
    QUANTIZE = 72,
    QUANTIZED_16BIT_LSTM = 73,
    RANDOM_MULTINOMIAL = 74,
    REDUCE_ALL = 75,
    REDUCE_ANY = 76,
    REDUCE_MAX = 77,
    REDUCE_MIN = 78,
    REDUCE_PROD = 79,
    REDUCE_SUM = 80,
    ROI_ALIGN = 81,
    ROI_POOLING = 82,
    RSQRT = 83,
    SELECT = 84,
    SIN = 85,
    SLICE = 86,
    SPLIT = 87,
    SQRT = 88,
    TILE = 89,
    TOPK_V2 = 90,
    TRANSPOSE_CONV_2D = 91,
    UNIDIRECTIONAL_SEQUENCE_LSTM = 92,
    UNIDIRECTIONAL_SEQUENCE_RNN = 93,
    RESIZE_NEAREST_NEIGHBOR = 94,
    QUANTIZED_LSTM = 95,
    IF = 96,
    WHILE = 97,
    ELU = 98,
    HARD_SWISH = 99,
    FILL = 100,
    RANK = 101,
};

enum class TestHalVersion { UNKNOWN, V1_0, V1_1, V1_2, V1_3 };

// Manages the data buffer for a test operand.
class TestBuffer {
   public:
    // The buffer must be aligned on a boundary of a byte size that is a multiple of the element
    // type byte size. In NNAPI, 4-byte boundary should be sufficient for all current data types.
    static constexpr size_t kAlignment = 4;

    // Create the buffer of a given size and initialize from data.
    // If data is nullptr, the allocated memory stays uninitialized.
    TestBuffer(size_t size = 0, const void* data = nullptr) : mSize(size) {
        if (size > 0) {
            // The size for aligned_alloc must be an integral multiple of alignment.
            mBuffer.reset(aligned_alloc(kAlignment, alignedSize()), free);
            if (data) memcpy(mBuffer.get(), data, size);
        }
    }

    // Explicitly create a deep copy.
    TestBuffer copy() const { return TestBuffer(mSize, mBuffer.get()); }

    // Factory method creating the buffer from a typed vector.
    template <typename T>
    static TestBuffer createFromVector(const std::vector<T>& vec) {
        return TestBuffer(vec.size() * sizeof(T), vec.data());
    }

    // Factory method for creating a randomized buffer with "size" number of
    // bytes.
    template <typename T>
    static TestBuffer createFromRng(size_t size, std::default_random_engine* gen) {
        static_assert(kAlignment % sizeof(T) == 0);
        TestBuffer testBuffer(size);
        std::uniform_int_distribution<T> dist{};
        const size_t adjustedSize = testBuffer.alignedSize() / sizeof(T);
        std::generate_n(testBuffer.getMutable<T>(), adjustedSize, [&] { return dist(*gen); });
        return testBuffer;
    }

    template <typename T>
    const T* get() const {
        return reinterpret_cast<const T*>(mBuffer.get());
    }

    template <typename T>
    T* getMutable() {
        return reinterpret_cast<T*>(mBuffer.get());
    }

    // Returns the byte size of the buffer.
    size_t size() const { return mSize; }

    // Returns the byte size that is aligned to kAlignment.
    size_t alignedSize() const { return ((mSize + kAlignment - 1) / kAlignment) * kAlignment; }

    bool operator==(std::nullptr_t) const { return mBuffer == nullptr; }
    bool operator!=(std::nullptr_t) const { return mBuffer != nullptr; }

   private:
    std::shared_ptr<void> mBuffer;
    size_t mSize = 0;
};

struct TestSymmPerChannelQuantParams {
    std::vector<float> scales;
    uint32_t channelDim = 0;
};

struct TestOperand {
    TestOperandType type;
    std::vector<uint32_t> dimensions;
    uint32_t numberOfConsumers;
    float scale = 0.0f;
    int32_t zeroPoint = 0;
    TestOperandLifeTime lifetime;
    TestSymmPerChannelQuantParams channelQuant;

    // For SUBGRAPH_OUTPUT only. Set to true to skip the accuracy check on this operand.
    bool isIgnored = false;

    // For CONSTANT_COPY/REFERENCE and SUBGRAPH_INPUT, this is the data set in model and request.
    // For SUBGRAPH_OUTPUT, this is the expected results.
    // For TEMPORARY_VARIABLE and NO_VALUE, this is nullptr.
    TestBuffer data;
};

struct TestOperation {
    TestOperationType type;
    std::vector<uint32_t> inputs;
    std::vector<uint32_t> outputs;
};

struct TestSubgraph {
    std::vector<TestOperand> operands;
    std::vector<TestOperation> operations;
    std::vector<uint32_t> inputIndexes;
    std::vector<uint32_t> outputIndexes;
};

struct TestModel {
    TestSubgraph main;
    std::vector<TestSubgraph> referenced;
    bool isRelaxed = false;

    // Additional testing information and flags associated with the TestModel.

    // Specifies the RANDOM_MULTINOMIAL distribution tolerance.
    // If set to greater than zero, the input is compared as log-probabilities
    // to the output and must be within this tolerance to pass.
    float expectedMultinomialDistributionTolerance = 0.0f;

    // If set to true, the TestModel specifies a validation test that is expected to fail during
    // compilation or execution.
    bool expectFailure = false;

    // The minimum supported HAL version.
    TestHalVersion minSupportedVersion = TestHalVersion::UNKNOWN;

    void forEachSubgraph(std::function<void(const TestSubgraph&)> handler) const {
        handler(main);
        for (const TestSubgraph& subgraph : referenced) {
            handler(subgraph);
        }
    }

    void forEachSubgraph(std::function<void(TestSubgraph&)> handler) {
        handler(main);
        for (TestSubgraph& subgraph : referenced) {
            handler(subgraph);
        }
    }

    // Explicitly create a deep copy.
    TestModel copy() const {
        TestModel newTestModel(*this);
        newTestModel.forEachSubgraph([](TestSubgraph& subgraph) {
            for (TestOperand& operand : subgraph.operands) {
                operand.data = operand.data.copy();
            }
        });
        return newTestModel;
    }

    bool hasQuant8CoupledOperands() const {
        bool result = false;
        forEachSubgraph([&result](const TestSubgraph& subgraph) {
            if (result) {
                return;
            }
            for (const TestOperation& operation : subgraph.operations) {
                /*
                 *  There are several ops that are exceptions to the general quant8
                 *  types coupling:
                 *  HASHTABLE_LOOKUP -- due to legacy reasons uses
                 *    TENSOR_QUANT8_ASYMM tensor as if it was TENSOR_BOOL. It
                 *    doesn't make sense to have coupling in this case.
                 *  LSH_PROJECTION -- hashes an input tensor treating it as raw
                 *    bytes. We can't expect same results for coupled inputs.
                 *  PAD_V2 -- pad_value is set using int32 scalar, so coupling
                 *    produces a wrong result.
                 *  CAST -- converts tensors without taking into account input's
                 *    scale and zero point. Coupled models shouldn't produce same
                 *    results.
                 *  QUANTIZED_16BIT_LSTM -- the op is made for a specific use case,
                 *    supporting signed quantization is not worth the compications.
                 */
                if (operation.type == TestOperationType::HASHTABLE_LOOKUP ||
                    operation.type == TestOperationType::LSH_PROJECTION ||
                    operation.type == TestOperationType::PAD_V2 ||
                    operation.type == TestOperationType::CAST ||
                    operation.type == TestOperationType::QUANTIZED_16BIT_LSTM) {
                    continue;
                }
                for (const auto operandIndex : operation.inputs) {
                    if (subgraph.operands[operandIndex].type ==
                        TestOperandType::TENSOR_QUANT8_ASYMM) {
                        result = true;
                        return;
                    }
                }
                for (const auto operandIndex : operation.outputs) {
                    if (subgraph.operands[operandIndex].type ==
                        TestOperandType::TENSOR_QUANT8_ASYMM) {
                        result = true;
                        return;
                    }
                }
            }
        });
        return result;
    }

    bool hasScalarOutputs() const {
        bool result = false;
        forEachSubgraph([&result](const TestSubgraph& subgraph) {
            if (result) {
                return;
            }
            for (const TestOperation& operation : subgraph.operations) {
                // RANK op returns a scalar and therefore shouldn't be tested
                // for dynamic output shape support.
                if (operation.type == TestOperationType::RANK) {
                    result = true;
                    return;
                }
                // Control flow operations do not support referenced model
                // outputs with dynamic shapes.
                if (operation.type == TestOperationType::IF ||
                    operation.type == TestOperationType::WHILE) {
                    result = true;
                    return;
                }
            }
        });
        return result;
    }

    bool isInfiniteLoopTimeoutTest() const {
        // This should only match the TestModel generated from while_infinite_loop.mod.py.
        return expectFailure && main.operations[0].type == TestOperationType::WHILE;
    }
};

// Manages all generated test models.
class TestModelManager {
   public:
    // Returns the singleton manager.
    static TestModelManager& get() {
        static TestModelManager instance;
        return instance;
    }

    // Registers a TestModel to the manager. Returns a dummy integer for global variable
    // initialization.
    int add(std::string name, const TestModel& testModel) {
        mTestModels.emplace(std::move(name), &testModel);
        return 0;
    }

    // Returns a vector of selected TestModels for which the given "filter" returns true.
    using TestParam = std::pair<std::string, const TestModel*>;
    std::vector<TestParam> getTestModels(std::function<bool(const TestModel&)> filter) {
        std::vector<TestParam> testModels;
        testModels.reserve(mTestModels.size());
        std::copy_if(mTestModels.begin(), mTestModels.end(), std::back_inserter(testModels),
                     [filter](const auto& nameTestPair) { return filter(*nameTestPair.second); });
        return testModels;
    }

    // Returns a vector of selected TestModels for which the given "filter" returns true.
    std::vector<TestParam> getTestModels(std::function<bool(const std::string&)> filter) {
        std::vector<TestParam> testModels;
        testModels.reserve(mTestModels.size());
        std::copy_if(mTestModels.begin(), mTestModels.end(), std::back_inserter(testModels),
                     [filter](const auto& nameTestPair) { return filter(nameTestPair.first); });
        return testModels;
    }

   private:
    TestModelManager() = default;
    TestModelManager(const TestModelManager&) = delete;
    TestModelManager& operator=(const TestModelManager&) = delete;

    // Contains all TestModels generated from nn/runtime/test/specs directory.
    // The TestModels are sorted by name to ensure a predictable order.
    std::map<std::string, const TestModel*> mTestModels;
};

struct AccuracyCriterion {
    // We expect the driver results to be unbiased.
    // Formula: abs(sum_{i}(diff) / sum(1)) <= bias, where
    // * fixed point: diff = actual - expected
    // * floating point: diff = (actual - expected) / max(1, abs(expected))
    float bias = std::numeric_limits<float>::max();

    // Set the threshold on Mean Square Error (MSE).
    // Formula: sum_{i}(diff ^ 2) / sum(1) <= mse
    float mse = std::numeric_limits<float>::max();

    // We also set accuracy thresholds on each element to detect any particular edge cases that may
    // be shadowed in bias or MSE. We use the similar approach as our CTS unit tests, but with much
    // relaxed criterion.
    // Formula: abs(actual - expected) <= atol + rtol * abs(expected)
    //   where atol stands for Absolute TOLerance and rtol for Relative TOLerance.
    float atol = 0.0f;
    float rtol = 0.0f;
};

struct AccuracyCriteria {
    AccuracyCriterion float32;
    AccuracyCriterion float16;
    AccuracyCriterion int32;
    AccuracyCriterion quant8Asymm;
    AccuracyCriterion quant8AsymmSigned;
    AccuracyCriterion quant8Symm;
    AccuracyCriterion quant16Asymm;
    AccuracyCriterion quant16Symm;
    float bool8AllowedErrorRatio = 0.1f;
    bool allowInvalidFpValues = true;
};

// Check the output results against the expected values in test model by calling
// GTEST_ASSERT/EXPECT. The index of the results corresponds to the index in
// model.main.outputIndexes. E.g., results[i] corresponds to model.main.outputIndexes[i].
void checkResults(const TestModel& model, const std::vector<TestBuffer>& results);
void checkResults(const TestModel& model, const std::vector<TestBuffer>& results,
                  const AccuracyCriteria& criteria);

bool isQuantizedType(TestOperandType type);

TestModel convertQuant8AsymmOperandsToSigned(const TestModel& testModel);

const char* toString(TestOperandType type);
const char* toString(TestOperationType type);

// Dump a test model in the format of a spec file for debugging and visualization purpose.
class SpecDumper {
   public:
    SpecDumper(const TestModel& testModel, std::ostream& os) : kTestModel(testModel), mOs(os) {}
    void dumpTestModel();
    void dumpResults(const std::string& name, const std::vector<TestBuffer>& results);

   private:
    // Dump a test model operand.
    // e.g. op0 = Input("op0", "TENSOR_FLOAT32", "{1, 2, 6, 1}")
    // e.g. op1 = Parameter("op1", "INT32", "{}", [2])
    void dumpTestOperand(const TestOperand& operand, uint32_t index);

    // Dump a test model operation.
    // e.g. model = model.Operation("CONV_2D", op0, op1, op2, op3, op4, op5, op6).To(op7)
    void dumpTestOperation(const TestOperation& operation);

    // Dump a test buffer as a python 1D list.
    // e.g. [1, 2, 3, 4, 5]
    //
    // If useHexFloat is set to true and the operand type is float, the buffer values will be
    // dumped in hex representation.
    void dumpTestBuffer(TestOperandType type, const TestBuffer& buffer, bool useHexFloat);

    const TestModel& kTestModel;
    std::ostream& mOs;
};

// Convert the test model to an equivalent float32 model. It will return std::nullopt if the
// conversion is not supported, or if there is no equivalent float32 model.
std::optional<TestModel> convertToFloat32Model(const TestModel& testModel);

// Used together with convertToFloat32Model. Convert the results computed from the float model to
// the actual data type in the original model.
void setExpectedOutputsFromFloat32Results(const std::vector<TestBuffer>& results, TestModel* model);

}  // namespace test_helper

#endif  // ANDROID_FRAMEWORKS_ML_NN_TOOLS_TEST_GENERATOR_TEST_HARNESS_TEST_HARNESS_H
