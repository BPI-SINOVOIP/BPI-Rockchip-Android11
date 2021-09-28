/*
 * Copyright (C) 2016 The Android Open Source Project
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
package android.telephony.cts.preconditions.app;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static org.junit.Assert.assertTrue;

import android.content.pm.PackageManager;
import android.net.wifi.WifiManager;
import android.test.AndroidTestCase;
import android.util.Log;

import com.android.compatibility.common.preconditions.TelephonyHelper;

/**
 * A test to verify that device-side preconditions are met for CTS
 */
public class TelephonyPreparerAppTest extends AndroidTestCase {
    private static final String TAG = "TelephonyPreparerAppTest";


    private boolean hasTelephony() {
        final PackageManager pm = getContext().getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
    }

    /**
     * Test if device has a valid phone number
     * @throws Exception
     */
    public void testPhoneNumberPresent() throws Exception {
        if (!hasTelephony()) return;

        if (!TelephonyHelper.hasPhoneNumber(this.getContext())) {
            Log.e(TAG, "No SIM card with phone number is found in the device, "
                + "some tests might not run properly");
        }
    }

    public void testEnsureWifiDisabled() throws Exception {
        if (!hasTelephony()) return;

        final WifiManager wifiManager = getContext().getSystemService(
                android.net.wifi.WifiManager.class);

        runWithShellPermissionIdentity(() -> assertTrue(wifiManager.setWifiEnabled(false)));
    }
}
