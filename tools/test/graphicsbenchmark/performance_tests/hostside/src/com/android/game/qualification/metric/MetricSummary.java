/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.metric;

import com.android.annotations.Nullable;
import com.android.game.qualification.CertificationRequirements;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.DataType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;

import com.google.common.base.Preconditions;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * Summary of frame time metrics data.
 */
public class MetricSummary {
    public enum TimeType {
        PRESENT,
        READY
    }

    private int loopCount;
    private long loadTimeMs;
    private Map<TimeType, List<LoopSummary>> summaries;

    private MetricSummary(
            int loopCount,
            long loadTimeMs,
            Map<TimeType, List<LoopSummary>> summaries) {
        this.loopCount = loopCount;
        this.loadTimeMs = loadTimeMs;
        this.summaries = summaries;
    }

    @Nullable
    public static MetricSummary parseRunMetrics(
            IInvocationContext context, HashMap<String, Metric> metrics) {
        int loopCount = 0;
        if (metrics.containsKey("loop_count")) {
            loopCount = (int) metrics.get("loop_count").getMeasurements().getSingleInt();
        }

        if (loopCount == 0) {
            return null;
        }

        Map<TimeType, List<LoopSummary>> summaries = new LinkedHashMap<>();
        for (TimeType type : TimeType.values()) {
            summaries.put(type, new ArrayList<>());
            for (int i = 0; i < loopCount; i++) {
                LoopSummary loopSummary = LoopSummary.parseRunMetrics(context, type, i, metrics);
                summaries.get(type).add(loopSummary);
            }
        }
        return new MetricSummary(
                loopCount,
                metrics.get("load_time").getMeasurements().getSingleInt(),
                summaries);
    }

    public long getLoadTimeMs() {
        return loadTimeMs;
    }

    public List<LoopSummary> getLoopSummaries() {
        return summaries.get(TimeType.PRESENT);
    }

    public void addToMetricData(DeviceMetricData runData) {
        runData.addMetric(
                "loop_count",
                Metric.newBuilder()
                        .setType(DataType.PROCESSED)
                        .setMeasurements(Measurements.newBuilder().setSingleInt(loopCount)));
        runData.addMetric(
                "load_time",
                Metric.newBuilder()
                        .setType(DataType.RAW)
                        .setMeasurements(Measurements.newBuilder().setSingleInt(loadTimeMs)));

        for (int i = 0; i < loopCount; i++) {
            for (TimeType type : TimeType.values()) {
                LoopSummary summary = summaries.get(type).get(i);
                summary.addToMetricData(runData, i, type);
            }
        }
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        MetricSummary summary = (MetricSummary) o;
        return loopCount == summary.loopCount &&
                loadTimeMs == summary.loadTimeMs &&
                Objects.equals(summaries, summary.summaries);
    }

    @Override
    public int hashCode() {
        return Objects.hash(loopCount, loadTimeMs, summaries);
    }

    public String toString() {
        StringBuilder sb = new StringBuilder();
        // Report primary metrics.
        sb.append("Summary\n");
        sb.append("-------\n");
        sb.append("Load time: ");
        if (getLoadTimeMs() == -1) {
            sb.append("unknown");
        } else {
            sb.append(getLoadTimeMs());
            sb.append(" ms\n");
        }
        sb.append("\n");

        // Report secondary metrics.
        sb.append("Details\n");
        sb.append("-------\n");


        for (int i = 0; i < loopCount; i++) {
            if (summaries.get(TimeType.PRESENT).get(i).getCount() == 0) {
                continue;
            }
            sb.append("Loop ");
            sb.append(i);
            sb.append('\n');
            for (TimeType type : TimeType.values()) {
                sb.append(type);
                sb.append(" Time Statistics\n");
                sb.append(summaries.get(type).get(i));
                sb.append("\n");
            }
        }
        return sb.toString();
    }

    private static long msToNs(float value) {
        return (long) (value * 1e6f);
    }

    public static class Builder {
        @Nullable
        private CertificationRequirements mRequirements;
        private long mVSyncPeriodNs;
        private int loopCount = 0;
        private long loadTimeMs = -1;
        private Map<TimeType, List<LoopSummary.Builder>> summaries = new LinkedHashMap<>();

        public Builder(@Nullable CertificationRequirements requirements, long vSyncPeriodNs) {
            mRequirements = requirements;
            mVSyncPeriodNs = vSyncPeriodNs;
            for (TimeType type : TimeType.values()) {
                summaries.put(type, new ArrayList<>());
            }
        }

        private LoopSummary.Builder getLatestSummary(TimeType type) {
            Preconditions.checkState(loopCount > 0, "First loop has not been started.");
            List<LoopSummary.Builder> list = summaries.get(type);
            return list.get(list.size() - 1);
        }

        public void setLoadTimeMs(long loadTimeMs) {
            this.loadTimeMs = loadTimeMs;
        }

        public void addFrameTime(TimeType type, long frameTimeNs) {
            LoopSummary.Builder summary = getLatestSummary(type);
            summary.addFrameTime(frameTimeNs);
        }

        public void beginLoop() {
            loopCount++;
            for (TimeType type : TimeType.values()) {
                summaries.get(type).add(new LoopSummary.Builder(mRequirements, mVSyncPeriodNs));
            }
        }

        public void endLoop() {
            // Do nothing.
        }

        public MetricSummary build() {
            Map<TimeType, List<LoopSummary>> summaryMap = new HashMap<>();
            for (Map.Entry<TimeType, List<LoopSummary.Builder>> entry : summaries.entrySet()) {
                summaryMap.put(
                        entry.getKey(),
                        entry.getValue().stream()
                                .map(LoopSummary.Builder::build)
                                .collect(Collectors.toList()));
            }
            return new MetricSummary(loopCount, loadTimeMs, summaryMap);
        }
    }
}
