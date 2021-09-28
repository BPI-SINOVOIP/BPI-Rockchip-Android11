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

import com.google.common.collect.ImmutableList;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import java.util.List;

public class DeviceDescriptor extends Structure {
    public static class ByReference extends DeviceDescriptor implements Structure.ByReference {}

    public DeviceDescriptor() {}

    public DeviceDescriptor(Pointer p) {
        super(p);
        read();
    }

    @Override
    protected List<String> getFieldOrder() {
        return ImmutableList.of("bLength", "bDescriptorType", "bcdUSB", "bDeviceClass",
                "bDeviceSubClass", "bDeviceProtocol", "bMaxPacketSize0", "idVendor", "idProduct",
                "bcdDevice", "iManufacturer", "iProduct", "iSerialNumber", "bNumConfigurations");
    }

    public byte bLength;
    public byte bDescriptorType;
    public short bcdUSB;
    public byte bDeviceClass;
    public byte bDeviceSubClass;
    public byte bDeviceProtocol;
    public byte bMaxPacketSize0;
    public short idVendor;
    public short idProduct;
    public short bcdDevice;
    public byte iManufacturer;
    public byte iProduct;
    public byte iSerialNumber;
    public byte bNumConfigurations;
}
