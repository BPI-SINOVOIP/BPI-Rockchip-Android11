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
package com.android.tradefed.device.metric;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.AppCrashOccurred;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsdStatsReport;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Spy;

/** Unit tests for {@link RuntimeRestartCollector}. */
@RunWith(JUnit4.class)
public class RuntimeRestartCollectorTest {
    private static final String DEVICE_SERIAL_1 = "device_serial_1";
    private static final String DEVICE_SERIAL_2 = "device_serial_2";

    private static final long CONFIG_ID_1 = 1;
    private static final long CONFIG_ID_2 = 2;

    private static final int TIMESTAMP_1_SECS = 1554764010;
    private static final String TIMESTAMP_1_STR =
            RuntimeRestartCollector.TIME_FORMATTER.format(
                    new Date(TimeUnit.SECONDS.toMillis(TIMESTAMP_1_SECS)));
    private static final int TIMESTAMP_2_SECS = 1554764135;
    private static final String TIMESTAMP_2_STR =
            RuntimeRestartCollector.TIME_FORMATTER.format(
                    new Date(TimeUnit.SECONDS.toMillis(TIMESTAMP_2_SECS)));

    private static final long UPTIME_1_NANOS =
            TimeUnit.HOURS.toNanos(1) + TimeUnit.MINUTES.toNanos(2) + TimeUnit.SECONDS.toNanos(3);
    private static final String UPTIME_1_STR = "01:02:03";
    private static final long UPTIME_2_NANOS =
            TimeUnit.HOURS.toNanos(1) + TimeUnit.MINUTES.toNanos(4) + TimeUnit.SECONDS.toNanos(8);
    private static final String UPTIME_2_STR = "01:04:08";

    private static final EventMetricData RUNTIME_RESTART_DATA_1 =
            EventMetricData.newBuilder()
                    .setElapsedTimestampNanos(UPTIME_1_NANOS)
                    .setAtom(
                            Atom.newBuilder()
                                    .setAppCrashOccurred(
                                            AppCrashOccurred.newBuilder()
                                                    .setProcessName(
                                                            RuntimeRestartCollector
                                                                    .SYSTEM_SERVER_KEYWORD)))
                    .build();
    private static final EventMetricData RUNTIME_RESTART_DATA_2 =
            RUNTIME_RESTART_DATA_1.toBuilder().setElapsedTimestampNanos(UPTIME_2_NANOS).build();
    private static final EventMetricData NOT_RUNTIME_RESTART_DATA =
            EventMetricData.newBuilder()
                    .setElapsedTimestampNanos(111)
                    .setAtom(
                            Atom.newBuilder()
                                    .setAppCrashOccurred(
                                            AppCrashOccurred.newBuilder()
                                                    .setProcessName("not_system_server")))
                    .build();

