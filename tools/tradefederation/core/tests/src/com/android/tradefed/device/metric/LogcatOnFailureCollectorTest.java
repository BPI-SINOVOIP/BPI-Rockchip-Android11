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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.ddmlib.IDevice;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.device.ILogcatReceiver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.NullDevice;
import com.android.tradefed.device.TestDeviceState;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.IRunUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;

/** Unit tests for {@link LogcatOnFailureCollector}. */
@RunWith(JUnit4.class)
public class LogcatOnFailureCollectorTest {
    private TestableLogcatOnFailureCollector mCollector;
    private ITestInvocationListener mMockListener;
    private ITestDevice mMockDevice;
    private ITestDevice mNullMockDevice;

    private ITestInvocationListener mTestListener;
    private IInvocationContext mContext;
    private ILogcatReceiver mMockReceiver;
    private IRunUtil mMockRunUtil;

    private class TestableLogcatOnFailureCollector extends LogcatOnFailureCollector {

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
        ILogcatReceiver createLogcatReceiver(ITestDevice device) {
            return mMockReceiver;
        }

        @Override
        IRunUtil getRunUtil() {
            return mMockRunUtil;
        }
    }

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mNullMockDevice = EasyMock.createMock(ITestDevice.class);
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockReceiver = EasyMock.createMock(ILogcatReceiver.class);
        mMockRunUtil = EasyMock.createMock(IRunUtil.class);
        mCollector = new TestableLogcatOnFailureCollector();
        mContext = new InvocationContext();
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mContext.addAllocatedDevice("second_null_device", mNullMockDevice);

        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("serial");
        EasyMock.expect(mMockDevice.getIDevice()).andStubReturn(EasyMock.createMock(IDevice.class));
        EasyMock.expect(mMockDevice.getDeviceState()).andStubReturn(TestDeviceState.ONLINE);

