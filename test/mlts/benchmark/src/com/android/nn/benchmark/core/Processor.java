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

package com.android.nn.benchmark.core;

import static java.util.concurrent.TimeUnit.MILLISECONDS;

import android.content.Context;
import android.os.Trace;
import android.util.Log;
import android.util.Pair;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

/** Processor is a helper thread for running the work without blocking the UI thread. */
public class Processor implements Runnable {

    public interface Callback {
        void onBenchmarkFinish(boolean ok);

        void onStatusUpdate(int testNumber, int numTests, String modelName);
    }

    protected static final String TAG = "NN_BENCHMARK";
    private Context mContext;

    private final AtomicBoolean mRun = new AtomicBoolean(true);

    volatile boolean mHasBeenStarted = false;
    // You cannot restart a thread, so the completion flag is final
    private final CountDownLatch mCompleted = new CountDownLatch(1);
    private boolean mDoingBenchmark;
    private NNTestBase mTest;
    private int mTestList[];
    private BenchmarkResult mTestResults[];

    private Processor.Callback mCallback;

    private boolean mUseNNApi;
    private boolean mCompleteInputSet;
    private boolean mToggleLong;
    private boolean mTogglePause;

    public Processor(Context context, Processor.Callback callback, int[] testList) {
        mContext = context;
        mCallback = callback;
        mTestList = testList;
        if (mTestList != null) {
            mTestResults = new BenchmarkResult[mTestList.length];
        }
    }

    public void setUseNNApi(boolean useNNApi) {
        mUseNNApi = useNNApi;
    }

    public void setCompleteInputSet(boolean completeInputSet) {
        mCompleteInputSet = completeInputSet;
    }

    public void setToggleLong(boolean toggleLong) {
        mToggleLong = toggleLong;
    }

    public void setTogglePause(boolean togglePause) {
        mTogglePause = togglePause;
    }

    // Method to retrieve benchmark results for instrumentation tests.
    public BenchmarkResult getInstrumentationResult(
            TestModels.TestModelEntry t, float warmupTimeSeconds, float runTimeSeconds)
            throws IOException, BenchmarkException {
        mTest = changeTest(mTest, t);
        BenchmarkResult result = getBenchmark(warmupTimeSeconds, runTimeSeconds);
        mTest.destroy();
        mTest = null;
        return result;
    }

    private NNTestBase changeTest(NNTestBase oldTestBase, TestModels.TestModelEntry t)
            throws BenchmarkException {
        if (oldTestBase != null) {
            // Make sure we don't leak memory.
            oldTestBase.destroy();
        }
        NNTestBase tb = t.createNNTestBase(mUseNNApi, false /* enableIntermediateTensorsDump */);
        if (!tb.setupModel(mContext)) {
            throw new BenchmarkException("Cannot initialise model");
        }
        return tb;
    }

    // Run one loop of kernels for at least the specified minimum time.
    // The function returns the average time in ms for the test run
    private BenchmarkResult runBenchmarkLoop(float minTime, boolean completeInputSet)
            throws IOException {
        try {
            // Run the kernel
            Pair<List<InferenceInOutSequence>, List<InferenceResult>> results;
            if (minTime > 0.f) {
                if (completeInputSet) {
                    results = mTest.runBenchmarkCompleteInputSet(1, minTime);
                } else {
                    results = mTest.runBenchmark(minTime);
                }
            } else {
                results = mTest.runInferenceOnce();
            }
            return BenchmarkResult.fromInferenceResults(
                    mTest.getTestInfo(),
                    mUseNNApi
                            ? BenchmarkResult.BACKEND_TFLITE_NNAPI
                            : BenchmarkResult.BACKEND_TFLITE_CPU,
                    results.first,
                    results.second,
                    mTest.getEvaluator());
        } catch (BenchmarkException e) {
            return new BenchmarkResult(e.getMessage());
        }
    }

    public BenchmarkResult[] getTestResults() {
        return mTestResults;
    }

