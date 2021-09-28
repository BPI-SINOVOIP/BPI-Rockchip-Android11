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
package android.cts.statsd.metric;

import static com.google.common.truth.Truth.assertThat;

import android.cts.statsd.atom.DeviceAtomTestCase;

import com.android.internal.os.StatsdConfigProto;
import com.android.internal.os.StatsdConfigProto.AtomMatcher;
import com.android.internal.os.StatsdConfigProto.FieldMatcher;
import com.android.internal.os.StatsdConfigProto.FieldValueMatcher;
import com.android.internal.os.StatsdConfigProto.Position;
import com.android.internal.os.StatsdConfigProto.Predicate;
import com.android.internal.os.StatsdConfigProto.SimpleAtomMatcher;
import com.android.internal.os.StatsdConfigProto.SimplePredicate;
import com.android.os.AtomsProto.AppBreadcrumbReported;
import com.android.os.AtomsProto.Atom;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.DurationBucketInfo;
import com.android.os.StatsLog.StatsLogReport;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;

import com.google.common.collect.Range;

import java.util.List;

public class DurationMetricsTests extends DeviceAtomTestCase {

    private static final int APP_BREADCRUMB_REPORTED_A_MATCH_START_ID = 0;
    private static final int APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID = 1;
    private static final int APP_BREADCRUMB_REPORTED_B_MATCH_START_ID = 2;
    private static final int APP_BREADCRUMB_REPORTED_B_MATCH_STOP_ID = 3;

