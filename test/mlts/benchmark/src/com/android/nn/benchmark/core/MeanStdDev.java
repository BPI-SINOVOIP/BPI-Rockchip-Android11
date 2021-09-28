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

/**
 * Mean and standard deviation pair, used for (de)normalization.
 */
public class MeanStdDev {
    public static final int ELEMENT_SIZE_BYTES = 4;
    public static final int DATA_SIZE_BYTES = 2 * ELEMENT_SIZE_BYTES;

    public float mean;
    public float stdDev;

    public MeanStdDev(float mean, float stdDev) {
        this.mean = mean;
        this.stdDev = stdDev;
    }

    public float denormalize(float value) {
        return value * stdDev + mean;
    }
}
