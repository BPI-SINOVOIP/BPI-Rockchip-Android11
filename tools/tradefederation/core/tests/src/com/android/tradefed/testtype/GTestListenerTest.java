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

import static org.junit.Assert.assertTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;

import org.easymock.Capture;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;

/** Unit tests for {@link GTestListener}. */
@RunWith(JUnit4.class)
public class GTestListenerTest {
    private ITestInvocationListener mMockListener = null;

    /** Helper to initialize the various EasyMocks we'll need. */
    @Before
    public void setUp() throws Exception {
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
    }

    /** Helper that replays all mocks. */
    private void replayMocks() {
        EasyMock.replay(mMockListener);
    }

    /** Helper that verifies all mocks. */
    private void verifyMocks() {
        EasyMock.verify(mMockListener);
    }

    /** Verify test passes without duplicate tests */
    @Test
    public void testNoDuplicateTests() throws DeviceNotAvailableException {
        String moduleName = "testNoDuplicateTest";
        String testClass = "testClass";
        String testName1 = "testName1";
        TestDescription testId1 = new TestDescription(testClass, testName1);
        String testName2 = "testName2";
        TestDescription testId2 = new TestDescription(testClass, testName2);

        mMockListener.testRunStarted(EasyMock.eq(moduleName), EasyMock.eq(2));
        mMockListener.testStarted(EasyMock.eq(testId1), EasyMock.anyLong());
        mMockListener.testEnded(
                EasyMock.eq(testId1),
                EasyMock.anyLong(),
                (HashMap<String, Metric>) EasyMock.anyObject());
        mMockListener.testStarted(EasyMock.eq(testId2), EasyMock.anyLong());
        mMockListener.testEnded(
                EasyMock.eq(testId2),
                EasyMock.anyLong(),
                (HashMap<String, Metric>) EasyMock.anyObject());
        mMockListener.testRunEnded(
                EasyMock.anyLong(), (HashMap<String, Metric>) EasyMock.anyObject());
        replayMocks();

        HashMap<String, Metric> emptyMap = new HashMap<>();
        GTestListener listener = new GTestListener(mMockListener);
        listener.testRunStarted(moduleName, 2);
        listener.testStarted(testId1);
        listener.testEnded(testId1, emptyMap);
        listener.testStarted(testId2);
        listener.testEnded(testId2, emptyMap);
        listener.testRunEnded(0, emptyMap);
        verifyMocks();
    }

    /** Verify test with duplicate tests fails if option enabled */
    @Test
    public void testDuplicateTestsFailsWithOptionEnabled() throws DeviceNotAvailableException {
        String moduleName = "testWithDuplicateTests";
        String testClass = "testClass";
        String testName1 = "testName1";
        String duplicateTestsMessage = "1 tests ran more than once. Full list:";
        TestDescription testId1 = new TestDescription(testClass, testName1);

        mMockListener.testRunStarted(EasyMock.eq(moduleName), EasyMock.eq(2));
        mMockListener.testStarted(EasyMock.eq(testId1), EasyMock.anyLong());
        mMockListener.testEnded(
                EasyMock.eq(testId1),
                EasyMock.anyLong(),
                (HashMap<String, Metric>) EasyMock.anyObject());
        mMockListener.testStarted(EasyMock.eq(testId1), EasyMock.anyLong());
        mMockListener.testEnded(
                EasyMock.eq(testId1),
                EasyMock.anyLong(),
                (HashMap<String, Metric>) EasyMock.anyObject());
        Capture<FailureDescription> capturedFailureDescription = new Capture<>();
        mMockListener.testRunFailed(EasyMock.capture(capturedFailureDescription));
        mMockListener.testRunEnded(
                EasyMock.anyLong(), (HashMap<String, Metric>) EasyMock.anyObject());
        replayMocks();

        HashMap<String, Metric> emptyMap = new HashMap<>();
        GTestListener listener = new GTestListener(mMockListener);
        listener.testRunStarted(moduleName, 2);
        listener.testStarted(testId1);
        listener.testEnded(testId1, emptyMap);
        listener.testStarted(testId1);
        listener.testEnded(testId1, emptyMap);
        listener.testRunEnded(0, emptyMap);
        FailureDescription failureDescription = capturedFailureDescription.getValue();
        assertTrue(failureDescription.getErrorMessage().contains(duplicateTestsMessage));
        assertTrue(FailureStatus.TEST_FAILURE.equals(failureDescription.getFailureStatus()));
        verifyMocks();
    }
}
