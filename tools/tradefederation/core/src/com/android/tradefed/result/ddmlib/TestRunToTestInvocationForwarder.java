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
package com.android.tradefed.result.ddmlib;

import com.android.ddmlib.testrunner.ITestRunListener;
import com.android.ddmlib.testrunner.TestIdentifier;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestLifeCycleReceiver;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.collect.ImmutableSet;

import java.util.Arrays;
import java.util.Collection;
import java.util.Map;
import java.util.Set;

/**
 * Forwarder from ddmlib {@link ITestRunListener} to {@link ITestLifeCycleReceiver}. Interface that
 * ensure the convertion of results from ddmlib interface to Tradefed Interface.
 *
 * <p>Ddmlib interface is linked to running instrumentation tests.
 */
public class TestRunToTestInvocationForwarder implements ITestRunListener {

    private static final Set<String> INVALID_METHODS =
            ImmutableSet.<String>of("null", "initializationError");

    public static final String ERROR_MESSAGE_FORMAT =
            "Runner reported an invalid method '%s' (%s). Something went wrong, Skipping "
                    + "its reporting.";

    private Collection<ITestLifeCycleReceiver> mListeners;
    private Long mStartTime;
    // Sometimes the instrumentation runner (Android JUnit Runner / AJUR) fails to load some class
    // and report a "null" as a test method. This creates a lot of issues in the reporting pipeline
    // so catch it, and avoid it at the root.
    private TestIdentifier mNullMethod = null;
    private String mNullStack = null;

    public TestRunToTestInvocationForwarder(Collection<ITestLifeCycleReceiver> listeners) {
        mListeners = listeners;
        mStartTime = null;
    }

    public TestRunToTestInvocationForwarder(ITestLifeCycleReceiver listener) {
        this(Arrays.asList(listener));
    }

    @Override
    public void testStarted(TestIdentifier testId) {
        if (INVALID_METHODS.contains(testId.getTestName())) {
            mNullMethod = testId;
            return;
        }
        mNullMethod = null;
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testStarted(TestDescription.createFromTestIdentifier(testId));
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testStarted",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testAssumptionFailure(TestIdentifier testId, String trace) {
        if (mNullMethod != null && mNullMethod.equals(testId)) {
            return;
        }
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testAssumptionFailure(
                        TestDescription.createFromTestIdentifier(testId), trace);
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testAssumptionFailure",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testFailed(TestIdentifier testId, String trace) {
        if (mNullMethod != null && mNullMethod.equals(testId)) {
            mNullStack = trace;
            return;
        }
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testFailed(TestDescription.createFromTestIdentifier(testId), trace);
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testFailed",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testIgnored(TestIdentifier testId) {
        if (mNullMethod != null && mNullMethod.equals(testId)) {
            return;
        }
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testIgnored(TestDescription.createFromTestIdentifier(testId));
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testIgnored",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testEnded(TestIdentifier testId, Map<String, String> testMetrics) {
        if (mNullMethod != null && mNullMethod.equals(testId)) {
            String message =
                    String.format(ERROR_MESSAGE_FORMAT, mNullMethod.getTestName(), mNullMethod);
            if (mNullStack != null) {
                message = String.format("%s Stack:%s", message, mNullStack);
            }
            FailureDescription failure =
                    FailureDescription.create(message, FailureStatus.TEST_FAILURE);
            for (ITestLifeCycleReceiver listener : mListeners) {
                listener.testRunFailed(failure);
            }
            mNullStack = null;
            return;
        }
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testEnded(
                        TestDescription.createFromTestIdentifier(testId),
                        TfMetricProtoUtil.upgradeConvert(testMetrics));
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testEnded",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testRunEnded(long elapsedTime, Map<String, String> runMetrics) {
        if (mStartTime != null) {
            elapsedTime = System.currentTimeMillis() - mStartTime;
        }
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testRunEnded(elapsedTime, TfMetricProtoUtil.upgradeConvert(runMetrics));
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testRunEnded",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testRunFailed(String failure) {
        FailureDescription failureDescription = FailureDescription.create(failure);
        failureDescription.setFailureStatus(FailureStatus.TEST_FAILURE);
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testRunFailed(failureDescription);
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testRunFailed",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        mStartTime = System.currentTimeMillis();
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testRunStarted(runName, testCount);
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testRunStarted",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }

    @Override
    public void testRunStopped(long elapsedTime) {
        for (ITestLifeCycleReceiver listener : mListeners) {
            try {
                listener.testRunStopped(elapsedTime);
            } catch (RuntimeException any) {
                CLog.e(
                        "RuntimeException when invoking %s#testRunStopped",
                        listener.getClass().getName());
                CLog.e(any);
            }
        }
    }
}
