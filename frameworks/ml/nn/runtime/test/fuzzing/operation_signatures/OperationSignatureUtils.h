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

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_FUZZING_OPERATION_SIGNATURES_OPERATION_SIGNATURE_UTILS_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_FUZZING_OPERATION_SIGNATURES_OPERATION_SIGNATURE_UTILS_H

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "TestHarness.h"
#include "TestNeuralNetworksWrapper.h"
#include "fuzzing/OperationManager.h"
#include "fuzzing/RandomGraphGenerator.h"
#include "fuzzing/RandomGraphGeneratorUtils.h"

namespace android {
namespace nn {
namespace fuzzing_test {

namespace {

using namespace test_helper;

// From TestOperandType to cpp type.
template <TestOperandType type>
struct CppType;
template <>
struct CppType<TestOperandType::TENSOR_FLOAT32> {
    using type = float;
};
template <>
struct CppType<TestOperandType::FLOAT32> {
    using type = float;
};
template <>
struct CppType<TestOperandType::TENSOR_INT32> {
    using type = int32_t;
};
template <>
struct CppType<TestOperandType::INT32> {
    using type = int32_t;
};
template <>
struct CppType<TestOperandType::TENSOR_QUANT8_ASYMM> {
    using type = uint8_t;
};
template <>
struct CppType<TestOperandType::TENSOR_QUANT8_SYMM> {
    using type = int8_t;
};
template <>
struct CppType<TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED> {
    using type = int8_t;
};
template <>
struct CppType<TestOperandType::TENSOR_QUANT16_ASYMM> {
    using type = uint16_t;
};
template <>
struct CppType<TestOperandType::TENSOR_QUANT16_SYMM> {
    using type = int16_t;
};
template <>
struct CppType<TestOperandType::TENSOR_BOOL8> {
    using type = bool8;
};
template <>
struct CppType<TestOperandType::BOOL> {
    using type = bool8;
};
template <>
struct CppType<TestOperandType::TENSOR_FLOAT16> {
    using type = _Float16;
};
template <>
struct CppType<TestOperandType::FLOAT16> {
    using type = _Float16;
};

// The buffer value X is chosen uniformly in the range [kMinFloat32, kMaxFloat32]. kMinFloat32 and
// kMaxFloat32 are selected by setting:
// * E[X] = 0, so that the sum will less likely to overflow or underflow;
// * E[abs(X)] = 1, so that the production will less likely to overflow or underflow.
constexpr float kMaxFloat32 = 2.0f;
constexpr float kMinFloat32 = -kMaxFloat32;

template <typename T>
inline T getUniformValue(int valueProperties, T low, T up, T zeroPoint) {
    if (valueProperties & RandomOperand::NON_NEGATIVE) {
        NN_FUZZER_CHECK(up >= zeroPoint);
        low = std::max(low, zeroPoint);
    }
    if (valueProperties & RandomOperand::NON_ZERO) {
        return getUniformNonZero(low, up, zeroPoint);
    } else {
        return getUniform(low, up);
    }
}
template <>
inline bool8 getUniformValue(int, bool8, bool8, bool8) {
    return getBernoulli(0.5f);
}

template <typename T>
inline void uniform(T low, T up, T zeroPoint, RandomOperand* op) {
    T* data = reinterpret_cast<T*>(op->buffer.data());
    uint32_t len = op->getNumberOfElements();
    for (uint32_t i = 0; i < len; i++) {
        data[i] = getUniformValue<T>(op->valueProperties, low, up, zeroPoint);
    }
}

// Generate random buffer values with uniform distribution.
// Dispatch to different generators by operand dataType.
inline void uniformFinalizer(RandomOperand* op) {
    switch (op->dataType) {
        case TestOperandType::TENSOR_FLOAT32:
        case TestOperandType::FLOAT32:
            uniform<float>(kMinFloat32, kMaxFloat32, 0.0f, op);
            break;
        case TestOperandType::TENSOR_INT32:
        case TestOperandType::INT32:
            uniform<int32_t>(0, 255, op->zeroPoint, op);
            break;
        case TestOperandType::TENSOR_QUANT8_ASYMM:
            uniform<uint8_t>(0, 255, op->zeroPoint, op);
            break;
        case TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED:
            uniform<int8_t>(-128, 127, op->zeroPoint, op);
            break;
        case TestOperandType::TENSOR_QUANT8_SYMM:
            uniform<int8_t>(-128, 127, op->zeroPoint, op);
            break;
        case TestOperandType::TENSOR_QUANT16_ASYMM:
            uniform<uint16_t>(0, 65535, op->zeroPoint, op);
            break;
        case TestOperandType::TENSOR_QUANT16_SYMM:
            uniform<int16_t>(-32768, 32767, op->zeroPoint, op);
            break;
        case TestOperandType::TENSOR_BOOL8:
            uniform<bool8>(true, false, false, op);
            break;
        case TestOperandType::TENSOR_FLOAT16:
        case TestOperandType::FLOAT16:
            uniform<_Float16>(kMinFloat32, kMaxFloat32, 0.0f, op);
            break;
        default:
            NN_FUZZER_CHECK(false) << "Unsupported data type.";
    }
}

// A helper struct for DEFINE_OPERATION_SIGNATURE macro.
struct OperationSignatureHelper {
    std::string name;
    OperationSignatureHelper(const std::string& name) : name(name) {}
    int operator+(const OperationSignature& op) {
        OperationManager::get()->addSignature(name, op);
        return 0;
    }
};

}  // namespace

inline void implicitPadding(const RandomVariable& input, const RandomVariable& filter,
                            const RandomVariable& stride, const RandomVariable& dilation,
                            int32_t paddingScheme, RandomVariable* output) {
    switch (paddingScheme) {
        case ANEURALNETWORKS_PADDING_SAME:
            *output = (input + (stride - 1)) / stride;
            break;
        case ANEURALNETWORKS_PADDING_VALID:
            *output = (input - filter * dilation + (dilation + stride - 1)) / stride;
            break;
        default:
            NN_FUZZER_CHECK(false) << "Unknown padding scheme";
    }
}

inline void explicitPadding(const RandomVariable& input, const RandomVariable& filter,
                            const RandomVariable& stride, const RandomVariable& dilation,
                            const RandomVariable& paddingHead, const RandomVariable& paddingTail,
                            RandomVariable* output) {
    auto effectiveFilter = (filter - 1) * dilation + 1;
    *output = (input - effectiveFilter + (stride + paddingHead + paddingTail)) / stride;
    // TFLite will crash if the filter size is less than or equal to the paddings.
    effectiveFilter.setGreaterThan(paddingHead);
    effectiveFilter.setGreaterThan(paddingTail);
}

inline void implicitPaddingTranspose(const RandomVariable& input, const RandomVariable& filter,
                                     const RandomVariable& stride, int32_t paddingScheme,
                                     RandomVariable* output) {
    switch (paddingScheme) {
        case ANEURALNETWORKS_PADDING_SAME:
            *output = input * stride;
            break;
        case ANEURALNETWORKS_PADDING_VALID:
            *output = (input - 1) * stride + filter;
            break;
        default:
            NN_FUZZER_CHECK(false) << "Unknown padding scheme";
    }
}

inline void explicitPaddingTranspose(const RandomVariable& input, const RandomVariable& filter,
                                     const RandomVariable& stride,
                                     const RandomVariable& paddingHead,
                                     const RandomVariable& paddingTail, RandomVariable* output) {
    *output = stride * input + filter - (stride + paddingHead + paddingTail);
}

inline void setSameQuantization(const std::shared_ptr<RandomOperand>& to,
                                const std::shared_ptr<RandomOperand>& from) {
    NN_FUZZER_CHECK(to->dataType == from->dataType);
    to->scale = from->scale;
    to->zeroPoint = from->zeroPoint;
}

inline void setFreeDimensions(const std::shared_ptr<RandomOperand>& op, uint32_t rank) {
    op->dimensions.resize(rank);
    for (uint32_t i = 0; i < rank; i++) op->dimensions[i] = RandomVariableType::FREE;
}

inline void setConvFCScale(bool applyOutputScaleBound, RandomOperation* op) {
    if (isQuantizedType(op->inputs[0]->dataType)) {
        float biasScale = op->inputs[0]->scale * op->inputs[1]->scale;
        op->inputs[2]->scale = biasScale;
        if (applyOutputScaleBound) {
            op->outputs[0]->scale = getUniform(biasScale, biasScale * 5);
        }
    }
}

// For ops with input0 and output0 of the same dimension.
inline void sameDimensionOpConstructor(TestOperandType, uint32_t rank, RandomOperation* op) {
    setFreeDimensions(op->inputs[0], rank);
    op->outputs[0]->dimensions = op->inputs[0]->dimensions;
}

// For ops with input0 and output0 of the same shape including scale and zeroPoint.
inline void sameShapeOpConstructor(TestOperandType dataType, uint32_t rank, RandomOperation* op) {
    sameDimensionOpConstructor(dataType, rank, op);
    setSameQuantization(op->outputs[0], op->inputs[0]);
}

inline void defaultOperandConstructor(TestOperandType dataType, uint32_t, RandomOperand* op) {
    op->dataType = dataType;
    if (dataType == TestOperandType::TENSOR_QUANT8_ASYMM) {
        op->scale = getUniform<float>(0.1, 2.0);
        op->zeroPoint = getUniform<int32_t>(0, 255);
    } else if (dataType == TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED) {
        op->scale = getUniform<float>(0.1, 2.0);
        op->zeroPoint = getUniform<int32_t>(-128, 127);
    } else if (dataType == TestOperandType::TENSOR_QUANT8_SYMM) {
        op->scale = getUniform<float>(0.1, 2.0);
        op->zeroPoint = 0;
    } else if (dataType == TestOperandType::TENSOR_QUANT16_ASYMM) {
        op->scale = getUniform<float>(0.1, 2.0);
        op->zeroPoint = getUniform<int32_t>(0, 65535);
    } else if (dataType == TestOperandType::TENSOR_QUANT16_SYMM) {
        op->scale = getUniform<float>(0.1, 2.0);
        op->zeroPoint = 0;
    } else {
        op->scale = 0.0f;
        op->zeroPoint = 0;
    }
}

inline void defaultScalarOperandConstructor(TestOperandType dataType, uint32_t, RandomOperand* op) {
    switch (dataType) {
        case TestOperandType::TENSOR_FLOAT32:
            op->dataType = TestOperandType::FLOAT32;
            op->scale = 0.0f;
            op->zeroPoint = 0;
            break;
        case TestOperandType::TENSOR_FLOAT16:
            op->dataType = TestOperandType::FLOAT16;
            op->scale = 0.0f;
            op->zeroPoint = 0;
            break;
        case TestOperandType::TENSOR_INT32:
            op->dataType = TestOperandType::INT32;
            op->scale = 0.0f;
            op->zeroPoint = 0;
            break;
        default:
            NN_FUZZER_CHECK(false) << "Data type " << toString(dataType)
                                   << " is not supported in defaultScalarOperandConstructor.";
    }
}

// An INPUT operand with uniformly distributed buffer values. The operand's data type is set the
// same as the operation's primary data type. In the case of quantized data type, the quantization
// parameters are chosen randomly and uniformly.
#define INPUT_DEFAULT                                                               \
    {                                                                               \
        .type = RandomOperandType::INPUT, .constructor = defaultOperandConstructor, \
        .finalizer = uniformFinalizer                                               \
    }

// A scalar operand with an uniformly distributed value. The operand's data type is set to the
// corresponding scalar type of the operation's primary data type (which is always a tensor type).
#define INPUT_SCALAR                                                                      \
    {                                                                                     \
        .type = RandomOperandType::INPUT, .constructor = defaultScalarOperandConstructor, \
        .finalizer = uniformFinalizer                                                     \
    }

// An INPUT operand with a specified data type and uniformly distributed buffer values. In the case
// of quantized data type, the quantization parameters are chosen randomly and uniformly.
#define INPUT_TYPED(opType)                                                                      \
    {                                                                                            \
        .type = RandomOperandType::INPUT,                                                        \
        .constructor = [](TestOperandType, uint32_t rank,                                        \
                          RandomOperand* op) { defaultOperandConstructor((opType), rank, op); }, \
        .finalizer = uniformFinalizer                                                            \
    }

// For the bias tensor in convolutions and fully connected operator.
// An INPUT operand with uniformly distributed buffer values. The operand's data type is set to
// TENSOR_INT32 if the operation's primary data type is TENSOR_QUANT8_ASYMM. Otherwise, it is the
// same as INPUT_DEFAULT.
#define INPUT_BIAS                                                                 \
    {                                                                              \
        .type = RandomOperandType::INPUT,                                          \
        .constructor =                                                             \
                [](TestOperandType dataType, uint32_t rank, RandomOperand* op) {   \
                    if (dataType == TestOperandType::TENSOR_QUANT8_ASYMM ||        \
                        dataType == TestOperandType::TENSOR_QUANT8_ASYMM_SIGNED) { \
                        dataType = TestOperandType::TENSOR_INT32;                  \
                    }                                                              \
                    defaultOperandConstructor(dataType, rank, op);                 \
                },                                                                 \
        .finalizer = uniformFinalizer                                              \
    }

// A helper macro for common code block filling operand buffer with random method.
#define PARAMETER_FILL_BUFFER_HELPER(opType, len, method, ...)               \
    op->dataType = opType;                                                   \
    int length = (len);                                                      \
    if (kScalarDataType[static_cast<int>(opType)]) {                         \
        NN_FUZZER_CHECK(length == 1);                                        \
    } else {                                                                 \
        op->dimensions = {length};                                           \
    }                                                                        \
    op->resizeBuffer<CppType<opType>::type>(length);                         \
    auto data = reinterpret_cast<CppType<opType>::type*>(op->buffer.data()); \
    for (int i = 0; i < length; i++) {                                       \
        data[i] = method<CppType<opType>::type>(__VA_ARGS__);                \
    }

// A 1-D vector of CONST parameters of length len, each uniformly selected within range [low, up].
#define PARAMETER_VEC_RANGE(opType, len, low, up)                           \
    {                                                                       \
        .type = RandomOperandType::CONST,                                   \
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) {   \
            PARAMETER_FILL_BUFFER_HELPER(opType, len, getUniform, low, up); \
        }                                                                   \
    }

// A CONST scalar uniformly selected within range [low, up].
#define PARAMETER_RANGE(opType, low, up) PARAMETER_VEC_RANGE(opType, 1, low, up)

// A CONST floating point scalar uniformly selected within range [low, up]. The operand's data type
// is set to FLOAT16 if the operation's primary data type is TENSOR_FLOAT16. Otherwise, the data
// type is set to FLOAT32.
#define PARAMETER_FLOAT_RANGE(low, up)                                                          \
    {                                                                                           \
        .type = RandomOperandType::CONST,                                                       \
        .constructor = [](TestOperandType dataType, uint32_t, RandomOperand* op) {              \
            if (dataType == TestOperandType::TENSOR_FLOAT16) {                                  \
                PARAMETER_FILL_BUFFER_HELPER(TestOperandType::FLOAT16, 1, getUniform, low, up); \
            } else {                                                                            \
                PARAMETER_FILL_BUFFER_HELPER(TestOperandType::FLOAT32, 1, getUniform, low, up); \
            }                                                                                   \
        }                                                                                       \
    }

// A CONST scalar uniformly selected from the provided choices.
#define PARAMETER_CHOICE(opType, ...)                                          \
    {                                                                          \
        .type = RandomOperandType::CONST,                                      \
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) {      \
            const std::vector<CppType<opType>::type> choices = {__VA_ARGS__};  \
            PARAMETER_FILL_BUFFER_HELPER(opType, 1, getRandomChoice, choices); \
        }                                                                      \
    }

// A CONST scalar with unintialized buffer value. The buffer values are expected to be filled in the
// operation constructor or finalizer.
#define PARAMETER_NONE(opType)                                                                     \
    {                                                                                              \
        .type = RandomOperandType::CONST,                                                          \
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) { op->dataType = opType; } \
    }

