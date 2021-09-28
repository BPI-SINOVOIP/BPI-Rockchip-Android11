/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.security.cts;

import com.android.compatibility.common.util.CddTest;
import com.android.compatibility.common.util.PropertyUtil;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.SecurityTest;
import android.test.AndroidTestCase;
import junit.framework.TestCase;

import android.os.Build;
import android.util.Log;

@SecurityTest
public class EncryptionTest extends AndroidTestCase {
    static {
        System.loadLibrary("ctssecurity_jni");
    }

    private static final int MIN_ENCRYPTION_REQUIRED_API_LEVEL = 23;

    // First API level where there are no speed exemptions.
    private static final int MIN_ALL_SPEEDS_API_LEVEL = Build.VERSION_CODES.Q;

    // First API level at which file based encryption must be used.
    private static final int MIN_FBE_REQUIRED_API_LEVEL = Build.VERSION_CODES.Q;

    private static final String TAG = "EncryptionTest";

    private static native boolean aesIsFast();

    private void handleUnencryptedDevice() {
        if (PropertyUtil.getFirstApiLevel() < MIN_ENCRYPTION_REQUIRED_API_LEVEL) {
            Log.d(TAG, "Exempt from encryption due to an old starting API level.");
            return;
        }
        // In older API levels, we grant an exemption if AES is not fast enough.
        if (PropertyUtil.getFirstApiLevel() < MIN_ALL_SPEEDS_API_LEVEL) {
            // Note: aesIsFast() takes ~2 second to run, so it's worth rearranging
            //     test logic to delay calling this.
            if (!aesIsFast()) {
                Log.d(TAG, "Exempt from encryption because AES performance is too low.");
                return;
            }
        }
        fail("Device encryption is required");
    }

    private void handleEncryptedDevice() {
        if ("file".equals(PropertyUtil.getProperty("ro.crypto.type"))) {
            Log.d(TAG, "Device is encrypted with file-based encryption.");
            // Note: this test doesn't check whether the requirements for
            // encryption algorithms are met, since apps don't have a way to
            // query this information.  Instead, it's tested in
            // CtsNativeEncryptionTestCases.
            return;
        }
        if (PropertyUtil.getFirstApiLevel() < MIN_FBE_REQUIRED_API_LEVEL) {
            Log.d(TAG, "Device is encrypted.");
            return;
        }
        fail("File-based encryption is required");
    }

    // "getprop", used by PropertyUtil.getProperty(), is not executable
    // to instant apps
    @AppModeFull
    @CddTest(requirement="9.9.2/C-0-1,C-0-2,C-0-3")
    public void testEncryption() throws Exception {
        if ("encrypted".equals(PropertyUtil.getProperty("ro.crypto.state"))) {
            handleEncryptedDevice();
        } else {
            handleUnencryptedDevice();
        }
    }
}
