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

package com.android.helpers;

import android.util.Log;

import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.GaugeBucketInfo;
import com.android.os.StatsLog.GaugeMetricData;

import com.google.common.collect.ArrayListMultimap;
import com.google.common.collect.ListMultimap;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * CpuUsageHelper consist of helper methods to set the app
 * cpu usage configs in statsd to track the cpu usage related
 * metrics and retrieve the necessary information from statsd
 * using the config id.
 */
public class CpuUsageHelper implements ICollectorHelper<Long> {

    private static final String LOG_TAG = CpuUsageHelper.class.getSimpleName();
    private static final String CPU_USAGE_PKG_UID = "cpu_usage_pkg_or_uid";
    private static final String CPU_USAGE_FREQ = "cpu_usage_freq";
    private static final String TOTAL_CPU_USAGE = "total_cpu_usage";
    private static final String TOTAL_CPU_USAGE_FREQ = "total_cpu_usage_freq";
    private static final String CLUSTER_ID = "cluster";
    private static final String FREQ_INDEX = "freq_index";
    private static final String USER_TIME = "user_time";
    private static final String SYSTEM_TIME = "system_time";
    private static final String TOTAL_CPU_TIME = "total_cpu_time";
    private static final String CPU_UTILIZATION = "cpu_utilization_average_per_core_percent";

    private StatsdHelper mStatsdHelper = new StatsdHelper();
    private boolean isPerFreqDisabled;
    private boolean isPerPkgDisabled;
    private boolean isTotalPkgDisabled;
    private boolean isTotalFreqDisabled;
    private boolean isCpuUtilizationEnabled;
    private long mStartTime;
    private long mEndTime;
    private Integer mCpuCores = null;

    @Override
    public boolean startCollecting() {
        Log.i(LOG_TAG, "Adding CpuUsage config to statsd.");
        List<Integer> atomIdList = new ArrayList<>();
        // Add the atoms to be tracked.
        atomIdList.add(Atom.CPU_TIME_PER_UID_FIELD_NUMBER);
        atomIdList.add(Atom.CPU_TIME_PER_FREQ_FIELD_NUMBER);

        if (isCpuUtilizationEnabled) {
            mStartTime = System.currentTimeMillis();
        }

        return mStatsdHelper.addGaugeConfig(atomIdList);
    }