    // Get a benchmark result for a specific test
    private BenchmarkResult getBenchmark(float warmupTimeSeconds, float runTimeSeconds)
            throws IOException {
        try {
            mTest.checkSdkVersion();
        } catch (UnsupportedSdkException e) {
            BenchmarkResult r = new BenchmarkResult(e.getMessage());
            Log.v(TAG, "Unsupported SDK for test: " + r.toString());
            return r;
        }

        // We run a short bit of work before starting the actual test
        // this is to let any power management do its job and respond.
        // For NNAPI systrace usage documentation, see
        // frameworks/ml/nn/common/include/Tracing.h.
        try {
            final String traceName = "[NN_LA_PWU]runBenchmarkLoop";
            Trace.beginSection(traceName);
            runBenchmarkLoop(warmupTimeSeconds, false);
        } finally {
            Trace.endSection();
        }

        // Run the actual benchmark
        BenchmarkResult r;
        try {
            final String traceName = "[NN_LA_PBM]runBenchmarkLoop";
            Trace.beginSection(traceName);
            r = runBenchmarkLoop(runTimeSeconds, mCompleteInputSet);
        } finally {
            Trace.endSection();
        }

        Log.v(TAG, "Completed benchmark loop");

        return r;
    }

    @Override
    public void run() {
        mHasBeenStarted = true;
        Log.d(TAG, "Processor starting");
        try {
            while (mRun.get()) {
                try {
                    benchmarkAllModels();
                } catch (IOException e) {
                    Log.e(TAG, "IOException during benchmark run", e);
                    break;
                } catch (Throwable e) {
                    Log.e(TAG, "Error during execution", e);
                    throw e;
                }

                mCallback.onBenchmarkFinish(mRun.get());
            }
        } finally {
            mCompleted.countDown();
        }
    }

    private void benchmarkAllModels() throws IOException {
        Log.i(TAG, String.format("Iterating through %d models", mTestList.length));
        // Loop over the tests we want to benchmark
        for (int ct = 0; ct < mTestList.length; ct++) {
            if (!mRun.get()) {
                Log.v(TAG, String.format("Asked to stop execution at model #%d", ct));
                break;
            }
            // For reproducibility we wait a short time for any sporadic work
            // created by the user touching the screen to launch the test to pass.
            // Also allows for things to settle after the test changes.
            try {
                Thread.sleep(250);
            } catch (InterruptedException ignored) {
                Thread.currentThread().interrupt();
                break;
            }

            TestModels.TestModelEntry testModel =
                    TestModels.modelsList().get(mTestList[ct]);

            Log.i(TAG, String.format("%d/%d: '%s'", ct, mTestList.length,
                    testModel.mTestName));
            int testNumber = ct + 1;
            mCallback.onStatusUpdate(testNumber, mTestList.length,
                    testModel.toString());

            // Select the next test
            try {
                mTest = changeTest(mTest, testModel);
            } catch (BenchmarkException e) {
                Log.w(TAG, String.format("Cannot initialise test %d: '%s', skipping", ct,
                        testModel.mTestName), e);
            }

            // If the user selected the "long pause" option, wait
            if (mTogglePause) {
                for (int i = 0; (i < 100) && mRun.get(); i++) {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException ignored) {
                        Thread.currentThread().interrupt();
                        break;
                    }
                }
            }

            // Run the test
            float warmupTime = 0.3f;
            float runTime = 1.f;
            if (mToggleLong) {
                warmupTime = 2.f;
                runTime = 10.f;
            }
            Log.i(TAG, "Running test for model " + testModel.mModelName + " file "
                    + testModel.mModelFile);
            mTestResults[ct] = getBenchmark(warmupTime, runTime);
        }
    }

    public void exit() {
        exitWithTimeout(-1l);
    }

    public void exitWithTimeout(long timeoutMs) {
        mRun.set(false);

        if (mHasBeenStarted) {
            try {
                if (timeoutMs > 0) {
                    boolean hasCompleted = mCompleted.await(timeoutMs, MILLISECONDS);
                    if (!hasCompleted) {
                        Log.w(TAG, "Exiting before execution actually completed");
                    }
                } else {
                    mCompleted.await();
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                Log.w(TAG, "Interrupted while waiting for Processor to complete", e);
            }
        }

        if (mTest != null) {
            mTest.destroy();
            mTest = null;
        }
    }
}
