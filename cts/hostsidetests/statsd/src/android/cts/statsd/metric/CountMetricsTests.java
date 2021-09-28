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
import static com.google.common.truth.Truth.assertWithMessage;

import android.cts.statsd.atom.DeviceAtomTestCase;

import com.android.internal.os.StatsdConfigProto;
import com.android.internal.os.StatsdConfigProto.FieldMatcher;
import com.android.internal.os.StatsdConfigProto.Position;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.AppBreadcrumbReported;
import com.android.os.AtomsProto.AttributionNode;
import com.android.os.AtomsProto.BleScanStateChanged;
import com.android.os.AtomsProto.WakelockStateChanged;
import com.android.os.StatsLog;
import com.android.os.StatsLog.ConfigMetricsReport;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.CountBucketInfo;
import com.android.os.StatsLog.CountMetricData;
import com.android.os.StatsLog.StatsLogReport;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;

import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

public class CountMetricsTests extends DeviceAtomTestCase {

    public void testSimpleEventCountMetric() throws Exception {
        int matcherId = 1;
        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder();
        builder.addCountMetric(StatsdConfigProto.CountMetric.newBuilder()
                .setId(MetricsUtils.COUNT_METRIC_ID)
                .setBucket(StatsdConfigProto.TimeUnit.CTS)
                .setWhat(matcherId))
                .addAtomMatcher(MetricsUtils.simpleAtomMatcher(matcherId));
        uploadConfig(builder);

        doAppBreadcrumbReportedStart(0);
        doAppBreadcrumbReportedStop(0);
        Thread.sleep(2000);  // Wait for the metrics to propagate to statsd.

        StatsLogReport metricReport = getStatsLogReport();
        LogUtil.CLog.d("Got the following stats log report: \n" + metricReport.toString());
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.COUNT_METRIC_ID);
        assertThat(metricReport.hasCountMetrics()).isTrue();

        StatsLogReport.CountMetricDataWrapper countData = metricReport.getCountMetrics();

