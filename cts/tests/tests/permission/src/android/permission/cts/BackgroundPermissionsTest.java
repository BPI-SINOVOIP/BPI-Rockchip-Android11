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

package android.permission.cts;

import static android.Manifest.permission.ACCESS_BACKGROUND_LOCATION;
import static android.Manifest.permission.ACCESS_COARSE_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_FOREGROUND;
import static android.app.AppOpsManager.MODE_IGNORED;
import static android.content.pm.PermissionInfo.PROTECTION_DANGEROUS;
import static android.permission.cts.PermissionUtils.getAppOp;
import static android.permission.cts.PermissionUtils.grantPermission;
import static android.permission.cts.PermissionUtils.install;
import static android.permission.cts.PermissionUtils.uninstallApp;

import static com.android.compatibility.common.util.SystemUtil.eventually;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.AppOpsManager;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.platform.test.annotations.AppModeFull;
import android.util.ArrayMap;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class BackgroundPermissionsTest {
    private static final String LOG_TAG = BackgroundPermissionsTest.class.getSimpleName();

    /** The package name of all apps used in the test */
    private static final String APP_PKG = "android.permission.cts.appthatrequestpermission";

    private static final String TMP_DIR = "/data/local/tmp/cts/permissions/";
    private static final String APK_LOCATION_BACKGROUND_29 =
            TMP_DIR + "CtsAppThatRequestsLocationAndBackgroundPermission29.apk";
    private static final String APK_LOCATION_29v4 =
            TMP_DIR + "CtsAppThatRequestsLocationPermission29v4.apk";

    private static final Context sContext =
            InstrumentationRegistry.getInstrumentation().getTargetContext();
    private static final UiAutomation sUiAutomation =
            InstrumentationRegistry.getInstrumentation().getUiAutomation();

    @After
    public void uninstallTestApp() {
        uninstallApp(APP_PKG);
    }

    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages")
    public void verifybackgroundPermissionsProperties() throws Exception {
        PackageInfo pkg = sContext.getPackageManager().getPackageInfo(
                "android", PackageManager.GET_PERMISSIONS);
        ArrayMap<String, String> potentialBackgroundPermissionsToGroup = new ArrayMap<>();

        int numPermissions = pkg.permissions.length;
        for (int i = 0; i < numPermissions; i++) {
            PermissionInfo permission = pkg.permissions[i];

            // background permissions must be dangerous
            if ((permission.getProtection() & PROTECTION_DANGEROUS) != 0) {
                potentialBackgroundPermissionsToGroup.put(permission.name, permission.group);
            }
        }

        for (int i = 0; i < numPermissions; i++) {
            PermissionInfo permission = pkg.permissions[i];
            String backgroundPermissionName = permission.backgroundPermission;

            if (backgroundPermissionName != null) {
                Log.i(LOG_TAG, permission.name + "->" + backgroundPermissionName);

                // foreground permissions must be dangerous
                assertNotEquals(0, permission.getProtection() & PROTECTION_DANGEROUS);

                // All foreground permissions need an app op
                assertNotNull(AppOpsManager.permissionToOp(permission.name));

                // the background permission must exist
                assertTrue(potentialBackgroundPermissionsToGroup
                        .containsKey(backgroundPermissionName));
            }
        }
    }

    /**
     * If a bg permission is lost during an upgrade, the app-op should downgrade to foreground
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpGetsDowngradedWhenBgPermIsNotRequestedAnymore() throws Exception {
        install(APK_LOCATION_BACKGROUND_29);
        grantPermission(APP_PKG, ACCESS_COARSE_LOCATION);

        install(APK_LOCATION_29v4);

        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "foreground app-op").isEqualTo(MODE_FOREGROUND));
    }

    /**
     * Make sure location switch-op is set if no location access is granted.
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpIsSetIfNoLocPermIsGranted() {
        install(APK_LOCATION_BACKGROUND_29);

        // Wait until the system sets the app-op automatically
        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "loc app-op").isEqualTo(MODE_IGNORED));
    }

    /**
     * Make sure location switch-op is set if only coarse location is granted
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpIsSetIfOnlyCoarseLocPermIsGranted() {
        install(APK_LOCATION_BACKGROUND_29);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_COARSE_LOCATION);

        // Wait until the system sets the app-op automatically
        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "loc app-op").isEqualTo(MODE_FOREGROUND));
    }

    /**
     * Make sure location switch-op is set if coarse location with background access is granted.
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpIsSetIfCoarseAndBgLocPermIsGranted() {
        install(APK_LOCATION_BACKGROUND_29);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_COARSE_LOCATION);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_BACKGROUND_LOCATION);

        // Wait until the system sets the app-op automatically
        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "loc app-op").isEqualTo(MODE_ALLOWED));
    }

    /**
     * Make sure location switch-op is set if only fine location is granted
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpIsSetIfOnlyFineLocPermIsGranted() {
        install(APK_LOCATION_BACKGROUND_29);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_FINE_LOCATION);

        // Wait until the system sets the app-op automatically
        // Fine location uses background location to limit access
        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "loc app-op").isEqualTo(MODE_FOREGROUND));
    }

    /**
     * Make sure location switch-op is set if fine location with background access is granted.
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpIsSetIfFineAndBgLocPermIsGranted() {
        install(APK_LOCATION_BACKGROUND_29);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_FINE_LOCATION);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_BACKGROUND_LOCATION);

        // Wait until the system sets the app-op automatically
        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "loc app-op").isEqualTo(MODE_ALLOWED));
    }

    /**
     * Make sure location switch-op is set if fine and coarse location access is granted.
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpIsSetIfFineAndCoarseLocPermIsGranted() {
        install(APK_LOCATION_BACKGROUND_29);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_FINE_LOCATION);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_COARSE_LOCATION);

        // Wait until the system sets the app-op automatically
        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "loc app-op").isEqualTo(MODE_FOREGROUND));
    }

    /**
     * Make sure location switch-op is set if fine and coarse location with background access is
     * granted.
     */
    @Test
    @AppModeFull(reason = "Instant apps cannot read properties of other packages which is needed "
            + "to grant permissions to them. Also instant apps are never updated, hence the test "
            + "is useless.")
    public void appOpIsSetIfFineCoarseAndBgLocPermIsGranted() {
        install(APK_LOCATION_BACKGROUND_29);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_FINE_LOCATION);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_COARSE_LOCATION);
        sUiAutomation.grantRuntimePermission(APP_PKG, ACCESS_BACKGROUND_LOCATION);

        // Wait until the system sets the app-op automatically
        eventually(() -> assertThat(getAppOp(APP_PKG, ACCESS_COARSE_LOCATION)).named(
                "loc app-op").isEqualTo(MODE_ALLOWED));
    }
}
