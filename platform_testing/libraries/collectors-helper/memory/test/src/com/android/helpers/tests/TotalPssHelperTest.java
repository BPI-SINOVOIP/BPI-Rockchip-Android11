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

package com.android.helpers.tests;

import static com.android.helpers.MetricUtility.constructKey;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.TotalPssHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

/**
 * Android Unit tests for {@link TotalPssHelper}.
 *
 * To run:
 * atest CollectorsHelperTest:com.android.helpers.tests.TotalPssHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class TotalPssHelperTest {

    // Process name used for testing
    private static final String TEST_PROCESS_NAME = "com.android.systemui";
    // Second process name used for testing
    private static final String TEST_PROCESS_NAME_2 = "com.google.android.apps.nexuslauncher";
    // Second process name used for testing
    private static final String INVALID_PROCESS_NAME = "abc";
    // Pss prefix in Key.
    private static final String PSS_METRIC_PREFIX = "am_totalpss_bytes";

    private TotalPssHelper mTotalPssHelper;

    @Before
    public void setUp() {
         mTotalPssHelper = new TotalPssHelper();
    }

    /** Test no metrics are sampled if process name is empty. */
    @Test
    public void testEmptyProcessName() {
        mTotalPssHelper.setUp("");
        Map<String, Long> pssMetrics = mTotalPssHelper.getMetrics();
        assertTrue(pssMetrics.isEmpty());
    }

    /** Test no metrics are sampled if process names is null */
    @Test
    public void testNullProcessName() {
        mTotalPssHelper.setUp(null);
        Map<String, Long> pssMetrics = mTotalPssHelper.getMetrics();
        assertTrue(pssMetrics.isEmpty());
    }

    /** Test getting metrics for single process. */
    @Test
    public void testGetMetrics_OneProcess() {
        mTotalPssHelper.setUp(TEST_PROCESS_NAME);
        Map<String, Long> pssMetrics = mTotalPssHelper.getMetrics();
        assertFalse(pssMetrics.isEmpty());
        assertTrue(pssMetrics.containsKey(constructKey(PSS_METRIC_PREFIX, TEST_PROCESS_NAME)));
        assertTrue(pssMetrics.get(constructKey(PSS_METRIC_PREFIX, TEST_PROCESS_NAME)) > 0);
    }

    /** Test getting metrics for multiple process. */
    @Test
    public void testGetMetrics_MultipleProcesses() {
        mTotalPssHelper.setUp(TEST_PROCESS_NAME, TEST_PROCESS_NAME_2);
        Map<String, Long> pssMetrics = mTotalPssHelper.getMetrics();
        assertFalse(pssMetrics.isEmpty());
        assertTrue(pssMetrics.containsKey(constructKey(PSS_METRIC_PREFIX, TEST_PROCESS_NAME)));
        assertTrue(pssMetrics.containsKey(constructKey(PSS_METRIC_PREFIX, TEST_PROCESS_NAME_2)));
        assertTrue(pssMetrics.get(constructKey(PSS_METRIC_PREFIX, TEST_PROCESS_NAME)) > 0);
        assertTrue(pssMetrics.get(constructKey(PSS_METRIC_PREFIX, TEST_PROCESS_NAME_2)) > 0);
    }

    /** Test pss metric is 0 for invalid process name. */
    @Test
    public void testGetMetrics_InvalidProcess() {
        mTotalPssHelper.setUp(INVALID_PROCESS_NAME);
        Map<String, Long> pssMetrics = mTotalPssHelper.getMetrics();
        assertTrue(pssMetrics.containsKey(constructKey(PSS_METRIC_PREFIX, INVALID_PROCESS_NAME)));
        assertTrue(pssMetrics.get(constructKey(PSS_METRIC_PREFIX, INVALID_PROCESS_NAME)) == 0);
    }
}
