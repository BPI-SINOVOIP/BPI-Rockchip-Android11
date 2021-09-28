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

import android.content.res.AssetManager;

import com.android.nn.benchmark.evaluators.MelCepLogF0;
import com.android.nn.benchmark.evaluators.TopK;
import com.android.nn.benchmark.util.IOUtils;

/**
 * Config options for inference accuracy evaluators.
 */
public class EvaluatorConfig {
    private String className;

    // Optional.
    private String outputMeanStdDev;

    // Optional
    private Double expectedTop1;

    public EvaluatorConfig(String className, String outputMeanStdDev, Double expectedTop1) {
        this.className = className;
        this.outputMeanStdDev = outputMeanStdDev;
        this.expectedTop1 = expectedTop1;
    }

    public EvaluatorInterface createEvaluator(AssetManager assetManager) {
        try {
            Class<?> clazz = Class.forName(
                    "com.android.nn.benchmark.evaluators." + className);
            EvaluatorInterface evaluator = (EvaluatorInterface) clazz.getConstructor().newInstance();

            // TODO(pszczepaniak): Refactor this into something more managable.
            if (clazz == MelCepLogF0.class && outputMeanStdDev != null) {
                ((MelCepLogF0)evaluator).setOutputMeanStdDev(new OutputMeanStdDev(
                        IOUtils.readAsset(
                        assetManager, outputMeanStdDev, MeanStdDev.ELEMENT_SIZE_BYTES)));
            }
            if (clazz == TopK.class && expectedTop1 != null) {
                ((TopK)evaluator).expectedTop1 = expectedTop1.floatValue();
            }
            return evaluator;
        } catch (Exception e) {
            throw new IllegalArgumentException(
                    "Can not create evaluator named '" + className + "'", e);
        }
    }
}
