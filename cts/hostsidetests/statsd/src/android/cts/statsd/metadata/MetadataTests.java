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
package android.cts.statsd.metadata;

import static com.google.common.truth.Truth.assertWithMessage;

import android.cts.statsd.atom.AtomTestCase;

import com.android.internal.os.StatsdConfigProto;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.internal.os.StatsdConfigProto.Subscription;
import com.android.internal.os.StatsdConfigProto.TimeUnit;
import com.android.internal.os.StatsdConfigProto.ValueMetric;
import com.android.os.AtomsProto.AnomalyDetected;
import com.android.os.AtomsProto.AppBreadcrumbReported;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.EventMetricData;
import com.android.os.StatsLog.StatsdStatsReport;
import com.android.os.StatsLog.StatsdStatsReport.ConfigStats;
import com.android.tradefed.log.LogUtil;


import java.util.List;

/**
 * Statsd Metadata tests.
 */
public class MetadataTests extends MetadataTestCase {

    private static final String TAG = "Statsd.MetadataTests";

    // Tests that the statsd config is reset after the specified ttl.
    public void testConfigTtl() throws Exception {
        final int TTL_TIME_SEC = 8;
        StatsdConfig.Builder config = getBaseConfig();
        config.setTtlInSeconds(TTL_TIME_SEC); // should reset in this many seconds.

        uploadConfig(config);
        long startTime = System.currentTimeMillis();
        Thread.sleep(WAIT_TIME_SHORT);
        doAppBreadcrumbReportedStart(/* irrelevant val */ 6); // Event, within < TTL_TIME_SEC secs.
        Thread.sleep(WAIT_TIME_SHORT);
        StatsdStatsReport report = getStatsdStatsReport(); // Has only been 1 second
        LogUtil.CLog.d("got following statsdstats report: " + report.toString());
        boolean foundActiveConfig = false;
        int creationTime = 0;
        for (ConfigStats stats: report.getConfigStatsList()) {
            if (stats.getId() == CONFIG_ID && stats.getUid() == getHostUid()) {
                if(!stats.hasDeletionTimeSec()) {
                    assertWithMessage("Found multiple active CTS configs!")
                            .that(foundActiveConfig).isFalse();
                    foundActiveConfig = true;
                    creationTime = stats.getCreationTimeSec();
                }
            }
        }
        assertWithMessage("Did not find an active CTS config").that(foundActiveConfig).isTrue();

        while(System.currentTimeMillis() - startTime < 8_000) {
            Thread.sleep(10);
        }
        doAppBreadcrumbReportedStart(/* irrelevant val */ 6); // Event, after TTL_TIME_SEC secs.
        Thread.sleep(WAIT_TIME_SHORT);
        report = getStatsdStatsReport();
        LogUtil.CLog.d("got following statsdstats report: " + report.toString());
        foundActiveConfig = false;
        int expectedTime = creationTime + TTL_TIME_SEC;
        for (ConfigStats stats: report.getConfigStatsList()) {
            if (stats.getId() == CONFIG_ID && stats.getUid() == getHostUid()) {
                // Original config should be TTL'd
                if (stats.getCreationTimeSec() == creationTime) {
                    assertWithMessage("Config should have TTL'd but is still active")
                            .that(stats.hasDeletionTimeSec()).isTrue();
                    assertWithMessage(
                            "Config deletion time should be about %s after creation", TTL_TIME_SEC
                    ).that(Math.abs(stats.getDeletionTimeSec() - expectedTime)).isAtMost(2);
                }
                // There should still be one active config, that is marked as reset.
                if(!stats.hasDeletionTimeSec()) {
                    assertWithMessage("Found multiple active CTS configs!")
                            .that(foundActiveConfig).isFalse();
                    foundActiveConfig = true;
                    creationTime = stats.getCreationTimeSec();
                    assertWithMessage("Active config after TTL should be marked as reset")
                            .that(stats.hasResetTimeSec()).isTrue();
                    assertWithMessage("Reset and creation time should be equal for TTl'd configs")
                            .that(stats.getResetTimeSec()).isEqualTo(stats.getCreationTimeSec());
                    assertWithMessage(
                            "Reset config should be created when the original config TTL'd"
                    ).that(Math.abs(stats.getCreationTimeSec() - expectedTime)).isAtMost(2);
                }
            }
        }
        assertWithMessage("Did not find an active CTS config after the TTL")
                .that(foundActiveConfig).isTrue();
    }
}
