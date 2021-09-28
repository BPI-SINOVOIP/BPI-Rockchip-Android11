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

import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.Pair;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BenchmarkResult implements Parcelable {
    public final static String BACKEND_TFLITE_NNAPI = "TFLite_NNAPI";
    public final static String BACKEND_TFLITE_CPU = "TFLite_CPU";

    private final static int TIME_FREQ_ARRAY_SIZE = 32;

    private float mTotalTimeSec;
    private float mSumOfMSEs;
    private float mMaxSingleError;
    private int mIterations;
    private float mTimeStdDeviation;
    private String mTestInfo;
    private int mNumberOfEvaluatorResults;
    private String[] mEvaluatorKeys = {};
    private float[] mEvaluatorResults = {};

    /** Type of backend used for inference */
    private String mBackendType;

    /** Time offset for inference frequency counts */
    private float mTimeFreqStartSec;

    /** Index time offset for inference frequency counts */
    private float mTimeFreqStepSec;

    /**
     * Array of inference frequency counts.
     * Each entry contains inference count for time range:
     * [mTimeFreqStartSec + i*mTimeFreqStepSec, mTimeFreqStartSec + (1+i*mTimeFreqStepSec)
     */
    private float[] mTimeFreqSec = {};

    /** Size of test set using for inference */
    private int mTestSetSize;

    /** List of validation errors */
    private String[] mValidationErrors = {};

    /** Error that prevents the benchmark from running, e.g. SDK version not supported. */
    private String mBenchmarkError;

    public BenchmarkResult(float totalTimeSec, int iterations, float timeVarianceSec,
            float sumOfMSEs, float maxSingleError, String testInfo,
            String[] evaluatorKeys, float[] evaluatorResults,
            float timeFreqStartSec, float timeFreqStepSec, float[] timeFreqSec,
            String backendType, int testSetSize, String[] validationErrors) {
        mTotalTimeSec = totalTimeSec;
        mSumOfMSEs = sumOfMSEs;
        mMaxSingleError = maxSingleError;
        mIterations = iterations;
        mTimeStdDeviation = timeVarianceSec;
        mTestInfo = testInfo;
        mTimeFreqStartSec = timeFreqStartSec;
        mTimeFreqStepSec = timeFreqStepSec;
        mTimeFreqSec = timeFreqSec;
        mBackendType = backendType;
        mTestSetSize = testSetSize;
        if (validationErrors == null) {
            mValidationErrors = new String[0];
        } else {
            mValidationErrors = validationErrors;
        }

        if (evaluatorKeys == null) {
            mEvaluatorKeys = new String[0];
        } else {
            mEvaluatorKeys = evaluatorKeys;
        }
        if (evaluatorResults == null) {
            mEvaluatorResults = new float[0];
        } else {
            mEvaluatorResults = evaluatorResults;
        }
        if (mEvaluatorResults.length != mEvaluatorKeys.length) {
            throw new IllegalArgumentException("Different number of evaluator keys vs values");
        }
        mNumberOfEvaluatorResults = mEvaluatorResults.length;
    }

    public BenchmarkResult(String benchmarkError) {
        mBenchmarkError = benchmarkError;
    }

    public boolean hasValidationErrors() {
        return mValidationErrors.length > 0;
    }

    protected BenchmarkResult(Parcel in) {
        mTotalTimeSec = in.readFloat();
        mSumOfMSEs = in.readFloat();
        mMaxSingleError = in.readFloat();
        mIterations = in.readInt();
        mTimeStdDeviation = in.readFloat();
        mTestInfo = in.readString();
        mNumberOfEvaluatorResults = in.readInt();
        mEvaluatorKeys = new String[mNumberOfEvaluatorResults];
        in.readStringArray(mEvaluatorKeys);
        mEvaluatorResults = new float[mNumberOfEvaluatorResults];
        in.readFloatArray(mEvaluatorResults);
        if (mEvaluatorResults.length != mEvaluatorKeys.length) {
            throw new IllegalArgumentException("Different number of evaluator keys vs values");
        }
        mTimeFreqStartSec = in.readFloat();
        mTimeFreqStepSec = in.readFloat();
        int timeFreqSecLength = in.readInt();
        mTimeFreqSec = new float[timeFreqSecLength];
        in.readFloatArray(mTimeFreqSec);
        mBackendType = in.readString();
        mTestSetSize = in.readInt();
        int validationsErrorsSize = in.readInt();
        mValidationErrors = new String[validationsErrorsSize];
        in.readStringArray(mValidationErrors);
        mBenchmarkError = in.readString();
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeFloat(mTotalTimeSec);
        dest.writeFloat(mSumOfMSEs);
        dest.writeFloat(mMaxSingleError);
        dest.writeInt(mIterations);
        dest.writeFloat(mTimeStdDeviation);
        dest.writeString(mTestInfo);
        dest.writeInt(mNumberOfEvaluatorResults);
        dest.writeStringArray(mEvaluatorKeys);
        dest.writeFloatArray(mEvaluatorResults);
        dest.writeFloat(mTimeFreqStartSec);
        dest.writeFloat(mTimeFreqStepSec);
        dest.writeInt(mTimeFreqSec.length);
        dest.writeFloatArray(mTimeFreqSec);
        dest.writeString(mBackendType);
        dest.writeInt(mTestSetSize);
        dest.writeInt(mValidationErrors.length);
        dest.writeStringArray(mValidationErrors);
        dest.writeString(mBenchmarkError);
    }

    @SuppressWarnings("unused")
    public static final Parcelable.Creator<BenchmarkResult> CREATOR =
            new Parcelable.Creator<BenchmarkResult>() {
                @Override
                public BenchmarkResult createFromParcel(Parcel in) {
                    return new BenchmarkResult(in);
                }

                @Override
                public BenchmarkResult[] newArray(int size) {
                    return new BenchmarkResult[size];
                }
            };

    public float getError() {
        return mSumOfMSEs;
    }

    public float getMeanTimeSec() {
        return mTotalTimeSec / mIterations;
    }

    public List<Pair<String, Float>> getEvaluatorResults() {
        List<Pair<String, Float>> results = new ArrayList<>();
        for (int i = 0; i < mEvaluatorKeys.length; ++i) {
            results.add(new Pair<>(mEvaluatorKeys[i], mEvaluatorResults[i]));
        }
        return results;
    }

    @Override
    public String toString() {
        if (!TextUtils.isEmpty(mBenchmarkError)) {
            return mBenchmarkError;
        }

        StringBuilder result = new StringBuilder("BenchmarkResult{" +
                "mTestInfo='" + mTestInfo + '\'' +
                ", getMeanTimeSec()=" + getMeanTimeSec() +
                ", mTotalTimeSec=" + mTotalTimeSec +
                ", mSumOfMSEs=" + mSumOfMSEs +
                ", mMaxSingleErrors=" + mMaxSingleError +
                ", mIterations=" + mIterations +
                ", mTimeStdDeviation=" + mTimeStdDeviation +
                ", mTimeFreqStartSec=" + mTimeFreqStartSec +
                ", mTimeFreqStepSec=" + mTimeFreqStepSec +
                ", mBackendType=" + mBackendType +
                ", mTestSetSize=" + mTestSetSize);
        for (int i = 0; i < mEvaluatorKeys.length; i++) {
            result.append(", ").append(mEvaluatorKeys[i]).append("=").append(mEvaluatorResults[i]);
        }

        result.append(", mValidationErrors=[");
        for (int i = 0; i < mValidationErrors.length; i++) {
            result.append(mValidationErrors[i]);
            if (i < mValidationErrors.length - 1) {
                result.append(",");
            }
        }
        result.append("]");
        result.append('}');
        return result.toString();
    }

    public String getSummary(float baselineSec) {
        if (!TextUtils.isEmpty(mBenchmarkError)) {
            return mBenchmarkError;
        }

        java.text.DecimalFormat df = new java.text.DecimalFormat("######.##");

        return df.format(rebase(getMeanTimeSec(), baselineSec)) +
            "X, n=" + mIterations + ", μ=" + df.format(getMeanTimeSec() * 1000.0)
            + "ms, σ=" + df.format(mTimeStdDeviation * 1000.0) + "ms";
    }

    public Bundle toBundle(String testName) {
        Bundle results = new Bundle();
        if (!TextUtils.isEmpty(mBenchmarkError)) {
            results.putString(testName + "_error", mBenchmarkError);
            return results;
        }

        // Reported in ms
        results.putFloat(testName + "_avg", getMeanTimeSec() * 1000.0f);
        results.putFloat(testName + "_std_dev", mTimeStdDeviation * 1000.0f);
        results.putFloat(testName + "_total_time", mTotalTimeSec * 1000.0f);
        results.putFloat(testName + "_mean_square_error", mSumOfMSEs / mIterations);
        results.putFloat(testName + "_max_single_error", mMaxSingleError);
        results.putInt(testName + "_iterations", mIterations);
        for (int i = 0; i < mEvaluatorKeys.length; i++) {
            results.putFloat(testName + "_" + mEvaluatorKeys[i],
                mEvaluatorResults[i]);
        }
        return results;
    }

    @SuppressWarnings("AndroidJdkLibsChecker")
    public String toCsvLine() {
        if (!TextUtils.isEmpty(mBenchmarkError)) {
            return "";
        }

        StringBuilder sb = new StringBuilder();
        sb.append(String.join(",",
            mTestInfo,
            mBackendType,
            String.valueOf(mIterations),
            String.valueOf(mTotalTimeSec),
            String.valueOf(mMaxSingleError),
            String.valueOf(mTestSetSize),
            String.valueOf(mTimeFreqStartSec),
            String.valueOf(mTimeFreqStepSec),
            String.valueOf(mEvaluatorKeys.length),
            String.valueOf(mTimeFreqSec.length),
            String.valueOf(mValidationErrors.length)));

        for (int i = 0; i < mEvaluatorKeys.length; ++i) {
            sb.append(',').append(mEvaluatorKeys[i]);
        }

        for (int i = 0; i < mEvaluatorKeys.length; ++i) {
            sb.append(',').append(mEvaluatorResults[i]);
        }

        for (float value : mTimeFreqSec) {
            sb.append(',').append(value);
        }

        for (String validationError : mValidationErrors) {
            sb.append(',').append(validationError.replace(',', ' '));
        }

        sb.append('\n');
        return sb.toString();
    }

    float rebase(float v, float baselineSec) {
        if (v > 0.001) {
            v = baselineSec / v;
        }
        return v;
    }

    public static BenchmarkResult fromInferenceResults(
            String testInfo,
            String backendType,
            List<InferenceInOutSequence> inferenceInOuts,
            List<InferenceResult> inferenceResults,
            EvaluatorInterface evaluator) {
        float totalTime = 0;
        int iterations = 0;
        float sumOfMSEs = 0;
        float maxSingleError = 0;

        float maxComputeTimeSec = 0.0f;
        float minComputeTimeSec = Float.MAX_VALUE;

        for (InferenceResult iresult : inferenceResults) {
            iterations++;
            totalTime += iresult.mComputeTimeSec;
            if (iresult.mMeanSquaredErrors != null) {
                for (float mse : iresult.mMeanSquaredErrors) {
                    sumOfMSEs += mse;
                }
            }
            if (iresult.mMaxSingleErrors != null) {
                for (float mse : iresult.mMaxSingleErrors) {
                    if (mse > maxSingleError) {
                        maxSingleError = mse;
                    }
                }
            }

            if (maxComputeTimeSec < iresult.mComputeTimeSec) {
                maxComputeTimeSec = iresult.mComputeTimeSec;
            }
            if (minComputeTimeSec > iresult.mComputeTimeSec) {
                minComputeTimeSec = iresult.mComputeTimeSec;
            }
        }

        float inferenceMean = (totalTime / iterations);

        float variance = 0.0f;
        for (InferenceResult iresult : inferenceResults) {
            float v = (iresult.mComputeTimeSec - inferenceMean);
            variance += v * v;
        }
        variance /= iterations;
        String[] evaluatorKeys = null;
        float[] evaluatorResults = null;
        String[] validationErrors = null;
        if (evaluator != null) {
            ArrayList<String> keys = new ArrayList<String>();
            ArrayList<Float> results = new ArrayList<Float>();
            ArrayList<String> validationErrorsList = new ArrayList<>();
            evaluator.EvaluateAccuracy(inferenceInOuts, inferenceResults, keys, results,
                    validationErrorsList);
            evaluatorKeys = new String[keys.size()];
            evaluatorKeys = keys.toArray(evaluatorKeys);
            evaluatorResults = new float[results.size()];
            for (int i = 0; i < evaluatorResults.length; i++) {
                evaluatorResults[i] = results.get(i).floatValue();
            }
            validationErrors = new String[validationErrorsList.size()];
            validationErrorsList.toArray(validationErrors);
        }

        // Calculate inference frequency/histogram across TIME_FREQ_ARRAY_SIZE buckets.
        float[] timeFreqSec = new float[TIME_FREQ_ARRAY_SIZE];
        float stepSize = (maxComputeTimeSec - minComputeTimeSec) / (TIME_FREQ_ARRAY_SIZE - 1);
        for (InferenceResult iresult : inferenceResults) {
            timeFreqSec[(int) ((iresult.mComputeTimeSec - minComputeTimeSec) / stepSize)] += 1;
        }

        // Calc test set size
        int testSetSize = 0;
        for (InferenceInOutSequence iios : inferenceInOuts) {
            testSetSize += iios.size();
        }

        return new BenchmarkResult(totalTime, iterations, (float) Math.sqrt(variance),
                sumOfMSEs, maxSingleError, testInfo, evaluatorKeys, evaluatorResults,
                minComputeTimeSec, stepSize, timeFreqSec, backendType, testSetSize,
                validationErrors);
    }
}
