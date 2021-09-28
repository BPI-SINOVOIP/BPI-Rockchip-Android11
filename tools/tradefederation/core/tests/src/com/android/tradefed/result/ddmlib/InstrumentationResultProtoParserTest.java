/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.android.commands.am.InstrumentationData.ResultsBundle;
import com.android.commands.am.InstrumentationData.ResultsBundleEntry;
import com.android.commands.am.InstrumentationData.Session;
import com.android.commands.am.InstrumentationData.SessionStatus;
import com.android.commands.am.InstrumentationData.SessionStatusCode;
import com.android.commands.am.InstrumentationData.TestStatus;
import com.android.ddmlib.testrunner.ITestRunListener;
import com.android.ddmlib.testrunner.TestIdentifier;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link InstrumentationResultProtoParser}. */
@RunWith(JUnit4.class)
public class InstrumentationResultProtoParserTest {

    private InstrumentationResultProtoParser mParser;
    private ITestRunListener mMockListener;

    private static final String RUN_KEY = "testing";
    private static final String CLASS_NAME_1 = "class_1";
    private static final String METHOD_NAME_1 = "method_1";
    private static final String CLASS_NAME_2 = "class_2";
    private static final String METHOD_NAME_2 = "method_2";
    private static final String TEST_FAILURE_MESSAGE_1 = "java.lang.AssertionError: No App";
    private static final String RUN_FAILURE_MESSAGE = "Unable to find instrumentation info:";
    private static final String TEST_COMPLETED_STATUS_1 = "Expected 2 tests, received 0";
    private static final String TEST_COMPLETED_STATUS_2 = "Expected 2 tests, received 1";
    private static final String INCOMPLETE_TEST_ERR_MSG_PREFIX = "Test failed to run"
            + " to completion";
    private static final String INCOMPLETE_RUN_ERR_MSG_PREFIX = "Test run failed to complete";
    private static final String FATAL_EXCEPTION_MSG = "Fatal exception when running tests";

    private File protoTestFile = null;

    @Before
    public void setUp() {
        List<ITestRunListener> runListeners = new ArrayList<>();
        mMockListener = EasyMock.createStrictMock(ITestRunListener.class);
        runListeners.add(mMockListener);
        mParser = new InstrumentationResultProtoParser(RUN_KEY, runListeners);
    }

    // Sample one test success instrumentation proto file in a test run.

    // result_code: 1
    // results {
    // entries {
    // key: "class"
    // value_string: "android.platform.test.scenario.clock.OpenAppMicrobenchmark"
    // }
    // entries {
    // key: "current"
    // value_int: 1
    // }
    // entries {
    // key: "id"
    // value_string: "AndroidJUnitRunner"
    // }
    // entries {
    // key: "numtests"
    // value_int: 1
    // }
    // entries {
    // key: "stream"
    // value_string: "\nandroid.platform.test.scenario.clock.OpenAppMicrobenchmark:"
    // }
    // entries {
    // key: "test"
    // value_string: "testOpen"
    // }
    // }
    // result_code: 2
    // results {
    // entries {
    // key: "cold_startup_com.google.android.deskclock"
    // value_string: "626"
    // }
    // }
    //
    // results {
    // entries {
    // key: "class"
    // value_string: "android.platform.test.scenario.clock.OpenAppMicrobenchmark"
    // }
    // entries {
    // key: "current"
    // value_int: 1
    // }
    // entries {
    // key: "id"
    // value_string: "AndroidJUnitRunner"
    // }
    // entries {
    // key: "numtests"
    // value_int: 1
    // }
    // entries {
    // key: "stream"
    // value_string: "."
    // }
    // entries {
    // key: "test"
    // value_string: "testOpen"
    // }
    // }
    //
    // result_code: -1
    // results {
    // entries {
    // key: "stream"
    // value_string: "\n\nTime: 27.013\n\nOK (1 test)\n\n"
    // }
    // entries {
    // key: "total_cpu_usage"
    // value_string: "39584"
    // }
    // }

