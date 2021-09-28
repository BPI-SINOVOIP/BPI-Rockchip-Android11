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
import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.metric.DeviceMetricData;

import org.junit.Test;

public class BaseGameQualificationMetricCollectorTest {

    @Test
    public void onTestStartHasErrorOnException() {
        BaseGameQualificationMetricCollector collector = new BaseGameQualificationMetricCollector() {
            @Override
            protected void onStart(DeviceMetricData testData) {
                throw new RuntimeException("foo");
            }
        };
        try {
            collector.onTestStart(null);
        } catch (RuntimeException e) {
            assertEquals("foo", e.getMessage());
            assertTrue(collector.hasError());
            assertEquals("foo", collector.getErrorMessage());
        }
    }

    @Test
    public void onTestEndHasErrorOnException() {
        BaseGameQualificationMetricCollector collector = new BaseGameQualificationMetricCollector() {
            @Override
            protected void onStart(DeviceMetricData testData) {
                throw new RuntimeException("foo");
            }
        };
        try {
            collector.onTestEnd(null, null);
        } catch (RuntimeException e) {
            assertEquals("foo", e.getMessage());
            assertTrue(collector.hasError());
            assertEquals("foo", collector.getErrorMessage());
        }
    }
}