/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.tradefed.targetprep;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.command.remote.DeviceDescriptor;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.InstrumentationTest;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;

/** Unit tests for {@link InstrumentationPreparer}. */
@RunWith(JUnit4.class)
public class InstrumentationPreparerTest {

    private InstrumentationPreparer mInstrumentationPreparer;
    private ITestDevice mMockDevice;
    private IDeviceBuildInfo mMockBuildInfo;
    private InstrumentationTest mMockITest;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.getSerialNumber()).andReturn("foo").anyTimes();
        mMockBuildInfo = new DeviceBuildInfo("0","");
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mMockDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    @Test
    public void testRun() throws Exception {
        final String packageName = "packageName";
        final TestDescription test = new TestDescription("FooTest", "testFoo");
        mMockITest =
                new InstrumentationTest() {
                    @Override
                    public void run(TestInformation testInfo, ITestInvocationListener listener) {
                        listener.testRunStarted(packageName, 1);
                        listener.testStarted(test);
                        listener.testEnded(test, new HashMap<String, Metric>());
                        listener.testRunEnded(0, new HashMap<String, Metric>());
                    }
                };
        mInstrumentationPreparer = new InstrumentationPreparer() {
            @Override
            InstrumentationTest createInstrumentationTest() {
                return mMockITest;
            }
        };
        EasyMock.replay(mMockDevice);
        mInstrumentationPreparer.setUp(mTestInfo);
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testRun_testFailed() throws Exception {
        final String packageName = "packageName";
        final TestDescription test = new TestDescription("FooTest", "testFoo");
        mMockITest =
                new InstrumentationTest() {
                    @Override
                    public void run(TestInformation testInfo, ITestInvocationListener listener) {
                        listener.testRunStarted(packageName, 1);
                        listener.testStarted(test);
                        listener.testFailed(test, "error");
                        listener.testEnded(test, new HashMap<String, Metric>());
                        listener.testRunEnded(0, new HashMap<String, Metric>());
                    }
                };
        mInstrumentationPreparer = new InstrumentationPreparer() {
            @Override
            InstrumentationTest createInstrumentationTest() {
                return mMockITest;
            }
        };
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andStubReturn(new DeviceDescriptor(
                "SERIAL", false, DeviceAllocationState.Available, "unknown", "unknown", "unknown",
                "unknown", "unknown"));
        EasyMock.replay(mMockDevice);
        try {
            mInstrumentationPreparer.setUp(mTestInfo);
            fail("BuildError not thrown");
        } catch(final BuildError e) {
            assertTrue("The exception message does not contain failed test names",
                    e.getMessage().contains(test.toString()));
            // expected
        }
        EasyMock.verify(mMockDevice);
    }
}