// A CONST omitted operand.
#define PARAMETER_NO_VALUE(opType)                                                                 \
    {                                                                                              \
        .type = RandomOperandType::NO_VALUE,                                                       \
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) { op->dataType = opType; } \
    }

// A CONST integer scalar with value set as a FREE RandomVariable within default range.
#define RANDOM_INT_FREE                                                   \
    {                                                                     \
        .type = RandomOperandType::CONST,                                 \
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) { \
            op->dataType = TestOperandType::INT32;                        \
            op->randomBuffer = {RandomVariableType::FREE};                \
        }                                                                 \
    }

// A CONST integer scalar with value set as a FREE RandomVariable within range [low, up].
#define RANDOM_INT_RANGE(low, up)                                         \
    {                                                                     \
        .type = RandomOperandType::CONST,                                 \
        .constructor = [](TestOperandType, uint32_t, RandomOperand* op) { \
            op->dataType = TestOperandType::INT32;                        \
            op->randomBuffer = {RandomVariable((low), (up))};             \
        }                                                                 \
    }

// An OUTPUT operand with data type set the same as the operation primary data type. In the case of
// quantized data type, the quantization parameters are chosen randomly and uniformly.
#define OUTPUT_DEFAULT \
    { .type = RandomOperandType::OUTPUT, .constructor = defaultOperandConstructor }

