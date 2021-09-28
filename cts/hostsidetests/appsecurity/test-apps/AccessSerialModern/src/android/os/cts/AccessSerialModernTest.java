/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.os.cts;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;
import static junit.framework.Assert.fail;

import android.os.Build;

import androidx.test.InstrumentationRegistry;

import org.junit.Test;

/**
 * Test that modern apps cannot access the device serial number even with the phone permission as
 * 3P apps are now restricted from accessing persistent device identifiers.
 */
public class AccessSerialModernTest {
    @Test
    public void testAccessSerialPermissionNeeded() throws Exception {
        // Build.SERIAL should not provide the device serial for modern apps.
        // We don't know the serial but know that it should be the dummy
        // value returned to unauthorized callers, so make sure that value
        assertTrue("Build.SERIAL must not work for modern apps",
                Build.UNKNOWN.equals(Build.SERIAL));

        // We don't have the read phone state permission, so this should throw
        try {
            Build.getSerial();
            fail("getSerial() must be gated on the READ_PHONE_STATE permission");
        } catch (SecurityException e) {
            /* expected */
        }

        // Now grant ourselves READ_PHONE_STATE
        grantReadPhoneStatePermission();

        // Build.SERIAL should not provide the device serial for modern apps.
        assertTrue("Build.SERIAL must not work for modern apps",
                Build.UNKNOWN.equals(Build.SERIAL));

        // To prevent breakage an app targeting pre-Q with the READ_PHONE_STATE permission will
        // receive Build.UNKNOWN; once the app is targeting Q+ a SecurityException will be thrown
        // even if the app has the READ_PHONE_STATE permission.
        try {
            assertEquals("Build.getSerial() must return " + Build.UNKNOWN
                            + " for an app targeting pre-Q with the READ_PHONE_STATE permission",
                    Build.UNKNOWN, Build.getSerial());
        } catch (SecurityException e) {
            fail("Build.getSerial() must return " + Build.UNKNOWN
                    + " for an app targeting pre-Q with the READ_PHONE_STATE permission, caught "
                    + "SecurityException: "
                    + e);
        }
    }

    private void grantReadPhoneStatePermission() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation().grantRuntimePermission(
                InstrumentationRegistry.getContext().getPackageName(),
                android.Manifest.permission.READ_PHONE_STATE);
    }
}
