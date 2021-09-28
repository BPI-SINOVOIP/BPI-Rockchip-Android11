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

import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.TestDescription;

import com.google.common.collect.ImmutableList;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;
import java.util.Map;

/** Unit tests for {@link AggregatePostProcessor} */
@RunWith(JUnit4.class)
public class AggregatePostProcessorTest {

    private static final String TEST_CLASS = "test.class";
    private static final String TEST_NAME = "test.name";

    private static final Integer TEST_ITERATIONS = 3;

    // Upload key suffixes for each aggregate metric
    private static final String STATS_KEY_MIN = "min";
    private static final String STATS_KEY_MAX = "max";
    private static final String STATS_KEY_MEAN = "mean";
    private static final String STATS_KEY_VAR = "var";
    private static final String STATS_KEY_STDEV = "stdev";
    private static final String STATS_KEY_MEDIAN = "median";
    private static final String STATS_KEY_TOTAL = "total";
    // Separator for final upload
    private static final String STATS_KEY_SEPARATOR = "-";

    private static final TestDescription TEST_1 = new TestDescription("pgk", "test1");
    private static final TestDescription TEST_2 = new TestDescription("pkg", "test2");

    private AggregatePostProcessor mProcessor;

    @Before
    public void setUp() {
        mProcessor = new AggregatePostProcessor();
    }

