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

package android.appsecurity.cts.locationpolicy;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.UserManager;
import android.platform.test.annotations.SecurityTest;
import android.telephony.TelephonyManager;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class LocationPolicyTest {

    private final Context mContext = InstrumentationRegistry.getInstrumentation().getContext();

    @Test
    @SecurityTest
    public void testLocationPolicyPermissions() throws Exception {
        assertNotNull(mContext);
        PackageManager pm = mContext.getPackageManager();
        assertNotNull(pm);
        assertNotEquals(
            PackageManager.PERMISSION_GRANTED,
            pm.checkPermission(Manifest.permission.ACCESS_FINE_LOCATION,
            mContext.getPackageName()));
        assertNotEquals(
            PackageManager.PERMISSION_GRANTED,
            pm.checkPermission(Manifest.permission.ACCESS_COARSE_LOCATION,
            mContext.getPackageName()));
        UserManager manager = mContext.getSystemService(UserManager.class);
        if (manager.isSystemUser()) {
            return;
        }
        if (pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
            TelephonyManager tele = mContext.getSystemService(TelephonyManager.class);
            try {
                tele.getCellLocation();
            fail(
                "ACCESS_FINE_LOCATION and ACCESS_COARSE_LOCATION Permissions not granted. Should"
                  + " have received a security exception when invoking getCellLocation().");
            } catch (SecurityException ignore) {
              // That's what we want!
            }
        }
    }
}
