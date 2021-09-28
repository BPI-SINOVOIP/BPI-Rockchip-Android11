/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.cts.duplicatepermissiondeclareapp;

import static android.content.pm.PackageManager.GET_PERMISSIONS;

import static org.junit.Assert.assertEquals;

import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class VerifyDuplicatePermissionTest {
    private final static String APP_THAT_WAS_INSTALLED_FIRST =
            "com.android.cts.permissiondeclareapp";
    private final static String APP_THAT_WAS_INSTALLED_AFTER =
            "com.android.cts.duplicatepermissiondeclareapp";
    private final static String PERM = "com.android.cts.permissionNormal";

    private final static PackageManager sPm =
            InstrumentationRegistry.getTargetContext().getPackageManager();

    private PermissionInfo getDeclaredPermission(String pkg, String permission) throws Exception {
        for (PermissionInfo permInfo : sPm.getPackageInfo(pkg, GET_PERMISSIONS).permissions) {
            if (permInfo.name.equals(permission)) {
                return permInfo;
            }
        }

        return null;
    }

    @Test
    public void verifyDuplicatePermission() throws Exception {
        // The other app was first. The second app could not steal the permission. Hence the
        // permission belongs to the first app.
        assertEquals(APP_THAT_WAS_INSTALLED_FIRST, sPm.getPermissionInfo(PERM, 0).packageName);

        // The first app declared the permission
        assertEquals(APP_THAT_WAS_INSTALLED_FIRST,
                getDeclaredPermission(APP_THAT_WAS_INSTALLED_FIRST, PERM).packageName);

        // The second app declared the permission too, but the permission is owned by first app.
        // Hence we end up with a permission info that is different from the one read from the
        // system.
        assertEquals(APP_THAT_WAS_INSTALLED_AFTER,
                getDeclaredPermission(APP_THAT_WAS_INSTALLED_AFTER, PERM).packageName);
    }
}
