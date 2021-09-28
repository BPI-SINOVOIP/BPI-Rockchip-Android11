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

import android.support.test.uiautomator.UiDevice;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.SimpleperfHelper;

import java.io.IOException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

/**
 * Android Unit tests for {@link SimpleperfHelper}.
 *
 * <p>atest CollectorsHelperTest:com.android.helpers.tests.SimpleperfHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class SimpleperfHelperTest {

    private static final String REMOVE_CMD = "rm %s";
    private static final String FILE_SIZE_IN_BYTES = "wc -c %s";

    private SimpleperfHelper simpleperfHelper;

    @Before
    public void setUp() {
        simpleperfHelper = new SimpleperfHelper();
    }

    @After
    public void teardown() throws IOException {
        simpleperfHelper.stopCollecting("data/local/tmp/perf.data");
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        uiDevice.executeShellCommand(String.format(REMOVE_CMD, "/data/local/tmp/perf.data"));
    }

    /** Test simpleperf collection starts collecting properly. */
    @Test
    public void testSimpleperfStartSuccess() throws Exception {
        assertTrue(simpleperfHelper.startCollecting());
    }

    /** Test if the path name is prefixed with /. */
    @Test
    public void testSimpleperfValidOutputPath() throws Exception {
        assertTrue(simpleperfHelper.startCollecting());
        assertTrue(simpleperfHelper.stopCollecting("data/local/tmp/perf.data"));
    }

    /** Test the invalid output path. */
    @Test
    public void testSimpleperfInvalidOutputPath() throws Exception {
        assertTrue(simpleperfHelper.startCollecting());
        // Don't have permission to create new folder under /data
        assertFalse(simpleperfHelper.stopCollecting("/data/dummy/xyz/perf.data"));
    }

    /** Test simpleperf collection returns true and output file size greater than zero */
    @Test
    public void testSimpleperfSuccess() throws Exception {
        assertTrue(simpleperfHelper.startCollecting());
        Thread.sleep(1000);
        assertTrue(simpleperfHelper.stopCollecting("/data/local/tmp/perf.data"));
        Thread.sleep(1000);
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        String[] fileStats =
                uiDevice.executeShellCommand(
                                String.format(FILE_SIZE_IN_BYTES, "/data/local/tmp/perf.data"))
                        .split(" ");
        int fileSize = Integer.parseInt(fileStats[0].trim());
        assertTrue(fileSize > 0);
    }
}
