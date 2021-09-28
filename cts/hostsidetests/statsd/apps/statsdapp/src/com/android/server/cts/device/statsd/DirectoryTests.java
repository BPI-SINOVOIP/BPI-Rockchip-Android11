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

package com.android.server.cts.device.statsd;

import org.junit.Test;

import java.io.File;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

public class DirectoryTests {

    @Test
    public void testStatsActiveMetricDirectoryExists() {
        final File f = new File("/data/misc/stats-active-metric/");
        assertTrue(f.exists());
        assertFalse(f.isFile());
    }

    @Test
    public void testStatsDataDirectoryExists() {
        final File f = new File("/data/misc/stats-data/");
        assertTrue(f.exists());
        assertFalse(f.isFile());
    }

    @Test
    public void testStatsMetadataDirectoryExists() {
        final File f = new File("/data/misc/stats-metadata/");
        assertTrue(f.exists());
        assertFalse(f.isFile());
    }

    @Test
    public void testStatsServiceDirectoryExists() {
        final File f = new File("/data/misc/stats-service/");
        assertTrue(f.exists());
        assertFalse(f.isFile());
    }

    @Test
    public void testTrainInfoDirectoryExists() {
        final File f = new File("/data/misc/train-info/");
        assertTrue(f.exists());
        assertFalse(f.isFile());
    }
}
