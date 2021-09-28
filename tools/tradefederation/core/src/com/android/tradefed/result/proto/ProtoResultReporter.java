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
package com.android.tradefed.result.proto;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.error.HarnessException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.LogFileProto.LogFileInfo;
import com.android.tradefed.result.proto.TestRecordProto.ChildReference;
import com.android.tradefed.result.proto.TestRecordProto.DebugInfo;
import com.android.tradefed.result.proto.TestRecordProto.DebugInfoContext;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.result.proto.TestRecordProto.TestStatus;
import com.android.tradefed.result.retry.ISupportGranularResults;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.SerializationUtil;
import com.android.tradefed.util.StreamUtil;

import com.google.common.base.Strings;
import com.google.protobuf.Any;
import com.google.protobuf.Timestamp;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Stack;
import java.util.UUID;

/**
 * Result reporter build a {@link TestRecord} protobuf with all the results inside. Should be
 * extended to handle what to do with the final proto in {@link #processFinalProto(TestRecord)}.
 */
@OptionClass(alias = "proto-reporter")
public abstract class ProtoResultReporter
        implements ITestInvocationListener, ILogSaverListener, ISupportGranularResults {

    @Option(
        name = "enable-granular-attempts",
        description =
                "Whether or not to allow this reporter receiving granular attempts. Feature flag."
    )
    private boolean mReportGranularResults = true;

    private Stack<TestRecord.Builder> mLatestChild;
    private TestRecord.Builder mInvocationRecordBuilder;
    private long mInvocationStartTime;
    private IInvocationContext mContext;

    private FailureDescription mInvocationFailureDescription = null;
    /** Whether or not a testModuleStart had currently been called. */
    private boolean mModuleInProgress = false;

    @Override
    public boolean supportGranularResults() {
        return mReportGranularResults;
    }

    /**
     * Handling of the partial invocation test record proto after {@link
     * #invocationStarted(IInvocationContext)} occurred.
     *
     * @param invocationStartRecord The partial proto populated after the invocationStart.
     * @param invocationContext The invocation {@link IInvocationContext}.
     */
    public void processStartInvocation(
            TestRecord invocationStartRecord, IInvocationContext invocationContext) {}

    /**
     * Handling of the final proto with all results.
     *
     * @param finalRecord The finalized proto with all the invocation results.
     */
    public void processFinalProto(TestRecord finalRecord) {}

    /**
     * Handling of the partial module record proto after {@link
     * #testModuleStarted(IInvocationContext)} occurred.
     *
     * @param moduleStartRecord The partial proto representing the module.
     */
    public void processTestModuleStarted(TestRecord moduleStartRecord) {}

    /**
     * Handling of the finalized module record proto after {@link #testModuleEnded()} occurred.
     *
     * @param moduleRecord The finalized proto representing the module.
     */
    public void processTestModuleEnd(TestRecord moduleRecord) {}

    /**
     * Handling of the partial test run record proto after {@link #testRunStarted(String, int)}
     * occurred.
     *
     * @param runStartedRecord The partial proto representing the run.
     */
    public void processTestRunStarted(TestRecord runStartedRecord) {}

    /**
     * Handling of the finalized run record proto after {@link #testRunEnded(long, HashMap)}
     * occurred.
     *
     * @param runRecord The finalized proto representing the run.
     * @param moduleInProgress whether or not a module is in progress.
     */
    public void processTestRunEnded(TestRecord runRecord, boolean moduleInProgress) {}

    /**
     * Handling of the partial test case record proto after {@link #testStarted(TestDescription,
     * long)} occurred.
     *
     * @param testCaseStartedRecord The partial proto representing the test case.
     */
    public void processTestCaseStarted(TestRecord testCaseStartedRecord) {}

    /**
     * Handling of the finalized test case record proto after {@link #testEnded(TestDescription,
     * long, HashMap)} occurred.
     *
     * @param testCaseRecord The finalized proto representing a test case.
     */
    public void processTestCaseEnded(TestRecord testCaseRecord) {}

    // Invocation events

    @Override
    public final void invocationStarted(IInvocationContext context) {
        mLatestChild = new Stack<>();
        mInvocationRecordBuilder = TestRecord.newBuilder();
        // Set invocation unique id
        mInvocationRecordBuilder.setTestRecordId(UUID.randomUUID().toString());

        // Populate start time of invocation
        mInvocationStartTime = System.currentTimeMillis();
        Timestamp startTime = createTimeStamp(mInvocationStartTime);
        mInvocationRecordBuilder.setStartTime(startTime);
        mInvocationRecordBuilder.setDescription(Any.pack(context.toProto()));

        mContext = context;

        // Put the invocation record at the bottom of the stack
        mLatestChild.add(mInvocationRecordBuilder);

        // Send the invocation proto with the currently set information to indicate the beginning
        // of the invocation.
        TestRecord startInvocationProto = mInvocationRecordBuilder.build();
        try {
            processStartInvocation(startInvocationProto, context);
        } catch (RuntimeException e) {
            CLog.e("Failed to process invocation started:");
            CLog.e(e);
        }
    }

    @Override
    public void invocationFailed(Throwable cause) {
        // Translate the exception into a FailureDescription
        mInvocationFailureDescription =
                FailureDescription.create(cause.getMessage()).setCause(cause);
        if (cause instanceof HarnessException) {
            mInvocationFailureDescription.setErrorIdentifier(
                    ((HarnessException) cause).getErrorId());
            mInvocationFailureDescription.setOrigin(((HarnessException) cause).getOrigin());
        }
    }

    @Override
    public void invocationFailed(FailureDescription failure) {
        mInvocationFailureDescription = failure;
    }

    @Override
    public final void invocationEnded(long elapsedTime) {
        if (mModuleInProgress) {
            // If we had a module in progress, and a new module start occurs, complete the call
            testModuleEnded();
        }
        // Populate end time of invocation
        Timestamp endTime = createTimeStamp(mInvocationStartTime + elapsedTime);
        mInvocationRecordBuilder.setEndTime(endTime);
        // Update the context in case it changed
        mInvocationRecordBuilder.setDescription(Any.pack(mContext.toProto()));

        DebugInfo invocationFailure = handleInvocationFailure();
        if (invocationFailure != null) {
            mInvocationRecordBuilder.setDebugInfo(invocationFailure);
        }

        // Finalize the protobuf handling: where to put the results.
        TestRecord record = mInvocationRecordBuilder.build();
        try {
            processFinalProto(record);
        } catch (RuntimeException e) {
            CLog.e("Failed to process invocation ended:");
            CLog.e(e);
        }
    }

    // Module events (optional when there is no suite)

    @Override
    public final void testModuleStarted(IInvocationContext moduleContext) {
        if (mModuleInProgress) {
            // If we had a module in progress, and a new module start occurs, complete the call
            testModuleEnded();
        }
        TestRecord.Builder moduleBuilder = TestRecord.newBuilder();
        moduleBuilder.setParentTestRecordId(mInvocationRecordBuilder.getTestRecordId());
        moduleBuilder.setTestRecordId(
                moduleContext.getAttributes().get(ModuleDefinition.MODULE_ID).get(0));
        moduleBuilder.setStartTime(createTimeStamp(System.currentTimeMillis()));
        moduleBuilder.setDescription(Any.pack(moduleContext.toProto()));
        mLatestChild.add(moduleBuilder);
        mModuleInProgress = true;
        try {
            processTestModuleStarted(moduleBuilder.build());
        } catch (RuntimeException e) {
            CLog.e("Failed to process invocation ended:");
            CLog.e(e);
        }
    }

    @Override
    public final void testModuleEnded() {
        TestRecord.Builder moduleBuilder = mLatestChild.pop();
        mModuleInProgress = false;
        moduleBuilder.setEndTime(createTimeStamp(System.currentTimeMillis()));
        TestRecord.Builder parentBuilder = mLatestChild.peek();

        // Finalize the module and track it in the child
        TestRecord moduleRecord = moduleBuilder.build();
        parentBuilder.addChildren(createChildReference(moduleRecord));
        try {
            processTestModuleEnd(moduleRecord);
        } catch (RuntimeException e) {
            CLog.e("Failed to process test module end:");
            CLog.e(e);
        }
    }

    // Run events

    @Override
    public final void testRunStarted(String runName, int testCount) {
        testRunStarted(runName, testCount, 0);
    }

    @Override
    public void testRunStarted(String runName, int testCount, int attemptNumber) {
        testRunStarted(runName, testCount, attemptNumber, System.currentTimeMillis());
    }

    @Override
    public void testRunStarted(String runName, int testCount, int attemptNumber, long startTime) {
        TestRecord.Builder runBuilder = TestRecord.newBuilder();
        TestRecord.Builder parent = mLatestChild.peek();
        runBuilder.setParentTestRecordId(parent.getTestRecordId());
        runBuilder.setTestRecordId(runName);
        runBuilder.setNumExpectedChildren(testCount);
        runBuilder.setStartTime(createTimeStamp(startTime));
        runBuilder.setAttemptId(attemptNumber);

        mLatestChild.add(runBuilder);
        try {
            processTestRunStarted(runBuilder.build());
        } catch (RuntimeException e) {
            CLog.e("Failed to process invocation ended:");
            CLog.e(e);
        }
    }

    @Override
    public final void testRunFailed(String errorMessage) {
        TestRecord.Builder current = mLatestChild.peek();
        DebugInfo.Builder debugBuilder = DebugInfo.newBuilder();
        debugBuilder.setErrorMessage(errorMessage);
        if (TestStatus.UNKNOWN.equals(current.getStatus())) {
            current.setDebugInfo(debugBuilder.build());
        } else {
            // We are in a test case and we need the run parent.
            TestRecord.Builder test = mLatestChild.pop();
            TestRecord.Builder run = mLatestChild.peek();
            run.setDebugInfo(debugBuilder.build());
            // Re-add the test
            mLatestChild.add(test);
        }
    }

    @Override
    public final void testRunFailed(FailureDescription failure) {
        TestRecord.Builder current = mLatestChild.peek();
        DebugInfo.Builder debugBuilder = DebugInfo.newBuilder();
        debugBuilder.setErrorMessage(failure.toString());
        if (failure.getFailureStatus() != null) {
            debugBuilder.setFailureStatus(failure.getFailureStatus());
        }
        DebugInfoContext.Builder debugContext = DebugInfoContext.newBuilder();
        if (failure.getActionInProgress() != null) {
            debugContext.setActionInProgress(failure.getActionInProgress().toString());
        }
        if (!Strings.isNullOrEmpty(failure.getDebugHelpMessage())) {
            debugContext.setDebugHelpMessage(failure.getDebugHelpMessage());
        }
        if (!Strings.isNullOrEmpty(failure.getOrigin())) {
            debugContext.setOrigin(failure.getOrigin());
        }
        if (failure.getErrorIdentifier() != null) {
            debugContext.setErrorName(failure.getErrorIdentifier().name());
            debugContext.setErrorCode(failure.getErrorIdentifier().code());
        }
        debugBuilder.setDebugInfoContext(debugContext.build());

        if (TestStatus.UNKNOWN.equals(current.getStatus())) {
            current.setDebugInfo(debugBuilder.build());
        } else {
            // We are in a test case and we need the run parent.
            TestRecord.Builder test = mLatestChild.pop();
            TestRecord.Builder run = mLatestChild.peek();
            run.setDebugInfo(debugBuilder.build());
            // Re-add the test
            mLatestChild.add(test);
        }
    }

    @Override
    public final void testRunEnded(long elapsedTimeMillis, HashMap<String, Metric> runMetrics) {
        TestRecord.Builder runBuilder = mLatestChild.pop();
        long startTime = timeStampToMillis(runBuilder.getStartTime());
        runBuilder.setEndTime(createTimeStamp(startTime + elapsedTimeMillis));
        runBuilder.putAllMetrics(runMetrics);
        TestRecord.Builder parentBuilder = mLatestChild.peek();

        // Finalize the run and track it in the child
        TestRecord runRecord = runBuilder.build();
        parentBuilder.addChildren(createChildReference(runRecord));
        try {
            processTestRunEnded(runRecord, mModuleInProgress);
        } catch (RuntimeException e) {
            CLog.e("Failed to process test run end:");
            CLog.e(e);
        }
    }

    // test case events

    @Override
    public final void testStarted(TestDescription test) {
        testStarted(test, System.currentTimeMillis());
    }

    @Override
    public final void testStarted(TestDescription test, long startTime) {
        TestRecord.Builder testBuilder = TestRecord.newBuilder();
        TestRecord.Builder parent = mLatestChild.peek();
        testBuilder.setParentTestRecordId(parent.getTestRecordId());
        testBuilder.setTestRecordId(test.toString());
        testBuilder.setStartTime(createTimeStamp(startTime));
        testBuilder.setStatus(TestStatus.PASS);

        mLatestChild.add(testBuilder);
        try {
            processTestCaseStarted(testBuilder.build());
        } catch (RuntimeException e) {
            CLog.e("Failed to process invocation ended:");
            CLog.e(e);
        }
    }

    @Override
    public void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        testEnded(test, System.currentTimeMillis(), testMetrics);
    }

    @Override
    public final void testEnded(
            TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        TestRecord.Builder testBuilder = mLatestChild.pop();
        testBuilder.setEndTime(createTimeStamp(endTime));
        testBuilder.putAllMetrics(testMetrics);
        TestRecord.Builder parentBuilder = mLatestChild.peek();

        // Finalize the run and track it in the child
        TestRecord testCaseRecord = testBuilder.build();
        parentBuilder.addChildren(createChildReference(testCaseRecord));
        try {
            processTestCaseEnded(testCaseRecord);
        } catch (RuntimeException e) {
            CLog.e("Failed to process test case end:");
            CLog.e(e);
        }
    }

    @Override
    public final void testFailed(TestDescription test, String trace) {
        TestRecord.Builder testBuilder = mLatestChild.peek();

        testBuilder.setStatus(TestStatus.FAIL);
        DebugInfo.Builder debugBuilder = DebugInfo.newBuilder();
        // FIXME: extract the error message from the trace
        debugBuilder.setErrorMessage(trace);
        debugBuilder.setTrace(trace);
        testBuilder.setDebugInfo(debugBuilder.build());
    }

    @Override
    public final void testFailed(TestDescription test, FailureDescription failure) {
        TestRecord.Builder testBuilder = mLatestChild.peek();

        testBuilder.setStatus(TestStatus.FAIL);
        DebugInfo.Builder debugBuilder = DebugInfo.newBuilder();
        // FIXME: extract the error message from the trace
        debugBuilder.setErrorMessage(failure.toString());
        debugBuilder.setTrace(failure.toString());
        if (failure.getFailureStatus() != null) {
            debugBuilder.setFailureStatus(failure.getFailureStatus());
        }
        DebugInfoContext.Builder debugContext = DebugInfoContext.newBuilder();
        if (failure.getActionInProgress() != null) {
            debugContext.setActionInProgress(failure.getActionInProgress().toString());
        }
        if (!Strings.isNullOrEmpty(failure.getDebugHelpMessage())) {
            debugContext.setDebugHelpMessage(failure.getDebugHelpMessage());
        }
        debugBuilder.setDebugInfoContext(debugContext.build());

        testBuilder.setDebugInfo(debugBuilder.build());
    }

    @Override
    public final void testIgnored(TestDescription test) {
        TestRecord.Builder testBuilder = mLatestChild.peek();
        testBuilder.setStatus(TestStatus.IGNORED);
    }

    @Override
    public final void testAssumptionFailure(TestDescription test, String trace) {
        TestRecord.Builder testBuilder = mLatestChild.peek();

        testBuilder.setStatus(TestStatus.ASSUMPTION_FAILURE);
        DebugInfo.Builder debugBuilder = DebugInfo.newBuilder();
        // FIXME: extract the error message from the trace
        debugBuilder.setErrorMessage(trace);
        debugBuilder.setTrace(trace);
        testBuilder.setDebugInfo(debugBuilder.build());
    }

    @Override
    public final void testAssumptionFailure(TestDescription test, FailureDescription failure) {
        TestRecord.Builder testBuilder = mLatestChild.peek();

        testBuilder.setStatus(TestStatus.ASSUMPTION_FAILURE);
        DebugInfo.Builder debugBuilder = DebugInfo.newBuilder();
        // FIXME: extract the error message from the trace
        debugBuilder.setErrorMessage(failure.toString());
        debugBuilder.setTrace(failure.toString());
        if (failure.getFailureStatus() != null) {
            debugBuilder.setFailureStatus(failure.getFailureStatus());
        }
        testBuilder.setDebugInfo(debugBuilder.build());
    }

    // log events

    @Override
    public final void logAssociation(String dataName, LogFile logFile) {
        if (mLatestChild == null || mLatestChild.isEmpty()) {
            CLog.w("Skip logging '%s' logAssociation called out of sequence.", dataName);
            return;
        }
        TestRecord.Builder current = mLatestChild.peek();
        Map<String, Any> fullmap = new HashMap<>();
        fullmap.putAll(current.getArtifactsMap());
        Any any = Any.pack(createFileProto(logFile));
        fullmap.put(dataName, any);
        current.putAllArtifacts(fullmap);
    }

    private ChildReference createChildReference(TestRecord record) {
        ChildReference.Builder child = ChildReference.newBuilder();
        child.setTestRecordId(record.getTestRecordId());
        child.setInlineTestRecord(record);
        return child.build();
    }

    /** Create and populate Timestamp as recommended in the javadoc of the Timestamp proto. */
    private Timestamp createTimeStamp(long currentTimeMs) {
        return Timestamp.newBuilder()
                .setSeconds(currentTimeMs / 1000)
                .setNanos((int) ((currentTimeMs % 1000) * 1000000))
                .build();
    }

    private long timeStampToMillis(Timestamp stamp) {
        return stamp.getSeconds() * 1000L + (stamp.getNanos() / 1000000L);
    }

    private LogFileInfo createFileProto(LogFile logFile) {
        LogFileInfo.Builder logFileBuilder = LogFileInfo.newBuilder();
        logFileBuilder
                .setPath(logFile.getPath())
                .setIsText(logFile.isText())
                .setLogType(logFile.getType().toString())
                .setIsCompressed(logFile.isCompressed())
                .setSize(logFile.getSize());
        // Url can be null so avoid NPE by checking it before setting the proto
        if (logFile.getUrl() != null) {
            logFileBuilder.setUrl(logFile.getUrl());
        }
        return logFileBuilder.build();
    }

    private DebugInfo handleInvocationFailure() {
        DebugInfo.Builder debugBuilder = DebugInfo.newBuilder();
        if (mInvocationFailureDescription == null) {
            return null;
        }

        Throwable baseException = mInvocationFailureDescription.getCause();
        if (mInvocationFailureDescription.getErrorMessage() != null) {
            debugBuilder.setErrorMessage(mInvocationFailureDescription.getErrorMessage());
        }
        debugBuilder.setTrace(StreamUtil.getStackTrace(baseException));
        if (mInvocationFailureDescription != null
                && mInvocationFailureDescription.getFailureStatus() != null) {
            debugBuilder.setFailureStatus(mInvocationFailureDescription.getFailureStatus());
        }
        DebugInfoContext.Builder debugContext = DebugInfoContext.newBuilder();
        if (mInvocationFailureDescription != null) {
            if (mInvocationFailureDescription.getActionInProgress() != null) {
                debugContext.setActionInProgress(
                        mInvocationFailureDescription.getActionInProgress().toString());
            }
            if (!Strings.isNullOrEmpty(mInvocationFailureDescription.getDebugHelpMessage())) {
                debugContext.setDebugHelpMessage(
                        mInvocationFailureDescription.getDebugHelpMessage());
            }
            if (!Strings.isNullOrEmpty(mInvocationFailureDescription.getOrigin())) {
                debugContext.setOrigin(mInvocationFailureDescription.getOrigin());
            }
            if (mInvocationFailureDescription.getErrorIdentifier() != null) {
                debugContext.setErrorName(
                        mInvocationFailureDescription.getErrorIdentifier().name());
                debugContext.setErrorCode(
                        mInvocationFailureDescription.getErrorIdentifier().code());
            }
        }
        try {
            debugContext.setErrorType(SerializationUtil.serializeToString(baseException));
        } catch (IOException e) {
            CLog.e("Failed to serialize the invocation failure:");
            CLog.e(e);
        }
        debugBuilder.setDebugInfoContext(debugContext);

        return debugBuilder.build();
    }
}
