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
package android.device.collectors;

import android.app.Instrumentation;
import android.device.collectors.util.SendToInstrumentation;
import android.os.Bundle;
import android.os.Environment;
import android.os.SystemClock;
import android.util.Log;

import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mockito;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.text.ParsePosition;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

/** Unit tests for {@link LogcatCollector}. */
@RunWith(AndroidJUnit4.class)
public final class LogcatCollectorTest {

    // A {@code Description} to pass when faking a test run start call.
    private static final Description RUN_DESCRIPTION = Description.createSuiteDescription("run");
    private static final Description TEST_DESCRIPTION =
            Description.createTestDescription("run", "test");
    private static final Description TEST_2_DESCRIPTION =
            Description.createTestDescription("run", "test-2");
    private static final Description TEST_FAILURE_DESCRIPTION =
            Description.createTestDescription("run", "test-failed");
    // A template for a logcat line for the purpose of this test. The three parameters are type (
    // info, warning, etc.), log tag and message.
    private static final String LOGCAT_REGEX_TEMPLATE =
            "^\\d{2}-\\d{2}\\s+\\d{2}:\\d{2}:\\d{2}\\.\\d{3}[\\s\\d]+%s\\s+%s\\s*:\\s+%s$";

    // A version of the date formatter used in the actual collector with the year added in, as the
    // logcat itself does not include year by default, and it is better not to enable it there
    // as it will just end up being visual noise for the user.
    private static final SimpleDateFormat DATE_FORMATTER =
            new SimpleDateFormat("yyyy " + LogcatCollector.DATE_FORMATTER.toPattern());
    private static final String RUN_METRIC_KEY = "android.device.collectors.LogcatCollector";

    private File mLogDir;
    private LogcatCollector mCollector;
    private Instrumentation mMockInstrumentation;

    @Before
    public void setUp() throws Exception {
        mMockInstrumentation = Mockito.mock(Instrumentation.class);
        mLogDir = new File(Environment.getExternalStorageDirectory(), "test_logcat");
        mLogDir.mkdirs();
    }

    @After
    public void tearDown() {
        mCollector.recursiveDelete(mLogDir);
    }

    private LogcatCollector initListener(Bundle bundle) {
        mCollector = new LogcatCollector(bundle);
        LogcatCollector listener = Mockito.spy(mCollector);
        listener.setInstrumentation(mMockInstrumentation);
        Mockito.doReturn(mLogDir).when(listener).createAndEmptyDirectory(Mockito.anyString());
        return listener;
    }

    @Test
    public void testLogcatCollectionOnEveryTestEnd() throws Exception {
        LogcatCollector listener = initListener(new Bundle());
        Mockito.doNothing()
                .when(listener)
                .getLogcatSince(Mockito.any(String.class), Mockito.any(File.class));
        // Test run start behavior
        listener.testRunStarted(RUN_DESCRIPTION);

        // Test 1 start behavior
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);

        // Test 2 test start behavior
        listener.testStarted(TEST_2_DESCRIPTION);
        listener.testFinished(TEST_2_DESCRIPTION);

