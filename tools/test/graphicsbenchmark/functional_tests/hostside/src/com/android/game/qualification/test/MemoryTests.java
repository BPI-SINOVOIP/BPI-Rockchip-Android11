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

package com.android.game.qualification.test;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.NativeDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.DataType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner.TestMetrics;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

@RunWith(DeviceJUnit4ClassRunner.class)
public class MemoryTests extends BaseHostJUnit4Test {
    // The time in ms to wait before starting logcat on NativeDevice
    private static final int LOG_START_DELAY = 5 * 1000;

    @Rule
    public TestMetrics metrics = new TestMetrics();

    @Before
    public void setUp() throws DeviceNotAvailableException {
        TestUtils.assumeGameCoreCertified(getDevice());
    }

    /**
     * Device must be able to allocate at least 2.3GB of memory.
     */
    @Test
    public void testMemoryAllocationLimit()
        throws DeviceNotAvailableException, IOException {
            getDevice().startLogcat();
            // Wait until starting logcat for the device.
            try {
                Thread.sleep(LOG_START_DELAY);
            } catch (InterruptedException e) {
            }
            getDevice().clearLogcat();

            String pkgname = "com.android.game.qualification.allocstress";
            String actname = pkgname + ".MainActivity";
            getDevice().executeShellCommand("am start -W " + pkgname + "/" + actname);

            // Wait until app is finished.
            while((getDevice().executeShellCommand("dumpsys activity | grep top-activity")).contains("allocstress"));

            boolean hasAllocStats = false;
            int totalAllocated = 0;

            try (
                InputStreamSource logcatSource = getDevice().getLogcat();
                BufferedReader logcat = new BufferedReader(new InputStreamReader(logcatSource.createInputStream()));
            ) {

                String s = logcat.readLine();
                String pattern = "total alloc: ";
                String p = null;
                while (s != null) {
                    if (s.contains(pattern)) {
                        hasAllocStats = true;
                        p = s;
                    }
                    s = logcat.readLine();
                }
                int totalAllocIndex = p.indexOf(pattern) + pattern.length();
                totalAllocated = Integer.parseInt(p.substring(totalAllocIndex));

            }

            getDevice().stopLogcat();

            assertTrue(hasAllocStats);
            metrics.addTestMetric("memory_allocated", Metric.newBuilder()
                                                            .setType(DataType.RAW)
                                                            .setMeasurements(Measurements.newBuilder().setSingleInt(totalAllocated))
                                                            .build());
            assertTrue("Device failed to allocate an appropriate amount of memory (2.3GB) before being killed", totalAllocated > 2300);
    }

}
