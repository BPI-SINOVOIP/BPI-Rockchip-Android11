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

package com.android.tradefed.module;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.*;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.testtype.suite.module.IModuleController;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.InjectMocks;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.List;
/**
 * Unit tests for {@link VtsHalAdapterModuleController).
 */
@RunWith(JUnit4.class)
public final class VtsHalAdapterModuleControllerTest {
    private String LIST_HAL_CMD = VtsHalAdapterModuleController.LIST_HAL_CMD;
    private String TEST_HAL_PACKAGE = "android.hardware.foo@1.1";

    private List<ITestDevice> mDevices = new ArrayList<ITestDevice>();

    @Mock private IInvocationContext mInvocationContext;
    @Mock private ITestDevice mDevice;
    @InjectMocks
    private VtsHalAdapterModuleController mModuleControler = new VtsHalAdapterModuleController();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mDevices.add(mDevice);
        doReturn(mDevices).when(mInvocationContext).getDevices();
        OptionSetter setter = new OptionSetter(mModuleControler);
        setter.setOptionValue("hal-package-name", TEST_HAL_PACKAGE);
    }

    // Test the normal case when the targeting HAL is available.
    @Test
    public void testShouldRun() throws Exception {
        String output = "android.hardware.foo@1.1::IFoo/default";
        doReturn(output).when(mDevice).executeShellCommand(
                String.format(LIST_HAL_CMD, TEST_HAL_PACKAGE));
        assertEquals(
                IModuleController.RunStrategy.RUN, mModuleControler.shouldRun(mInvocationContext));
    }

    // Test the case when we need to skip the test as the targeting HAL is not available.
    @Test
    public void testShouldSkip() throws Exception {
        doReturn("").when(mDevice).executeShellCommand(
                String.format(LIST_HAL_CMD, TEST_HAL_PACKAGE));
        assertEquals(IModuleController.RunStrategy.SKIP_MODULE_TESTCASES,
                mModuleControler.shouldRun(mInvocationContext));
    }
}
