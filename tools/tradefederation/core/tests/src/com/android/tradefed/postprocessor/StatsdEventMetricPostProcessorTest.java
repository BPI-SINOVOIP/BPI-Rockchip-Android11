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
package com.android.tradefed.postprocessor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.AttributionNode;
import com.android.os.AtomsProto.AppCrashOccurred;
import com.android.os.AtomsProto.AppStartOccurred;
import com.android.os.AtomsProto.BleScanStateChanged;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsLogReport;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link StatsdEventMetricPostProcessor}. */
@RunWith(JUnit4.class)
public class StatsdEventMetricPostProcessorTest {
    @Rule public TemporaryFolder testDir = new TemporaryFolder();

    @Mock private ITestInvocationListener mListener;
    private StatsdEventMetricPostProcessor mProcessor;
    private OptionSetter mOptionSetter;

    private Map<String, LogFile> mRunLogs;
    private static final String REPORT_PREFIX = "statsd-metric";

    // A few constants to enhance readbility of the value checking of test data.
    private static final String STARTUP_PKG_1 = "startup.package.1";
    private static final int STARTUP_MILLIS_1 = 11;
    private static final String STARTUP_PKG_2 = "startup.package.2";
    private static final int STARTUP_MILLIS_2 = 22;
    private static final String CRASH_PKG = "crash.package";
    private static final long CRASH_NANOS = 333;
    private static final int ATTRIBUTION_NODE_UID_1 = 1;
    private static final int ATTRIBUTION_NODE_UID_2 = 2;

    @Before
    public void setUp() throws IOException, ConfigurationException {
        initMocks(this);
        mProcessor = new StatsdEventMetricPostProcessor();
        mProcessor.init(mListener);
        mOptionSetter = new OptionSetter(mProcessor);

        String reportFilename = "report.pb";
        File reportFile = testDir.newFile(reportFilename);
        Files.write(reportFile.toPath(), generateTestProto().toByteArray());

        // Set up the log files and related option for parsing; only the actual parsing is tested
        // for this class as the logic around getting to the parsing stage has been tested in
        // StatsdGenericPostProcessor.
        mOptionSetter.setOptionValue("statsd-report-data-prefix", REPORT_PREFIX);
        mRunLogs = new HashMap<>();
        mRunLogs.put(
                REPORT_PREFIX + "report",
                new LogFile(reportFile.getAbsolutePath(), "some.url", LogDataType.PB));
    }

    /** Test the simple case of formatting one atom. */
    @Test
    public void testFormattingSingleAtom() throws ConfigurationException {
        mOptionSetter.setOptionValue(
                "metric-formatter", "app_crash_occurred", "crash=[package_name]");
        mOptionSetter.setOptionValue(
                "metric-formatter",
                "app_crash_occurred",
                "crash_[foreground_state]=[package_name]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        String crashKey =
                String.join(StatsdEventMetricPostProcessor.METRIC_SEP, REPORT_PREFIX, "crash");
        assertTrue(parsedMetrics.containsKey(crashKey));
        assertEquals(
                CRASH_PKG, parsedMetrics.get(crashKey).build().getMeasurements().getSingleString());
        String crashWithForegroundStateKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP, REPORT_PREFIX, "crash_UNKNOWN");
        assertTrue(parsedMetrics.containsKey(crashWithForegroundStateKey));
        assertEquals(
                CRASH_PKG,
                parsedMetrics.get(crashWithForegroundStateKey).getMeasurements().getSingleString());
    }

