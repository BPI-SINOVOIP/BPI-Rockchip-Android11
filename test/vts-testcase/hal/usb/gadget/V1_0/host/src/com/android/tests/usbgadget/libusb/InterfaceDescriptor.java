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

public class InterfaceDescriptor extends Structure {
    public static class ByReference extends InterfaceDescriptor implements Structure.ByReference {}

    public InterfaceDescriptor() {}

    public InterfaceDescriptor(Pointer p) {
        super(p);
        read();
    }

    public InterfaceDescriptor[] toArray(int size) {
        return (InterfaceDescriptor[]) super.toArray(size);
    }

    @Override
    protected List<String> getFieldOrder() {
        return ImmutableList.of("bLength", "bDescriptorType", "bInterfaceNumber",
                "bAlternateSetting", "bNumEndpoints", "bInterfaceClass", "bInterfaceSubClass",
                "bInterfaceProtocol", "iInterface", "endpoint", "extra", "extra_length");
    }

    public byte bLength;
    public byte bDescriptorType;
    public byte bInterfaceNumber;
    public byte bAlternateSetting;
    public byte bNumEndpoints;
    public byte bInterfaceClass;
    public byte bInterfaceSubClass;
    public byte bInterfaceProtocol;
    public byte iInterface;
    public Pointer endpoint;
    public Pointer extra;
    public int extra_length;
}
