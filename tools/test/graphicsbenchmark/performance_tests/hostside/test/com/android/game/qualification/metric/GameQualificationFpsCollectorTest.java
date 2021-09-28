/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification.metric;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.game.qualification.ApkInfo;
import com.android.tradefed.device.metric.DeviceMetricData;

import org.junit.Before;
import org.junit.Test;

import java.util.Collections;
import java.util.List;

/** Test for {@link GameQualificationFpsCollector}. */
public class GameQualificationFpsCollectorTest {
    private static final String VSYNC = "16666666";

    private static final ApkInfo APK = new ApkInfo(
            "foo",
            "foo.apk",
            "com.foo",
            null,
            "Surface View - com.foo#0",
            null,
            Collections.emptyList(),
            10000,
            10000,
            true);

    private GameQualificationFpsCollector mCollector;

    @Before
    public void setUp() {
        mCollector = new GameQualificationFpsCollector();
        mCollector.setApkInfo(APK);
        mCollector.enable();
    }

    @Test
    public void basic() {
        mCollector.doStart(new DeviceMetricData(null));
        assertTrue(mCollector.hasError());

        mCollector.processRawData(new String[]{VSYNC, "1\t2\t3", "4\t5\t6"});
        List<GameQualificationMetric> metrics = mCollector.getElapsedTimes();
        assertFalse(mCollector.hasError());
        assertEquals(2, metrics.get(0).getActualPresentTime());
        assertEquals(3, metrics.get(0).getFrameReadyTime());

        assertEquals(5, metrics.get(1).getActualPresentTime());
        assertEquals(6, metrics.get(1).getFrameReadyTime());

        mCollector.processRawData(new String[]{VSYNC, "7\t8\t9"});
        assertEquals(8, metrics.get(2).getActualPresentTime());
        assertEquals(9, metrics.get(2).getFrameReadyTime());
    }

    @Test
    public void appTerminated() {
        mCollector.doStart(new DeviceMetricData(null));

        mCollector.processRawData(new String[]{VSYNC, "1\t2\t3"});
        assertFalse(mCollector.hasError());
        try {
            // If layer does not exist, dumpsys contains a single
            mCollector.processRawData(new String[]{VSYNC});
            fail("expected exception");
        } catch (RuntimeException e){
            // Do nothing.
        }
    }

    @Test
    public void regexPattern() {
        ApkInfo apk = new ApkInfo(
                "foo",
                "foo.apk",
                "com.foo",
                null,
                "*Invalid pattern",
                null,
                Collections.emptyList(),
                10000,
                10000,
                true);
        mCollector.setApkInfo(apk);
        try {
            mCollector.doStart(new DeviceMetricData(null));
            fail("expected exception");
        } catch (RuntimeException e){
            // Do nothing.
        }
    }
}