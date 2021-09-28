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
package com.android.tradefed.postprocessor;

import com.android.tradefed.metrics.proto.MetricMeasurement.DoubleValues;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements.MeasurementCase;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric.Builder;
import com.android.tradefed.metrics.proto.MetricMeasurement.NumericValues;
import com.android.tradefed.result.LogFile;

import java.util.HashMap;
import java.util.Map;
import java.util.stream.Collectors;

/** Implementation of post processor that calculate the average of the list of metrics. */
public class AveragePostProcessor extends BasePostProcessor {

    public static final String AVERAGE_KEY_TAG = "_avg";

    @Override
    public Map<String, Builder> processRunMetricsAndLogs(
            HashMap<String, Metric> rawMetrics, Map<String, LogFile> runLogs) {
        Map<String, Builder> newMetrics = new HashMap<>();
        for (String key : rawMetrics.keySet()) {
            Metric metric = rawMetrics.get(key);
            Measurements measure = metric.getMeasurements();
            MeasurementCase measureCase = measure.getMeasurementCase();
            if (MeasurementCase.DOUBLE_VALUES.equals(measureCase)) {
                DoubleValues doubleValues = measure.getDoubleValues();
                if (doubleValues.getDoubleValueList().size() > 0) {
                    double avg =
                            doubleValues
                                    .getDoubleValueList()
                                    .stream()
                                    .collect(Collectors.summarizingDouble(Double::doubleValue))
                                    .getAverage();
                    newMetrics.put(key + AVERAGE_KEY_TAG, createAvgMetric(avg, metric));
                }
            } else if (MeasurementCase.NUMERIC_VALUES.equals(measureCase)) {
                NumericValues numValues = measure.getNumericValues();
                if (numValues.getNumericValueList().size() > 0) {
                    double avg =
                            numValues
                                    .getNumericValueList()
                                    .stream()
                                    .collect(Collectors.summarizingLong(Long::longValue))
                                    .getAverage();
                    newMetrics.put(key + AVERAGE_KEY_TAG, createAvgMetric(avg, metric));
                }
            }
        }
        return newMetrics;
    }

    private Builder createAvgMetric(double avg, Metric originalMetric) {
        Metric.Builder builder = Metric.newBuilder();
        builder.setMeasurements(Measurements.newBuilder().setSingleDouble(avg).build());
        builder.setUnit(originalMetric.getUnit());
        return builder;
    }
}
