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
package com.android.tradefed.postprocessor;

import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableList;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * A post processor that processes binary proto statsd reports into key-value pairs by expanding the
 * report as a tree structure.
 *
 * <p>This processor is agnostic to the type of metric reports it encounters. It also serves as the
 * base class for other statsd post processors by including common code to retrieve and read statsd
 * reports.
 */
@OptionClass(alias = "statsd-generic-processor")
public class StatsdGenericPostProcessor extends BasePostProcessor {
    // TODO(harrytczhang): Add ability to dump the results out to a JSON file once saving files from
    // post processors is possible.

    @Option(
        name = "statsd-report-data-prefix",
        description =
                "Prefix for identifying a data name that points to a statsd report. "
                        + "Can be repeated. "
                        + "Will also be prepended to the outcoming metric to \"namespace\" them."
    )
    private Set<String> mReportPrefixes = new HashSet<>();

    @VisibleForTesting static final String METRIC_SEP = "-";
    @VisibleForTesting static final String INDEX_SEP = "#";

    // A few fields to skip as they are large and not currently used.
    private static final String UID_MAP_FIELD_NAME = "uid_map";
    private static final String STRINGS_FIELD_NAME = "strings";
    private static final ImmutableList FIELDS_TO_SKIP =
            ImmutableList.of(UID_MAP_FIELD_NAME, STRINGS_FIELD_NAME);

    @Override
    public Map<String, Metric.Builder> processTestMetricsAndLogs(
            TestDescription testDescription,
            HashMap<String, Metric> testMetrics,
            Map<String, LogFile> testLogs) {
        return processStatsdReportsFromLogs(testLogs);
    }

    @Override
    public Map<String, Metric.Builder> processRunMetricsAndLogs(
            HashMap<String, Metric> rawMetrics, Map<String, LogFile> runLogs) {
        return processStatsdReportsFromLogs(runLogs);
    }

    /**
     * Parse metrics from a {@link ConfigMetricsReportList} read from a statsd report proto.
     *
     * <p>This is the main interface for subclasses of this statsd post processor.
     */
    protected Map<String, Metric.Builder> parseMetricsFromReportList(
            ConfigMetricsReportList reportList) {
        return convertProtoMessage(reportList);
    }

    /**
     * Uses the metrics to locate the statsd report files and then call into the processing method
     * to parse the reports into metrics.
     */
    private Map<String, Metric.Builder> processStatsdReportsFromLogs(Map<String, LogFile> logs) {
        Map<String, Metric.Builder> parsedMetrics = new HashMap<>();

        for (String key : logs.keySet()) {
            // Go through the logs and match them to the list of prefixes. Only proceed when a match
            // is found.
            Optional<String> reportPrefix =
                    mReportPrefixes.stream().filter(prefix -> key.startsWith(prefix)).findAny();
            if (!reportPrefix.isPresent()) {
                continue;
            }

            File reportFile = new File(logs.get(key).getPath());
            try (FileInputStream reportStream = new FileInputStream(reportFile)) {
                ConfigMetricsReportList reportList =
                        ConfigMetricsReportList.parseFrom(reportStream);
                if (reportList.getReportsList().isEmpty()) {
                    CLog.i("No reports collected for %s.", key);
                    continue;
                }
                Map<String, Metric.Builder> metricsForReport =
                        parseMetricsFromReportList(reportList);
                // Prepend the report prefix to serve as top-level organization of the outputted
                // metrics. Please see comment on the prepend method for details.
                parsedMetrics.putAll(addPrefixToMetrics(metricsForReport, reportPrefix.get()));
            } catch (FileNotFoundException e) {
                CLog.e("Report file does not exist at supposed path %s.", logs.get(key).getPath());
            } catch (IOException e) {
                CLog.i("Failed to read report file due to error %s.", e.toString());
            }
        }

        return parsedMetrics;
    }

    /**
     * Add a prefix to the metrics out of a single report.
     *
     * <p>This is used to add top-level organization for the metric output as due to the nesting
     * nature of the metrics the actual metric key can be hard to read at a glance without the
     * prefixes added here. As an example, if we use "statsd-app-start" as a prefix, then some
     * of the metrics would read:
     *
     * <pre>
     * statsd-app-start-reports-metrics-event_metrics-data-atom-app_start_occurred-type: WARM
     * statsd-app-start-reports-metrics-event_metrics-data-atom-app_start_occurred-uid: 10149
     * statsd-app-start-reports-metrics-event_metrics-data-elapsed_timestamp_nanos: 1449806339931935
     */
    private Map<String, Metric.Builder> addPrefixToMetrics(
            Map<String, Metric.Builder> metrics, String prefix) {
        return metrics.entrySet()
                .stream()
                .collect(
                        Collectors.toMap(
                                e -> String.join(METRIC_SEP, prefix, e.getKey()),
                                e -> e.getValue()));
    }

    /**
     * Flatten a proto message to a set of key-value pairs which become metrics.
     *
     * <p>It treats a message as a tree and uses the concatenated path from the root to a
     * non-message value as the key, while the non-message value becomes the metric value. Nodes
     * from repeated fields are distinguished by having a 1-based index number appended to all
     * elements after the first element. The first element is not appended as in most cases only one
     * element is in the list field and having it appear as-is is easier to read.
     *
     * <p>TODO(b/140432161): Separate this out into a utility should the need arise.
     */
    protected Map<String, Metric.Builder> convertProtoMessage(Message reportMessage) {
        Map<FieldDescriptor, Object> fields = reportMessage.getAllFields();
        Map<String, Metric.Builder> convertedMetrics = new HashMap<String, Metric.Builder>();
        for (Entry<FieldDescriptor, Object> entry : fields.entrySet()) {
            if (FIELDS_TO_SKIP.contains(entry.getKey().getName())) {
                continue;
            }
            if (entry.getValue() instanceof Message) {
                Map<String, Metric.Builder> messageMetrics =
                        convertProtoMessage((Message) entry.getValue());
                for (Entry<String, Metric.Builder> metricEntry : messageMetrics.entrySet()) {
                    convertedMetrics.put(
                            String.join(METRIC_SEP, entry.getKey().getName(), metricEntry.getKey()),
                            metricEntry.getValue());
                }
            } else if (entry.getValue() instanceof List) {
                List<? extends Object> listMetrics = (List) entry.getValue();
                for (int i = 0; i < listMetrics.size(); i++) {
                    String metricKeyRoot =
                            (i == 0
                                    ? entry.getKey().getName()
                                    : String.join(
                                            INDEX_SEP,
                                            entry.getKey().getName(),
                                            String.valueOf(i + 1)));
                    if (listMetrics.get(i) instanceof Message) {
                        Map<String, Metric.Builder> messageMetrics =
                                convertProtoMessage((Message) listMetrics.get(i));
                        for (Entry<String, Metric.Builder> metricEntry :
                                messageMetrics.entrySet()) {
                            convertedMetrics.put(
                                    String.join(METRIC_SEP, metricKeyRoot, metricEntry.getKey()),
                                    metricEntry.getValue());
                        }
                    } else {
                        convertedMetrics.put(
                                metricKeyRoot,
                                TfMetricProtoUtil.stringToMetric(listMetrics.get(i).toString())
                                        .toBuilder());
                    }
                }
            } else {
                convertedMetrics.put(
                        entry.getKey().getName(),
                        TfMetricProtoUtil.stringToMetric(entry.getValue().toString()).toBuilder());
            }
        }
        return convertedMetrics;
    }
}