        listener.testRunFinished(new Result());
        // AJUR runner is then gonna call instrumentationRunFinished

        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation, Mockito.times(2))
                .sendStatus(Mockito.eq(
                        SendToInstrumentation.INST_STATUS_IN_PROGRESS), capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        assertEquals(2, capturedBundle.size());

        Bundle check1 = capturedBundle.get(0);
        // Ensure we received the file
        assertEquals(1, check1.size());
        // The only key is ours
        for (String key : check1.keySet()) {
            assertTrue(
                    key.contains(
                            String.join(
                                    "",
                                    "run.test",
                                    LogcatCollector.METRIC_SEP
                                            + LogcatCollector.FILENAME_SUFFIX,
                                    "-logcat.txt")));

        }

        Bundle check2 = capturedBundle.get(1);
        // Ensure we received the file
        assertEquals(1, check2.size());
        // The only key is ours
        for (String key : check2.keySet()) {
            assertTrue(
                    key.contains(
                            String.join(
                                    "",
                                    "run.test-2",
                                    LogcatCollector.METRIC_SEP
                                            + LogcatCollector.FILENAME_SUFFIX,
                                    "-logcat.txt")));

        }
    }

    @Test
    public void testLogcatCollectionWithBeforeDuration() throws Exception {

        Bundle bundle = new Bundle();
        bundle.putString(LogcatCollector.BEFORE_LOGCAT_DURATION_SECS, "6");
        LogcatCollector listener = initListener(bundle);

        ArgumentCaptor<String> captureFinalTime = ArgumentCaptor.forClass(String.class);
        Mockito.doNothing()
                .when(listener)
                .getLogcatSince(captureFinalTime.capture(), Mockito.any(File.class));

        String logTag = this.getClass().getSimpleName() + "_testLogcatCollectionWithBeforeDuration";
        Log.i(logTag, "Sample Message");

        // Log three lines after a short delay.
        SystemClock.sleep(4000);

        // Test run start behavior
        listener.testRunStarted(RUN_DESCRIPTION);
        Date date = new Date(System.currentTimeMillis());

        // Test test start behavior
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);
        listener.testRunFinished(new Result());
        // AJUR runner is then gonna call instrumentationRunFinished

        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation)
                .sendStatus(Mockito.eq(
                        SendToInstrumentation.INST_STATUS_IN_PROGRESS), capture.capture());

        // Verify logcat since time is 60 secs before the test started time.
        List<String> capturedList = captureFinalTime.getAllValues();
        assertEquals(1, capturedList.size());
        int year = Calendar.getInstance().get(Calendar.YEAR);
        String finalDateWithYear = year + " " + capturedList.get(0);
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyy MM-dd HH:mm:ss.SSS");
        Date finallogcatSinceDate = dateFormat.parse(finalDateWithYear);
        Log.i("Time Difference in msecs",
                String.valueOf(date.getTime() - finallogcatSinceDate.getTime()));
        assertTrue((date.getTime() - finallogcatSinceDate.getTime()) >= 4000);
    }

    @Test
    public void testLogcatCollectionWithDirectoryOption() throws Exception {

        Bundle bundle = new Bundle();
        bundle.putString(LogcatCollector.RETURN_LOGCAT_DIR, "true");
        LogcatCollector listener = initListener(bundle);

        Mockito.doNothing()
                .when(listener)
                .getLogcatSince(Mockito.any(String.class), Mockito.any(File.class));
        // Test run start behavior
        listener.testRunStarted(RUN_DESCRIPTION);

        // Test test start behavior
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);

        // Test test start behavior
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);
        listener.testRunFinished(new Result());

        // Results available in run metrics.
        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(1, resultBundle.size());

        // The only key is ours
        for (String key : resultBundle.keySet()) {
            assertTrue(key.contains(RUN_METRIC_KEY));
        }

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation, Mockito.times(0))
                .sendStatus(Mockito.eq(
                        SendToInstrumentation.INST_STATUS_IN_PROGRESS), capture.capture());
    }

    @Test
    public void testLogcatOnlyOnTestFailureOption() throws Exception {

        Bundle bundle = new Bundle();
        bundle.putString(LogcatCollector.COLLECT_ON_FAILURE_ONLY, "true");
        LogcatCollector listener = initListener(bundle);

        Mockito.doNothing()
                .when(listener)
                .getLogcatSince(Mockito.any(String.class), Mockito.any(File.class));
        // Test run start behavior
        listener.testRunStarted(RUN_DESCRIPTION);

        // Successful test
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);

        // Failed test.
        listener.testStarted(TEST_FAILURE_DESCRIPTION);
        Failure f = new Failure(TEST_FAILURE_DESCRIPTION, new RuntimeException("I failed."));
        listener.testFailure(f);
        listener.testFinished(TEST_FAILURE_DESCRIPTION);

        listener.testRunFinished(new Result());
        // AJUR runner is then gonna call instrumentationRunFinished
        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation)
                .sendStatus(Mockito.eq(
                        SendToInstrumentation.INST_STATUS_IN_PROGRESS), capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        assertEquals(1, capturedBundle.size());
        Bundle check = capturedBundle.get(0);
        // Ensure we received the file
        assertEquals(1, check.size());
        // The only key is ours
        for (String key: check.keySet()) {
            assertTrue(
                    key.contains(
                            String.join(
                                    "",
                                    "run.test-failed",
                                    LogcatCollector.METRIC_SEP
                                            + LogcatCollector.FILENAME_SUFFIX,
                                    "-logcat.txt")));
        }
    }

    /** Test that the collector can actually retrieve logcat. */
    @Test
    public void testRetrievingLogcat() throws Exception {
        LogcatCollector listener = initListener(new Bundle());
        // Test run start behavior
        listener.testRunStarted(RUN_DESCRIPTION);

        Description testDescription = Description.createTestDescription("run", "test_logcat");
        // Test test start behavior
        listener.testStarted(testDescription);
        // Log three lines after a short delay.
        SystemClock.sleep(10);
        String logTag = this.getClass().getSimpleName() + "_testRetrievingLogcat";
        Log.i(logTag, "Message 1");
        Log.w(logTag, "Message 2");
        Log.e(logTag, "Message 3");
        SystemClock.sleep(10);
        listener.testFinished(testDescription);
        listener.testRunFinished(new Result());
        // AJUR runner is then gonna call instrumentationRunFinished
        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation)
                .sendStatus(
                        Mockito.eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS),
                        capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        assertEquals(1, capturedBundle.size());
        Bundle check = capturedBundle.get(0);
        // Ensure we received the file
        assertEquals(1, check.size());
        String logPath = null;
        for (String key : check.keySet()) {
            if (key.contains("run.test_logcat")) {
                logPath = check.getString(key);
            }
        }
        assertTrue(logPath != null);

        String message1Regex = String.format(LOGCAT_REGEX_TEMPLATE, "I", logTag, "Message 1");
        String message2Regex = String.format(LOGCAT_REGEX_TEMPLATE, "W", logTag, "Message 2");
        String message3Regex = String.format(LOGCAT_REGEX_TEMPLATE, "E", logTag, "Message 3");
        // Read the metric file and check that the lines logged in this test are present.
        boolean hasMessage1 = false;
        boolean hasMessage2 = false;
        boolean hasMessage3 = false;
        try (BufferedReader reader = new BufferedReader(new FileReader(new File(logPath)))) {
            for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                if (line.matches(message1Regex)) {
                    hasMessage1 = true;
                }
                if (line.matches(message2Regex)) {
                    hasMessage2 = true;
                }
                if (line.matches(message3Regex)) {
                    hasMessage3 = true;
                }
            }
            assertTrue(hasMessage1);
            assertTrue(hasMessage2);
            assertTrue(hasMessage3);
        }
    }

    /** Test that the logcat retrieved are within the time bounds of the test. */
    @Ignore
    @Test
    public void testLogcatTimespan() throws Exception {
        LogcatCollector listener = initListener(new Bundle());

        listener.testRunStarted(RUN_DESCRIPTION);
        // Store the start time of the test. The logcat should begin after this time.
        long startTimeMillis = System.currentTimeMillis();
        Description testDescription = Description.createTestDescription("run", "test_logcat_time");
        // Test test start behavior
        listener.testStarted(testDescription);
        // Log three lines after a short delay.
        SystemClock.sleep(10);
        String logTag = this.getClass().getSimpleName() + "_testRetrievingLogcat";
        Log.i(logTag, "Message");
        Log.i(logTag, "Another message");
        SystemClock.sleep(10);
        // Store the end time of the test. The logcat should end before this time.
        long endTimeMillis = System.currentTimeMillis();
        listener.testRunFinished(new Result());

        // AJUR runner is then gonna call instrumentationRunFinished
        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation)
                .sendStatus(
                        Mockito.eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS),
                        capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        assertEquals(1, capturedBundle.size());
        Bundle check = capturedBundle.get(0);
        // Ensure we received the file
        assertEquals(1, check.size());
        String logPath = null;
        for (String key : check.keySet()) {
            if (key.contains("run.test_logcat")) {
                logPath = check.getString(key);
            }
        }
        assertTrue(logPath != null);

        // A regex to match any logcat line for the purpose of this test.
        String logcatLineRegex = String.format(LOGCAT_REGEX_TEMPLATE, "[A-Z]", ".*", ".*");
        String year = String.format("%04d", Calendar.getInstance().get(Calendar.YEAR));
        String startTime = null;
        String endTime = null;
        try (BufferedReader reader = new BufferedReader(new FileReader(new File(logPath)))) {
            String firstLine = null;
            String lastLine = null;
            for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                // Skip the "--- beginning of..." lines.
                if (line.matches("^-+\\s+beginning of.*$")) {
                    continue;
                }
                if (firstLine == null) {
                    firstLine = line;
                }
                lastLine = line;
            }
            // These lines should be present.
            assertNotNull(firstLine);
            assertNotNull(lastLine);
            // These lines should be logcat lines.
            assertTrue(firstLine.matches(logcatLineRegex));
            assertTrue(lastLine.matches(logcatLineRegex));
            // Parse the timestamp at the start of the logcat lines. They should fall within the
            // start and end time recorded in the test.
            // Note that it is necessary to prepend the year to the logcat lines or the dates will
            // end up in 1970.
            long firstTimestamp =
                    DATE_FORMATTER.parse(year + " " + firstLine, new ParsePosition(0)).getTime();
            long lastTimestamp =
                    DATE_FORMATTER.parse(year + " " + lastLine, new ParsePosition(0)).getTime();
            assertTrue(firstTimestamp >= startTimeMillis);
            assertTrue(lastTimestamp <= endTimeMillis);
            assertTrue(firstTimestamp <= lastTimestamp);
        }
    }

    /** Test that the logcat collection supports multiple iterations. */
    @Test
    public void testMultipleIterations() throws Exception {
        LogcatCollector listener = initListener(new Bundle());
        Mockito.doNothing()
                .when(listener)
                .getLogcatSince(Mockito.any(String.class), Mockito.any(File.class));
        Failure f = new Failure(TEST_DESCRIPTION, new RuntimeException("I failed."));

        listener.testRunStarted(RUN_DESCRIPTION);

        // Schedule 3 identical tests.
        // Fail the first one.
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFailure(f);
        listener.testFinished(TEST_DESCRIPTION);
        // Let the second one pass.
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);
        // Fail the third one.
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFailure(f);
        listener.testFinished(TEST_DESCRIPTION);

        listener.testRunFinished(new Result());
        // AJUR runner is then gonna call instrumentationRunFinished
        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation, Mockito.times(3))
                .sendStatus(
                        Mockito.eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS),
                        capture.capture());
        List<Bundle> capturedBundles = capture.getAllValues();
        // 2 bundles as we have two tests that failed (and thus has metrics).
        assertEquals(3, capturedBundles.size());

        // The first bundle should have the first logcat file, for the first iteration.
        Bundle check1 = capturedBundles.get(0);
        assertEquals(1, check1.size());
        String expectedKey1 =
                String.join(
                        "",
                        "run.test",
                        LogcatCollector.METRIC_SEP
                                + LogcatCollector.FILENAME_SUFFIX,
                        "-logcat.txt");
        for (String key : check1.keySet()) {
            // The first iteration should not have an iteration number.
            assertTrue(key.contains(expectedKey1));
        }

        // The second bundle should have the second logcat file, for the second iteration.
        Bundle check2 = capturedBundles.get(1);
        assertEquals(1, check2.size());
        String expectedKey2 =
                String.join(
                        "",
                        "run.test-2",
                        LogcatCollector.METRIC_SEP
                                + LogcatCollector.FILENAME_SUFFIX,
                        "-logcat.txt");
        for (String key : check2.keySet()) {
            // The third iteration should have an iteration number, 3.
            assertTrue(key.contains(expectedKey2));
        }

        // The second bundle should have the second logcat file, for the third iteration.
        Bundle check3 = capturedBundles.get(2);
        assertEquals(1, check3.size());
        String expectedKey3 =
                String.join(
                        "",
                        "run.test-3",
                        LogcatCollector.METRIC_SEP
                                + LogcatCollector.FILENAME_SUFFIX,
                        "-logcat.txt");
        for (String key : check3.keySet()) {
            // The third iteration should have an iteration number, 3.
            assertTrue(key.contains(expectedKey3));
        }
    }
}
