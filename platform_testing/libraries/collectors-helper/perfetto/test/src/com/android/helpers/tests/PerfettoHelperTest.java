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

import android.support.test.uiautomator.UiDevice;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.helpers.PerfettoHelper;

import java.io.IOException;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;


/**
 * Android Unit tests for {@link PerfettoHelper}.
 *
 * To run:
 * Have a valid perfetto config under /data/misc/perfetto-traces/valid_config.pb
 * Have a valid text perfetto config under /data/misc/perfetto-traces/valid_text_config.textproto
 * TODO: b/119020380 to keep track of automating the above step.
 * atest CollectorsHelperTest:com.android.helpers.tests.PerfettoHelperTest
 */
@RunWith(AndroidJUnit4.class)
public class PerfettoHelperTest {

    private static final String REMOVE_CMD = "rm %s";
    private static final String FILE_SIZE_IN_BYTES = "wc -c %s";

    private PerfettoHelper perfettoHelper;

    @Before
    public void setUp() {
        perfettoHelper = new PerfettoHelper();
    }

    @After
    public void teardown() throws IOException {
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        uiDevice.executeShellCommand(String.format(REMOVE_CMD, "/data/local/tmp/out.pb"));
    }

    /**
     * Test perfetto collection returns false if the config file name is null.
     */
    @Test
    public void testNullConfigName() throws Exception {
        assertFalse(perfettoHelper.startCollecting(null, false));
    }

    /**
     * Test perfetto collection returns false if the config file name is empty.
     */
    @Test
    public void testEmptyConfigName() throws Exception {
        assertFalse(perfettoHelper.startCollecting("", false));
    }

    /**
     * Test perfetto collection returns false if the perfetto config file does not exist.
     */
    @Test
    public void testNoConfigFile() throws Exception {
        assertFalse(perfettoHelper.startCollecting("no_config.pb", false));
    }

    /**
     * Test perfetto collection returns true if the valid perfetto config file.
     */
    @Test
    public void testPerfettoStartSuccess() throws Exception {
        assertTrue(perfettoHelper.startCollecting("valid_config.pb", false));
    }

    /**
     * Test if the path name is prefixed with /.
     */
    @Test
    public void testPerfettoValidOutputPath() throws Exception {
        assertTrue(perfettoHelper.startCollecting("valid_config.pb", false));
        assertTrue(perfettoHelper.stopCollecting(1000, "data/local/tmp/out.pb"));
    }

    /**
     * Test the invalid output path.
     */
    @Test
    public void testPerfettoInvalidOutputPath() throws Exception {
        assertTrue(perfettoHelper.startCollecting("valid_config.pb", false));
        // Don't have permission to create new folder under /data
        assertFalse(perfettoHelper.stopCollecting(1000, "/data/dummy/xyz/out.pb"));
    }

    /**
     * Test perfetto collection returns true and output file size greater than zero
     * if the valid perfetto config file used.
     */
    @Test
    public void testPerfettoSuccess() throws Exception {
        assertTrue(perfettoHelper.startCollecting("valid_config.pb", false));
        assertTrue(perfettoHelper.stopCollecting(1000, "/data/local/tmp/out.pb"));
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        String[] fileStats = uiDevice.executeShellCommand(String.format(
                FILE_SIZE_IN_BYTES, "/data/local/tmp/out.pb")).split(" ");
        int fileSize = Integer.parseInt(fileStats[0].trim());
        assertTrue(fileSize > 0);
    }

    /**
     * Test perfetto collection returns true and output file size greater than zero
     * if the valid perfetto config file used.
     */
    @Test
    public void testTextProtoConfigSuccess() throws Exception {
        assertTrue(perfettoHelper.startCollecting("valid_text_config.textproto", true));
        assertTrue(perfettoHelper.stopCollecting(1000, "/data/local/tmp/out.pb"));
        UiDevice uiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        String[] fileStats = uiDevice.executeShellCommand(String.format(
                FILE_SIZE_IN_BYTES, "/data/local/tmp/out.pb")).split(" ");
        int fileSize = Integer.parseInt(fileStats[0].trim());
        assertTrue(fileSize > 0);
    }
}
