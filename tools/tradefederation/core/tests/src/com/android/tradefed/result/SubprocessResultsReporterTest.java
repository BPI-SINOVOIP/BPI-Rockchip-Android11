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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.SubprocessTestResultsParser;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Unit Tests for {@link SubprocessResultsReporter}
 */
@RunWith(JUnit4.class)
public class SubprocessResultsReporterTest {

    private static final long LONG_TIMEOUT_MS = 20000L;
    private SubprocessResultsReporter mReporter;

    @Before
    public void setUp() {
        mReporter = new SubprocessResultsReporter();
    }

    /**
     * Test that when none of the option for reporting is set, nothing happen.
     */
    @Test
    public void testPrintEvent_Inop() {
        TestDescription testId = new TestDescription("com.fakeclass", "faketest");
        mReporter.testStarted(testId);
        mReporter.testFailed(testId, "fake failure");
        mReporter.testEnded(testId, new HashMap<String, Metric>());
        mReporter.printEvent(null, null);
    }

    /**
     * Test that when a report file is specified it logs event to it.
     */
    @Test
    public void testPrintEvent_printToFile() throws Exception {
        OptionSetter setter = new OptionSetter(mReporter);
        File tmpReportFile = FileUtil.createTempFile("subprocess-reporter", "unittest");
        try {
            setter.setOptionValue("subprocess-report-file", tmpReportFile.getAbsolutePath());
            mReporter.testRunStarted("TEST", 5, 1, 500);
            mReporter.testRunEnded(100, new HashMap<String, Metric>());
            String content = FileUtil.readStringFromFile(tmpReportFile);
            assertTrue(content.contains("TEST_RUN_STARTED"));
            assertTrue(content.contains("\"testCount\":5"));
            assertTrue(content.contains("\"runName\":\"TEST\""));
            assertTrue(content.contains("\"start_time\":500"));
            assertTrue(content.contains("\"runAttempt\":1"));
            assertTrue(content.contains("TEST_RUN_ENDED {\"time\":100}\n"));
        } finally {
            FileUtil.deleteFile(tmpReportFile);
        }
    }

    /**
     * Test that when the specified report file is not writable we throw an exception.
     */
    @Test
    public void testPrintEvent_nonWritableFile() throws Exception {
        OptionSetter setter = new OptionSetter(mReporter);
        File tmpReportFile = FileUtil.createTempFile("subprocess-reporter", "unittest");
        try {
            // Delete the file to make it non-writable. (do not use setWritable as a root tradefed
            // process would still be able to write it)
            tmpReportFile.delete();
            setter.setOptionValue("subprocess-report-file", tmpReportFile.getAbsolutePath());
            mReporter.testRunStarted("TEST", 5);
            fail("Should have thrown an exception.");
        } catch (RuntimeException expected) {
            assertEquals(String.format("report file: %s is not writable",
                    tmpReportFile.getAbsolutePath()), expected.getMessage());
        } finally {
            FileUtil.deleteFile(tmpReportFile);
        }
    }

    /**
     * Test that events sent through the socket reporting part are received on the other hand.
     */
    @Test
    public void testPrintEvent_printToSocket() throws Exception {
        TestDescription testId = new TestDescription("com.fakeclass", "faketest");
        ITestInvocationListener mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        SubprocessTestResultsParser receiver =
                new SubprocessTestResultsParser(mMockListener, true, new InvocationContext());
        Capture<FailureDescription> captured = new Capture<>();
        try {
            OptionSetter setter = new OptionSetter(mReporter);
            setter.setOptionValue("subprocess-report-port",
                    Integer.toString(receiver.getSocketServerPort()));
            // mirror calls between receiver and sender.
            mMockListener.testIgnored(testId);
            mMockListener.testAssumptionFailure(testId, "fake trace");
            mMockListener.testRunFailed(EasyMock.capture(captured));
            mMockListener.invocationFailed((Throwable)EasyMock.anyObject());
            EasyMock.replay(mMockListener);
            mReporter.testIgnored(testId);
            mReporter.testAssumptionFailure(testId, "fake trace");
            mReporter.testRunFailed("no reason");
            mReporter.invocationFailed(new Throwable());
            mReporter.close();
            receiver.joinReceiver(LONG_TIMEOUT_MS);
            EasyMock.verify(mMockListener);
        } finally {
            receiver.close();
        }
        FailureDescription capturedFailure = captured.getValue();
        assertEquals("no reason", capturedFailure.getErrorMessage());
        assertNull(capturedFailure.getFailureStatus());
        assertEquals(ActionInProgress.UNSET, capturedFailure.getActionInProgress());
        assertEquals("", capturedFailure.getOrigin());
    }

