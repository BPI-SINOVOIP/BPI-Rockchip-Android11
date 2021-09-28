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
package com.android.tradefed.result;

import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/**
 * Variant of {@link ResultForwarder} that only allows a whitelist of {@link TestDescription} to be
 * reported.
 */
public class FilteredResultForwarder extends ResultForwarder {

    private final Collection<TestDescription> mAllowedTests;

    public FilteredResultForwarder(
            Collection<TestDescription> allowedTests, ITestInvocationListener... listeners) {
        super(listeners);
        mAllowedTests = allowedTests;
    }

    @Override
    public void testStarted(TestDescription test) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testStarted(test);
    }

    @Override
    public void testStarted(TestDescription test, long startTime) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testStarted(test, startTime);
    }

    @Override
    public void testEnded(TestDescription test, HashMap<String, Metric> testMetrics) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testEnded(test, testMetrics);
    }

    @Override
    public void testEnded(TestDescription test, long endTime, HashMap<String, Metric> testMetrics) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testEnded(test, endTime, testMetrics);
    }

    @Override
    public void testEnded(TestDescription test, long endTime, Map<String, String> testMetrics) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testEnded(test, endTime, testMetrics);
    }

    @Override
    public void testEnded(TestDescription test, Map<String, String> testMetrics) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testEnded(test, testMetrics);
    }

    @Override
    public void testAssumptionFailure(TestDescription test, String trace) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testAssumptionFailure(test, trace);
    }

    @Override
    public void testIgnored(TestDescription test) {
        if (!mAllowedTests.contains(test)) {
            return;
        }
        super.testIgnored(test);
    }
}
