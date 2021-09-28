/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.apps.common.util;

/**
 * Simple utilities to deal with vector math.
 */
public class VectorMath {

    private VectorMath() {
    }

    /** Error threshold constant. */
    public static final float EPSILON = 0.0001f;

    /** Returns the dot product of the given vectors. */
    public static float dotProduct(float vx, float vy, float ux, float uy) {
        return (vx * ux) + (vy * uy);
    }

    /** Returns the Euclidean norm of the given vector. */
    public static float norm2(float vx, float vy) {
        return (float) Math.sqrt(dotProduct(vx, vy, vx, vy));
    }

    /** Returns the center of the given coordinates. */
    public static float center(float a, float b) {
        return (a + b) / 2.0f;
    }
}
