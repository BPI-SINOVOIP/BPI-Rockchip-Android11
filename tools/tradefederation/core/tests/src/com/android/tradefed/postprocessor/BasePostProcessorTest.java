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
package com.android.tradefed.postprocessor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.tradefed.metrics.proto.MetricMeasurement.DataType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.LogSaverResultForwarder;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.collect.ListMultimap;

import org.easymock.Capture;
import org.easymock.CaptureType;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.IOException;
import java.lang.StringBuilder;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link BasePostProcessor}. */
@RunWith(JUnit4.class)
public class BasePostProcessorTest {

    private class TestablePostProcessor extends BasePostProcessor {
        public static final String FILE_PREFIX = "file-";

        @Override
        public Map<String, Metric.Builder> processTestMetricsAndLogs(
                TestDescription test,
                HashMap<String, Metric> rawMetrics,
                Map<String, LogFile> testLogs) {
            HashMap<String, Metric.Builder> newMap = new HashMap<>();
            for (String key : rawMetrics.keySet()) {
                // Change, e.g. "value" to "value2".
                Metric.Builder newBuilder = Metric.newBuilder();
                newBuilder
                        .getMeasurementsBuilder()
                        .setSingleString(
                                rawMetrics.get(key).getMeasurements().getSingleString() + "2");
                // Attempt to overwrite the original metric; should not appear in final result.
                newMap.put(key, newBuilder);
                // Write a new metric.
                newMap.put(key + "2", newBuilder);
            }
            // "Process" the log files by logging their path as a metric.
            for (String dataName : testLogs.keySet()) {
                newMap.put(
                        FILE_PREFIX + dataName,
                        TfMetricProtoUtil.stringToMetric(testLogs.get(dataName).getPath())
                                .toBuilder());
            }
            return newMap;
        }

        @Override
        public Map<String, Metric.Builder> processRunMetricsAndLogs(
                HashMap<String, Metric> rawMetrics, Map<String, LogFile> runLogs) {
            HashMap<String, Metric.Builder> newMap = new HashMap<>();
            for (String key : rawMetrics.keySet()) {
                // Change, e.g. "value" to "value2".
                Metric.Builder newBuilder = Metric.newBuilder();
                newBuilder
                        .getMeasurementsBuilder()
                        .setSingleString(
                                rawMetrics.get(key).getMeasurements().getSingleString() + "2");
                // Attempt to overwrite the original metric; should not appear in final result.
                newMap.put(key, newBuilder);
                // Write a new metric.
                newMap.put(key + "2", newBuilder);
            }
            // "Process" the log files by logging their path as a metric.
            for (String dataName : runLogs.keySet()) {
                newMap.put(
                        FILE_PREFIX + dataName,
                        TfMetricProtoUtil.stringToMetric(runLogs.get(dataName).getPath())
                                .toBuilder());
            }
            return newMap;
        }

        @Override
        public Map<String, Metric.Builder> processAllTestMetricsAndLogs(
                ListMultimap<String, Metric> allTestMetrics,
                Map<TestDescription, Map<String, LogFile>> allTestLogs) {
            HashMap<String, Metric.Builder> newMap = new HashMap<>();
            for (String key : allTestMetrics.keySet()) {
                // For test purposes we just concatenate the metric strings here.
                List<Metric> metrics = allTestMetrics.get(key);
                StringBuilder resultStringBuilder = new StringBuilder();
                for (Metric metricVal : metrics) {
                    resultStringBuilder.append(metricVal.getMeasurements().getSingleString());
                }
                Metric.Builder newBuilder = Metric.newBuilder();
                newBuilder.getMeasurementsBuilder().setSingleString(resultStringBuilder.toString());
                // Attempt to overwrite the original metric; should not appear in final result.
                newMap.put(key, newBuilder);
                // Write a new metric.
                newMap.put(key + "-agg", newBuilder);
            }
            // "Process" the log files by logging their path as a metric under a key composed from
            // the test description and data name.
            for (TestDescription test : allTestLogs.keySet()) {
                for (String dataName : allTestLogs.get(test).keySet()) {
                    newMap.put(
                            FILE_PREFIX + String.join("-", test.toString(), dataName),
                            TfMetricProtoUtil.stringToMetric(
                                            allTestLogs.get(test).get(dataName).getPath())
                                    .toBuilder());
                }
            }
            return newMap;
        }
    }

