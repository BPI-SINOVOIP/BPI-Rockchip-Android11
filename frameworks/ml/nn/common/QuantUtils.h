// Quantized calculation utilities.
// TODO(vddang): Replace this with tensorflow/lite/kernels/internal/tensor_utils(common).h
// after TFLite module has been synced.

#ifndef ANDROID_FRAMEWORKS_ML_NN_COMMON_QUANTUTILS_H
#define ANDROID_FRAMEWORKS_ML_NN_COMMON_QUANTUTILS_H

#include <public/gemmlowp.h>

#include <limits>
#include <memory>

#include "OperationsUtils.h"
#include "Utils.h"

namespace android {
namespace nn {

inline int32_t MultiplyByQuantizedMultiplier(int32_t x, int32_t quantized_multiplier, int shift) {
    using gemmlowp::RoundingDivideByPOT;
    using gemmlowp::SaturatingRoundingDoublingHighMul;
    int left_shift = shift > 0 ? shift : 0;
    int right_shift = shift > 0 ? 0 : -shift;
    return RoundingDivideByPOT(
            SaturatingRoundingDoublingHighMul(x * (1 << left_shift), quantized_multiplier),
            right_shift);
}

template <typename T>
void MatrixBatchVectorMultiplyAccumulate(const int8_t* input, const int32_t* bias,
                                         const int8_t* input_to_gate_weights, int32_t multiplier,
                                         int32_t shift, int32_t n_batch, int32_t n_input,
                                         int32_t n_output, int32_t output_zp, T* output) {
    const int16_t output_max = std::numeric_limits<T>::max();
    const int16_t output_min = std::numeric_limits<T>::min();
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int row = 0; row < n_output; ++row) {
            int32_t acc = bias[row];
            for (int col = 0; col < n_input; ++col) {
                int8_t input_val = input[batch * n_input + col];
                int8_t weights_val = input_to_gate_weights[row * n_input + col];
                acc += input_val * weights_val;
            }
            acc = MultiplyByQuantizedMultiplier(acc, multiplier, shift);
            acc += output_zp;
            acc += output[batch * n_output + row];
            if (acc > output_max) {
                acc = output_max;
            }
            if (acc < output_min) {
                acc = output_min;
            }
            output[batch * n_output + row] = static_cast<T>(acc);
        }
    }
}

template <typename T>
int CountLeadingZeros(T integer_input) {
    static_assert(std::is_unsigned<T>::value, "Only unsigned integer types handled.");
#if defined(__GNUC__)
    return integer_input ? __builtin_clz(integer_input) : std::numeric_limits<T>::digits;
#else
    if (integer_input == 0) {
        return std::numeric_limits<T>::digits;
    }

    const T one_in_leading_positive = static_cast<T>(1) << (std::numeric_limits<T>::digits - 1);
    int leading_zeros = 0;
    while (integer_input < one_in_leading_positive) {
        integer_input <<= 1;
        ++leading_zeros;
    }
    return leading_zeros;
#endif
}

inline bool GetInvSqrtQuantizedMultiplierExp(int32_t input, int reverse_shift,
                                             int32_t* output_inv_sqrt, int* output_shift) {
    NN_RET_CHECK_GE(input, 0);
    if (input <= 1) {
        // Handle the input value 1 separately to avoid overflow in that case
        // in the general computation below. Also handle 0 as if it
        // were a 1. 0 is an invalid input here (divide by zero) and 1 is a valid
        // but rare/unrealistic input value. We can expect both to occur in some
        // incompletely trained models, but probably not in fully trained models.
        *output_inv_sqrt = std::numeric_limits<std::int32_t>::max();
        *output_shift = 0;
        return true;
    }

    *output_shift = 11;
    while (input >= (1 << 29)) {
        input /= 4;
        ++*output_shift;
    }
    const unsigned max_left_shift_bits = CountLeadingZeros(static_cast<uint32_t>(input)) - 1;
    const unsigned max_left_shift_bit_pairs = max_left_shift_bits / 2;
    const unsigned left_shift_bit_pairs = max_left_shift_bit_pairs - 1;
    *output_shift -= left_shift_bit_pairs;
    input <<= 2 * left_shift_bit_pairs;
    NN_RET_CHECK_GE(input, (1 << 27));
    NN_RET_CHECK_LT(input, (1 << 29));
    using gemmlowp::FixedPoint;
    using gemmlowp::Rescale;
    using gemmlowp::SaturatingRoundingMultiplyByPOT;
    // Using 3 integer bits gives us enough room for the internal arithmetic in
    // this Newton-Raphson iteration.
    using F3 = FixedPoint<int32_t, 3>;
    using F0 = FixedPoint<int32_t, 0>;
    const F3 fixedpoint_input = F3::FromRaw(input >> 1);
    const F3 fixedpoint_half_input = SaturatingRoundingMultiplyByPOT<-1>(fixedpoint_input);
    const F3 fixedpoint_half_three =
            GEMMLOWP_CHECKED_FIXEDPOINT_CONSTANT(F3, (1 << 28) + (1 << 27), 1.5);
    // Newton-Raphson iteration
    // Naive unoptimized starting guess: x = 1
    F3 x = F3::One();
    // Naive unoptimized number of iterations: 5
    for (int i = 0; i < 5; i++) {
        const F3 x3 = Rescale<3>(x * x * x);
        x = Rescale<3>(fixedpoint_half_three * x - fixedpoint_half_input * x3);
    }
    const F0 fixedpoint_half_sqrt_2 =
            GEMMLOWP_CHECKED_FIXEDPOINT_CONSTANT(F0, 1518500250, std::sqrt(2.) / 2.);
    x = x * fixedpoint_half_sqrt_2;
    *output_inv_sqrt = x.raw();
    if (*output_shift < 0) {
        *output_inv_sqrt <<= -*output_shift;
        *output_shift = 0;
    }
    // Convert right shift (right is positive) to left shift.
    *output_shift *= reverse_shift;
    return true;
}

