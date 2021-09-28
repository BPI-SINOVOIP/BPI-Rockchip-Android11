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
package com.android.tradefed.testtype;

import static org.easymock.EasyMock.anyObject;
import static org.easymock.EasyMock.createMock;
import static org.easymock.EasyMock.eq;
import static org.easymock.EasyMock.expectLastCall;
import static org.easymock.EasyMock.replay;
import static org.easymock.EasyMock.verify;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.ArrayUtil;

import junit.framework.AssertionFailedError;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.Vector;

/** Unit tests for {@link PythonUnitTestResultParser}. */
@RunWith(JUnit4.class)
public class PythonUnitTestResultParserTest {

    public static final String PYTHON_OUTPUT_FILE_1 = "python_output1.txt";
    public static final String PYTHON_OUTPUT_FILE_2 = "python_output2.txt";
    public static final String PYTHON_OUTPUT_FILE_3 = "python_output3.txt";

    private PythonUnitTestResultParser mParser;
    private ITestInvocationListener mMockListener;

    @Before
    public void setUp() throws Exception {
        mMockListener = createMock(ITestInvocationListener.class);
        mParser = new PythonUnitTestResultParser(ArrayUtil.list(mMockListener), "test");
    }

    @Test
    public void testRegexTestCase() {
        String s = "a (b) ... ok";
        assertTrue(PythonUnitTestResultParser.PATTERN_ONE_LINE_RESULT.matcher(s).matches());
        assertFalse(PythonUnitTestResultParser.PATTERN_TWO_LINE_RESULT_FIRST.matcher(s).matches());
        s = "a (b) ... FAIL";
        assertTrue(PythonUnitTestResultParser.PATTERN_ONE_LINE_RESULT.matcher(s).matches());
        s = "a (b) ... ERROR";
        assertTrue(PythonUnitTestResultParser.PATTERN_ONE_LINE_RESULT.matcher(s).matches());
        s = "a (b) ... expected failure";
        assertTrue(PythonUnitTestResultParser.PATTERN_ONE_LINE_RESULT.matcher(s).matches());
        s = "a (b) ... skipped 'reason foo'";
        assertTrue(PythonUnitTestResultParser.PATTERN_ONE_LINE_RESULT.matcher(s).matches());
        s = "a (b)";
        assertFalse(PythonUnitTestResultParser.PATTERN_ONE_LINE_RESULT.matcher(s).matches());
        assertTrue(PythonUnitTestResultParser.PATTERN_TWO_LINE_RESULT_FIRST.matcher(s).matches());
        s = "doc string foo bar ... ok";
        assertTrue(PythonUnitTestResultParser.PATTERN_TWO_LINE_RESULT_SECOND.matcher(s).matches());
        s = "docstringfoobar ... ok";
        assertTrue(PythonUnitTestResultParser.PATTERN_TWO_LINE_RESULT_SECOND.matcher(s).matches());

        s = "a (b) ... "; // Tests with failed subtest assertions could be missing the status.
        assertTrue(PythonUnitTestResultParser.PATTERN_ONE_LINE_RESULT.matcher(s).matches());
    }

    @Test
    public void testRegexFailMessage() {
        String s = "FAIL: a (b)";
        assertTrue(PythonUnitTestResultParser.PATTERN_FAIL_MESSAGE.matcher(s).matches());
        s = "ERROR: a (b)";
        assertTrue(PythonUnitTestResultParser.PATTERN_FAIL_MESSAGE.matcher(s).matches());
        s = "FAIL: a (b) (<subtest>)";
        assertTrue(PythonUnitTestResultParser.PATTERN_FAIL_MESSAGE.matcher(s).matches());
        s = "FAIL: a (b) (i=3)";
        assertTrue(PythonUnitTestResultParser.PATTERN_FAIL_MESSAGE.matcher(s).matches());
    }

