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
 * limitations under the License
 */

package com.android.helpers.tests;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.DumpsysMeminfoHelper;
import com.android.helpers.MetricUtility;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

/**
 * Android unit test for {@link DumpsysMeminfoHelper}
 *
 * <p>To run: atest CollectorsHelperTest:com.android.helpers.tests.DumpsysMeminfoHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class DumpsysMeminfoHelperTest {

    // Process name used for testing
    private static final String TEST_PROCESS_NAME = "com.android.systemui";
    // Second process name used for testing
    private static final String TEST_PROCESS_NAME_2 = "system_server";
    // A process that does not exists
    private static final String PROCESS_NOT_FOUND = "process_not_found";

    private static final String[] CATEGORIES = {"native", "dalvik"};

    private static final String[] METRICS = {
        "pss_total", "shared_dirty", "private_dirty", "heap_size", "heap_alloc"
    };

    private static final String UNIT = "kb";

    private static final String METRIC_SOURCE = "dumpsys";

    private DumpsysMeminfoHelper mDumpsysMeminfoHelper;

    @Before
    public void setUp() {
        mDumpsysMeminfoHelper = new DumpsysMeminfoHelper();
    }

    @Test
    public void testCollectMeminfo_noProcess() {
        mDumpsysMeminfoHelper.startCollecting();
        Map<String, Long> results = mDumpsysMeminfoHelper.getMetrics();
        mDumpsysMeminfoHelper.stopCollecting();
        assertTrue(results.isEmpty());
    }

    @Test
    public void testCollectMeminfo_nullProcess() {
        mDumpsysMeminfoHelper.setUp(null);
        mDumpsysMeminfoHelper.startCollecting();
        Map<String, Long> results = mDumpsysMeminfoHelper.getMetrics();
        assertTrue(results.isEmpty());
    }

    @Test
    public void testCollectMeminfo_wrongProcesses() {
        mDumpsysMeminfoHelper.setUp(PROCESS_NOT_FOUND, null, "");
        mDumpsysMeminfoHelper.startCollecting();
        Map<String, Long> results = mDumpsysMeminfoHelper.getMetrics();
        assertTrue(results.isEmpty());
    }

    @Test
    public void testCollectMeminfo_oneProcess() {
        mDumpsysMeminfoHelper.setUp(TEST_PROCESS_NAME);
        mDumpsysMeminfoHelper.startCollecting();
        Map<String, Long> results = mDumpsysMeminfoHelper.getMetrics();
        mDumpsysMeminfoHelper.stopCollecting();
        assertFalse(results.isEmpty());
        verifyKeysForProcess(results, TEST_PROCESS_NAME);
    }

    @Test
    public void testCollectMeminfo_multipleProcesses() {
        mDumpsysMeminfoHelper.setUp(TEST_PROCESS_NAME, TEST_PROCESS_NAME_2);
        mDumpsysMeminfoHelper.startCollecting();
        Map<String, Long> results = mDumpsysMeminfoHelper.getMetrics();
        mDumpsysMeminfoHelper.stopCollecting();
        assertFalse(results.isEmpty());
        verifyKeysForProcess(results, TEST_PROCESS_NAME);
        verifyKeysForProcess(results, TEST_PROCESS_NAME_2);
    }

    private void verifyKeysForProcess(Map<String, Long> results, String processName) {
        for (String category : CATEGORIES) {
            for (String metric : METRICS) {
                assertTrue(
                        results.containsKey(
                                MetricUtility.constructKey(
                                        METRIC_SOURCE, category, metric, UNIT, processName)));
            }
        }
        assertTrue(
                results.containsKey(
                        MetricUtility.constructKey(
                                METRIC_SOURCE, "total", "pss_total", UNIT, processName)));
    }
}
