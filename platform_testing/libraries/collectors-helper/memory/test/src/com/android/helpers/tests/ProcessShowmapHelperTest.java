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

package com.android.helpers.tests;

import static com.android.helpers.MetricUtility.constructKey;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.ProcessShowmapHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

/**
 * Android Unit tests for {@link ProcessShowmapHelper}.
 *
 * To run:
 * atest CollectorsHelperTest:com.android.helpers.tests.ProcessShowmapHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class ProcessShowmapHelperTest {

    // Process name used for testing
    private static final String TEST_PROCESS_NAME = "com.android.systemui";
    // Second process name used for testing
    private static final String TEST_PROCESS_NAME_2 = "system_server";
    // Pss string in key
    private static final String PSS = "pss";
    // Delta string in keys
    private static final String DELTA = "delta";

    private ProcessShowmapHelper mShowmapHelper;


    @Before
    public void setUp() {
         mShowmapHelper = new ProcessShowmapHelper();
    }

    /**
     * Test start collecting returns false if the helper has not been properly set up.
     */
    @Test
    public void testSetUpNotCalled() {
        assertFalse(mShowmapHelper.startCollecting());
    }

    /** Test no metrics are sampled if process name is empty. */
    @Test
    public void testEmptyProcessName() {
        mShowmapHelper.setUp("");
        Map<String, Long> showmapMetrics = mShowmapHelper.getMetrics();
        assertTrue(showmapMetrics.isEmpty());
    }

    /**
     * Test sampling on a valid and running process.
     */
    @Test
    public void testSamplesMemory() {
        mShowmapHelper.setUp(TEST_PROCESS_NAME);
        assertTrue(mShowmapHelper.startCollecting());
    }

    /** Test getting metrics based off sampled memory. */
    @Test
    public void testGetMetrics_OneProcess() {
        mShowmapHelper.setUp(TEST_PROCESS_NAME);
        assertTrue(mShowmapHelper.startCollecting());
        Map<String, Long> showmapMetrics = mShowmapHelper.getMetrics();
        assertFalse(showmapMetrics.isEmpty());
        assertTrue(showmapMetrics.containsKey(constructKey(TEST_PROCESS_NAME, PSS)));
        assertTrue(showmapMetrics.containsKey(constructKey(TEST_PROCESS_NAME, PSS, DELTA)));
    }

    @Test
    public void testGetMetrics_MultipleProcesses() {
        mShowmapHelper.setUp(TEST_PROCESS_NAME, TEST_PROCESS_NAME_2);
        assertTrue(mShowmapHelper.startCollecting());
        Map<String, Long> showmapMetrics = mShowmapHelper.getMetrics();
        assertFalse(showmapMetrics.isEmpty());
        assertTrue(showmapMetrics.containsKey(constructKey(TEST_PROCESS_NAME, PSS)));
        assertTrue(showmapMetrics.containsKey(constructKey(TEST_PROCESS_NAME, PSS, DELTA)));
        assertTrue(showmapMetrics.containsKey(constructKey(TEST_PROCESS_NAME_2, PSS)));
        assertTrue(showmapMetrics.containsKey(constructKey(TEST_PROCESS_NAME_2, PSS, DELTA)));
    }
}
