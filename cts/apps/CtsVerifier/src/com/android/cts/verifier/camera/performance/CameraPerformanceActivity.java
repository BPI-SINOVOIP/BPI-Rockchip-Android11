/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.cts.verifier.camera.performance;

import android.app.AlertDialog;
import android.app.Instrumentation;
import android.app.ProgressDialog;
import android.content.Context;
import android.hardware.camera2.cts.PerformanceTest;
import android.hardware.cts.LegacyCameraPerformanceTest;
import android.os.Bundle;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.ReportLog.Metric;
import com.android.cts.verifier.ArrayTestListAdapter;
import com.android.cts.verifier.DialogTestListActivity;
import com.android.cts.verifier.R;
import com.android.cts.verifier.TestResult;

import org.junit.runner.Description;
import org.junit.runner.JUnitCore;
import org.junit.runner.Request;
import org.junit.runner.Result;
import org.junit.runner.notification.Failure;
import org.junit.runner.notification.RunListener;

import java.lang.annotation.Annotation;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * This test checks the camera performance by running the respective CTS performance test cases
 * and collecting the corresponding KPIs.
 */
public class CameraPerformanceActivity extends DialogTestListActivity {
    private static final String TAG = "CameraPerformanceActivity";
    private static final Class[] TEST_CLASSES =
            { PerformanceTest.class, LegacyCameraPerformanceTest.class };

    private ExecutorService mExecutorService;
    private CameraTestInstrumentation mCameraInstrumentation = new CameraTestInstrumentation();
    private Instrumentation mCachedInstrumentation;
    private Bundle mCachedInstrumentationArgs;
    private HashMap<String, Class> mTestCaseMap = new HashMap<String, Class>();
    private ProgressDialog mSpinnerDialog;
    private AlertDialog mResultDialog;
    private ArrayList<Metric> mResults = new ArrayList<Metric>();

    public CameraPerformanceActivity() {
        super(R.layout.camera_performance, R.string.camera_performance_test,
                R.string.camera_performance_test_info, R.string.camera_performance_test_info);
    }

    private void executeTest(Class testClass, String testName) {
        JUnitCore testRunner = new JUnitCore();
        Log.v(TAG, String.format("Execute Test: %s#%s", testClass.getSimpleName(), testName));
        Request request = Request.method(testClass, testName);
        testRunner.addListener(new CameraRunListener());
        testRunner.run(request);
    }

