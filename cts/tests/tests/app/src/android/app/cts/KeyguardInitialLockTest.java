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

package android.app;

import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.test.AndroidTestCase;
import android.util.ArraySet;

import java.util.List;

public class KeyguardInitialLockTest extends AndroidTestCase {

    public void testSetInitialLockPermission() {
        final ArraySet<String> allowedPackages = new ArraySet();
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_SETUP_WIZARD);
        PackageManager pm = getContext().getPackageManager();
        final List<ResolveInfo> activities =
                pm.queryIntentActivities(intent, PackageManager.MATCH_DISABLED_COMPONENTS);
        String validPkg = "";
        for (ResolveInfo ri : activities) {
            if (ri != null) {
                allowedPackages.add(ri.activityInfo.packageName);
                validPkg = ri.activityInfo.packageName;
            }
        }
        final List<PackageInfo> holding = pm.getPackagesHoldingPermissions(new String[]{
                android.Manifest.permission.SET_INITIAL_LOCK
        }, PackageManager.MATCH_UNINSTALLED_PACKAGES);
        for (PackageInfo pi : holding) {
            if (!allowedPackages.contains(pi.packageName)) {
                fail("The SET_INITIAL_LOCK permission must not be held by " + pi.packageName
                        + " and must be revoked for security reasons [" + validPkg + "]");
            }
        }
    }
}
