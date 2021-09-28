/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.MockitoAnnotations.initMocks;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.postprocessor.PerfettoGenericPostProcessor.METRIC_FILE_FORMAT;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.ZipUtil;

import com.google.protobuf.TextFormat;
import com.google.protobuf.TextFormat.ParseException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import perfetto.protos.PerfettoMergedMetrics.TraceMetrics;

/** Unit tests for {@link PerfettoGenericPostProcessor}. */
@RunWith(JUnit4.class)
public class PerfettoGenericPostProcessorTest {

    @Mock private ITestInvocationListener mListener;
    private PerfettoGenericPostProcessor mProcessor;
    private OptionSetter mOptionSetter;

    private static final String PREFIX_OPTION = "perfetto-proto-file-prefix";
    private static final String PREFIX_OPTION_VALUE = "metric-perfetto";
    private static final String INDEX_OPTION = "perfetto-indexed-list-field";
    private static final String KEY_PREFIX_OPTION = "perfetto-prefix-key-field";
    private static final String REGEX_OPTION_VALUE = "perfetto-metric-filter-regex";
    private static final String ALL_METRICS_OPTION = "perfetto-include-all-metrics";
    private static final String ALL_METRICS_PREFIX_OPTION = "perfetto-all-metric-prefix";
    private static final String REPLACE_REGEX_OPTION = "perfetto-metric-replace-prefix";
    private static final String FILE_FORMAT_OPTION = "trace-processor-output-format";

    File perfettoMetricProtoFile = null;

    @Before
    public void setUp() throws ConfigurationException {
        initMocks(this);
        mProcessor = new PerfettoGenericPostProcessor();
        mProcessor.init(mListener);
        mOptionSetter = new OptionSetter(mProcessor);
    }

