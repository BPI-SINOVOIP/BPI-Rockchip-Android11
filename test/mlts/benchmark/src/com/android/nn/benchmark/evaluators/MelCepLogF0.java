/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.util.List;

/**
 * Inference evaluator for the TTS model.
 *
 * This validates that the Mel-cep distortion and log F0 error are within the limits.
 */
public class MelCepLogF0 extends BaseSequenceEvaluator {

    static private final float MEL_CEP_DISTORTION_LIMIT = 4f;
    static private final float LOG_F0_ERROR_LIMIT = 0.01f;

    // The TTS model predicts 4 frames per inference.
    // For each frame, there are 40 amplitude values, 7 aperiodicity values,
    // 1 log F0 value and 1 voicing value.
    static private final int FRAMES_PER_INFERENCE = 4;
    static private final int AMPLITUDE_DIMENSION = 40;
    static private final int APERIODICITY_DIMENSION = 7;
    static private final int LOG_F0_DIMENSION = 1;
    static private final int VOICING_DIMENSION = 1;
    static private final int FRAME_OUTPUT_DIMENSION = AMPLITUDE_DIMENSION + APERIODICITY_DIMENSION +
            LOG_F0_DIMENSION + VOICING_DIMENSION;
    // The threshold to classify if a frame is voiced (above threshold) or unvoiced (below).
    static private final float VOICED_THRESHOLD = 0f;

    private float mMaxMelCepDistortion = 0f;
    private float mMaxLogF0Error = 0f;

    @Override
    protected void EvaluateSequenceAccuracy(float[][] outputs, float[][] expectedOutputs,
            List<String> outValidationErrors) {
        float melCepDistortion = calculateMelCepDistortion(outputs, expectedOutputs);
        if (melCepDistortion > MEL_CEP_DISTORTION_LIMIT) {
            outValidationErrors.add("Mel-cep distortion exceeded the limit: " +
                    melCepDistortion);
        }
        mMaxMelCepDistortion = Math.max(mMaxMelCepDistortion, melCepDistortion);

        float logF0Error = calculateLogF0Error(outputs, expectedOutputs);
        if (logF0Error > LOG_F0_ERROR_LIMIT) {
            outValidationErrors.add("Log F0 error exceeded the limit: " + logF0Error);
        }
        mMaxLogF0Error = Math.max(mMaxLogF0Error, logF0Error);
    }

    @Override
    protected void AddValidationResult(List<String> keys, List<Float> values) {
        keys.add("max_mel_cep_distortion");
        values.add(mMaxMelCepDistortion);
        keys.add("max_log_f0_error");
        values.add(mMaxLogF0Error);
    }

    private static float calculateMelCepDistortion(float[][] outputs, float[][] expectedOutputs) {
        int inferenceCount = outputs.length;
        float squared_error = 0;
        for (int inferenceIndex = 0; inferenceIndex < inferenceCount; ++inferenceIndex) {
            for (int frameIndex = 0; frameIndex < FRAMES_PER_INFERENCE; ++frameIndex) {
                // Mel-Cep distortion skips the first amplitude element.
                for (int amplitudeIndex = 1; amplitudeIndex < AMPLITUDE_DIMENSION;
                     ++amplitudeIndex) {
                    int i = frameIndex * FRAME_OUTPUT_DIMENSION + amplitudeIndex;
                    squared_error += Math.pow(
                            outputs[inferenceIndex][i] - expectedOutputs[inferenceIndex][i], 2);
                }
            }
        }

        return (float)Math.sqrt(squared_error /
                (inferenceCount * FRAMES_PER_INFERENCE * (AMPLITUDE_DIMENSION - 1)));
    }

    private static float calculateLogF0Error(float[][] outputs, float[][] expectedOutputs) {
        int inferenceCount = outputs.length;
        float squared_error = 0;
        int count = 0;
        for (int inferenceIndex = 0; inferenceIndex < inferenceCount; ++inferenceIndex) {
            for (int frameIndex = 0; frameIndex < FRAMES_PER_INFERENCE; ++frameIndex) {
                int f0Index = frameIndex * FRAME_OUTPUT_DIMENSION + AMPLITUDE_DIMENSION +
                        APERIODICITY_DIMENSION;
                int voicedIndex = f0Index + LOG_F0_DIMENSION;
                if (outputs[inferenceIndex][voicedIndex] > VOICED_THRESHOLD &&
                        expectedOutputs[inferenceIndex][voicedIndex] > VOICED_THRESHOLD) {
                    squared_error += Math.pow(outputs[inferenceIndex][f0Index] -
                            expectedOutputs[inferenceIndex][f0Index], 2);
                    ++count;
                }
            }
        }
        float logF0Error = 0f;
        if (count > 0) {
            logF0Error = (float)Math.sqrt(squared_error / count);
        }
        return logF0Error;
    }
}