    @Override
    public Map<String, Long> getMetrics() {
        Map<String, Long> cpuUsageFinalMap = new HashMap<>();

        List<GaugeMetricData> gaugeMetricList = mStatsdHelper.getGaugeMetrics();

        if (isCpuUtilizationEnabled) {
            mEndTime = System.currentTimeMillis();
        }

        ListMultimap<String, Long> cpuUsageMap = ArrayListMultimap.create();

        for (GaugeMetricData gaugeMetric : gaugeMetricList) {
            Log.v(LOG_TAG, "Bucket Size: " + gaugeMetric.getBucketInfoCount());
            for (GaugeBucketInfo gaugeBucketInfo : gaugeMetric.getBucketInfoList()) {
                for (Atom atom : gaugeBucketInfo.getAtomList()) {

                    // Track CPU usage in user time and system time per package or UID
                    if (atom.getCpuTimePerUid().hasUid()) {
                        int uId = atom.getCpuTimePerUid().getUid();
                        String packageName = mStatsdHelper.getPackageName(uId);
                        // Convert to milliseconds to compare with CpuTimePerFreq
                        long userTimeMillis = atom.getCpuTimePerUid().getUserTimeMicros() / 1000;
                        long sysTimeMillis = atom.getCpuTimePerUid().getSysTimeMicros() / 1000;
                        Log.v(LOG_TAG, String.format("Uid:%d, Pkg Name: %s, User_Time: %d,"
                                + " System_Time: %d", uId, packageName, userTimeMillis,
                                sysTimeMillis));

                        // Use the package name if exist for the UID otherwise use the UID.
                        // Note: UID for the apps will be different across the builds.

                        // It is possible to have multiple bucket info. Track all the gauge info
                        // and take the difference of the first and last to compute the
                        // final usage.

                        // Add uid suffix to CPU_USAGE_PKG_UID type of key to tell apart processes
                        // with the same package name but different uid in multi-user situations,
                        // so that cpu gauge info won't get mixed up when taking the difference. All
                        // uid suffix will be removed subsequently in final result.

                        String UserTimeKey = MetricUtility.constructKey(CPU_USAGE_PKG_UID,
                                (packageName == null) ? String.valueOf(uId) : packageName,
                                USER_TIME, String.valueOf(uId));
                        String SystemTimeKey = MetricUtility.constructKey(CPU_USAGE_PKG_UID,
                                (packageName == null) ? String.valueOf(uId) : packageName,
                                SYSTEM_TIME, String.valueOf(uId));
                        cpuUsageMap.put(UserTimeKey, userTimeMillis);
                        cpuUsageMap.put(SystemTimeKey, sysTimeMillis);
                    }

                    // Track cpu usage per cluster_id and freq_index
                    if (atom.getCpuTimePerFreq().hasFreqIndex()) {
                        int clusterId = atom.getCpuTimePerFreq().getCluster();
                        int freqIndex = atom.getCpuTimePerFreq().getFreqIndex();
                        long timeInFreq = atom.getCpuTimePerFreq().getTimeMillis();
                        Log.v(LOG_TAG, String.format("Cluster Id: %d FreqIndex: %d,"
                                + " Time_in_Freq: %d", clusterId, freqIndex, timeInFreq));
                        String finalFreqIndexKey = MetricUtility.constructKey(
                                CPU_USAGE_FREQ, CLUSTER_ID, String.valueOf(clusterId), FREQ_INDEX,
                                String.valueOf(freqIndex));
                        cpuUsageMap.put(finalFreqIndexKey, timeInFreq);
                    }

                }
            }
        }

        // Compute the final result map
        Long totalCpuUsage = 0L;
        Long totalCpuFreq = 0L;
        for (String key : cpuUsageMap.keySet()) {
            List<Long> cpuUsageList = cpuUsageMap.get(key);
            if (cpuUsageList.size() > 1) {
                // Compute the total usage by taking the difference of last and first value.
                Long cpuUsage = cpuUsageList.get(cpuUsageList.size() - 1)
                        - cpuUsageList.get(0);
                // Add the final result only if the cpu usage is greater than 0.
                if (cpuUsage > 0) {
                    if (key.startsWith(CPU_USAGE_PKG_UID)
                            && !isPerPkgDisabled) {
                        // remove uid suffix from key.
                        String finalKey = key.substring(0, key.lastIndexOf('_'));
                        if (cpuUsageFinalMap.containsKey(finalKey)) {
                            // accumulate cpu time of the same package from different uid.
                            cpuUsageFinalMap.put(
                                    finalKey, cpuUsage + cpuUsageFinalMap.get(finalKey));
                        } else {
                            cpuUsageFinalMap.put(finalKey, cpuUsage);
                        }
                    }
                    if (key.startsWith(CPU_USAGE_FREQ)
                            && !isPerFreqDisabled) {
                        cpuUsageFinalMap.put(key, cpuUsage);
                    }
                }
                // Add the CPU time to their respective (usage or frequency) total metric.
                if (key.startsWith(CPU_USAGE_PKG_UID)
                        && !isTotalPkgDisabled) {
                    totalCpuUsage += cpuUsage;
                } else if (key.startsWith(CPU_USAGE_FREQ)
                        && !isTotalFreqDisabled) {
                    totalCpuFreq += cpuUsage;
                }
            }
        }
        // Put the total results into the final result map.
        if (!isTotalPkgDisabled) {
            cpuUsageFinalMap.put(TOTAL_CPU_USAGE, totalCpuUsage);
        }
        if (!isTotalFreqDisabled) {
            cpuUsageFinalMap.put(TOTAL_CPU_USAGE_FREQ, totalCpuFreq);
        }

        // Calculate cpu utilization
        if (isCpuUtilizationEnabled) {
            long totalCpuTime = (mEndTime - mStartTime) * getCores();
            cpuUsageFinalMap.put(TOTAL_CPU_TIME, totalCpuTime);
            if (!isTotalPkgDisabled) {
                double utilization =
                        ((double) cpuUsageFinalMap.get(TOTAL_CPU_USAGE) / totalCpuTime) * 100;
                cpuUsageFinalMap.put(CPU_UTILIZATION, (long) utilization);
            }
        }

        return cpuUsageFinalMap;
    }

    /**
     * Remove the statsd config used to track the cpu usage metrics.
     */
    @Override
    public boolean stopCollecting() {
        return mStatsdHelper.removeStatsConfig();
    }

    /**
     * Disable the cpu metric collection per package.
     */
    public void setDisablePerPackage() {
        isPerPkgDisabled = true;
    }

    /**
     * Disable the cpu metric collection per frequency.
     */
    public void setDisablePerFrequency() {
        isPerFreqDisabled = true;
    }

    /**
     * Disable the total cpu metric collection by all the packages.
     */
    public void setDisableTotalPackage() {
        isTotalPkgDisabled = true;
    }

    /**
     * Disable the total cpu metric collection by all the frequency.
     */
    public void setDisableTotalFrequency() {
        isTotalFreqDisabled = true;
    }

    /**
     * Enable the collection of cpu utilization.
     */
    public void setEnableCpuUtilization() {
        isCpuUtilizationEnabled = true;
    }

    /**
     * return the number of cores that the device has.
     */
    private int getCores() {
        if (mCpuCores == null) {
            int count = 0;
            // every core corresponds to a folder named "cpu[0-9]+" under /sys/devices/system/cpu
            File cpuDir = new File("/sys/devices/system/cpu");
            File[] files = cpuDir.listFiles();
            for (File file : files) {
                if (Pattern.matches("cpu[0-9]+", file.getName())) {
                    ++count;
                }
            }
            mCpuCores = count;
        }
        return mCpuCores.intValue();
    }
}
