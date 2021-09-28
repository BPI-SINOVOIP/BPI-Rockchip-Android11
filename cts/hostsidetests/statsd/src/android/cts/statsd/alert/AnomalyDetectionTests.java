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
package android.cts.statsd.alert;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.cts.statsd.atom.AtomTestCase;

import com.android.internal.os.StatsdConfigProto;
import com.android.internal.os.StatsdConfigProto.Alert;
import com.android.internal.os.StatsdConfigProto.CountMetric;
import com.android.internal.os.StatsdConfigProto.DurationMetric;
import com.android.internal.os.StatsdConfigProto.FieldFilter;
import com.android.internal.os.StatsdConfigProto.FieldMatcher;
import com.android.internal.os.StatsdConfigProto.GaugeMetric;
import com.android.internal.os.StatsdConfigProto.IncidentdDetails;
import com.android.internal.os.StatsdConfigProto.PerfettoDetails;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.internal.os.StatsdConfigProto.Subscription;
import com.android.internal.os.StatsdConfigProto.TimeUnit;
import com.android.internal.os.StatsdConfigProto.ValueMetric;
import com.android.os.AtomsProto.AnomalyDetected;
import com.android.os.AtomsProto.AppBreadcrumbReported;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.KernelWakelock;
import com.android.os.StatsLog.EventMetricData;
import com.android.tradefed.log.LogUtil.CLog;
import java.util.List;

/**
 * Statsd Anomaly Detection tests.
 */
public class AnomalyDetectionTests extends AtomTestCase {

    private static final String TAG = "Statsd.AnomalyDetectionTests";

    private static final boolean INCIDENTD_TESTS_ENABLED = false;
    private static final boolean PERFETTO_TESTS_ENABLED = true;

    private static final int WAIT_AFTER_BREADCRUMB_MS = 2000;

