/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.tradefed.testtype.suite.module;

import static org.junit.Assert.assertEquals;

import com.android.ddmlib.IDevice;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.testtype.suite.module.IModuleController.RunStrategy;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link CarModuleController}. */
@RunWith(JUnit4.class)
public class CarModuleControllerTest {

    private static final String FEATURE_AUTOMOTIVE = "android.hardware.type.automotive";

    private CarModuleController mController;
    private IInvocationContext mContext;
    private ITestDevice mMockDevice;
    private IDevice mMockIDevice;

    @Before
    public void setUp() {
        mController = new CarModuleController();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mContext = new InvocationContext();
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_NAME, "module1");
        mMockIDevice = EasyMock.createMock(IDevice.class);
    }

    /** Test that a StubDevice is ignored by the check. */
    @Test
    public void testStubDevice() {
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(new StubDevice("serial"));
        EasyMock.replay(mMockDevice);
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }

    /** Test the check when the device does not support feature automotive. */
    @Test
    public void testNotAutomotive() throws Exception {
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        EasyMock.expect(mMockDevice.hasFeature(FEATURE_AUTOMOTIVE)).andReturn(false);
        EasyMock.replay(mMockDevice, mMockIDevice);
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice, mMockIDevice);
    }

    /** Test when the device supports feature automotive. */
    @Test
    public void testAutomotive() throws Exception {
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        EasyMock.expect(mMockDevice.hasFeature(FEATURE_AUTOMOTIVE)).andReturn(true);
        EasyMock.replay(mMockDevice, mMockIDevice);
        assertEquals(RunStrategy.RUN, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice, mMockIDevice);
    }
}
