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

import com.android.helpers.FreeMemHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

/**
 * Android Unit tests for {@link FreeMemHelper}.
 *
 * To run:
 * atest CollectorsHelperTest:com.android.helpers.tests.FreeMemHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class FreeMemHelperTest {
    private FreeMemHelper mFreeMemHelper;

    @Before
    public void setUp() {
        mFreeMemHelper = new FreeMemHelper();
    }

    @Test
    public void testGetMetrics() {
        assertTrue(mFreeMemHelper.startCollecting());
        Map<String, Long> freeMemMetrics = mFreeMemHelper.getMetrics();
        assertFalse(freeMemMetrics.isEmpty());
        assertTrue(freeMemMetrics.containsKey(FreeMemHelper.MEM_AVAILABLE_CACHE_PROC_DIRTY));
        assertTrue(freeMemMetrics.containsKey(FreeMemHelper.PROC_MEMINFO_MEM_AVAILABLE));
        assertTrue(freeMemMetrics.containsKey(FreeMemHelper.DUMPSYS_CACHED_PROC_MEMORY));
        assertTrue(freeMemMetrics.get(FreeMemHelper.MEM_AVAILABLE_CACHE_PROC_DIRTY) > 0);
        assertTrue(freeMemMetrics.get(FreeMemHelper.PROC_MEMINFO_MEM_AVAILABLE) > 0);
        assertTrue(freeMemMetrics.get(FreeMemHelper.PROC_MEMINFO_MEM_FREE) > 0);
        assertTrue(freeMemMetrics.get(FreeMemHelper.DUMPSYS_CACHED_PROC_MEMORY) > 0);
    }
}
