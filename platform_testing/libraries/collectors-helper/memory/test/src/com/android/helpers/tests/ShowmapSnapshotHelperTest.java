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

package com.android.helpers.tests;

import static com.android.helpers.MetricUtility.constructKey;
import static org.junit.Assert.fail;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.runner.AndroidJUnit4;
import com.android.helpers.ShowmapSnapshotHelper;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;


/**
 * Android Unit tests for {@link ShowmapSnapshotHelper}.
 *
 * To run:
 * atest CollectorsHelperTest:com.android.helpers.tests.ShowmapSnapshotHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class ShowmapSnapshotHelperTest {
    private static final String TAG = ShowmapSnapshotHelperTest.class.getSimpleName();

    // Valid output file
    private static final String VALID_OUTPUT_DIR = "/sdcard/test_results";
    // Invalid output file (no permissions to write)
    private static final String INVALID_OUTPUT_DIR = "/data/local/tmp";
    // Valid metric index string.
    private static final String METRIC_INDEX_STR = "rss:1,pss:2";
    // Invalid metric index string. Reverse order.
    private static final String METRIC_INVALID_INDEX_STR = "1:pss";
    // Empty metric index string.
    private static final String METRIC_EMPTY_INDEX_STR = "";

    // Lists of process names
    private static final String[] EMPTY_PROCESS_LIST = {};
    private static final String[] ONE_PROCESS_LIST = {
            "com.android.systemui"
    };
    private static final String[] TWO_PROCESS_LIST = {
            "com.android.systemui", "system_server"
    };
    private static final String[] NO_PROCESS_LIST = {
            null
    };

    private ShowmapSnapshotHelper mShowmapSnapshotHelper;

    @Before
    public void setUp() {
        mShowmapSnapshotHelper = new ShowmapSnapshotHelper();
    }

    /**
     * Test start collecting returns false if the helper has not been properly set up.
     */
    @Test
    public void testSetUpNotCalled() {
        assertFalse(mShowmapSnapshotHelper.startCollecting());
    }

    /**
     * Test invalid options for drop cache flag.
     */
    @Test
    public void testInvalidDropCacheOptions() {
        assertFalse(mShowmapSnapshotHelper.setDropCacheOption(-1));
        assertFalse(mShowmapSnapshotHelper.setDropCacheOption(0));
        assertFalse(mShowmapSnapshotHelper.setDropCacheOption(4));
    }

    /**
     * Test invalid options for drop cache flag.
     */
    @Test
    public void testValidDropCacheOptions() {
        assertTrue(mShowmapSnapshotHelper.setDropCacheOption(1));
        assertTrue(mShowmapSnapshotHelper.setDropCacheOption(2));
        assertTrue(mShowmapSnapshotHelper.setDropCacheOption(3));
    }

    /**
     * Test no metrics are sampled if process name is empty.
     */
    @Test
    public void testEmptyProcessName() {
        mShowmapSnapshotHelper.setUp(VALID_OUTPUT_DIR, EMPTY_PROCESS_LIST);
        Map<String, String> metrics = mShowmapSnapshotHelper.getMetrics();
        assertTrue(metrics.isEmpty());
    }

    /**
     * Test sampling on a valid and running process.
     */
    @Test
    public void testValidFile() {
        mShowmapSnapshotHelper.setUp(VALID_OUTPUT_DIR, ONE_PROCESS_LIST);
        assertTrue(mShowmapSnapshotHelper.startCollecting());
    }

    /**
     * Test sampling on using an invalid output file.
     */
    @Test
    public void testInvalidFile() {
        mShowmapSnapshotHelper.setUp(INVALID_OUTPUT_DIR, ONE_PROCESS_LIST);
        assertFalse(mShowmapSnapshotHelper.startCollecting());
    }

    /**
     * Test getting metrics from one process.
     */
    @Test
    public void testGetMetrics_OneProcess() {
        testProcessList(METRIC_INDEX_STR, ONE_PROCESS_LIST);
    }

    /**
     * Test getting metrics from multiple processes process.
     */
    @Test
    public void testGetMetrics_MultipleProcesses() {
        testProcessList(METRIC_INDEX_STR, TWO_PROCESS_LIST);
    }

    /**
     * Test all process flag return more than 2 processes metrics at least.
     */
    @Test
    public void testGetMetrics_AllProcess() {
        mShowmapSnapshotHelper.setUp(VALID_OUTPUT_DIR, NO_PROCESS_LIST);
        mShowmapSnapshotHelper.setMetricNameIndex(METRIC_INDEX_STR);
        mShowmapSnapshotHelper.setAllProcesses();
        assertTrue(mShowmapSnapshotHelper.startCollecting());
        Map<String, String> metrics = mShowmapSnapshotHelper.getMetrics();
        assertTrue(metrics.size() > 2);
        assertTrue(metrics.containsKey(ShowmapSnapshotHelper.OUTPUT_FILE_PATH_KEY));

    }

    @Test
    public void testGetMetrics_Invalid_Metric_Pattern() {
        mShowmapSnapshotHelper.setUp(VALID_OUTPUT_DIR, NO_PROCESS_LIST);
        try {
            mShowmapSnapshotHelper.setMetricNameIndex(METRIC_INVALID_INDEX_STR);
            fail("Should have thrown an exception due to invalid pattern.");
        } catch (Exception e) {
            // No-op during the exception
        }

        mShowmapSnapshotHelper.setAllProcesses();
        assertTrue(mShowmapSnapshotHelper.startCollecting());
        Map<String, String> metrics = mShowmapSnapshotHelper.getMetrics();
        // process count, process with child process count and path to snapshot file in the output
        // by default.
        assertTrue(verifyDefaultMetrics(metrics));
    }

    @Test
    public void testGetMetrics_Empty_Metric_Pattern() {
        mShowmapSnapshotHelper.setUp(VALID_OUTPUT_DIR, NO_PROCESS_LIST);
        mShowmapSnapshotHelper.setMetricNameIndex(METRIC_EMPTY_INDEX_STR);

        mShowmapSnapshotHelper.setAllProcesses();
        assertTrue(mShowmapSnapshotHelper.startCollecting());
        Map<String, String> metrics = mShowmapSnapshotHelper.getMetrics();
        // process count, process with child process count and path to snapshot file in the output
        // by default.
        assertTrue(verifyDefaultMetrics(metrics));
    }

    @Test
    public void testGetMetrics_verify_child_processes_metrics() {
        mShowmapSnapshotHelper.setUp(VALID_OUTPUT_DIR, NO_PROCESS_LIST);
        mShowmapSnapshotHelper.setMetricNameIndex(METRIC_EMPTY_INDEX_STR);

        mShowmapSnapshotHelper.setAllProcesses();
        assertTrue(mShowmapSnapshotHelper.startCollecting());
        Map<String, String> metrics = mShowmapSnapshotHelper.getMetrics();

        assertTrue(metrics.size() != 0);

        // process count, process with child process count and path to snapshot file in the output
        // by default.
        assertTrue(metrics.containsKey(ShowmapSnapshotHelper.PROCESS_WITH_CHILD_PROCESS_COUNT));

        Set<String> parentWithChildProcessSet = metrics.keySet()
                .stream()
                .filter(s -> s.startsWith(ShowmapSnapshotHelper.CHILD_PROCESS_COUNT_PREFIX))
                .collect(Collectors.toSet());

        // At least one process (i.e init) will have child process
        assertTrue(parentWithChildProcessSet.size() > 0);
        assertTrue(metrics.containsKey(ShowmapSnapshotHelper.CHILD_PROCESS_COUNT_PREFIX + "_init"));
    }

    private boolean verifyDefaultMetrics(Map<String, String> metrics) {
        if(metrics.size() == 0) {
            return false;
        }
        for (String key : metrics.keySet()) {
            if (!(key.equals(ShowmapSnapshotHelper.PROCESS_COUNT)
                    || key.equals(ShowmapSnapshotHelper.OUTPUT_FILE_PATH_KEY)
                    || key.equals(ShowmapSnapshotHelper.PROCESS_WITH_CHILD_PROCESS_COUNT)
                    || key.startsWith(ShowmapSnapshotHelper.CHILD_PROCESS_COUNT_PREFIX))) {
                return false;
            }
        }
        return true;
    }

    private void testProcessList(String metricIndexStr, String... processNames) {
        mShowmapSnapshotHelper.setUp(VALID_OUTPUT_DIR, processNames);
        mShowmapSnapshotHelper.setMetricNameIndex(metricIndexStr);
        assertTrue(mShowmapSnapshotHelper.startCollecting());
        Map<String, String> metrics = mShowmapSnapshotHelper.getMetrics();
        assertFalse(metrics.isEmpty());
        for (String processName : processNames) {
            assertTrue(
                    metrics.containsKey(constructKey(String.format(
                            ShowmapSnapshotHelper.OUTPUT_METRIC_PATTERN, "rss"), processName)));
            assertTrue(
                    metrics.containsKey(constructKey(String.format(
                            ShowmapSnapshotHelper.OUTPUT_METRIC_PATTERN, "pss"), processName)));
        }
        assertTrue(metrics.containsKey(ShowmapSnapshotHelper.OUTPUT_FILE_PATH_KEY));
    }
}
