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

import android.content.Context;
import android.app.StatsManager;
import android.app.StatsManager.StatsUnavailableException;
import android.os.SystemClock;
import android.util.Log;
import android.util.StatsLog;
import androidx.test.InstrumentationRegistry;

import com.android.internal.os.StatsdConfigProto.AtomMatcher;
import com.android.internal.os.StatsdConfigProto.EventMetric;
import com.android.internal.os.StatsdConfigProto.FieldFilter;
import com.android.internal.os.StatsdConfigProto.GaugeMetric;
import com.android.internal.os.StatsdConfigProto.SimpleAtomMatcher;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.internal.os.StatsdConfigProto.TimeUnit;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.GaugeMetricData;
import com.android.os.StatsLog.StatsLogReport;

import com.google.protobuf.InvalidProtocolBufferException;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

/**
 * StatsdHelper consist of basic utilities that will be used to setup statsd
 * config, parse the collected information and remove the statsd config.
 */
public class StatsdHelper {
    private static final String LOG_TAG = StatsdHelper.class.getSimpleName();
    private static final long MAX_ATOMS = 2000;
    private static final long METRIC_DELAY_MS = 3000;
    private long mConfigId = -1;
    private StatsManager mStatsManager;

    /**
     * Add simple event configurations using a list of atom ids.
     *
     * @param atomIdList uniquely identifies the information that we need to track by statsManager.
     * @return true if the configuration is added successfully, otherwise false.
     */
    public boolean addEventConfig(List<Integer> atomIdList) {
        long configId = System.currentTimeMillis();
        StatsdConfig.Builder statsConfigBuilder = getSimpleSources(configId);

        for (Integer atomId : atomIdList) {
            int atomUniqueId = getUniqueId();
            statsConfigBuilder
                    .addEventMetric(
                            EventMetric.newBuilder()
                                    .setId(getUniqueId())
                                    .setWhat(atomUniqueId))
                    .addAtomMatcher(getSimpleAtomMatcher(atomUniqueId, atomId));
        }
        try {
            adoptShellIdentity();
            getStatsManager().addConfig(configId, statsConfigBuilder.build().toByteArray());
            dropShellIdentity();
        } catch (Exception e) {
            Log.e(LOG_TAG, "Not able to setup the event config.", e);
            return false;
        }
        Log.i(LOG_TAG, "Successfully added config with config-id:" + configId);
        setConfigId(configId);
        return true;
    }

    /**
     * Build gauge metric config based on trigger events (i.e AppBreadCrumbReported).
     * Whenever the events are triggered via StatsLog.logEvent() collect the gauge metrics.
     * It doesn't matter what the log event is. It could be 0 or 1.
     * In order to capture the usage during the test take the difference of gauge metrics
     * before and after the test.
     *
     * @param atomIdList List of atoms to be collected in gauge metrics.
     * @return if the config is added successfully otherwise false.
     */
    public boolean addGaugeConfig(List<Integer> atomIdList) {
        long configId = System.currentTimeMillis();
        StatsdConfig.Builder statsConfigBuilder = getSimpleSources(configId);
        int appBreadCrumbUniqueId = getUniqueId();

        // Needed for collecting gauge metric based on trigger events.
        statsConfigBuilder.addAtomMatcher(getSimpleAtomMatcher(appBreadCrumbUniqueId,
                Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER));
        statsConfigBuilder.addWhitelistedAtomIds(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER);

        for (Integer atomId : atomIdList) {
            int atomUniqueId = getUniqueId();
            // Build Gauge metric config.
            GaugeMetric.Builder gaugeMetric = GaugeMetric.newBuilder()
                    .setId(getUniqueId())
                    .setWhat(atomUniqueId)
                    .setGaugeFieldsFilter(FieldFilter.newBuilder().setIncludeAll(true).build())
                    .setMaxNumGaugeAtomsPerBucket(MAX_ATOMS)
                    .setSamplingType(GaugeMetric.SamplingType.FIRST_N_SAMPLES)
                    .setTriggerEvent(appBreadCrumbUniqueId)
                    .setBucket(TimeUnit.CTS);

            // add the gauge config.
            statsConfigBuilder.addAtomMatcher(getSimpleAtomMatcher(atomUniqueId, atomId))
                    .addGaugeMetric(gaugeMetric.build());
        }

        try {
            adoptShellIdentity();
            getStatsManager().addConfig(configId,
                    statsConfigBuilder.build().toByteArray());
            StatsLog.logEvent(0);
            // Dump the counters before the test started.
            SystemClock.sleep(METRIC_DELAY_MS);
            dropShellIdentity();
        } catch (Exception e) {
            Log.e(LOG_TAG, "Not able to setup the gauge config.", e);
            return false;
        }

        Log.i(LOG_TAG, "Successfully added config with config-id:" + configId);
        setConfigId(configId);
        return true;
    }

    /**
     * Create simple atom matcher with the given id and the field id.
     *
     * @param id
     * @param fieldId
     * @return
     */
    private AtomMatcher.Builder getSimpleAtomMatcher(int id, int fieldId) {
        return AtomMatcher.newBuilder()
                .setId(id)
                .setSimpleAtomMatcher(SimpleAtomMatcher.newBuilder()
                        .setAtomId(fieldId));
    }

