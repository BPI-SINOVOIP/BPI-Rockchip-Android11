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

import static com.android.helper.aoa.AoaDevice.GOOGLE_VID;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import com.google.common.collect.Sets;
import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashSet;

/** Unit tests for {@link UsbHelper} */
@RunWith(JUnit4.class)
public class UsbHelperTest {

    private static final String SERIAL_NUMBER = "serial-number";
    private static final int ACCESSORY_PID = 0x2D00;

    private UsbHelper mHelper;

    private IUsbNative mUsb;
    private UsbDevice mDevice;

    @Before
    public void setUp() {
        // create dummy pointer
        Pointer pointer = new Memory(1);

        mUsb = mock(IUsbNative.class);
        // populate context when initialized
        when(mUsb.libusb_init(any()))
                .then(
                        invocation -> {
                            PointerByReference context =
                                    (PointerByReference) invocation.getArguments()[0];
                            context.setValue(pointer);
                            return null;
                        });
        // find device pointer when listing devices
        when(mUsb.libusb_get_device_list(any(), any()))
                .then(
                        invocation -> {
                            PointerByReference list =
                                    (PointerByReference) invocation.getArguments()[1];
                            list.setValue(pointer);
                            return 1;
                        });

        // device is valid, has right serial number, and is in accessory mode by default
        mDevice = mock(UsbDevice.class);
        when(mDevice.isValid()).thenReturn(true);
        when(mDevice.getSerialNumber()).thenReturn(SERIAL_NUMBER);
        when(mDevice.getVendorId()).thenReturn(GOOGLE_VID);
        when(mDevice.getProductId()).thenReturn(ACCESSORY_PID);

        mHelper = spy(new UsbHelper(mUsb));
        // always return the mocked device
        doReturn(mDevice).when(mHelper).connect(any());
    }

    @Test
    public void testContext() {
        // initialized on creation
        verify(mUsb, times(1)).libusb_init(any());

        // exited on close
        mHelper.close();
        verify(mUsb, times(1)).libusb_exit(any());
    }

    @Test
    public void testCheckResult() {
        // non-negative numbers are always valid
        assertEquals(0, mHelper.checkResult(0));
        assertEquals(1, mHelper.checkResult(1));
        assertEquals(Integer.MAX_VALUE, mHelper.checkResult(Integer.MAX_VALUE));
    }

    @Test(expected = UsbException.class)
    public void testCheckResult_invalid() {
        // negative numbers indicate errors
        mHelper.checkResult(-1);
    }

    @Test
    public void testGetSerialNumbers() {
        assertEquals(Sets.newHashSet(SERIAL_NUMBER), mHelper.getSerialNumbers(false));

        // device list and device were closed
        verify(mDevice, times(1)).close();
        verify(mUsb, times(1)).libusb_free_device_list(any(), eq(true));
    }

    @Test
    public void testGetSerialNumbers_aoaOnly() {
        when(mDevice.isAoaCompatible()).thenReturn(false);
        assertEquals(Sets.newHashSet(SERIAL_NUMBER), mHelper.getSerialNumbers(false));
        assertEquals(new HashSet<>(), mHelper.getSerialNumbers(true));

        when(mDevice.isAoaCompatible()).thenReturn(true);
        assertEquals(Sets.newHashSet(SERIAL_NUMBER), mHelper.getSerialNumbers(false));
        assertEquals(Sets.newHashSet(SERIAL_NUMBER), mHelper.getSerialNumbers(true));
    }

    @Test
    public void testGetDevice() {
        // valid connection was found and opened
        assertEquals(mDevice, mHelper.getDevice(SERIAL_NUMBER));

        // device list was closed, but not connection
        verify(mDevice, never()).close();
        verify(mUsb, times(1)).libusb_free_device_list(any(), eq(true));
    }

    @Test
    public void testGetDevice_invalid() {
        when(mDevice.getSerialNumber()).thenReturn(null);

        // valid device not found
        assertNull(mHelper.getDevice(SERIAL_NUMBER));

        // connection and device list were closed
        verify(mDevice, times(1)).close();
        verify(mUsb, times(1)).libusb_free_device_list(any(), eq(true));
    }

    @Test
    public void testGetAoaDevice() {
        when(mDevice.isAoaCompatible()).thenReturn(true);
        assertNotNull(mHelper.getAoaDevice(SERIAL_NUMBER));
    }

    @Test
    public void testGetAoaDevice_incompatible() {
        when(mDevice.isAoaCompatible()).thenReturn(false);
        assertNull(mHelper.getAoaDevice(SERIAL_NUMBER));
    }
}
