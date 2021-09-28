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

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.device.metric.DeviceMetricData;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

public class GameQualificationScheduledMetricCollectorTest {

    /** Ensure collect and doEnd do not run at the same time */
    @Test
    public void checkConcurrency() {
        GameQualificationScheduledMetricCollector collector =
                new GameQualificationScheduledMetricCollector() {
                    private boolean value = true;

                    @Override
                    protected void collect() {
                        try {
                            TimeUnit.MILLISECONDS.sleep(1);
                        } catch (InterruptedException e) {
                            fail(e.getMessage());
                        }
                        value = false;
                    }

                    @Override
                    protected void doStart(DeviceMetricData runData) {
                        value = true;
                        assertTrue(value);
                    }

                    @Override
                    protected void doEnd(DeviceMetricData runData) {
                        value = true;
                        try {
                            TimeUnit.MILLISECONDS.sleep(5);
                        } catch (InterruptedException e) {
                            fail(e.getMessage());
                        }
                        assertTrue(value);
                    }
                };
        collector.mIntervalMs = 1;
        collector.enable();

        collector.onTestStart(null);
        collector.onTestEnd(null, null);
    }
}