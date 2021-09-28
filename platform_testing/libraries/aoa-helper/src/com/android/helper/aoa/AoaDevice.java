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

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Range;
import com.google.common.primitives.Bytes;
import com.google.common.util.concurrent.Uninterruptibles;

import java.awt.Point;
import java.time.Duration;
import java.time.Instant;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nonnull;

/**
 * USB connected AOAv2-compatible Android device.
 *
 * <p>This host-side utility can be used to send commands (e.g. clicks, swipes, keystrokes, and
 * more) to a connected device without the need for ADB.
 *
 * @see <a href="https://source.android.com/devices/accessories/aoa2">Android Open Accessory
 *     Protocol 2.0</a>
 */
public class AoaDevice implements AutoCloseable {

    // USB error code
    static final int DEVICE_NOT_FOUND = -4;

    // USB request types (direction and vendor type)
    static final byte INPUT = (byte) (0x80 | (0x02 << 5));
    static final byte OUTPUT = (byte) (0x00 | (0x02 << 5));

    // AOA VID and PID
    static final int GOOGLE_VID = 0x18D1;
    private static final Range<Integer> AOA_PID = Range.closed(0x2D00, 0x2D05);
    private static final ImmutableSet<Integer> ADB_PID = ImmutableSet.of(0x2D01, 0x2D03, 0x2D05);

    // AOA requests
    static final byte ACCESSORY_GET_PROTOCOL = 51;
    static final byte ACCESSORY_START = 53;
    static final byte ACCESSORY_REGISTER_HID = 54;
    static final byte ACCESSORY_UNREGISTER_HID = 55;
    static final byte ACCESSORY_SET_HID_REPORT_DESC = 56;
    static final byte ACCESSORY_SEND_HID_EVENT = 57;

    // Maximum attempts at restarting in accessory mode
    static final int ACCESSORY_START_MAX_RETRIES = 5;

    // Touch types
    static final byte TOUCH_UP = 0b00;
    static final byte TOUCH_DOWN = 0b11;

    // System buttons
    static final byte SYSTEM_WAKE = 0b001;
    static final byte SYSTEM_HOME = 0b010;
    static final byte SYSTEM_BACK = 0b100;

    // Durations and steps
    private static final Duration CONNECTION_TIMEOUT = Duration.ofSeconds(10L);
    private static final Duration CONFIGURE_DELAY = Duration.ofSeconds(1L);
    private static final Duration ACTION_DELAY = Duration.ofSeconds(3L);
    private static final Duration STEP_DELAY = Duration.ofMillis(10L);
    static final Duration LONG_CLICK = Duration.ofSeconds(1L);

    private final UsbHelper mHelper;
    private UsbDevice mDelegate;
    private String mSerialNumber;

    AoaDevice(@Nonnull UsbHelper helper, @Nonnull UsbDevice delegate) {
        mHelper = helper;
        mDelegate = delegate;
        initialize(0);
    }

    // Configure the device, switching to accessory mode if necessary and registering the HIDs
    private void initialize(int attempt) {
        if (!isValid()) {
            throw new UsbException("Invalid device connection");
        }

        mSerialNumber = mDelegate.getSerialNumber();
        if (mSerialNumber == null) {
            throw new UsbException("Missing serial number");
        }

        if (isAccessoryMode()) {
            registerHIDs();
        } else if (attempt >= ACCESSORY_START_MAX_RETRIES) {
            throw new UsbException("Failed to start accessory mode");
        } else {
            // restart in accessory mode and try to initialize again
            mHelper.checkResult(
                    mDelegate.controlTransfer(OUTPUT, ACCESSORY_START, 0, 0, new byte[0]));
            sleep(CONFIGURE_DELAY);
            mDelegate.close();
            mDelegate = mHelper.getDevice(mSerialNumber, CONNECTION_TIMEOUT);
            initialize(attempt + 1);
        }
    }

    // Register HIDs
    private void registerHIDs() {
        for (HID hid : HID.values()) {
            // register HID identifier
            mHelper.checkResult(
                    mDelegate.controlTransfer(
                            OUTPUT,
                            ACCESSORY_REGISTER_HID,
                            hid.getId(),
                            hid.getDescriptor().length,
                            new byte[0]));
            // register HID descriptor
            mHelper.checkResult(
                    mDelegate.controlTransfer(
                            OUTPUT,
                            ACCESSORY_SET_HID_REPORT_DESC,
                            hid.getId(),
                            0,
                            hid.getDescriptor()));
        }
        sleep(CONFIGURE_DELAY);
    }

    // Unregister HIDs
    private void unregisterHIDs() {
        for (HID hid : HID.values()) {
            mDelegate.controlTransfer(
                    OUTPUT, ACCESSORY_UNREGISTER_HID, hid.getId(), 0, new byte[0]);
        }
    }