    /**
     * Test metrics count should be zero if "perfetto-include-all-metrics" is not set or set to
     * false;
     */
    @Test
    public void testNoMetricsByDefault() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(INDEX_OPTION, "perfetto.protos.AndroidStartupMetric.startup");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);

        assertTrue(
                "Number of metrics parsed without indexing is incorrect.",
                parsedMetrics.size() == 0);
    }

    /**
     * Test metrics are filtered correctly when filter regex are passed and
     * "perfetto-include-all-metrics" is set to false (Note: by default false)
     */
    @Test
    public void testMetricsFilterWithRegEx() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(INDEX_OPTION, "perfetto.protos.AndroidStartupMetric.startup");
        mOptionSetter.setOptionValue(REGEX_OPTION_VALUE, "android_startup-startup-1.*");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);

        assertMetricsContain(parsedMetrics, "perfetto_android_startup-startup-1-startup_id", 1);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_startup-startup-1-package_name-com.google."
                        + "android.apps.nexuslauncher-to_first_frame-dur_ns",
                36175473);
    }

    /**
     * Test metrics are filtered correctly when filter regex are passed and
     * prefix are replaced with the given string.
     */
    @Test
    public void testMetricsFilterWithRegExAndReplacePrefix()
            throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(INDEX_OPTION, "perfetto.protos.AndroidStartupMetric.startup");
        mOptionSetter.setOptionValue(REGEX_OPTION_VALUE, "android_startup-startup-1.*");
        mOptionSetter.setOptionValue(REPLACE_REGEX_OPTION, "android_startup-startup-1",
                "newprefix");

        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics = mProcessor
                .processRunMetricsAndLogs(new HashMap<>(), testLogs);

        assertFalse("Metric key not expected but found",
                parsedMetrics.containsKey("android_startup-startup-1-startup_id"));
        assertMetricsContain(parsedMetrics, "perfetto_newprefix-startup_id", 1);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_newprefix-package_name-com.google."
                        + "android.apps.nexuslauncher-to_first_frame-dur_ns",
                36175473);
    }

    /**
     * Test all metrics are included when "perfetto-include-all-metrics" is set to true and ignores
     * any of the filter regex set.
     */
    @Test
    public void testAllMetricsOptionIgnoresFilter() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(INDEX_OPTION, "perfetto.protos.AndroidStartupMetric.startup");
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);
        // Test for non startup metrics exists.
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_mem-process_metrics-process_name-"
                        + ".dataservices-total_counters-anon_rss-min",
                27938816);
    }

    /** Test that the post processor can parse reports from test metrics. */
    @Test
    public void testParsingTestMetrics() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processTestMetricsAndLogs(
                        new TestDescription("class", "test"), new HashMap<>(), testLogs);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_mem-process_metrics-process_name-"
                        + ".dataservices-total_counters-anon_rss-min",
                27938816);
    }

    /** Test custom all metric suffix is applied correctly. */
    @Test
    public void testParsingWithAllMetricsPrefix() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        mOptionSetter.setOptionValue(ALL_METRICS_PREFIX_OPTION, "custom_all_prefix");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processTestMetricsAndLogs(
                        new TestDescription("class", "test"), new HashMap<>(), testLogs);
        assertMetricsContain(
                parsedMetrics,
                "custom_all_prefix_android_mem-process_metrics-process_name-"
                        + ".dataservices-total_counters-anon_rss-min",
                27938816);
    }

    /** Test the post processor can parse reports from run metrics. */
    @Test
    public void testParsingRunMetrics() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_mem-process_metrics-process_name-"
                        + ".dataservices-total_counters-anon_rss-min",
                27938816);
    }

    /**
     * Test metrics count and metrics without indexing. In case of app startup metrics startup
     * messages for same package name will be overridden without indexing.
     */
    @Test
    public void testParsingWithoutIndexing() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);
        assertMetricsContain(parsedMetrics, "perfetto_android_startup-startup-startup_id", 2);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_startup-startup-package_name-com.google."
                        + "android.apps.nexuslauncher-to_first_frame-dur_ns",
                53102401);
    }

    /**
     * Test metrics count and metrics with indexing. In case of app startup metrics, startup
     * messages for same package name will not be overridden with indexing.
     */
    @Test
    public void testParsingWithIndexing() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(INDEX_OPTION, "perfetto.protos.AndroidStartupMetric.startup");
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);

        assertMetricsContain(parsedMetrics, "perfetto_android_startup-startup-1-startup_id", 1);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_startup-startup-1-package_name-com.google."
                        + "android.apps.nexuslauncher-to_first_frame-dur_ns",
                36175473);
        assertMetricsContain(parsedMetrics, "perfetto_android_startup-startup-2-startup_id", 2);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_startup-startup-2-package_name-com.google."
                        + "android.apps.nexuslauncher-to_first_frame-dur_ns",
                53102401);
    }

    /**
     * Test metrics enabled with key prefixing.
     */
    @Test
    public void testParsingWithKeyPrefixing() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(KEY_PREFIX_OPTION,
                "perfetto.protos.ProcessRenderInfo.process_name");
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);

        assertMetricsContain(parsedMetrics,
                "perfetto_android_hwui_metric-process_info-process_name-com.android.systemui-all_mem_min",
                15120269);

    }


    /** Test the post processor can parse binary perfetto metric proto format. */
    @Test
    public void testParsingBinaryProto() throws ConfigurationException, IOException {
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.binary, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        mOptionSetter.setOptionValue(FILE_FORMAT_OPTION, "binary");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.PB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_mem-process_metrics-process_name-"
                        + ".dataservices-total_counters-anon_rss-min",
                27938816);
    }

    /** Test the post processor can parse binary perfetto metric proto format. */
    @Test
    public void testNoSupportForJsonParsing() throws ConfigurationException, IOException {
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        mOptionSetter.setOptionValue(FILE_FORMAT_OPTION, "json");
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);
        assertTrue("Should not have any metrics if json format is set", parsedMetrics.size() == 0);
    }

    /**
     * Test the post processor can parse reports from run metrics when the text proto file is
     * compressed format.
     */
    @Test
    public void testParsingRunMetricsWithCompressedFile()
            throws ConfigurationException, IOException {
        // Setup compressed text proto metric file.
        setupPerfettoMetricFile(METRIC_FILE_FORMAT.text, true);
        mOptionSetter.setOptionValue(PREFIX_OPTION, PREFIX_OPTION_VALUE);
        mOptionSetter.setOptionValue(ALL_METRICS_OPTION, "true");
        Map<String, LogFile> testLogs = new HashMap<>();
        testLogs.put(
                PREFIX_OPTION_VALUE,
                new LogFile(
                        perfettoMetricProtoFile.getAbsolutePath(), "some.url", LogDataType.TEXTPB));
        Map<String, Metric.Builder> parsedMetrics =
                mProcessor.processRunMetricsAndLogs(new HashMap<>(), testLogs);
        assertMetricsContain(
                parsedMetrics,
                "perfetto_android_mem-process_metrics-process_name-"
                        + ".dataservices-total_counters-anon_rss-min",
                27938816);
    }

    /** Creates sample perfetto metric proto file used for testing. */
    private File setupPerfettoMetricFile(METRIC_FILE_FORMAT format, boolean isCompressed)
            throws IOException {
        String perfettoTextContent =
                "android_mem {\n"
                        + "  process_metrics {\n"
                        + "    process_name: \".dataservices\"\n"
                        + "    total_counters {\n"
                        + "      anon_rss {\n"
                        + "        min: 27938816\n"
                        + "        max: 27938816\n"
                        + "        avg: 27938816\n"
                        + "      }\n"
                        + "      file_rss {\n"
                        + "        min: 62390272\n"
                        + "        max: 62390272\n"
                        + "        avg: 62390272\n"
                        + "      }\n"
                        + "      swap {\n"
                        + "        min: 0\n"
                        + "        max: 0\n"
                        + "        avg: 0\n"
                        + "      }\n"
                        + "      anon_and_swap {\n"
                        + "        min: 27938816\n"
                        + "        max: 27938816\n"
                        + "        avg: 27938816\n"
                        + "      }\n"
                        + "    }\n"
                        + "}}"
                        + "android_startup {\n"
                        + "  startup {\n"
                        + "    startup_id: 1\n"
                        + "    package_name: \"com.google.android.apps.nexuslauncher\"\n"
                        + "    process_name: \"com.google.android.apps.nexuslauncher\"\n"
                        + "    zygote_new_process: false\n"
                        + "    to_first_frame {\n"
                        + "      dur_ns: 36175473\n"
                        + "      main_thread_by_task_state {\n"
                        + "        running_dur_ns: 11496200\n"
                        + "        runnable_dur_ns: 487290\n"
                        + "        uninterruptible_sleep_dur_ns: 0\n"
                        + "        interruptible_sleep_dur_ns: 23645107\n"
                        + "      }\n"
                        + "      other_processes_spawned_count: 0\n"
                        + "      time_activity_manager {\n"
                        + "        dur_ns: 4135001\n"
                        + "      }\n"
                        + "      time_activity_resume {\n"
                        + "        dur_ns: 345105\n"
                        + "      }\n"
                        + "      time_choreographer {\n"
                        + "        dur_ns: 15314324\n"
                        + "      }\n"
                        + "    }\n"
                        + "    activity_hosting_process_count: 1\n"
                        + "  }\n"
                        + "  startup {\n"
                        + "    startup_id: 2\n"
                        + "    package_name: \"com.google.android.apps.nexuslauncher\"\n"
                        + "    process_name: \"com.google.android.apps.nexuslauncher\"\n"
                        + "    zygote_new_process: false\n"
                        + "    to_first_frame {\n"
                        + "      dur_ns: 53102401\n"
                        + "      main_thread_by_task_state {\n"
                        + "        running_dur_ns: 9766774\n"
                        + "        runnable_dur_ns: 320103\n"
                        + "        uninterruptible_sleep_dur_ns: 0\n"
                        + "        interruptible_sleep_dur_ns: 42358858\n"
                        + "      }\n"
                        + "      other_processes_spawned_count: 0\n"
                        + "      time_activity_manager {\n"
                        + "        dur_ns: 4742396\n"
                        + "      }\n"
                        + "      time_activity_resume {\n"
                        + "        dur_ns: 280208\n"
                        + "      }\n"
                        + "      time_choreographer {\n"
                        + "        dur_ns: 13705366\n"
                        + "      }\n"
                        + "    }\n"
                        + "    activity_hosting_process_count: 1\n"
                        + "  }\n"
                        + "}\n"
                        + "android_hwui_metric {\n" +
                        "  process_info {\n" +
                        "    process_name: \"com.android.systemui\"\n" +
                        "    rt_cpu_time_ms: 2481\n" +
                        "    draw_frame_count: 889\n" +
                        "    draw_frame_max: 21990523\n" +
                        "    draw_frame_min: 660573\n" +
                        "    draw_frame_avg: 3515215.0101237344\n" +
                        "    flush_count: 884\n" +
                        "    flush_max: 8101094\n" +
                        "    flush_min: 127760\n" +
                        "    flush_avg: 773943.91515837109\n" +
                        "    prepare_tree_count: 889\n" +
                        "    prepare_tree_max: 1718593\n" +
                        "    prepare_tree_min: 25052\n" +
                        "    prepare_tree_avg: 133403.03374578178\n" +
                        "    gpu_completion_count: 572\n" +
                        "    gpu_completion_max: 8600365\n" +
                        "    gpu_completion_min: 3594\n" +
                        "    gpu_completion_avg: 737765.40209790214\n" +
                        "    ui_record_count: 889\n" +
                        "    ui_record_max: 7079949\n" +
                        "    ui_record_min: 4583\n" +
                        "    ui_record_avg: 477551.82902137231\n" +
                        "    graphics_cpu_mem_max: 265242\n" +
                        "    graphics_cpu_mem_min: 244198\n" +
                        "    graphics_cpu_mem_avg: 260553.33484162897\n" +
                        "    graphics_gpu_mem_max: 34792176\n" +
                        "    graphics_gpu_mem_min: 9855728\n" +
                        "    graphics_gpu_mem_avg: 19030174.914027151\n" +
                        "    texture_mem_max: 5217091\n" +
                        "    texture_mem_min: 5020343\n" +
                        "    texture_mem_avg: 5177376.0407239823\n" +
                        "    all_mem_max: 40274509\n" +
                        "    all_mem_min: 15120269\n" +
                        "    all_mem_avg: 24468104.289592762\n" +
                        "  }\n" +
                        "}";
        FileWriter fileWriter = null;
        try {
            perfettoMetricProtoFile = FileUtil.createTempFile("metric_perfetto", "");
            fileWriter = new FileWriter(perfettoMetricProtoFile);
            fileWriter.write(perfettoTextContent);
        } finally {
            if (fileWriter != null) {
                fileWriter.close();
            }
        }

        if (format.equals(METRIC_FILE_FORMAT.binary)) {
            File perfettoBinaryFile = FileUtil.createTempFile("metric_perfetto_binary", ".pb");
            try (BufferedReader bufferedReader =
                    new BufferedReader(new FileReader(perfettoMetricProtoFile))) {
                TraceMetrics.Builder builder = TraceMetrics.newBuilder();
                TextFormat.merge(bufferedReader, builder);
                builder.build().writeTo(new FileOutputStream(perfettoBinaryFile));
            } catch (ParseException e) {
                CLog.e("Failed to merge the perfetto metric file." + e.getMessage());
            } catch (IOException ioe) {
                CLog.e(
                        "IOException happened when reading the perfetto metric file."
                                + ioe.getMessage());
            } finally {
                perfettoMetricProtoFile.delete();
                perfettoMetricProtoFile = perfettoBinaryFile;
            }
            return perfettoMetricProtoFile;
        }

        if (isCompressed) {
            perfettoMetricProtoFile = compressFile(perfettoMetricProtoFile);
        }
        return perfettoMetricProtoFile;
    }

    /** Create a zip file with perfetto metric proto file */
    private File compressFile(File decompressedFile) throws IOException {
        File compressedFile = FileUtil.createTempFile("compressed_temp", ".zip");
        try {
            ZipUtil.createZip(decompressedFile, compressedFile);
        } catch (IOException ioe) {
            CLog.e("Unable to gzip the file.");
        } finally {
            decompressedFile.delete();
        }
        return compressedFile;
    }

    @After
    public void teardown() {
        if (perfettoMetricProtoFile != null) {
            perfettoMetricProtoFile.delete();
        }
    }

    /** Assert that metrics contain a key and a corresponding value. */
    private void assertMetricsContain(
            Map<String, Metric.Builder> metrics, String key, Object value) {
        assertTrue(
                String.format(
                        "Metric with key containing %s and value %s was expected but not found.",
                        key, value),
                metrics.entrySet()
                        .stream()
                        .anyMatch(
                                e ->
                                        e.getKey().contains(key)
                                                && String.valueOf(value)
                                                        .equals(
                                                                e.getValue()
                                                                        .build()
                                                                        .getMeasurements()
                                                                        .getSingleString())));
    }
}
