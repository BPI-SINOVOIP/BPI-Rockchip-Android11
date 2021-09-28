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

import static android.Manifest.permission.ACCESS_BACKGROUND_LOCATION;
import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.READ_CALL_LOG;
import static android.Manifest.permission.READ_CONTACTS;
import static android.content.pm.PackageManager.FLAG_PERMISSION_REVIEW_REQUIRED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_REVOKE_ON_UPGRADE;
import static android.content.pm.PackageManager.FLAG_PERMISSION_REVOKE_WHEN_REQUESTED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_USER_FIXED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_USER_SET;
import static android.permission.cts.PermissionUtils.clearAppState;
import static android.permission.cts.PermissionUtils.getPermissionFlags;
import static android.permission.cts.PermissionUtils.install;
import static android.permission.cts.PermissionUtils.isGranted;
import static android.permission.cts.PermissionUtils.setPermissionFlags;
import static android.permission.cts.PermissionUtils.uninstallApp;

import static com.android.compatibility.common.util.SystemUtil.eventually;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.AppModeFull;

import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests how permission flags behave.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "Cannot read permission flags of other app.")
public class PermissionFlagsTest {
    /** The package name of all apps used in the test */
    private static final String APP_PKG = "android.permission.cts.appthatrequestpermission";

    private static final String TMP_DIR = "/data/local/tmp/cts/permissions/";
    private static final String APK_CONTACTS_15 =
            TMP_DIR + "CtsAppThatRequestsContactsPermission15.apk";
    private static final String APK_LOCATION_22 =
            TMP_DIR + "CtsAppThatRequestsLocationPermission22.apk";
    private static final String APK_LOCATION_28 =
            TMP_DIR + "CtsAppThatRequestsLocationPermission28.apk";

    @After
    public void uninstallTestApp() {
        uninstallApp(APP_PKG);
    }