    /** Test corrrect aggregation of singular double metrics. */
    @Test
    public void testSingularDoubleMetric() {
        // Singular double metrics test: Sample results and expected aggregate metric values.
        final String singularDoubleKey = "singular_double";
        final ImmutableList<String> singularDoubleMetrics = ImmutableList.of("1.1", "2.9", "2");
        HashMap<String, String> singularDoubleStats = new HashMap<String, String>();
        singularDoubleStats.put(STATS_KEY_MIN, "1.10");
        singularDoubleStats.put(STATS_KEY_MAX, "2.90");
        singularDoubleStats.put(STATS_KEY_MEAN, "2.00");
        singularDoubleStats.put(STATS_KEY_VAR, "0.54");
        singularDoubleStats.put(STATS_KEY_STDEV, "0.73");
        singularDoubleStats.put(STATS_KEY_MEDIAN, "2.00");
        singularDoubleStats.put(STATS_KEY_TOTAL, "6.00");

        // Construct ListMultimap of multiple iterations of test metrics.
        // Stores processed metrics which is overwitten with every test; this is consistent with
        // the current reporting behavior. We only test the correctness on the final metrics values.
        Map<String, Metric.Builder> processedMetrics = new HashMap<String, Metric.Builder>();
        // Simulate multiple iterations of tests and the per-test post-processing that follows.
        for (Integer i = 0; i < TEST_ITERATIONS; i++) {
            HashMap<String, Metric> testMetrics = new HashMap<String, Metric>();
            Metric.Builder metricBuilder = Metric.newBuilder();
            metricBuilder.getMeasurementsBuilder().setSingleString(singularDoubleMetrics.get(i));
            Metric currentTestMetric = metricBuilder.build();
            testMetrics.put(singularDoubleKey, currentTestMetric);
            processedMetrics =
                    mProcessor.processTestMetricsAndLogs(TEST_1, testMetrics, new HashMap<>());
        }

        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MIN)));
        Assert.assertEquals(
                singularDoubleStats.get(STATS_KEY_MIN),
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MIN))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MAX)));
        Assert.assertEquals(
                singularDoubleStats.get(STATS_KEY_MAX),
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MAX))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MEAN)));
        Assert.assertEquals(
                singularDoubleStats.get(STATS_KEY_MEAN),
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MEAN))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_VAR)));
        Assert.assertEquals(
                singularDoubleStats.get(STATS_KEY_VAR),
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_VAR))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_STDEV)));
        Assert.assertEquals(
                singularDoubleStats.get(STATS_KEY_STDEV),
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_STDEV))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MEDIAN)));
        Assert.assertEquals(
                singularDoubleStats.get(STATS_KEY_MEDIAN),
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_MEDIAN))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_TOTAL)));
        Assert.assertEquals(
                singularDoubleStats.get(STATS_KEY_TOTAL),
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singularDoubleKey, STATS_KEY_TOTAL))
                        .build()
                        .getMeasurements()
                        .getSingleString());
    }

    /** Test correct aggregation of list double metrics. */
    @Test
    public void testListDoubleMetric() {
        // List double metrics test: Sample results and expected aggregate metric values.
        final String listDoubleKey = "list_double";
        final ImmutableList<String> listDoubleMetrics =
                ImmutableList.of("1.1, 2.2", "", "1.5, 2.5, 1.9, 2.9");
        HashMap<String, String> listDoubleStats = new HashMap<String, String>();
        listDoubleStats.put(STATS_KEY_MIN, "1.10");
        listDoubleStats.put(STATS_KEY_MAX, "2.90");
        listDoubleStats.put(STATS_KEY_MEAN, "2.02");
        listDoubleStats.put(STATS_KEY_VAR, "0.36");
        listDoubleStats.put(STATS_KEY_STDEV, "0.60");
        listDoubleStats.put(STATS_KEY_MEDIAN, "2.05");
        listDoubleStats.put(STATS_KEY_TOTAL, "12.10");

        // Stores processed metrics which is overwitten with every test; this is consistent with
        // the current reporting behavior. We only test the correctness on the final metrics values.
        Map<String, Metric.Builder> processedMetrics = new HashMap<String, Metric.Builder>();
        // Simulate multiple iterations of tests and the per-test post-processing that follows.
        for (Integer i = 0; i < TEST_ITERATIONS; i++) {
            HashMap<String, Metric> testMetrics = new HashMap<String, Metric>();
            Metric.Builder metricBuilder = Metric.newBuilder();
            metricBuilder.getMeasurementsBuilder().setSingleString(listDoubleMetrics.get(i));
            Metric currentTestMetric = metricBuilder.build();
            testMetrics.put(listDoubleKey, currentTestMetric);
            processedMetrics =
                    mProcessor.processTestMetricsAndLogs(TEST_1, testMetrics, new HashMap<>());
        }

        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, listDoubleKey, STATS_KEY_MIN)));
        Assert.assertEquals(
                listDoubleStats.get(STATS_KEY_MIN),
                processedMetrics
                        .get(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_MIN)
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_MAX));
        Assert.assertEquals(
                listDoubleStats.get(STATS_KEY_MAX),
                processedMetrics
                        .get(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_MAX)
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_MEAN));
        Assert.assertEquals(
                listDoubleStats.get(STATS_KEY_MEAN),
                processedMetrics
                        .get(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_MEAN)
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_VAR));
        Assert.assertEquals(
                listDoubleStats.get(STATS_KEY_VAR),
                processedMetrics
                        .get(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_VAR)
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_STDEV));
        Assert.assertEquals(
                listDoubleStats.get(STATS_KEY_STDEV),
                processedMetrics
                        .get(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_STDEV)
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_MEDIAN));
        Assert.assertEquals(
                listDoubleStats.get(STATS_KEY_MEDIAN),
                processedMetrics
                        .get(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_MEDIAN)
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_TOTAL));
        Assert.assertEquals(
                listDoubleStats.get(STATS_KEY_TOTAL),
                processedMetrics
                        .get(listDoubleKey + STATS_KEY_SEPARATOR + STATS_KEY_TOTAL)
                        .build()
                        .getMeasurements()
                        .getSingleString());
    }


    /** Test that non-numeric metric does not show up in the reported results. */
    @Test
    public void testNonNumericMetric() {
        // Non-numeric metrics test: Sample results; should not show up in aggregate metrics
        final String nonNumericKey = "non_numeric";
        final ImmutableList<String> nonNumericMetrics = ImmutableList.of("1", "success", "failed");

        // Stores processed metrics which is overwitten with every test; this is consistent with
        // the current reporting behavior. We only test the correctness on the final metrics values.
        Map<String, Metric.Builder> processedMetrics = new HashMap<String, Metric.Builder>();
        // Simulate multiple iterations of tests and the per-test post-processing that follows.
        for (Integer i = 0; i < TEST_ITERATIONS; i++) {
            HashMap<String, Metric> testMetrics = new HashMap<String, Metric>();
            Metric.Builder metricBuilder = Metric.newBuilder();
            metricBuilder.getMeasurementsBuilder().setSingleString(nonNumericMetrics.get(i));
            Metric currentTestMetric = metricBuilder.build();
            testMetrics.put(nonNumericKey, currentTestMetric);
            processedMetrics =
                    mProcessor.processTestMetricsAndLogs(TEST_1, testMetrics, new HashMap<>());
        }

        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, nonNumericKey, STATS_KEY_MIN)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, nonNumericKey, STATS_KEY_MAX)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, nonNumericKey, STATS_KEY_MEAN)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, nonNumericKey, STATS_KEY_VAR)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, nonNumericKey, STATS_KEY_STDEV)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, nonNumericKey, STATS_KEY_MEDIAN)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, nonNumericKey, STATS_KEY_TOTAL)));
    }

    /** Test empty result. */
    @Test
    public void testEmptyResult() {
        final String emptyResultKey = "empty_result";

        // Stores processed metrics which is overwitten with every test; this is consistent with
        // the current reporting behavior. We only test the correctness on the final metrics values.
        Map<String, Metric.Builder> processedMetrics = new HashMap<String, Metric.Builder>();
        // Simulate multiple iterations of tests and the per-test post-processing that follows.
        for (Integer i = 0; i < TEST_ITERATIONS; i++) {
            HashMap<String, Metric> testMetrics = new HashMap<String, Metric>();
            Metric.Builder metricBuilder = Metric.newBuilder();
            metricBuilder.getMeasurementsBuilder().setSingleString("");
            Metric currentTestMetric = metricBuilder.build();
            testMetrics.put(emptyResultKey, currentTestMetric);
            processedMetrics =
                    mProcessor.processTestMetricsAndLogs(TEST_1, testMetrics, new HashMap<>());
        }

        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, emptyResultKey, STATS_KEY_MIN)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, emptyResultKey, STATS_KEY_MAX)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, emptyResultKey, STATS_KEY_MEAN)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, emptyResultKey, STATS_KEY_VAR)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, emptyResultKey, STATS_KEY_STDEV)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, emptyResultKey, STATS_KEY_MEDIAN)));
        Assert.assertFalse(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, emptyResultKey, STATS_KEY_TOTAL)));
    }

    /** Test single run. */
    @Test
    public void testSingleRun() {
        final String singleRunKey = "single_run";
        final String singleRunVal = "1.00";
        final String zeroStr = "0.00";

        // Stores processed metrics which is overwitten with every test; this is consistent with
        // the current reporting behavior. We only test the correctness on the final metrics values.
        // Simulate a single iteration of test and the per-test post-processing that follows.
        HashMap<String, Metric> testMetrics = new HashMap<String, Metric>();
        Metric.Builder metricBuilder = Metric.newBuilder();
        metricBuilder.getMeasurementsBuilder().setSingleString(singleRunVal);
        Metric currentTestMetric = metricBuilder.build();
        testMetrics.put(singleRunKey, currentTestMetric);
        Map<String, Metric.Builder> processedMetrics =
                mProcessor.processTestMetricsAndLogs(TEST_1, testMetrics, new HashMap<>());

        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MIN)));
        Assert.assertEquals(
                singleRunVal,
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MIN))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MAX)));
        Assert.assertEquals(
                singleRunVal,
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MAX))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MEAN)));
        Assert.assertEquals(
                singleRunVal,
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MEAN))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_VAR)));
        Assert.assertEquals(
                zeroStr,
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_VAR))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_STDEV)));
        Assert.assertEquals(
                zeroStr,
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_STDEV))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MEDIAN)));
        Assert.assertEquals(
                singleRunVal,
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_MEDIAN))
                        .build()
                        .getMeasurements()
                        .getSingleString());
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_TOTAL)));
        Assert.assertEquals(
                singleRunVal,
                processedMetrics
                        .get(String.join(STATS_KEY_SEPARATOR, singleRunKey, STATS_KEY_TOTAL))
                        .build()
                        .getMeasurements()
                        .getSingleString());
    }


    /**
     *  Test successful processed run metrics when there are more than one comma
     *  separated value.
     */
    @Test
    public void testSuccessfullProcessRunMetrics() {
        final String key = "single_run";
        final String value = "1.00, 2.00";

        HashMap<String, Metric> runMetrics = new HashMap<String, Metric>();
        Metric.Builder metricBuilder = Metric.newBuilder();
        metricBuilder.getMeasurementsBuilder().setSingleString(value);
        Metric currentRunMetric = metricBuilder.build();
        runMetrics.put(key, currentRunMetric);
        Map<String, Metric.Builder> processedMetrics =
                mProcessor.processRunMetricsAndLogs(runMetrics, new HashMap<>());

        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MIN)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MAX)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MEAN)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_VAR)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_STDEV)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MEDIAN)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_TOTAL)));
    }

    /**
     * Test collecting processed run metrics when there is one double value associated with the key.
     */
    @Test
    public void testSingleValueProcessRunMetrics() {
        final String key = "single_run";
        final String value = "1.00";

        HashMap<String, Metric> runMetrics = new HashMap<String, Metric>();
        Metric.Builder metricBuilder = Metric.newBuilder();
        metricBuilder.getMeasurementsBuilder().setSingleString(value);
        Metric currentRunMetric = metricBuilder.build();
        runMetrics.put(key, currentRunMetric);
        Map<String, Metric.Builder> processedMetrics =
                mProcessor.processRunMetricsAndLogs(runMetrics, new HashMap<>());

        Assert.assertTrue(
                processedMetrics.containsKey(String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MIN)));
        Assert.assertTrue(
                processedMetrics.containsKey(String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MAX)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MEAN)));
        Assert.assertTrue(
                processedMetrics.containsKey(String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_VAR)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_STDEV)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_MEDIAN)));
        Assert.assertTrue(
                processedMetrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, key, STATS_KEY_TOTAL)));
    }

    /**
     *  Test non double run metrics values return empty processed metrics.
     */
    @Test
    public void testNoDoubleProcessRunMetrics() {
        final String key = "single_run";
        final String value = "1.00, abc";

        HashMap<String, Metric> runMetrics = new HashMap<String, Metric>();
        Metric.Builder metricBuilder = Metric.newBuilder();
        metricBuilder.getMeasurementsBuilder().setSingleString(value);
        Metric currentRunMetric = metricBuilder.build();
        runMetrics.put(key, currentRunMetric);
        Map<String, Metric.Builder> processedMetrics =
                mProcessor.processRunMetricsAndLogs(runMetrics, new HashMap<>());

        Assert.assertEquals(0, processedMetrics.size());
    }

    /** Test that metrics are correctly aggregated for different tests. */
    @Test
    public void testDifferentTests() {
        // We only test for median here as the stats calculation has been tested separately.

        // Metrics and aggregation result for the first test.
        final String test1Key = "singular_double";
        final ImmutableList<String> test1Metrics = ImmutableList.of("1.1", "2.9", "2");
        HashMap<String, String> test1Stats = new HashMap<String, String>();
        test1Stats.put(STATS_KEY_MEDIAN, "2.00");

        // List double metrics test: Sample results and expected aggregate metric values.
        final String test2Key = "list_double";
        final ImmutableList<String> test2Metrics =
                ImmutableList.of("1.1, 2.2", "", "1.5, 2.5, 1.9, 2.9");
        HashMap<String, String> test2Stats = new HashMap<String, String>();
        test2Stats.put(STATS_KEY_MEDIAN, "2.05");

        // Stores processed metrics which is overwitten with every test; this is consistent with
        // the current reporting behavior. We only test the correctness on the final metrics values.
        Map<String, Metric.Builder> processedTest1Metrics = new HashMap<String, Metric.Builder>();
        Map<String, Metric.Builder> processedTest2Metrics = new HashMap<String, Metric.Builder>();
        // Simulate multiple iterations of tests and the per-test post-processing that follows.
        for (Integer i = 0; i < TEST_ITERATIONS; i++) {
            HashMap<String, Metric> currentTest1Metrics = new HashMap<String, Metric>();
            Metric.Builder metricBuilder1 = Metric.newBuilder();
            metricBuilder1.getMeasurementsBuilder().setSingleString(test1Metrics.get(i));
            Metric currentTest1Metric = metricBuilder1.build();
            currentTest1Metrics.put(test1Key, currentTest1Metric);
            processedTest1Metrics =
                    mProcessor.processTestMetricsAndLogs(
                            TEST_1, currentTest1Metrics, new HashMap<>());

            HashMap<String, Metric> currentTest2Metrics = new HashMap<String, Metric>();
            Metric.Builder metricBuilder2 = Metric.newBuilder();
            metricBuilder2.getMeasurementsBuilder().setSingleString(test2Metrics.get(i));
            Metric currentTest2Metric = metricBuilder2.build();
            currentTest2Metrics.put(test2Key, currentTest2Metric);
            processedTest2Metrics =
                    mProcessor.processTestMetricsAndLogs(
                            TEST_2, currentTest2Metrics, new HashMap<>());
        }

        Assert.assertTrue(
                processedTest1Metrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, test1Key, STATS_KEY_MEDIAN)));
        Assert.assertEquals(
                test1Stats.get(STATS_KEY_MEDIAN),
                processedTest1Metrics
                        .get(String.join(STATS_KEY_SEPARATOR, test1Key, STATS_KEY_MEDIAN))
                        .build()
                        .getMeasurements()
                        .getSingleString());

        Assert.assertTrue(
                processedTest2Metrics.containsKey(
                        String.join(STATS_KEY_SEPARATOR, test2Key, STATS_KEY_MEDIAN)));
        Assert.assertEquals(
                test2Stats.get(STATS_KEY_MEDIAN),
                processedTest2Metrics
                        .get(String.join(STATS_KEY_SEPARATOR, test2Key, STATS_KEY_MEDIAN))
                        .build()
                        .getMeasurements()
                        .getSingleString());
    }
}