    /**
     * Close and re-fetch the connection. This is necessary after the USB connection has been reset,
     * e.g. when toggling accessory mode or USB debugging.
     */
    public void resetConnection() {
        close();
        mDelegate = mHelper.getDevice(mSerialNumber, CONNECTION_TIMEOUT);
        initialize(0);
    }

    /** @return true if connection is non-null, but does not check if resetting is necessary */
    public boolean isValid() {
        return mDelegate != null && mDelegate.isValid();
    }

    /** @return device's serial number */
    @Nonnull
    public String getSerialNumber() {
        return mSerialNumber;
    }

    // Checks whether the device is in accessory mode
    private boolean isAccessoryMode() {
        return GOOGLE_VID == mDelegate.getVendorId()
                && AOA_PID.contains(mDelegate.getProductId());
    }

    /** @return true if device has USB debugging enabled */
    public boolean isAdbEnabled() {
        return GOOGLE_VID == mDelegate.getVendorId()
                && ADB_PID.contains(mDelegate.getProductId());
    }

    /** Get current time. */
    @VisibleForTesting
    Instant now() {
        return Instant.now();
    }

    /** Wait for a specified duration. */
    public void sleep(@Nonnull Duration duration) {
        Uninterruptibles.sleepUninterruptibly(duration.toNanos(), TimeUnit.NANOSECONDS);
    }

    /** Perform a click. */
    public void click(@Nonnull Point point) {
        click(point, Duration.ZERO);
    }

    /** Perform a long click. */
    public void longClick(@Nonnull Point point) {
        click(point, LONG_CLICK);
    }

    // Click and wait at a location.
    private void click(Point point, Duration duration) {
        touch(TOUCH_DOWN, point, duration);
        touch(TOUCH_UP, point, ACTION_DELAY);
    }

    /**
     * Swipe from one position to another in the specified duration.
     *
     * @param from starting position
     * @param to final position
     * @param duration swipe motion duration
     */
    public void swipe(@Nonnull Point from, @Nonnull Point to, @Nonnull Duration duration) {
        Instant start = now();
        touch(TOUCH_DOWN, from, STEP_DELAY);
        while (true) {
            Duration elapsed = Duration.between(start, now());
            if (duration.compareTo(elapsed) < 0) {
                break;
            }
            double progress = (double) elapsed.toMillis() / duration.toMillis();
            Point point =
                    new Point(
                            (int) (progress * to.x + (1 - progress) * from.x),
                            (int) (progress * to.y + (1 - progress) * from.y));
            touch(TOUCH_DOWN, point, STEP_DELAY);
        }
        touch(TOUCH_UP, to, ACTION_DELAY);
    }

    // Send a touch event to the device
    private void touch(byte type, Point point, Duration pause) {
        int x = Math.min(Math.max(point.x, 0), 360);
        int y = Math.min(Math.max(point.y, 0), 640);
        byte[] data = new byte[] {type, (byte) x, (byte) (x >> 8), (byte) y, (byte) (y >> 8)};
        send(HID.TOUCH_SCREEN, data, pause);
    }

    /**
     * Press a combination of keys.
     *
     * @param keys key HID usages, see <a
     *     https://source.android.com/devices/input/keyboard-devices">Keyboard devices</a>
     */
    public void pressKeys(Integer... keys) {
        pressKeys(Arrays.asList(keys));
    }

    /**
     * Press a combination of keys.
     *
     * @param keys list of key HID usages, see <a
     *     https://source.android.com/devices/input/keyboard-devices">Keyboard devices</a>
     */
    public void pressKeys(@Nonnull List<Integer> keys) {
        Iterator<Integer> it = keys.stream().filter(Objects::nonNull).iterator();
        while (it.hasNext()) {
            Integer keyCode = it.next();
            send(HID.KEYBOARD, new byte[] {keyCode.byteValue()}, STEP_DELAY);
            send(HID.KEYBOARD, new byte[] {(byte) 0}, it.hasNext() ? STEP_DELAY : ACTION_DELAY);
        }
    }

    /** Wake up the device if it is sleeping. */
    public void wakeUp() {
        send(AoaDevice.HID.SYSTEM, new byte[] {SYSTEM_WAKE}, ACTION_DELAY);
    }

    /** Press the device's home button. */
    public void goHome() {
        send(AoaDevice.HID.SYSTEM, new byte[] {SYSTEM_HOME}, ACTION_DELAY);
    }

    /** Press the device's back button. */
    public void goBack() {
        send(AoaDevice.HID.SYSTEM, new byte[] {SYSTEM_BACK}, ACTION_DELAY);
    }

    // Send a HID event to the device
    private void send(HID hid, byte[] data, Duration pause) {
        int result =
                mDelegate.controlTransfer(OUTPUT, ACCESSORY_SEND_HID_EVENT, hid.getId(), 0, data);
        if (result == DEVICE_NOT_FOUND) {
            // device not found, reset the connection and retry
            resetConnection();
            result =
                    mDelegate.controlTransfer(
                            OUTPUT, ACCESSORY_SEND_HID_EVENT, hid.getId(), 0, data);
        }
        mHelper.checkResult(result);
        sleep(pause);
    }

