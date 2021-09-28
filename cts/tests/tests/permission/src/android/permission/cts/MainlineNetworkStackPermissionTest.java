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

package android.permission.cts;

import static android.net.NetworkStack.PERMISSION_MAINLINE_NETWORK_STACK;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.platform.test.annotations.AppModeFull;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class MainlineNetworkStackPermissionTest{
    private final Context mContext = InstrumentationRegistry.getContext();

    /**
     * Test that a package defining android.permission.MAINLINE_NETWORK_STACK is installed,
     * and is a system package
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot access PackageManager#getPermissionInfo")
    public void testPackageWithMainlineNetworkStackPermission() throws Exception {
        final PackageManager packageManager = mContext.getPackageManager();
        assertNotNull("Unable to find PackageManager.", packageManager);

        final PermissionInfo permissioninfo =
                packageManager.getPermissionInfo(PERMISSION_MAINLINE_NETWORK_STACK, 0);
        assertNotNull("Network stack permission is not defined.", permissioninfo);

        PackageInfo packageInfo = packageManager.getPackageInfo(permissioninfo.packageName,
                PackageManager.MATCH_SYSTEM_ONLY | PackageManager.GET_PERMISSIONS);
        assertNotNull("Package defining the network stack permission is not a system package.",
                packageInfo.permissions);

        for (PermissionInfo permission : packageInfo.permissions) {
            if (PERMISSION_MAINLINE_NETWORK_STACK.equals(permission.name)) {
                return;
            }
        }

        fail("Expect a system package defining " + PERMISSION_MAINLINE_NETWORK_STACK
                + " is installed.");
    }
}
