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

#ifndef ANDROID_FRAMEWORKS_ML_NN_EXTENSIONS_EXAMPLE_FIBONACCI_FIBONACCI_EXTENSION_H
#define ANDROID_FRAMEWORKS_ML_NN_EXTENSIONS_EXAMPLE_FIBONACCI_FIBONACCI_EXTENSION_H

/**
 * A sample extension definition.
 *
 * "Example" is a stand-in for the vendor name.
 *
 * Tests are available in
 * runtime/test/fibonacci_extension/FibonacciExtensionTest.cpp
 */

const char EXAMPLE_FIBONACCI_EXTENSION_NAME[] = "com.example.fibonacci";

/**
 * Quantization parameters for {@link EXAMPLE_TENSOR_QUANT64_ASYMM}.
 */
typedef struct ExampleQuant64AsymmParams {
    double scale;
    int64_t zeroPoint;
} ExampleQuant64AsymmParams;

enum {
    /**
     * A signed 64-bit integer scalar value.
     */
    EXAMPLE_INT64 = 0,

    /**
     * A tensor of 64-bit unsigned integers that represent real numbers.
     *
     * Attached to this tensor is {@link ExampleQuant64AsymmParams} that is
     * used to convert the 64-bit bit integer to the real value and vice versa.
     *
     * The formula is:
     *   real_value = (integer_value - zeroPoint) * scale.
     */
    EXAMPLE_TENSOR_QUANT64_ASYMM = 1,
};

enum {
    /**
     * Computes the Fibonacci sequence up to n.
     *
     * Supported input types:
     * - {@link EXAMPLE_INT64}
     * - {@link ANEURALNETWORKS_TENSOR_FLOAT32} (must contain exactly 1 element)
     *
     * Supported output types:
     * - {@link EXAMPLE_TENSOR_QUANT64_ASYMM}
     * - {@link ANEURALNETWORKS_TENSOR_FLOAT32}
     *
     * Inputs:
     * * 0: A scalar n.
     *
     * Outputs:
     * * 0: A 1-D tensor of size n.
     */
    EXAMPLE_FIBONACCI = 0,
};

#endif  // ANDROID_FRAMEWORKS_ML_NN_EXTENSIONS_EXAMPLE_FIBONACCI_FIBONACCI_EXTENSION_H