    /** Close the device connection. */
    @Override
    public void close() {
        if (isValid()) {
            if (isAccessoryMode()) {
                unregisterHIDs();
            }
            mDelegate.close();
            mDelegate = null;
        }
    }

    /**
     * Human interface device descriptors.
     *
     * @see <a href="https://www.usb.org/hid">USB HID information</a>
     */
    @VisibleForTesting
    enum HID {
        /** 360 x 640 touch screen: 6-bit padding, 2-bit type, 16-bit X coord., 16-bit Y coord. */
        TOUCH_SCREEN(
                new Integer[] {
                    0x05, 0x0D, //      Usage Page (Digitizer)
                    0x09, 0x04, //      Usage (Touch Screen)
                    0xA1, 0x01, //      Collection (Application)
                    0x09, 0x32, //          Usage (In Range) - proximity to screen
                    0x09, 0x33, //          Usage (Touch) - contact with screen
                    0x15, 0x00, //          Logical Minimum (0)
                    0x25, 0x01, //          Logical Maximum (1)
                    0x75, 0x01, //          Report Size (1)
                    0x95, 0x02, //          Report Count (2)
                    0x81, 0x02, //          Input (Data, Variable, Absolute)
                    0x75, 0x01, //          Report Size (1)
                    0x95, 0x06, //          Report Count (6) - padding
                    0x81, 0x01, //          Input (Constant)
                    0x05, 0x01, //          Usage Page (Generic)
                    0x09, 0x30, //          Usage (X)
                    0x15, 0x00, //          Logical Minimum (0)
                    0x26, 0x68, 0x01, //    Logical Maximum (360)
                    0x75, 0x10, //          Report Size (16)
                    0x95, 0x01, //          Report Count (1)
                    0x81, 0x02, //          Input (Data, Variable, Absolute)
                    0x09, 0x31, //          Usage (Y)
                    0x15, 0x00, //          Logical Minimum (0)
                    0x26, 0x80, 0x02, //    Logical Maximum (640)
                    0x75, 0x10, //          Report Size (16)
                    0x95, 0x01, //          Report Count (1)
                    0x81, 0x02, //          Input (Data, Variable, Absolute)
                    0xC0, //            End Collection
                }),

        /** 101-key keyboard: 8-bit keycode. */
        KEYBOARD(
                new Integer[] {
                    0x05, 0x01, //      Usage Page (Generic)
                    0x09, 0x06, //      Usage (Keyboard)
                    0xA1, 0x01, //      Collection (Application)
                    0x05, 0x07, //          Usage Page (Key Codes)
                    0x19, 0x00, //          Usage Minimum (0)
                    0x29, 0x65, //          Usage Maximum (101)
                    0x15, 0x00, //          Logical Minimum (0)
                    0x25, 0x65, //          Logical Maximum (101)
                    0x75, 0x08, //          Report Size (8)
                    0x95, 0x01, //          Report Count (1)
                    0x81, 0x00, //          Input (Data, Array, Absolute)
                    0xC0, //            End Collection
                }),

        /** System buttons: 5-bit padding, 3-bit flags (wake, home, back). */
        SYSTEM(
                new Integer[] {
                    0x05, 0x01, //      Usage Page (Generic)
                    0x09, 0x80, //      Usage (System Control)
                    0xA1, 0x01, //      Collection (Application)
                    0x15, 0x00, //          Logical Minimum (0)
                    0x25, 0x01, //          Logical Maximum (1)
                    0x75, 0x01, //          Report Size (1)
                    0x95, 0x01, //          Report Count (1)
                    0x09, 0x83, //          Usage (Wake)
                    0x81, 0x06, //          Input (Data, Variable, Relative)
                    0xC0, //            End Collection
                    0x05, 0x0C, //      Usage Page (Consumer)
                    0x09, 0x01, //      Usage (Consumer Control)
                    0xA1, 0x01, //      Collection (Application)
                    0x15, 0x00, //          Logical Minimum (0)
                    0x25, 0x01, //          Logical Maximum (1)
                    0x75, 0x01, //          Report Size (1)
                    0x95, 0x01, //          Report Count (1)
                    0x0A, 0x23, 0x02, //    Usage (Home)
                    0x81, 0x06, //          Input (Data, Variable, Relative)
                    0x0A, 0x24, 0x02, //    Usage (Back)
                    0x81, 0x06, //          Input (Data, Variable, Relative)
                    0x75, 0x01, //          Report Size (1)
                    0x95, 0x05, //          Report Count (5) - padding
                    0x81, 0x01, //          Input (Constant)
                    0xC0, //            End Collection
                });

        private final ImmutableList<Integer> mDescriptor;

        HID(Integer[] descriptor) {
            mDescriptor = ImmutableList.copyOf(descriptor);
        }

        int getId() {
            return ordinal();
        }

        byte[] getDescriptor() {
            return Bytes.toArray(mDescriptor);
        }
    }
}
