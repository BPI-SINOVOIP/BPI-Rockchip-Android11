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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ActionInProgress;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.util.HashMap;

/** Unit tests for {@link ProtoResultParser}. */
@RunWith(JUnit4.class)
public class ProtoResultParserTest {

    private static final String CONTEXT_TEST_KEY = "context-late-attribute";
    private static final String TEST_KEY = "late-attribute";

    private ProtoResultParser mParser;
    private ILogSaverListener mMockListener;
    private TestProtoParser mTestParser;
    private FinalTestProtoParser mFinalTestParser;
    private IInvocationContext mInvocationContext;
    private IInvocationContext mMainInvocationContext;

    private class TestProtoParser extends ProtoResultReporter {

        @Override
        public void processStartInvocation(
                TestRecord invocationStartRecord, IInvocationContext context) {
            mParser.processNewProto(invocationStartRecord);
            // After context was proto-ified once add an attribute
            context.getBuildInfos().get(0).addBuildAttribute(TEST_KEY, "build_value");
            // Test a context attribute
            context.addInvocationAttribute(CONTEXT_TEST_KEY, "context_value");
        }

        @Override
        public void processFinalProto(TestRecord finalRecord) {
            mParser.processNewProto(finalRecord);
        }

        @Override
        public void processTestModuleStarted(TestRecord moduleStartRecord) {
            mParser.processNewProto(moduleStartRecord);
        }

        @Override
        public void processTestModuleEnd(TestRecord moduleRecord) {
            mParser.processNewProto(moduleRecord);
        }

        @Override
        public void processTestRunStarted(TestRecord runStartedRecord) {
            mParser.processNewProto(runStartedRecord);
        }

        @Override
        public void processTestRunEnded(TestRecord runRecord, boolean moduleInProgress) {
            mParser.processNewProto(runRecord);
        }

        @Override
        public void processTestCaseStarted(TestRecord testCaseStartedRecord) {
            mParser.processNewProto(testCaseStartedRecord);
        }

        @Override
        public void processTestCaseEnded(TestRecord testCaseRecord) {
            mParser.processNewProto(testCaseRecord);
        }
    }

    private class FinalTestProtoParser extends ProtoResultReporter {
        @Override
        public void processFinalProto(TestRecord finalRecord) {
            mParser.processFinalizedProto(finalRecord);
        }
    }

    @Before
    public void setUp() {
        mMockListener = EasyMock.createStrictMock(ILogSaverListener.class);
        mMainInvocationContext = new InvocationContext();
        mParser = new ProtoResultParser(mMockListener, mMainInvocationContext, true);
        mTestParser = new TestProtoParser();
        mFinalTestParser = new FinalTestProtoParser();
        mInvocationContext = new InvocationContext();
        mInvocationContext.setConfigurationDescriptor(new ConfigurationDescriptor());
        BuildInfo info = new BuildInfo();
        mInvocationContext.addAllocatedDevice(
                ConfigurationDef.DEFAULT_DEVICE_NAME, EasyMock.createMock(ITestDevice.class));
        mMainInvocationContext.addAllocatedDevice(
                ConfigurationDef.DEFAULT_DEVICE_NAME, EasyMock.createMock(ITestDevice.class));
        mInvocationContext.addDeviceBuildInfo(ConfigurationDef.DEFAULT_DEVICE_NAME, info);
        mMainInvocationContext.addDeviceBuildInfo(
                ConfigurationDef.DEFAULT_DEVICE_NAME, new BuildInfo());
    }

    @Test
    public void testEvents() {
        TestDescription test1 = new TestDescription("class1", "test1");
        TestDescription test2 = new TestDescription("class1", "test2");
        HashMap<String, Metric> metrics = new HashMap<String, Metric>();
        metrics.put("metric1", TfMetricProtoUtil.stringToMetric("value1"));
        LogFile logFile = new LogFile("path", "url", false, LogDataType.TEXT, 5);
        Throwable failure = new RuntimeException("invoc failure");
        Capture<LogFile> capture = new Capture<>();

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());