    @Spy private RuntimeRestartCollector mCollector;
    @Mock private IInvocationContext mContext;
    @Mock private ITestInvocationListener mListener;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
    }

    /**
     * Test that the collector makes the correct calls to the statsd utilities for a single device.
     *
     * <p>During testRunStarted() it should push the config and pull the statsd metadata. During
     * testRunEnded() it should pull the event metric data, remove the statsd config, and pull the
     * statsd metadata again.
     */
    @Test
    public void testStatsdInteractions_singleDevice() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        doReturn(Arrays.asList(testDevice)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(new ArrayList<EventMetricData>())
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(StatsdStatsReport.newBuilder().build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        mCollector.init(mContext, mListener);

        mCollector.testRunStarted("test run", 1);
        verify(mCollector, times(1))
                .pushStatsConfig(
                        eq(testDevice),
                        argThat(l -> l.contains(Atom.APP_CRASH_OCCURRED_FIELD_NUMBER)));
        verify(mCollector, times(1)).getStatsdMetadata(eq(testDevice));

        mCollector.testRunEnded(0, new HashMap<String, Metric>());
        verify(mCollector, times(1)).getEventMetricData(eq(testDevice), eq(CONFIG_ID_1));
        verify(mCollector, times(1)).removeConfig(eq(testDevice), eq(CONFIG_ID_1));
        verify(mCollector, times(2)).getStatsdMetadata(eq(testDevice));
    }

    /**
     * Test that the collector makes the correct calls to the statsd utilities for multiple devices.
     *
     * <p>During testRunStarted() it should push the config and pull the statsd metadata for each
     * device. During testRunEnded() it should pull the event metric data, remove the statsd config,
     * and pull the statsd metadata again for each device.
     */
    @Test
    public void testStatsdInteractions_multiDevice() throws Exception {
        ITestDevice testDevice1 = mockTestDevice(DEVICE_SERIAL_1);
        ITestDevice testDevice2 = mockTestDevice(DEVICE_SERIAL_2);
        doReturn(Arrays.asList(testDevice1, testDevice2)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1).when(mCollector).pushStatsConfig(eq(testDevice1), any(List.class));
        doReturn(CONFIG_ID_2).when(mCollector).pushStatsConfig(eq(testDevice2), any(List.class));
        doReturn(new ArrayList<EventMetricData>())
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(StatsdStatsReport.newBuilder().build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        mCollector.init(mContext, mListener);

        mCollector.testRunStarted("test run", 1);
        verify(mCollector, times(1))
                .pushStatsConfig(
                        eq(testDevice1),
                        argThat(l -> l.contains(Atom.APP_CRASH_OCCURRED_FIELD_NUMBER)));
        verify(mCollector, times(1))
                .pushStatsConfig(
                        eq(testDevice2),
                        argThat(l -> l.contains(Atom.APP_CRASH_OCCURRED_FIELD_NUMBER)));
        verify(mCollector, times(1)).getStatsdMetadata(eq(testDevice1));
        verify(mCollector, times(1)).getStatsdMetadata(eq(testDevice2));

        mCollector.testRunEnded(0, new HashMap<String, Metric>());
        verify(mCollector, times(1)).getEventMetricData(eq(testDevice1), eq(CONFIG_ID_1));
        verify(mCollector, times(1)).getEventMetricData(eq(testDevice2), eq(CONFIG_ID_2));
        verify(mCollector, times(1)).removeConfig(eq(testDevice1), eq(CONFIG_ID_1));
        verify(mCollector, times(1)).removeConfig(eq(testDevice2), eq(CONFIG_ID_2));
        verify(mCollector, times(2)).getStatsdMetadata(eq(testDevice1));
        verify(mCollector, times(2)).getStatsdMetadata(eq(testDevice2));
    }

    /**
     * Test that the collector collects a count of zero and no timestamps when no runtime restarts
     * occur during the test run and there were no prior runtime restarts.
     */
    @Test
    public void testAddingMetrics_noRuntimeRestart_noPriorRuntimeRestart() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        doReturn(Arrays.asList(testDevice)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(new ArrayList<EventMetricData>())
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(StatsdStatsReport.newBuilder().build(), StatsdStatsReport.newBuilder().build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // A count should always be present, and should be zero in this case.
        // If a count is not present, .findFirst().get() throws and the test will fail.
        int count = getCount(runMetrics);
        Assert.assertEquals(0, count);
        // No other metric keys should be present as there is no data.
        ensureNoMetricWithKeySuffix(
                runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_SECS);
        ensureNoMetricWithKeySuffix(
                runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_FORMATTED);
        ensureNoMetricWithKeySuffix(runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_NANOS);
        ensureNoMetricWithKeySuffix(
                runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_FORMATTED);
    }

    /**
     * Test that the collector collects a count of zero and no timestamps when no runtime restarts
     * and there were prior runtime restarts.
     */
    @Test
    public void testAddingMetrics_noRuntimeRestart_withPriorRuntimeRestart() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        doReturn(Arrays.asList(testDevice)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(new ArrayList<EventMetricData>())
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(Arrays.asList(1, 2))
                                .build(),
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(Arrays.asList(1, 2))
                                .build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // A count should always be present, and should be zero in this case.
        // If a count is not present, .findFirst().get() throws and the test will fail.
        int count = getCount(runMetrics);
        Assert.assertEquals(0, count);
        // No other metric keys should be present as there is no data.
        ensureNoMetricWithKeySuffix(
                runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_SECS);
        ensureNoMetricWithKeySuffix(
                runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_FORMATTED);
        ensureNoMetricWithKeySuffix(runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_NANOS);
        ensureNoMetricWithKeySuffix(
                runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_FORMATTED);
    }

    /**
     * Test that the collector collects counts and timestamps correctly when there are runtime
     * restarts and there were no prior runtime restarts.
     */
    @Test
    public void testAddingMetrics_withRuntimeRestart_noPriorRuntimeRestart() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        doReturn(Arrays.asList(testDevice)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(Arrays.asList(RUNTIME_RESTART_DATA_1, RUNTIME_RESTART_DATA_2))
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(
                        StatsdStatsReport.newBuilder().build(),
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(
                                        Arrays.asList(TIMESTAMP_1_SECS, TIMESTAMP_2_SECS))
                                .build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // Count should be two.
        int count = getCount(runMetrics);
        Assert.assertEquals(2, count);
        // There should be two timestamps that match TIMESTAMP_1_SECS and TIMESTAMP_2_SECS
        // respectively.
        List<Integer> timestampSecs =
                getIntMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_SECS);
        Assert.assertEquals(Arrays.asList(TIMESTAMP_1_SECS, TIMESTAMP_2_SECS), timestampSecs);
        // There should be two timestamp strings that match TIMESTAMP_1_STR and TIMESTAMP_2_STR
        // respectively.
        List<String> timestampStrs =
                getStringMetricValuesByKeySuffix(
                        runMetrics,
                        RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_FORMATTED);
        Assert.assertEquals(Arrays.asList(TIMESTAMP_1_STR, TIMESTAMP_2_STR), timestampStrs);
        // There should be two uptime timestsmps that match UPTIME_1_NANOS and UPTIME_2_NANOS
        // respectively.
        List<Long> uptimeNanos =
                getLongMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_NANOS);
        Assert.assertEquals(Arrays.asList(UPTIME_1_NANOS, UPTIME_2_NANOS), uptimeNanos);
        // There should be two uptime timestamp strings that match UPTIME_1_STR and UPTIME_2_STR
        // respectively.
        List<String> uptimeStrs =
                getStringMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_FORMATTED);
        Assert.assertEquals(Arrays.asList(UPTIME_1_STR, UPTIME_2_STR), uptimeStrs);
    }

    /**
     * Test that the collector collects counts and metrics correctly when there are runtime restarts
     * and there were prior runtime restarts.
     */
    @Test
    public void testAddingMetrics_withRuntimeRestart_withPriorRuntimeRestart() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        doReturn(Arrays.asList(testDevice)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(Arrays.asList(RUNTIME_RESTART_DATA_1, RUNTIME_RESTART_DATA_2))
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(Arrays.asList(1, 2, 3))
                                .build(),
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(
                                        Arrays.asList(2, 3, TIMESTAMP_1_SECS, TIMESTAMP_2_SECS))
                                .build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // Count should be two.
        int count = getCount(runMetrics);
        Assert.assertEquals(2, count);
        // There should be two timestamps that match TIMESTAMP_1_SECS and TIMESTAMP_2_SECS
        // respectively.
        List<Integer> timestampSecs =
                getIntMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_SECS);
        Assert.assertEquals(Arrays.asList(TIMESTAMP_1_SECS, TIMESTAMP_2_SECS), timestampSecs);
        // There should be two timestamp strings that match TIMESTAMP_1_STR and TIMESTAMP_2_STR
        // respectively.
        List<String> timestampStrs =
                getStringMetricValuesByKeySuffix(
                        runMetrics,
                        RuntimeRestartCollector.METRIC_SUFFIX_SYSTEM_TIMESTAMP_FORMATTED);
        Assert.assertEquals(Arrays.asList(TIMESTAMP_1_STR, TIMESTAMP_2_STR), timestampStrs);
        // There should be two uptime timestsmps that match UPTIME_1_NANOS and UPTIME_2_NANOS
        // respectively.
        List<Long> uptimeNanos =
                getLongMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_NANOS);
        Assert.assertEquals(Arrays.asList(UPTIME_1_NANOS, UPTIME_2_NANOS), uptimeNanos);
        // There should be two uptime timestamp strings that match UPTIME_1_STR and UPTIME_2_STR
        // respectively.
        List<String> uptimeStrs =
                getStringMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_FORMATTED);
        Assert.assertEquals(Arrays.asList(UPTIME_1_STR, UPTIME_2_STR), uptimeStrs);
    }

    /**
     * Test that the {@link AppCrashOccurred}-based collection only collects runtime restarts (i.e.
     * system server crashes) and not other crashes.
     */
    @Test
    public void testAddingMetrics_withRuntimeRestart_reportsSystemServerCrashesOnly()
            throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        doReturn(Arrays.asList(testDevice)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(
                        Arrays.asList(
                                RUNTIME_RESTART_DATA_1,
                                NOT_RUNTIME_RESTART_DATA,
                                RUNTIME_RESTART_DATA_2))
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(Arrays.asList(1, 2, 3))
                                .build(),
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(
                                        Arrays.asList(2, 3, TIMESTAMP_1_SECS, TIMESTAMP_2_SECS))
                                .build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // We only check for the uptime timestamps coming from the AppCrashOccurred atoms.

        // There should be two uptime timestamps that match UPTIME_1_NANOS and UPTIME_2_NANOS
        // respectively.
        List<Long> uptimeNanos =
                getLongMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_NANOS);
        Assert.assertEquals(Arrays.asList(UPTIME_1_NANOS, UPTIME_2_NANOS), uptimeNanos);
        // There should be two uptime timestamp strings that match UPTIME_1_STR and UPTIME_2_STR
        // respectively.
        List<String> uptimeStrs =
                getStringMetricValuesByKeySuffix(
                        runMetrics, RuntimeRestartCollector.METRIC_SUFFIX_UPTIME_FORMATTED);
        Assert.assertEquals(Arrays.asList(UPTIME_1_STR, UPTIME_2_STR), uptimeStrs);
    }

    /**
     * Test that the collector reports counts based on the {@link StatsdStatsReport} results when it
     * disagrees with info from the {@link AppCrashOccurred} atom.
     */
    @Test
    public void testAddingMetrics_withRuntimeRestart_useStatsdMetadataResultsForCount()
            throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        doReturn(Arrays.asList(testDevice)).when(mContext).getDevices();
        // Two data points from the AppCrashOccurred data.
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(Arrays.asList(RUNTIME_RESTART_DATA_1, RUNTIME_RESTART_DATA_2))
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        // Data from statsd metadata only has one timestamp.
        doReturn(
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(Arrays.asList())
                                .build(),
                        StatsdStatsReport.newBuilder()
                                .addAllSystemRestartSec(Arrays.asList(TIMESTAMP_1_SECS))
                                .build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // Count should be two as in the stubbed EventMetricDataResults, even though statsd metadata
        // only reported one timestamp.
        int count = getCount(runMetrics);
        Assert.assertEquals(1, count);
    }

    /**
     * Test that the device serial number is included in the metric key when multiple devices are
     * under test.
     */
    @Test
    public void testAddingMetrics_includesSerialForMultipleDevices() throws Exception {
        ITestDevice testDevice1 = mockTestDevice(DEVICE_SERIAL_1);
        ITestDevice testDevice2 = mockTestDevice(DEVICE_SERIAL_2);
        doReturn(Arrays.asList(testDevice1, testDevice2)).when(mContext).getDevices();
        doReturn(CONFIG_ID_1).when(mCollector).pushStatsConfig(eq(testDevice1), any(List.class));
        doReturn(CONFIG_ID_2).when(mCollector).pushStatsConfig(eq(testDevice2), any(List.class));
        doReturn(new ArrayList<EventMetricData>())
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());
        doReturn(StatsdStatsReport.newBuilder().build())
                .when(mCollector)
                .getStatsdMetadata(any(ITestDevice.class));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // Check that the count metrics contain the serial numbers for the two devices,
        // respectively.
        List<String> countMetricKeys =
                runMetrics
                        .keySet()
                        .stream()
                        .filter(
                                key ->
                                        hasPrefixAndSuffix(
                                                key,
                                                RuntimeRestartCollector.METRIC_PREFIX,
                                                RuntimeRestartCollector.METRIC_SUFFIX_COUNT))
                        .collect(Collectors.toList());
        // One key for each device.
        Assert.assertEquals(2, countMetricKeys.size());
        Assert.assertTrue(
                countMetricKeys
                        .stream()
                        .anyMatch(
                                key ->
                                        (key.contains(DEVICE_SERIAL_1)
                                                && !key.contains(DEVICE_SERIAL_2))));
        Assert.assertTrue(
                countMetricKeys
                        .stream()
                        .anyMatch(
                                key ->
                                        (key.contains(DEVICE_SERIAL_2)
                                                && !key.contains(DEVICE_SERIAL_1))));
    }

    /** Helper method to get count from metrics. */
    private static int getCount(Map<String, Metric> metrics) {
        return Integer.parseInt(
                metrics.entrySet()
                        .stream()
                        .filter(
                                entry ->
                                        hasPrefixAndSuffix(
                                                entry.getKey(),
                                                RuntimeRestartCollector.METRIC_PREFIX,
                                                RuntimeRestartCollector.METRIC_SUFFIX_COUNT))
                        .map(entry -> entry.getValue())
                        .findFirst()
                        .get()
                        .getMeasurements()
                        .getSingleString());
    }

    /** Helper method to check that no metric key with the given suffix is in the metrics. */
    private static void ensureNoMetricWithKeySuffix(Map<String, Metric> metrics, String suffix) {
        Assert.assertTrue(
                metrics.keySet()
                        .stream()
                        .noneMatch(
                                key ->
                                        hasPrefixAndSuffix(
                                                key,
                                                RuntimeRestartCollector.METRIC_PREFIX,
                                                suffix)));
    }

    /**
     * Helper method to find a comma-separated String metric value by its key suffix, and return its
     * values as a list.
     */
    private static List<String> getStringMetricValuesByKeySuffix(
            Map<String, Metric> metrics, String suffix) {
        return metrics.entrySet()
                .stream()
                .filter(
                        entry ->
                                hasPrefixAndSuffix(
                                        entry.getKey(),
                                        RuntimeRestartCollector.METRIC_PREFIX,
                                        suffix))
                .map(entry -> entry.getValue().getMeasurements().getSingleString().split(","))
                .flatMap(arr -> Arrays.stream(arr))
                .collect(Collectors.toList());
    }

    /**
     * Helper method to find a comma-separated int metric value by its key suffix, and return its
     * values as a list.
     */
    private static List<Integer> getIntMetricValuesByKeySuffix(
            Map<String, Metric> metrics, String suffix) {
        return getStringMetricValuesByKeySuffix(metrics, suffix)
                .stream()
                .map(val -> Integer.valueOf(val))
                .collect(Collectors.toList());
    }

    /**
     * Helper method to find a comma-separated long metric value by its key suffix, and return its
     * values as a list.
     */
    private static List<Long> getLongMetricValuesByKeySuffix(
            Map<String, Metric> metrics, String suffix) {
        return getStringMetricValuesByKeySuffix(metrics, suffix)
                .stream()
                .map(val -> Long.valueOf(val))
                .collect(Collectors.toList());
    }

    private static boolean hasPrefixAndSuffix(String target, String prefix, String suffix) {
        return target.startsWith(prefix) && target.endsWith(suffix);
    }

    private ITestDevice mockTestDevice(String serial) {
        ITestDevice device = mock(ITestDevice.class);
        doReturn(serial).when(device).getSerialNumber();
        return device;
    }
}
