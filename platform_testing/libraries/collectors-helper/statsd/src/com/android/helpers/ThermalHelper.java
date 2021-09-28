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

package com.android.helpers;

import android.os.TemperatureTypeEnum;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.EventMetricData;

import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * ThermalHelper is a helper class to collect thermal events from statsd. Currently, it identifies
 * severity state changes.
 */
public class ThermalHelper implements ICollectorHelper<StringBuilder> {
    private static final String LOG_TAG = ThermalHelper.class.getSimpleName();

    private static final int UNDEFINED_SEVERITY = -1;
    private static final Pattern SEVERITY_DUMPSYS_PATTERN =
            Pattern.compile("Thermal Status: (\\d+)");

    private StatsdHelper mStatsdHelper;
    private UiDevice mDevice;
    private int mInitialSeverity;

    /** Set up the statsd config to track thermal events. */
    @Override
    public boolean startCollecting() {
        // Add an initial value because this only detects changes.
        mInitialSeverity = UNDEFINED_SEVERITY;
        try {
            String[] output = getDevice().executeShellCommand("dumpsys thermalservice").split("\n");
            for (String line : output) {
                Matcher severityMatcher = SEVERITY_DUMPSYS_PATTERN.matcher(line);
                if (severityMatcher.matches()) {
                    mInitialSeverity = Integer.parseInt(severityMatcher.group(1));
                    Log.v(LOG_TAG, String.format("Initial severity: %s.", mInitialSeverity));
                }
            }
        } catch (NumberFormatException nfe) {
            Log.w(LOG_TAG, String.format("Couldn't identify severity. Error parsing: %s", nfe));
            return false;
        } catch (IOException ioe) {
            Log.w(LOG_TAG, String.format("Failed to query thermalservice. Error: %s", ioe));
            return false;
        }

        // Skip this monitor if severity isn't identified.
        if (mInitialSeverity == UNDEFINED_SEVERITY) {
            Log.w(LOG_TAG, "Did not find an initial severity. Quitting.");
            return false;
        }

        // Register the thermal event config to statsd.
        List<Integer> atomIdList = new ArrayList<>();
        atomIdList.add(Atom.THERMAL_THROTTLING_SEVERITY_STATE_CHANGED_FIELD_NUMBER);
        return getStatsdHelper().addEventConfig(atomIdList);
    }

    /** Collect the thermal events that occurred during the test. */
    @Override
    public Map<String, StringBuilder> getMetrics() {
        Map<String, StringBuilder> results = new HashMap<>();

        // Add the initial severity value every time metrics are collected.
        String severityKey = MetricUtility.constructKey("thermal", "throttling", "severity");
        MetricUtility.addMetric(severityKey, mInitialSeverity, results);

        List<EventMetricData> eventMetricData = getStatsdHelper().getEventMetrics();
        Log.i(LOG_TAG, String.format("%d thermal data points found.", eventMetricData.size()));
        // Collect all thermal throttling severity state change events.
        for (EventMetricData dataItem : eventMetricData) {
            if (dataItem.getAtom().hasThermalThrottlingSeverityStateChanged()) {
                // TODO(b/137878503): Add elapsed_timestamp_nanos for timpestamp data.
                // Get thermal throttling severity state change data point.
                int severity =
                        dataItem.getAtom()
                                .getThermalThrottlingSeverityStateChanged()
                                .getSeverity()
                                .getNumber();
                // Store the severity state change ignoring where the measurement came from.
                MetricUtility.addMetric(severityKey, severity, results);
                // Set the initial severity to the last value, in case #getMetrics is called again.
                mInitialSeverity = severity;
            }
        }

        return results;
    }

    /** Remove the statsd config used to track thermal events. */
    @Override
    public boolean stopCollecting() {
        Log.i(LOG_TAG, "Unregistering thermal config from statsd.");
        return getStatsdHelper().removeStatsConfig();
    }

    /** A shorthand name for temperature sensor types used in metric keys. */
    @VisibleForTesting
    static String getShorthandSensorType(TemperatureTypeEnum type) {
        switch (type) {
            case TEMPERATURE_TYPE_CPU:
                return "cpu";

            case TEMPERATURE_TYPE_GPU:
                return "gpu";

            case TEMPERATURE_TYPE_BATTERY:
                return "battery";

            case TEMPERATURE_TYPE_SKIN:
                return "skin";

            case TEMPERATURE_TYPE_USB_PORT:
                return "usb_port";

            case TEMPERATURE_TYPE_POWER_AMPLIFIER:
                return "power_amplifier";

            default:
                return "unknown";
        }
    }

    private StatsdHelper getStatsdHelper() {
        if (mStatsdHelper == null) {
            mStatsdHelper = new StatsdHelper();
        }
        return mStatsdHelper;
    }

    @VisibleForTesting
    void setStatsdHelper(StatsdHelper helper) {
        mStatsdHelper = helper;
    }

    private UiDevice getDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        }
        return mDevice;
    }

    @VisibleForTesting
    void setUiDevice(UiDevice device) {
        mDevice = device;
    }
}
