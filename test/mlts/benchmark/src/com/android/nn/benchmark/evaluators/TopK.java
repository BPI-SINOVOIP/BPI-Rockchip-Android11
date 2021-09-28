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

package com.android.nn.benchmark.evaluators;

import android.util.Pair;

import com.android.nn.benchmark.core.EvaluatorInterface;
import com.android.nn.benchmark.core.InferenceInOut;
import com.android.nn.benchmark.core.InferenceInOutSequence;
import com.android.nn.benchmark.core.InferenceResult;
import com.android.nn.benchmark.util.IOUtils;

import java.util.Comparator;
import java.util.List;
import java.util.PriorityQueue;

/**
 * Accuracy evaluator for classifiers - top-k accuracy (with k=5).
 */

public class TopK implements EvaluatorInterface {

    public static final int K_TOP = 5;
    public static final float VALIDATION_TOP1_THRESHOLD = 0.05f;
    public float expectedTop1 = 0.0f;
    public int targetOutputIndex = 0;

    public void EvaluateAccuracy(
            List<InferenceInOutSequence> inferenceInOuts,
            List<InferenceResult> inferenceResults,
            List<String> outKeys,
            List<Float> outValues,
            List<String> outValidationErrors) {

        int total = 0;
        int[] topk = new int[K_TOP];
        for (int i = 0; i < inferenceResults.size(); i++) {
            InferenceResult result = inferenceResults.get(i);
            if (result.mInferenceOutput == null) {
                throw new IllegalArgumentException("Needs mInferenceOutput for TopK");
            }
            InferenceInOutSequence sequence = inferenceInOuts.get(result.mInputOutputSequenceIndex);
            if (sequence.size() != 1) {
                throw new IllegalArgumentException("Only one item in InferenceInOutSequenece " +
                        "supported by TopK evaluator");
            }
            if (result.mInputOutputIndex != 0) {
                throw new IllegalArgumentException("Unexpected non-zero InputOutputIndex");
            }
            InferenceInOut io = sequence.get(0);
            final int expectedClass = io.mExpectedClass;
            if (expectedClass < 0) {
                throw new IllegalArgumentException("expected class not set");
            }
            PriorityQueue<Pair<Integer, Float>> sorted = new PriorityQueue<Pair<Integer, Float>>(
                    new Comparator<Pair<Integer, Float>>() {
                        @Override
                        public int compare(Pair<Integer, Float> o1, Pair<Integer, Float> o2) {
                            // Note reverse order to get highest probability first
                            return o2.second.compareTo(o1.second);
                        }
                    });
            float[] probabilities = IOUtils.readFloats(result.mInferenceOutput[targetOutputIndex],
                    sequence.mDatasize);
            for (int index = 0; index < probabilities.length; index++) {
                sorted.add(new Pair<>(index, probabilities[index]));
            }
            total++;
            boolean seen = false;
            for (int k = 0; k < K_TOP; k++) {
                Pair<Integer, Float> top = sorted.remove();
                if (top.first.intValue() == expectedClass) {
                    seen = true;
                }
                if (seen) {
                    topk[k]++;
                }
            }
        }
        for (int i = 0; i < K_TOP; i++) {
            outKeys.add("top_" + (i + 1));
            outValues.add(new Float((float) topk[i] / (float) total));
        }

        if (expectedTop1 > 0.0) {
            float top1 = ((float) topk[0] / (float) total);
            float lowestTop1 = expectedTop1 - VALIDATION_TOP1_THRESHOLD;
            if (top1 < lowestTop1) {
                outValidationErrors.add(
                        "Top 1 value is far below the validation threshold " +
                                String.format("%.2f%%", expectedTop1 * 100.0));
            }
        }
    }
}