// An OUTPUT operand with a specified data type. In the case of quantized data type, the
// quantization parameters are chosen randomly and uniformly.
#define OUTPUT_TYPED(opType)                                                   \
    {                                                                          \
        .type = RandomOperandType::OUTPUT,                                     \
        .constructor = [](TestOperandType, uint32_t rank, RandomOperand* op) { \
            defaultOperandConstructor((opType), rank, op);                     \
        }                                                                      \
    }

// An OUTPUT operand with data type set the same as the operation primary data type. In the case of
// quantized data type, the quantization parameters are set to the specified values.
#define OUTPUT_QUANT(fixedScale, fixedZeroPoint)                                        \
    {                                                                                   \
        .type = RandomOperandType::OUTPUT,                                              \
        .constructor = [](TestOperandType dataType, uint32_t rank, RandomOperand* op) { \
            defaultOperandConstructor(dataType, rank, op);                              \
            if (isQuantizedType(op->dataType)) {                                        \
                op->scale = (fixedScale);                                               \
                op->zeroPoint = (fixedZeroPoint);                                       \
            }                                                                           \
        }                                                                               \
    }

// DEFINE_OPERATION_SIGNATURE creates a OperationSignature by aggregate initialization and adds it
// to the global OperationManager singleton.
//
// Usage:
//   DEFINE_OPERATION_SIGNATURE(name) { aggregate_initialization };
//
// Example:
//   DEFINE_OPERATION_SIGNATURE(RELU_V1_0) {
//       .opType = TestOperationType::RELU,
//       .supportedDataTypes = {TestOperandType::TENSOR_FLOAT32,
//       TestOperandType::TENSOR_QUANT8_ASYMM}, .supportedRanks = {1, 2, 3, 4}, .version =
//       TestHalVersion::V1_0, .inputs = {INPUT_DEFAULT}, .outputs = {OUTPUT_DEFAULT}, .constructor
//       = sameShapeOpConstructor};
//
#define DEFINE_OPERATION_SIGNATURE(name) \
    const int dummy_##name = OperationSignatureHelper(#name) + OperationSignature

}  // namespace fuzzing_test
}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_TEST_FUZZING_OPERATION_SIGNATURES_OPERATION_SIGNATURE_UTILS_H
