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
package com.android.tradefed.result.suite;

import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.error.IHarnessException;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.suite.retry.ResultsPlayer;

/** Reporter that allows to generate reports in a particular format. TODO: fix logged file */
public abstract class FormattedGeneratorReporter extends SuiteResultReporter
        implements IConfigurationReceiver {

    private Throwable mTestHarnessError = null;
    private IConfiguration mConfiguration;

    @Override
    public final void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    public final IConfiguration getConfiguration() {
        return mConfiguration;
    }

    /** {@inheritDoc} */
    @Override
    public final void invocationEnded(long elapsedTime) {
        // Let the parent create the results structures
        super.invocationEnded(elapsedTime);

        // If invocation failed due to a test harness error and did not see any tests
        if (mTestHarnessError != null) {
            Boolean replaySuccess = null;
            for (IRemoteTest test : mConfiguration.getTests()) {
                if (test instanceof ResultsPlayer) {
                    replaySuccess = ((ResultsPlayer) test).completed();
                }
            }
            if (replaySuccess != null && !replaySuccess) {
                CLog.e(
                        "Invocation failed and previous session results couldn't be copied, skip "
                                + "generating the formatted report due to:");
                CLog.e(mTestHarnessError);
                return;
            }
        }

        SuiteResultHolder holder = generateResultHolder();
        IFormatterGenerator generator = createFormatter();
        finalizeResults(generator, holder);
    }

    @Override
    public void invocationFailed(Throwable cause) {
        FailureDescription description =
                FailureDescription.create(cause.getMessage()).setCause(cause);
        if (cause instanceof IHarnessException) {
            description.setErrorIdentifier(((IHarnessException) cause).getErrorId());
        }
        invocationFailed(description);
    }

    @Override
    public void invocationFailed(FailureDescription failure) {
        // Some exception indicate a harness level issue, the tests result cannot be trusted at
        // that point so we should skip the reporting.
        Throwable cause = failure.getCause();
        if (cause != null
                && (cause instanceof TargetSetupError
                        || cause instanceof RuntimeException
                        || cause instanceof OutOfMemoryError)) {
            mTestHarnessError = cause;
        }
        super.invocationFailed(cause);
    }

    /**
     * Step that handles using the {@link IFormatterGenerator} and the {@link SuiteResultHolder} in
     * order to generate some formatted results.
     *
     * @param generator
     * @param resultHolder
     */
    public abstract void finalizeResults(
            IFormatterGenerator generator, SuiteResultHolder resultHolder);

    /** Returns a new instance of the {@link IFormatterGenerator} to use. */
    public abstract IFormatterGenerator createFormatter();

    private SuiteResultHolder generateResultHolder() {
        final SuiteResultHolder holder = new SuiteResultHolder();
        holder.context = getInvocationContext();

        holder.runResults = getMergedTestRunResults();
        //holder.loggedFiles = mLoggedFiles;
        holder.modulesAbi = getModulesAbi();

        holder.completeModules = getCompleteModules();
        holder.totalModules = getTotalModules();
        holder.passedTests = getPassedTests();
        holder.failedTests = getFailedTests();

        holder.startTime = getStartTime();
        holder.endTime = getEndTime();

        return holder;
    }
}
