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

import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsdStatsReport;
import com.android.os.StatsLog.StatsLogReport;
import com.android.tradefed.device.CollectingByteOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.InputStreamSource;

import com.google.common.annotations.VisibleForTesting;
import com.google.protobuf.InvalidProtocolBufferException;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/** Utility class for pulling metrics from pushed statsd configurations. */
public class MetricUtil {
    @VisibleForTesting
    static final String DUMP_REPORT_CMD_TEMPLATE = "cmd stats dump-report %s %s --proto";

    // Android P version does not support this argument. Make it separate and add only when needed
    @VisibleForTesting
    static final String DUMP_REPORT_INCLUDE_CURRENT_BUCKET = "--include_current_bucket";

    // The command is documented in frameworks/base/cmds/statsd/src/StatsService.cpp.
    @VisibleForTesting
    static final String DUMP_STATSD_METADATA_CMD = "dumpsys stats --metadata --proto";

    @VisibleForTesting static final int SDK_VERSION_Q = 29;

    /** Get statsd event metrics data from the device using the statsd config id. */
    public static List<EventMetricData> getEventMetricData(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        ConfigMetricsReportList reports = getReportList(device, configId);
        if (reports.getReportsList().isEmpty()) {
            CLog.d("No stats report collected.");
            return new ArrayList<EventMetricData>();
        }
        List<EventMetricData> data = new ArrayList<>();
        // Usually, there is only one report. However, a runtime restart will generate a new report
        // for the same config, resulting in multiple reports. Their data is independent and can
        // simply be concatenated together.
        for (ConfigMetricsReport report : reports.getReportsList()) {
            for (StatsLogReport metric : report.getMetricsList()) {
                data.addAll(metric.getEventMetrics().getDataList());
            }
        }
        data.sort(Comparator.comparing(EventMetricData::getElapsedTimestampNanos));
        CLog.d("Received EventMetricDataList as following:\n");
        for (EventMetricData d : data) {
            CLog.d("Atom at %d:\n%s", d.getElapsedTimestampNanos(), d.getAtom().toString());
        }
        return data;
    }

    /** Get Statsd report as a byte stream source */
    public static InputStreamSource getReportByteStream(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        return new ByteArrayInputStreamSource(getReportByteArray(device, configId));
    }

    /** Get statsd metadata which also contains system server crash information. */
    public static StatsdStatsReport getStatsdMetadata(ITestDevice device)
            throws DeviceNotAvailableException, InvalidProtocolBufferException {
        final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        device.executeShellCommand(DUMP_STATSD_METADATA_CMD, receiver);
        return StatsdStatsReport.parser().parseFrom(receiver.getOutput());
    }

    /** Get the report list proto from the device for the given {@code configId}. */
    private static ConfigMetricsReportList getReportList(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        try {
            byte[] output = getReportByteArray(device, configId);
            return ConfigMetricsReportList.parser().parseFrom(output);
        } catch (InvalidProtocolBufferException e) {
            throw new RuntimeException(e);
        }
    }

    private static byte[] getReportByteArray(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        final CollectingByteOutputReceiver receiver = new CollectingByteOutputReceiver();
        String dumpCommand =
                String.format(
                        DUMP_REPORT_CMD_TEMPLATE,
                        String.valueOf(configId),
                        device.checkApiLevelAgainstNextRelease(SDK_VERSION_Q)
                                ? DUMP_REPORT_INCLUDE_CURRENT_BUCKET
                                : "");
        CLog.d("Dumping stats report with command: " + dumpCommand);
        device.executeShellCommand(dumpCommand, receiver);
        return receiver.getOutput();
    }
}