        mMockListener.testModuleStarted(EasyMock.anyObject());
        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testStarted(test2, 11L);
        mMockListener.testFailed(test2, FailureDescription.create("I failed"));
        mMockListener.logAssociation(EasyMock.eq("subprocess-log1"), EasyMock.capture(capture));
        mMockListener.testEnded(test2, 60L, metrics);
        mMockListener.logAssociation(EasyMock.eq("subprocess-run_log1"), EasyMock.anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.logAssociation(EasyMock.eq("subprocess-module_log1"), EasyMock.anyObject());
        mMockListener.testModuleEnded();
        mMockListener.logAssociation(
                EasyMock.eq("subprocess-invocation_log1"), EasyMock.anyObject());
        // Invocation failure is replayed
        Capture<FailureDescription> captureInvocFailure = new Capture<>();
        mMockListener.invocationFailed(EasyMock.capture(captureInvocFailure));
        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        // Invocation start
        mInvocationContext.getBuildInfos().get(0).addBuildAttribute("early_key", "build_value");
        mInvocationContext.addInvocationAttribute("early_context_key", "context_value");
        mTestParser.invocationStarted(mInvocationContext);
        mInvocationContext.getBuildInfos().get(0).addBuildAttribute("after_start", "build_value");
        // Check early context
        IInvocationContext context = mMainInvocationContext;
        assertFalse(context.getBuildInfos().get(0).getBuildAttributes().containsKey(TEST_KEY));
        assertFalse(context.getAttributes().containsKey(CONTEXT_TEST_KEY));
        assertEquals(
                "build_value",
                context.getBuildInfos().get(0).getBuildAttributes().get("early_key"));
        assertEquals("context_value", context.getAttributes().get("early_context_key").get(0));

        assertFalse(context.getBuildInfos().get(0).getBuildAttributes().containsKey("after_start"));
        // Run modules
        mTestParser.testModuleStarted(createModuleContext("arm64 module1"));
        assertEquals(
                "build_value",
                context.getBuildInfos().get(0).getBuildAttributes().get("after_start"));
        mTestParser.testRunStarted("run1", 2);

        mTestParser.testStarted(test1, 5L);
        mTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());

        mTestParser.testStarted(test2, 11L);
        mTestParser.testFailed(test2, "I failed");
        // test log
        mTestParser.logAssociation("log1", logFile);

        mTestParser.testEnded(test2, 60L, metrics);

        mTestParser.invocationFailed(failure);
        // run log
        mTestParser.logAssociation(
                "run_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        mTestParser.testRunEnded(50L, new HashMap<String, Metric>());

        mTestParser.logAssociation(
                "module_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        mTestParser.testModuleEnded();

        mTestParser.logAssociation(
                "invocation_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        // Invocation ends
        mTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);

        // Check capture
        LogFile capturedFile = capture.getValue();
        assertEquals(logFile.getPath(), capturedFile.getPath());
        assertEquals(logFile.getUrl(), capturedFile.getUrl());
        assertEquals(logFile.getType(), capturedFile.getType());
        assertEquals(logFile.getSize(), capturedFile.getSize());

        FailureDescription invocFailureCaptured = captureInvocFailure.getValue();
        assertTrue(invocFailureCaptured.getCause() instanceof RuntimeException);

        // Check Context at the end
        assertEquals(
                "build_value", context.getBuildInfos().get(0).getBuildAttributes().get(TEST_KEY));
        assertEquals("context_value", context.getAttributes().get(CONTEXT_TEST_KEY).get(0));
        assertEquals(
                "build_value",
                context.getBuildInfos().get(0).getBuildAttributes().get("early_key"));
        assertEquals("context_value", context.getAttributes().get("early_context_key").get(0));
        assertEquals(
                "build_value",
                context.getBuildInfos().get(0).getBuildAttributes().get("after_start"));
    }

    @Test
    public void testEvents_invocationFailure() {
        Throwable failure = new RuntimeException("invoc failure");
        FailureDescription invocFailure =
                FailureDescription.create(failure.getMessage())
                        .setCause(failure)
                        .setActionInProgress(ActionInProgress.FETCHING_ARTIFACTS)
                        .setErrorIdentifier(InfraErrorIdentifier.UNDETERMINED)
                        .setOrigin("origin");

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());
        // Invocation failure is replayed
        Capture<FailureDescription> captureInvocFailure = new Capture<>();
        mMockListener.invocationFailed(EasyMock.capture(captureInvocFailure));
        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        // Invocation start
        mInvocationContext.getBuildInfos().get(0).addBuildAttribute("early_key", "build_value");
        mInvocationContext.addInvocationAttribute("early_context_key", "context_value");
        mTestParser.invocationStarted(mInvocationContext);
        mTestParser.invocationFailed(invocFailure);
        // Invocation ends
        mTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);

