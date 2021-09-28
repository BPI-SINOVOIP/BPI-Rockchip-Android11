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

package android.platform.test.rule;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.os.Bundle;

import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.junit.runners.model.Statement;
import org.mockito.Mockito;

@RunWith(JUnit4.class)
public class StopwatchRuleTest {

    private static final int SLEEP_TIME_MS = 1000;
    private static final int TIME_DELTA_MS = 20;

    @Test
    public void testMeasurementIsCorrect() throws Throwable {
        StopwatchRule rule = new StopwatchRule();
        rule.apply(
                        new Statement() {
                            //Mock a test that will take 1000ms long
                            @Override
                            public void evaluate() throws Throwable {
                                Thread.sleep(SLEEP_TIME_MS);
                            }
                        },
                        Description.createTestDescription("clzz", "method"))
                .evaluate();

        Bundle metric = rule.getMetric();
        String metricKey = String.format(StopwatchRule.METRIC_FORMAT, "clzz", "method");
        long value = metric.getLong(metricKey);
        // Assert if StopwatchRule correctly measures the test time.
        assertEquals(SLEEP_TIME_MS, value, TIME_DELTA_MS);
    }

    @Test
    public void testMetricSendToInstr() throws Throwable {
        StopwatchRule rule = new StopwatchRule();
        Instrumentation instr = Mockito.mock(Instrumentation.class);
        rule.setInstrumentation(instr);
        rule.apply(
                        new Statement() {
                            @Override
                            public void evaluate() throws Throwable {}
                        },
                        Description.EMPTY)
                .evaluate();
        verify(instr).sendStatus(StopwatchRule.INST_STATUS_IN_PROGRESS, rule.getMetric());
    }
}
