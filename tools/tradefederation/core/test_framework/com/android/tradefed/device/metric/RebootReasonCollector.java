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
import com.android.os.AtomsProto.BootSequenceReported;
import com.android.os.StatsLog.EventMetricData;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.util.statsd.ConfigUtil;
import com.android.tradefed.util.statsd.MetricUtil;
import com.google.common.annotations.VisibleForTesting;

import java.io.IOException;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Collector that collects device reboot during the test run and report them by reason and counts.
 */
@OptionClass(alias = "reboot-reason-collector")
public class RebootReasonCollector extends BaseDeviceMetricCollector {
    private static final String METRIC_SEP = "-";
    public static final String METRIC_PREFIX = "rebooted" + METRIC_SEP;
    public static final String COUNT_KEY = String.join(METRIC_SEP, "reboot", "count");

    private List<ITestDevice> mTestDevices;
    // Map to store statsd config ids for each device, keyed by the device serial number.
    private Map<String, Long> mDeviceConfigIds = new HashMap<>();

    /** Push the statsd config to each device and store the config Ids. */
    @Override
    public void onTestRunStart(DeviceMetricData runData) {
        mTestDevices = getDevices();
        for (ITestDevice device : mTestDevices) {
            try {
                mDeviceConfigIds.put(
                        device.getSerialNumber(),
                        pushStatsConfig(
                                device, Arrays.asList(Atom.BOOT_SEQUENCE_REPORTED_FIELD_NUMBER)));
            } catch (DeviceNotAvailableException | IOException e) {
                // Error is not thrown as we still want to push the config to other devices.
                CLog.e(
                        "Failed to push statsd config to device %s. Exception: %s.",
                        device.getSerialNumber(), e.toString());
            }
        }
    }

    @Override
    public void onTestRunEnd(
            DeviceMetricData runData, final Map<String, Metric> currentRunMetrics) {
        for (ITestDevice device : mTestDevices) {
            List<EventMetricData> metricData = new ArrayList<>();
            if (!mDeviceConfigIds.containsKey(device.getSerialNumber())) {
                CLog.e("No config ID is associated with device %s.", device.getSerialNumber());
                continue;
            }
            long configId = mDeviceConfigIds.get(device.getSerialNumber());
            try {
                metricData.addAll(getEventMetricData(device, configId));
            } catch (DeviceNotAvailableException e) {
                CLog.e(
                        "Failed to pull metric data from device %s. Exception: %s.",
                        device.getSerialNumber(), e.toString());
            }
            Map<String, Integer> metricsForDevice = new HashMap<>();
            int rebootCount = 0;
            for (EventMetricData eventMetricEntry : metricData) {
                Atom eventAtom = eventMetricEntry.getAtom();
                if (eventAtom.hasBootSequenceReported()) {
                    rebootCount += 1;
                    BootSequenceReported bootAtom = eventAtom.getBootSequenceReported();
                    String bootReasonKey =
                            METRIC_PREFIX
                                    + String.join(
                                            METRIC_SEP,
                                            bootAtom.getBootloaderReason(),
                                            bootAtom.getSystemReason());
                    // Update the counts for the specific boot reason in the current atom.
                    metricsForDevice.computeIfPresent(bootReasonKey, (k, v) -> v + 1);
                    metricsForDevice.computeIfAbsent(bootReasonKey, k -> 1);
                }
            }
            for (String key : metricsForDevice.keySet()) {
                runData.addMetricForDevice(
                        device,
                        key,
                        stringToMetric(String.valueOf(metricsForDevice.get(key))).toBuilder());
            }
            // Add the count regardless of whether reboots occurred or not.
            runData.addMetricForDevice(
                    device, COUNT_KEY, stringToMetric(String.valueOf(rebootCount)).toBuilder());
            try {
                removeConfig(device, configId);
            } catch (DeviceNotAvailableException e) {
                // Error is not thrown as we still want to remove the config from other devices.
                CLog.e(
                        "Failed to remove statsd config from device %s. Exception: %s.",
                        device.getSerialNumber(), e.toString());
            }
        }
    }

    @VisibleForTesting
    long pushStatsConfig(ITestDevice device, List<Integer> eventAtomIds)
            throws IOException, DeviceNotAvailableException {
        return ConfigUtil.pushStatsConfig(device, eventAtomIds);
    }

    @VisibleForTesting
    void removeConfig(ITestDevice device, long configId) throws DeviceNotAvailableException {
        ConfigUtil.removeConfig(device, configId);
    }

    @VisibleForTesting
    List<EventMetricData> getEventMetricData(ITestDevice device, long configId)
            throws DeviceNotAvailableException {
        return MetricUtil.getEventMetricData(device, configId);
    }
}
