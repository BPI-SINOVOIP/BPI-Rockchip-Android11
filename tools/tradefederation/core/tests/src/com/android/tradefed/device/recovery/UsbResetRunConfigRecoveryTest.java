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
package com.android.tradefed.device.recovery;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.IManagedTestDevice;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

/** Unit tests for {@link UsbResetRunConfigRecovery}. */
@RunWith(JUnit4.class)
public class UsbResetRunConfigRecoveryTest {

    private UsbResetRunConfigRecovery mUsbReset = new UsbResetRunConfigRecovery();

    /**
     * Test that should skip can only return false once, it should attempt the usb reset twice in a
     * row for the same device.
     */
    @Test
    public void testShouldSkip() {
        IManagedTestDevice mockDevice = Mockito.mock(IManagedTestDevice.class);
        doReturn("serial").when(mockDevice).getSerialNumber();
        doReturn(false).when(mockDevice).isStateBootloaderOrFastbootd();
        boolean res = mUsbReset.shouldSkip(mockDevice);
        assertFalse(res);
        res = mUsbReset.shouldSkip(mockDevice);
        assertTrue(res);
        res = mUsbReset.shouldSkip(mockDevice);
        assertFalse(res);
    }

    @Test
    public void testShouldSkip_fastboot() {
        IManagedTestDevice mockDevice = Mockito.mock(IManagedTestDevice.class);
        doReturn("serial").when(mockDevice).getSerialNumber();
        doReturn(true).when(mockDevice).isStateBootloaderOrFastbootd();
        assertTrue(mUsbReset.shouldSkip(mockDevice));
    }

    @Test
    public void testShouldSkip_available() {
        IManagedTestDevice mockDevice = Mockito.mock(IManagedTestDevice.class);
        doReturn("serial").when(mockDevice).getSerialNumber();
        doReturn(false).when(mockDevice).isStateBootloaderOrFastbootd();
        doReturn(DeviceAllocationState.Available).when(mockDevice).getAllocationState();
        assertTrue(mUsbReset.shouldSkip(mockDevice));
    }
}
