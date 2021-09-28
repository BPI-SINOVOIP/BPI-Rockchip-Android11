package com.android.nn.benchmark.evaluators;

import com.android.nn.benchmark.core.EvaluatorInterface;
import com.android.nn.benchmark.core.InferenceInOut;
import com.android.nn.benchmark.core.InferenceInOutSequence;
import com.android.nn.benchmark.core.InferenceResult;
import com.android.nn.benchmark.core.OutputMeanStdDev;
import com.android.nn.benchmark.util.IOUtils;

import java.util.List;

/**
 * Base class for (input/output)sequence-by-sequence evaluation.
 */
public abstract class BaseSequenceEvaluator implements EvaluatorInterface {
    private OutputMeanStdDev mOutputMeanStdDev = null;
    protected int targetOutputIndex = 0;

    public void setOutputMeanStdDev(OutputMeanStdDev outputMeanStdDev) {
        mOutputMeanStdDev = outputMeanStdDev;
    }

    @Override
    public void EvaluateAccuracy(
            List<InferenceInOutSequence> inferenceInOuts, List<InferenceResult> inferenceResults,
            List<String> outKeys, List<Float> outValues,
            List<String> outValidationErrors) {
        if (inferenceInOuts.isEmpty()) {
            throw new IllegalArgumentException("Empty inputs/outputs");
        }

        int dataSize = inferenceInOuts.get(0).mDatasize;
        int outputSize = inferenceInOuts.get(0).get(0).mExpectedOutputs[targetOutputIndex].length
                / dataSize;
        int sequenceIndex = 0;
        int inferenceIndex = 0;
        while (inferenceIndex < inferenceResults.size()) {
            int sequenceLength = inferenceInOuts.get(sequenceIndex % inferenceInOuts.size()).size();
            float[][] outputs = new float[sequenceLength][outputSize];
            float[][] expectedOutputs = new float[sequenceLength][outputSize];
            for (int i = 0; i < sequenceLength; ++i, ++inferenceIndex) {
                InferenceResult result = inferenceResults.get(inferenceIndex);
                if (mOutputMeanStdDev != null) {
                    System.arraycopy(
                            mOutputMeanStdDev.denormalize(
                                    IOUtils.readFloats(result.mInferenceOutput[targetOutputIndex],
                                            dataSize)), 0,
                            outputs[i], 0, outputSize);
                } else {
                    System.arraycopy(
                            IOUtils.readFloats(result.mInferenceOutput[targetOutputIndex],
                                    dataSize), 0,
                            outputs[i], 0, outputSize);
                }

                InferenceInOut inOut = inferenceInOuts.get(result.mInputOutputSequenceIndex)
                        .get(result.mInputOutputIndex);
                if (mOutputMeanStdDev != null) {
                    System.arraycopy(
                            mOutputMeanStdDev.denormalize(
                                    IOUtils.readFloats(inOut.mExpectedOutputs[targetOutputIndex],
                                            dataSize)), 0,
                            expectedOutputs[i], 0, outputSize);
                } else {
                    System.arraycopy(
                            IOUtils.readFloats(inOut.mExpectedOutputs[targetOutputIndex], dataSize),
                            0,
                            expectedOutputs[i], 0, outputSize);
                }
            }

            EvaluateSequenceAccuracy(outputs, expectedOutputs, outValidationErrors);
            ++sequenceIndex;
        }
        AddValidationResult(outKeys, outValues);
    }


    protected abstract void EvaluateSequenceAccuracy(float[][] outputs, float[][] expectedOutputs,
            List<String> outValidationErrors);

    protected abstract void AddValidationResult(List<String> keys, List<Float> values);
}
