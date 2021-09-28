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
package com.android.tradefed.util.statsd;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.matches;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.BleScanStateChanged;
import com.android.os.AtomsProto.BleScanResultReceived;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsdStatsReport;
import com.android.os.StatsLog.StatsLogReport;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;

import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.List;

/** Unit tests for {@link MetricUtil} */
@RunWith(JUnit4.class)
public class MetricUtilTest {
    // A config id used for testing.
    private static final long CONFIG_ID = 11;
    // Three metrics for a sample report.
    private static final long METRIC_1_NANOS = 222;
    private static final Atom METRIC_1_ATOM =
            Atom.newBuilder().setBleScanStateChanged(BleScanStateChanged.newBuilder()).build();
    private static final EventMetricData METRIC_1_DATA =
            EventMetricData.newBuilder()
                    .setElapsedTimestampNanos(METRIC_1_NANOS)
                    .setAtom(METRIC_1_ATOM)
                    .build();
    private static final long METRIC_2_NANOS = 111;
    private static final Atom METRIC_2_ATOM =
            Atom.newBuilder().setBleScanResultReceived(BleScanResultReceived.newBuilder()).build();
    private static final EventMetricData METRIC_2_DATA =
            EventMetricData.newBuilder()
                    .setElapsedTimestampNanos(METRIC_2_NANOS)
                    .setAtom(METRIC_2_ATOM)
                    .build();
    // The third metric has the same atom as the first one for the "two reports on the same atom"
    // test case.
    private static final long METRIC_3_NANOS = 333;
    private static final EventMetricData METRIC_3_DATA =
            EventMetricData.newBuilder()
                    .setElapsedTimestampNanos(METRIC_3_NANOS)
                    .setAtom(METRIC_1_ATOM)
                    .build();
    // A sample metrics report.
    private static final ConfigMetricsReportList SINGLE_REPORT_LIST_PROTO =
            ConfigMetricsReportList.newBuilder()
                    .addReports(
                            ConfigMetricsReport.newBuilder()
                                    .addMetrics(
                                            StatsLogReport.newBuilder()
                                                    .setEventMetrics(
                                                            StatsLogReport.EventMetricDataWrapper
                                                                    .newBuilder()
                                                                    .addData(METRIC_1_DATA)
                                                                    .addData(METRIC_2_DATA))))
                    .build();
    // A sample metrics report list with multiple reports on different atoms.
    private static final ConfigMetricsReportList MULTIPLE_REPORT_LIST_PROTO_DIFFERENT_ATOMS =
            ConfigMetricsReportList.newBuilder()
                    .addReports(
                            ConfigMetricsReport.newBuilder()
                                    .addMetrics(
                                            StatsLogReport.newBuilder()
                                                    .setEventMetrics(
                                                            StatsLogReport.EventMetricDataWrapper
                                                                    .newBuilder()
                                                                    .addData(METRIC_1_DATA))))
                    .addReports(
                            ConfigMetricsReport.newBuilder()
                                    .addMetrics(
                                            StatsLogReport.newBuilder()
                                                    .setEventMetrics(
                                                            StatsLogReport.EventMetricDataWrapper
                                                                    .newBuilder()
                                                                    .addData(METRIC_2_DATA))))
                    .build();
    // A sample metrics report list with multiple reports on the same atom.
    private static final ConfigMetricsReportList MULTIPLE_REPORT_LIST_PROTO_SAME_ATOM =
            ConfigMetricsReportList.newBuilder()
                    .addReports(
                            ConfigMetricsReport.newBuilder()
                                    .addMetrics(
                                            StatsLogReport.newBuilder()
                                                    .setEventMetrics(
                                                            StatsLogReport.EventMetricDataWrapper
                                                                    .newBuilder()
                                                                    .addData(METRIC_1_DATA))))
                    .addReports(
                            ConfigMetricsReport.newBuilder()
                                    .addMetrics(
                                            StatsLogReport.newBuilder()
                                                    .setEventMetrics(
                                                            StatsLogReport.EventMetricDataWrapper
                                                                    .newBuilder()
                                                                    .addData(METRIC_3_DATA))))
                    .build();

    // Implementation of Mockito's {@link Answer<T>} interface to write bytes to the
    // {@link CollectingByteOutputReceiver} when mocking {@link ITestDevice#executeShellCommand}.
    private class ExecuteShellCommandWithOutputAnswer implements Answer<Object> {
        private byte[] mOutputBytes;

        public ExecuteShellCommandWithOutputAnswer(byte[] outputBytes) {
            mOutputBytes = outputBytes;
        }

        @Override
        public Object answer(InvocationOnMock invocation) {
            ((CollectingByteOutputReceiver) invocation.getArguments()[1])
                    .addOutput(mOutputBytes, 0, mOutputBytes.length);
            return null;
        }
    }

    private ITestDevice mTestDevice;

    @Before
    public void setUpMocks() {
        mTestDevice = Mockito.mock(ITestDevice.class);
    }