        // Check capture
        FailureDescription invocFailureCaptured = captureInvocFailure.getValue();
        assertTrue(invocFailureCaptured.getCause() instanceof RuntimeException);
        assertEquals(
                ActionInProgress.FETCHING_ARTIFACTS, invocFailureCaptured.getActionInProgress());
        assertEquals(
                InfraErrorIdentifier.UNDETERMINED.name(),
                invocFailureCaptured.getErrorIdentifier().name());
        assertEquals(
                InfraErrorIdentifier.UNDETERMINED.code(),
                invocFailureCaptured.getErrorIdentifier().code());
        assertEquals("origin", invocFailureCaptured.getOrigin());
    }

    @Test
    public void testEvents_invocationFailure_errorNotSet() {
        Throwable failure = new RuntimeException("invoc failure");
        // Error with not ErrorIdentifier set.
        FailureDescription invocFailure =
                FailureDescription.create(failure.getMessage())
                        .setCause(failure)
                        .setActionInProgress(ActionInProgress.FETCHING_ARTIFACTS)
                        .setOrigin("origin");

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());
        // Invocation failure is replayed
        Capture<FailureDescription> captureInvocFailure = new Capture<>();
        mMockListener.invocationFailed(EasyMock.capture(captureInvocFailure));
        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        // Invocation start
        mInvocationContext.getBuildInfos().get(0).addBuildAttribute("early_key", "build_value");
        mInvocationContext.addInvocationAttribute("early_context_key", "context_value");
        mTestParser.invocationStarted(mInvocationContext);
        mTestParser.invocationFailed(invocFailure);
        // Invocation ends
        mTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);

        // Check capture
        FailureDescription invocFailureCaptured = captureInvocFailure.getValue();
        assertTrue(invocFailureCaptured.getCause() instanceof RuntimeException);
        assertEquals(
                ActionInProgress.FETCHING_ARTIFACTS, invocFailureCaptured.getActionInProgress());
        assertEquals("origin", invocFailureCaptured.getOrigin());
        assertNull(invocFailureCaptured.getErrorIdentifier());
    }

    /** Test that a run failure occurring inside a test case pair is handled properly. */
    @Test
    public void testRunFail_interleavedWithTest() {
        TestDescription test1 = new TestDescription("class1", "test1");

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());

        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testRunFailed(FailureDescription.create("run failure"));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        mTestParser.invocationStarted(mInvocationContext);
        // Run modules
        mTestParser.testRunStarted("run1", 2);

        mTestParser.testStarted(test1, 5L);
        // test run failed inside a test
        mTestParser.testRunFailed("run failure");

        mTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());

        mTestParser.testRunEnded(50L, new HashMap<String, Metric>());
        // Invocation ends
        mTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);
    }

    @Test
    public void testEvents_finaleProto() {
        TestDescription test1 = new TestDescription("class1", "test1");
        TestDescription test2 = new TestDescription("class1", "test2");
        HashMap<String, Metric> metrics = new HashMap<String, Metric>();
        metrics.put("metric1", TfMetricProtoUtil.stringToMetric("value1"));
        LogFile logFile = new LogFile("path", "url", false, LogDataType.TEXT, 5);
        LogFile logModuleFile = new LogFile("path", "url", false, LogDataType.TEXT, 5);

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());

        mMockListener.testModuleStarted(EasyMock.anyObject());
        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testStarted(test2, 11L);
        mMockListener.testFailed(test2, FailureDescription.create("I failed"));
        mMockListener.logAssociation(EasyMock.eq("subprocess-log1"), EasyMock.anyObject());
        mMockListener.testEnded(test2, 60L, metrics);
        mMockListener.logAssociation(EasyMock.eq("subprocess-run_log1"), EasyMock.anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.logAssociation(EasyMock.eq("subprocess-log-module"), EasyMock.anyObject());
        mMockListener.testModuleEnded();

        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        // Invocation start
        mFinalTestParser.invocationStarted(mInvocationContext);
        // Run modules
        mFinalTestParser.testModuleStarted(createModuleContext("arm64 module1"));
        // test log at module level
        mFinalTestParser.logAssociation("log-module", logModuleFile);
        mFinalTestParser.testRunStarted("run1", 2);

        mFinalTestParser.testStarted(test1, 5L);
        mFinalTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());

        mFinalTestParser.testStarted(test2, 11L);
        mFinalTestParser.testFailed(test2, "I failed");
        // test log
        mFinalTestParser.logAssociation("log1", logFile);

        mFinalTestParser.testEnded(test2, 60L, metrics);
        // run log
        mFinalTestParser.logAssociation(
                "run_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        mFinalTestParser.testRunEnded(50L, new HashMap<String, Metric>());

        mFinalTestParser.testModuleEnded();

        // Invocation ends
        mFinalTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);
    }

    /** Test that a run failure occurring inside a test case pair is handled properly. */
    @Test
    public void testRunFail_interleavedWithTest_finalProto() {
        TestDescription test1 = new TestDescription("class1", "test1");

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());

        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testRunFailed(FailureDescription.create("run failure"));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        mFinalTestParser.invocationStarted(mInvocationContext);
        // Run modules
        mFinalTestParser.testRunStarted("run1", 2);

        mFinalTestParser.testStarted(test1, 5L);
        // test run failed inside a test
        mFinalTestParser.testRunFailed("run failure");

        mFinalTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());

        mFinalTestParser.testRunEnded(50L, new HashMap<String, Metric>());
        // Invocation ends
        mFinalTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);
    }

    /** Ensure that the FailureDescription is properly populated. */
    @Test
    public void testRunFail_failureDescription() {
        TestDescription test1 = new TestDescription("class1", "test1");

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());

        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testRunFailed(
                FailureDescription.create("run failure")
                        .setFailureStatus(FailureStatus.INFRA_FAILURE)
                        .setActionInProgress(ActionInProgress.TEST)
                        .setDebugHelpMessage("help message"));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        mFinalTestParser.invocationStarted(mInvocationContext);
        // Run modules
        mFinalTestParser.testRunStarted("run1", 2);

        mFinalTestParser.testStarted(test1, 5L);
        // test run failed inside a test
        mFinalTestParser.testRunFailed(
                FailureDescription.create("run failure")
                        .setFailureStatus(FailureStatus.INFRA_FAILURE)
                        .setActionInProgress(ActionInProgress.TEST)
                        .setDebugHelpMessage("help message"));

        mFinalTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());

        mFinalTestParser.testRunEnded(50L, new HashMap<String, Metric>());
        // Invocation ends
        mFinalTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);
    }

    /**
     * Ensure the testRunStart specified with an attempt number is carried through our proto test
     * record.
     */
    @Test
    public void testRun_withAttempts() {
        TestDescription test1 = new TestDescription("class1", "test1");

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());

        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testRunFailed(FailureDescription.create("run failure"));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(1), EasyMock.eq(1), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        mTestParser.invocationStarted(mInvocationContext);
        // Run modules
        mTestParser.testRunStarted("run1", 2);
        mTestParser.testStarted(test1, 5L);
        // test run failed inside a test
        mTestParser.testRunFailed("run failure");
        mTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());
        mTestParser.testRunEnded(50L, new HashMap<String, Metric>());

        mTestParser.testRunStarted("run1", 1, 1);
        mTestParser.testStarted(test1, 5L);
        mTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());
        mTestParser.testRunEnded(50L, new HashMap<String, Metric>());
        // Invocation ends
        mTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);
    }

    /**
     * Test a subprocess situation where invocation start/end is not called again in the parent
     * process.
     */
    @Test
    public void testEvents_subprocess() throws Exception {
        // In subprocess, we do not report invocationStart/end again
        mParser = new ProtoResultParser(mMockListener, mMainInvocationContext, false);

        TestDescription test1 = new TestDescription("class1", "test1");
        TestDescription test2 = new TestDescription("class1", "test2");
        HashMap<String, Metric> metrics = new HashMap<String, Metric>();
        metrics.put("metric1", TfMetricProtoUtil.stringToMetric("value1"));
        LogFile logFile = new LogFile("path", "url", false, LogDataType.TEXT, 5);
        Capture<LogFile> capture = new Capture<>();

        // Verify Mocks - No Invocation Start and End
        mMockListener.testModuleStarted(EasyMock.anyObject());
        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testStarted(test2, 11L);
        mMockListener.testFailed(test2, FailureDescription.create("I failed"));
        mMockListener.logAssociation(EasyMock.eq("subprocess-log1"), EasyMock.capture(capture));
        mMockListener.testEnded(test2, 60L, metrics);
        mMockListener.logAssociation(EasyMock.eq("subprocess-run_log1"), EasyMock.anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.logAssociation(EasyMock.eq("subprocess-module_log1"), EasyMock.anyObject());
        mMockListener.testModuleEnded();
        mMockListener.logAssociation(
                EasyMock.eq("subprocess-invocation_log1"), EasyMock.anyObject());
        mMockListener.testLog(
                EasyMock.eq("subprocess-host_log"),
                EasyMock.eq(LogDataType.TEXT),
                EasyMock.anyObject());
        mMockListener.testLog(
                EasyMock.eq("subprocess-host_log_zip"),
                EasyMock.eq(LogDataType.ZIP),
                EasyMock.anyObject());

        EasyMock.replay(mMockListener);
        // Invocation start
        mTestParser.invocationStarted(mInvocationContext);
        // Run modules
        mTestParser.testModuleStarted(createModuleContext("arm64 module1"));
        mTestParser.testRunStarted("run1", 2);

        mTestParser.testStarted(test1, 5L);
        mTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());

        mTestParser.testStarted(test2, 11L);
        mTestParser.testFailed(test2, "I failed");
        // test log
        mTestParser.logAssociation("log1", logFile);

        mTestParser.testEnded(test2, 60L, metrics);
        // run log
        mTestParser.logAssociation(
                "run_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        mTestParser.testRunEnded(50L, new HashMap<String, Metric>());

        mTestParser.logAssociation(
                "module_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        mTestParser.testModuleEnded();

        mTestParser.logAssociation(
                "invocation_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        File tmpLogFile = FileUtil.createTempFile("host_log", ".txt");
        File tmpZipLogFile = FileUtil.createTempFile("zip_host_log", ".zip");
        try {
            mTestParser.logAssociation(
                    "host_log",
                    new LogFile(tmpLogFile.getAbsolutePath(), "", false, LogDataType.TEXT, 5));
            mTestParser.logAssociation(
                    "host_log_zip",
                    new LogFile(tmpZipLogFile.getAbsolutePath(), "", false, LogDataType.TEXT, 5));
            // Invocation ends
            mTestParser.invocationEnded(500L);
            EasyMock.verify(mMockListener);

            // Check capture
            LogFile capturedFile = capture.getValue();
            assertEquals(logFile.getPath(), capturedFile.getPath());
            assertEquals(logFile.getUrl(), capturedFile.getUrl());
            assertEquals(logFile.getType(), capturedFile.getType());
            assertEquals(logFile.getSize(), capturedFile.getSize());

            // Check Context
            IInvocationContext context = mMainInvocationContext;
            assertEquals(
                    "build_value",
                    context.getBuildInfos().get(0).getBuildAttributes().get(TEST_KEY));
            assertEquals("context_value", context.getAttributes().get(CONTEXT_TEST_KEY).get(0));
        } finally {
            FileUtil.deleteFile(tmpLogFile);
            FileUtil.deleteFile(tmpZipLogFile);
        }
    }

    /** Test the parsing when a missing testModuleEnded occurs. */
    @Test
    public void testEvents_finaleProto_partialEvents() {
        TestDescription test1 = new TestDescription("class1", "test1");
        TestDescription test2 = new TestDescription("class1", "test2");
        HashMap<String, Metric> metrics = new HashMap<String, Metric>();
        metrics.put("metric1", TfMetricProtoUtil.stringToMetric("value1"));
        LogFile logFile = new LogFile("path", "url", false, LogDataType.TEXT, 5);
        LogFile logModuleFile = new LogFile("path", "url", false, LogDataType.TEXT, 5);

        // Verify Mocks
        mMockListener.invocationStarted(EasyMock.anyObject());

        mMockListener.testModuleStarted(EasyMock.anyObject());
        mMockListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(test1, 5L);
        mMockListener.testEnded(test1, 10L, new HashMap<String, Metric>());

        mMockListener.testStarted(test2, 11L);
        mMockListener.testFailed(test2, FailureDescription.create("I failed"));
        mMockListener.logAssociation(EasyMock.eq("subprocess-log1"), EasyMock.anyObject());
        mMockListener.testEnded(test2, 60L, metrics);
        mMockListener.logAssociation(EasyMock.eq("subprocess-run_log1"), EasyMock.anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.logAssociation(EasyMock.eq("subprocess-log-module"), EasyMock.anyObject());
        // We complete the missing event
        mMockListener.testModuleEnded();

        mMockListener.invocationEnded(500L);

        EasyMock.replay(mMockListener);
        // Invocation start
        mFinalTestParser.invocationStarted(mInvocationContext);
        // Run modules
        mFinalTestParser.testModuleStarted(createModuleContext("arm64 module1"));
        // test log at module level
        mFinalTestParser.logAssociation("log-module", logModuleFile);
        mFinalTestParser.testRunStarted("run1", 2);

        mFinalTestParser.testStarted(test1, 5L);
        mFinalTestParser.testEnded(test1, 10L, new HashMap<String, Metric>());

        mFinalTestParser.testStarted(test2, 11L);
        mFinalTestParser.testFailed(test2, "I failed");
        // test log
        mFinalTestParser.logAssociation("log1", logFile);

        mFinalTestParser.testEnded(test2, 60L, metrics);
        // run log
        mFinalTestParser.logAssociation(
                "run_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        mFinalTestParser.testRunEnded(50L, new HashMap<String, Metric>());

        // Missing testModuleEnded due to a timeout for example
        //mFinalTestParser.testModuleEnded();

        // Invocation ends
        mFinalTestParser.invocationEnded(500L);
        EasyMock.verify(mMockListener);
    }

    /** Helper to create a module context. */
    private IInvocationContext createModuleContext(String moduleId) {
        IInvocationContext context = new InvocationContext();
        context.addInvocationAttribute(ModuleDefinition.MODULE_ID, moduleId);
        context.setConfigurationDescriptor(new ConfigurationDescriptor());
        context.addDeviceBuildInfo(
                ConfigurationDef.DEFAULT_DEVICE_NAME, mInvocationContext.getBuildInfos().get(0));
        return context;
    }
}
