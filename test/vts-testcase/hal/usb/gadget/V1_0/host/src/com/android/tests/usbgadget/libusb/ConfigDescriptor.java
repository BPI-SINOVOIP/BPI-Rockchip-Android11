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

public class ConfigDescriptor extends Structure {
    public static class ByReference extends ConfigDescriptor implements Structure.ByReference {}

    public ConfigDescriptor() {}

    public ConfigDescriptor(Pointer p) {
        super(p);
        read();
    }

    @Override
    protected List<String> getFieldOrder() {
        return ImmutableList.of("bLength", "bDescriptorType", "wTotalLength", "bNumInterfaces",
                "bConfigurationValue", "iConfiguration", "bmAttributes", "bMaxPower", "interfaces",
                "extra", "extra_length");
    }

    public byte bLength;
    public byte bDescriptorType;
    public short wTotalLength;
    public byte bNumInterfaces;
    public byte bConfigurationValue;
    public byte iConfiguration;
    public byte bmAttributes;
    public byte bMaxPower;
    public Interface.ByReference interfaces;
    public Pointer extra;
    public int extra_length;
}
