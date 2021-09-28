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
package com.android.tradefed.testtype;

import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.android.helper.aoa.UsbDevice;
import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.TestDeviceState;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.util.IRunUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

/** Unit tests for {@link UsbResetTest}. */
@RunWith(JUnit4.class)
public class UsbResetTestTest {

    private UsbResetTest mTest;
    private TestInformation mTestInfo;
    private ITestDevice mDevice;
    private UsbHelper mUsb;

    @Before
    public void setUp() {
        mUsb = Mockito.mock(UsbHelper.class);
        mDevice = Mockito.mock(ITestDevice.class);
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice(ConfigurationDef.DEFAULT_DEVICE_NAME, mDevice);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
        mTest =
                new UsbResetTest() {
                    @Override
                    UsbHelper getUsbHelper() {
                        return mUsb;
                    }

                    @Override
                    IRunUtil getRunUtil() {
                        return Mockito.mock(IRunUtil.class);
                    }
                };
    }

    @Test
    public void testReset() throws DeviceNotAvailableException {
        UsbDevice usbDevice = Mockito.mock(UsbDevice.class);
        doReturn("serial").when(mDevice).getSerialNumber();
        doReturn(usbDevice).when(mUsb).getDevice("serial");
        mTest.run(mTestInfo, null);

        verify(usbDevice).reset();
        verify(mDevice).waitForDeviceOnline();
        verify(mDevice).reboot();
    }

    @Test
    public void testReset_recovery() throws DeviceNotAvailableException {
        UsbDevice usbDevice = Mockito.mock(UsbDevice.class);
        doReturn("serial").when(mDevice).getSerialNumber();
        doReturn(TestDeviceState.RECOVERY).when(mDevice).getDeviceState();
        doReturn(usbDevice).when(mUsb).getDevice("serial");
        mTest.run(mTestInfo, null);

        verify(usbDevice).reset();
        verify(mDevice, times(0)).waitForDeviceOnline();
        verify(mDevice).reboot();
    }

    @Test
    public void testReset_noDevice() throws DeviceNotAvailableException {
        doReturn("serial").when(mDevice).getSerialNumber();
        doReturn(null).when(mUsb).getDevice("serial");
        try {
            mTest.run(mTestInfo, null);
            fail("Should have thrown an exception");
        } catch (DeviceNotAvailableException expected) {
            // expected
        }

        verify(mDevice, never()).reboot();
    }
}
