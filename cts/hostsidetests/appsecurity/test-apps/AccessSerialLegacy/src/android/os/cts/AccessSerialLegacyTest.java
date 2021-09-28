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

import static junit.framework.Assert.assertTrue;
import static junit.framework.Assert.fail;

import android.os.Build;
import org.junit.Test;

/**
 * Test that legacy apps can no longer access the device serial.
 */
public class AccessSerialLegacyTest {
    @Test
    public void testAccessSerialNoPermissionNeeded() throws Exception {
        // Build.SERIAL must no longer provide the device serial for legacy apps.
        assertTrue("Build.SERIAL must not be visible to legacy apps",
                Build.UNKNOWN.equals(Build.SERIAL));

        // We don't have the READ_PHONE_STATE permission, so this should throw
        try {
            Build.getSerial();
            fail("getSerial() must be gated on the READ_PHONE_STATE permission");
        } catch (SecurityException e) {
            /* expected */
        }
    }
}
