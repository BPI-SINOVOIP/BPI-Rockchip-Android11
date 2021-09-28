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

package android.telephony.cts.telephonypermission;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PackageManager;
import android.platform.test.annotations.AppModeFull;
import android.telephony.TelephonyManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test TelephonyManager APIs with READ_PHONE_STATE Permission.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "Cannot grant the runtime permission in instant app mode")
public class TelephonyManagerReadPhoneStatePermissionTest {

    private boolean mHasTelephony;
    TelephonyManager mTelephonyManager = null;

    @Before
    public void setUp() throws Exception {
        mHasTelephony = getContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_TELEPHONY);
        mTelephonyManager =
                (TelephonyManager) getContext().getSystemService(Context.TELEPHONY_SERVICE);
        assertNotNull(mTelephonyManager);
    }

    public static void grantUserReadPhoneStatePermission() {
        UiAutomation uiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        uiAutomation.grantRuntimePermission(getContext().getPackageName(),
                android.Manifest.permission.READ_PHONE_STATE);
    }

    /**
     * Verify that TelephonyManager APIs requiring READ_PHONE_STATE Permission must work.
     * <p>
     * Requires Permission:
     * {@link android.Manifest.permission#READ_PHONE_STATE}.
     *
     * APIs list:
     * 1) getNetworkType
     * TODO Adding more APIs
     */
    @Test
    public void testTelephonyManagersAPIsRequiringReadPhoneStatePermissions() {
        if (!mHasTelephony) {
            return;
        }

        grantUserReadPhoneStatePermission();

        try {
            mTelephonyManager.getNetworkType();
        } catch (SecurityException e) {
            fail("getNetworkType() must not throw a SecurityException with READ_PHONE_STATE" + e);
        }
    }

    private static Context getContext() {
        return InstrumentationRegistry.getContext();
    }
}
