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

package com.android.tradefed.testtype.suite;

import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.metric.CollectorHelper;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.device.metric.IMetricCollectorReceiver;
import com.android.tradefed.error.IHarnessException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.logger.CurrentInvocation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.result.error.ErrorIdentifier;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.retry.MergeStrategy;
import com.android.tradefed.retry.RetryLogSaverResultForwarder;
import com.android.tradefed.retry.RetryStatistics;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestCollector;
import com.android.tradefed.testtype.ITestFilterReceiver;

import com.google.common.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * A wrapper class works on the {@link IRemoteTest} to granulate the IRemoteTest in testcase level.
 * An IRemoteTest can contain multiple testcases. Previously, these testcases are treated as a
 * whole: When IRemoteTest runs, all testcases will run. Some IRemoteTest (The ones that implements
 * ITestFilterReceiver) can accept a whitelist of testcases and only run those testcases. This class
 * takes advantage of the existing feature and provides a more flexible way to run test suite.
 *
 * <ul>
 *   <li> Single testcase can be retried multiple times (within the same IRemoteTest run) to reduce
 *       the non-test-error failure rates.
 *   <li> The retried testcases are dynamically collected from previous run failures.
 * </ul>
 *
 * <p>Note:
 *
 * <ul>
 *   <li> The prerequisite to run a subset of test cases is that the test type should implement the
 *       interface {@link ITestFilterReceiver}.
 *   <li> X is customized max retry number.
 * </ul>
 */
public class GranularRetriableTestWrapper implements IRemoteTest, ITestCollector {

    private IRetryDecision mRetryDecision;
    private IRemoteTest mTest;
    private List<IMetricCollector> mRunMetricCollectors;
    private TestFailureListener mFailureListener;
    private IInvocationContext mModuleInvocationContext;
    private IConfiguration mModuleConfiguration;
    private ModuleListener mMainGranularRunListener;
    private RetryLogSaverResultForwarder mRetryAttemptForwarder;
    private List<ITestInvocationListener> mModuleLevelListeners;
    private ILogSaver mLogSaver;
    private String mModuleId;
    private int mMaxRunLimit;

    private boolean mCollectTestsOnly = false;

    // Tracking of the metrics
    private RetryStatistics mRetryStats = null;

    public GranularRetriableTestWrapper(
            IRemoteTest test,
            ITestInvocationListener mainListener,
            TestFailureListener failureListener,
            List<ITestInvocationListener> moduleLevelListeners,
            int maxRunLimit) {
        mTest = test;
        mMainGranularRunListener = new ModuleListener(mainListener);
        mFailureListener = failureListener;
        mModuleLevelListeners = moduleLevelListeners;
        mMaxRunLimit = maxRunLimit;
    }

    /** Sets the {@link IRetryDecision} to be used. */
    public void setRetryDecision(IRetryDecision decision) {
        mRetryDecision = decision;
    }

    /**
     * Set the {@link ModuleDefinition} name as a {@link GranularRetriableTestWrapper} attribute.
     *
     * @param moduleId the name of the moduleDefinition.
     */
    public void setModuleId(String moduleId) {
        mModuleId = moduleId;
    }

    /**
     * Set the {@link ModuleDefinition} RunStrategy as a {@link GranularRetriableTestWrapper}
     * attribute.
     *
     * @param skipTestCases whether the testcases should be skipped.
     */
    public void setMarkTestsSkipped(boolean skipTestCases) {
        mMainGranularRunListener.setMarkTestsSkipped(skipTestCases);
    }

    /**
     * Set the {@link ModuleDefinition}'s runMetricCollector as a {@link
     * GranularRetriableTestWrapper} attribute.
     *
     * @param runMetricCollectors A list of MetricCollector for the module.
     */
    public void setMetricCollectors(List<IMetricCollector> runMetricCollectors) {
        mRunMetricCollectors = runMetricCollectors;
    }

    /**
     * Set the {@link ModuleDefinition}'s ModuleConfig as a {@link GranularRetriableTestWrapper}
     * attribute.
     *
     * @param moduleConfiguration Provide the module metrics.
     */
    public void setModuleConfig(IConfiguration moduleConfiguration) {
        mModuleConfiguration = moduleConfiguration;
    }

