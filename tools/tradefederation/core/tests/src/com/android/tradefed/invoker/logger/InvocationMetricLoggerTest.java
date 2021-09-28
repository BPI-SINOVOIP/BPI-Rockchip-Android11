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
package com.android.tradefed.invoker.logger;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;

import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Map;
import java.util.UUID;

/** Unit tests for {@link InvocationMetricLogger}. */
@RunWith(JUnit4.class)
public class InvocationMetricLoggerTest {

    @Test
    public void testLogMetrics() throws Exception {
        Map<String, String> result = logMetric(InvocationMetricKey.FETCH_BUILD, "TEST", null);
        assertEquals("TEST", result.get(InvocationMetricKey.FETCH_BUILD.toString()));
        // Ensure that it wasn't set to the current ThreadGroup
        assertNotSame(
                "TEST",
                InvocationMetricLogger.getInvocationMetrics()
                        .get(InvocationMetricKey.FETCH_BUILD.toString()));
    }

    @Test
    public void testLogMetrics_appendString() throws Exception {
        Map<String, String> result =
                logMetric(InvocationMetricKey.STAGE_TESTS_INDIVIDUAL_DOWNLOADS, "file1", "file2");
        // Ensure that the metric value is appended.
        assertEquals(
                "file1,file2",
                result.get(InvocationMetricKey.STAGE_TESTS_INDIVIDUAL_DOWNLOADS.toString()));
    }

    private Map<String, String> logMetric(InvocationMetricKey key, String value, String value2)
            throws Exception {
        String uuid = UUID.randomUUID().toString();
        ThreadGroup testGroup = new ThreadGroup("unit-test-group-" + uuid);
        TestRunnable runnable = new TestRunnable(key, value, value2);
        Thread testThread = new Thread(testGroup, runnable);
        testThread.setName("InvocationMetricLoggerTest-test-thread");
        testThread.setDaemon(true);
        testThread.start();
        testThread.join(10000);
        return runnable.getResultMap();
    }

    private class TestRunnable implements Runnable {

        private InvocationMetricKey mKey;
        private String mValue;
        private String mValue2 = null;
        private Map<String, String> mResultMap;

        public TestRunnable(InvocationMetricKey key, String value, String value2) {
            mKey = key;
            mValue = value;
            mValue2 = value2;
        }

        @Override
        public void run() {
            InvocationMetricLogger.addInvocationMetrics(mKey, mValue);
            if (mValue2 != null) {
                InvocationMetricLogger.addInvocationMetrics(mKey, mValue2);
            }
            mResultMap = InvocationMetricLogger.getInvocationMetrics();
        }

        public Map<String, String> getResultMap() {
            return mResultMap;
        }
    }
}
