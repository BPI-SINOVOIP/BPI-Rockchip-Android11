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

package android.telephony.cts;

import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.SystemProperties;
import android.telephony.TelephonyManager;
import android.content.res.Resources;
import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;

public class IwlanModeTest {
    private TelephonyManager mTelephonyManager;

    private PackageManager mPackageManager;

    private Context mContext;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();
        mPackageManager = mContext.getPackageManager();
        mTelephonyManager = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
    }

    @Test
    public void testIwlanMode() {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            return;
        }

        // All new devices should not enable IWLAN legacy mode. Legacy mode is for existing old
        // devices.
        if (mTelephonyManager.getRadioHalVersion().first >= 1
                && mTelephonyManager.getRadioHalVersion().second >= 5) {
            String mode = SystemProperties.get("ro.telephony.iwlan_operation_mode");
            assertNotEquals("legacy", mode);

            Resources res = Resources.getSystem();
            int id = res.getIdentifier("config_wlan_data_service_package", "string", "android");
            String wlanDataServicePackage = res.getString(id);
            assertNotEquals("", wlanDataServicePackage);

            id = res.getIdentifier("config_wlan_network_service_package", "string", "android");
            String wlanNetworkServicePackage = res.getString(id);
            assertNotEquals("", wlanNetworkServicePackage);
        }
    }
}
