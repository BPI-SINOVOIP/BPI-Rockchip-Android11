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
package com.android.tradefed.device.recovery;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.android.helper.aoa.UsbHelper;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.DeviceManager.FastbootDevice;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.FastbootHelper;
import com.android.tradefed.device.IManagedTestDevice;
import com.android.tradefed.device.StubDevice;

import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Sets;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Answers;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;

/** Unit tests for {@link UsbResetMultiDeviceRecovery}. */
@RunWith(JUnit4.class)
public class UsbResetMultiDeviceRecoveryTest {

    private static final String SERIAL = "SERIAL";

    private UsbResetMultiDeviceRecovery mRecoverer;

    private IManagedTestDevice mDevice;
    private FastbootHelper mFastboot;
    private UsbHelper mUsb;

    @Before
    public void setUp() {
        mDevice = mock(IManagedTestDevice.class);
        when(mDevice.getSerialNumber()).thenReturn(SERIAL);

        mFastboot = mock(FastbootHelper.class);
        when(mFastboot.getDevices()).thenReturn(new HashSet<>());

        mUsb = mock(UsbHelper.class, Answers.RETURNS_DEEP_STUBS);
        when(mUsb.getSerialNumbers(anyBoolean())).thenReturn(new HashSet<>());

        mRecoverer =
                new UsbResetMultiDeviceRecovery() {
                    @Override
                    FastbootHelper getFastbootHelper() {
                        return mFastboot;
                    }

                    @Override
                    UsbHelper getUsbHelper() {
                        return mUsb;
                    }
                };
    }

    @Test
    public void testRecover_stub() {
        // stub device
        when(mDevice.getIDevice()).thenReturn(new StubDevice(SERIAL));
        mRecoverer.recoverDevices(Arrays.asList(mDevice));

        // stub devices are ignored and not recovered
        verify(mUsb, never()).getDevice(any());
    }

    @Test
    public void testRecover_available() throws DeviceNotAvailableException {
        // connected and available device
        when(mUsb.getSerialNumbers(anyBoolean())).thenReturn(Sets.newHashSet(SERIAL));
        when(mDevice.getAllocationState()).thenReturn(DeviceAllocationState.Available);

        mRecoverer.recoverDevices(Arrays.asList(mDevice));

        // device is in a valid state and not recovered
        verify(mUsb.getDevice(SERIAL), never()).reset();
        verify(mDevice, never()).reboot();
    }

    @Test
    public void testRecover_unavailable() throws DeviceNotAvailableException {
        // connected but unavailable device
        when(mUsb.getSerialNumbers(anyBoolean())).thenReturn(Sets.newHashSet(SERIAL));
        when(mDevice.getAllocationState()).thenReturn(DeviceAllocationState.Unavailable);

        mRecoverer.recoverDevices(Arrays.asList(mDevice));

        // device is in an invalid state and reset/reboot is performed
        verify(mUsb.getDevice(SERIAL), times(1)).reset();
        verify(mDevice, times(1)).reboot();
    }

    @Test
    public void testRecover_fastboot_allocated() throws DeviceNotAvailableException {
        // allocated fastboot device
        when(mFastboot.getDevices()).thenReturn(Sets.newHashSet(SERIAL));
        when(mDevice.getIDevice()).thenReturn(new FastbootDevice(SERIAL));
        when(mDevice.getAllocationState()).thenReturn(DeviceAllocationState.Allocated);

        mRecoverer.recoverDevices(Arrays.asList(mDevice));

        // device is in a valid state and not recovered
        verify(mUsb.getDevice(SERIAL), never()).reset();
        verify(mDevice, never()).reboot();
    }

    @Test
    public void testRecover_fastboot_unallocated() throws DeviceNotAvailableException {
        // non-allocated fastboot device
        when(mFastboot.getDevices()).thenReturn(ImmutableSet.of(SERIAL));
        when(mDevice.getIDevice()).thenReturn(new FastbootDevice(SERIAL));
        when(mDevice.getAllocationState()).thenReturn(DeviceAllocationState.Ignored);

        mRecoverer.recoverDevices(Arrays.asList(mDevice));

        // device is in an invalid state and reset/reboot is performed
        verify(mUsb.getDevice(SERIAL), times(1)).reset();
        verify(mDevice, times(1)).reboot();
    }

    @Test
    public void testRecover_notManaged() throws DeviceNotAvailableException {
        // connected device found, but no managed devices
        when(mUsb.getSerialNumbers(anyBoolean())).thenReturn(Sets.newHashSet(SERIAL));

        mRecoverer.recoverDevices(new ArrayList<>());

        // reset still performed on connected device, but no reboot
        verify(mUsb.getDevice(SERIAL), times(1)).reset();
        verify(mDevice, never()).reboot();
    }

    @Test
    public void testRecover_notConnected() throws DeviceNotAvailableException {
        // no connected devices found, but one managed device in an unknown state
        when(mUsb.getSerialNumbers(anyBoolean())).thenReturn(new HashSet<>());
        when(mDevice.getAllocationState()).thenReturn(DeviceAllocationState.Unknown);

        mRecoverer.recoverDevices(Arrays.asList(mDevice));

        // device not found and not recovered
        verify(mUsb.getDevice(SERIAL), never()).reset();
        verify(mDevice, never()).reboot();
    }
}
