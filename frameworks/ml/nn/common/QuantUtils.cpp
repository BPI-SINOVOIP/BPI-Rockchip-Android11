#include "QuantUtils.h"

#include <algorithm>
#include <limits>
#include <memory>

namespace android {
namespace nn {

void ApplyLayerNorm(const int16_t* input, const int16_t* layer_norm_weights, const int32_t* bias,
                    int32_t layer_norm_scale_a, int32_t layer_norm_scale_b, int32_t variance_limit,
                    int n_batch, int n_input, int16_t* output) {
    static const int kOverflowGuard = 1 << 20;
    for (int i = 0; i < n_batch; ++i) {
        int64_t sum = 0;
        int64_t sum_sq = 0;
        for (int j = 0; j < n_input; ++j) {
            const int32_t index = i * n_input + j;
            int32_t val = static_cast<int32_t>(input[index]);
            sum += val;
            sum_sq += val * val;
        }
        int32_t mean = static_cast<int32_t>(static_cast<int64_t>(sum) * 1024 / n_input);
        // TODO(jianlijianli): Avoids overflow but only works for POT n_input.
        int32_t temp = kOverflowGuard / n_input;
        int64_t variance = sum_sq * temp - static_cast<int64_t>(mean) * static_cast<int64_t>(mean);
        int32_t variance2 = static_cast<int32_t>(variance / kOverflowGuard);
        if (variance2 < 1) {
            variance2 = variance_limit;
        }
        int32_t stddev_inverse_a;
        int stddev_inverse_b;
        GetInvSqrtQuantizedMultiplierExp(variance2, /*reverse_shift*/ -1, &stddev_inverse_a,
                                         &stddev_inverse_b);

        for (int j = 0; j < n_input; ++j) {
            const int32_t index = i * n_input + j;
            int32_t val = static_cast<int32_t>(input[index]);
            int32_t shifted = 1024 * val - mean;
            int32_t rescaled =
                    MultiplyByQuantizedMultiplier(shifted, stddev_inverse_a, stddev_inverse_b);
            // TODO(jianlijianli): Saturate this.
            int64_t val3 = rescaled * layer_norm_weights[j] + bias[j];
            int32_t val4 = static_cast<int32_t>((val3 > 0 ? val3 + 512 : val3 - 512) / 1024);
            int32_t val5 = MultiplyByQuantizedMultiplier(val4, layer_norm_scale_a,
                                                         layer_norm_scale_b + 12);
            val5 = std::min(std::max(INT16_MIN, val5), INT16_MAX);
            output[index] = static_cast<int16_t>(val5);
        }
    }
}

void MatrixScalarMultiplyAccumulate(const int8_t* matrix, int32_t scalar, int32_t n_row,
                                    int32_t n_col, int32_t* output) {
    for (int i = 0; i < n_row; ++i) {
        int32_t row_sum = 0;
        for (int j = 0; j < n_col; ++j) {
            row_sum += *matrix++;
        }
        output[i] += row_sum * scalar;
    }
}

bool PrecomputeZeroPointTimesWeightWithBias(int32_t zero_point, const int8_t* weight_tensor,
                                            const Shape& weight_shape, const int32_t* bias_tensor,
                                            std::unique_ptr<int32_t[]>* output) {
    if (weight_tensor == nullptr) {
        return true;
    }

    NN_RET_CHECK_EQ(weight_shape.dimensions.size(), 2u);
    const int row = weight_shape.dimensions[0];
    const int col = weight_shape.dimensions[1];
    *output = std::make_unique<int32_t[]>(row);
    if (bias_tensor == nullptr) {
        memset(output->get(), 0, row * sizeof(int32_t));
    } else {
        memcpy(output->get(), bias_tensor, row * sizeof(int32_t));
    }
    if (zero_point != 0) {
        MatrixScalarMultiplyAccumulate(weight_tensor, zero_point, row, col, output->get());
    }
    return true;
}

void ApplySigmoid(const int16_t* input, int32_t n_batch, int32_t n_input, int16_t* output) {
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int c = 0; c < n_input; c++) {
            using F3 = gemmlowp::FixedPoint<std::int16_t, 3>;
            using F0 = gemmlowp::FixedPoint<std::int16_t, 0>;
            const int index = batch * n_input + c;
            F3 sigmoid_input = F3::FromRaw(input[index]);
            F0 sigmoid_output = gemmlowp::logistic(sigmoid_input);
            output[index] = sigmoid_output.raw();
        }
    }
}

