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
package com.android.helper.aoa;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyByte;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyShort;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/** Unit tests for {@link UsbDevice} */
@RunWith(JUnit4.class)
public class UsbDeviceTest {

    private UsbDevice mDevice;

    private Pointer mHandle;
    private IUsbNative mUsb;

    @Before
    public void setUp() {
        // create dummy device handle
        mHandle = new Memory(1);

        mUsb = mock(IUsbNative.class);
        // return device handle when opening connection
        when(mUsb.libusb_open(any(), any()))
                .then(
                        invocation -> {
                            PointerByReference handle =
                                    (PointerByReference) invocation.getArguments()[1];
                            handle.setValue(mHandle);
                            return 0;
                        });

        mDevice = new UsbDevice(mUsb, mock(Pointer.class));
    }

    @Test
    public void testIsValid() {
        // has device handle initially
        assertTrue(mDevice.isValid());

        // device handle invalid after closing connection
        mDevice.close();
        assertFalse(mDevice.isValid());
    }

    @Test
    public void testControlTransfer() {
        byte[] data = new byte[]{1, 2, 3, 4};
        mDevice.controlTransfer((byte) 1, (byte) 2, 3, 4, data);

        // passes right arguments to libusb
        verify(mUsb).libusb_control_transfer(eq(mHandle),
                eq((byte) 1), eq((byte) 2), eq((short) 3), eq((short) 4),
                eq(data), eq((short) 4), // data and length
                eq(0)); // timeout
    }

    @Test(expected = NullPointerException.class)
    public void testControlTransfer_closed() {
        mDevice.close();
        // trying to operate on a closed handle should fail
        mDevice.controlTransfer((byte) 1, (byte) 2, 3, 4, new byte[0]);
    }

    @Test
    public void testReset() {
        mDevice.reset();
        verify(mUsb).libusb_reset_device(eq(mHandle));
    }

    @Test(expected = NullPointerException.class)
    public void testReset_closed() {
        mDevice.close();
        // trying to operate on a closed handle should fail
        mDevice.reset();
    }

    @Test
    public void testIsAoaCompatible() {
        // supports AOA protocol version 2
        when(mUsb.libusb_control_transfer(
                        eq(mHandle),
                        anyByte(),
                        anyByte(),
                        anyShort(),
                        anyShort(),
                        any(),
                        anyShort(),
                        anyInt()))
                .thenReturn(2);
        assertTrue(mDevice.isAoaCompatible());

        // does not support any AOA protocol
        when(mUsb.libusb_control_transfer(
                        eq(mHandle),
                        anyByte(),
                        anyByte(),
                        anyShort(),
                        anyShort(),
                        any(),
                        anyShort(),
                        anyInt()))
                .thenReturn(-1);
        assertFalse(mDevice.isAoaCompatible());
    }
}