    private class MetricListener implements CameraTestInstrumentation.MetricListener {
        @Override
        public void onResultMetric(Metric metric) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mResults.add(metric);
                }
            });
        }
    }

    /**
     * Basic {@link RunListener} implementation.
     * It is only used to handle logging into the UI.
     */
    private class CameraRunListener extends RunListener {
        private volatile boolean mCurrentTestReported;

        @Override
        public void testRunStarted(Description description) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mResults.clear();
                    mSpinnerDialog.show();
                }
            });
        }

        @Override
        public void testRunFinished(Result result) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mSpinnerDialog.dismiss();

                    if (!mResults.isEmpty()) {
                        StringBuilder message = new StringBuilder();
                        for (Metric m : mResults) {
                            message.append(String.format("%s : %5.2f %s\n",
                                    m.getMessage().replaceAll("_", " "), m.getValues()[0],
                                    m.getUnit()));
                        }
                        mResultDialog.setMessage(message);
                        mResultDialog.show();
                    }

                    mResults.clear();
                }
            });
        }

        @Override
        public void testStarted(Description description) {
            mCurrentTestReported = false;
        }

        @Override
        public void testFinished(Description description) {
            if (!mCurrentTestReported) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        setTestResult(description.getMethodName(), TestResult.TEST_RESULT_PASSED);
                    }
                });
            }
        }

        @Override
        public void testFailure(Failure failure) {
            mCurrentTestReported = true;

            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    setTestResult(failure.getDescription().getMethodName(),
                            TestResult.TEST_RESULT_FAILED);
                    mSpinnerDialog.dismiss();
                    mResults.clear();
                    String message = new String();
                    String failureMessage = failure.getMessage();
                    String failureTrace = failure.getTrace();
                    if ((failureMessage != null) && (!failureMessage.isEmpty())) {
                        message = failureMessage + "\n";
                    } else if ((failureTrace != null) && (!failureTrace.isEmpty())) {
                        message += failureTrace;
                    }

                    if (!message.isEmpty()) {
                        mResultDialog.setMessage(message);
                        mResultDialog.show();
                    }
                }
            });
        }

        @Override
        public void testAssumptionFailure(Failure failure) {
            mCurrentTestReported = true;
        }

        @Override
        public void testIgnored(Description description) {
            mCurrentTestReported = true;
        }
    }

    private void initializeTestCases(Context ctx) {
        for (Class testClass : TEST_CLASSES) {
            Log.v(TAG, String.format("Test class: %s", testClass.getSimpleName()));
            for (Method method : testClass.getMethods()) {
                Annotation an = method.getAnnotation((Class) org.junit.Test.class);
                Log.v(TAG, String.format("Test method: %s; Annotation: %s", method.getName(), an));
                if (an == null) {
                    continue;
                }
                mTestCaseMap.put(method.getName(), testClass);
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Need to enumerate and initialize the available test cases first
        // before calling the base 'onCreate' implementation.
        initializeTestCases(getApplicationContext());

        super.onCreate(savedInstanceState);

        String spinnerMessage = (String) getResources().getString(
                R.string.camera_performance_spinner_text);
        String resultTitle = (String) getResources().getString(
                R.string.camera_performance_result_title);
        mSpinnerDialog = new ProgressDialog(this);
        mSpinnerDialog.setIndeterminate(true);
        mSpinnerDialog.setCancelable(false);
        mSpinnerDialog.setMessage(spinnerMessage);
        mResultDialog =
                new AlertDialog.Builder(this).setCancelable(true).setTitle(resultTitle).create();

    }

    private class TestListItem extends DialogTestListActivity.DialogTestListItem {
        private String mTestId;

        public TestListItem(Context context, String nameId, String testId) {
            super(context, nameId, testId);
            mTestId = testId;
        }

        @Override
        public void performTest(DialogTestListActivity activity) {
            Class testClass = mTestCaseMap.get(mTestId);
            if (testClass == null) {
                Log.e(TAG, "Test case with name: " + mTestId + " not found!");
                return;
            }

            mExecutorService.execute(new Runnable() {
                @Override
                public void run() {
                    executeTest(testClass, mTestId);
                }
            });
        }
    }

    @Override
    protected void setupTests(ArrayTestListAdapter adapter) {
        Set<String> testCaseNames = mTestCaseMap.keySet();
        for (String testCaseName : testCaseNames) {
            adapter.add(new TestListItem(this, testCaseName, testCaseName));
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        mCameraInstrumentation.initialize(this, new MetricListener());

        try {
            mCachedInstrumentation = InstrumentationRegistry.getInstrumentation();
            mCachedInstrumentationArgs = InstrumentationRegistry.getArguments();
        } catch (IllegalStateException e) {
            // This is expected in case there was no prior instrumentation.
        }
        InstrumentationRegistry.registerInstance(mCameraInstrumentation, new Bundle());

        mExecutorService = Executors.newSingleThreadExecutor();
    }

    @Override
    protected void onPause() {
        super.onPause();

        // Terminate any running test cases.
        mExecutorService.shutdownNow();

        // Restore any cached instrumentation.
        if ((mCachedInstrumentation != null) && (mCachedInstrumentationArgs != null)) {
            InstrumentationRegistry.registerInstance(mCachedInstrumentation,
                    mCachedInstrumentationArgs);
        }
        mCameraInstrumentation.release();
    }
}