void CwiseMul(const int16_t* input_1, const int16_t* input_2, int n_batch, int n_input, int shift,
              int16_t* output) {
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int i = 0; i < n_input; ++i) {
            const int index = batch * n_input + i;
            const int16_t a = input_1[index];
            const int16_t b = input_2[index];
            const int32_t value = static_cast<int32_t>(a) * static_cast<int32_t>(b);
            output[index] = static_cast<int16_t>(gemmlowp::RoundingDivideByPOT(value, shift));
        }
    }
}

void CwiseMul(const int16_t* input_1, const int16_t* input_2, int32_t multiplier, int32_t shift,
              int32_t n_batch, int32_t n_input, int32_t output_zp, int8_t* output) {
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int i = 0; i < n_input; ++i) {
            const int index = batch * n_input + i;
            const int16_t a = input_1[index];
            const int16_t b = input_2[index];
            int32_t value = static_cast<int32_t>(a) * static_cast<int32_t>(b);
            value = MultiplyByQuantizedMultiplier(value, multiplier, shift);
            value -= output_zp;
            value = std::min(std::max(-128, value), 127);

            output[index] = static_cast<int8_t>(value);
        }
    }
}

bool CheckedLog2(const float x, int* log2_result) {
    const float x_log2 = std::log(x) * (1.0f / std::log(2.0f));
    const float x_log2_rounded = std::round(x_log2);
    const float x_log2_fracpart = x_log2 - x_log2_rounded;

    *log2_result = static_cast<int>(x_log2_rounded);
    return std::abs(x_log2_fracpart) < 1e-3;
}

void CwiseAdd(const int16_t* input_1, const int16_t* input_2, int n_batch, int n_input,
              int16_t* output) {
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int i = 0; i < n_input; ++i) {
            const int index = batch * n_input + i;
            int32_t sum = input_1[index] + input_2[index];
            const int32_t sum_clamped = std::min(INT16_MAX, std::max(INT16_MIN, sum));
            output[index] = static_cast<int16_t>(sum_clamped);
        }
    }
}

void CwiseClipping(int16_t* input, const int16_t clipping_value, int32_t n_batch, int32_t n_input) {
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int i = 0; i < n_input; ++i) {
            const int index = batch * n_input + i;
            if (input[index] > clipping_value) {
                input[index] = clipping_value;
            }
            if (input[index] < -clipping_value) {
                input[index] = -clipping_value;
            }
        }
    }
}

void CwiseClipping(int8_t* input, const int8_t clipping_value, int32_t n_batch, int32_t n_input) {
    for (int batch = 0; batch < n_batch; ++batch) {
        for (int i = 0; i < n_input; ++i) {
            const int index = batch * n_input + i;
            if (input[index] > clipping_value) {
                input[index] = clipping_value;
            }
            if (input[index] < -clipping_value) {
                input[index] = -clipping_value;
            }
        }
    }
}

void VectorBatchVectorCwiseProductAccumulate(const int16_t* vector, int v_size,
                                             const int16_t* batch_vector, int n_batch,
                                             int32_t multiplier, int shift, int16_t* result) {
    for (int b = 0; b < n_batch; b++) {
        for (int v = 0; v < v_size; v++) {
            int32_t prod = vector[v] * *batch_vector++;
            prod = MultiplyByQuantizedMultiplier(prod, multiplier, shift);
            int32_t output = prod + *result;
            output = std::max(std::min(32767, output), -32768);
            *result++ = output;
        }
    }
}

}  // namespace nn
}  // namespace android
