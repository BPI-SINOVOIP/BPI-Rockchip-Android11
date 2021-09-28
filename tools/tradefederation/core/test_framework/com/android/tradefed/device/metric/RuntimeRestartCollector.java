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

import static com.android.tradefed.util.proto.TfMetricProtoUtil.stringToMetric;

import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsdStatsReport;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.util.statsd.ConfigUtil;
import com.android.tradefed.util.statsd.MetricUtil;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.Iterables;
import com.google.protobuf.InvalidProtocolBufferException;

import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

/**
 * Collector that collects timestamps of runtime restarts (system server crashes) during the test
 * run, if any.
 *
 * <p>Outputs results in counts, wall clock time in seconds and in HH:mm:ss format, and system
 * uptime in nanoseconds and HH:mm:ss format.
 *
 * <p>This collector uses two sources for system server crashes:
 *
 * <p>
 *
 * <ol>
 *   <li>The system_restart_sec list from StatsdStatsReport, which is a rolling list of 20
 *       timestamps when the system server crashes, in seconds, with newer crashes appended to the
 *       end (when the list fills up, older timestamps fall off the beginning).
 *   <li>The AppCrashOccurred statsd atom, where a system server crash shows up as a system_server
 *       process crash (this behavior is documented in the statsd atoms.proto definition). The event
 *       metric gives the device uptime when the crash occurs.
 * </ol>
 *
 * <p>Both can be useful information, as the former makes it easy to correlate timestamps in logs,
 * while the latter serves as a longevity metric.
 */
@OptionClass(alias = "runtime-restart-collector")
public class RuntimeRestartCollector extends BaseDeviceMetricCollector {
    private static final String METRIC_SEP = "-";
    public static final String METRIC_PREFIX = "runtime-restart";
    public static final String METRIC_SUFFIX_COUNT = "count";
    public static final String METRIC_SUFFIX_SYSTEM_TIMESTAMP_SECS = "timestamps_secs";
    public static final String METRIC_SUFFIX_SYSTEM_TIMESTAMP_FORMATTED = "timestamps_str";
    public static final String METRIC_SUFFIX_UPTIME_NANOS = "uptime_nanos";
    public static final String METRIC_SUFFIX_UPTIME_FORMATTED = "uptime_str";
    public static final SimpleDateFormat TIME_FORMATTER = new SimpleDateFormat("HH:mm:ss");
    public static final String SYSTEM_SERVER_KEYWORD = "system_server";

    private List<ITestDevice> mTestDevices;
    // Map to store the runtime restart timestamps for each device, keyed by device serial numbers.
    private Map<String, List<Integer>> mExistingTimestamps = new HashMap<>();
    // Map to store the statsd config IDs for each device, keyed by device serial numbers.
    private Map<String, Long> mDeviceConfigIds = new HashMap<>();
    // Flag variable to indicate whether including the device serial in the metrics is needed.
    // Device serial is included when there are more than one device under test.
    private boolean mIncludeDeviceSerial = false;

    /**
     * Store the existing timestamps of system server restarts prior to the test run as statsd keeps
     * a running log of them, and push the config to collect app crashes.
     */
    @Override
    public void onTestRunStart(DeviceMetricData runData) {
        mTestDevices = getDevices();
        mIncludeDeviceSerial = (mTestDevices.size() > 1);
        for (ITestDevice device : mTestDevices) {
            // Pull statsd metadata (StatsdStatsReport) from the device and keep track of existing
            // runtime restart timestamps.
            try {
                List<Integer> existingTimestamps =
                        getStatsdMetadata(device).getSystemRestartSecList();
                mExistingTimestamps.put(device.getSerialNumber(), existingTimestamps);
            } catch (DeviceNotAvailableException | InvalidProtocolBufferException e) {
                // Error is not thrown as we still want to process the other devices.
                CLog.e(
                        "Failed to get statsd metadata from device %s. Exception: %s.",
                        device.getSerialNumber(), e);
            }

            // Register statsd config to collect app crashes.
            try {
                mDeviceConfigIds.put(
                        device.getSerialNumber(),
                        pushStatsConfig(
                                device, Arrays.asList(Atom.APP_CRASH_OCCURRED_FIELD_NUMBER)));
            } catch (DeviceNotAvailableException | IOException e) {
                // Error is not thrown as we still want to push the config to other devices.
                CLog.e(
                        "Failed to push statsd config to device %s. Exception: %s.",
                        device.getSerialNumber(), e);
            }
        }
    }

