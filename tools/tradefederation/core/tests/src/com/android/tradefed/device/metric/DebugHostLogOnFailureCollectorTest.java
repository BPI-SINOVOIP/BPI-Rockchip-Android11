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
package com.android.tradefed.device.metric;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;

import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.StreamUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;
import org.mockito.stubbing.Answer;

import java.util.HashMap;

/** Unit tests for {@link DebugHostLogOnFailureCollector}. */
@RunWith(JUnit4.class)
public class DebugHostLogOnFailureCollectorTest {
    private TestableHostLogOnFailureCollector mCollector;
    private ITestInvocationListener mMockListener;

    private ITestInvocationListener mTestListener;
    private IInvocationContext mContext;
    private ILeveledLogOutput mMockLogger;

    private class TestableHostLogOnFailureCollector extends DebugHostLogOnFailureCollector {

        public boolean mOnTestStartCalled = false;
        public boolean mOnTestFailCalled = false;

        @Override
        public void onTestStart(DeviceMetricData testData) {
            super.onTestStart(testData);
            mOnTestStartCalled = true;
        }

        @Override
        public void onTestFail(DeviceMetricData testData, TestDescription test) {
            super.onTestFail(testData, test);
            mOnTestFailCalled = true;
        }

        @Override
        ILeveledLogOutput getLogger() {
            return mMockLogger;
        }
    }

    @Before
    public void setUp() {
        mMockListener = Mockito.mock(ITestInvocationListener.class);
        mMockLogger = Mockito.mock(ILeveledLogOutput.class);
        mCollector = new TestableHostLogOnFailureCollector();
        mContext = new InvocationContext();
    }

    @Test
    public void testCollect() throws Exception {
        TestDescription test = new TestDescription("class", "test");
        // Buffer at testRunStarted, then the one we want to log
        Mockito.when(mMockLogger.getLog())
                .thenReturn(
                        new ByteArrayInputStreamSource("aaa".getBytes()),
                        new ByteArrayInputStreamSource("aaabbb".getBytes()));
        Mockito.doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    InputStreamSource arg3 =
                                            (InputStreamSource) invocation.getArgument(2);
                                    assertEquals("bbb", StreamUtil.getStringFromSource(arg3));
                                    return null;
                                })
                .when(mMockListener)
                .testLog(
                        Mockito.eq("class#test-debug-hostlog-on-failure"),
                        Mockito.eq(LogDataType.TEXT),
                        Mockito.any());
        mTestListener = mCollector.init(mContext, mMockListener);
        mTestListener.testRunStarted("runName", 1);
        mTestListener.testStarted(test);
        mTestListener.testFailed(test, "I failed");
        mTestListener.testEnded(test, new HashMap<String, Metric>());
        mTestListener.testRunEnded(0L, new HashMap<String, Metric>());

        // Ensure the callback went through
        assertTrue(mCollector.mOnTestStartCalled);
        assertTrue(mCollector.mOnTestFailCalled);

        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq("runName"), Mockito.eq(1), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mMockListener).testStarted(Mockito.eq(test), Mockito.anyLong());
        Mockito.verify(mMockListener).testFailed(Mockito.eq(test), (String) Mockito.any());
        Mockito.verify(mMockListener)
                .testLog(
                        Mockito.eq("class#test-debug-hostlog-on-failure"),
                        Mockito.eq(LogDataType.TEXT),
                        Mockito.any());
        Mockito.verify(mMockListener)
                .testEnded(
                        Mockito.eq(test),
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, Metric>>any());
        Mockito.verify(mMockListener).testRunEnded(0L, new HashMap<String, Metric>());
    }

    /** Test when we fail to obtain the host_log from the start of the collector. */
    @Test
    public void testCollect_null() throws Exception {
        TestDescription test = new TestDescription("class", "test");
        // Buffer at testRunStarted, then the one we want to log
        Mockito.when(mMockLogger.getLog()).thenReturn(null);
        mTestListener = mCollector.init(mContext, mMockListener);
        mTestListener.testRunStarted("runName", 1);
        mTestListener.testStarted(test);
        mTestListener.testFailed(test, "I failed");
        mTestListener.testEnded(test, new HashMap<String, Metric>());
        mTestListener.testRunEnded(0L, new HashMap<String, Metric>());

        // Ensure the callback went through
        assertTrue(mCollector.mOnTestStartCalled);
        assertTrue(mCollector.mOnTestFailCalled);

        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq("runName"), Mockito.eq(1), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mMockListener).testStarted(Mockito.eq(test), Mockito.anyLong());
        Mockito.verify(mMockListener).testFailed(Mockito.eq(test), (String) Mockito.any());
        // No file is logged
        Mockito.verify(mMockListener, never())
                .testLog(
                        Mockito.eq("class#test-debug-hostlog-on-failure"),
                        Mockito.eq(LogDataType.TEXT),
                        Mockito.any());
        Mockito.verify(mMockListener)
                .testEnded(
                        Mockito.eq(test),
                        Mockito.anyLong(),
                        Mockito.<HashMap<String, Metric>>any());
        Mockito.verify(mMockListener).testRunEnded(0L, new HashMap<String, Metric>());
    }
}