    /**
     * Set the {@link IInvocationContext} as a {@link GranularRetriableTestWrapper} attribute.
     *
     * @param moduleInvocationContext The wrapper uses the InvocationContext to initialize the
     *     MetricCollector when necessary.
     */
    public void setInvocationContext(IInvocationContext moduleInvocationContext) {
        mModuleInvocationContext = moduleInvocationContext;
    }

    /**
     * Set the Module's {@link ILogSaver} as a {@link GranularRetriableTestWrapper} attribute.
     *
     * @param logSaver The listeners for each test run should save the logs.
     */
    public void setLogSaver(ILogSaver logSaver) {
        mLogSaver = logSaver;
    }

    /**
     * Initialize a new {@link ModuleListener} for each test run.
     *
     * @return a {@link ITestInvocationListener} listener which contains the new {@link
     *     ModuleListener}, the main {@link ITestInvocationListener} and main {@link
     *     TestFailureListener}, and wrapped by RunMetricsCollector and Module MetricCollector (if
     *     not initialized).
     */
    private ITestInvocationListener initializeListeners() {
        List<ITestInvocationListener> currentTestListener = new ArrayList<>();
        // Add all the module level listeners, including TestFailureListener
        if (mModuleLevelListeners != null) {
            currentTestListener.addAll(mModuleLevelListeners);
        }
        currentTestListener.add(mMainGranularRunListener);

        mRetryAttemptForwarder = new RetryLogSaverResultForwarder(mLogSaver, currentTestListener);
        ITestInvocationListener runListener = mRetryAttemptForwarder;
        if (mFailureListener != null) {
            mFailureListener.setLogger(mRetryAttemptForwarder);
            currentTestListener.add(mFailureListener);
        }

        // The module collectors itself are added: this list will be very limited.
        // We clone them since the configuration object is shared across shards.
        for (IMetricCollector collector :
                CollectorHelper.cloneCollectors(mModuleConfiguration.getMetricCollectors())) {
            if (collector.isDisabled()) {
                CLog.d("%s has been disabled. Skipping.", collector);
            } else {
                runListener = collector.init(mModuleInvocationContext, runListener);
            }
        }

        return runListener;
    }

    /**
     * Schedule a series of {@link IRemoteTest#run(TestInformation, ITestInvocationListener)}.
     *
     * @param listener The ResultForwarder listener which contains a new moduleListener for each
     *     run.
     */
    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        mMainGranularRunListener.setCollectTestsOnly(mCollectTestsOnly);
        ITestInvocationListener allListeners = initializeListeners();
        // First do the regular run, not retried.
        intraModuleRun(testInfo, allListeners);

        if (mMaxRunLimit <= 1) {
            return;
        }

        if (mRetryDecision == null) {
            CLog.e("RetryDecision is null. Something is misconfigured this shouldn't happen");
            return;
        }

        // Bail out early if there is no need to retry at all.
        if (!mRetryDecision.shouldRetry(
                mTest, 0, mMainGranularRunListener.getTestRunForAttempts(0))) {
            return;
        }
        // Avoid rechecking the shouldRetry below the first time as it could retrigger reboot.
        boolean firstCheck = true;

