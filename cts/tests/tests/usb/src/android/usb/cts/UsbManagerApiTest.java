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

package android.usb.cts;

import static android.Manifest.permission.MANAGE_USB;

import android.app.UiAutomation;
import android.hardware.usb.UsbManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Unit tests for {@link android.hardware.usb.UsbManager}.
 * Note: MUST claimed MANAGE_USB permission in Manifest
 */
@RunWith(AndroidJUnit4.class)
public class UsbManagerApiTest {
    private static final String TAG = UsbManagerApiTest.class.getSimpleName();

    private UsbManager mUsbManagerSys =
        InstrumentationRegistry.getContext().getSystemService(UsbManager.class);
    private UiAutomation mUiAutomation =
        InstrumentationRegistry.getInstrumentation().getUiAutomation();

    @Before
    public void setUp() {
        Assert.assertNotNull(mUsbManagerSys);
    }

    /**
     * Verify NO SecurityException.
     * Go through System Server.
     */
    @Test
    public void test_UsbApiSetGetCurrentFunctionsSys() throws Exception {
        // Adopt MANAGE_USB permission.
        mUiAutomation.adoptShellPermissionIdentity(MANAGE_USB);

        // Should pass with permission.
        mUsbManagerSys.setCurrentFunctions(UsbManager.FUNCTION_NONE);
        Assert.assertEquals("CurrentFunctions mismatched: ", UsbManager.FUNCTION_NONE,
                mUsbManagerSys.getCurrentFunctions());

        // Drop MANAGE_USB permission.
        mUiAutomation.dropShellPermissionIdentity();

        try {
            mUsbManagerSys.getCurrentFunctions();
            Assert.fail("Expecting SecurityException on getCurrentFunctions.");
        } catch (SecurityException secEx) {
            Log.d(TAG, "Expected SecurityException on getCurrentFunctions");
        }

        try {
            mUsbManagerSys.setCurrentFunctions(UsbManager.FUNCTION_NONE);
            Assert.fail("Expecting SecurityException on setCurrentFunctions.");
        } catch (SecurityException secEx) {
            Log.d(TAG, "Expected SecurityException on setCurrentFunctions");
        }
    }
}
