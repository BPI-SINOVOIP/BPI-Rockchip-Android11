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

/** Unit tests for {@link MinApiLevelModuleController}. */
@RunWith(JUnit4.class)
public class MinApiLevelModuleControllerTest {
    private MinApiLevelModuleController mController;
    private IInvocationContext mContext;
    private ITestDevice mMockDevice;
    private IDevice mMockIDevice;
    private String mApiLevelProp;

    @Before
    public void setUp() {
        mController = new MinApiLevelModuleController();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mContext = new InvocationContext();
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_NAME, "module1");
        mMockIDevice = EasyMock.createMock(IDevice.class);
    }

    /** Test Api Level is lower than min-api-level */
    @Test
    public void testDeviceApiLevelLowerThanMinApiLevel()
            throws DeviceNotAvailableException, ConfigurationException {
        mApiLevelProp = "ro.build.version.sdk";
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        EasyMock.expect(mMockDevice.getProperty(mApiLevelProp)).andReturn("28");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("api-level-prop", mApiLevelProp);
        setter.setOptionValue("min-api-level", "29");
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }

    /** Test Api Level is higher than min-api-level */
    @Test
    public void testDeviceApiLevelHigherThanMinApiLevel()
            throws DeviceNotAvailableException, ConfigurationException {
        mApiLevelProp = "ro.product.first_api_level";
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        EasyMock.expect(mMockDevice.getProperty(mApiLevelProp)).andReturn("28");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("api-level-prop", mApiLevelProp);
        setter.setOptionValue("min-api-level", "27");
        assertEquals(RunStrategy.RUN, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }

    /** Test Api Level cannot be found in the Device */
    @Test
    public void testDeviceApiLevelNotFound()
            throws DeviceNotAvailableException, ConfigurationException {
        mApiLevelProp = "ro.product.first_api_level";
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice);
        EasyMock.expect(mMockDevice.getProperty(mApiLevelProp)).andReturn(null);
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("api-level-prop", mApiLevelProp);
        setter.setOptionValue("min-api-level", "27");
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }
}