        assertThat(countData.getDataCount()).isGreaterThan(0);
        assertThat(countData.getData(0).getBucketInfo(0).getCount()).isEqualTo(2);
    }
    public void testEventCountWithCondition() throws Exception {
        int startMatcherId = 1;
        int endMatcherId = 2;
        int whatMatcherId = 3;
        int conditionId = 4;

        StatsdConfigProto.AtomMatcher whatMatcher =
               MetricsUtils.unspecifiedAtomMatcher(whatMatcherId);

        StatsdConfigProto.AtomMatcher predicateStartMatcher =
                MetricsUtils.startAtomMatcher(startMatcherId);

        StatsdConfigProto.AtomMatcher predicateEndMatcher =
                MetricsUtils.stopAtomMatcher(endMatcherId);

        StatsdConfigProto.Predicate p = StatsdConfigProto.Predicate.newBuilder()
                .setSimplePredicate(StatsdConfigProto.SimplePredicate.newBuilder()
                        .setStart(startMatcherId)
                        .setStop(endMatcherId)
                        .setCountNesting(false))
                .setId(conditionId)
                .build();

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder()
                .addCountMetric(StatsdConfigProto.CountMetric.newBuilder()
                        .setId(MetricsUtils.COUNT_METRIC_ID)
                        .setBucket(StatsdConfigProto.TimeUnit.CTS)
                        .setWhat(whatMatcherId)
                        .setCondition(conditionId))
                .addAtomMatcher(whatMatcher)
                .addAtomMatcher(predicateStartMatcher)
                .addAtomMatcher(predicateEndMatcher)
                .addPredicate(p);

        uploadConfig(builder);

        doAppBreadcrumbReported(0, AppBreadcrumbReported.State.UNSPECIFIED.ordinal());
        Thread.sleep(10);
        doAppBreadcrumbReportedStart(0);
        Thread.sleep(10);
        doAppBreadcrumbReported(0, AppBreadcrumbReported.State.UNSPECIFIED.ordinal());
        Thread.sleep(10);
        doAppBreadcrumbReportedStop(0);
        Thread.sleep(10);
        doAppBreadcrumbReported(0, AppBreadcrumbReported.State.UNSPECIFIED.ordinal());
        Thread.sleep(2000);  // Wait for the metrics to propagate to statsd.

        StatsLogReport metricReport = getStatsLogReport();
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.COUNT_METRIC_ID);
        assertThat(metricReport.hasCountMetrics()).isTrue();

        StatsLogReport.CountMetricDataWrapper countData = metricReport.getCountMetrics();

        assertThat(countData.getDataCount()).isGreaterThan(0);
        assertThat(countData.getData(0).getBucketInfo(0).getCount()).isEqualTo(1);
    }

    public void testEventCountWithConditionAndActivation() throws Exception {
        int startMatcherId = 1;
        int startMatcherLabel = 1;
        int endMatcherId = 2;
        int endMatcherLabel = 2;
        int whatMatcherId = 3;
        int whatMatcherLabel = 3;
        int conditionId = 4;
        int activationMatcherId = 5;
        int activationMatcherLabel = 5;
        int ttlSec = 5;

        StatsdConfigProto.AtomMatcher whatMatcher =
                MetricsUtils.appBreadcrumbMatcherWithLabel(whatMatcherId, whatMatcherLabel);

        StatsdConfigProto.AtomMatcher predicateStartMatcher =
                MetricsUtils.startAtomMatcherWithLabel(startMatcherId, startMatcherLabel);

        StatsdConfigProto.AtomMatcher predicateEndMatcher =
                MetricsUtils.stopAtomMatcherWithLabel(endMatcherId, endMatcherLabel);

        StatsdConfigProto.AtomMatcher activationMatcher =
                MetricsUtils.appBreadcrumbMatcherWithLabel(activationMatcherId,
                                                           activationMatcherLabel);

        StatsdConfigProto.Predicate p = StatsdConfigProto.Predicate.newBuilder()
                .setSimplePredicate(StatsdConfigProto.SimplePredicate.newBuilder()
                        .setStart(startMatcherId)
                        .setStop(endMatcherId)
                        .setCountNesting(false))
                .setId(conditionId)
                .build();

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder()
                .addCountMetric(StatsdConfigProto.CountMetric.newBuilder()
                        .setId(MetricsUtils.COUNT_METRIC_ID)
                        .setBucket(StatsdConfigProto.TimeUnit.CTS)
                        .setWhat(whatMatcherId)
                        .setCondition(conditionId)
                )
                .addAtomMatcher(whatMatcher)
                .addAtomMatcher(predicateStartMatcher)
                .addAtomMatcher(predicateEndMatcher)
                .addAtomMatcher(activationMatcher)
                .addPredicate(p)
                .addMetricActivation(StatsdConfigProto.MetricActivation.newBuilder()
                        .setMetricId(MetricsUtils.COUNT_METRIC_ID)
                        .setActivationType(StatsdConfigProto.ActivationType.ACTIVATE_IMMEDIATELY)
                        .addEventActivation(StatsdConfigProto.EventActivation.newBuilder()
                                .setAtomMatcherId(activationMatcherId)
                                .setTtlSeconds(ttlSec)));

        uploadConfig(builder);

        // Activate the metric.
        doAppBreadcrumbReported(activationMatcherLabel);
        Thread.sleep(10);

        // Set the condition to true.
        doAppBreadcrumbReportedStart(startMatcherLabel);
        Thread.sleep(10);

        // Log an event that should be counted. Bucket 1 Count 1.
        doAppBreadcrumbReported(whatMatcherLabel);
        Thread.sleep(10);

        // Log an event that should be counted. Bucket 1 Count 2.
        doAppBreadcrumbReported(whatMatcherLabel);
        Thread.sleep(10);

        // Set the condition to false.
        doAppBreadcrumbReportedStop(endMatcherLabel);
        Thread.sleep(10);

        // Log an event that should not be counted because condition is false.
        doAppBreadcrumbReported(whatMatcherLabel);
        Thread.sleep(10);

        // Let the metric deactivate.
        Thread.sleep(ttlSec * 1000);

        // Log an event that should not be counted.
        doAppBreadcrumbReported(whatMatcherLabel);
        Thread.sleep(10);

        // Condition to true again.
        doAppBreadcrumbReportedStart(startMatcherLabel);
        Thread.sleep(10);

        // Event should not be counted, metric is still not active.
        doAppBreadcrumbReported(whatMatcherLabel);
        Thread.sleep(10);

        // Activate the metric.
        doAppBreadcrumbReported(activationMatcherLabel);
        Thread.sleep(10);

        //  Log an event that should be counted.
        doAppBreadcrumbReported(whatMatcherLabel);
        Thread.sleep(10);

        // Let the metric deactivate.
        Thread.sleep(ttlSec * 1000);

        // Log an event that should not be counted.
        doAppBreadcrumbReported(whatMatcherLabel);
        Thread.sleep(10);

        // Wait for the metrics to propagate to statsd.
        Thread.sleep(2000);

        StatsLogReport metricReport = getStatsLogReport();
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.COUNT_METRIC_ID);
        LogUtil.CLog.d("Received the following data: " + metricReport.toString());
        assertThat(metricReport.hasCountMetrics()).isTrue();
        assertThat(metricReport.getIsActive()).isFalse();

        StatsLogReport.CountMetricDataWrapper countData = metricReport.getCountMetrics();
        assertThat(countData.getDataCount()).isEqualTo(1);
        assertThat(countData.getData(0).getBucketInfoCount()).isEqualTo(2);
        assertThat(countData.getData(0).getBucketInfo(0).getCount()).isEqualTo(2);
        assertThat(countData.getData(0).getBucketInfo(1).getCount()).isEqualTo(1);
    }

    public void testPartialBucketCountMetric() throws Exception {
        int matcherId = 1;
        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder();
        builder.addCountMetric(StatsdConfigProto.CountMetric.newBuilder()
                .setId(MetricsUtils.COUNT_METRIC_ID)
                .setBucket(StatsdConfigProto.TimeUnit.ONE_DAY)  // Should ensure partial bucket.
                .setWhat(matcherId))
                .addAtomMatcher(MetricsUtils.simpleAtomMatcher(matcherId));
        uploadConfig(builder);

        doAppBreadcrumbReportedStart(0);

        builder.getCountMetricBuilder(0).setBucket(StatsdConfigProto.TimeUnit.CTS);
        uploadConfig(builder);  // The count metric had a partial bucket.
        doAppBreadcrumbReportedStart(0);
        Thread.sleep(10);
        doAppBreadcrumbReportedStart(0);
        Thread.sleep(WAIT_TIME_LONG); // Finish the current bucket.

        ConfigMetricsReportList reports = getReportList();
        LogUtil.CLog.d("Got following report list: " + reports.toString());

        assertThat(reports.getReportsCount()).isEqualTo(2);
        boolean inOrder = reports.getReports(0).getCurrentReportWallClockNanos() <
                reports.getReports(1).getCurrentReportWallClockNanos();

        // Only 1 metric, so there should only be 1 StatsLogReport.
        for (ConfigMetricsReport report : reports.getReportsList()) {
            assertThat(report.getMetricsCount()).isEqualTo(1);
            assertThat(report.getMetrics(0).getCountMetrics().getDataCount()).isEqualTo(1);
        }
        CountMetricData data1 =
                reports.getReports(inOrder? 0 : 1).getMetrics(0).getCountMetrics().getData(0);
        CountMetricData data2 =
                reports.getReports(inOrder? 1 : 0).getMetrics(0).getCountMetrics().getData(0);
        // Data1 should have only 1 bucket, and it should be a partial bucket.
        // The count should be 1.
        assertThat(data1.getBucketInfoCount()).isEqualTo(1);
        CountBucketInfo bucketInfo = data1.getBucketInfo(0);
        assertThat(bucketInfo.getCount()).isEqualTo(1);
        assertWithMessage("First report's bucket should be less than 1 day")
                .that(bucketInfo.getEndBucketElapsedNanos())
                .isLessThan(bucketInfo.getStartBucketElapsedNanos() +
                        1_000_000_000L * 60L * 60L * 24L);

        //Second report should have a count of 2.
        assertThat(data2.getBucketInfoCount()).isAtMost(2);
        int totalCount = 0;
        for (CountBucketInfo bucket : data2.getBucketInfoList()) {
            totalCount += bucket.getCount();
        }
        assertThat(totalCount).isEqualTo(2);
    }

    public void testSlicedStateCountMetric() throws Exception {
        if (!hasFeature(FEATURE_BLUETOOTH_LE, true)) return;

        int whatMatcherId = 3;
        int stateId = 4;

        // Atom 9999 {
        //     repeated AttributionNode attribution_node = 1;
        //     optional bool is_filtered = 2;
        //     optional bool is_first_match = 3;
        //     optional bool is_opportunistic = 4;
        // }
        int whatAtomId = 9_999;

        StatsdConfigProto.AtomMatcher whatMatcher =
                MetricsUtils.getAtomMatcher(whatAtomId)
                        .setId(whatMatcherId)
                        .build();

        StatsdConfigProto.State state = StatsdConfigProto.State.newBuilder()
            .setId(stateId)
            .setAtomId(Atom.BLE_SCAN_STATE_CHANGED_FIELD_NUMBER)
            .build();

        StatsdConfigProto.MetricStateLink stateLink = StatsdConfigProto.MetricStateLink.newBuilder()
            .setStateAtomId(Atom.BLE_SCAN_STATE_CHANGED_FIELD_NUMBER)
            .setFieldsInWhat(FieldMatcher.newBuilder()
                    .setField(whatAtomId)
                    .addChild(FieldMatcher.newBuilder()
                            .setField(1)
                            .setPosition(Position.FIRST)
                            .addChild(FieldMatcher.newBuilder()
                                    .setField(AttributionNode.UID_FIELD_NUMBER)
                            )
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(2)
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(3)
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(4)
                    )
            )
            .setFieldsInState(FieldMatcher.newBuilder()
                    .setField(Atom.BLE_SCAN_STATE_CHANGED_FIELD_NUMBER)
                    .addChild(FieldMatcher.newBuilder()
                            .setField(BleScanStateChanged.ATTRIBUTION_NODE_FIELD_NUMBER)
                            .setPosition(Position.FIRST)
                            .addChild(FieldMatcher.newBuilder()
                                    .setField(AttributionNode.UID_FIELD_NUMBER)
                            )
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(BleScanStateChanged.IS_FILTERED_FIELD_NUMBER)
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(BleScanStateChanged.IS_FIRST_MATCH_FIELD_NUMBER)
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(BleScanStateChanged.IS_OPPORTUNISTIC_FIELD_NUMBER)
                    )
            )
            .build();

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder()
                .addCountMetric(StatsdConfigProto.CountMetric.newBuilder()
                    .setId(MetricsUtils.COUNT_METRIC_ID)
                    .setBucket(StatsdConfigProto.TimeUnit.CTS)
                    .setWhat(whatMatcherId)
                    .addSliceByState(stateId)
                    .addStateLink(stateLink)
                )
                .addAtomMatcher(whatMatcher)
                .addState(state);
        uploadConfig(builder);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testBleScanInterrupted");

        StatsLogReport metricReport = getStatsLogReport();
        LogUtil.CLog.d("Got the following stats log report: \n" + metricReport.toString());
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.COUNT_METRIC_ID);
        assertThat(metricReport.hasCountMetrics()).isTrue();

        StatsLogReport.CountMetricDataWrapper dataWrapper = metricReport.getCountMetrics();
        assertThat(dataWrapper.getDataCount()).isEqualTo(2);

        CountMetricData data = dataWrapper.getData(0);
        assertThat(data.getSliceByStateCount()).isEqualTo(1);
        assertThat(data.getSliceByState(0).getAtomId())
                .isEqualTo(Atom.BLE_SCAN_STATE_CHANGED_FIELD_NUMBER);
        assertThat(data.getSliceByState(0).getValue())
                .isEqualTo(BleScanStateChanged.State.OFF.ordinal());
        long totalCount = data.getBucketInfoList().stream()
                .mapToLong(CountBucketInfo::getCount)
                .sum();
        assertThat(totalCount).isEqualTo(3);

        data = dataWrapper.getData(1);
        assertThat(data.getSliceByStateCount()).isEqualTo(1);
        assertThat(data.getSliceByState(0).getAtomId())
                .isEqualTo(Atom.BLE_SCAN_STATE_CHANGED_FIELD_NUMBER);
        assertThat(data.getSliceByState(0).getValue())
                .isEqualTo(BleScanStateChanged.State.ON.ordinal());
        totalCount = data.getBucketInfoList().stream()
                .mapToLong(CountBucketInfo::getCount)
                .sum();
        assertThat(totalCount).isEqualTo(2);
    }

    public void testSlicedStateCountMetricNoReset() throws Exception {
        int whatMatcherId = 3;
        int stateId = 4;
        int onStateGroupId = 5;
        int offStateGroupId = 6;

        // Atom 9998 {
        //     repeated AttributionNode attribution_node = 1;
        //     optional WakeLockLevelEnum type = 2;
        //     optional string tag = 3;
        // }
        int whatAtomId = 9_998;

        StatsdConfigProto.AtomMatcher whatMatcher =
                MetricsUtils.getAtomMatcher(whatAtomId)
                        .setId(whatMatcherId)
                        .build();

        StatsdConfigProto.State state = StatsdConfigProto.State.newBuilder()
            .setId(stateId)
            .setAtomId(Atom.WAKELOCK_STATE_CHANGED_FIELD_NUMBER)
            .setMap(StatsdConfigProto.StateMap.newBuilder()
                    .addGroup(StatsdConfigProto.StateMap.StateGroup.newBuilder()
                            .setGroupId(onStateGroupId)
                            .addValue(WakelockStateChanged.State.ACQUIRE_VALUE)
                            .addValue(WakelockStateChanged.State.CHANGE_ACQUIRE_VALUE)
                    )
                    .addGroup(StatsdConfigProto.StateMap.StateGroup.newBuilder()
                            .setGroupId(offStateGroupId)
                            .addValue(WakelockStateChanged.State.RELEASE_VALUE)
                            .addValue(WakelockStateChanged.State.CHANGE_RELEASE_VALUE)
                    )
            )
            .build();

        StatsdConfigProto.MetricStateLink stateLink = StatsdConfigProto.MetricStateLink.newBuilder()
            .setStateAtomId(Atom.WAKELOCK_STATE_CHANGED_FIELD_NUMBER)
            .setFieldsInWhat(FieldMatcher.newBuilder()
                    .setField(whatAtomId)
                    .addChild(FieldMatcher.newBuilder()
                            .setField(1)
                            .setPosition(Position.FIRST)
                            .addChild(FieldMatcher.newBuilder()
                                    .setField(AttributionNode.UID_FIELD_NUMBER)
                            )
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(2)
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(3)
                    )
            )
            .setFieldsInState(FieldMatcher.newBuilder()
                    .setField(Atom.WAKELOCK_STATE_CHANGED_FIELD_NUMBER)
                    .addChild(FieldMatcher.newBuilder()
                            .setField(WakelockStateChanged.ATTRIBUTION_NODE_FIELD_NUMBER)
                            .setPosition(Position.FIRST)
                            .addChild(FieldMatcher.newBuilder()
                                    .setField(AttributionNode.UID_FIELD_NUMBER)
                            )
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(WakelockStateChanged.TYPE_FIELD_NUMBER)
                    )
                    .addChild(FieldMatcher.newBuilder()
                            .setField(WakelockStateChanged.TAG_FIELD_NUMBER)
                    )
            )
            .build();

        StatsdConfigProto.StatsdConfig.Builder builder = createConfigBuilder()
                .addCountMetric(StatsdConfigProto.CountMetric.newBuilder()
                    .setId(MetricsUtils.COUNT_METRIC_ID)
                    .setBucket(StatsdConfigProto.TimeUnit.CTS)
                    .setWhat(whatMatcherId)
                    .addSliceByState(stateId)
                    .addStateLink(stateLink)
                )
                .addAtomMatcher(whatMatcher)
                .addState(state);
        uploadConfig(builder);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testSliceByWakelockState");

        StatsLogReport metricReport = getStatsLogReport();
        LogUtil.CLog.d("Got the following stats log report: \n" + metricReport.toString());
        assertThat(metricReport.getMetricId()).isEqualTo(MetricsUtils.COUNT_METRIC_ID);
        assertThat(metricReport.hasCountMetrics()).isTrue();

        StatsLogReport.CountMetricDataWrapper dataWrapper = metricReport.getCountMetrics();
        assertThat(dataWrapper.getDataCount()).isEqualTo(2);


        List<CountMetricData> sortedDataList = IntStream.range(0, dataWrapper.getDataCount())
                .mapToObj(i -> {
                        CountMetricData data = dataWrapper.getData(i);
                        assertWithMessage("Unexpected SliceByState count for data[%s]", "" + i)
                                .that(data.getSliceByStateCount()).isEqualTo(1);
                        return data;
                })
                .sorted((data1, data2) ->
                        Long.compare(data1.getSliceByState(0).getGroupId(),
                                data2.getSliceByState(0).getGroupId())
                )
                .collect(Collectors.toList());

        CountMetricData data = sortedDataList.get(0);
        assertThat(data.getSliceByState(0).getAtomId())
                .isEqualTo(Atom.WAKELOCK_STATE_CHANGED_FIELD_NUMBER);
        assertThat(data.getSliceByState(0).getGroupId())
                .isEqualTo(onStateGroupId);
        long totalCount = data.getBucketInfoList().stream()
                .mapToLong(CountBucketInfo::getCount)
                .sum();
        assertThat(totalCount).isEqualTo(6);

        data = sortedDataList.get(1);
        assertThat(data.getSliceByState(0).getAtomId())
                .isEqualTo(Atom.WAKELOCK_STATE_CHANGED_FIELD_NUMBER);
        assertThat(data.getSliceByState(0).getGroupId())
                .isEqualTo(offStateGroupId);
        totalCount = data.getBucketInfoList().stream()
                .mapToLong(CountBucketInfo::getCount)
                .sum();
        assertThat(totalCount).isEqualTo(3);
    }
}
