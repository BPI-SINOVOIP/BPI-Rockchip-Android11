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
package com.android.tests.usbgadget.libusb;

import com.sun.jna.Library;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;

/** JNA adapter for <a href="https://libusb.info">libusb</a>. */
public interface IUsbNative extends Library {
    /**
     * Initialize libusb, must be called before calling any other function.
     *
     * @param context output location for context pointer
     * @return 0 on success, or an error code
     */
    int libusb_init(PointerByReference context);

    /**
     * Deinitialize libusb.
     *
     * @param ctx context to deinitialize
     */
    void libusb_exit(Pointer ctx);

    /**
     * Returns a list of USB devices currently attached to the system.
     *
     * @param ctx context to operate on
     * @param list output location for a list of devices
     * @return number of devices, or an error code
     */
    int libusb_get_device_list(Pointer ctx, PointerByReference list);

    /**
     * Get the USB device descriptor for a given device.
     *
     * @param dev device
     * @param desc output location for the descriptor data
     * @return 0 on success, or an error code
     */
    int libusb_get_device_descriptor(Pointer dev, DeviceDescriptor[] desc);

    /**
     * Get a USB configuration descriptor based on its index.
     *
     * @param dev device
     * @param config_index config index
     * @param config a USB configuration descriptor pointer
     * @return 0 on success, or an error code
     */
    int libusb_get_config_descriptor(Pointer dev, int config_index, PointerByReference config);
}