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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Process;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Tests that shell has acceptable permissions.
 */
@RunWith(AndroidJUnit4.class)
public class ShellPermissionTest {
    private static final String LOG_TAG = ShellPermissionTest.class.getSimpleName();

    /** Permissions that shell is NOT permitted to have. */
    private static final String[] BLACKLISTED_PERMISSIONS = {
            "android.permission.MANAGE_USERS",
            "android.permission.NETWORK_STACK",
    };

    private static final Context sContext = InstrumentationRegistry.getTargetContext();

    /**
     * Verify that the shell uid does not have any of the permissions listed in
     *   {@link #BLACKLISTED_PERMISSIONS}.
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages. Also the shell "
            + "is never an instant app, hence this test does not matter for instant apps.")
    public void testBlacklistedPermissions() throws Exception {
        final Set<String> blacklist = new HashSet<>(Arrays.asList(BLACKLISTED_PERMISSIONS));

        final PackageManager pm = sContext.getPackageManager();
        final String[] pkgs = pm.getPackagesForUid(Process.SHELL_UID);
        assertNotNull("No SHELL packages were found", pkgs);
        assertNotEquals("SHELL package list had 0 size", 0, pkgs.length);
        String pkg = pkgs[0];

        final PackageInfo packageInfo = pm.getPackageInfo(pkg, PackageManager.GET_PERMISSIONS);
        assertNotNull("No permissions found for " + pkg, packageInfo.requestedPermissions);

        for (String permission : packageInfo.requestedPermissions) {
            Log.d(LOG_TAG, "SHELL as " + pkg + " uses permission " + permission);
            assertFalse("SHELL as " + pkg + " contains the illegal permission " + permission,
                    blacklist.contains(permission));
        }
    }
}
