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
package com.android.tradefed.testtype;

import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * Internal listener to Trade Federation for {@link GTest}. Detect and report if duplciated tests
 * are run.
 */
final class GTestListener extends ResultForwarder {

    private static final int MAX_PARTIAL_SET_SIZE = 20;
    private Set<TestDescription> mTests = new HashSet<>();
    private Set<TestDescription> mDuplicateTests = new HashSet<>();
    private boolean mPartialList = false;

    public GTestListener(ITestInvocationListener... listeners) {
        super(listeners);
    }

    @Override
    public void testStarted(TestDescription test, long startTime) {
        super.testStarted(test, startTime);
        if (mDuplicateTests.size() < MAX_PARTIAL_SET_SIZE) {
            if (!mTests.add(test)) {
                mDuplicateTests.add(test);
            }
        } else {
            mPartialList = true;
            // Avoid storing too much data for too long.
            mTests.clear();
        }
    }

    @Override
    public void testRunEnded(long elapsedTime, HashMap<String, Metric> runMetrics) {
        if (!mDuplicateTests.isEmpty()) {
            StringBuilder errorMessage = new StringBuilder();
            errorMessage.append(
                    String.format("%s tests ran more than once.", mDuplicateTests.size()));
            if (mPartialList) {
                errorMessage.append(String.format(" Partial list: %s", mDuplicateTests));
            } else {
                errorMessage.append(String.format(" Full list: %s", mDuplicateTests));
            }
            FailureDescription error = FailureDescription.create(errorMessage.toString());
            error.setFailureStatus(FailureStatus.TEST_FAILURE)
                    // This error should not be retried
                    .setRetriable(false);
            super.testRunFailed(error);
        }
        super.testRunEnded(elapsedTime, runMetrics);
    }
}
