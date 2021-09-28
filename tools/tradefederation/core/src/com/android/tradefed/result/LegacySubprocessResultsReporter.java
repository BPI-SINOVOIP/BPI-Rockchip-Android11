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
package com.android.tradefed.result;

import com.android.ddmlib.testrunner.TestIdentifier;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.util.SubprocessEventHelper.BaseTestEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.FailedTestEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationFailedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunFailedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestModuleStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.LogAssociationEventInfo;
import com.android.tradefed.util.SubprocessTestResultsParser;

import org.json.JSONObject;

import java.io.Serializable;
import java.util.Map;

/**
 * A frozen implementation of the subprocess results reporter which should remain compatible with
 * earlier versions of TF/CTS (e.g. 8+), despite changes in its superclass.
 */
public final class LegacySubprocessResultsReporter extends SubprocessResultsReporter {

    /* Legacy method compatible with TF/CTS 8+. */
    public void testAssumptionFailure(TestIdentifier testId, String trace) {
        FailedTestEventInfo info =
                new FailedTestEventInfo(testId.getClassName(), testId.getTestName(), trace);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_ASSUMPTION_FAILURE, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    public void testEnded(TestIdentifier testId, Map<String, String> metrics) {
        testEnded(testId, System.currentTimeMillis(), metrics);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    public void testEnded(TestIdentifier testId, long endTime, Map<String, String> metrics) {
        TestEndedEventInfo info =
                new TestEndedEventInfo(
                        testId.getClassName(), testId.getTestName(), endTime, metrics);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_ENDED, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    public void testFailed(TestIdentifier testId, String reason) {
        FailedTestEventInfo info =
                new FailedTestEventInfo(testId.getClassName(), testId.getTestName(), reason);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_FAILED, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    public void testIgnored(TestIdentifier testId) {
        BaseTestEventInfo info = new BaseTestEventInfo(testId.getClassName(), testId.getTestName());
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_IGNORED, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    public void testStarted(TestIdentifier testId) {
        testStarted(testId, System.currentTimeMillis());
    }

    /* Legacy method compatible with TF/CTS 8+. */
    public void testStarted(TestIdentifier testId, long startTime) {
        TestStartedEventInfo info =
                new TestStartedEventInfo(testId.getClassName(), testId.getTestName(), startTime);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_STARTED, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    public void invocationStarted(IBuildInfo buildInfo) {
        InvocationStartedEventInfo info =
                new InvocationStartedEventInfo(buildInfo.getTestTag(), System.currentTimeMillis());
        printEvent(SubprocessTestResultsParser.StatusKeys.INVOCATION_STARTED, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    @Override
    public void invocationFailed(Throwable cause) {
        InvocationFailedEventInfo info = new InvocationFailedEventInfo(cause);
        printEvent(SubprocessTestResultsParser.StatusKeys.INVOCATION_FAILED, info);
    }

    /* Legacy method compatible with TF/CTS 8. */
    @Override
    public void invocationEnded(long elapsedTime) {
        // ignore
    }

    /* Legacy method compatible with TF/CTS 8+. */
    @Override
    public void testRunFailed(String reason) {
        TestRunFailedEventInfo info = new TestRunFailedEventInfo(reason);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_RUN_FAILED, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    @Override
    public void testRunStarted(String runName, int testCount) {
        TestRunStartedEventInfo info = new TestRunStartedEventInfo(runName, testCount);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_RUN_STARTED, info);
    }

    /* Legacy method compatible with TF/CTS 8+. */
    @Override
    public void testRunEnded(long time, Map<String, String> runMetrics) {
        TestRunEndedEventInfo info = new TestRunEndedEventInfo(time, runMetrics);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_RUN_ENDED, info);
    }

    /* Legacy method compatible with TF/CTS 9+ (skipped in 8.1 and not called in 8). */
    @Override
    public void testModuleStarted(IInvocationContext moduleContext) {
        if (!Serializable.class.isAssignableFrom(IAbi.class)) {
            // TODO(b/154349022): remove after releasing serialization fix
            // Test packages prior to 1d6869f will fail with a not serializable exception.
            // see https://cs.android.com/android/_/android/platform/tools/tradefederation/+/1d6869f
            CLog.d("testModuleStarted is called but ignored intentionally");
            return;
        }
        TestModuleStartedEventInfo info = new TestModuleStartedEventInfo(moduleContext);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_MODULE_STARTED, info);
    }

    /* Legacy method compatible with TF/CTS 9+ (skipped in 8.1 and not called in 8). */
    @Override
    public void testModuleEnded() {
        if (!Serializable.class.isAssignableFrom(IAbi.class)) {
            // TODO(b/154349022): remove after releasing serialization fix
            // Test packages prior to 1d6869f will fail with a not serializable exception.
            // see https://cs.android.com/android/_/android/platform/tools/tradefederation/+/1d6869f
            CLog.d("testModuleEnded is called but ignored intentionally");
            return;
        }
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_MODULE_ENDED, new JSONObject());
    }

    /* Legacy method compatible with TF/CTS 8.1+ (not called in 8). */
    @Override
    public void testLogSaved(
            String dataName, LogDataType dataType, InputStreamSource dataStream, LogFile logFile) {
        // ignore
    }

    /* Legacy method compatible with TF/CTS 9+ (skipped in 8.1 and not called in 8). */
    @Override
    public void logAssociation(String dataName, LogFile logFile) {
        if (!Serializable.class.isAssignableFrom(LogFile.class)) {
            // TODO(b/154349022): remove after releasing serialization fix
            // Test packages prior to 52fb8df will fail with a not serializable exception.
            // see https://cs.android.com/android/_/android/platform/tools/tradefederation/+/52fb8df
            CLog.d("logAssociation is called but ignored intentionally");
            return;
        }
        LogAssociationEventInfo info = new LogAssociationEventInfo(dataName, logFile);
        printEvent(SubprocessTestResultsParser.StatusKeys.LOG_ASSOCIATION, info);
    }

    /* Legacy method compatible with TF/CTS 8.1+ (not called in 8). */
    @Override
    public void setLogSaver(ILogSaver logSaver) {
        // ignore
    }
}
