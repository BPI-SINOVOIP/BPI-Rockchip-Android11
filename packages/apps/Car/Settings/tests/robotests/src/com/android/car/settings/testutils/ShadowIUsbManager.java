/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.car.settings.testutils;

import android.hardware.usb.IUsbManager;
import android.os.IBinder;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

@Implements(value = IUsbManager.Stub.class)
public class ShadowIUsbManager {

    private static IUsbManager sInstance;

    public static void setInstance(IUsbManager instance) {
        sInstance = instance;
    }

    @Implementation
    public static IUsbManager asInterface(IBinder obj) {
        return sInstance;
    }

    @Resetter
    public static void reset() {
        sInstance = null;
    }
}