    /** Test that a non-empty ConfigMetricsReportList is parsed correctly. */
    @Test
    public void testNonEmptyMetricReportList() throws DeviceNotAvailableException {
        when(mTestDevice.checkApiLevelAgainstNextRelease(MetricUtil.SDK_VERSION_Q))
                .thenReturn(true);
        byte[] sampleReportBytes = SINGLE_REPORT_LIST_PROTO.toByteArray();
        // Mock executeShellCommand to supply the passed-in CollectingByteOutputReceiver with
        // SINGLE_REPORT_LIST_PROTO.
        doAnswer(new ExecuteShellCommandWithOutputAnswer(sampleReportBytes))
                .when(mTestDevice)
                .executeShellCommand(
                        matches(
                                String.format(
                                        MetricUtil.DUMP_REPORT_CMD_TEMPLATE,
                                        String.valueOf(CONFIG_ID),
                                        MetricUtil.DUMP_REPORT_INCLUDE_CURRENT_BUCKET)),
                        any(CollectingByteOutputReceiver.class));
        List<EventMetricData> data = MetricUtil.getEventMetricData(mTestDevice, CONFIG_ID);
        // Resulting list should have two metrics.
        assertThat(data.size()).comparesEqualTo(2);
        // The first metric should correspond to METRIC_2_* as its timestamp is earlier.
        assertThat(data.get(0).getElapsedTimestampNanos()).comparesEqualTo(METRIC_2_NANOS);
        assertThat(data.get(0).getAtom().hasBleScanResultReceived()).isTrue();
        // The second metric should correspond to METRIC_1_*.
        assertThat(data.get(1).getElapsedTimestampNanos()).comparesEqualTo(METRIC_1_NANOS);
        assertThat(data.get(1).getAtom().hasBleScanStateChanged()).isTrue();
    }

    /** Test that an empty list is returned for an empty ConfigMetricsReportList proto. */
    @Test
    public void testEmptyMetricReportList() throws DeviceNotAvailableException {
        when(mTestDevice.checkApiLevelAgainstNextRelease(MetricUtil.SDK_VERSION_Q))
                .thenReturn(true);
        byte[] emptyReportBytes = ConfigMetricsReport.newBuilder().build().toByteArray();
        // Mock executeShellCommand to supply the passed-in CollectingByteOutputReceiver with the
        // empty report list proto.
        doAnswer(new ExecuteShellCommandWithOutputAnswer(emptyReportBytes))
                .when(mTestDevice)
                .executeShellCommand(
                        matches(
                                String.format(
                                        MetricUtil.DUMP_REPORT_CMD_TEMPLATE,
                                        String.valueOf(CONFIG_ID),
                                        MetricUtil.DUMP_REPORT_INCLUDE_CURRENT_BUCKET)),
                        any(CollectingByteOutputReceiver.class));
        List<EventMetricData> data = MetricUtil.getEventMetricData(mTestDevice, CONFIG_ID);
        // Resulting list should be empty.
        assertThat(data.size()).comparesEqualTo(0);
    }

    /**
     * Test that the utility can process multiple {@link ConfigMetricsReport} in the {@link
     * ConfigMetricsReportList} that contain metrics from different atoms.
     */
    @Test
    public void testMultipleReportsInReportList_differentAtoms()
            throws DeviceNotAvailableException {
        when(mTestDevice.checkApiLevelAgainstNextRelease(MetricUtil.SDK_VERSION_Q))
                .thenReturn(true);
        byte[] sampleReportBytes = MULTIPLE_REPORT_LIST_PROTO_DIFFERENT_ATOMS.toByteArray();
        // Mock executeShellCommand to supply the passed-in CollectingByteOutputReceiver with
        // SINGLE_REPORT_LIST_PROTO.
        doAnswer(new ExecuteShellCommandWithOutputAnswer(sampleReportBytes))
                .when(mTestDevice)
                .executeShellCommand(
                        matches(
                                String.format(
                                        MetricUtil.DUMP_REPORT_CMD_TEMPLATE,
                                        String.valueOf(CONFIG_ID),
                                        MetricUtil.DUMP_REPORT_INCLUDE_CURRENT_BUCKET)),
                        any(CollectingByteOutputReceiver.class));
        List<EventMetricData> data = MetricUtil.getEventMetricData(mTestDevice, CONFIG_ID);
        // Resulting list should have two metrics.
        assertThat(data.size()).comparesEqualTo(2);
        // The first metric should correspond to METRIC_2_* as its timestamp is earlier.
        assertThat(data.get(0).getElapsedTimestampNanos()).comparesEqualTo(METRIC_2_NANOS);
        assertThat(data.get(0).getAtom().hasBleScanResultReceived()).isTrue();
        // The second metric should correspond to METRIC_1_*.
        assertThat(data.get(1).getElapsedTimestampNanos()).comparesEqualTo(METRIC_1_NANOS);
        assertThat(data.get(1).getAtom().hasBleScanStateChanged()).isTrue();
    }

