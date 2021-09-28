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

package com.android.tradefed.device.metric;

import android.os.IncidentProto;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.google.protobuf.InvalidProtocolBufferException;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.util.Map;

/**
 * Pulls and processes incident reports that are reported device-side and collects incident reports
 * host-side at the end of a test run if configured to do so.
 */
@OptionClass(alias = "incident-collector")
public class IncidentReportCollector extends FilePullerLogCollector {
    // Prefix of the keys for all incident files passed from the device.
    private static final String INCIDENT_KEY_MATCHER = "incident-report";
    // Suffix for all of the logs that are processed incident reports.
    private static final String PROCESSED_KEY_SUFFIX = "-processed";
    // Incident report command and associated timeouts that are used.
    static final String INCIDENT_REPORT_CMD = "incident -b -p EXPLICIT";
    private static final long INCIDENT_DUMP_TIMEOUT = 5 * 60 * 1000;
    private static final long DEVICE_AVAILABLE_TIMEOUT = 10 * 60 * 1000;

    @Option(
            name = "incident-on-test-run-end",
            description =
                    "Collect an incident report at the end of each test run. This report will not"
                            + " be collected if a report was collected on-device and logged prior.")
    private boolean mIncidentOnRunEnd = false;

    public IncidentReportCollector() {
        addKeys(INCIDENT_KEY_MATCHER);
    }

    @Override
    public void onTestRunEnd(
            DeviceMetricData runData, final Map<String, Metric> currentRunMetrics) {
        super.onTestRunEnd(runData, currentRunMetrics);
        // Only collect if set and there wasn't an on-device report collected.
        if (!mIncidentOnRunEnd) {
            return;
        }
        for (ITestDevice device : getDevices()) {
            File outFile = null;
            try {
                // Ensure the device is available for the command.
                device.waitForDeviceAvailable(DEVICE_AVAILABLE_TIMEOUT);
                // Collect the incident report to a new file.
                outFile = File.createTempFile("incident-on-test-run-end", ".pb");
                FileOutputStream outStream = new FileOutputStream(outFile, false);
                CommandResult result = device.executeShellV2Command(INCIDENT_REPORT_CMD, outStream);
                // Complain (and say why) if something didn't go right.
                if (result.getStatus().equals(CommandStatus.SUCCESS)) {
                    // Log the extra file and process it as a report.
                    try (InputStreamSource source = new FileInputStreamSource(outFile)) {
                        testLog(outFile.getName(), LogDataType.PB, source);
                    }
                    logProcessedReport(outFile);
                } else {
                    CLog.e(
                            "There was an error collecting an incident report: %s",
                            result.getStderr());
                }
                // Just wrap exceptions and print them.
            } catch (DeviceNotAvailableException dnae) {
                CLog.e(dnae);
            } catch (IOException ioe) {
                CLog.e(ioe);
            } finally {
                FileUtil.deleteFile(outFile);
            }
        }
    }

    @Override
    protected void postProcessMetricFile(String key, File metricFile, DeviceMetricData runData) {
        logProcessedReport(metricFile);
    }

    private void logProcessedReport(File metricFile) {
        // Read and interpret the incident report's bytes.
        IncidentProto processedReport;
        try {
            byte[] output = Files.readAllBytes(metricFile.toPath());
            processedReport = IncidentProto.parser().parseFrom(output);
        } catch (InvalidProtocolBufferException e) {
            throw new RuntimeException(
                    String.format("Failed to parse protobuf: %s", metricFile.toPath().toString()),
                    e);
        } catch (IOException e) {
            throw new RuntimeException(
                    String.format(
                            "Failed to read incident file: %s", metricFile.toPath().toString()),
                    e);
        }
        // Report the newly processed incident report.
        testLog(
                metricFile.getName().concat(PROCESSED_KEY_SUFFIX),
                LogDataType.PB,
                new ByteArrayInputStreamSource(processedReport.toString().getBytes()));
    }
}
