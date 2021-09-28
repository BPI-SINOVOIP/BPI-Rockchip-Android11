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

import com.android.os.AtomsProto.AppStartFullyDrawn;
import com.android.os.AtomsProto.AppStartOccurred;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.ProcessStartTime;
import com.android.os.StatsLog.EventMetricData;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.stream.Collectors;

/**
 * AppStartupHelper consist of helper methods to set the app
 * startup configs in statsd to track the app startup related
 * performance metrics and retrieve the necessary information from
 * statsd using the config id.
 */
public class AppStartupHelper implements ICollectorHelper<StringBuilder> {

    private static final String LOG_TAG = AppStartupHelper.class.getSimpleName();

    private static final String COLD_STARTUP = "cold_startup";
    private static final String WARM_STARTUP = "warm_startup";
    private static final String HOT_STARTUP = "hot_startup";
    private static final String COUNT = "count";
    private static final String TOTAL_COUNT = "total_count";

    private static final String STARTUP_FULLY_DRAWN_UNKNOWN = "startup_fully_drawn_unknown";
    private static final String STARTUP_FULLY_DRAWN_WITH_BUNDLE = "startup_fully_drawn_with_bundle";
    private static final String STARTUP_FULLY_DRAWN_WITHOUT_BUNDLE =
            "startup_fully_drawn_without_bundle";

    private static final String PROCESS_START = "process_start";
    private static final String PROCESS_START_DELAY = "process_start_delay";
    private static final String TRANSITION_DELAY_MILLIS = "transition_delay_millis";
    private boolean isProcStartDetailsDisabled;

    private StatsdHelper mStatsdHelper = new StatsdHelper();

    /**
     * Set up the app startup statsd config to track the metrics during the app start occurred.
     */
    @Override
    public boolean startCollecting() {
        Log.i(LOG_TAG, "Adding app startup configs to statsd.");
        List<Integer> atomIdList = new ArrayList<>();
        atomIdList.add(Atom.APP_START_OCCURRED_FIELD_NUMBER);
        atomIdList.add(Atom.APP_START_FULLY_DRAWN_FIELD_NUMBER);
        if (!isProcStartDetailsDisabled) {
            atomIdList.add(Atom.PROCESS_START_TIME_FIELD_NUMBER);
        }
        return mStatsdHelper.addEventConfig(atomIdList);
    }

