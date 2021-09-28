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

import com.sun.jna.Library;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;

/** JNA adapter for <a href="https://libusb.info">libusb</a>. */
interface IUsbNative extends Library {

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
     * Returns a short user-friendly description of the given error code.
     *
     * @param errcode error code to look up
     * @return short description of the error code
     */
    String libusb_strerror(int errcode);

    /**
     * Returns a list of USB devices currently attached to the system.
     *
     * @param ctx context to operate on
     * @param list output location for a list of devices
     * @return number of devices, or an error code
     */
    int libusb_get_device_list(Pointer ctx, PointerByReference list);

    /**
     * Frees a list of devices previously discovered using {@link #libusb_get_device_list}.
     *
     * @param list list to free
     * @param unref_devices true to unref devices
     */
    void libusb_free_device_list(Pointer list, boolean unref_devices);

    /**
     * Open a device and obtain a device handle.
     *
     * @param dev device to open
     * @param dev_handle output location for the device handle
     * @return 0 on success, or an error code
     */
    int libusb_open(Pointer dev, PointerByReference dev_handle);

    /**
     * Close a device handle previously opened with {@link #libusb_open}.
     *
     * @param dev_handle device handle to close
     */
    void libusb_close(Pointer dev_handle);

    /**
     * Perform a USB port reset to reinitialize a device.
     *
     * @param dev_handle device handle to reset
     * @return 0 on success, or an error code
     */
    int libusb_reset_device(Pointer dev_handle);

    /**
     * Get the USB device descriptor for a given device.
     *
     * @param dev device
     * @param desc output location for the descriptor data
     * @return 0 on success, or an error code
     */
    int libusb_get_device_descriptor(Pointer dev, byte[] desc);

    /**
     * Retrieve a string descriptor in C style ASCII.
     *
     * @param dev_handle device handle
     * @param desc_index index of the descriptor to retrieve
     * @param data output buffer for ASCII string descriptor
     * @param length size of data buffer
     * @return number of bytes returned, or an error code
     */
    int libusb_get_string_descriptor_ascii(
        Pointer dev_handle, byte desc_index, byte[] data, int length);

    /**
     * Perform a synchronous USB control transfer.
     *
     * @param dev_handle device handle to communicate with
     * @param bmRequestType request type field, determines the direction of the transfer
     * @param bRequest request field
     * @param wValue value field
     * @param wIndex index field
     * @param data suitably-sized data buffer for input or output
     * @param wLength length of the data buffer
     * @param timeout timeout milliseconds, or 0 for unlimited
     * @return number of bytes transferred, or an error code
     */
    int libusb_control_transfer(
        Pointer dev_handle,
        byte bmRequestType,
        byte bRequest,
        short wValue,
        short wIndex,
        byte[] data,
        short wLength,
        int timeout);
}