    @Test
    public void testRegexRunSummary() {
        String s = "Ran 1 test in 1s";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_SUMMARY.matcher(s).matches());
        s = "Ran 42 tests in 1s";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_SUMMARY.matcher(s).matches());
        s = "Ran 1 tests in 0.000s";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_SUMMARY.matcher(s).matches());
        s = "Ran 1 test in 0.001s";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_SUMMARY.matcher(s).matches());
        s = "Ran 1 test in 12345s";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_SUMMARY.matcher(s).matches());
    }

    @Test
    public void testRegexRunResult() {
        String s = "OK";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_RESULT.matcher(s).matches());
        s = "OK (expected failures=2) ";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_RESULT.matcher(s).matches());
        s = "FAILED (errors=1)";
        assertTrue(PythonUnitTestResultParser.PATTERN_RUN_RESULT.matcher(s).matches());
    }

    @Test
    public void testParseNoTests() throws Exception {
        String[] output = {
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 0 tests in 0.000s",
                "",
                "OK"
        };
        setRunListenerChecks(0, 0);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseSingleTestPass() throws Exception {
        String[] output = {
                "b (a) ... ok",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "OK"
        };
        TestDescription[] ids = {new TestDescription("a", "b")};
        boolean[] didPass = {true};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(1, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParsePartialSingleLineMatchSkipped() throws Exception {
        String[] output = {
            "b (a) ... ok bad-token",
            "",
            PythonUnitTestResultParser.DASH_LINE,
            "Ran 1 test in 1s",
            "",
            "OK"
        };
        setRunListenerChecks(1, 1000);
        // The below verifies that the listener method is never called.
        mMockListener.testStarted(anyObject());
        expectLastCall().andThrow(new AssertionError("unexpected"));
        replay(mMockListener);

        mParser.processNewLines(output);
    }

    @Test
    public void testParseSingleTestPassWithExpectedFailure() throws Exception {
        String[] output = {
                "b (a) ... expected failure",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "OK (expected failures=1)"
        };
        TestDescription[] ids = {new TestDescription("a", "b")};
        boolean[] didPass = {true};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(1, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseMultiTestPass() throws Exception {
        String[] output = {
                "b (a) ... ok",
                "d (c) ... ok",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 2 tests in 1s",
                "",
                "OK"
        };
        TestDescription[] ids = {new TestDescription("a", "b"), new TestDescription("c", "d")};
        boolean didPass[] = {true, true};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(2, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseMultiTestPassWithOneExpectedFailure() throws Exception {
        String[] output = {
                "b (a) ... expected failure",
                "d (c) ... ok",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 2 tests in 1s",
                "",
                "OK (expected failures=1)"
        };
        TestDescription[] ids = {new TestDescription("a", "b"), new TestDescription("c", "d")};
        boolean[] didPass = {true, true};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(2, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseMultiTestPassWithAllExpectedFailure() throws Exception {
        String[] output = {
                "b (a) ... expected failure",
                "d (c) ... expected failure",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 2 tests in 1s",
                "",
                "OK (expected failures=2)"
        };
        TestDescription[] ids = {new TestDescription("a", "b"), new TestDescription("c", "d")};
        boolean[] didPass = {true, true};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(2, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseSingleTestFail() throws Exception {
        String[] output = {
                "b (a) ... ERROR",
                "",
                PythonUnitTestResultParser.EQUAL_LINE,
                "ERROR: b (a)",
                PythonUnitTestResultParser.DASH_LINE,
                "Traceback (most recent call last):",
                "  File \"test_rangelib.py\", line 129, in test_reallyfail",
                "    raise ValueError()",
                "ValueError",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "FAILED (errors=1)"
        };
        TestDescription[] ids = {new TestDescription("a", "b")};
        boolean[] didPass = {false};
        setRunListenerChecks(1, 1000);
        setTestIdChecks(ids, didPass);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseSubtestFailure() throws Exception {
        String[] output = {
            "b (a) ... d (c) ... ok", // Tests with failed subtests don't output a status.
            "",
            PythonUnitTestResultParser.EQUAL_LINE,
            "FAIL: b (a) (i=3)",
            PythonUnitTestResultParser.DASH_LINE,
            "Traceback (most recent call last):",
            "  File \"example_test.py\", line 129, in test_with_failing_subtests",
            "    self.assertTrue(False)",
            "",
            PythonUnitTestResultParser.DASH_LINE,
            "Ran 2 test in 1s",
            "",
            "FAILED (failures=1)"
        };
        TestDescription[] ids = {new TestDescription("a", "b"), new TestDescription("c", "d")};
        boolean[] didPass = {false, true};
        setRunListenerChecks(2, 1000);
        setTestIdChecks(ids, didPass);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseMultiTestFailWithExpectedFailure() throws Exception {
        String[] output = {
                "b (a) ... expected failure",
                "d (c) ... ERROR",
                "",
                PythonUnitTestResultParser.EQUAL_LINE,
                "ERROR: d (c)",
                PythonUnitTestResultParser.DASH_LINE,
                "Traceback (most recent call last):",
                "  File \"test_rangelib.py\", line 129, in test_reallyfail",
                "    raise ValueError()",
                "ValueError",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "FAILED (errors=1)"
        };
        TestDescription[] ids = {new TestDescription("a", "b"), new TestDescription("c", "d")};
        boolean[] didPass = {true, false};
        setRunListenerChecks(1, 1000);
        setTestIdChecks(ids, didPass);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseSingleTestUnexpectedSuccess() throws Exception {
        String[] output = {
                "b (a) ... unexpected success",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "OK (unexpected success=1)",
        };
        TestDescription[] ids = {new TestDescription("a", "b")};
        boolean[] didPass = {false};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(1, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseSingleTestSkipped() throws Exception {
        String[] output = {
                "b (a) ... skipped 'reason foo'",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "OK (skipped=1)",
        };
        TestDescription[] ids = {new TestDescription("a", "b")};
        boolean[] didPass = {false};
        boolean[] didSkip = {true};
        setTestIdChecks(ids, didPass, didSkip);
        setRunListenerChecks(1, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseSingleTestPassWithDocString() throws Exception {
        String[] output = {
                "b (a)",
                "doc string foo bar ... ok",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "OK",
        };
        TestDescription[] ids = {new TestDescription("a", "b")};
        boolean[] didPass = {true};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(1, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseSingleTestFailWithDocString() throws Exception {
        String[] output = {
                "b (a)",
                "doc string foo bar ... ERROR",
                "",
                PythonUnitTestResultParser.EQUAL_LINE,
                "ERROR: b (a)",
                "doc string foo bar",
                PythonUnitTestResultParser.DASH_LINE,
                "Traceback (most recent call last):",
                "  File \"test_rangelib.py\", line 129, in test_reallyfail",
                "    raise ValueError()",
                "ValueError",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "FAILED (errors=1)"
        };
        TestDescription[] ids = {new TestDescription("a", "b")};
        boolean[] didPass = {false};
        setTestIdChecks(ids, didPass);
        setRunListenerChecks(1, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseOneWithEverything() throws Exception {
        String[] output = {
                "testError (foo.testFoo) ... ERROR",
                "testExpectedFailure (foo.testFoo) ... expected failure",
                "testFail (foo.testFoo) ... FAIL",
                "testFailWithDocString (foo.testFoo)",
                "foo bar ... FAIL",
                "testOk (foo.testFoo) ... ok",
                "testOkWithDocString (foo.testFoo)",
                "foo bar ... ok",
                "testSkipped (foo.testFoo) ... skipped 'reason foo'",
                "testUnexpectedSuccess (foo.testFoo) ... unexpected success",
                "",
                PythonUnitTestResultParser.EQUAL_LINE,
                "ERROR: testError (foo.testFoo)",
                PythonUnitTestResultParser.DASH_LINE,
                "Traceback (most recent call last):",
                "File \"foo.py\", line 11, in testError",
                "self.assertEqual(2+2, 5/0)",
                "ZeroDivisionError: integer division or modulo by zero",
                "",
                PythonUnitTestResultParser.EQUAL_LINE,
                "FAIL: testFail (foo.testFoo)",
                PythonUnitTestResultParser.DASH_LINE,
                "Traceback (most recent call last):",
                "File \"foo.py\", line 8, in testFail",
                "self.assertEqual(2+2, 5)",
                "AssertionError: 4 != 5",
                "",
                PythonUnitTestResultParser.EQUAL_LINE,
                "FAIL: testFailWithDocString (foo.testFoo)",
                "foo bar",
                PythonUnitTestResultParser.DASH_LINE,
                "Traceback (most recent call last):",
                "File \"foo.py\", line 8, in testFail",
                "self.assertEqual(2+2, 5)",
                "AssertionError: 4 != 5",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 8 tests in 1s",
                "",
                "FAILED (failures=2, errors=1, skipped=1, expected failures=1, unexpected successes=1)",
        };
        TestDescription[] ids = {
            new TestDescription("foo.testFoo", "testError"),
            new TestDescription("foo.testFoo", "testExpectedFailure"),
            new TestDescription("foo.testFoo", "testFail"),
            new TestDescription("foo.testFoo", "testFailWithDocString"),
            new TestDescription("foo.testFoo", "testOk"),
            new TestDescription("foo.testFoo", "testOkWithDocString"),
            new TestDescription("foo.testFoo", "testSkipped"),
            new TestDescription("foo.testFoo", "testUnexpectedSuccess")
        };
        boolean[] didPass = {false, true, false, false, true, true, false, false};
        boolean[] didSkip = {false, false, false, false, false, false, true, false};
        setTestIdChecks(ids, didPass, didSkip);
        setRunListenerChecks(8, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testCaptureMultilineTraceback() {
        String[] output = {
                "b (a) ... ERROR",
                "",
                PythonUnitTestResultParser.EQUAL_LINE,
                "ERROR: b (a)",
                PythonUnitTestResultParser.DASH_LINE,
                "Traceback (most recent call last):",
                "  File \"test_rangelib.py\", line 129, in test_reallyfail",
                "    raise ValueError()",
                "ValueError",
                "",
                PythonUnitTestResultParser.DASH_LINE,
                "Ran 1 test in 1s",
                "",
                "FAILED (errors=1)"
        };
        String[] tracebackLines = Arrays.copyOfRange(output, 5, 10);
        String expectedTrackback = String.join(System.lineSeparator(), tracebackLines);

        mMockListener.testStarted(anyObject());
        expectLastCall().times(1);
        mMockListener.testFailed(anyObject(), eq(expectedTrackback));
        expectLastCall().times(1);
        mMockListener.testEnded(anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        expectLastCall().times(1);
        setRunListenerChecks(1, 1000);

        replay(mMockListener);
        mParser.processNewLines(output);
        verify(mMockListener);
    }

    @Test
    public void testParseRealOutput() {
        String[] contents = readInFile(PYTHON_OUTPUT_FILE_1);

        mMockListener.testRunStarted("test", 11);
        for (int i = 0; i < 11; i++) {
            mMockListener.testStarted(EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        }

        mMockListener.testFailed(
                EasyMock.eq(new TestDescription("__main__.DisconnectionTest", "test_disconnect")),
                (String) EasyMock.anyObject());
        mMockListener.testFailed(
                EasyMock.eq(new TestDescription("__main__.EmulatorTest", "test_emulator_connect")),
                (String) EasyMock.anyObject());
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.PowerTest", "test_resume_usb_kick")));
        // Multi-line error
        mMockListener.testFailed(
                EasyMock.eq(new TestDescription("__main__.ServerTest", "test_handle_inheritance")),
                (String) EasyMock.anyObject());

        mMockListener.testRunEnded(10314, new HashMap<String, Metric>());
        replay(mMockListener);
        mParser.processNewLines(contents);
        verify(mMockListener);
    }

    private void setRunListenerChecks(int numTests, long time) {
        mMockListener.testRunStarted("test", numTests);
        expectLastCall().times(1);
        mMockListener.testRunEnded(time, new HashMap<String, Metric>());
        expectLastCall().times(1);
    }

    /** Test another output starting by a warning */
    @Test
    public void testParseRealOutput2() {
        String[] contents = readInFile(PYTHON_OUTPUT_FILE_2);
        mMockListener.testRunStarted("test", 107);
        for (int i = 0; i < 107; i++) {
            mMockListener.testStarted(EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        }
        mMockListener.testRunEnded(295, new HashMap<String, Metric>());
        replay(mMockListener);
        mParser.processNewLines(contents);
        verify(mMockListener);
    }

    @Test
    public void testParseRealOutput3() {
        String[] contents = readInFile(PYTHON_OUTPUT_FILE_3);

        mMockListener.testRunStarted("test", 11);
        for (int i = 0; i < 11; i++) {
            mMockListener.testStarted(EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        }

        mMockListener.testFailed(
                EasyMock.eq(new TestDescription("__main__.ConnectionTest", "test_reconnect")),
                (String) EasyMock.anyObject());
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.PowerTest", "test_resume_usb_kick")));

        mMockListener.testRunEnded(27353, new HashMap<String, Metric>());
        replay(mMockListener);
        mParser.processNewLines(contents);
        verify(mMockListener);
    }

    @Test
    public void testParseSubtestOutput() {
        String[] contents = readInFile("python_subtest_output.txt");
        int totalTestCount = 4;

        mMockListener.testRunStarted("test", totalTestCount);
        for (int i = 0; i < totalTestCount; i++) {
            mMockListener.testStarted(EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        }

        mMockListener.testFailed(
                EasyMock.eq(
                        new TestDescription(
                                "__main__.ExampleTest", "test_with_some_failing_subtests")),
                (String) EasyMock.anyObject());
        mMockListener.testFailed(
                EasyMock.eq(
                        new TestDescription(
                                "__main__.ExampleTest", "test_with_more_failing_subtests")),
                (String) EasyMock.anyObject());

        mMockListener.testRunEnded(1, new HashMap<String, Metric>());
        replay(mMockListener);
        mParser.processNewLines(contents);
        verify(mMockListener);
    }

    @Test
    public void testParseTestResults_withIncludeFilters() {
        Set<String> includeFilters = new LinkedHashSet<>();
        Set<String> excludeFilters = new LinkedHashSet<>();
        includeFilters.add("__main__.ConnectionTest#test_connect_ipv4_ipv6");
        includeFilters.add("__main__.EmulatorTest");
        mParser =
                new PythonUnitTestResultParser(
                        ArrayUtil.list(mMockListener), "test", includeFilters, excludeFilters);

        String[] contents = readInFile(PYTHON_OUTPUT_FILE_1);

        mMockListener.testRunStarted("test", 11);
        for (int i = 0; i < 11; i++) {
            mMockListener.testStarted(EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        }

        mMockListener.testFailed(
                EasyMock.eq(new TestDescription("__main__.EmulatorTest", "test_emulator_connect")),
                (String) EasyMock.anyObject());
        // Passed/failed tests are ignored due to include-filter setting.
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.CommandlineTest", "test_help")));
        mMockListener.testIgnored(
                EasyMock.eq(
                        new TestDescription(
                                "__main__.CommandlineTest", "test_tcpip_error_messages")));
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.CommandlineTest", "test_version")));
        mMockListener.testIgnored(
                EasyMock.eq(
                        new TestDescription("__main__.ConnectionTest", "test_already_connected")));
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.ConnectionTest", "test_reconnect")));
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.DisconnectionTest", "test_disconnect")));
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.PowerTest", "test_resume_usb_kick")));
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.ServerTest", "test_handle_inheritance")));

        mMockListener.testRunEnded(10314, new HashMap<String, Metric>());
        replay(mMockListener);
        mParser.processNewLines(contents);
        verify(mMockListener);
    }

    @Test
    public void testParseTestResults_withExcludeFilters() {
        Set<String> includeFilters = new LinkedHashSet<>();
        Set<String> excludeFilters = new LinkedHashSet<>();
        excludeFilters.add("__main__.ConnectionTest#test_connect_ipv4_ipv6");
        excludeFilters.add("__main__.EmulatorTest");
        mParser =
                new PythonUnitTestResultParser(
                        ArrayUtil.list(mMockListener), "test", includeFilters, excludeFilters);

        String[] contents = readInFile(PYTHON_OUTPUT_FILE_1);

        mMockListener.testRunStarted("test", 11);
        for (int i = 0; i < 11; i++) {
            mMockListener.testStarted(EasyMock.anyObject());
            mMockListener.testEnded(
                    EasyMock.anyObject(), EasyMock.<HashMap<String, Metric>>anyObject());
        }

        // Passed/failed tests are ignored due to exclude-filter setting.
        mMockListener.testIgnored(
                EasyMock.eq(
                        new TestDescription("__main__.ConnectionTest", "test_connect_ipv4_ipv6")));
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.EmulatorTest", "test_emu_kill")));
        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.EmulatorTest", "test_emulator_connect")));

        mMockListener.testIgnored(
                EasyMock.eq(new TestDescription("__main__.PowerTest", "test_resume_usb_kick")));

        mMockListener.testFailed(
                EasyMock.eq(new TestDescription("__main__.DisconnectionTest", "test_disconnect")),
                (String) EasyMock.anyObject());
        mMockListener.testFailed(
                EasyMock.eq(new TestDescription("__main__.ServerTest", "test_handle_inheritance")),
                (String) EasyMock.anyObject());

        mMockListener.testRunEnded(10314, new HashMap<String, Metric>());
        replay(mMockListener);
        mParser.processNewLines(contents);
        verify(mMockListener);
    }

    private void setTestIdChecks(TestDescription[] ids, boolean[] didPass) {
        for (int i = 0; i < ids.length; i++) {
            mMockListener.testStarted(ids[i]);
            expectLastCall().times(1);
            if (didPass[i]) {
                mMockListener.testEnded(ids[i], new HashMap<String, Metric>());
                expectLastCall().times(1);
                mMockListener.testFailed(eq(ids[i]), (String)anyObject());
                expectLastCall().andThrow(new AssertionFailedError()).anyTimes();
            } else {
                mMockListener.testFailed(eq(ids[i]), (String)anyObject());
                expectLastCall().times(1);
                mMockListener.testEnded(ids[i], new HashMap<String, Metric>());
                expectLastCall().times(1);
            }
        }
    }

    private void setTestIdChecks(TestDescription[] ids, boolean[] didPass, boolean[] didSkip) {
        for (int i = 0; i < ids.length; i++) {
            mMockListener.testStarted(ids[i]);
            expectLastCall().times(1);
            if (didPass[i]) {
                mMockListener.testEnded(ids[i], new HashMap<String, Metric>());
                expectLastCall().times(1);
                mMockListener.testFailed(eq(ids[i]), (String) anyObject());
                expectLastCall().andThrow(new AssertionFailedError()).anyTimes();
            } else if (didSkip[i]) {
                mMockListener.testIgnored(ids[i]);
                expectLastCall().times(1);
                mMockListener.testEnded(ids[i], new HashMap<String, Metric>());
                expectLastCall().times(1);
            } else {
                mMockListener.testFailed(eq(ids[i]), (String)anyObject());
                expectLastCall().times(1);
                mMockListener.testEnded(ids[i], new HashMap<String, Metric>());
                expectLastCall().times(1);
            }
        }
    }

    /**
     * Helper to read a file from the res/testtype directory and return its contents as a String[]
     *
     * @param filename the name of the file (without the extension) in the res/testtype directory
     * @return a String[] of the
     */
    private String[] readInFile(String filename) {
        Vector<String> fileContents = new Vector<String>();
        try {
            InputStream gtestResultStream1 =
                    getClass()
                            .getResourceAsStream(
                                    File.separator + "testtype" + File.separator + filename);
            BufferedReader reader = new BufferedReader(new InputStreamReader(gtestResultStream1));
            String line = null;
            while ((line = reader.readLine()) != null) {
                fileContents.add(line);
            }
        } catch (NullPointerException e) {
            CLog.e("Gest output file does not exist: " + filename);
        } catch (IOException e) {
            CLog.e("Unable to read contents of gtest output file: " + filename);
        }
        return fileContents.toArray(new String[fileContents.size()]);
    }
}