    /**
     * Pull the timestamps at the end of the test run and report the difference with existing ones,
     * if any.
     */
    @Override
    public void onTestRunEnd(
            DeviceMetricData runData, final Map<String, Metric> currentRunMetrics) {
        for (ITestDevice device : mTestDevices) {
            // Pull statsd metadata again to look at the changes of system server crash timestamps
            // during the test run, and report them.
            // A copy of the list is created as the list returned by the proto is not modifiable.
            List<Integer> updatedTimestamps = new ArrayList<>();
            try {
                updatedTimestamps.addAll(getStatsdMetadata(device).getSystemRestartSecList());
            } catch (DeviceNotAvailableException | InvalidProtocolBufferException e) {
                // Error is not thrown as we still want to process the other devices.
                CLog.e(
                        "Failed to get statsd metadata from device %s. Exception: %s.",
                        device.getSerialNumber(), e);
                continue;
            }
            if (!mExistingTimestamps.containsKey(device.getSerialNumber())) {
                CLog.e("No prior state recorded for device %s.", device.getSerialNumber());
                addStatsdStatsBasedMetrics(
                        currentRunMetrics, updatedTimestamps, device.getSerialNumber());
            } else {
                try {
                    int lastTimestampBeforeTestRun =
                            Iterables.getLast(mExistingTimestamps.get(device.getSerialNumber()));
                    // If the last timestamp is not found, lastIndexOf(...) + 1 returns 0 which is
                    // what is needed in that situation.
                    addStatsdStatsBasedMetrics(
                            currentRunMetrics,
                            getAllValuesAfter(lastTimestampBeforeTestRun, updatedTimestamps),
                            device.getSerialNumber());
                } catch (NoSuchElementException e) {
                    // The exception occurs when no prior runtime restarts had been recorded. It is
                    // thrown from Iterables.getLast().
                    addStatsdStatsBasedMetrics(
                            currentRunMetrics, updatedTimestamps, device.getSerialNumber());
                }
            }
        }

        // Pull the AppCrashOccurred event metric reports and report the system server crashes
        // within.
        for (ITestDevice device : mTestDevices) {
            if (!mDeviceConfigIds.containsKey(device.getSerialNumber())) {
                CLog.e("No config ID is associated with device %s.", device.getSerialNumber());
                continue;
            }
            long configId = mDeviceConfigIds.get(device.getSerialNumber());
            List<Long> uptimeListNanos = new ArrayList<>();
            try {
                List<EventMetricData> metricData = getEventMetricData(device, configId);
                uptimeListNanos.addAll(
                        metricData
                                .stream()
                                .filter(d -> d.hasElapsedTimestampNanos())
                                .filter(d -> d.hasAtom())
                                .filter(d -> d.getAtom().hasAppCrashOccurred())
                                .filter(d -> d.getAtom().getAppCrashOccurred().hasProcessName())
                                .filter(
                                        d ->
                                                SYSTEM_SERVER_KEYWORD.equals(
                                                        d.getAtom()
                                                                .getAppCrashOccurred()
                                                                .getProcessName()))
                                .map(d -> d.getElapsedTimestampNanos())
                                .collect(Collectors.toList()));
            } catch (DeviceNotAvailableException e) {
                // Error is not thrown as we still want to process other devices.
                CLog.e(
                        "Failed to retrieve event metric data from device %s. Exception: %s.",
                        device.getSerialNumber(), e);
            }
            addAtomBasedMetrics(currentRunMetrics, uptimeListNanos, device.getSerialNumber());
            try {
                removeConfig(device, configId);
            } catch (DeviceNotAvailableException e) {
                // Error is not thrown as we still want to remove the config from other devices.
                CLog.e(
                        "Failed to remove statsd config from device %s. Exception: %s.",
                        device.getSerialNumber(), e);
            }
        }
    }

    /** Helper method to add metrics from StatsdStatsReport according to timestamps. */
    private void addStatsdStatsBasedMetrics(
            final Map<String, Metric> metrics, List<Integer> timestampsSecs, String serial) {
        // Always add a count of system server crashes, regardless of whether there are any.
        // The statsd metadata-based count is used as the atom-based data can miss runtime restart
        // instances.
        // TODO(b/135770315): Re-assess this after the root cause for missing instances in the atom
        // -based results is found.
        String countMetricKey = createMetricKey(METRIC_SUFFIX_COUNT, serial);
        metrics.put(countMetricKey, stringToMetric(String.valueOf(timestampsSecs.size())));
        // If there are runtime restarts, add a comma-separated list of timestamps.
        if (!timestampsSecs.isEmpty()) {
            // Store both the raw timestamp and the formatted, more readable version.

            String timestampMetricKey =
                    createMetricKey(METRIC_SUFFIX_SYSTEM_TIMESTAMP_SECS, serial);
            metrics.put(
                    timestampMetricKey,
                    stringToMetric(
                            timestampsSecs
                                    .stream()
                                    .map(t -> String.valueOf(t))
                                    .collect(Collectors.joining(","))));

            String formattedTimestampMetricKey =
                    createMetricKey(METRIC_SUFFIX_SYSTEM_TIMESTAMP_FORMATTED, serial);
            metrics.put(
                    formattedTimestampMetricKey,
                    stringToMetric(
                            timestampsSecs
                                    .stream()
                                    .map(t -> timestampToHoursMinutesSeconds(t))
                                    .collect(Collectors.joining(","))));
        }
    }

