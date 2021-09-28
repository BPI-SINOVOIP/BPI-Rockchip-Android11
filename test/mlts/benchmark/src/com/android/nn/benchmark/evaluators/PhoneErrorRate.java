package com.android.nn.benchmark.evaluators;

import com.android.nn.benchmark.util.SequenceUtils;

import java.util.List;

/**
 * Inference evaluator for the ASR model.
 *
 * This validates that the Phone Error Rate (PER) is within the limit.
 */
public class PhoneErrorRate extends BaseSequenceEvaluator {
    static private final float PHONE_ERROR_RATE_LIMIT = 5f;  // 5%

    private float mMaxPER = 0f;

    @Override
    protected void EvaluateSequenceAccuracy(float[][] outputs, float[][] expectedOutputs,
            List<String> outValidationErrors) {
        float per = calculatePER(outputs, expectedOutputs);
        if (per > PHONE_ERROR_RATE_LIMIT) {
            outValidationErrors.add("Phone error rate exceeded the limit: " + per);
        }
        mMaxPER = Math.max(mMaxPER, per);
    }

    @Override
    protected void AddValidationResult(List<String> keys, List<Float> values) {
        keys.add("max_phone_error_rate");
        values.add(mMaxPER);
    }

    /** Calculates Phone Error Rate in percent. */
    private static float calculatePER(float[][] outputs, float[][] expectedOutputs) {
        int inferenceCount = outputs.length;
        int[] outputPhones = new int[inferenceCount];
        int[] expectedOutputPhones = new int[inferenceCount];
        for (int i = 0; i < inferenceCount; ++i) {
            outputPhones[i] = SequenceUtils.indexOfLargest(outputs[i]);
            expectedOutputPhones[i] = SequenceUtils.indexOfLargest(expectedOutputs[i]);
        }
        int errorCount = SequenceUtils.calculateEditDistance(outputPhones, expectedOutputPhones);
        return (float)(errorCount * 100.0 / inferenceCount);
    }
}
