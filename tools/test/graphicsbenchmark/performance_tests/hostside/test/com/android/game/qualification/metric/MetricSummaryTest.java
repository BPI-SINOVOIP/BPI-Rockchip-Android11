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

import static com.android.game.qualification.metric.MetricSummary.TimeType.PRESENT;
import static com.android.game.qualification.metric.MetricSummary.TimeType.READY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import com.android.game.qualification.CertificationRequirements;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;

@RunWith(JUnit4.class)
public class MetricSummaryTest {

    private static final double EPSILON = Math.ulp(1e9);
    private static final CertificationRequirements TEST_REQUIREMENTS =
            new CertificationRequirements(
                    "foo",
                    500,  /* 500ms */
                    0.0f,
                    10000);

    @Test
    public void testConversion() {
        MetricSummary.Builder builder = new MetricSummary.Builder(TEST_REQUIREMENTS, 500_000_000);
        builder.beginLoop();
        builder.addFrameTime(PRESENT, 1);
        builder.addFrameTime(PRESENT, 2);
        builder.addFrameTime(PRESENT, 3);
        builder.addFrameTime(READY, 10);
        builder.addFrameTime(READY, 10);
        builder.addFrameTime(READY, 10);
        builder.endLoop();
        builder.beginLoop();
        builder.addFrameTime(PRESENT, 749_999_999);
        builder.addFrameTime(PRESENT, 750_000_000);
        builder.addFrameTime(PRESENT, 499_999_995);
        builder.addFrameTime(READY, 20);
        builder.addFrameTime(READY, 20);
        builder.endLoop();
        builder.setLoadTimeMs(42);

        MetricSummary summary = builder.build();

        assertEquals(42, summary.getLoadTimeMs());

        IInvocationContext context = new InvocationContext();
        DeviceMetricData runData = new DeviceMetricData(context);
        summary.addToMetricData(runData);
        HashMap<String, MetricMeasurement.Metric> metrics = new HashMap<>();
        runData.addToMetrics(metrics);

        MetricSummary result = MetricSummary.parseRunMetrics(context, metrics);
        assertNotNull(result);
        assertEquals(summary, result);
    }
}