    @Test
    public void implicitPermission() {
        install(APK_LOCATION_28);

        assertEquals(FLAG_PERMISSION_REVOKE_WHEN_REQUESTED,
                getPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION));
    }

    @Test
    public void regularPermission() {
        install(APK_LOCATION_28);

        assertEquals(0, getPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION));
    }

    @Test
    public void regularPermissionPreM() {
        install(APK_CONTACTS_15);

        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE | FLAG_PERMISSION_REVIEW_REQUIRED,
                getPermissionFlags(APP_PKG, READ_CONTACTS));
    }

    @Test
    public void implicitPermissionPreM() {
        install(APK_CONTACTS_15);

        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE | FLAG_PERMISSION_REVIEW_REQUIRED |
                FLAG_PERMISSION_REVOKE_WHEN_REQUESTED, getPermissionFlags(APP_PKG, READ_CALL_LOG));
    }

    @Test
    public void clearRegularPermissionPreM() {
        install(APK_CONTACTS_15);

        int defaultState = getPermissionFlags(APP_PKG, READ_CONTACTS);
        setPermissionFlags(APP_PKG, READ_CONTACTS, FLAG_PERMISSION_REVIEW_REQUIRED, 0);
        setPermissionFlags(APP_PKG, READ_CONTACTS,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);

        clearAppState(APP_PKG);

        eventually(() -> assertEquals(defaultState, getPermissionFlags(APP_PKG, READ_CONTACTS)));
    }

    @Test
    public void clearImplicitPermissionPreM() {
        install(APK_CONTACTS_15);

        int defaultState = getPermissionFlags(APP_PKG, READ_CALL_LOG);
        setPermissionFlags(APP_PKG, READ_CALL_LOG, FLAG_PERMISSION_REVIEW_REQUIRED, 0);
        setPermissionFlags(APP_PKG, READ_CALL_LOG,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);

        clearAppState(APP_PKG);

        eventually(() -> assertEquals(defaultState, getPermissionFlags(APP_PKG, READ_CALL_LOG)));
    }

    @Test
    public void clearRegularPermission() {
        install(APK_LOCATION_28);

        int defaultState = getPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION);
        setPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);

        clearAppState(APP_PKG);

        eventually(() -> assertEquals(defaultState,
                getPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION)));
    }

    @Test
    public void clearImplicitPermission() {
        install(APK_LOCATION_28);

        int defaultState = getPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION);
        setPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);

        clearAppState(APP_PKG);

        eventually(() -> assertEquals(defaultState,
                getPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION)));
    }

    @Test
    public void reinstallPreM() {
        install(APK_CONTACTS_15);
        install(APK_CONTACTS_15);

        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE | FLAG_PERMISSION_REVIEW_REQUIRED,
                getPermissionFlags(APP_PKG, READ_CONTACTS));
        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE | FLAG_PERMISSION_REVIEW_REQUIRED |
                FLAG_PERMISSION_REVOKE_WHEN_REQUESTED, getPermissionFlags(APP_PKG, READ_CALL_LOG));
    }

    @Test
    public void reinstallDoesNotOverrideChangesPreM() {
        install(APK_CONTACTS_15);

        setPermissionFlags(APP_PKG, READ_CONTACTS, FLAG_PERMISSION_REVIEW_REQUIRED, 0);
        setPermissionFlags(APP_PKG, READ_CONTACTS,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);
        setPermissionFlags(APP_PKG, READ_CALL_LOG, FLAG_PERMISSION_REVIEW_REQUIRED, 0);
        setPermissionFlags(APP_PKG, READ_CALL_LOG,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);

        install(APK_CONTACTS_15);

        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE | FLAG_PERMISSION_USER_SET
                        | FLAG_PERMISSION_USER_FIXED,
                getPermissionFlags(APP_PKG, READ_CONTACTS));

        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE | FLAG_PERMISSION_USER_SET
                        | FLAG_PERMISSION_USER_FIXED | FLAG_PERMISSION_REVOKE_WHEN_REQUESTED,
                getPermissionFlags(APP_PKG, READ_CALL_LOG));
    }

    @Test
    public void reinstall() {
        install(APK_LOCATION_28);
        install(APK_LOCATION_28);

        assertEquals(0, getPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION));
        assertEquals(FLAG_PERMISSION_REVOKE_WHEN_REQUESTED,
                getPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION));
    }

    @Test
    public void reinstallDoesNotOverrideChanges() {
        install(APK_LOCATION_28);

        setPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);
        setPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED);

        install(APK_LOCATION_28);

        assertEquals(FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED,
                getPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION));

        assertEquals(FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED
                        | FLAG_PERMISSION_REVOKE_WHEN_REQUESTED,
                getPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION));
    }

    @Test
    public void preMPermsAreMarkedRevokeOnUpgrade() throws Exception {
        install(APK_LOCATION_22);

        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE,
                getPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION)
                        & FLAG_PERMISSION_REVOKE_ON_UPGRADE);
        assertEquals(FLAG_PERMISSION_REVOKE_ON_UPGRADE,
                getPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION)
                        & FLAG_PERMISSION_REVOKE_ON_UPGRADE);
    }


    @Test
    public void revokeOnUpgrade() throws Exception {
        install(APK_LOCATION_22);

        setPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION, FLAG_PERMISSION_REVOKE_ON_UPGRADE, 0);

        install(APK_LOCATION_28);

        assertTrue(isGranted(APP_PKG, ACCESS_COARSE_LOCATION));
        assertFalse(isGranted(APP_PKG, ACCESS_BACKGROUND_LOCATION));

        assertEquals(0,getPermissionFlags(APP_PKG, ACCESS_COARSE_LOCATION)
                        & FLAG_PERMISSION_REVOKE_ON_UPGRADE);
        assertEquals(0,getPermissionFlags(APP_PKG, ACCESS_BACKGROUND_LOCATION)
                        & FLAG_PERMISSION_REVOKE_ON_UPGRADE);
    }
}