    public void testDurationMetric() throws Exception {
        final int label = 1;
        // Add AtomMatchers.
        AtomMatcher startAtomMatcher =
            MetricsUtils.startAtomMatcherWithLabel(APP_BREADCRUMB_REPORTED_A_MATCH_START_ID, label);
        AtomMatcher stopAtomMatcher =
            MetricsUtils.stopAtomMatcherWithLabel(APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID, label);

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder();
        builder.addAtomMatcher(startAtomMatcher);
        builder.addAtomMatcher(stopAtomMatcher);

        // Add Predicates.
        SimplePredicate simplePredicate = SimplePredicate.newBuilder()
                .setStart(APP_BREADCRUMB_REPORTED_A_MATCH_START_ID)
                .setStop(APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID)
                .build();
        Predicate predicate = Predicate.newBuilder()
                                  .setId(MetricsUtils.StringToId("Predicate"))
                                  .setSimplePredicate(simplePredicate)
                                  .build();
        builder.addPredicate(predicate);

        // Add DurationMetric.
        builder.addDurationMetric(
            StatsdConfigProto.DurationMetric.newBuilder()
                .setId(MetricsUtils.DURATION_METRIC_ID)
                .setWhat(predicate.getId())
                .setAggregationType(StatsdConfigProto.DurationMetric.AggregationType.SUM)
                .setBucket(StatsdConfigProto.TimeUnit.CTS));

        // Upload config.
        uploadConfig(builder);

        // Create AppBreadcrumbReported Start/Stop events.
        doAppBreadcrumbReportedStart(label);
        Thread.sleep(2000);
        doAppBreadcrumbReportedStop(label);

        // Wait for the metrics to propagate to statsd.
        Thread.sleep(2000);

        StatsLogReport metricReport = getStatsLogReport();
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.DURATION_METRIC_ID);
        LogUtil.CLog.d("Received the following data: " + metricReport.toString());
        assertThat(metricReport.hasDurationMetrics()).isTrue();
        StatsLogReport.DurationMetricDataWrapper durationData
                = metricReport.getDurationMetrics();
        assertThat(durationData.getDataCount()).isEqualTo(1);
        assertThat(durationData.getData(0).getBucketInfo(0).getDurationNanos())
                .isIn(Range.open(0L, (long)1e9));
    }

    public void testDurationMetricWithCondition() throws Exception {
        final int durationLabel = 1;
        final int conditionLabel = 2;

        // Add AtomMatchers.
        AtomMatcher startAtomMatcher = MetricsUtils.startAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_A_MATCH_START_ID, durationLabel);
        AtomMatcher stopAtomMatcher = MetricsUtils.stopAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID, durationLabel);
        AtomMatcher conditionStartAtomMatcher = MetricsUtils.startAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_B_MATCH_START_ID, conditionLabel);
        AtomMatcher conditionStopAtomMatcher = MetricsUtils.stopAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_B_MATCH_STOP_ID, conditionLabel);

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder()
                .addAtomMatcher(startAtomMatcher)
                .addAtomMatcher(stopAtomMatcher)
                .addAtomMatcher(conditionStartAtomMatcher)
                .addAtomMatcher(conditionStopAtomMatcher);

        // Add Predicates.
        SimplePredicate simplePredicate = SimplePredicate.newBuilder()
                .setStart(APP_BREADCRUMB_REPORTED_A_MATCH_START_ID)
                .setStop(APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID)
                .build();
        Predicate predicate = Predicate.newBuilder()
                                  .setId(MetricsUtils.StringToId("Predicate"))
                                  .setSimplePredicate(simplePredicate)
                                  .build();

        SimplePredicate conditionSimplePredicate = SimplePredicate.newBuilder()
                .setStart(APP_BREADCRUMB_REPORTED_B_MATCH_START_ID)
                .setStop(APP_BREADCRUMB_REPORTED_B_MATCH_STOP_ID)
                .build();
        Predicate conditionPredicate = Predicate.newBuilder()
                                  .setId(MetricsUtils.StringToId("ConditionPredicate"))
                                  .setSimplePredicate(conditionSimplePredicate)
                                  .build();

        builder
            .addPredicate(predicate)
            .addPredicate(conditionPredicate);

        // Add DurationMetric.
        builder
                .addDurationMetric(StatsdConfigProto.DurationMetric.newBuilder()
                        .setId(MetricsUtils.DURATION_METRIC_ID)
                        .setWhat(predicate.getId())
                        .setAggregationType(StatsdConfigProto.DurationMetric.AggregationType.SUM)
                        .setBucket(StatsdConfigProto.TimeUnit.CTS)
                        .setCondition(conditionPredicate.getId())
                );

        // Upload config.
        uploadConfig(builder);

        // Start uncounted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop uncounted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Set the condition to true.
        doAppBreadcrumbReportedStart(conditionLabel);
        Thread.sleep(10);

        // Start counted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop counted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Set the condition to false.
        doAppBreadcrumbReportedStop(conditionLabel);
        Thread.sleep(10);

        // Start uncounted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop uncounted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);
        StatsLogReport metricReport = getStatsLogReport();
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.DURATION_METRIC_ID);
        LogUtil.CLog.d("Received the following data: " + metricReport.toString());
        assertThat(metricReport.hasDurationMetrics()).isTrue();
        StatsLogReport.DurationMetricDataWrapper durationData
                = metricReport.getDurationMetrics();
        assertThat(durationData.getDataCount()).isEqualTo(1);
        long totalDuration = durationData.getData(0).getBucketInfoList().stream()
                .mapToLong(bucketInfo -> bucketInfo.getDurationNanos())
                .peek(durationNs -> assertThat(durationNs).isIn(Range.openClosed(0L, (long)1e9)))
                .sum();
        assertThat(totalDuration).isIn(Range.open((long)2e9, (long)3e9));
    }

    public void testDurationMetricWithActivation() throws Exception {
        final int activationMatcherId = 5;
        final int activationMatcherLabel = 5;
        final int ttlSec = 5;
        final int durationLabel = 1;

        // Add AtomMatchers.
        AtomMatcher startAtomMatcher = MetricsUtils.startAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_A_MATCH_START_ID, durationLabel);
        AtomMatcher stopAtomMatcher = MetricsUtils.stopAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID, durationLabel);
        StatsdConfigProto.AtomMatcher activationMatcher =
                MetricsUtils.appBreadcrumbMatcherWithLabel(activationMatcherId,
                                                           activationMatcherLabel);

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder()
                .addAtomMatcher(startAtomMatcher)
                .addAtomMatcher(stopAtomMatcher)
                .addAtomMatcher(activationMatcher);

        // Add Predicates.
        SimplePredicate simplePredicate = SimplePredicate.newBuilder()
                .setStart(APP_BREADCRUMB_REPORTED_A_MATCH_START_ID)
                .setStop(APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID)
                .build();
        Predicate predicate = Predicate.newBuilder()
                                  .setId(MetricsUtils.StringToId("Predicate"))
                                  .setSimplePredicate(simplePredicate)
                                  .build();
        builder.addPredicate(predicate);

        // Add DurationMetric.
        builder
                .addDurationMetric(StatsdConfigProto.DurationMetric.newBuilder()
                        .setId(MetricsUtils.DURATION_METRIC_ID)
                        .setWhat(predicate.getId())
                        .setAggregationType(StatsdConfigProto.DurationMetric.AggregationType.SUM)
                        .setBucket(StatsdConfigProto.TimeUnit.CTS)
                )
                .addMetricActivation(StatsdConfigProto.MetricActivation.newBuilder()
                        .setMetricId(MetricsUtils.DURATION_METRIC_ID)
                        .addEventActivation(StatsdConfigProto.EventActivation.newBuilder()
                                .setAtomMatcherId(activationMatcherId)
                                .setActivationType(
                                        StatsdConfigProto.ActivationType.ACTIVATE_IMMEDIATELY)
                                .setTtlSeconds(ttlSec)));

        // Upload config.
        uploadConfig(builder);

        // Start uncounted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop uncounted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Activate the metric.
        doAppBreadcrumbReported(activationMatcherLabel);
        Thread.sleep(10);

        // Start counted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop counted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);
        StatsLogReport metricReport = getStatsLogReport();
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.DURATION_METRIC_ID);
        LogUtil.CLog.d("Received the following data: " + metricReport.toString());
        assertThat(metricReport.hasDurationMetrics()).isTrue();
        StatsLogReport.DurationMetricDataWrapper durationData
                = metricReport.getDurationMetrics();
        assertThat(durationData.getDataCount()).isEqualTo(1);
        long totalDuration = durationData.getData(0).getBucketInfoList().stream()
                .mapToLong(bucketInfo -> bucketInfo.getDurationNanos())
                .peek(durationNs -> assertThat(durationNs).isIn(Range.openClosed(0L, (long)1e9)))
                .sum();
        assertThat(totalDuration).isIn(Range.open((long)2e9, (long)3e9));
    }

    public void testDurationMetricWithConditionAndActivation() throws Exception {
        final int durationLabel = 1;
        final int conditionLabel = 2;
        final int activationMatcherId = 5;
        final int activationMatcherLabel = 5;
        final int ttlSec = 5;

        // Add AtomMatchers.
        AtomMatcher startAtomMatcher = MetricsUtils.startAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_A_MATCH_START_ID, durationLabel);
        AtomMatcher stopAtomMatcher = MetricsUtils.stopAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID, durationLabel);
        AtomMatcher conditionStartAtomMatcher = MetricsUtils.startAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_B_MATCH_START_ID, conditionLabel);
        AtomMatcher conditionStopAtomMatcher = MetricsUtils.stopAtomMatcherWithLabel(
                APP_BREADCRUMB_REPORTED_B_MATCH_STOP_ID, conditionLabel);
        StatsdConfigProto.AtomMatcher activationMatcher =
                MetricsUtils.appBreadcrumbMatcherWithLabel(activationMatcherId,
                                                           activationMatcherLabel);

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder()
                .addAtomMatcher(startAtomMatcher)
                .addAtomMatcher(stopAtomMatcher)
                .addAtomMatcher(conditionStartAtomMatcher)
                .addAtomMatcher(conditionStopAtomMatcher)
                .addAtomMatcher(activationMatcher);

        // Add Predicates.
        SimplePredicate simplePredicate = SimplePredicate.newBuilder()
                .setStart(APP_BREADCRUMB_REPORTED_A_MATCH_START_ID)
                .setStop(APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID)
                .build();
        Predicate predicate = Predicate.newBuilder()
                                  .setId(MetricsUtils.StringToId("Predicate"))
                                  .setSimplePredicate(simplePredicate)
                                  .build();
        builder.addPredicate(predicate);

        SimplePredicate conditionSimplePredicate = SimplePredicate.newBuilder()
                .setStart(APP_BREADCRUMB_REPORTED_B_MATCH_START_ID)
                .setStop(APP_BREADCRUMB_REPORTED_B_MATCH_STOP_ID)
                .build();
        Predicate conditionPredicate = Predicate.newBuilder()
                                  .setId(MetricsUtils.StringToId("ConditionPredicate"))
                                  .setSimplePredicate(conditionSimplePredicate)
                                  .build();
        builder.addPredicate(conditionPredicate);

        // Add DurationMetric.
        builder
                .addDurationMetric(StatsdConfigProto.DurationMetric.newBuilder()
                        .setId(MetricsUtils.DURATION_METRIC_ID)
                        .setWhat(predicate.getId())
                        .setAggregationType(StatsdConfigProto.DurationMetric.AggregationType.SUM)
                        .setBucket(StatsdConfigProto.TimeUnit.CTS)
                        .setCondition(conditionPredicate.getId())
                )
                .addMetricActivation(StatsdConfigProto.MetricActivation.newBuilder()
                        .setMetricId(MetricsUtils.DURATION_METRIC_ID)
                        .addEventActivation(StatsdConfigProto.EventActivation.newBuilder()
                                .setAtomMatcherId(activationMatcherId)
                                .setActivationType(
                                        StatsdConfigProto.ActivationType.ACTIVATE_IMMEDIATELY)
                                .setTtlSeconds(ttlSec)));

        // Upload config.
        uploadConfig(builder);

        // Activate the metric.
        doAppBreadcrumbReported(activationMatcherLabel);
        Thread.sleep(10);

        // Set the condition to true.
        doAppBreadcrumbReportedStart(conditionLabel);
        Thread.sleep(10);

        // Start counted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop counted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Set the condition to false.
        doAppBreadcrumbReportedStop(conditionLabel);
        Thread.sleep(10);

        // Start uncounted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop uncounted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Let the metric deactivate.
        Thread.sleep(ttlSec * 1000);
        //doAppBreadcrumbReported(99); // TODO: maybe remove?
        //Thread.sleep(10);

        // Start uncounted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop uncounted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Set condition to true again.
        doAppBreadcrumbReportedStart(conditionLabel);
        Thread.sleep(10);

        // Start uncounted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop uncounted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Activate the metric.
        doAppBreadcrumbReported(activationMatcherLabel);
        Thread.sleep(10);

        // Start counted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop counted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Let the metric deactivate.
        Thread.sleep(ttlSec * 1000);

        // Start uncounted duration.
        doAppBreadcrumbReportedStart(durationLabel);
        Thread.sleep(10);

        Thread.sleep(2_000);

        // Stop uncounted duration.
        doAppBreadcrumbReportedStop(durationLabel);
        Thread.sleep(10);

        // Wait for the metrics to propagate to statsd.
        Thread.sleep(2000);

        StatsLogReport metricReport = getStatsLogReport();
        LogUtil.CLog.d("Received the following data: " + metricReport.toString());
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.DURATION_METRIC_ID);
        assertThat(metricReport.hasDurationMetrics()).isTrue();
        StatsLogReport.DurationMetricDataWrapper durationData
                = metricReport.getDurationMetrics();
        assertThat(durationData.getDataCount()).isEqualTo(1);
        long totalDuration = durationData.getData(0).getBucketInfoList().stream()
                .mapToLong(bucketInfo -> bucketInfo.getDurationNanos())
                .peek(durationNs -> assertThat(durationNs).isIn(Range.openClosed(0L, (long)1e9)))
                .sum();
        assertThat(totalDuration).isIn(Range.open((long)4e9, (long)5e9));
    }

    public void testDurationMetricWithDimension() throws Exception {
        // Add AtomMatchers.
        AtomMatcher startAtomMatcherA =
            MetricsUtils.startAtomMatcher(APP_BREADCRUMB_REPORTED_A_MATCH_START_ID);
        AtomMatcher stopAtomMatcherA =
            MetricsUtils.stopAtomMatcher(APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID);
        AtomMatcher startAtomMatcherB =
            MetricsUtils.startAtomMatcher(APP_BREADCRUMB_REPORTED_B_MATCH_START_ID);
        AtomMatcher stopAtomMatcherB =
            MetricsUtils.stopAtomMatcher(APP_BREADCRUMB_REPORTED_B_MATCH_STOP_ID);

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder();
        builder.addAtomMatcher(startAtomMatcherA);
        builder.addAtomMatcher(stopAtomMatcherA);
        builder.addAtomMatcher(startAtomMatcherB);
        builder.addAtomMatcher(stopAtomMatcherB);

        // Add Predicates.
        SimplePredicate simplePredicateA = SimplePredicate.newBuilder()
                .setStart(APP_BREADCRUMB_REPORTED_A_MATCH_START_ID)
                .setStop(APP_BREADCRUMB_REPORTED_A_MATCH_STOP_ID)
                .build();
        Predicate predicateA = Predicate.newBuilder()
                                   .setId(MetricsUtils.StringToId("Predicate_A"))
                                   .setSimplePredicate(simplePredicateA)
                                   .build();
        builder.addPredicate(predicateA);

        FieldMatcher.Builder dimensionsBuilder = FieldMatcher.newBuilder()
                .setField(AppBreadcrumbReported.STATE_FIELD_NUMBER);
        dimensionsBuilder.addChild(FieldMatcher.newBuilder()
                .setField(AppBreadcrumbReported.LABEL_FIELD_NUMBER)
                .setPosition(Position.FIRST)
                .addChild(FieldMatcher.newBuilder().setField(
                        AppBreadcrumbReported.LABEL_FIELD_NUMBER)));
        Predicate predicateB =
            Predicate.newBuilder()
                .setId(MetricsUtils.StringToId("Predicate_B"))
                .setSimplePredicate(SimplePredicate.newBuilder()
                                        .setStart(APP_BREADCRUMB_REPORTED_B_MATCH_START_ID)
                                        .setStop(APP_BREADCRUMB_REPORTED_B_MATCH_STOP_ID)
                                        .setDimensions(dimensionsBuilder.build())
                                        .build())
                .build();
        builder.addPredicate(predicateB);

        // Add DurationMetric.
        builder.addDurationMetric(
            StatsdConfigProto.DurationMetric.newBuilder()
                .setId(MetricsUtils.DURATION_METRIC_ID)
                .setWhat(predicateB.getId())
                .setCondition(predicateA.getId())
                .setAggregationType(StatsdConfigProto.DurationMetric.AggregationType.SUM)
                .setBucket(StatsdConfigProto.TimeUnit.CTS)
                .setDimensionsInWhat(
                    FieldMatcher.newBuilder()
                        .setField(Atom.BATTERY_SAVER_MODE_STATE_CHANGED_FIELD_NUMBER)
                        .addChild(FieldMatcher.newBuilder()
                                      .setField(AppBreadcrumbReported.STATE_FIELD_NUMBER)
                                      .setPosition(Position.FIRST)
                                      .addChild(FieldMatcher.newBuilder().setField(
                                          AppBreadcrumbReported.LABEL_FIELD_NUMBER)))));

        // Upload config.
        uploadConfig(builder);

        // Trigger events.
        doAppBreadcrumbReportedStart(1);
        Thread.sleep(2000);
        doAppBreadcrumbReportedStart(2);
        Thread.sleep(2000);
        doAppBreadcrumbReportedStop(1);
        Thread.sleep(2000);
        doAppBreadcrumbReportedStop(2);

        // Wait for the metrics to propagate to statsd.
        Thread.sleep(2000);

        StatsLogReport metricReport = getStatsLogReport();
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.DURATION_METRIC_ID);
        assertThat(metricReport.hasDurationMetrics()).isTrue();
        StatsLogReport.DurationMetricDataWrapper durationData
                = metricReport.getDurationMetrics();
        assertThat(durationData.getDataCount()).isEqualTo(1);
        assertThat(durationData.getData(0).getBucketInfoCount()).isGreaterThan(1);
        for (DurationBucketInfo bucketInfo : durationData.getData(0).getBucketInfoList()) {
            assertThat(bucketInfo.getDurationNanos()).isIn(Range.openClosed(0L, (long)1e9));
        }
    }
}