void ApplyLayerNorm(const int16_t* input, const int16_t* layer_norm_weights, const int32_t* bias,
                    int32_t layer_norm_scale_a, int32_t layer_norm_scale_b, int32_t variance_limit,
                    int n_batch, int n_input, int16_t* output);

void MatrixScalarMultiplyAccumulate(const int8_t* matrix, int32_t scalar, int32_t n_row,
                                    int32_t n_col, int32_t* output);

bool PrecomputeZeroPointTimesWeightWithBias(int32_t zero_point, const int8_t* weight_tensor,
                                            const Shape& weight_shape, const int32_t* bias_tensor,
                                            std::unique_ptr<int32_t[]>* output);

void ApplySigmoid(const int16_t* input, int32_t n_batch, int32_t n_input, int16_t* output);

template <int IntegerBits>
void ApplyTanh(const int16_t* input, int32_t n_batch, int32_t n_input, int16_t* output) {
    using FX = gemmlowp::FixedPoint<std::int16_t, IntegerBits>;
    using F0 = gemmlowp::FixedPoint<std::int16_t, 0>;
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int i = 0; i < n_input; ++i) {
            const int index = batch * n_input + i;
            FX tanh_input = FX::FromRaw(input[index]);
            F0 tanh_output = gemmlowp::tanh(tanh_input);
            output[index] = tanh_output.raw();
        }
    }
}

inline void ApplyTanh(int32_t integer_bits, const int16_t* input, int32_t n_batch, int32_t n_input,
                      int16_t* output) {
    assert(integer_bits <= 6);
#define DISPATCH_TANH(i)                               \
    case i:                                            \
        ApplyTanh<i>(input, n_batch, n_input, output); \
        break;
    switch (integer_bits) {
        DISPATCH_TANH(0);
        DISPATCH_TANH(1);
        DISPATCH_TANH(2);
        DISPATCH_TANH(3);
        DISPATCH_TANH(4);
        DISPATCH_TANH(5);
        DISPATCH_TANH(6);
        default:
            return;
    }
#undef DISPATCH_TANH
}

void CwiseMul(const int16_t* input_1, const int16_t* input_2, int n_batch, int n_input, int shift,
              int16_t* output);
void CwiseMul(const int16_t* input_1, const int16_t* input_2, int32_t multiplier, int32_t shift,
              int32_t n_batch, int32_t n_input, int32_t output_zp, int8_t* output);

bool CheckedLog2(const float x, int* log2_result);

void CwiseAdd(const int16_t* input_1, const int16_t* input_2, int n_batch, int n_input,
              int16_t* output);

inline void Sub1Vector(const int16_t* vector, int v_size, int16_t* result) {
    static const int16_t kOne = 32767;
    for (int v = 0; v < v_size; v++) {
        *result++ = kOne - *vector++;
    }
}

void CwiseClipping(int16_t* input, const int16_t clipping_value, int32_t n_batch, int32_t n_input);

void CwiseClipping(int8_t* input, const int8_t clipping_value, int32_t n_batch, int32_t n_input);

void VectorBatchVectorCwiseProductAccumulate(const int16_t* vector, int v_size,
                                             const int16_t* batch_vector, int n_batch,
                                             int32_t multiplier, int shift, int16_t* result);

}  // namespace nn
}  // namespace android

#endif  // ANDROID_FRAMEWORKS_ML_NN_COMMON_QUANTUTILS_H
