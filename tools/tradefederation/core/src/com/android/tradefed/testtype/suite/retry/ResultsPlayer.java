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
package com.android.tradefed.testtype.suite.retry;

import com.android.annotations.VisibleForTesting;
import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.TimeUtil;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** Special runner that replays the results given to it. */
public final class ResultsPlayer implements IRemoteTest, IConfigurationReceiver {

    private class ReplayModuleHolder {
        public IInvocationContext mModuleContext;
        public List<Entry<TestDescription, TestResult>> mResults = new ArrayList<>();
    }

    private Map<TestRunResult, ReplayModuleHolder> mModuleResult;
    private IConfiguration mConfiguration;
    private boolean mCompleted;

    /** Ctor. */
    public ResultsPlayer() {
        mModuleResult = new LinkedHashMap<>();
    }

    @VisibleForTesting
    public ResultsPlayer(boolean completed) {
        mCompleted = completed;
    }

    @Override
    public void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        // Very first thing of the retry is to check whether all devices are available, this avoids
        // use wasting time replaying result for an invocation that will fail right after during
        // the re-run.
        for (ITestDevice device : testInfo.getContext().getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            device.waitForDeviceAvailable();
        }

        long startReplay = System.currentTimeMillis();
        CLog.logAndDisplay(
                LogLevel.DEBUG,
                "Start replaying the previous results. Please wait this can take a few minutes.");
        // Change the logging level to avoid too much logs from the replay.
        LogLevel originalLevel = mConfiguration.getLogOutput().getLogLevel();
        mConfiguration.getLogOutput().setLogLevel(LogLevel.WARN);

        for (TestRunResult module : mModuleResult.keySet()) {
            ReplayModuleHolder holder = mModuleResult.get(module);

            IInvocationContext moduleContext = holder.mModuleContext;
            if (moduleContext != null) {
                for (String deviceName : testInfo.getContext().getDeviceConfigNames()) {
                    moduleContext.addAllocatedDevice(
                            deviceName, testInfo.getContext().getDevice(deviceName));
                    moduleContext.addDeviceBuildInfo(
                            deviceName, testInfo.getContext().getBuildInfo(deviceName));
                }
                listener.testModuleStarted(moduleContext);
            }

            // Replay full or partial results
            Collection<Entry<TestDescription, TestResult>> testSet = holder.mResults;
            if (testSet.isEmpty()) {
                testSet = module.getTestResults().entrySet();
            }

            forwardTestResults(module, testSet, listener);

            if (moduleContext != null) {
                listener.testModuleEnded();
            }

            // Clean up the memory: IRemoteTest object are kept in memory until the command finish
            // So we need to clean up the entries when we are done with them to free up the
            // memory early
            holder.mResults.clear();
        }
        // Restore the original log level to continue execution with the requested log level.
        mConfiguration.getLogOutput().setLogLevel(originalLevel);
        CLog.logAndDisplay(
                LogLevel.DEBUG,
                "Done replaying results in %s",
                TimeUtil.formatElapsedTime(System.currentTimeMillis() - startReplay));
        mModuleResult.clear();
        mCompleted = true;
    }

    /**
     * Register a module to be replayed.
     *
     * @param moduleContext The Context of the module. Or null if it's a simple test run.
     * @param module The results of the test run or module.
     * @param testResult The particular test and its result to replay. Can be null if the full
     *     module should be replayed.
     */
    void addToReplay(
            IInvocationContext moduleContext,
            TestRunResult module,
            Entry<TestDescription, TestResult> testResult) {
        ReplayModuleHolder holder = mModuleResult.get(module);
        if (holder == null) {
            holder = new ReplayModuleHolder();
            holder.mModuleContext = moduleContext;
            mModuleResult.put(module, holder);
        }
        if (testResult != null) {
            holder.mResults.add(testResult);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    /** Returns whether or not the ResultsReplayer is done replaying the results. */
    public boolean completed() {
        return mCompleted;
    }

    private void forwardTestResults(
            TestRunResult module,
            Collection<Entry<TestDescription, TestResult>> testSet,
            ITestInvocationListener listener) {
        listener.testRunStarted(module.getName(), testSet.size());
        for (Map.Entry<TestDescription, TestResult> testEntry : testSet) {
            listener.testStarted(testEntry.getKey(), testEntry.getValue().getStartTime());
            switch (testEntry.getValue().getStatus()) {
                case FAILURE:
                    listener.testFailed(testEntry.getKey(), testEntry.getValue().getStackTrace());
                    break;
                case ASSUMPTION_FAILURE:
                    listener.testAssumptionFailure(
                            testEntry.getKey(), testEntry.getValue().getStackTrace());
                    break;
                case IGNORED:
                    listener.testIgnored(testEntry.getKey());
                    break;
                case INCOMPLETE:
                    listener.testFailed(
                            testEntry.getKey(), "Test did not complete due to exception.");
                    break;
                default:
                    break;
            }
            // Provide a strong association of the test to its logs.
            for (Entry<String, LogFile> logFile :
                    testEntry.getValue().getLoggedFiles().entrySet()) {
                if (listener instanceof ILogSaverListener) {
                    ((ILogSaverListener) listener)
                            .logAssociation(logFile.getKey(), logFile.getValue());
                }
            }
            listener.testEnded(
                    testEntry.getKey(),
                    testEntry.getValue().getEndTime(),
                    testEntry.getValue().getProtoMetrics());
        }
        if (module.isRunFailure()) {
            listener.testRunFailed(module.getRunFailureMessage());
        }
        listener.testRunEnded(module.getElapsedTime(), module.getRunProtoMetrics());
    }
}