    /** Test the case of formatting for multiple atoms. */
    @Test
    public void testFormattingMultipleAtomTypes() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "app_crash_occurred",
                "crash_[foreground_state]=[package_name]");
        setter.setOptionValue(
                "metric-formatter", "app_start_occurred", "[pkg_name]_startup_instances=[type]");
        setter.setOptionValue(
                "metric-formatter",
                "app_start_occurred",
                "[type]_startup_[pkg_name]=[windows_drawn_delay_millis]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        // Key for app crash should exist.
        String crashWithForegroundStateKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP, REPORT_PREFIX, "crash_UNKNOWN");
        assertTrue(parsedMetrics.containsKey(crashWithForegroundStateKey));
        // Keys for app startup should exist.
        String startupInstancesKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        STARTUP_PKG_1 + "_startup_instances");
        assertTrue(parsedMetrics.containsKey(startupInstancesKey));
        String startupMillisKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        "COLD_startup_" + STARTUP_PKG_1);
        assertTrue(parsedMetrics.containsKey(startupMillisKey));
    }

    /** Test that multiple metrics from the same key result in comma-separated metric values. */
    @Test
    public void testCsvForRepeatedKeys() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "app_start_occurred",
                "[type]_startup_[pkg_name]=[windows_drawn_delay_millis]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        String startupMillisKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        "COLD_startup_" + STARTUP_PKG_1);
        assertTrue(parsedMetrics.containsKey(startupMillisKey));
        List<String> metricVals =
                Arrays.asList(
                        parsedMetrics
                                .get(startupMillisKey)
                                .getMeasurements()
                                .getSingleString()
                                .split(","));
        assertEquals(2, metricVals.size());
        assertTrue(metricVals.contains(String.valueOf(STARTUP_MILLIS_1)));
        assertTrue(metricVals.contains(String.valueOf(STARTUP_MILLIS_2)));
    }

    /** Test that the same atom type with different values create different keys. */
    @Test
    public void testNewKeysForAtomsOfSameTypeAndDifferentValues() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "app_start_occurred",
                "[type]_startup_[pkg_name]=[windows_drawn_delay_millis]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        String package1Key =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        "COLD_startup_" + STARTUP_PKG_1);
        assertTrue(parsedMetrics.containsKey(package1Key));
        String package2Key =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        "COLD_startup_" + STARTUP_PKG_2);
        assertTrue(parsedMetrics.containsKey(package2Key));
    }

    /**
     * Test that fields from the {@link EventMetricData} message can be accessed by prepending "_"
     * to the field reference.
     */
    @Test
    public void testAccessingEventMetricDataField() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "app_crash_occurred",
                "crash_[package_name]_timestamps=[_elapsed_timestamp_nanos]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        String crashTimestampKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        "crash_" + CRASH_PKG + "_timestamps");
        assertTrue(parsedMetrics.containsKey(crashTimestampKey));
        assertEquals(
                String.valueOf(CRASH_NANOS),
                parsedMetrics.get(crashTimestampKey).getMeasurements().getSingleString());
    }

    /** Test accessing repeated fields. */
    @Test
    public void testAccessingRepeatedField() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "ble_scan_state_changed",
                "ble_scan_attribution_chain=[attribution_node.uid]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        String attributionChainKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        "ble_scan_attribution_chain");
        assertTrue(parsedMetrics.containsKey(attributionChainKey));
        assertEquals(
                String.join(
                        ",",
                        String.valueOf(ATTRIBUTION_NODE_UID_1),
                        String.valueOf(ATTRIBUTION_NODE_UID_2)),
                parsedMetrics.get(attributionChainKey).getMeasurements().getSingleString());
    }

    /**
     * Test that when both the formatter key and value references repeated fields and results in
     * combinations, the metrics are discarded.
     */
    @Test
    public void testIgnoresRepeatedFieldInBothKeyAndValue() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "ble_scan_state_changed",
                "should_be_ignored_[attribution_node.uid]=[attribution_node.uid]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        assertTrue(
                "The formatter should be ignored.",
                parsedMetrics.keySet().stream().noneMatch(k -> k.contains("should_be_ignored")));
    }

    /**
     * Test that when the formatter references multiple repeated fields in key or value and results
     * in combinations the metrics are discarded.
     */
    @Test
    public void testIgnoresMultipleRepeatedFieldInKeyOrValue() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        // Multiple repeated field reference in key.
        setter.setOptionValue(
                "metric-formatter",
                "ble_scan_state_changed",
                "should_be_ignored_[attribution_node.uid]_[attribution_node.uid]=1");
        // Multiple repeated field reference in value.
        setter.setOptionValue(
                "metric-formatter",
                "ble_scan_state_changed",
                "should_be_ignored_[attribution_node.uid]_[attribution_node.uid]=1");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        assertTrue(
                "The formatters should be ignored.",
                parsedMetrics.keySet().stream().noneMatch(k -> k.contains("should_be_ignored")));
    }

    /** Test that invalid field references are ignored. */
    @Test
    public void testIgnoresInvalidFieldReference() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "app_crash_occurred",
                "should_be_ignored_[not_a_field]_timestamps=[_elapsed_timestamp_nanos]");
        setter.setOptionValue(
                "metric-formatter",
                "app_crash_occurred",
                "should_be_ignored_[package]_timestamps=[_also_not_a_field]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        assertTrue(
                "Both formatters should be ignored.",
                parsedMetrics.keySet().stream().noneMatch(k -> k.contains("should_be_ignored")));
    }

    /** Test that invalid atom references are ignored. */
    @Test
    public void testIgnoresInvalidAtomReference() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "not_an_atom",
                "should_be_ignored_[attribution_node.uid]=[attribution_node.uid]");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        assertTrue(
                "The formatter should be ignored.",
                parsedMetrics.keySet().stream().noneMatch(k -> k.contains("should_be_ignored")));
    }

    /**
     * Test accessing a nested field.
     *
     * <p>This tests achieves the purpose by accessing the "atom" field in the {@link
     * EventMetricData} message. While this use case is unlikely in the real world it will trigger
     * the same code path in the parser.
     */
    @Test
    public void testAccessingNestedField() throws ConfigurationException {
        OptionSetter setter = new OptionSetter(mProcessor);
        setter.setOptionValue(
                "metric-formatter",
                "app_crash_occurred",
                "crash_[_atom.app_crash_occurred.package_name]_timestamps=some_val");

        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), mRunLogs);
        String crashTimestampKey =
                String.join(
                        StatsdEventMetricPostProcessor.METRIC_SEP,
                        REPORT_PREFIX,
                        "crash_" + CRASH_PKG + "_timestamps");
        assertTrue(parsedMetrics.containsKey(crashTimestampKey));
    }

    /** Generates an app startup event for testing purposes. */
    private EventMetricData generateAppStartupData(String packageName, int launchMillis) {
        return EventMetricData.newBuilder()
                .setElapsedTimestampNanos(7)
                .setAtom(
                        Atom.newBuilder()
                                .setAppStartOccurred(
                                        AppStartOccurred.newBuilder()
                                                .setPkgName(packageName)
                                                .setType(AppStartOccurred.TransitionType.COLD)
                                                .setWindowsDrawnDelayMillis(launchMillis)))
                .build();
    }

    /** Generates an app crash event for testing purposes. */
    private EventMetricData generateAppCrashData() {
        return EventMetricData.newBuilder()
                .setElapsedTimestampNanos(CRASH_NANOS)
                .setAtom(
                        Atom.newBuilder()
                                .setAppCrashOccurred(
                                        AppCrashOccurred.newBuilder()
                                                .setPackageName(CRASH_PKG)
                                                .setForegroundState(
                                                        AppCrashOccurred.ForegroundState.UNKNOWN)))
                .build();
    }

    /** Generates an event with repeated fields. */
    private EventMetricData generateRepeatedFieldData() {
        return EventMetricData.newBuilder()
                .setElapsedTimestampNanos(111)
                .setAtom(
                        Atom.newBuilder()
                                .setBleScanStateChanged(
                                        BleScanStateChanged.newBuilder()
                                                .addAttributionNode(
                                                        AttributionNode.newBuilder()
                                                                .setUid(ATTRIBUTION_NODE_UID_1))
                                                .addAttributionNode(
                                                        AttributionNode.newBuilder()
                                                                .setUid(ATTRIBUTION_NODE_UID_2))))
                .build();
    }

    /** Generates a {@link StatsLogReport} from an {@link EventMetricData} instance. */
    private StatsLogReport generateStatsLogReport(EventMetricData data) {
        return StatsLogReport.newBuilder()
                .setEventMetrics(StatsLogReport.EventMetricDataWrapper.newBuilder().addData(data))
                .build();
    }

    /**
     * Generate a test {@link ConfigMetricsReportList} with two {@link Atom}s in the first report,
     * and one {@link Atom} in the second, third and fourth reports.
     */
    private ConfigMetricsReportList generateTestProto() {
        return ConfigMetricsReportList.newBuilder()
                .addReports(
                        ConfigMetricsReport.newBuilder()
                                .addMetrics(
                                        generateStatsLogReport(
                                                generateAppStartupData(
                                                        STARTUP_PKG_1, STARTUP_MILLIS_1)))
                                .addMetrics(generateStatsLogReport(generateAppCrashData())))
                .addReports(
                        ConfigMetricsReport.newBuilder()
                                .addMetrics(
                                        generateStatsLogReport(
                                                generateAppStartupData(
                                                        STARTUP_PKG_2, STARTUP_MILLIS_2))))
                .addReports(
                        ConfigMetricsReport.newBuilder()
                                .addMetrics(
                                        generateStatsLogReport(
                                                generateAppStartupData(
                                                        STARTUP_PKG_1, STARTUP_MILLIS_2))))
                .addReports(
                        ConfigMetricsReport.newBuilder()
                                .addMetrics(generateStatsLogReport(generateRepeatedFieldData())))
                .build();
    }
}
