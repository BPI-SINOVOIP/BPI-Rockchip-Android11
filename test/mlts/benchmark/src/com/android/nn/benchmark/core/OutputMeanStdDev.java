/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.nn.benchmark.core;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Means and standard deviations for a model's output, used for de-normalization.
 */
public class OutputMeanStdDev {
    private int mNumOutputs;
    private MeanStdDev mMeanStdDevs[];

    public OutputMeanStdDev(byte[] bytes) {
        mNumOutputs = bytes.length / MeanStdDev.DATA_SIZE_BYTES;
        mMeanStdDevs = new MeanStdDev[mNumOutputs];
        ByteBuffer buffer = ByteBuffer.wrap(bytes);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        for (int i = 0; i < mNumOutputs; ++i) {
            mMeanStdDevs[i] = new MeanStdDev(buffer.getFloat(), buffer.getFloat());
        }
    }

    public float[] denormalize(float[] values) {
        if (values.length != mNumOutputs) {
            throw new IllegalArgumentException("Invalid number of values: " + values.length);
        }
        float[] results = new float[mNumOutputs];
        for (int i = 0; i < mNumOutputs; ++i) {
            results[i] = mMeanStdDevs[i].denormalize(values[i]);
        }
        return results;
    }
}