    /**
     * Test for the null input instrumentation results proto file.
     *
     * @throws IOException
     */
    @Test
    public void testNullProtoFile() throws IOException {
        protoTestFile = null;
        mMockListener.testRunStarted(RUN_KEY, 0);
        mMockListener.testRunFailed(EasyMock
                .eq(InstrumentationResultProtoParser.NO_TEST_RESULTS_FILE));
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);
    }

    /**
     * Test for the empty input instrumentation results proto file.
     *
     * @throws IOException
     */
    @Test
    public void testEmptyProtoFile() throws IOException {
        protoTestFile = File.createTempFile("tmp", ".pb");
        mMockListener.testRunStarted(RUN_KEY, 0);
        mMockListener.testRunFailed(EasyMock
                .eq(InstrumentationResultProtoParser.NO_TEST_RESULTS_MSG));
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);
    }

    /**
     * Test for the invalid input instrumentation results proto file.
     *
     * @throws IOException
     */
    @Test
    public void testInvalidResultsProtoFile() throws IOException {
        protoTestFile = File.createTempFile("tmp", ".pb");
        FileOutputStream fout = new FileOutputStream(protoTestFile);
        fout.write(65);
        fout.close();
        mMockListener.testRunStarted(RUN_KEY, 0);
        mMockListener.testRunFailed(EasyMock
                .eq(InstrumentationResultProtoParser.INVALID_TEST_RESULTS_FILE));
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);
    }

    /**
     * Test for the no test results in input instrumentation results proto file.
     *
     * @throws IOException
     */
    @Test
    public void testNoTestResults() throws IOException {

        protoTestFile = buildNoTestResultsProtoFile();

        mMockListener.testRunStarted(RUN_KEY, 0);
        mMockListener.testRunEnded(27013, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);
    }

    /**
     * Test for one test success results in input instrumentation results proto file.
     *
     * @throws IOException
     */
    @Test
    public void testOneTestSuccessWithMetrics() throws IOException {
        protoTestFile = buildSingleTestMetricSuccessProtoFile();

        TestIdentifier td = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        Capture<Map<String, String>> captureTestMetrics = new Capture<Map<String, String>>();

        mMockListener.testRunStarted(RUN_KEY, 1);
        mMockListener.testStarted(td);
        mMockListener.testEnded(EasyMock.eq(td), EasyMock.capture(captureTestMetrics));
        mMockListener.testRunEnded(27013, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);

        // Verify the test metrics
        assertEquals("626",
                captureTestMetrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTestMetrics.getValue()
                        .get("metric_key2"));
    }

    /**
     * Test for one test success result with multiple listeners in instrumentation results proto
     * file.
     *
     * @throws IOException
     */
    @Test
    public void testOneTestSuccessWithMultipleListeners() throws IOException {

        List<ITestRunListener> runListeners = new ArrayList<>();
        ITestRunListener mMockListener1 = EasyMock.createStrictMock(ITestRunListener.class);
        ITestRunListener mMockListener2 = EasyMock.createStrictMock(ITestRunListener.class);
        runListeners.add(mMockListener1);
        runListeners.add(mMockListener2);

        mParser = new InstrumentationResultProtoParser(RUN_KEY, runListeners);

        protoTestFile = buildSingleTestMetricSuccessProtoFile();

        TestIdentifier td = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        Capture<Map<String, String>> captureListener1Metrics = new Capture<Map<String, String>>();
        Capture<Map<String, String>> captureListener2Metrics = new Capture<Map<String, String>>();

        mMockListener1.testRunStarted(RUN_KEY, 1);
        mMockListener1.testStarted(td);
        mMockListener1.testEnded(EasyMock.eq(td), EasyMock.capture(captureListener1Metrics));
        mMockListener1.testRunEnded(27013, Collections.emptyMap());

        mMockListener2.testRunStarted(RUN_KEY, 1);
        mMockListener2.testStarted(td);
        mMockListener2.testEnded(EasyMock.eq(td), EasyMock.capture(captureListener2Metrics));
        mMockListener2.testRunEnded(27013, Collections.emptyMap());

        EasyMock.replay(mMockListener1);
        EasyMock.replay(mMockListener2);
        mParser.processProtoFile(protoTestFile);
        EasyMock.verify(mMockListener1);
        EasyMock.verify(mMockListener2);

        // Verify the test metrics
        assertEquals("626",
                captureListener1Metrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureListener1Metrics.getValue()
                        .get("metric_key2"));

        // Verify the test metrics
        assertEquals("626",
                captureListener2Metrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureListener2Metrics.getValue()
                        .get("metric_key2"));
    }

    /**
     * Test for test run with the metrics.
     *
     * @throws IOException
     */
    @Test
    public void testOneRunSuccessWithMetrics() throws IOException {
        protoTestFile = buildRunMetricSuccessProtoFile();

        TestIdentifier td = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        Capture<Map<String, String>> captureRunMetrics = new Capture<Map<String, String>>();
        mMockListener.testRunStarted(RUN_KEY, 1);
        mMockListener.testStarted(td);
        mMockListener.testEnded(td, Collections.emptyMap());
        mMockListener.testRunEnded(EasyMock.eq(27013L), EasyMock.capture(captureRunMetrics));

        processProtoAndVerify(protoTestFile);

        // Verify run metrics
        assertEquals("39584",
                captureRunMetrics.getValue().get("run_metric_key"));
    }

    /**
     * Test for test metrics and test run metrics in instrumentation proto file.
     *
     * @throws IOException
     */
    @Test
    public void testOneTestAndRunSuccessWithMetrics() throws IOException {
        protoTestFile = buildTestAndRunMetricSuccessProtoFile();

        TestIdentifier td = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        Capture<Map<String, String>> captureTestMetrics = new Capture<Map<String, String>>();
        Capture<Map<String, String>> captureRunMetrics = new Capture<Map<String, String>>();
        mMockListener.testRunStarted(RUN_KEY, 1);
        mMockListener.testStarted(td);
        mMockListener.testEnded(EasyMock.eq(td), EasyMock.capture(captureTestMetrics));
        mMockListener.testRunEnded(EasyMock.eq(27013L), EasyMock.capture(captureRunMetrics));

        processProtoAndVerify(protoTestFile);

        // Verify the test metrics
        assertEquals("626",
                captureTestMetrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTestMetrics.getValue()
                        .get("metric_key2"));

        // Verify run metrics
        assertEquals("39584",
                captureRunMetrics.getValue().get("run_metric_key"));
    }

    /**
     * Test for multiple test success with metrics.
     *
     * @throws IOException
     */
    @Test
    public void testMultipleTestSuccessWithMetrics() throws IOException {
        protoTestFile = buildMultipleTestAndRunMetricSuccessProtoFile();

        TestIdentifier td1 = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        TestIdentifier td2 = new TestIdentifier(CLASS_NAME_2, METHOD_NAME_2);

        Capture<Map<String, String>> captureTest1Metrics = new Capture<Map<String, String>>();
        Capture<Map<String, String>> captureTest2Metrics = new Capture<Map<String, String>>();
        Capture<Map<String, String>> captureRunMetrics = new Capture<Map<String, String>>();

        mMockListener.testRunStarted(RUN_KEY, 2);
        mMockListener.testStarted(td1);
        mMockListener.testEnded(EasyMock.eq(td1), EasyMock.capture(captureTest1Metrics));
        mMockListener.testStarted(td2);
        mMockListener.testEnded(EasyMock.eq(td2), EasyMock.capture(captureTest2Metrics));
        mMockListener.testRunEnded(EasyMock.eq(27013L), EasyMock.capture(captureRunMetrics));

        processProtoAndVerify(protoTestFile);

        // Verify the test1 and test2 metrics
        assertEquals("626",
                captureTest1Metrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTest1Metrics.getValue()
                        .get("metric_key2"));
        assertEquals("626",
                captureTest2Metrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTest2Metrics.getValue()
                        .get("metric_key2"));

        // Verify run metrics
        assertEquals("39584",
                captureRunMetrics.getValue().get("run_metric_key"));
    }

    /**
     * Test for one test failure.
     *
     * @throws IOException
     */
    @Test
    public void testOneTestFailure() throws IOException {
        protoTestFile = buildSingleTestFailureProtoFile();

        TestIdentifier td = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        Capture<Map<String, String>> captureTestMetrics = new Capture<Map<String, String>>();

        mMockListener.testRunStarted(RUN_KEY, 1);
        mMockListener.testStarted(td);
        mMockListener.testFailed(EasyMock.eq(td), EasyMock.eq(TEST_FAILURE_MESSAGE_1));
        mMockListener.testEnded(EasyMock.eq(td), EasyMock.capture(captureTestMetrics));
        mMockListener.testRunEnded(27013, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);

        // Verify the test metrics
        assertEquals("626",
                captureTestMetrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTestMetrics.getValue()
                        .get("metric_key2"));
    }

    /**
     * Test for one test pass and one test failure.
     *
     * @throws IOException
     */
    @Test
    public void testOneTestPassOneTestFailure() throws IOException {
        protoTestFile = buildOneTestPassOneTestFailProtoFile();

        TestIdentifier td1 = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        TestIdentifier td2 = new TestIdentifier(CLASS_NAME_2, METHOD_NAME_2);

        Capture<Map<String, String>> captureTest1Metrics = new Capture<Map<String, String>>();

        mMockListener.testRunStarted(RUN_KEY, 2);
        mMockListener.testStarted(td1);
        mMockListener.testEnded(EasyMock.eq(td1), EasyMock.capture(captureTest1Metrics));

        mMockListener.testStarted(td2);
        mMockListener.testFailed(EasyMock.eq(td2), EasyMock.eq(TEST_FAILURE_MESSAGE_1));
        mMockListener.testEnded(td2, Collections.emptyMap());

        mMockListener.testRunEnded(27013, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);

        // Verify the test metrics
        assertEquals("626",
                captureTest1Metrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTest1Metrics.getValue()
                        .get("metric_key2"));
    }

    /**
     * Test for all tests incomplete in a test run.
     *
     * @throws IOException
     */
    @Test
    public void testAllTestsIncomplete() throws IOException {
        protoTestFile = buildTestsIncompleteProtoFile();
        Capture<String> testOutputErrorMessage = new Capture<>();
        Capture<String> runOutputErrorMessage = new Capture<>();

        TestIdentifier td1 = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        mMockListener.testRunStarted(RUN_KEY, 2);
        mMockListener.testStarted(td1);
        mMockListener.testFailed(EasyMock.eq(td1), EasyMock.capture(testOutputErrorMessage));
        mMockListener.testEnded(td1, Collections.emptyMap());
        mMockListener.testRunFailed(EasyMock.capture(runOutputErrorMessage));
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);

        assertTrue(testOutputErrorMessage.toString().contains(
                INCOMPLETE_TEST_ERR_MSG_PREFIX));
        assertTrue(testOutputErrorMessage.toString().contains(TEST_COMPLETED_STATUS_1));
        assertTrue(runOutputErrorMessage.toString().contains(
                INCOMPLETE_RUN_ERR_MSG_PREFIX));
        assertTrue(runOutputErrorMessage.toString().contains(TEST_COMPLETED_STATUS_1));
    }

    /**
     * Test for one test complete and another test partial status.
     *
     * @throws IOException
     */
    @Test
    public void testPartialTestsIncomplete() throws IOException {
        protoTestFile = buildPartialTestsIncompleteProtoFile();

        Capture<String> testOutputErrorMessage = new Capture<>();
        Capture<String> runOutputErrorMessage = new Capture<>();
        TestIdentifier td1 = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        TestIdentifier td2 = new TestIdentifier(CLASS_NAME_2, METHOD_NAME_2);
        Capture<Map<String, String>> captureTest1Metrics = new Capture<Map<String, String>>();

        mMockListener.testRunStarted(RUN_KEY, 2);
        mMockListener.testStarted(td1);
        mMockListener.testEnded(EasyMock.eq(td1), EasyMock.capture(captureTest1Metrics));
        mMockListener.testStarted(td2);
        mMockListener.testFailed(EasyMock.eq(td2), EasyMock.capture(testOutputErrorMessage));
        mMockListener.testEnded(td2, Collections.emptyMap());
        mMockListener.testRunFailed(EasyMock.capture(runOutputErrorMessage));
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);

        assertEquals("626",
                captureTest1Metrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTest1Metrics.getValue()
                        .get("metric_key2"));
        assertTrue(testOutputErrorMessage.toString().contains(
                INCOMPLETE_TEST_ERR_MSG_PREFIX));
        assertTrue(testOutputErrorMessage.toString().contains(TEST_COMPLETED_STATUS_2));
        assertTrue(runOutputErrorMessage.toString().contains(
                INCOMPLETE_RUN_ERR_MSG_PREFIX));
        assertTrue(runOutputErrorMessage.toString().contains(TEST_COMPLETED_STATUS_2));
    }

    /**
     * Test 1 test completed, 1 test not started from two expected tests in a test run.
     *
     * @throws IOException
     */
    @Test
    public void testOneTestNotStarted() throws IOException {
        protoTestFile = buildOneTestNotStarted();
        Capture<String> runOutputErrorMessage = new Capture<>();
        TestIdentifier td1 = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        Capture<Map<String, String>> captureTest1Metrics = new Capture<Map<String, String>>();

        mMockListener.testRunStarted(RUN_KEY, 2);
        mMockListener.testStarted(td1);
        mMockListener.testEnded(EasyMock.eq(td1), EasyMock.capture(captureTest1Metrics));
        mMockListener.testRunFailed(EasyMock.capture(runOutputErrorMessage));
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);

        assertEquals("626",
                captureTest1Metrics.getValue().get("metric_key1"));
        assertEquals("1",
                captureTest1Metrics.getValue()
                        .get("metric_key2"));
        assertTrue(runOutputErrorMessage.toString().contains(
                INCOMPLETE_RUN_ERR_MSG_PREFIX));
        assertTrue(runOutputErrorMessage.toString().contains(TEST_COMPLETED_STATUS_2));
    }

    /**
     * Test for no time stamp parsing error when the time stamp parsing is not enforced.
     *
     * @throws IOException
     */
    @Test
    public void testTimeStampMissingNotEnforced() throws IOException {
        protoTestFile = buildInvalidTimeStampResultsProto(false);

        mMockListener.testRunStarted(RUN_KEY, 0);
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);
    }

    /**
     * Tests parsing the fatal error output of an instrumentation invoked with "-e log true". Since
     * it is log only, it will not report directly the failure, but the stream should still be
     * populated.
     *
     * @throws IOException
     */
    @Test
    public void testDirectFailure() throws IOException {
        protoTestFile = buildValidTimeStampWithFatalExceptionResultsProto();

        Capture<String> capture = new Capture<>();
        mMockListener.testRunStarted(RUN_KEY, 0);
        mMockListener.testRunFailed(EasyMock.capture(capture));
        mMockListener.testRunEnded(0, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);

        String failure = capture.getValue();
        assertTrue(failure.contains("java.lang.RuntimeException: it failed super fast."));
    }

    /**
     * Tests for ignore test status from the proto output.
     *
     * @throws IOException
     */
    @Test
    public void testIgnoreProtoResult() throws IOException {
        protoTestFile = buildTestIgnoredResultsProto();

        mMockListener.testRunStarted(RUN_KEY, 1);
        TestIdentifier td1 = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        mMockListener.testStarted(td1);
        mMockListener.testIgnored(td1);
        mMockListener.testEnded(td1, Collections.emptyMap());
        mMockListener.testRunEnded(27013, Collections.emptyMap());

        processProtoAndVerify(protoTestFile);
    }

    /**
     * Tests for assumption failure test status from the proto output.
     *
     * @throws IOException
     */
    @Test
    public void testAssumptionProtoResult() throws IOException {
        protoTestFile = buildTestAssumptionResultsProto();

        mMockListener.testRunStarted(RUN_KEY, 1);
        TestIdentifier td1 = new TestIdentifier(CLASS_NAME_1, METHOD_NAME_1);
        mMockListener.testStarted(td1);
        mMockListener.testAssumptionFailure(EasyMock.eq(td1),
                EasyMock.startsWith(
                        "org.junit.AssumptionViolatedException:"
                                + " got: <false>, expected: is <true>"));
        mMockListener.testEnded(td1, Collections.emptyMap());
        mMockListener.testRunEnded(27013, Collections.emptyMap());
        processProtoAndVerify(protoTestFile);

    }

    @After
    public void tearDown() {
        if (protoTestFile != null && protoTestFile.exists()) {
            protoTestFile.delete();
        }
    }

    private void processProtoAndVerify(File protoTestFile) throws IOException {
        EasyMock.replay(mMockListener);
        mParser.processProtoFile(protoTestFile);
        EasyMock.verify(mMockListener);
    }

    private File buildNoTestResultsProtoFile() throws IOException {
        Session sessionProto = Session.newBuilder()
                .setSessionStatus(getSessionStatusProto(false, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildSingleTestMetricSuccessProtoFile() throws IOException {
        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, true, false));
        // Test Metric
        testStatusList.add(getTestStatusProto(true));
        // Test End
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, false, false));
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(false, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildRunMetricSuccessProtoFile() throws IOException {
        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(false));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, false, false));
        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(true, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildTestAndRunMetricSuccessProtoFile() throws IOException {
        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(true));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, false, false));
        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(true, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildMultipleTestAndRunMetricSuccessProtoFile() throws IOException {
        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(true));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, false, false));
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_2, METHOD_NAME_2, 2, 2, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(true));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_2, METHOD_NAME_2, 2, 2, false, false));
        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(true, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildSingleTestFailureProtoFile() throws IOException {
        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(true));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, false, true));
        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(false, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildOneTestPassOneTestFailProtoFile() throws IOException {
        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(true));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, false, false));
        testStatusList.add(getTestInfoProto(CLASS_NAME_2, METHOD_NAME_2, 2, 2, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(false));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_2, METHOD_NAME_2, 2, 2, false, true));
        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(false, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildTestsIncompleteProtoFile() throws IOException {
        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, true, false));

        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;

    }

    private File buildPartialTestsIncompleteProtoFile() throws IOException {

        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(true));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, false, false));
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_2, METHOD_NAME_2, 2, 2, true, false));

        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildOneTestNotStarted() throws IOException {

        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, true, false));
        // Test status without metrics.
        testStatusList.add(getTestStatusProto(true));
        // Test End.
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 2, false, false));

        // Session with metrics.
        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildInvalidTimeStampResultsProto(boolean isWithStack) throws IOException {

        List<ResultsBundleEntry> entryList = new LinkedList<ResultsBundleEntry>();

        if (isWithStack) {
            entryList.add(ResultsBundleEntry.newBuilder()
                    .setKey("stream")
                    .setValueString(FATAL_EXCEPTION_MSG
                            + " java.lang.IllegalArgumentException: Ambiguous arguments: "
                            + "cannot provide both test package and test class(es) to run")
                    .build());
        } else {
            entryList.add(ResultsBundleEntry.newBuilder()
                    .setKey("stream").setValueString("")
                    .build());
        }

        SessionStatus sessionStatus = SessionStatus.newBuilder().setResultCode(-1)
                .setStatusCode(SessionStatusCode.SESSION_FINISHED)
                .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                .build();

        // Session with metrics.
        Session sessionProto = Session.newBuilder()
                .setSessionStatus(sessionStatus).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildValidTimeStampWithFatalExceptionResultsProto() throws IOException {
        List<ResultsBundleEntry> entryList = new LinkedList<ResultsBundleEntry>();

        entryList.add(ResultsBundleEntry.newBuilder()
                .setKey("stream")
                .setValueString(FATAL_EXCEPTION_MSG
                        + "Time: 0 \n"
                        + "1) Fatal exception when running tests"
                        + "java.lang.RuntimeException: it failed super fast."
                        + "at stackstack"
                )
                .build());

        SessionStatus sessionStatus = SessionStatus.newBuilder().setResultCode(-1)
                .setStatusCode(SessionStatusCode.SESSION_FINISHED)
                .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                .build();

        // Session with metrics.
        Session sessionProto = Session.newBuilder()
                .setSessionStatus(sessionStatus).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildTestIgnoredResultsProto() throws IOException {

        List<TestStatus> testStatusList = new LinkedList<TestStatus>();
        // Test start
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, true, false));

        // Test ignore status result.
        List<ResultsBundleEntry> entryList = new LinkedList<ResultsBundleEntry>();
        entryList.add(ResultsBundleEntry.newBuilder().setKey("class").setValueString(CLASS_NAME_1)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("current").setValueInt(1)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("id")
                .setValueString("AndroidJUnitRunner").build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("numtests").setValueInt(1)
                .build());

        entryList.add(ResultsBundleEntry.newBuilder().setKey("test").setValueString(METHOD_NAME_1)
                .build());

        testStatusList.add(TestStatus.newBuilder().setResultCode(-3)
                .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                .build());

        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(false, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    private File buildTestAssumptionResultsProto() throws IOException {

        List<TestStatus> testStatusList = new LinkedList<TestStatus>();

        // Test start
        testStatusList.add(getTestInfoProto(CLASS_NAME_1, METHOD_NAME_1, 1, 1, true, false));

        // Test ignore status result.
        List<ResultsBundleEntry> entryList = new LinkedList<ResultsBundleEntry>();
        entryList.add(ResultsBundleEntry.newBuilder().setKey("class").setValueString(CLASS_NAME_1)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("current").setValueInt(1)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("id")
                .setValueString("AndroidJUnitRunner").build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("numtests").setValueInt(1)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("test").setValueString(METHOD_NAME_1)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("stack").setValueString(
                "org.junit.AssumptionViolatedException: got: <false>, expected: is <true>")
                .build());
        testStatusList.add(TestStatus.newBuilder().setResultCode(-4)
                .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                .build());

        Session sessionProto = Session.newBuilder().addAllTestStatus(testStatusList)
                .setSessionStatus(getSessionStatusProto(false, false)).build();
        File protoFile = File.createTempFile("tmp", ".pb");
        sessionProto.writeTo(new FileOutputStream(protoFile));
        return protoFile;
    }

    /**
     * Add test status proto message based on the args supplied to this method.
     *
     * @param className class name where the test method is.
     * @param methodName method name currently running.
     * @param current current number of the test.
     * @param numTests total number of test.
     * @param isStart true is if it is start of the test otherwise treated as end of the test.
     * @param isFailure true if the test if failed.
     * @return
     */
    private TestStatus getTestInfoProto(String className, String methodName, int current,
            int numTests, boolean isStart, boolean isFailure) {
        List<ResultsBundleEntry> entryList = new LinkedList<ResultsBundleEntry>();
        entryList.add(ResultsBundleEntry.newBuilder().setKey("class").setValueString(className)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("current").setValueInt(current)
                .build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("id")
                .setValueString("AndroidJUnitRunner").build());
        entryList.add(ResultsBundleEntry.newBuilder().setKey("numtests").setValueInt(numTests)
                .build());

        entryList.add(ResultsBundleEntry.newBuilder().setKey("test").setValueString(methodName)
                .build());

        if (isFailure) {
            entryList.add(ResultsBundleEntry.newBuilder().setKey("stack")
                    .setValueString(TEST_FAILURE_MESSAGE_1)
                    .build());
            entryList.add(ResultsBundleEntry.newBuilder().setKey("stream")
                    .setValueString(TEST_FAILURE_MESSAGE_1)
                    .build());
            // Test failure will have result code "-2"
            return TestStatus.newBuilder().setResultCode(-2)
                    .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                    .build();
        }

        entryList.add(ResultsBundleEntry.newBuilder().setKey("stream").setValueString("\nabc:")
                .build());

        if (isStart) {
            // Test start will have result code 1.
            return TestStatus.newBuilder().setResultCode(1)
                    .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                    .build();
        }

        return TestStatus.newBuilder()
                .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build()).build();
    }

    /**
     * Add test status with the metrics in the proto result file.
     *
     * @param isWithMetrics if false metric will be ignored.
     * @return
     */
    private TestStatus getTestStatusProto(boolean isWithMetrics) {
        List<ResultsBundleEntry> entryList = new LinkedList<ResultsBundleEntry>();
        if (isWithMetrics) {
            entryList.add(ResultsBundleEntry.newBuilder()
                    .setKey("metric_key1").setValueString("626")
                    .build());
            entryList.add(ResultsBundleEntry.newBuilder()
                    .setKey("metric_key2").setValueString("1")
                    .build());
        }

        // Metric status will be in progress
        return TestStatus.newBuilder().setResultCode(2)
                .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                .build();
    }

    /**
     * Add session status message in the proto result file based on the args supplied to this
     * method.
     *
     * @param isWithMetrics is true then add metrics to the session message.
     * @param isFailure is true then failure message will be added to the final message.
     * @return
     */
    private SessionStatus getSessionStatusProto(boolean isWithMetrics, boolean isFailure) {
        List<ResultsBundleEntry> entryList = new LinkedList<ResultsBundleEntry>();

        if (isFailure) {
            entryList.add(ResultsBundleEntry.newBuilder()
                    .setKey("Error").setValueString(RUN_FAILURE_MESSAGE)
                    .build());
            entryList.add(ResultsBundleEntry.newBuilder()
                    .setKey("id").setValueString("ActivityManagerService")
                    .build());
            return SessionStatus.newBuilder().setResultCode(-1)
                    .setStatusCode(SessionStatusCode.SESSION_FINISHED)
                    .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                    .build();

        }
        entryList.add(ResultsBundleEntry.newBuilder()
                .setKey("stream").setValueString("\n\nTime: 27.013\n\nOK (1 test)\n\n")
                .build());

        if (isWithMetrics) {
            entryList.add(ResultsBundleEntry.newBuilder()
                    .setKey("run_metric_key").setValueString("39584")
                    .build());
        }



        return SessionStatus.newBuilder().setResultCode(-1)
                .setStatusCode(SessionStatusCode.SESSION_FINISHED)
                .setResults(ResultsBundle.newBuilder().addAllEntries(entryList).build())
                .build();
    }
}
