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
package com.android.tradefed.device.cloud;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import com.android.ddmlib.IDevice;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IDeviceMonitor;
import com.android.tradefed.device.IDeviceStateMonitor;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.IRunUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

/** Unit tests for {@link NestedRemoteDevice}. */
@RunWith(JUnit4.class)
public class NestedRemoteDeviceTest {

    private NestedRemoteDevice mDevice;
    private IDevice mMockIDevice;
    private IDeviceStateMonitor mMockStateMonitor;
    private IDeviceMonitor mMockMonitor;
    private IRunUtil mMockRunUtil;
    private ITestLogger mMockLogger;

    @Before
    public void setUp() throws Exception {
        mMockIDevice = Mockito.mock(IDevice.class);
        mMockStateMonitor = Mockito.mock(IDeviceStateMonitor.class);
        mMockMonitor = Mockito.mock(IDeviceMonitor.class);
        mMockRunUtil = Mockito.mock(IRunUtil.class);
        mMockLogger = Mockito.mock(ITestLogger.class);
        TestDeviceOptions options = new TestDeviceOptions();
        OptionSetter setter = new OptionSetter(options);
        setter.setOptionValue(TestDeviceOptions.INSTANCE_TYPE_OPTION, "CUTTLEFISH");
        mDevice =
                new NestedRemoteDevice(mMockIDevice, mMockStateMonitor, mMockMonitor) {
                    @Override
                    protected IRunUtil getRunUtil() {
                        return mMockRunUtil;
                    }

                    @Override
                    public TestDeviceOptions getOptions() {
                        return options;
                    }

                    @Override
                    public int getApiLevel() throws DeviceNotAvailableException {
                        return 23;
                    }
                };
    }

    @After
    public void tearDown() throws Exception {
        mDevice.postInvocationTearDown(null);
    }

    /** Test that reset device returns true in case of success */
    @Test
    public void testResetVirtualDevice() throws DeviceNotAvailableException {
        CommandResult stopCvdRes = new CommandResult(CommandStatus.SUCCESS);
        doReturn(stopCvdRes).when(mMockRunUtil).runTimedCmd(Mockito.anyLong(), Mockito.any());
        doReturn(mMockIDevice).when(mMockStateMonitor).waitForDeviceAvailable();

        assertTrue(mDevice.resetVirtualDevice(mMockLogger, new BuildInfo(), true));
    }
}