    // A few constants used for testing log file post processing.
    private static final String TEST_DATA_NAME_1 = "test-log-1";
    private static final LogFile TEST_LOG_1 =
            new LogFile("test-log-path-1", "url", LogDataType.TEXT);
    private static final String TEST_DATA_NAME_2 = "test-log-2";
    private static final LogFile TEST_LOG_2 =
            new LogFile("test-log-path-2", "url", LogDataType.TEXT);
    private static final String RUN_DATA_NAME_1 = "run-log-1";
    private static final LogFile RUN_LOG_1 = new LogFile("run-log-path-1", "url", LogDataType.TEXT);
    private static final String RUN_DATA_NAME_2 = "run-log-2";
    private static final LogFile RUN_LOG_2 = new LogFile("run-log-path-2", "url", LogDataType.TEXT);

    private BasePostProcessor mProcessor;
    private ITestInvocationListener mMockListener;

    // A mocked ILogSaver instance to simulate log saving events, used along with an acutal instance
    // of LogSaverResultForwarder to generate logAssociation() callbacks upon calls to testLog().
    private ILogSaver mMockLogSaver;

    @Before
    public void setUp() throws IOException {
        mProcessor = new TestablePostProcessor();
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);

        mMockLogSaver = EasyMock.createMock(ILogSaver.class);
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(TEST_DATA_NAME_1),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andStubReturn(TEST_LOG_1);
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(TEST_DATA_NAME_2),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andStubReturn(TEST_LOG_2);
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(RUN_DATA_NAME_1),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andStubReturn(RUN_LOG_1);
        EasyMock.expect(
                        mMockLogSaver.saveLogData(
                                EasyMock.eq(RUN_DATA_NAME_2),
                                EasyMock.anyObject(),
                                EasyMock.anyObject()))
                .andStubReturn(RUN_LOG_2);
    }

    /** Test that the run-level post processing metrics are found in the final callback. */
    @Test
    public void testRunLevelPostProcessing() {
        ITestInvocationListener listener = mProcessor.init(mMockListener);
        HashMap<String, Metric> initialMetrics = new HashMap<>();
        initialMetrics.put("test", TfMetricProtoUtil.stringToMetric("value"));

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        mMockListener.testRunEnded(EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener);
        listener.testRunEnded(0L, initialMetrics);
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> finalMetrics = capture.getValue();
        // Check that original key is still here
        assertTrue(finalMetrics.containsKey("test"));
        // Check that original key still has the original value
        assertTrue(finalMetrics.get("test").getMeasurements().getSingleString().equals("value"));
        // Check that our new metric was added
        assertTrue(finalMetrics.containsKey("test2"));
        assertEquals(DataType.PROCESSED, finalMetrics.get("test2").getType());
        assertTrue(finalMetrics.get("test2").getMeasurements().getSingleString().equals("value2"));
    }

    /** Test that metrics from run logs are found in the final callback. */
    @Test
    public void testRunLogsPostProcessing_processRunLogs() {
        // A nice mock is used here as this test involves more complex interactions with the
        // listener but only cares about a subset of it.
        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        // A LogSaverResultForwarder is used here so that testLog() calls generate logAssocation()
        // callbacks.
        LogSaverResultForwarder listener =
                new LogSaverResultForwarder(
                        mMockLogSaver, Arrays.asList(mProcessor.init(mMockListener)));

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        mMockListener.testRunEnded(EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener, mMockLogSaver);
        listener.testRunStarted("test-run", 0, 0, 0L);
        listener.testLog(
                RUN_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("run-log-1".getBytes()));
        listener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> finalMetrics = capture.getValue();
        assertTrue(finalMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_1));
        assertEquals(
                RUN_LOG_1.getPath(),
                finalMetrics
                        .get(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_1)
                        .getMeasurements()
                        .getSingleString());
    }

    /** Test that only run logs are processed in post processing. */
    @Test
    public void testRunLevelPostProcessing_processRunLogsOnly() {
        // A nice mock is used here as this test involves more complex interactions with the
        // listener but only cares about a subset of it.
        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        // A LogSaverResultForwarder is used here so that testLog() calls generate logAssocation()
        // callbacks.
        LogSaverResultForwarder listener =
                new LogSaverResultForwarder(
                        mMockLogSaver, Arrays.asList(mProcessor.init(mMockListener)));

        TestDescription test1 = new TestDescription("class", "test1");
        TestDescription test2 = new TestDescription("class", "test2");

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        mMockListener.testRunEnded(EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener, mMockLogSaver);
        // Simulate two tests that log one file each, with run-level logs in-between and after.
        listener.testRunStarted("test-run", 2, 0, 0L);
        listener.testStarted(test1);
        listener.testLog(
                TEST_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-1".getBytes()));
        listener.testEnded(test1, 0L, new HashMap<String, Metric>());
        listener.testLog(
                RUN_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("run-log-1".getBytes()));
        listener.testStarted(test2);
        listener.testLog(
                TEST_DATA_NAME_2,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-2".getBytes()));
        listener.testEnded(test2, 0L, new HashMap<String, Metric>());
        listener.testLog(
                RUN_DATA_NAME_2,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("run-log-2".getBytes()));
        listener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> finalMetrics = capture.getValue();
        // Both run-level logs should be in the metrics.
        assertTrue(finalMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_1));
        assertTrue(finalMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_2));
        // Neither of the test-level logs should be in the metrics.
        assertFalse(finalMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1));
        assertFalse(finalMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2));
    }

    /** Test that the test metrics are found in the after-test callback. */
    @Test
    public void testPerTestPostProcessing() {
        ITestInvocationListener listener = mProcessor.init(mMockListener);
        HashMap<String, Metric> initialMetrics = new HashMap<>();
        initialMetrics.put("test", TfMetricProtoUtil.stringToMetric("value"));

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener);
        listener.testEnded(null, 0L, initialMetrics);
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> processedMetrics = capture.getValue();
        // Check that original key is still here
        assertTrue(processedMetrics.containsKey("test"));
        // Check that original key still has the original value
        assertTrue(
                processedMetrics.get("test").getMeasurements().getSingleString().equals("value"));
        // Check that our new metric was added.
        assertTrue(processedMetrics.containsKey("test2"));
        assertEquals(DataType.PROCESSED, processedMetrics.get("test2").getType());
        assertTrue(
                processedMetrics.get("test2").getMeasurements().getSingleString().equals("value2"));
    }

    /** Test that the post processor processed test logs. */
    @Test
    public void testPerTestLogPostProcessing_processTestLogs() {
        // A nice mock is used here as this test involves more complex interactions with the
        // listener but only cares about a subset of it.
        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        // A LogSaverResultForwarder is used here so that testLog() calls generate logAssocation()
        // callbacks.
        LogSaverResultForwarder listener =
                new LogSaverResultForwarder(
                        mMockLogSaver, Arrays.asList(mProcessor.init(mMockListener)));

        TestDescription test = new TestDescription("class", "test");

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener, mMockLogSaver);
        // Simulate a run with two test logs.
        listener.testStarted(test);
        listener.testLog(
                TEST_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-1".getBytes()));
        listener.testLog(
                TEST_DATA_NAME_2,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-2".getBytes()));
        listener.testEnded(test, 0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> processedMetrics = capture.getValue();
        // Check that the both test logs end up being in the metrics.
        assertTrue(
                processedMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1));
        assertEquals(
                TEST_LOG_1.getPath(),
                processedMetrics
                        .get(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1)
                        .getMeasurements()
                        .getSingleString());
        assertTrue(
                processedMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2));
        assertEquals(
                TEST_LOG_2.getPath(),
                processedMetrics
                        .get(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2)
                        .getMeasurements()
                        .getSingleString());
    }

    /** Test that the post processor only exposes the test logs to the per-test processing. */
    @Test
    public void testPerTestLogPostProcessing_processTestLogsOnly() {
        // A nice mock is used here as this test involves more complex interactions with the
        // listener but only cares about a subset of it.
        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        // A LogSaverResultForwarder is used here so that testLog() calls generate logAssocation()
        // callbacks.
        LogSaverResultForwarder listener =
                new LogSaverResultForwarder(
                        mMockLogSaver, Arrays.asList(mProcessor.init(mMockListener)));

        TestDescription test = new TestDescription("class", "test");

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener, mMockLogSaver);
        // Simulate a run with one test log and one run log.
        listener.testStarted(test);
        listener.testLog(
                TEST_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log".getBytes()));
        listener.testEnded(test, 0L, new HashMap<String, Metric>());
        listener.testLog(
                RUN_DATA_NAME_1,
                LogDataType.PB,
                new ByteArrayInputStreamSource("run-log".getBytes()));
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> processedMetrics = capture.getValue();
        // Check that the test log ends up being in the metrics.
        assertTrue(
                processedMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1));
        // Check that the run log does not end up being in the metrics.
        assertFalse(
                processedMetrics.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_1));
    }

    /**
     * Test that the post processor correctly exposes test logs to the test they are collected from.
     */
    @Test
    public void testPerTestPostProcessing_logToTestAssociation() {
        // A nice mock is used here as this test involves more complex interactions with the
        // listener but only cares about a subset of it.
        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        LogSaverResultForwarder listener =
                new LogSaverResultForwarder(
                        mMockLogSaver, Arrays.asList(mProcessor.init(mMockListener)));

        TestDescription test1 = new TestDescription("class", "test1");
        TestDescription test2 = new TestDescription("class", "test2");

        Capture<HashMap<String, Metric>> capture = new Capture<>(CaptureType.ALL);
        // Two calls are expected since there are two tests.
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(capture));
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener, mMockLogSaver);
        // Simulate two tests that log one file each.
        listener.testStarted(test1);
        listener.testLog(
                TEST_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-1".getBytes()));
        listener.testEnded(test1, 0L, new HashMap<String, Metric>());
        listener.testStarted(test2);
        listener.testLog(
                TEST_DATA_NAME_2,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-2".getBytes()));
        listener.testEnded(test2, 0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener);

        // Check that the processed metrics out of te first test only has the first log file.
        HashMap<String, Metric> test1Metrics = capture.getValues().get(0);
        // Check that the first log file ends up being in the metrics.
        assertTrue(test1Metrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1));
        assertEquals(
                TEST_LOG_1.getPath(),
                test1Metrics
                        .get(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1)
                        .getMeasurements()
                        .getSingleString());
        // Check that the second log file is not in the metrics.
        assertFalse(test1Metrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2));

        // Check that the processed metrics out of te first test only has the first log file.
        HashMap<String, Metric> test2Metrics = capture.getValues().get(1);
        // Check that the first log file ends up being in the metrics.
        assertTrue(test2Metrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2));
        assertEquals(
                TEST_LOG_2.getPath(),
                test2Metrics
                        .get(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2)
                        .getMeasurements()
                        .getSingleString());
        // Check that the second log file is not in the metrics.
        assertFalse(test2Metrics.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1));
    }

    /**
     * Test that the stored test metrics and their aggregate processed results are found in the
     * final callback.
     */
    @Test
    public void testAllTestMetricsPostProcessing() {
        ITestInvocationListener listener = mProcessor.init(mMockListener);
        HashMap<String, Metric> test1Metrics = new HashMap<>();
        test1Metrics.put("test", TfMetricProtoUtil.stringToMetric("value1"));
        HashMap<String, Metric> test2Metrics = new HashMap<>();
        test2Metrics.put("test", TfMetricProtoUtil.stringToMetric("value2"));
        HashMap<String, Metric> runMetrics = new HashMap<>();
        runMetrics.put("test", TfMetricProtoUtil.stringToMetric("should not change"));

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        // I put this dummyCapture in since I can't specify a matcher for HashMap<String, Metric>
        // in EasyMock (not doing so causes the compiler to complain about ambiguous references).
        Capture<HashMap<String, Metric>> dummyCapture = new Capture<>();
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(dummyCapture));
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(dummyCapture));
        mMockListener.testRunEnded(EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener);
        listener.testEnded(null, 0L, test1Metrics);
        listener.testEnded(null, 0L, test2Metrics);
        listener.testRunEnded(0L, runMetrics);
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> processedMetrics = capture.getValue();
        // Check that the original run metric key is still there and
        // that it corresponds to the original value.
        assertTrue(processedMetrics.containsKey("test"));
        assertTrue(
                processedMetrics
                        .get("test")
                        .getMeasurements()
                        .getSingleString()
                        .equals("should not change"));
        // Check that the new aggregate metric was added.
        assertTrue(processedMetrics.containsKey("test-agg"));
        assertEquals(DataType.PROCESSED, processedMetrics.get("test-agg").getType());
        assertTrue(
                processedMetrics
                        .get("test-agg")
                        .getMeasurements()
                        .getSingleString()
                        .equals("value1value2"));
    }

    /** Test that the all the test logs are processed when processing across tests. */
    @Test
    public void testAllTestLogsPostProcessing() {
        // A nice mock is used here as this test involves more complex interactions with the
        // listener but only cares about a subset of it.
        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        LogSaverResultForwarder listener =
                new LogSaverResultForwarder(
                        mMockLogSaver, Arrays.asList(mProcessor.init(mMockListener)));

        TestDescription test1 = new TestDescription("class", "test1");
        TestDescription test2 = new TestDescription("class", "test2");

        Capture<HashMap<String, Metric>> capture = new Capture<>();
        mMockListener.testRunEnded(EasyMock.anyLong(), EasyMock.capture(capture));

        EasyMock.replay(mMockListener, mMockLogSaver);
        // Simulate a run with two tests that log one file each.
        listener.testRunStarted("test-run", 2, 0, 0L);
        listener.testStarted(test1);
        listener.testLog(
                TEST_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-1".getBytes()));
        listener.testEnded(test1, 0L, new HashMap<String, Metric>());
        listener.testStarted(test2);
        listener.testLog(
                TEST_DATA_NAME_2,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-2".getBytes()));
        listener.testEnded(test2, 0L, new HashMap<String, Metric>());
        listener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener);

        HashMap<String, Metric> processedMetrics = capture.getValue();
        // Check that the metrics out of the two log files are in the metrics.
        assertTrue(
                processedMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                e ->
                                        e.getKey().contains(TestablePostProcessor.FILE_PREFIX)
                                                && e.getKey().contains(test1.toString())
                                                && e.getKey().contains(TEST_DATA_NAME_1)
                                                && e.getValue()
                                                        .getMeasurements()
                                                        .getSingleString()
                                                        .equals(TEST_LOG_1.getPath())));
        assertTrue(
                processedMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                e ->
                                        e.getKey().contains(TestablePostProcessor.FILE_PREFIX)
                                                && e.getKey().contains(test2.toString())
                                                && e.getKey().contains(TEST_DATA_NAME_2)
                                                && e.getValue()
                                                        .getMeasurements()
                                                        .getSingleString()
                                                        .equals(TEST_LOG_2.getPath())));
    }

    /** Test that during each test run the post processor only access logs from the current run. */
    @Test
    public void testLogPostProcessingInMultipleRuns() {
        // A nice mock is used here as this test involves more complex interactions with the
        // listener but only cares about a subset of it.
        mMockListener = EasyMock.createNiceMock(ITestInvocationListener.class);
        LogSaverResultForwarder listener =
                new LogSaverResultForwarder(
                        mMockLogSaver, Arrays.asList(mProcessor.init(mMockListener)));

        TestDescription test = new TestDescription("class", "test");
        String runName = "test-run";

        Capture<HashMap<String, Metric>> testMetricsCapture = new Capture<>(CaptureType.ALL);
        Capture<HashMap<String, Metric>> runMetricsCapture = new Capture<>(CaptureType.ALL);

        // Two sets of expected captures for two test runs.
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(testMetricsCapture));
        mMockListener.testRunEnded(EasyMock.anyLong(), EasyMock.capture(runMetricsCapture));
        mMockListener.testEnded(
                EasyMock.anyObject(), EasyMock.anyLong(), EasyMock.capture(testMetricsCapture));
        mMockListener.testRunEnded(EasyMock.anyLong(), EasyMock.capture(runMetricsCapture));

        EasyMock.replay(mMockListener, mMockLogSaver);
        // Simulate a test run with two runs and one test each.
        // Run 1.
        listener.testRunStarted(runName, 1, 0, 0L);
        listener.testStarted(test);
        listener.testLog(
                TEST_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-1".getBytes()));
        listener.testEnded(test, 0L, new HashMap<String, Metric>());
        listener.testLog(
                RUN_DATA_NAME_1,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("run-log-1".getBytes()));
        listener.testRunEnded(0L, new HashMap<String, Metric>());
        // Run 2.
        listener.testRunStarted(runName, 1, 1, 0L);
        listener.testStarted(test);
        listener.testLog(
                TEST_DATA_NAME_2,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("test-log-2".getBytes()));
        listener.testEnded(test, 0L, new HashMap<String, Metric>());
        listener.testLog(
                RUN_DATA_NAME_2,
                LogDataType.TEXT,
                new ByteArrayInputStreamSource("run-log-2".getBytes()));
        listener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener);

        // Each capture should have two sets of captured values from the two runs.
        assertEquals(2, testMetricsCapture.getValues().size());
        assertEquals(2, runMetricsCapture.getValues().size());
        // Check that metrics from each run only has info from log file in that run.
        // Run 1.
        HashMap<String, Metric> testMetrics1 = testMetricsCapture.getValues().get(0);
        // Checking results of processTestMetrics().
        assertTrue(testMetrics1.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1));
        assertFalse(testMetrics1.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2));
        HashMap<String, Metric> runMetrics1 = runMetricsCapture.getValues().get(0);
        // Checking results of processRunMetrics().
        assertTrue(runMetrics1.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_1));
        assertFalse(runMetrics1.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_2));
        // Checking results of processAllTestMetrics().
        assertTrue(
                runMetrics1
                        .entrySet()
                        .stream()
                        .anyMatch(
                                e ->
                                        e.getKey().contains(TestablePostProcessor.FILE_PREFIX)
                                                && e.getKey().contains(test.toString())
                                                && e.getKey().contains(TEST_DATA_NAME_1)));
        assertTrue(
                runMetrics1
                        .entrySet()
                        .stream()
                        .noneMatch(
                                e ->
                                        e.getKey().contains(TestablePostProcessor.FILE_PREFIX)
                                                && e.getKey().contains(test.toString())
                                                && e.getKey().contains(TEST_DATA_NAME_2)));
        // Run 2.
        HashMap<String, Metric> testMetrics2 = testMetricsCapture.getValues().get(1);
        // Checking results of processTestMetrics().
        assertFalse(testMetrics2.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_1));
        assertTrue(testMetrics2.containsKey(TestablePostProcessor.FILE_PREFIX + TEST_DATA_NAME_2));
        HashMap<String, Metric> runMetrics2 = runMetricsCapture.getValues().get(1);
        // Checking results of processRunMetrics().
        assertFalse(runMetrics2.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_1));
        assertTrue(runMetrics2.containsKey(TestablePostProcessor.FILE_PREFIX + RUN_DATA_NAME_2));
        // Checking results of processAllTestMetrics().
        assertTrue(
                runMetrics2
                        .entrySet()
                        .stream()
                        .noneMatch(
                                e ->
                                        e.getKey().contains(TestablePostProcessor.FILE_PREFIX)
                                                && e.getKey().contains(test.toString())
                                                && e.getKey().contains(TEST_DATA_NAME_1)));
        assertTrue(
                runMetrics2
                        .entrySet()
                        .stream()
                        .anyMatch(
                                e ->
                                        e.getKey().contains(TestablePostProcessor.FILE_PREFIX)
                                                && e.getKey().contains(test.toString())
                                                && e.getKey().contains(TEST_DATA_NAME_2)));
    }
}
