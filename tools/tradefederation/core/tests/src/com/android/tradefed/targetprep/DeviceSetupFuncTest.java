/*
 * Copyright (C) 2010 The Android Open Source Project
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

import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IDeviceTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Functional tests for {@link DeviceSetup}. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class DeviceSetupFuncTest implements IDeviceTest {

    private DeviceSetup mDeviceSetup;
    private ITestDevice mDevice;
    private IDeviceBuildInfo mMockBuildInfo;
    private TestInformation mStubInformation;

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    @Before
    public void setUp() throws Exception {
        mMockBuildInfo = new DeviceBuildInfo("0", "");
        mDeviceSetup = new DeviceSetup();
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mDevice);
        context.addDeviceBuildInfo("device", mMockBuildInfo);
        mStubInformation = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    /**
     * Simple normal case test for {@link DeviceSetup#setUp(TestInformation)}.
     *
     * <p>Do setup and verify a few expected properties
     */
    @Test
    public void testSetup() throws Exception {
        // Reset expected property
        getDevice().executeShellCommand("setprop ro.audio.silent 0");
        mDeviceSetup.setUp(mStubInformation);
        assertTrue(getDevice().executeShellCommand("getprop ro.audio.silent").contains("1"));
        assertTrue(getDevice().executeShellCommand("getprop ro.monkey").contains("1"));
        assertTrue(getDevice().executeShellCommand("getprop ro.test_harness").contains("1"));
        // Verify root
        assertTrue(getDevice().executeShellCommand("id").contains("root"));
    }
}
