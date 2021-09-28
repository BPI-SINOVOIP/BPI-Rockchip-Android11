/*
 * Copyright (C) 2016 The Android Open Source Project
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

import com.android.tradefed.config.Option;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.logger.InvocationMetricLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.retry.ISupportGranularResults;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.SubprocessEventHelper.BaseTestEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.FailedTestEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationFailedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.InvocationStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.LogAssociationEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestLogEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestModuleStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunEndedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunFailedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestRunStartedEventInfo;
import com.android.tradefed.util.SubprocessEventHelper.TestStartedEventInfo;
import com.android.tradefed.util.SubprocessTestResultsParser;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.json.JSONObject;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.HashMap;
import java.util.Map;

/**
 * Implements {@link ITestInvocationListener} to be specified as a result_reporter and forward from
 * the subprocess the results of tests, test runs, test invocations.
 */
public class SubprocessResultsReporter
        implements ITestInvocationListener,
                ILogSaverListener,
                AutoCloseable,
                ISupportGranularResults {

    @Option(
            name = "enable-granular-attempts",
            description = "Whether to allow SubprocessResultsReporter receiving granular attempts.")
    private boolean mGranularAttempts = true;

    @Option(name = "subprocess-report-file", description = "the file where to log the events.")
    private File mReportFile = null;

    @Option(name = "subprocess-report-port", description = "the port where to connect to send the"
            + "events.")
    private Integer mReportPort = null;

    @Option(name = "output-test-log", description = "Option to report test logs to parent process.")
    private boolean mOutputTestlog = false;

    private IInvocationContext mContext = null;
    private Socket mReportSocket = null;
    private Object mLock = new Object();
    private PrintWriter mPrintWriter = null;

    private boolean mPrintWarning = true;
    private boolean mCancelled = false;

    @Override
    public boolean supportGranularResults() {
        return mGranularAttempts;
    }

    /** {@inheritDoc} */
    @Override
    public void testAssumptionFailure(TestDescription testId, String trace) {
        FailedTestEventInfo info =
                new FailedTestEventInfo(testId.getClassName(), testId.getTestName(), trace);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_ASSUMPTION_FAILURE, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testEnded(TestDescription testId, HashMap<String, Metric> metrics) {
        testEnded(testId, System.currentTimeMillis(), metrics);
    }

    /** {@inheritDoc} */
    @Override
    public void testEnded(TestDescription testId, long endTime, HashMap<String, Metric> metrics) {
        // TODO: transfer the proto metrics instead of string metrics
        TestEndedEventInfo info =
                new TestEndedEventInfo(
                        testId.getClassName(),
                        testId.getTestName(),
                        endTime,
                        TfMetricProtoUtil.compatibleConvert(metrics));
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_ENDED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testFailed(TestDescription testId, String reason) {
        FailedTestEventInfo info =
                new FailedTestEventInfo(testId.getClassName(), testId.getTestName(), reason);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_FAILED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testIgnored(TestDescription testId) {
        BaseTestEventInfo info = new BaseTestEventInfo(testId.getClassName(), testId.getTestName());
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_IGNORED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunEnded(long time, HashMap<String, Metric> runMetrics) {
        // TODO: Transfer the full proto instead of just Strings.
        TestRunEndedEventInfo info =
                new TestRunEndedEventInfo(time, TfMetricProtoUtil.compatibleConvert(runMetrics));
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_RUN_ENDED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(String reason) {
        TestRunFailedEventInfo info = new TestRunFailedEventInfo(reason);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_RUN_FAILED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunFailed(FailureDescription failure) {
        TestRunFailedEventInfo info = new TestRunFailedEventInfo(failure);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_RUN_FAILED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStarted(String runName, int testCount) {
        testRunStarted(runName, testCount, 0);
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStarted(String runName, int testCount, int attemptNumber) {
        testRunStarted(runName, testCount, attemptNumber, System.currentTimeMillis());
    }

    /** {@inheritDoc} */
    @Override
    public void testRunStarted(String runName, int testCount, int attemptNumber, long startTime) {
        TestRunStartedEventInfo info =
                new TestRunStartedEventInfo(runName, testCount, attemptNumber, startTime);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_RUN_STARTED, info);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void testRunStopped(long arg0) {
        // ignore
    }

    /** {@inheritDoc} */
    @Override
    public void testStarted(TestDescription testId) {
        testStarted(testId, System.currentTimeMillis());
    }

    /** {@inheritDoc} */
    @Override
    public void testStarted(TestDescription testId, long startTime) {
        TestStartedEventInfo info =
                new TestStartedEventInfo(testId.getClassName(), testId.getTestName(), startTime);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_STARTED, info);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void invocationStarted(IInvocationContext context) {
        InvocationStartedEventInfo info =
                new InvocationStartedEventInfo(context.getTestTag(), System.currentTimeMillis());
        printEvent(SubprocessTestResultsParser.StatusKeys.INVOCATION_STARTED, info);
        // Save off the context so that we can parse it later during invocation ended.
        mContext = context;
    }

    /** {@inheritDoc} */
    @Override
    public void testLog(String dataName, LogDataType dataType, InputStreamSource dataStream) {
        if (!mOutputTestlog || (mReportPort == null && mReportFile == null)) {
            return;
        }
        if (dataStream != null && dataStream.size() != 0) {
            File tmpFile = null;
            try {
                // put 'subprocess' in front to identify the files.
                tmpFile =
                        FileUtil.createTempFile(
                                "subprocess-" + dataName, "." + dataType.getFileExt());
                FileUtil.writeToFile(dataStream.createInputStream(), tmpFile);
                TestLogEventInfo info = new TestLogEventInfo(dataName, dataType, tmpFile);
                printEvent(SubprocessTestResultsParser.StatusKeys.TEST_LOG, info);
            } catch (IOException e) {
                CLog.e(e);
                FileUtil.deleteFile(tmpFile);
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public void logAssociation(String dataName, LogFile logFile) {
        LogAssociationEventInfo info = new LogAssociationEventInfo(dataName, logFile);
        printEvent(SubprocessTestResultsParser.StatusKeys.LOG_ASSOCIATION, info);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void invocationEnded(long elapsedTime) {
        if (mContext == null) {
            return;
        }

        Map<String, String> metrics = mContext.getAttributes().getUniqueMap();
        // All the invocation level metrics collected
        metrics.putAll(InvocationMetricLogger.getInvocationMetrics());
        InvocationEndedEventInfo eventEnd = new InvocationEndedEventInfo(metrics);
        printEvent(SubprocessTestResultsParser.StatusKeys.INVOCATION_ENDED, eventEnd);
        // Upon invocation ended, trigger the end of the socket when the process finishes
        SocketFinisher thread = new SocketFinisher();
        Runtime.getRuntime().addShutdownHook(thread);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void invocationFailed(Throwable cause) {
        InvocationFailedEventInfo info = new InvocationFailedEventInfo(cause);
        printEvent(SubprocessTestResultsParser.StatusKeys.INVOCATION_FAILED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void invocationFailed(FailureDescription failure) {
        InvocationFailedEventInfo info = new InvocationFailedEventInfo(failure);
        printEvent(SubprocessTestResultsParser.StatusKeys.INVOCATION_FAILED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testModuleStarted(IInvocationContext moduleContext) {
        TestModuleStartedEventInfo info = new TestModuleStartedEventInfo(moduleContext);
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_MODULE_STARTED, info);
    }

    /** {@inheritDoc} */
    @Override
    public void testModuleEnded() {
        printEvent(SubprocessTestResultsParser.StatusKeys.TEST_MODULE_ENDED, new JSONObject());
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public TestSummary getSummary() {
        return null;
    }

    /**
     * Helper to print the event key and then the json object.
     */
    public void printEvent(String key, Object event) {
        if (mReportFile != null) {
            if (mReportFile.canWrite()) {
                try {
                    try (FileWriter fw = new FileWriter(mReportFile, true)) {
                        String eventLog = String.format("%s %s\n", key, event.toString());
                        fw.append(eventLog);
                        fw.flush();
                    }
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            } else {
                throw new RuntimeException(
                        String.format("report file: %s is not writable",
                                mReportFile.getAbsolutePath()));
            }
        }
        if(mReportPort != null) {
            try {
                if (mCancelled) {
                    return;
                }
                synchronized (mLock) {
                    if (mReportSocket == null) {
                        mReportSocket = new Socket("localhost", mReportPort.intValue());
                        mPrintWriter = new PrintWriter(mReportSocket.getOutputStream(), true);
                    }
                    if (!mReportSocket.isConnected()) {
                        throw new RuntimeException("Reporter Socket is not connected");
                    }
                    String eventLog = String.format("%s %s\n", key, event.toString());
                    mPrintWriter.print(eventLog);
                    mPrintWriter.flush();
                }
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        if (mReportFile == null && mReportPort == null) {
            if (mPrintWarning) {
                // Only print the warning the first time.
                mPrintWarning = false;
                CLog.w("No report file or socket has been configured, skipping this reporter.");
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public void close() {
        mCancelled = true;
        synchronized (mLock) {
            if (mPrintWriter != null) {
                mPrintWriter.flush();
            }
            StreamUtil.close(mPrintWriter);
            mPrintWriter = null;
            StreamUtil.close(mReportSocket);
            mReportSocket = null;
        }
    }

    /** Sets whether or not we should output the test logged or not. */
    public void setOutputTestLog(boolean outputTestLog) {
        mOutputTestlog = outputTestLog;
    }

    /** Threads that help terminating the socket. */
    private class SocketFinisher extends Thread {

        public SocketFinisher() {
            super();
            setName("SubprocessResultsReporter-socket-finisher");
        }

        @Override
        public void run() {
            close();
        }
    }
}
