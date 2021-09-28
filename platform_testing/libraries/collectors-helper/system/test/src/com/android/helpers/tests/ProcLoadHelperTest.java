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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.ProcLoadHelper;
import static com.android.helpers.ProcLoadHelper.LAST_MINUTE_LOAD_METRIC_KEY;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

/**
 * Android Unit tests for {@link ProcLoadHelperTest}.
 *
 * To run:
 * atest CollectorsHelperTest:com.android.helpers.tests.ProcLoadHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class ProcLoadHelperTest {

    private ProcLoadHelper mLoadHelper;

    @Before
    public void setUp() {
        mLoadHelper = new ProcLoadHelper();
    }

    /** Test to verify succesfull threshold flow. **/
    @Test
    public void testThresholdSuccess() {
        // By setting the threshold higher(i.e 100) the current load should be always
        // lesser than the the 100 which should proceed with add the current
        // load avg during the last minute in the metrics.
        mLoadHelper.setProcLoadThreshold(100);
        mLoadHelper.setProcLoadWaitTimeInMs(1100L);
        mLoadHelper.setProcLoadIntervalInMs(100L);
        assertTrue(mLoadHelper.startCollecting());
        Map<String, Double> procLoadMetric = mLoadHelper.getMetrics();
        assertTrue(procLoadMetric.containsKey(LAST_MINUTE_LOAD_METRIC_KEY));
        assertTrue(procLoadMetric.get(LAST_MINUTE_LOAD_METRIC_KEY) > 0);
    }

    /** Test to verify threshold not met workflow. */
    @Test
    public void testMetricAfterTimeout() {
        // By setting the threshold lower(i.e 0) the cpu utilization will never be met
        // which should result in timeout.
        mLoadHelper.setProcLoadThreshold(0);
        mLoadHelper.setProcLoadWaitTimeInMs(4000L);
        mLoadHelper.setProcLoadIntervalInMs(200L);
        assertFalse(mLoadHelper.startCollecting());
        Map<String, Double> procLoadMetric = mLoadHelper.getMetrics();
        assertTrue(procLoadMetric.get(LAST_MINUTE_LOAD_METRIC_KEY) > 0);
    }

    /** Test to verify the timeout if the threshold did not meet. */
    @Test
    public void testWaitForTimeout() {
        // By setting the threshold lower(i.e 0) the cpu utilization will never be met
        // which should result in timeout.
        mLoadHelper.setProcLoadThreshold(0);
        mLoadHelper.setProcLoadWaitTimeInMs(4000L);
        mLoadHelper.setProcLoadIntervalInMs(200L);
        long startTime = System.currentTimeMillis();
        assertFalse(mLoadHelper.startCollecting());
        long waitTime = System.currentTimeMillis() - startTime;
        assertTrue((waitTime > 4000L && waitTime < 6000L));
        Map<String, Double> procLoadMetric = mLoadHelper.getMetrics();
        assertTrue(procLoadMetric.get(LAST_MINUTE_LOAD_METRIC_KEY) > 0);
    }

    /** Test to verify metric exist and exits with the default threshold, timeout and interval */
    @Test
    public void testDefaults() {
        long startTime = System.currentTimeMillis();
        assertFalse(mLoadHelper.startCollecting());
        Map<String, Double> procLoadMetric = mLoadHelper.getMetrics();
        assertTrue(procLoadMetric.containsKey(LAST_MINUTE_LOAD_METRIC_KEY));
        assertTrue(procLoadMetric.get(LAST_MINUTE_LOAD_METRIC_KEY) > 0);
    }
}