    /**
     * List of authorized source that can write the information into statsd.
     *
     * @param configId unique id of the configuration tracked by StatsManager.
     * @return
     */
    private static StatsdConfig.Builder getSimpleSources(long configId) {
        return StatsdConfig.newBuilder()
                .setId(configId)
                .addAllowedLogSource("AID_ROOT")
                .addAllowedLogSource("AID_SYSTEM")
                .addAllowedLogSource("AID_RADIO")
                .addAllowedLogSource("AID_BLUETOOTH")
                .addAllowedLogSource("AID_GRAPHICS")
                .addAllowedLogSource("AID_STATSD")
                .addAllowedLogSource("AID_INCIENTD")
                .addDefaultPullPackages("AID_SYSTEM")
                .addDefaultPullPackages("AID_RADIO")
                .addDefaultPullPackages("AID_STATSD")
                .addDefaultPullPackages("AID_GPU_SERVICE");
    }

    /**
     * Returns the list of EventMetricData tracked under the config.
     */
    public List<EventMetricData> getEventMetrics() {
        ConfigMetricsReportList reportList = null;
        List<EventMetricData> eventData = new ArrayList<>();
        try {
            if (getConfigId() != -1) {
                adoptShellIdentity();
                reportList = ConfigMetricsReportList.parser()
                        .parseFrom(getStatsManager().getReports(getConfigId()));
                dropShellIdentity();
            }
        } catch (InvalidProtocolBufferException | StatsUnavailableException se) {
            Log.e(LOG_TAG, "Retreiving event metrics failed.", se);
            return eventData;
        }

        if (reportList != null) {
            ConfigMetricsReport configReport = reportList.getReports(0);
            for (StatsLogReport metric : configReport.getMetricsList()) {
                eventData.addAll(metric.getEventMetrics().getDataList());
            }
        }
        Log.i(LOG_TAG, "Number of events: " + eventData.size());
        return eventData;
    }

    /**
     * Returns the list of GaugeMetric data tracked under the config.
     */
    public List<GaugeMetricData> getGaugeMetrics() {
        ConfigMetricsReportList reportList = null;
        List<GaugeMetricData> gaugeData = new ArrayList<>();
        try {
            if (getConfigId() != -1) {
                adoptShellIdentity();
                StatsLog.logEvent(0);
                // Dump the the counters after the test completed.
                SystemClock.sleep(METRIC_DELAY_MS);
                reportList = ConfigMetricsReportList.parser()
                        .parseFrom(getStatsManager().getReports(getConfigId()));
                dropShellIdentity();
            }
        } catch (InvalidProtocolBufferException | StatsUnavailableException se) {
            Log.e(LOG_TAG, "Retreiving gauge metrics failed.", se);
            return gaugeData;
        }

        if (reportList != null) {
            ConfigMetricsReport configReport = reportList.getReports(0);
            for (StatsLogReport metric : configReport.getMetricsList()) {
                gaugeData.addAll(metric.getGaugeMetrics().getDataList());
            }
        }
        Log.i(LOG_TAG, "Number of Gauge data: " + gaugeData.size());
        return gaugeData;
    }

    /**
     * Remove the existing config tracked in the statsd.
     *
     * @return true if the config is removed successfully otherwise false.
     */
    public boolean removeStatsConfig() {
        Log.i(LOG_TAG, "Removing statsd config-id: " + getConfigId());
        try {
            adoptShellIdentity();
            getStatsManager().removeConfig(getConfigId());
            dropShellIdentity();
            Log.i(LOG_TAG, "Successfully removed config-id: " + getConfigId());
            return true;
        } catch (StatsUnavailableException e) {
            Log.e(LOG_TAG, String.format("Not able to remove the config-id: %d due to %s ",
                    getConfigId(), e.getMessage()));
            return false;
        }
    }

    /**
     * StatsManager used to configure, collect and remove the statsd config.
     *
     * @return StatsManager
     */
    private StatsManager getStatsManager() {
        if (mStatsManager == null) {
            mStatsManager = (StatsManager) InstrumentationRegistry.getTargetContext().
                    getSystemService(Context.STATS_MANAGER);
        }
        return mStatsManager;
    }

    /**
     * Returns the package name for the UID if it is available. Otherwise return null.
     *
     * @param uid
     * @return
     */
    public String getPackageName(int uid) {
        String pkgName = InstrumentationRegistry.getTargetContext().getPackageManager()
                .getNameForUid(uid);
        // Remove the UID appended at the end of the package name.
        if (pkgName != null) {
            String pkgNameSplit[] = pkgName.split(String.format("\\:%d", uid));
            return pkgNameSplit[0];
        }
        return pkgName;
    }

    /**
     * Set the config id tracked in the statsd.
     */
    private void setConfigId(long configId) {
        mConfigId = configId;
    }

    /**
     * Return the config id tracked in the statsd.
     */
    private long getConfigId() {
        return mConfigId;
    }

    /**
     * Returns the unique id
     */
    private int getUniqueId() {
        return UUID.randomUUID().hashCode();
    }

    /**
     * Adopts shell permission identity needed to access StatsManager service
     */
    public static void adoptShellIdentity() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity();
    }

    /**
     * Drop shell permission identity
     */
    public static void dropShellIdentity() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

}