        EasyMock.expect(mNullMockDevice.getIDevice()).andStubReturn(new NullDevice("null-dev"));
    }

    @Test
    public void testCollect() throws Exception {
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(20);
        mMockReceiver.start();
        mMockReceiver.clear();
        mMockReceiver.stop();
        mMockListener.testRunStarted(
                EasyMock.eq("runName"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        TestDescription test = new TestDescription("class", "test");
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testFailed(EasyMock.eq(test), (String) EasyMock.anyObject());
        mMockListener.testEnded(
                EasyMock.eq(test),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.testRunEnded(0L, new HashMap<String, Metric>());
        // Buffer at testRunStarted
        EasyMock.expect(mMockReceiver.getLogcatData())
                .andReturn(new ByteArrayInputStreamSource("aaa".getBytes()));
        // Buffer to be logged
        EasyMock.expect(mMockReceiver.getLogcatData(EasyMock.anyInt(), EasyMock.eq(3)))
                .andReturn(new ByteArrayInputStreamSource("aaabbb".getBytes()));
        mMockListener.testLog(
                EasyMock.eq("class#test-serial-logcat-on-failure"),
                EasyMock.eq(LogDataType.LOGCAT),
                EasyMock.anyObject());

        EasyMock.replay(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        mTestListener = mCollector.init(mContext, mMockListener);
        mTestListener.testRunStarted("runName", 1);
        mTestListener.testStarted(test);
        mTestListener.testFailed(test, "I failed");
        mTestListener.testEnded(test, new HashMap<String, Metric>());
        mTestListener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        // Ensure the callback went through
        assertTrue(mCollector.mOnTestStartCalled);
        assertTrue(mCollector.mOnTestFailCalled);
    }

    /**
     * If the API level support of the device is lower than a threshold we fall back to a different
     * collection for the logcat.
     */
    @Test
    public void testCollect_legacy() throws Exception {
        EasyMock.expect(mMockDevice.getApiLevel()).andReturn(18);
        mMockListener.testRunStarted(
                EasyMock.eq("runName"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        TestDescription test = new TestDescription("class", "test");
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testFailed(EasyMock.eq(test), (String) EasyMock.anyObject());
        mMockListener.testEnded(
                EasyMock.eq(test),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.testRunEnded(0L, new HashMap<String, Metric>());
        mMockDevice.executeShellCommand(EasyMock.eq("logcat -t 5000"), EasyMock.anyObject());
        mMockListener.testLog(
                EasyMock.eq("class#test-serial-logcat-on-failure"),
                EasyMock.eq(LogDataType.LOGCAT),
                EasyMock.anyObject());

        EasyMock.replay(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        mTestListener = mCollector.init(mContext, mMockListener);
        mTestListener.testRunStarted("runName", 1);
        mTestListener.testStarted(test);
        mTestListener.testFailed(test, "I failed");
        mTestListener.testEnded(test, new HashMap<String, Metric>());
        mTestListener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        // Ensure the callback went through
        assertTrue(mCollector.mOnTestStartCalled);
        assertTrue(mCollector.mOnTestFailCalled);
    }

    @Test
    public void testCollect_noRuns() throws Exception {
        // If there was no runs, nothing should be done.
        EasyMock.replay(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        mTestListener = mCollector.init(mContext, mMockListener);
        EasyMock.verify(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        assertFalse(mCollector.mOnTestStartCalled);
        assertFalse(mCollector.mOnTestFailCalled);
    }

    @Test
    public void testCollect_multiRun() throws Exception {
        EasyMock.expect(mMockDevice.getApiLevel()).andStubReturn(20);
        mMockReceiver.start();
        EasyMock.expectLastCall().times(2);
        mMockReceiver.clear();
        EasyMock.expectLastCall().times(2);
        mMockReceiver.stop();
        EasyMock.expectLastCall().times(2);
        mMockListener.testRunStarted(
                EasyMock.eq("runName"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        TestDescription test = new TestDescription("class", "test");
        TestDescription test2 = new TestDescription("class2", "test2");
        mMockListener.testStarted(EasyMock.eq(test), EasyMock.anyLong());
        mMockListener.testFailed(EasyMock.eq(test), (String) EasyMock.anyObject());
        mMockListener.testEnded(
                EasyMock.eq(test),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.testRunEnded(0L, new HashMap<String, Metric>());
        // Buffer at testRunStarted
        EasyMock.expect(mMockReceiver.getLogcatData())
                .andReturn(new ByteArrayInputStreamSource("aaa".getBytes()));
        // Buffer to be logged
        EasyMock.expect(mMockReceiver.getLogcatData(EasyMock.anyInt(), EasyMock.eq(3)))
                .andReturn(new ByteArrayInputStreamSource("aaabbb".getBytes()));
        mMockListener.testLog(
                EasyMock.eq("class#test-serial-logcat-on-failure"),
                EasyMock.eq(LogDataType.LOGCAT),
                EasyMock.anyObject());

        mMockListener.testRunStarted(
                EasyMock.eq("runName2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mMockListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mMockListener.testFailed(EasyMock.eq(test2), (String) EasyMock.anyObject());
        mMockListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mMockListener.testRunEnded(0L, new HashMap<String, Metric>());
        // Buffer at testRunStarted
        EasyMock.expect(mMockReceiver.getLogcatData())
                .andReturn(new ByteArrayInputStreamSource("aaa".getBytes()));
        // Buffer to be logged
        EasyMock.expect(mMockReceiver.getLogcatData(EasyMock.anyInt(), EasyMock.eq(3)))
                .andReturn(new ByteArrayInputStreamSource("aaabbb".getBytes()));
        mMockListener.testLog(
                EasyMock.eq("class2#test2-serial-logcat-on-failure"),
                EasyMock.eq(LogDataType.LOGCAT),
                EasyMock.anyObject());

        EasyMock.replay(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        mTestListener = mCollector.init(mContext, mMockListener);
        mTestListener.testRunStarted("runName", 1);
        mTestListener.testStarted(test);
        mTestListener.testFailed(test, "I failed");
        mTestListener.testEnded(test, new HashMap<String, Metric>());
        mTestListener.testRunEnded(0L, new HashMap<String, Metric>());
        // Second run
        mTestListener.testRunStarted("runName2", 1);
        mTestListener.testStarted(test2);
        mTestListener.testFailed(test2, "I failed");
        mTestListener.testEnded(test2, new HashMap<String, Metric>());
        mTestListener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener, mMockDevice, mMockReceiver, mNullMockDevice);
        // Ensure the callback went through
        assertTrue(mCollector.mOnTestStartCalled);
        assertTrue(mCollector.mOnTestFailCalled);
    }
}
