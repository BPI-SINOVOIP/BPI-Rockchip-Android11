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

import static com.google.common.base.Preconditions.checkNotNull;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.util.concurrent.Uninterruptibles;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;

import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Iterator;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * USB and AOAv2 device manager, from which devices are retrieved.
 *
 * @see <a href="https://source.android.com/devices/accessories/aoa2">Android Open Accessory
 *     Protocol 2.0</a>
 */
public class UsbHelper implements AutoCloseable {

    private static final Duration POLL_INTERVAL = Duration.ofSeconds(1L);

    private final IUsbNative mUsb;
    private Pointer mContext;

    public UsbHelper() {
        this((IUsbNative) Native.loadLibrary("usb-1.0", IUsbNative.class));
    }

    @VisibleForTesting
    UsbHelper(@Nonnull IUsbNative usb) {
        mUsb = usb;
        // initialize context
        PointerByReference context = new PointerByReference();
        checkResult(mUsb.libusb_init(context));
        mContext = context.getValue();
    }

    /**
     * Verifies a USB response, throwing an exception if it corresponds to an error.
     *
     * @param value result or error code
     */
    public int checkResult(int value) {
        if (value < 0) {
            throw new UsbException(mUsb.libusb_strerror(value));
        }
        return value;
    }

    /**
     * Find all connected device serial numbers.
     *
     * @param aoaOnly whether to only include AOAv2-compatible devices
     * @return USB device serial numbers
     */
    @Nonnull
    public Set<String> getSerialNumbers(boolean aoaOnly) {
        Set<String> serialNumbers = new HashSet<>();

        try (DeviceList list = new DeviceList()) {
            for (Pointer devicePointer : list) {
                // add all valid serial numbers
                try (UsbDevice device = connect(devicePointer)) {
                    String serialNumber = device.getSerialNumber();
                    if (serialNumber != null && (!aoaOnly || device.isAoaCompatible())) {
                        serialNumbers.add(serialNumber);
                    }
                }
            }
        }

        return serialNumbers;
    }

    /**
     * Find a USB device using its serial number.
     *
     * @param serialNumber device serial number
     * @return USB device or {@code null} if not found
     */
    @Nullable
    public UsbDevice getDevice(@Nonnull String serialNumber) {
        try (DeviceList list = new DeviceList()) {
            for (Pointer devicePointer : list) {
                // check if device has the right serial number
                UsbDevice device = connect(devicePointer);
                if (serialNumber.equals(device.getSerialNumber())) {
                    return device;
                }
                device.close();
            }
        }

        // device not found
        return null;
    }

    @VisibleForTesting
    UsbDevice connect(@Nonnull Pointer devicePointer) {
        return new UsbDevice(mUsb, devicePointer);
    }

    /**
     * Wait for a USB device using its serial number.
     *
     * @param serialNumber device serial number
     * @param timeout maximum time to wait for
     * @return USB device or {@code null} if not found
     */
    @Nullable
    public UsbDevice getDevice(@Nonnull String serialNumber, @Nonnull Duration timeout) {
        Instant start = Instant.now();
        UsbDevice device = getDevice(serialNumber);

        while (device == null && timeout.compareTo(Duration.between(start, Instant.now())) > 0) {
            Uninterruptibles.sleepUninterruptibly(POLL_INTERVAL.toNanos(), TimeUnit.NANOSECONDS);
            device = getDevice(serialNumber);
        }

        return device;
    }

    /**
     * Find an AOAv2-compatible device using its serial number.
     *
     * @param serialNumber device serial number
     * @return AOAv2-compatible device or {@code null} if not found
     */
    @Nullable
    public AoaDevice getAoaDevice(@Nonnull String serialNumber) {
        return getAoaDevice(serialNumber, Duration.ZERO);
    }

    /**
     * Wait for an AOAv2-compatible device using its serial number.
     *
     * @param serialNumber device serial number
     * @param timeout maximum time to wait for
     * @return AOAv2-compatible device or {@code null} if not found
     */
    @Nullable
    public AoaDevice getAoaDevice(@Nonnull String serialNumber, @Nonnull Duration timeout) {
        UsbDevice device = getDevice(serialNumber, timeout);
        return device != null && device.isAoaCompatible() ? new AoaDevice(this, device) : null;
    }

    /** De-initialize the USB context. */
    @Override
    public void close() {
        if (mContext != null) {
            mUsb.libusb_exit(mContext);
            mContext = null;
        }
    }

    /** USB device pointer list. */
    private class DeviceList implements Iterable<Pointer>, AutoCloseable {

        private final Pointer list;
        private final int count;

        private DeviceList() {
            PointerByReference list = new PointerByReference();
            this.count = checkResult(mUsb.libusb_get_device_list(checkNotNull(mContext), list));
            this.list = list.getValue();
        }

        @Override
        @Nonnull
        public Iterator<Pointer> iterator() {
            Pointer[] devices = list.getPointerArray(0, count);
            return Arrays.stream(devices).iterator();
        }

        @Override
        public void close() {
            mUsb.libusb_free_device_list(list, true);
        }
    }
}
