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
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.BootSequenceReported;
import com.android.os.StatsLog.EventMetricData;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.Spy;

/** Unit tests for {@link RebootReasonCollector}. */
@RunWith(JUnit4.class)
public class RebootReasonCollectorTest {
    private static final String DEVICE_SERIAL_1 = "device_serial_1";
    private static final String DEVICE_SERIAL_2 = "device_serial_2";
    private static final long CONFIG_ID_1 = 1;
    private static final long CONFIG_ID_2 = 2;

    @Spy private RebootReasonCollector mCollector;
    @Mock private IInvocationContext mContext;
    @Mock private ITestInvocationListener mListener;

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        doNothing().when(mCollector).removeConfig(any(ITestDevice.class), anyLong());
    }

    /** Test that the collector makes the correct callbacks when testing a single device. */
    @Test
    public void testStatsdInteractions_singleDevice() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        when(mContext.getDevices()).thenReturn(Arrays.asList(testDevice));
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(new ArrayList<EventMetricData>())
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());

        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, new HashMap<String, Metric>());

        verify(mCollector, times(1))
                .pushStatsConfig(
                        eq(testDevice),
                        argThat(l -> l.contains(Atom.BOOT_SEQUENCE_REPORTED_FIELD_NUMBER)));
        verify(mCollector, times(1)).getEventMetricData(eq(testDevice), eq(CONFIG_ID_1));
        verify(mCollector, times(1)).removeConfig(eq(testDevice), eq(CONFIG_ID_1));
    }

    /** Test that boot reasons are correctly counted for a single device. */
    @Test
    public void testAddingMetrics_singleDevice() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        when(mContext.getDevices()).thenReturn(Arrays.asList(testDevice));
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(
                        Arrays.asList(
                                mockBootEventMetric("bootloader_reason", "system_reason_1"),
                                mockBootEventMetric("bootloader_reason", "system_reason_2"),
                                mockBootEventMetric("bootloader_reason", "system_reason_1")))
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // runMetrics should count of two for "bootloader_reason" + "system_reason_1" and one for
        // "bootloader_reason" + "system_reason_2". The metric keys should also not contain the
        // device serial number.
        boolean reason1CountCorrect =
                runMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                entry ->
                                        entry.getKey().contains(RebootReasonCollector.METRIC_PREFIX)
                                                && entry.getKey().contains("bootloader_reason")
                                                && entry.getKey().contains("system_reason_1")
                                                && !entry.getKey().contains(DEVICE_SERIAL_1)
                                                && Integer.valueOf(
                                                                entry.getValue()
                                                                        .getMeasurements()
                                                                        .getSingleString())
                                                        .equals(2));
        boolean reason2CountCorrect =
                runMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                entry ->
                                        entry.getKey().contains(RebootReasonCollector.METRIC_PREFIX)
                                                && entry.getKey().contains("bootloader_reason")
                                                && entry.getKey().contains("system_reason_2")
                                                && !entry.getKey().contains(DEVICE_SERIAL_1)
                                                && Integer.valueOf(
                                                                entry.getValue()
                                                                        .getMeasurements()
                                                                        .getSingleString())
                                                        .equals(1));
        boolean totalCountCorrect =
                runMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                entry ->
                                        entry.getKey().equals(RebootReasonCollector.COUNT_KEY)
                                                && Integer.valueOf(
                                                                entry.getValue()
                                                                        .getMeasurements()
                                                                        .getSingleString())
                                                        .equals(3));
        Assert.assertTrue(reason1CountCorrect);
        Assert.assertTrue(reason2CountCorrect);
        Assert.assertTrue(totalCountCorrect);
    }

    /** Test that the collector makes the correct callbacks when testing multiple devices. */
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

        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, new HashMap<String, Metric>());

        verify(mCollector, times(1))
                .pushStatsConfig(
                        eq(testDevice1),
                        argThat(l -> l.contains(Atom.BOOT_SEQUENCE_REPORTED_FIELD_NUMBER)));
        verify(mCollector, times(1))
                .pushStatsConfig(
                        eq(testDevice2),
                        argThat(l -> l.contains(Atom.BOOT_SEQUENCE_REPORTED_FIELD_NUMBER)));
        verify(mCollector, times(1)).getEventMetricData(eq(testDevice1), eq(CONFIG_ID_1));
        verify(mCollector, times(1)).getEventMetricData(eq(testDevice2), eq(CONFIG_ID_2));
        verify(mCollector, times(1)).removeConfig(eq(testDevice1), eq(CONFIG_ID_1));
        verify(mCollector, times(1)).removeConfig(eq(testDevice2), eq(CONFIG_ID_2));
    }

    /** Test that boot reasons are correctly counted for each device when multiple are present. */
    @Test
    public void testAddingMetrics_multiDevice() throws Exception {
        ITestDevice testDevice1 = mockTestDevice(DEVICE_SERIAL_1);
        ITestDevice testDevice2 = mockTestDevice(DEVICE_SERIAL_2);
        doReturn(Arrays.asList(testDevice1, testDevice2)).when(mContext).getDevices();
        doReturn(DEVICE_SERIAL_1).when(mContext).getDeviceName(testDevice1);
        doReturn(DEVICE_SERIAL_2).when(mContext).getDeviceName(testDevice2);
        doReturn(CONFIG_ID_1).when(mCollector).pushStatsConfig(eq(testDevice1), any(List.class));
        doReturn(CONFIG_ID_2).when(mCollector).pushStatsConfig(eq(testDevice2), any(List.class));
        doReturn(Arrays.asList(mockBootEventMetric("bootloader_reason", "system_reason")))
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), eq(CONFIG_ID_1));
        doReturn(Arrays.asList(mockBootEventMetric("bootloader_reason", "system_reason")))
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), eq(CONFIG_ID_2));

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        // Each device should have a separate key for their respective boot event.
        boolean device1CountCorrect =
                runMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                entry ->
                                        entry.getKey().contains(RebootReasonCollector.METRIC_PREFIX)
                                                && entry.getKey().contains("bootloader_reason")
                                                && entry.getKey().contains("system_reason")
                                                && entry.getKey().contains(DEVICE_SERIAL_1)
                                                && Integer.valueOf(
                                                                entry.getValue()
                                                                        .getMeasurements()
                                                                        .getSingleString())
                                                        .equals(1));
        boolean device1TotalCountCorrect =
                runMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                entry ->
                                        entry.getKey().contains(RebootReasonCollector.COUNT_KEY)
                                                && entry.getKey().contains(DEVICE_SERIAL_1)
                                                && Integer.valueOf(
                                                                entry.getValue()
                                                                        .getMeasurements()
                                                                        .getSingleString())
                                                        .equals(1));
        boolean device2CountCorrect =
                runMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                entry ->
                                        entry.getKey().contains(RebootReasonCollector.METRIC_PREFIX)
                                                && entry.getKey().contains("bootloader_reason")
                                                && entry.getKey().contains("system_reason")
                                                && entry.getKey().contains(DEVICE_SERIAL_2)
                                                && Integer.valueOf(
                                                                entry.getValue()
                                                                        .getMeasurements()
                                                                        .getSingleString())
                                                        .equals(1));
        boolean device2TotalCountCorrect =
                runMetrics
                        .entrySet()
                        .stream()
                        .anyMatch(
                                entry ->
                                        entry.getKey().contains(RebootReasonCollector.COUNT_KEY)
                                                && entry.getKey().contains(DEVICE_SERIAL_2)
                                                && Integer.valueOf(
                                                                entry.getValue()
                                                                        .getMeasurements()
                                                                        .getSingleString())
                                                        .equals(1));
        Assert.assertTrue(device1CountCorrect);
        Assert.assertTrue(device1TotalCountCorrect);
        Assert.assertTrue(device2CountCorrect);
        Assert.assertTrue(device2TotalCountCorrect);
    }

    /** Test that only a count is added when no reboots were recorded. */
    @Test
    public void testCountOnlyWhenNoReboots() throws Exception {
        ITestDevice testDevice = mockTestDevice(DEVICE_SERIAL_1);
        when(mContext.getDevices()).thenReturn(Arrays.asList(testDevice));
        doReturn(CONFIG_ID_1)
                .when(mCollector)
                .pushStatsConfig(any(ITestDevice.class), any(List.class));
        doReturn(new ArrayList<EventMetricData>())
                .when(mCollector)
                .getEventMetricData(any(ITestDevice.class), anyLong());

        HashMap<String, Metric> runMetrics = new HashMap<>();
        mCollector.init(mContext, mListener);
        mCollector.testRunStarted("test run", 1);
        mCollector.testRunEnded(0, runMetrics);

        Assert.assertTrue(
                runMetrics
                        .keySet()
                        .stream()
                        .noneMatch(key -> key.startsWith(RebootReasonCollector.METRIC_PREFIX)));
        Assert.assertTrue(
                runMetrics
                        .keySet()
                        .stream()
                        .anyMatch(key -> key.contains(RebootReasonCollector.COUNT_KEY)));
    }

    private ITestDevice mockTestDevice(String serial) {
        ITestDevice device = mock(ITestDevice.class);
        when(device.getSerialNumber()).thenReturn(serial);
        return device;
    }

    private EventMetricData mockBootEventMetric(String bootloaderReason, String systemReason) {
        return EventMetricData.newBuilder()
                .setAtom(
                        Atom.newBuilder()
                                .setBootSequenceReported(
                                        BootSequenceReported.newBuilder()
                                                .setBootloaderReason(bootloaderReason)
                                                .setSystemReason(systemReason)))
                .build();
    }
}
