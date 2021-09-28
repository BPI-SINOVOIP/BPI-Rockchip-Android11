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

import com.android.os.AtomsProto.ANROccurred;
import com.android.os.AtomsProto.AppCrashOccurred;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.EventMetricData;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * CrashHelper consist of helper methods to set the crash collection
 * configs in statsd to track the crashes happened during the test
 * and retrieve the necessary information from statsd using the config id.
 */
public class CrashHelper implements ICollectorHelper<Integer> {

    private static final String LOG_TAG = CrashHelper.class.getSimpleName();
    private static final String TOTAL_PREFIX = "total_";
    private static final String EVENT_JAVA_CRASH = "crash";
    private static final String EVENT_NATIVE_CRASH = "native_crash";
    private static final String EVENT_ANR = "anr";

    private StatsdHelper mStatsdHelper = new StatsdHelper();

    /**
     * Set up the app crash statsd config to track the crash metrics during the test.
     */
    @Override
    public boolean startCollecting() {
        Log.i(LOG_TAG, "Adding AppCrashOccured config to statsd.");
        List<Integer> atomIdList = new ArrayList<>();
        atomIdList.add(Atom.APP_CRASH_OCCURRED_FIELD_NUMBER);
        atomIdList.add(Atom.ANR_OCCURRED_FIELD_NUMBER);
        return mStatsdHelper.addEventConfig(atomIdList);
    }

    /**
     * Collect the app crash metrics tracked during the test from the statsd.
     */
    @Override
    public Map<String, Integer> getMetrics() {
        List<EventMetricData> eventMetricData = mStatsdHelper.getEventMetrics();
        Map<String, Integer> appCrashResultMap = new HashMap<>();
        // We need this because even if there are no crashes we need to report 0 count
        // in the dashboard for the total crash, native crash and ANR.
        appCrashResultMap.put(TOTAL_PREFIX + EVENT_JAVA_CRASH, 0);
        appCrashResultMap.put(TOTAL_PREFIX + EVENT_NATIVE_CRASH, 0);
        appCrashResultMap.put(TOTAL_PREFIX + EVENT_ANR, 0);
        for (EventMetricData dataItem : eventMetricData) {
            if (dataItem.getAtom().hasAppCrashOccurred()) {
                AppCrashOccurred appCrashAtom = dataItem.getAtom().getAppCrashOccurred();
                String eventType = appCrashAtom.getEventType();
                String pkgName = appCrashAtom.getPackageName();
                AppCrashOccurred.ForegroundState foregroundState =
                        appCrashAtom.getForegroundState();
                Log.i(LOG_TAG, String.format("Event Type:%s Pkg Name: %s "
                        + " ForegroundState: %s", eventType, pkgName, foregroundState.toString()));

                // Track the total crash and native crash count.
                MetricUtility.addMetric(TOTAL_PREFIX + eventType, appCrashResultMap);
                // Add more detailed crash count key metrics.
                String detailKey = MetricUtility.constructKey(eventType, pkgName,
                        foregroundState.toString());
                MetricUtility.addMetric(detailKey, appCrashResultMap);
            } else if (dataItem.getAtom().hasAnrOccurred()) {
                ANROccurred anrAtom = dataItem.getAtom().getAnrOccurred();
                String processName = anrAtom.getProcessName();
                String reason = anrAtom.getReason();
                ANROccurred.ForegroundState foregoundState = anrAtom.getForegroundState();
                Log.i(LOG_TAG,
                        String.format("ANR occurred in process %s due to %s; foregound state is %s",
                                processName, reason, foregoundState.toString()));

                // Track the total ANR count.
                MetricUtility.addMetric(TOTAL_PREFIX + EVENT_ANR, appCrashResultMap);
                String detailKey =
                        MetricUtility.constructKey(
                                EVENT_ANR, processName, foregoundState.toString());
                MetricUtility.addMetric(detailKey, appCrashResultMap);
            }
        }
        return appCrashResultMap;
    }

    /**
     * Remove the statsd config used to track the app crash metrics.
     */
    @Override
    public boolean stopCollecting() {
        return mStatsdHelper.removeStatsConfig();
    }
}