    /**
     * Test that events sent through the socket reporting part are received on the other hand even
     * with the new structured failures.
     */
    @Test
    public void testPrintEvent_printToSocket_StructuredFailures() throws Exception {
        TestDescription testId = new TestDescription("com.fakeclass", "faketest");
        ITestInvocationListener mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        SubprocessTestResultsParser receiver =
                new SubprocessTestResultsParser(mMockListener, true, new InvocationContext());
        Capture<FailureDescription> captured = new Capture<>();
        Capture<FailureDescription> invocationFailureCaptured = new Capture<>();
        try {
            OptionSetter setter = new OptionSetter(mReporter);
            setter.setOptionValue(
                    "subprocess-report-port", Integer.toString(receiver.getSocketServerPort()));
            // mirror calls between receiver and sender.
            mMockListener.testIgnored(testId);
            mMockListener.testAssumptionFailure(testId, "fake trace");
            mMockListener.testRunFailed(EasyMock.capture(captured));
            mMockListener.invocationFailed(EasyMock.capture(invocationFailureCaptured));
            EasyMock.replay(mMockListener);
            mReporter.testIgnored(testId);
            mReporter.testAssumptionFailure(testId, "fake trace");
            FailureDescription runFailure =
                    FailureDescription.create("no reason")
                            .setFailureStatus(FailureStatus.TEST_FAILURE)
                            .setOrigin("origin")
                            .setActionInProgress(ActionInProgress.FETCHING_ARTIFACTS)
                            .setErrorIdentifier(InfraErrorIdentifier.UNDETERMINED);
            mReporter.testRunFailed(runFailure);
            FailureDescription invocationFailure =
                    FailureDescription.create("invoc error")
                            .setCause(new Throwable("invoc erroc"))
                            .setFailureStatus(FailureStatus.INFRA_FAILURE)
                            .setActionInProgress(ActionInProgress.TEST)
                            .setOrigin("invoc origin")
                            .setErrorIdentifier(InfraErrorIdentifier.UNDETERMINED);
            mReporter.invocationFailed(invocationFailure);
            mReporter.close();
            receiver.joinReceiver(LONG_TIMEOUT_MS);
            EasyMock.verify(mMockListener);
        } finally {
            receiver.close();
        }
        FailureDescription capturedFailure = captured.getValue();
        assertEquals("no reason", capturedFailure.getErrorMessage());
        assertEquals(FailureStatus.TEST_FAILURE, capturedFailure.getFailureStatus());
        assertEquals(ActionInProgress.FETCHING_ARTIFACTS, capturedFailure.getActionInProgress());
        assertEquals("origin", capturedFailure.getOrigin());
        assertEquals(
                InfraErrorIdentifier.UNDETERMINED.name(),
                capturedFailure.getErrorIdentifier().name());
        assertEquals(
                InfraErrorIdentifier.UNDETERMINED.code(),
                capturedFailure.getErrorIdentifier().code());

        FailureDescription capturedInvocation = invocationFailureCaptured.getValue();
        assertEquals("invoc error", capturedInvocation.getErrorMessage());
        assertEquals(FailureStatus.INFRA_FAILURE, capturedInvocation.getFailureStatus());
        assertEquals(ActionInProgress.TEST, capturedInvocation.getActionInProgress());
        assertEquals("invoc origin", capturedInvocation.getOrigin());
        assertEquals(
                InfraErrorIdentifier.UNDETERMINED.name(),
                capturedInvocation.getErrorIdentifier().name());
        assertEquals(
                InfraErrorIdentifier.UNDETERMINED.code(),
                capturedInvocation.getErrorIdentifier().code());
    }

    @Test
    public void testTestLog() throws ConfigurationException, IOException {
        byte[] logData = new byte[1024];
        InputStreamSource logStreamSource = new ByteArrayInputStreamSource(logData);
        AtomicBoolean testLogCalled = new AtomicBoolean(false);
        ITestInvocationListener mMockListener =
                new CollectingTestListener() {
                    @Override
                    public void testLog(
                            String dataName, LogDataType dataType, InputStreamSource dataStream) {
                        testLogCalled.set(true);
                    }
                };
        try (SubprocessTestResultsParser receiver =
                new SubprocessTestResultsParser(mMockListener, true, new InvocationContext())) {
            receiver.setIgnoreTestLog(false);
            OptionSetter setter = new OptionSetter(mReporter);
            setter.setOptionValue(
                    "subprocess-report-port", Integer.toString(receiver.getSocketServerPort()));
            setter.setOptionValue("output-test-log", "true");

            mReporter.testLog("foo", LogDataType.TEXT, logStreamSource);
            mReporter.close();
            receiver.joinReceiver(LONG_TIMEOUT_MS);

            assertTrue(testLogCalled.get());
        }
    }

    @Test
    public void testTestLog_disabled() throws ConfigurationException, IOException {
        byte[] logData = new byte[1024];
        InputStreamSource logStreamSource = new ByteArrayInputStreamSource(logData);
        AtomicBoolean testLogCalled = new AtomicBoolean(false);
        ITestInvocationListener mMockListener =
                new CollectingTestListener() {
                    @Override
                    public void testLog(
                            String dataName, LogDataType dataType, InputStreamSource dataStream) {
                        testLogCalled.set(true);
                    }
                };
        try (SubprocessTestResultsParser receiver =
                new SubprocessTestResultsParser(mMockListener, true, new InvocationContext())) {
            OptionSetter setter = new OptionSetter(mReporter);
            setter.setOptionValue(
                    "subprocess-report-port", Integer.toString(receiver.getSocketServerPort()));
            setter.setOptionValue("output-test-log", "false");

            mReporter.testLog("foo", LogDataType.TEXT, logStreamSource);
            mReporter.close();
            receiver.joinReceiver(500);

            assertFalse(testLogCalled.get());
        }
    }
}