    /** Helper method to add metrics from the AppCrashOccurred atoms according to timestamps. */
    private void addAtomBasedMetrics(
            final Map<String, Metric> metrics, List<Long> timestampsNanos, String serial) {
        // If there are runtime restarts, add a comma-separated list of device uptime timestamps.
        if (!timestampsNanos.isEmpty()) {
            // Store both the raw timestamp and the formatted, more readable version.

            String uptimeNanosMetricKey = createMetricKey(METRIC_SUFFIX_UPTIME_NANOS, serial);
            metrics.put(
                    uptimeNanosMetricKey,
                    stringToMetric(
                            timestampsNanos
                                    .stream()
                                    .map(t -> String.valueOf(t))
                                    .collect(Collectors.joining(","))));

            String formattedUptimeMetricKey =
                    createMetricKey(METRIC_SUFFIX_UPTIME_FORMATTED, serial);
            metrics.put(
                    formattedUptimeMetricKey,
                    stringToMetric(
                            timestampsNanos
                                    .stream()
                                    .map(t -> nanosToHoursMinutesSeconds(t))
                                    .collect(Collectors.joining(","))));
        }
    }

    private String createMetricKey(String suffix, String serial) {
        return mIncludeDeviceSerial
                ? String.join(METRIC_SEP, METRIC_PREFIX, serial, suffix)
                : String.join(METRIC_SEP, METRIC_PREFIX, suffix);
    }

    /**
     * Forwarding logic to {@link ConfigUtil} for testing as it is a static utility class.
     *
     * @hide
     */
    @VisibleForTesting
    protected long pushStatsConfig(ITestDevice device, List<Integer> eventAtomIds)
            throws IOException, DeviceNotAvailableException {
        return ConfigUtil.pushStatsConfig(device, eventAtomIds);
    }

    /**
     * Forwarding logic to {@link ConfigUtil} for testing as it is a static utility class.
     *
     * @hide
     */
    @VisibleForTesting
    protected void removeConfig(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        ConfigUtil.removeConfig(device, configId);
    }

    /**
     * Forwarding logic to {@link MetricUtil} for testing as it is a static utility class.
     *
     * @hide
     */
    @VisibleForTesting
    protected StatsdStatsReport getStatsdMetadata(ITestDevice device)
            throws DeviceNotAvailableException, InvalidProtocolBufferException {
        return MetricUtil.getStatsdMetadata(device);
    }

    /**
     * Forwarding logic to {@link MetricUtil} for testing as it is a static utility class.
     *
     * @hide
     */
    @VisibleForTesting
    protected List<EventMetricData> getEventMetricData(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        return MetricUtil.getEventMetricData(device, configId);
    }

    /**
     * Helper method to convert nanoseconds to HH:mm:ss. {@link SimpleDateFormat} is not used here
     * as the input is not a real epoch-based timestamp.
     */
    private String nanosToHoursMinutesSeconds(long nanos) {
        long hours = TimeUnit.NANOSECONDS.toHours(nanos);
        nanos -= TimeUnit.HOURS.toNanos(hours);
        long minutes = TimeUnit.NANOSECONDS.toMinutes(nanos);
        nanos -= TimeUnit.MINUTES.toNanos(minutes);
        long seconds = TimeUnit.NANOSECONDS.toSeconds(nanos);
        return String.format("%02d:%02d:%02d", hours, minutes, seconds);
    }

    /** Helper method to format an epoch-based timestamp in seconds to HH:mm:ss. */
    private String timestampToHoursMinutesSeconds(int seconds) {
        return TIME_FORMATTER.format(new Date(TimeUnit.SECONDS.toMillis(seconds)));
    }

    /** Helper method to get all values in a list that appear after all occurrences of target. */
    private List<Integer> getAllValuesAfter(int target, List<Integer> l) {
        return l.subList(l.lastIndexOf(target) + 1, l.size());
    }
}
