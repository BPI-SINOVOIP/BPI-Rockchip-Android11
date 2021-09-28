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
package com.android.tradefed.testtype;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogcatCrashResultForwarder;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.util.ProcessInfo;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Set;

/**
 * Internal listener to Trade Federation for {@link InstrumentationTest}. It allows to collect extra
 * information needed for easier debugging.
 */
final class InstrumentationListener extends LogcatCrashResultForwarder {

    // Message from ddmlib InstrumentationResultParser for interrupted instrumentation.
    private static final String DDMLIB_INSTRU_FAILURE_MSG = "Test run failed to complete";

    private Set<TestDescription> mTests = new HashSet<>();
    private Set<TestDescription> mDuplicateTests = new HashSet<>();
    private final Collection<TestDescription> mExpectedTests;
    private boolean mDisableDuplicateCheck = false;
    private boolean mReportUnexecutedTests = false;
    private ProcessInfo mSystemServerProcess = null;

    /**
     * @param device
     * @param listeners
     */
    public InstrumentationListener(
            ITestDevice device,
            Collection<TestDescription> expectedTests,
            ITestInvocationListener... listeners) {
        super(device, listeners);
        mExpectedTests = expectedTests;
    }

    /** Whether or not to disable the duplicate test method check. */
    public void setDisableDuplicateCheck(boolean disable) {
        mDisableDuplicateCheck = disable;
    }

    public void setOriginalSystemServer(ProcessInfo info) {
        mSystemServerProcess = info;
    }

    public void setReportUnexecutedTests(boolean enable) {
        mReportUnexecutedTests = enable;
    }

    @Override
    public void testRunStarted(String runName, int testCount) {
        // In case of crash, run will attempt to report with 0
        if (testCount == 0 && !mExpectedTests.isEmpty()) {
            CLog.e("Run reported 0 tests while we collected %s", mExpectedTests.size());
            super.testRunStarted(runName, mExpectedTests.size());
        } else {
            super.testRunStarted(runName, testCount);
        }
    }

    @Override
    public void testStarted(TestDescription test, long startTime) {
        super.testStarted(test, startTime);
        if (!mTests.add(test)) {
            mDuplicateTests.add(test);
        }
    }

    @Override
    public void testRunFailed(FailureDescription error) {
        if (error.getErrorMessage().startsWith(DDMLIB_INSTRU_FAILURE_MSG)) {
            Set<TestDescription> expected = new LinkedHashSet<>(mExpectedTests);
            expected.removeAll(mTests);
            String helpMessage = String.format("The following tests didn't run: %s", expected);
            error.setDebugHelpMessage(helpMessage);
            error.setFailureStatus(FailureStatus.TEST_FAILURE);
            if (mSystemServerProcess != null) {
                boolean restarted = false;
                try {
                    restarted = getDevice().deviceSoftRestarted(mSystemServerProcess);
                } catch (DeviceNotAvailableException e) {
                    // Ignore
                }
                if (restarted) {
                    error.setFailureStatus(FailureStatus.SYSTEM_UNDER_TEST_CRASHED);
                }
            }
        }
        super.testRunFailed(error);
    }

    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        if (!mDuplicateTests.isEmpty() && !mDisableDuplicateCheck) {
            FailureDescription error =
                    FailureDescription.create(
                            String.format(
                                    "The following tests ran more than once: %s. Check "
                                            + "your run configuration, you might be "
                                            + "including the same test class several "
                                            + "times.",
                                    mDuplicateTests));
            error.setFailureStatus(FailureStatus.TEST_FAILURE);
            super.testRunFailed(error);
        } else if (mReportUnexecutedTests && mExpectedTests.size() > mTests.size()) {
            Set<TestDescription> missingTests = new LinkedHashSet<>(mExpectedTests);
            missingTests.removeAll(mTests);
            for (TestDescription miss : missingTests) {
                super.testStarted(miss);
                FailureDescription failure =
                        FailureDescription.create(
                                "test did not run due to instrumentation issue.",
                                FailureStatus.NOT_EXECUTED);
                super.testFailed(miss, failure);
                super.testEnded(miss, new HashMap<String, Metric>());
            }
        }
        super.testRunEnded(elapsedTime, runMetrics);
    }
}