    /**
     * Collect the app startup metrics tracked during the app startup occurred from the statsd.
     */
    @Override
    public Map<String, StringBuilder> getMetrics() {
        List<EventMetricData> eventMetricData = mStatsdHelper.getEventMetrics();
        Map<String, StringBuilder> appStartResultMap = new HashMap<>();
        Map<String, Integer> appStartCountMap = new HashMap<>();
        Map<String, Integer> tempResultCountMap = new HashMap<>();
        for (EventMetricData dataItem : eventMetricData) {
            Atom atom = dataItem.getAtom();
            if (atom.hasAppStartOccurred()) {
                AppStartOccurred appStartAtom = atom.getAppStartOccurred();
                String pkgName = appStartAtom.getPkgName();
                String transitionType = appStartAtom.getType().toString();
                int windowsDrawnMillis = appStartAtom.getWindowsDrawnDelayMillis();
                int transitionDelayMillis = appStartAtom.getTransitionDelayMillis();
                Log.i(LOG_TAG, String.format("Pkg Name: %s, Transition Type: %s, "
                        + "WindowDrawnDelayMillis: %s, TransitionDelayMillis: %s",
                        pkgName, transitionType, windowsDrawnMillis, transitionDelayMillis));

                String metricTypeKey = "";
                String metricTransitionKey = "";
                // To track number of startups per type per package.
                String metricCountKey = "";
                // To track total number of startups per type.
                String totalCountKey = "";
                String typeKey = "";
                switch (appStartAtom.getType()) {
                    case COLD:
                        typeKey = COLD_STARTUP;
                        break;
                    case WARM:
                        typeKey = WARM_STARTUP;
                        break;
                    case HOT:
                        typeKey = HOT_STARTUP;
                        break;
                    case UNKNOWN:
                        break;
                }
                if (!typeKey.isEmpty()) {
                    metricTypeKey = MetricUtility.constructKey(typeKey, pkgName);
                    metricCountKey = MetricUtility.constructKey(typeKey, COUNT, pkgName);
                    totalCountKey = MetricUtility.constructKey(typeKey, TOTAL_COUNT);

                    // Update the windows drawn delay metrics.
                    MetricUtility.addMetric(metricTypeKey, windowsDrawnMillis, appStartResultMap);
                    MetricUtility.addMetric(metricCountKey, appStartCountMap);
                    MetricUtility.addMetric(totalCountKey, appStartCountMap);

                    // Update the transition delay metrics.
                    metricTransitionKey = MetricUtility.constructKey(typeKey,
                            TRANSITION_DELAY_MILLIS, pkgName);
                    MetricUtility.addMetric(metricTransitionKey, transitionDelayMillis,
                            appStartResultMap);
                }
            }
            if (atom.hasAppStartFullyDrawn()) {
                AppStartFullyDrawn appFullyDrawnAtom = atom.getAppStartFullyDrawn();
                String pkgName = appFullyDrawnAtom.getPkgName();
                String transitionType = appFullyDrawnAtom.getType().toString();
                long startupTimeMillis = appFullyDrawnAtom.getAppStartupTimeMillis();
                Log.i(LOG_TAG, String.format("Pkg Name: %s, Transition Type: %s, "
                        + "AppStartupTimeMillis: %d", pkgName, transitionType, startupTimeMillis));

                String metricKey = "";
                switch (appFullyDrawnAtom.getType()) {
                    case UNKNOWN:
                        metricKey = MetricUtility.constructKey(
                                STARTUP_FULLY_DRAWN_UNKNOWN, pkgName);
                        break;
                    case WITH_BUNDLE:
                        metricKey = MetricUtility.constructKey(
                                STARTUP_FULLY_DRAWN_WITH_BUNDLE, pkgName);
                        break;
                    case WITHOUT_BUNDLE:
                        metricKey = MetricUtility.constructKey(
                                STARTUP_FULLY_DRAWN_WITHOUT_BUNDLE, pkgName);
                        break;
                }
                if (!metricKey.isEmpty()) {
                    MetricUtility.addMetric(metricKey, startupTimeMillis, appStartResultMap);
                }
            }
            // ProcessStartTime reports startup time for both foreground and background process.
            if (atom.hasProcessStartTime()) {
                ProcessStartTime processStartTimeAtom = atom.getProcessStartTime();
                String processName = processStartTimeAtom.getProcessName();
                // Number of milliseconds it takes to finish start of the process.
                long processStartDelayMillis = processStartTimeAtom.getProcessStartDelayMillis();
                // Treating activity hosting type as foreground and everything else as background.
                String hostingType = processStartTimeAtom.getHostingType().equals("activity")
                        ? "fg" : "bg";
                Log.i(LOG_TAG, String.format("Process Name: %s, Start Type: %s, Hosting Type: %s,"
                        + " ProcessStartDelayMillis: %d", processName,
                        processStartTimeAtom.getType().toString(),
                        hostingType, processStartDelayMillis));

                String metricKey = "";
                // To track number of startups per type per package.
                String metricCountKey = "";
                // To track total number of startups per type.
                String totalCountKey = "";
                String typeKey = "";
                switch (processStartTimeAtom.getType()) {
                    case COLD:
                        typeKey = COLD_STARTUP;
                        break;
                    case WARM:
                        typeKey = WARM_STARTUP;
                        break;
                    case HOT:
                        typeKey = HOT_STARTUP;
                        break;
                    case UNKNOWN:
                        break;
                }
                if (!typeKey.isEmpty()) {
                    metricKey = MetricUtility.constructKey(typeKey,
                            PROCESS_START_DELAY, processName, hostingType);
                    metricCountKey = MetricUtility.constructKey(typeKey, PROCESS_START,
                            COUNT, processName, hostingType);
                    totalCountKey = MetricUtility.constructKey(typeKey, PROCESS_START,
                            TOTAL_COUNT);
                    // Update the metrics
                    if (isProcStartDetailsDisabled) {
                        MetricUtility.addMetric(metricCountKey, tempResultCountMap);
                    } else {
                        MetricUtility.addMetric(metricKey, processStartDelayMillis,
                                appStartResultMap);
                        MetricUtility.addMetric(metricCountKey, appStartCountMap);
                    }

                    MetricUtility.addMetric(totalCountKey, appStartCountMap);
                }
            }
        }

        if (isProcStartDetailsDisabled) {
            for (Entry<String, Integer> entry : tempResultCountMap.entrySet()) {
                Log.i(LOG_TAG, String.format("Process_delay_key: %s, Count: %d", entry.getKey(),
                        entry.getValue()));
            }
        }

        // Cast to StringBuilder as the raw app startup metric could be comma separated values
        // if there are multiple app launches.
        Map<String, StringBuilder> finalCountMap = appStartCountMap
                .entrySet()
                .stream()
                .collect(
                        Collectors.toMap(Map.Entry::getKey,
                                e -> new StringBuilder(Integer.toString(e.getValue()))));
        // Add the count map in the app start result map.
        appStartResultMap.putAll(finalCountMap);
        return appStartResultMap;
    }

    /**
     * Remove the statsd config used to track the app startup metrics.
     */
    @Override
    public boolean stopCollecting() {
        return mStatsdHelper.removeStatsConfig();
    }

    /**
     * Disable process start detailed metrics.
     */
    public void setDisableProcStartDetails() {
        isProcStartDetailsDisabled = true;
    }
}