    /**
     * Test that the utility can process multiple {@link ConfigMetricsReport} in the {@link
     * ConfigMetricsReportList} that contain metrics from the same atom.
     */
    @Test
    public void testMultipleReportsInReportList_sameAtom() throws DeviceNotAvailableException {
        when(mTestDevice.checkApiLevelAgainstNextRelease(MetricUtil.SDK_VERSION_Q))
                .thenReturn(true);
        byte[] sampleReportBytes = MULTIPLE_REPORT_LIST_PROTO_SAME_ATOM.toByteArray();
        // Mock executeShellCommand to supply the passed-in CollectingByteOutputReceiver with
        // SINGLE_REPORT_LIST_PROTO.
        doAnswer(new ExecuteShellCommandWithOutputAnswer(sampleReportBytes))
                .when(mTestDevice)
                .executeShellCommand(
                        matches(
                                String.format(
                                        MetricUtil.DUMP_REPORT_CMD_TEMPLATE,
                                        String.valueOf(CONFIG_ID),
                                        MetricUtil.DUMP_REPORT_INCLUDE_CURRENT_BUCKET)),
                        any(CollectingByteOutputReceiver.class));
        List<EventMetricData> data = MetricUtil.getEventMetricData(mTestDevice, CONFIG_ID);
        // Resulting list should have two metrics.
        assertThat(data.size()).comparesEqualTo(2);
        // The first metric should correspond to METRIC_1_* as its timestamp is earlier.
        assertThat(data.get(0).getElapsedTimestampNanos()).comparesEqualTo(METRIC_1_NANOS);
        assertThat(data.get(0).getAtom().hasBleScanStateChanged()).isTrue();
        // The second metric should correspond to METRIC_3_*.
        assertThat(data.get(1).getElapsedTimestampNanos()).comparesEqualTo(METRIC_3_NANOS);
        assertThat(data.get(1).getAtom().hasBleScanStateChanged()).isTrue();
    }

    /** Test that invalid proto from the dump report command causes an exception. */
    @Test
    public void testInvalidDumpedReportThrows() throws DeviceNotAvailableException {
        when(mTestDevice.checkApiLevelAgainstNextRelease(MetricUtil.SDK_VERSION_Q))
                .thenReturn(true);
        byte[] invalidProtoBytes = "not a proto".getBytes();
        // Mock executeShellCommand to supply the passed-in CollectingByteOutputReceiver with the
        // invalid proto bytes.
        doAnswer(new ExecuteShellCommandWithOutputAnswer(invalidProtoBytes))
                .when(mTestDevice)
                .executeShellCommand(
                        matches(
                                String.format(
                                        MetricUtil.DUMP_REPORT_CMD_TEMPLATE,
                                        String.valueOf(CONFIG_ID),
                                        MetricUtil.DUMP_REPORT_INCLUDE_CURRENT_BUCKET)),
                        any(CollectingByteOutputReceiver.class));
        try {
            List<EventMetricData> data = MetricUtil.getEventMetricData(mTestDevice, CONFIG_ID);
            fail("An exception should be thrown for the invalid proto data.");
        } catch (RuntimeException e) {
            // Expected behavior, no action required.
        }
    }

    /** Test that the legacy dump stats report command executes with version below Android Q */
    @Test
    public void testLegacyDumpReportCmd() throws DeviceNotAvailableException {
        when(mTestDevice.checkApiLevelAgainstNextRelease(MetricUtil.SDK_VERSION_Q))
                .thenReturn(false);
        List<EventMetricData> data = MetricUtil.getEventMetricData(mTestDevice, CONFIG_ID);
        verify(mTestDevice)
                .executeShellCommand(
                        matches(
                                String.format(
                                        MetricUtil.DUMP_REPORT_CMD_TEMPLATE,
                                        String.valueOf(CONFIG_ID),
                                        "")),
                        any(CollectingByteOutputReceiver.class));
    }

    /** Test that the utility can correctly retrieve statsd metadata. */
    @Test
    public void testGettingStatsdStats()
            throws DeviceNotAvailableException, InvalidProtocolBufferException {
        doAnswer(
                        new ExecuteShellCommandWithOutputAnswer(
                                StatsdStatsReport.newBuilder().build().toByteArray()))
                .when(mTestDevice)
                .executeShellCommand(
                        eq(MetricUtil.DUMP_STATSD_METADATA_CMD),
                        any(CollectingByteOutputReceiver.class));
        assertThat(MetricUtil.getStatsdMetadata(mTestDevice)).isNotNull();
    }

    /** Test that the utility throws when the retrieved statsd metatdata is not valid. */
    @Test
    public void testInvalidStatsdStatsThrows() throws DeviceNotAvailableException {
        doAnswer(new ExecuteShellCommandWithOutputAnswer("not a proto".getBytes()))
                .when(mTestDevice)
                .executeShellCommand(
                        eq(MetricUtil.DUMP_STATSD_METADATA_CMD),
                        any(CollectingByteOutputReceiver.class));
        try {
            MetricUtil.getStatsdMetadata(mTestDevice);
            fail("An exception should be thrown for the invalid proto data.");
        } catch (InvalidProtocolBufferException e) {
            // Expected behavior, no action required.
        }
    }
}
