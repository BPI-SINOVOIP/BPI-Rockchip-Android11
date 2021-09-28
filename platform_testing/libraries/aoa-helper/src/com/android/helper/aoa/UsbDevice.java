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

import static com.android.helper.aoa.AoaDevice.ACCESSORY_GET_PROTOCOL;
import static com.android.helper.aoa.AoaDevice.INPUT;

import static com.google.common.base.Preconditions.checkNotNull;

import com.google.common.primitives.Shorts;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/** Connected USB device. */
public class UsbDevice implements AutoCloseable {

    private final IUsbNative mUsb;
    private final byte[] mDescriptor = new byte[18];
    private Pointer mHandle;

    UsbDevice(@Nonnull IUsbNative usb, @Nonnull Pointer devicePointer) {
        mUsb = usb;

        // retrieve device descriptor
        mUsb.libusb_get_device_descriptor(devicePointer, mDescriptor);

        // obtain device handle
        PointerByReference handle = new PointerByReference();
        mUsb.libusb_open(devicePointer, handle);
        mHandle = handle.getValue();
    }

    /**
     * Performs a synchronous control transaction with unlimited timeout.
     *
     * @return number of bytes transferred, or an error code
     */
    public int controlTransfer(byte requestType, byte request, int value, int index, byte[] data) {
        return mUsb.libusb_control_transfer(
                checkNotNull(mHandle),
                requestType,
                request,
                (short) value,
                (short) index,
                data,
                (short) data.length,
                0);
    }

    /**
     * Performs a USB port reset. A LIBUSB_ERROR_NOT_FOUND error may indicate that the connection
     * was reset, but that this {@link UsbDevice} is no longer valid and needs to be recreated.
     *
     * @return 0 on success or error code
     */
    public int reset() {
        return mUsb.libusb_reset_device(checkNotNull(mHandle));
    }

    /** @return true if device handle is non-null, but does not check if resetting is necessary */
    public boolean isValid() {
        return mHandle != null;
    }

    /** @return device's serial number or {@code null} if serial could not be determined */
    @Nullable
    public String getSerialNumber() {
        if (!isValid() || mDescriptor[16] <= 0) {
            // no device handle or string index is invalid
            return null;
        }

        byte[] data = new byte[64];
        int length = mUsb.libusb_get_string_descriptor_ascii(mHandle, mDescriptor[16], data, 64);
        return length > 0 ? new String(data, 0, length) : null;
    }

    /** @return device's vendor ID */
    public int getVendorId() {
        return Shorts.fromBytes(mDescriptor[9], mDescriptor[8]);
    }

    /** @return device's product ID */
    public int getProductId() {
        return Shorts.fromBytes(mDescriptor[11], mDescriptor[10]);
    }

    /** @return true if device is AOAv2-compatible */
    public boolean isAoaCompatible() {
        return isValid() && controlTransfer(INPUT, ACCESSORY_GET_PROTOCOL, 0, 0, new byte[2]) >= 2;
    }

    /** Close the connection if necessary. */
    @Override
    public void close() {
        if (isValid()) {
            mUsb.libusb_close(mHandle);
            mHandle = null;
        }
    }
}