    // Config constants
    private static final int APP_BREADCRUMB_REPORTED_MATCH_START_ID = 1;
    private static final int APP_BREADCRUMB_REPORTED_MATCH_STOP_ID = 2;
    private static final int METRIC_ID = 8;
    private static final int ALERT_ID = 11;
    private static final int SUBSCRIPTION_ID_INCIDENTD = 41;
    private static final int SUBSCRIPTION_ID_PERFETTO = 42;
    private static final int ANOMALY_DETECT_MATCH_ID = 10;
    private static final int ANOMALY_EVENT_ID = 101;
    private static final int INCIDENTD_SECTION = -1;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (!INCIDENTD_TESTS_ENABLED) {
            CLog.w(TAG, TAG + " anomaly tests are disabled by a flag. Change flag to true to run");
        }
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (PERFETTO_TESTS_ENABLED) {
            //Deadline to finish trace collection
            final long deadLine = System.currentTimeMillis() + 10000;
            while (isSystemTracingEnabled()) {
                if (System.currentTimeMillis() > deadLine) {
                    CLog.w("/sys/kernel/debug/tracing/tracing_on is still 1 after 10 secs : " + isSystemTracingEnabled());
                    break;
                }
                CLog.d("Waiting to finish collecting traces. ");
                Thread.sleep(WAIT_TIME_SHORT);
            }
        }
    }

    // Tests that anomaly detection for count works.
    // Also tests that anomaly detection works when spanning multiple buckets.
    public void testCountAnomalyDetection() throws Exception {
        StatsdConfig.Builder config = getBaseConfig(10, 20, 2 /* threshold: > 2 counts */)
                .addCountMetric(CountMetric.newBuilder()
                        .setId(METRIC_ID)
                        .setWhat(APP_BREADCRUMB_REPORTED_MATCH_START_ID)
                        .setBucket(TimeUnit.CTS) // 1 second
                        // Slice by label
                        .setDimensionsInWhat(FieldMatcher.newBuilder()
                                .setField(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                                .addChild(FieldMatcher.newBuilder()
                                        .setField(AppBreadcrumbReported.LABEL_FIELD_NUMBER)
                                )
                        )
                );
        uploadConfig(config);

        String markTime = getCurrentLogcatDate();
        // count(label=6) -> 1 (not an anomaly, since not "greater than 2")
        doAppBreadcrumbReportedStart(6);
        Thread.sleep(500);
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isFalse();

        // count(label=6) -> 2 (not an anomaly, since not "greater than 2")
        doAppBreadcrumbReportedStart(6);
        Thread.sleep(500);
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isFalse();

        // count(label=12) -> 1 (not an anomaly, since not "greater than 2")
        doAppBreadcrumbReportedStart(12);
        Thread.sleep(1000);
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isFalse();

        doAppBreadcrumbReportedStart(6); // count(label=6) -> 3 (anomaly, since "greater than 2"!)
        Thread.sleep(WAIT_AFTER_BREADCRUMB_MS);

        List<EventMetricData> data = getEventMetricDataList();
        assertWithMessage("Expected anomaly").that(data).hasSize(1);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isTrue();
    }

    // Tests that anomaly detection for duration works.
    // Also tests that refractory periods in anomaly detection work.
    public void testDurationAnomalyDetection() throws Exception {
        final int APP_BREADCRUMB_REPORTED_IS_ON_PREDICATE = 1423;
        StatsdConfig.Builder config =
                getBaseConfig(17, 17, 10_000_000_000L  /*threshold: > 10 seconds in nanoseconds*/)
                        .addDurationMetric(DurationMetric.newBuilder()
                                .setId(METRIC_ID)
                                .setWhat(APP_BREADCRUMB_REPORTED_IS_ON_PREDICATE) // predicate below
                                .setAggregationType(DurationMetric.AggregationType.SUM)
                                .setBucket(TimeUnit.CTS) // 1 second
                        )
                        .addPredicate(StatsdConfigProto.Predicate.newBuilder()
                                .setId(APP_BREADCRUMB_REPORTED_IS_ON_PREDICATE)
                                .setSimplePredicate(StatsdConfigProto.SimplePredicate.newBuilder()
                                        .setStart(APP_BREADCRUMB_REPORTED_MATCH_START_ID)
                                        .setStop(APP_BREADCRUMB_REPORTED_MATCH_STOP_ID)
                                )
                        );
        uploadConfig(config);

        // Since timing is crucial and checking logcat for incidentd is slow, we don't test for it.

        // Test that alarm doesn't fire early.
        String markTime = getCurrentLogcatDate();
        doAppBreadcrumbReportedStart(1);
        Thread.sleep(6_000);  // Recorded duration at end: 6s
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();

        doAppBreadcrumbReportedStop(1);
        Thread.sleep(4_000);  // Recorded duration at end: 6s
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();

        // Test that alarm does fire when it is supposed to (after 4s, plus up to 5s alarm delay).
        doAppBreadcrumbReportedStart(1);
        Thread.sleep(9_000);  // Recorded duration at end: 13s
        List<EventMetricData> data = getEventMetricDataList();
        assertWithMessage("Expected anomaly").that(data).hasSize(1);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);

        // Now test that the refractory period is obeyed.
        markTime = getCurrentLogcatDate();
        doAppBreadcrumbReportedStop(1);
        doAppBreadcrumbReportedStart(1);
        Thread.sleep(3_000);  // Recorded duration at end: 13s
        // NB: the previous getEventMetricDataList also removes the report, so size is back to 0.
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();

        // Test that detection works again after refractory period finishes.
        doAppBreadcrumbReportedStop(1);
        Thread.sleep(8_000);  // Recorded duration at end: 9s
        doAppBreadcrumbReportedStart(1);
        Thread.sleep(15_000);  // Recorded duration at end: 15s
        // We can do an incidentd test now that all the timing issues are done.
        data = getEventMetricDataList();
        assertWithMessage("Expected anomaly").that(data).hasSize(1);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isTrue();

        doAppBreadcrumbReportedStop(1);
    }

    // Tests that anomaly detection for duration works even when the alarm fires too late.
    public void testDurationAnomalyDetectionForLateAlarms() throws Exception {
        final int APP_BREADCRUMB_REPORTED_IS_ON_PREDICATE = 1423;
        StatsdConfig.Builder config =
                getBaseConfig(50, 0, 6_000_000_000L /* threshold: > 6 seconds in nanoseconds */)
                        .addDurationMetric(DurationMetric.newBuilder()
                                .setId(METRIC_ID)
                                .setWhat(
                                        APP_BREADCRUMB_REPORTED_IS_ON_PREDICATE) // Predicate below.
                                .setAggregationType(DurationMetric.AggregationType.SUM)
                                .setBucket(TimeUnit.CTS) // 1 second
                        )
                        .addPredicate(StatsdConfigProto.Predicate.newBuilder()
                                .setId(APP_BREADCRUMB_REPORTED_IS_ON_PREDICATE)
                                .setSimplePredicate(StatsdConfigProto.SimplePredicate.newBuilder()
                                        .setStart(APP_BREADCRUMB_REPORTED_MATCH_START_ID)
                                        .setStop(APP_BREADCRUMB_REPORTED_MATCH_STOP_ID)
                                )
                        );
        uploadConfig(config);

        doAppBreadcrumbReportedStart(1);
        Thread.sleep(5_000);
        doAppBreadcrumbReportedStop(1);
        Thread.sleep(2_000);
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();

        // Test that alarm does fire when it is supposed to.
        // The anomaly occurs in 1s, but alarms won't fire that quickly.
        // It is likely that the alarm will only fire after this period is already over, but the
        // anomaly should nonetheless be detected when the event stops.
        doAppBreadcrumbReportedStart(1);
        Thread.sleep(1_200);
        // Anomaly should be detected here if the alarm didn't fire yet.
        doAppBreadcrumbReportedStop(1);
        Thread.sleep(200);
        List<EventMetricData> data = getEventMetricDataList();
        if (data.size() == 2) {
            // Although we expect that the alarm won't fire, we certainly cannot demand that.
            CLog.w(TAG, "The anomaly was detected twice. Presumably the alarm did manage to fire.");
        }
        assertThat(data.size()).isAnyOf(1, 2);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);
    }

    // Tests that anomaly detection for value works.
    public void testValueAnomalyDetection() throws Exception {
        StatsdConfig.Builder config = getBaseConfig(4, 0, 6 /* threshold: value > 6 */)
                .addValueMetric(ValueMetric.newBuilder()
                        .setId(METRIC_ID)
                        .setWhat(APP_BREADCRUMB_REPORTED_MATCH_START_ID)
                        .setBucket(TimeUnit.ONE_MINUTE)
                        // Get the label field's value:
                        .setValueField(FieldMatcher.newBuilder()
                                .setField(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                                .addChild(FieldMatcher.newBuilder()
                                        .setField(AppBreadcrumbReported.LABEL_FIELD_NUMBER))
                        )

                );
        uploadConfig(config);

        String markTime = getCurrentLogcatDate();
        doAppBreadcrumbReportedStart(6); // value = 6, which is NOT > trigger
        Thread.sleep(WAIT_AFTER_BREADCRUMB_MS);
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isFalse();

        doAppBreadcrumbReportedStart(14); // value = 14 > trigger
        Thread.sleep(WAIT_AFTER_BREADCRUMB_MS);

        List<EventMetricData> data = getEventMetricDataList();
        assertWithMessage("Expected anomaly").that(data).hasSize(1);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isTrue();
    }

    // Test that anomaly detection integrates with perfetto properly.
    public void testPerfetto() throws Exception {
        String chars = getDevice().getProperty("ro.build.characteristics");
        if (chars.contains("watch")) {
                return;
        }

        if (PERFETTO_TESTS_ENABLED) resetPerfettoGuardrails();

        StatsdConfig.Builder config = getBaseConfig(4, 0, 6 /* threshold: value > 6 */)
                .addSubscription(Subscription.newBuilder()
                        .setId(SUBSCRIPTION_ID_PERFETTO)
                        .setRuleType(Subscription.RuleType.ALERT)
                        .setRuleId(ALERT_ID)
                        .setPerfettoDetails(PerfettoDetails.newBuilder()
                                .setTraceConfig(getPerfettoConfig()))
                )
                .addValueMetric(ValueMetric.newBuilder()
                        .setId(METRIC_ID)
                        .setWhat(APP_BREADCRUMB_REPORTED_MATCH_START_ID)
                        .setBucket(TimeUnit.ONE_MINUTE)
                        // Get the label field's value:
                        .setValueField(FieldMatcher.newBuilder()
                                .setField(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                                .addChild(FieldMatcher.newBuilder()
                                        .setField(AppBreadcrumbReported.LABEL_FIELD_NUMBER))
                        )

                );
        uploadConfig(config);

        String markTime = getCurrentLogcatDate();
        doAppBreadcrumbReportedStart(6); // value = 6, which is NOT > trigger
        Thread.sleep(WAIT_AFTER_BREADCRUMB_MS);
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();
        if (PERFETTO_TESTS_ENABLED) assertThat(isSystemTracingEnabled()).isFalse();

        doAppBreadcrumbReportedStart(14); // value = 14 > trigger
        Thread.sleep(WAIT_AFTER_BREADCRUMB_MS);

        List<EventMetricData> data = getEventMetricDataList();
        assertWithMessage("Expected anomaly").that(data).hasSize(1);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);

        // Pool a few times to allow for statsd <-> traced <-> traced_probes communication to happen.
        if (PERFETTO_TESTS_ENABLED) {
                boolean tracingEnabled = false;
                for (int i = 0; i < 5; i++) {
                        if (isSystemTracingEnabled()) {
                                tracingEnabled = true;
                                break;
                        }
                        Thread.sleep(1000);
                }
                assertThat(tracingEnabled).isTrue();
        }
    }

    // Tests that anomaly detection for gauge works.
    public void testGaugeAnomalyDetection() throws Exception {
        StatsdConfig.Builder config = getBaseConfig(1, 20, 6 /* threshold: value > 6 */)
                .addGaugeMetric(GaugeMetric.newBuilder()
                        .setId(METRIC_ID)
                        .setWhat(APP_BREADCRUMB_REPORTED_MATCH_START_ID)
                        .setBucket(TimeUnit.CTS)
                        // Get the label field's value into the gauge:
                        .setGaugeFieldsFilter(
                                FieldFilter.newBuilder().setFields(FieldMatcher.newBuilder()
                                        .setField(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                                        .addChild(FieldMatcher.newBuilder()
                                                .setField(AppBreadcrumbReported.LABEL_FIELD_NUMBER))
                                )
                        )
                );
        uploadConfig(config);

        String markTime = getCurrentLogcatDate();
        doAppBreadcrumbReportedStart(6); // gauge = 6, which is NOT > trigger
        Thread.sleep(Math.max(WAIT_AFTER_BREADCRUMB_MS, 1_100)); // Must be >1s to push next bucket.
        assertWithMessage("Premature anomaly").that(getEventMetricDataList()).isEmpty();
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isFalse();

        // We waited for >1s above, so we are now in the next bucket (which is essential).
        doAppBreadcrumbReportedStart(14); // gauge = 14 > trigger
        Thread.sleep(WAIT_AFTER_BREADCRUMB_MS);

        List<EventMetricData> data = getEventMetricDataList();
        assertWithMessage("Expected anomaly").that(data).hasSize(1);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);
        if (INCIDENTD_TESTS_ENABLED) assertThat(didIncidentdFireSince(markTime)).isTrue();
    }

    // Test that anomaly detection for pulled metrics work.
    public void testPulledAnomalyDetection() throws Exception {
        final int ATOM_ID = Atom.KERNEL_WAKELOCK_FIELD_NUMBER;  // A pulled atom
        final int SLICE_BY_FIELD = KernelWakelock.NAME_FIELD_NUMBER;
        final int VALUE_FIELD = KernelWakelock.VERSION_FIELD_NUMBER;  // Something that will be > 0.
        final int ATOM_MATCHER_ID = 300;

        StatsdConfig.Builder config = getBaseConfig(10, 20, 0 /* threshold: value > 0 */)
                .addAllowedLogSource("AID_SYSTEM")
                // Track the ATOM_ID pulled atom
                .addAtomMatcher(StatsdConfigProto.AtomMatcher.newBuilder()
                        .setId(ATOM_MATCHER_ID)
                        .setSimpleAtomMatcher(StatsdConfigProto.SimpleAtomMatcher.newBuilder()
                                .setAtomId(ATOM_ID)))
                .addGaugeMetric(GaugeMetric.newBuilder()
                        .setId(METRIC_ID)
                        .setWhat(ATOM_MATCHER_ID)
                        .setBucket(TimeUnit.CTS)
                        .setSamplingType(GaugeMetric.SamplingType.RANDOM_ONE_SAMPLE)
                        // Slice by SLICE_BY_FIELD (typical usecase)
                        .setDimensionsInWhat(FieldMatcher.newBuilder()
                                .setField(ATOM_ID)
                                .addChild(FieldMatcher.newBuilder().setField(SLICE_BY_FIELD))
                        )
                        // Track the VALUE_FIELD (anomaly detection requires exactly one field here)
                        .setGaugeFieldsFilter(
                                FieldFilter.newBuilder().setFields(FieldMatcher.newBuilder()
                                        .setField(ATOM_ID)
                                        .addChild(FieldMatcher.newBuilder().setField(VALUE_FIELD))
                                )
                        )
                );
        uploadConfig(config);

        Thread.sleep(6_000); // Wait long enough to ensure AlarmManager signals >= 1 pull

        List<EventMetricData> data = getEventMetricDataList();
        // There will likely be many anomalies (one for each dimension). There must be at least one.
        assertThat(data.size()).isAtLeast(1);
        assertThat(data.get(0).getAtom().getAnomalyDetected().getAlertId()).isEqualTo(ALERT_ID);
    }


    private final StatsdConfig.Builder getBaseConfig(int numBuckets,
                                                     int refractorySecs,
                                                     long triggerIfSumGt) throws Exception {
      return createConfigBuilder()
          // Items of relevance for detecting the anomaly:
          .addAtomMatcher(
              StatsdConfigProto.AtomMatcher.newBuilder()
                  .setId(APP_BREADCRUMB_REPORTED_MATCH_START_ID)
                  .setSimpleAtomMatcher(
                      StatsdConfigProto.SimpleAtomMatcher.newBuilder()
                          .setAtomId(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                          // Event only when the uid is this app's uid.
                          .addFieldValueMatcher(createFvm(AppBreadcrumbReported.UID_FIELD_NUMBER)
                                                    .setEqInt(getHostUid()))
                          .addFieldValueMatcher(
                              createFvm(AppBreadcrumbReported.STATE_FIELD_NUMBER)
                                  .setEqInt(AppBreadcrumbReported.State.START.ordinal()))))
          .addAtomMatcher(
              StatsdConfigProto.AtomMatcher.newBuilder()
                  .setId(APP_BREADCRUMB_REPORTED_MATCH_STOP_ID)
                  .setSimpleAtomMatcher(
                      StatsdConfigProto.SimpleAtomMatcher.newBuilder()
                          .setAtomId(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER)
                          // Event only when the uid is this app's uid.
                          .addFieldValueMatcher(createFvm(AppBreadcrumbReported.UID_FIELD_NUMBER)
                                                    .setEqInt(getHostUid()))
                          .addFieldValueMatcher(
                              createFvm(AppBreadcrumbReported.STATE_FIELD_NUMBER)
                                  .setEqInt(AppBreadcrumbReported.State.STOP.ordinal()))))
          .addAlert(Alert.newBuilder()
                        .setId(ALERT_ID)
                        .setMetricId(METRIC_ID) // The metric itself must yet be added by the test.
                        .setNumBuckets(numBuckets)
                        .setRefractoryPeriodSecs(refractorySecs)
                        .setTriggerIfSumGt(triggerIfSumGt))
          .addSubscription(
              Subscription.newBuilder()
                  .setId(SUBSCRIPTION_ID_INCIDENTD)
                  .setRuleType(Subscription.RuleType.ALERT)
                  .setRuleId(ALERT_ID)
                  .setIncidentdDetails(IncidentdDetails.newBuilder().addSection(INCIDENTD_SECTION)))
          // We want to trigger anomalies on METRIC_ID, but don't want the actual data.
          .addNoReportMetric(METRIC_ID)

          // Items of relevance to reporting the anomaly (we do want this data):
          .addAtomMatcher(
              StatsdConfigProto.AtomMatcher.newBuilder()
                  .setId(ANOMALY_DETECT_MATCH_ID)
                  .setSimpleAtomMatcher(
                      StatsdConfigProto.SimpleAtomMatcher.newBuilder()
                          .setAtomId(Atom.ANOMALY_DETECTED_FIELD_NUMBER)
                          .addFieldValueMatcher(createFvm(AnomalyDetected.CONFIG_UID_FIELD_NUMBER)
                                                    .setEqInt(getHostUid()))
                          .addFieldValueMatcher(createFvm(AnomalyDetected.CONFIG_ID_FIELD_NUMBER)
                                                    .setEqInt(CONFIG_ID))))
          .addEventMetric(StatsdConfigProto.EventMetric.newBuilder()
                              .setId(ANOMALY_EVENT_ID)
                              .setWhat(ANOMALY_DETECT_MATCH_ID));
    }
}
