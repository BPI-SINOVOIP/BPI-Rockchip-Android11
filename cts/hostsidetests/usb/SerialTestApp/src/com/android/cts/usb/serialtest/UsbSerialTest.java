/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.cts.usb.serialtest;

import android.os.Build;
import android.util.Log;

import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.ShellIdentityUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Device test which reads Build.SERIAL and just print it.
 */
@RunWith(AndroidJUnit4.class)
public class UsbSerialTest {
    private static final String TAG = "CtsUsbSerialTest";

    @Test
    public void logSerial() {
        // Since device identifiers are no longer accessible to third party apps invoke the
        // Build#getSerial method with the shell identity's permissions.
        Log.e(TAG, ShellIdentityUtils.invokeStaticMethodWithShellPermissions(Build::getSerial));
    }

    @Test(expected = SecurityException.class)
    public void verifySerialCannotBeRead() {
        Build.getSerial();
    }
}
