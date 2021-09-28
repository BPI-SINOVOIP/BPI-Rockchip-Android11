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
 * limitations under the License
 */

package com.android.tradefed.device.metric;

import com.android.loganalysis.item.GenericTimingItem;
import com.android.loganalysis.parser.TimingsLogParser;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.LogcatReceiver;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.DataType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;

import com.google.common.annotations.VisibleForTesting;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

/**
 * A metric collector that collects timing information (e.g. user switch time) from logcat during
 * one or multiple repeated tests by using given regex patterns to parse start and end signals of an
 * event from logcat lines.
 */
@OptionClass(alias = "timing-metric-collector")
public class LogcatTimingMetricCollector extends BaseDeviceMetricCollector {

    private static final String LOGCAT_NAME_FORMAT = "device_%s_test_logcat";
    // Use logcat -T 'count' to only print a few line before we start and not the full buffer
    private static final String LOGCAT_CMD = "logcat *:D -T 150";

    @Option(
            name = "start-pattern",
            description =
                    "Key-value pairs to specify the timing metric start patterns to capture from"
                            + " logcat. Key: metric name, value: regex pattern of logcat line"
                            + " indicating the start of the timing metric")
    private final Map<String, String> mStartPatterns = new HashMap<>();

    @Option(
            name = "end-pattern",
            description =
                    "Key-value pairs to specify the timing metric end patterns to capture from"
                            + " logcat. Key: metric name, value: regex pattern of logcat line"
                            + " indicating the end of the timing metric")
    private final Map<String, String> mEndPatterns = new HashMap<>();

    @Option(
            name = "logcat-buffer",
            description =
                    "Logcat buffers where the timing metrics are captured. Default buffers will be"
                            + " used if not specified.")
    private final List<String> mLogcatBuffers = new ArrayList<>();

    private final Map<ITestDevice, LogcatReceiver> mLogcatReceivers = new HashMap<>();
    private final TimingsLogParser mParser = new TimingsLogParser();

    @Override
    public void onTestRunStart(DeviceMetricData testData) {
        // Adding patterns
        mParser.clearDurationPatterns();
        for (Map.Entry<String, String> entry : mStartPatterns.entrySet()) {
            String name = entry.getKey();
            if (!mEndPatterns.containsKey(name)) {
                CLog.w("Metric %s is missing end pattern, skipping.", name);
                continue;
            }
            Pattern start = Pattern.compile(entry.getValue());
            Pattern end = Pattern.compile(mEndPatterns.get(name));
            CLog.d("Adding metric: %s", name);
            mParser.addDurationPatternPair(name, start, end);
        }
        // Start receiving logcat
        String logcatCmd = LOGCAT_CMD;
        if (!mLogcatBuffers.isEmpty()) {
            logcatCmd += " -b " + String.join(",", mLogcatBuffers);
        }
        for (ITestDevice device : getDevices()) {
            CLog.d(
                    "Creating logcat receiver on device %s with command %s",
                    device.getSerialNumber(), logcatCmd);
            mLogcatReceivers.put(device, createLogcatReceiver(device, logcatCmd));
            try {
                device.executeShellCommand("logcat -c");
            } catch (DeviceNotAvailableException e) {
                CLog.e(
                        "Device not available when clear logcat. Skip logcat collection on %s",
                        device.getSerialNumber());
                continue;
            }
            mLogcatReceivers.get(device).start();
        }
    }

    @Override
    public void onTestRunEnd(
            DeviceMetricData testData, final Map<String, Metric> currentTestCaseMetrics) {
        boolean isMultiDevice = getDevices().size() > 1;
        for (ITestDevice device : getDevices()) {
            try (InputStreamSource logcatData = mLogcatReceivers.get(device).getLogcatData()) {
                Map<String, List<Double>> metrics = parse(logcatData);
                for (Map.Entry<String, List<Double>> entry : metrics.entrySet()) {
                    String name = entry.getKey();
                    List<Double> values = entry.getValue();
                    if (isMultiDevice) {
                        testData.addMetricForDevice(device, name, createMetric(values));
                    } else {
                        testData.addMetric(name, createMetric(values));
                    }
                    CLog.d(
                            "Metric: %s with value: %s, added to device %s",
                            name, values, device.getSerialNumber());
                }
                testLog(
                        String.format(LOGCAT_NAME_FORMAT, device.getSerialNumber()),
                        LogDataType.TEXT,
                        logcatData);
            }
            mLogcatReceivers.get(device).stop();
            mLogcatReceivers.get(device).clear();
        }
    }

    @Override
    public void onTestFail(DeviceMetricData testData, TestDescription test) {
        for (ITestDevice device : getDevices()) {
            try (InputStreamSource logcatData = mLogcatReceivers.get(device).getLogcatData()) {
                testLog(
                        String.format(LOGCAT_NAME_FORMAT, device.getSerialNumber()),
                        LogDataType.TEXT,
                        logcatData);
            }
            mLogcatReceivers.get(device).stop();
            mLogcatReceivers.get(device).clear();
        }
    }

    @VisibleForTesting
    Map<String, List<Double>> parse(InputStreamSource logcatData) {
        Map<String, List<Double>> metrics = new HashMap<>();
        try (InputStream inputStream = logcatData.createInputStream();
                InputStreamReader logcatReader = new InputStreamReader(inputStream);
                BufferedReader br = new BufferedReader(logcatReader)) {
            List<GenericTimingItem> items = mParser.parseGenericTimingItems(br);
            for (GenericTimingItem item : items) {
                String metricKey = item.getName();
                if (!metrics.containsKey(metricKey)) {
                    metrics.put(metricKey, new ArrayList<>());
                }
                metrics.get(metricKey).add(item.getDuration());
            }
        } catch (IOException e) {
            CLog.e("Failed to parse timing metrics from logcat %s", e);
        }
        return metrics;
    }

    @VisibleForTesting
    LogcatReceiver createLogcatReceiver(ITestDevice device, String logcatCmd) {
        return new LogcatReceiver(device, logcatCmd, device.getOptions().getMaxLogcatDataSize(), 0);
    }

    private Metric.Builder createMetric(List<Double> values) {
        // TODO: Fix post processors to handle double values. For now use concatenated string as we
        // prefer to use AggregatedPostProcessor
        String stringValue =
                values.stream()
                        .map(value -> Double.toString(value))
                        .collect(Collectors.joining(","));
        return Metric.newBuilder()
                .setType(DataType.RAW)
                .setMeasurements(Measurements.newBuilder().setSingleString(stringValue));
    }
}
