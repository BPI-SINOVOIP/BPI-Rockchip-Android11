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

package android.os.ext.cts;

import android.os.Build;
import android.os.ext.SdkExtensions;
import junit.framework.TestCase;

public class SdkExtensionsTest extends TestCase {

    /** Verify that getExtensionVersion only accepts valid extension SDKs */
    public void testBadArgument() throws Exception {
        // R is the first SDK version with extensions.
        for (int sdk = -1_000_000; sdk < Build.VERSION_CODES.R; sdk++) {
            try {
                SdkExtensions.getExtensionVersion(sdk);
                fail("expected IllegalArgumentException");
            } catch (IllegalArgumentException expected) { }
        }
    }

    /** Verifies that getExtensionVersion only return existing versions */
    public void testValidValues() throws Exception {
        for (int sdk = Build.VERSION_CODES.R; sdk <= 1_000_000; sdk++) {
            // No extension SDKs versions yet.
            assertEquals(0, SdkExtensions.getExtensionVersion(sdk));
        }
    }
}
