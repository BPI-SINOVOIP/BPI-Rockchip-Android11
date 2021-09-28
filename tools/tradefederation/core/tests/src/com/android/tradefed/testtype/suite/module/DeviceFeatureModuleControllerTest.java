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
package com.android.tradefed.testtype.suite.module;

import static org.junit.Assert.assertEquals;

import com.android.ddmlib.IDevice;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.testtype.suite.module.IModuleController.RunStrategy;
import com.android.tradefed.testtype.suite.ModuleDefinition;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link DeviceFeatureModuleController}. */
@RunWith(JUnit4.class)
public class DeviceFeatureModuleControllerTest {
    private static final String REQUIRED_FEATURE = "required.feature";
    private DeviceFeatureModuleController mController;
    private IInvocationContext mContext;
    private ITestDevice mMockDevice;
    private IDevice mMockIDevice;

    @Before
    public void setUp() {
        mController = new DeviceFeatureModuleController();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mContext = new InvocationContext();
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_NAME, "module1");
        mMockIDevice = EasyMock.createMock(IDevice.class);
    }

    /** Test device has the required feature. */
    @Test
    public void testDeviceHasRequiredFeature()
            throws DeviceNotAvailableException, ConfigurationException {
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        EasyMock.expect(mMockDevice.hasFeature(REQUIRED_FEATURE)).andReturn(true);
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("required-feature", REQUIRED_FEATURE);
        assertEquals(RunStrategy.RUN, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }

    /** Test device does not have the required feature. */
    @Test
    public void testDeviceHasNoRequiredFeature()
            throws DeviceNotAvailableException, ConfigurationException {
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        EasyMock.expect(mMockDevice.hasFeature(REQUIRED_FEATURE)).andReturn(false);
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("required-feature", REQUIRED_FEATURE);
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }
}
