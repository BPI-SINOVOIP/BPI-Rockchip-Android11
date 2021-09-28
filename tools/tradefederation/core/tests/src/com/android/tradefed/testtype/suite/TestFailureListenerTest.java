/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.tradefed.testtype.suite;

import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/** Unit tests for {@link com.android.tradefed.testtype.suite.TestFailureListener} */
@RunWith(JUnit4.class)
public class TestFailureListenerTest {

    private TestFailureListener mFailureListener;
    private ITestInvocationListener mMockListener;
    private ITestDevice mMockDevice;
    private List<ITestDevice> mListDevice;

    @Before
    public void setUp() {
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockDevice = EasyMock.createStrictMock(ITestDevice.class);
        mListDevice = new ArrayList<>();
        mListDevice.add(mMockDevice);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("SERIAL");
        // Create base failure listener with all option ON and default logcat size.
        mFailureListener = new TestFailureListener(mListDevice, true, true);
        mFailureListener.setLogger(mMockListener);
    }

    /** Test that on testFailed all the collection are triggered. */
    @Test
    @SuppressWarnings("MustBeClosedChecker")
    public void testTestFailed() throws Exception {
        TestDescription testId = new TestDescription("com.fake", "methodfake");
        final String trace = "oups it failed";
        // Bugreport routine - testLog is internal to it.
        EasyMock.expect(mMockDevice.logBugreport(EasyMock.anyObject(), EasyMock.anyObject()))
                .andReturn(true);
        // Reboot routine
        EasyMock.expect(mMockDevice.getProperty(EasyMock.eq("ro.build.type")))
                .andReturn("userdebug");
        mMockDevice.reboot();
        EasyMock.replay(mMockListener, mMockDevice);
        mFailureListener.testStarted(testId);
        mFailureListener.testFailed(testId, trace);
        mFailureListener.testEnded(testId, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener, mMockDevice);
    }

    /**
     * Test when a test failure occurs and it is a user build, no reboot is attempted.
     */
    @Test
    public void testTestFailed_userBuild() throws Exception {
        mFailureListener = new TestFailureListener(mListDevice, false, true);
        mFailureListener.setLogger(mMockListener);
        final String trace = "oups it failed";
        TestDescription testId = new TestDescription("com.fake", "methodfake");
        EasyMock.expect(mMockDevice.getProperty(EasyMock.eq("ro.build.type"))).andReturn("user");
        EasyMock.replay(mMockListener, mMockDevice);
        mFailureListener.testStarted(testId);
        mFailureListener.testFailed(testId, trace);
        mFailureListener.testEnded(testId, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener, mMockDevice);
    }

    /**
     * Test when a test failure occurs during a multi device run. Each device should capture the
     * logs.
     */
    @Test
    public void testFailed_multiDevice() throws Exception {
        ITestDevice device2 = EasyMock.createMock(ITestDevice.class);
        mListDevice.add(device2);
        mFailureListener = new TestFailureListener(mListDevice, false, true);
        mFailureListener.setLogger(mMockListener);
        final String trace = "oups it failed";
        TestDescription testId = new TestDescription("com.fake", "methodfake");
        EasyMock.expect(mMockDevice.getProperty(EasyMock.eq("ro.build.type"))).andReturn("debug");
        mMockDevice.reboot();
        EasyMock.expect(device2.getSerialNumber()).andStubReturn("SERIAL2");
        EasyMock.expect(device2.getProperty(EasyMock.eq("ro.build.type"))).andReturn("debug");
        device2.reboot();

        EasyMock.replay(mMockListener, mMockDevice, device2);
        mFailureListener.testStarted(testId);
        mFailureListener.testFailed(testId, trace);
        mFailureListener.testEnded(testId, new HashMap<String, Metric>());
        EasyMock.verify(mMockListener, mMockDevice, device2);
    }
}