        // Deal with retried attempted
        long startTime = System.currentTimeMillis();
        try {
            CLog.d("Starting intra-module retry.");
            for (int attemptNumber = 1; attemptNumber < mMaxRunLimit; attemptNumber++) {
                if (!firstCheck) {
                    boolean retry =
                            mRetryDecision.shouldRetry(
                                    mTest,
                                    attemptNumber - 1,
                                    mMainGranularRunListener.getTestRunForAttempts(
                                            attemptNumber - 1));
                    if (!retry) {
                        return;
                    }
                }
                firstCheck = false;
                CLog.d("Intra-module retry attempt number %s", attemptNumber);
                // Run the tests again
                intraModuleRun(testInfo, allListeners);
            }
            // Feed the last attempt if we reached here.
            mRetryDecision.addLastAttempt(
                    mMainGranularRunListener.getTestRunForAttempts(mMaxRunLimit - 1));
        } finally {
            mRetryStats = mRetryDecision.getRetryStatistics();
            // Track how long we spend in retry
            mRetryStats.mRetryTime = System.currentTimeMillis() - startTime;
        }
    }

    /** The workflow for each individual {@link IRemoteTest} run. */
    private final void intraModuleRun(TestInformation testInfo, ITestInvocationListener runListener)
            throws DeviceNotAvailableException {
        try {
            List<IMetricCollector> clonedCollectors = cloneCollectors(mRunMetricCollectors);
            if (mTest instanceof IMetricCollectorReceiver) {
                ((IMetricCollectorReceiver) mTest).setMetricCollectors(clonedCollectors);
                // If test can receive collectors then let it handle how to set them up
                mTest.run(testInfo, runListener);
            } else {
                // Module only init the collectors here to avoid triggering the collectors when
                // replaying the cached events at the end. This ensures metrics are capture at
                // the proper time in the invocation.
                for (IMetricCollector collector : clonedCollectors) {
                    if (collector.isDisabled()) {
                        CLog.d("%s has been disabled. Skipping.", collector);
                    } else {
                        runListener = collector.init(mModuleInvocationContext, runListener);
                    }
                }
                mTest.run(testInfo, runListener);
            }
        } catch (RuntimeException | AssertionError re) {
            CLog.e("Module '%s' - test '%s' threw exception:", mModuleId, mTest.getClass());
            CLog.e(re);
            CLog.e("Proceeding to the next test.");
            runListener.testRunFailed(createFromException(re));
        } catch (DeviceUnresponsiveException due) {
            // being able to catch a DeviceUnresponsiveException here implies that recovery was
            // successful, and test execution should proceed to next module.
            CLog.w(
                    "Ignored DeviceUnresponsiveException because recovery was "
                            + "successful, proceeding with next module. Stack trace:");
            CLog.w(due);
            CLog.w("Proceeding to the next test.");
            runListener.testRunFailed(createFromException(due));
        } catch (DeviceNotAvailableException dnae) {
            // TODO: See if it's possible to report IReportNotExecuted
            CLog.e("Run in progress was not completed due to:");
            CLog.e(dnae);
            // If it already was marked as failure do not remark it.
            if (!mMainGranularRunListener.hasLastAttemptFailed()) {
                runListener.testRunFailed(createFromException(dnae));
            }
            // Device Not Available Exception are rethrown.
            throw dnae;
        } finally {
            mRetryAttemptForwarder.incrementAttempt();
        }
    }

    /** Get the merged TestRunResults from each {@link IRemoteTest} run. */
    public final List<TestRunResult> getFinalTestRunResults() {
        MergeStrategy strategy = MergeStrategy.getMergeStrategy(mRetryDecision.getRetryStrategy());
        mMainGranularRunListener.setMergeStrategy(strategy);
        return mMainGranularRunListener.getMergedTestRunResults();
    }

    @VisibleForTesting
    Map<String, List<TestRunResult>> getTestRunResultCollected() {
        Map<String, List<TestRunResult>> runResultMap = new LinkedHashMap<>();
        for (String runName : mMainGranularRunListener.getTestRunNames()) {
            runResultMap.put(runName, mMainGranularRunListener.getTestRunAttempts(runName));
        }
        return runResultMap;
    }

    @VisibleForTesting
    List<IMetricCollector> cloneCollectors(List<IMetricCollector> originalCollectors) {
        return CollectorHelper.cloneCollectors(originalCollectors);
    }

    /**
     * Calculate the number of testcases in the {@link IRemoteTest}. This value distincts the same
     * testcases that are rescheduled multiple times.
     */
    public final int getExpectedTestsCount() {
        return mMainGranularRunListener.getExpectedTests();
    }

    /** Returns the listener containing all the results. */
    public ModuleListener getResultListener() {
        return mMainGranularRunListener;
    }

    @Override
    public void setCollectTestsOnly(boolean shouldCollectTest) {
        mCollectTestsOnly = shouldCollectTest;
    }

    private FailureDescription createFromException(Throwable exception) {
        FailureDescription failure =
                CurrentInvocation.createFailure(exception.getMessage(), null).setCause(exception);
        if (exception instanceof IHarnessException) {
            ErrorIdentifier id = ((IHarnessException) exception).getErrorId();
            failure.setErrorIdentifier(id);
            if (id != null) {
                failure.setFailureStatus(id.status());
            }
            failure.setOrigin(((IHarnessException) exception).getOrigin());
        }
        return failure;
    }
}
