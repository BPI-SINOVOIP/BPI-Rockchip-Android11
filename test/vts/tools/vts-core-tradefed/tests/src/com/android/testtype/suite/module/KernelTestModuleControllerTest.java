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
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.testtype.suite.module.IModuleController.RunStrategy;
import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for {@link KernelTestModuleControllerTest}.
 */
@RunWith(JUnit4.class)
public class KernelTestModuleControllerTest {
    private KernelTestModuleController mController;
    private IInvocationContext mContext;
    private ITestDevice mMockDevice;
    private IDevice mMockIDevice;
    private final String mProductNameProp = "ro.product.name";
    private final String mLowMemProp = "ro.config.low_ram";

    @Before
    public void setUp() {
        mController = new KernelTestModuleController();
        mMockDevice = EasyMock.createMock(ITestDevice.class);
        mContext = new InvocationContext();
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_NAME, "module1");
        mContext.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mMockDevice);
        mMockIDevice = EasyMock.createMock(IDevice.class);
    }

    @Test
    public void testModuleAbiMatchesArch()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("false");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm64");
        assertEquals(RunStrategy.RUN, mController.shouldRunModule(mContext));
    }

    @Test
    public void testModuleAbiMismatchesArch()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("false");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm");
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
    }

    @Test
    public void testDeviceWithLowMemAndIsLowMemFlagTrue()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("true");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm64");
        setter.setOptionValue("is-low-mem", "true");
        assertEquals(RunStrategy.RUN, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testDeviceWithLowMemButIsLowMemFalse()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("true");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm64");
        setter.setOptionValue("is-low-mem", "false");
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
    }

    @Test
    public void testDeviceNotLowMemButIsLowMemFlagTrue()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("false");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product_hwasan");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm64");
        setter.setOptionValue("is-low-mem", "true");
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
    }

    @Test
    public void testDeviceWithHwasanAndIsHwasanFlagTure()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("false");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product_hwasan");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm64");
        setter.setOptionValue("is-hwasan", "true");
        assertEquals(RunStrategy.RUN, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testDeviceWithHwasanButIsHwasanFlagFalse()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("false");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product_hwasan");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm64");
        setter.setOptionValue("is-hwasan", "false");
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }

    @Test
    public void testDeviceNotHwasanButIsHwasanFlagTrue()
            throws DeviceNotAvailableException, ConfigurationException {
        mContext.addInvocationAttribute(ModuleDefinition.MODULE_ABI, "arm64-v8a");
        EasyMock.expect(mMockDevice.getIDevice()).andReturn(mMockIDevice).times(2);
        EasyMock.expect(mMockDevice.getProperty(mLowMemProp)).andReturn("false");
        EasyMock.expect(mMockDevice.getProperty(mProductNameProp)).andReturn("product");
        EasyMock.replay(mMockDevice);
        OptionSetter setter = new OptionSetter(mController);
        setter.setOptionValue("arch", "arm64");
        setter.setOptionValue("is-hwasan", "true");
        assertEquals(RunStrategy.FULL_MODULE_BYPASS, mController.shouldRunModule(mContext));
        EasyMock.verify(mMockDevice);
    }
}
