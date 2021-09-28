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

import static android.content.pm.PackageInfo.REQUESTED_PERMISSION_GRANTED;
import static android.content.pm.PackageManager.GET_PERMISSIONS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.SecurityTest;

import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

@AppModeFull(reason = "Instant apps cannot read state of other packages.")
public class RemovePermissionTest {
    private static final String APP_PKG_NAME_BASE =
            "android.permission.cts.revokepermissionwhenremoved";
    private static final String ADVERSARIAL_PERMISSION_DEFINER_PKG_NAME =
            APP_PKG_NAME_BASE + ".AdversarialPermissionDefinerApp";
    private static final String VICTIM_PERMISSION_DEFINER_PKG_NAME =
            APP_PKG_NAME_BASE + ".VictimPermissionDefinerApp";
    private static final String ADVERSARIAL_PERMISSION_USER_PKG_NAME =
            APP_PKG_NAME_BASE + ".userapp";
    private static final String RUNTIME_PERMISSION_USER_PKG_NAME =
            APP_PKG_NAME_BASE + ".runtimepermissionuserapp";
    private static final String RUNTIME_PERMISSION_DEFINER_PKG_NAME =
            APP_PKG_NAME_BASE + ".runtimepermissiondefinerapp";
    private static final String INSTALL_PERMISSION_USER_PKG_NAME =
            APP_PKG_NAME_BASE + ".installpermissionuserapp";
    private static final String INSTALL_PERMISSION_DEFINER_PKG_NAME =
            APP_PKG_NAME_BASE + ".installpermissiondefinerapp";
    private static final String INSTALL_PERMISSION_ESCALATOR_PKG_NAME =
            APP_PKG_NAME_BASE + ".installpermissionescalatorapp";

    private static final String TEST_PERMISSION =
            "android.permission.cts.revokepermissionwhenremoved.TestPermission";
    private static final String TEST_RUNTIME_PERMISSION =
            APP_PKG_NAME_BASE + ".TestRuntimePermission";
    private static final String TEST_INSTALL_PERMISSION =
            APP_PKG_NAME_BASE + ".TestInstallPermission";

    private static final String ADVERSARIAL_PERMISSION_DEFINER_APK_NAME =
            "CtsAdversarialPermissionDefinerApp";
    private static final String ADVERSARIAL_PERMISSION_USER_APK_NAME =
            "CtsAdversarialPermissionUserApp";
    private static final String VICTIM_PERMISSION_DEFINER_APK_NAME =
            "CtsVictimPermissionDefinerApp";
    private static final String RUNTIME_PERMISSION_DEFINER_APK_NAME =
            "CtsRuntimePermissionDefinerApp";
    private static final String RUNTIME_PERMISSION_USER_APK_NAME =
            "CtsRuntimePermissionUserApp";
    private static final String INSTALL_PERMISSION_DEFINER_APK_NAME =
            "CtsInstallPermissionDefinerApp";
    private static final String INSTALL_PERMISSION_USER_APK_NAME =
            "CtsInstallPermissionUserApp";
    private static final String INSTALL_PERMISSION_ESCALATOR_APK_NAME =
            "CtsInstallPermissionEscalatorApp";

    private Context mContext;
    private Instrumentation mInstrumentation;

    @Before
    public void setContextAndInstrumentation() {
        mContext = InstrumentationRegistry.getTargetContext();
        mInstrumentation = InstrumentationRegistry.getInstrumentation();
    }

    @Before
    public void wakeUpScreen() {
        SystemUtil.runShellCommand("input keyevent KEYCODE_WAKEUP");
    }

    @After
    public void cleanUpTestApps() throws Exception {
        uninstallApp(ADVERSARIAL_PERMISSION_DEFINER_PKG_NAME, true);
        uninstallApp(ADVERSARIAL_PERMISSION_USER_PKG_NAME, true);
        uninstallApp(VICTIM_PERMISSION_DEFINER_PKG_NAME, true);
        uninstallApp(RUNTIME_PERMISSION_DEFINER_PKG_NAME, true);
        uninstallApp(RUNTIME_PERMISSION_USER_PKG_NAME, true);
        uninstallApp(INSTALL_PERMISSION_USER_PKG_NAME, true);
        uninstallApp(INSTALL_PERMISSION_DEFINER_PKG_NAME, true);
        uninstallApp(INSTALL_PERMISSION_ESCALATOR_PKG_NAME, true);
        Thread.sleep(5000);
    }

    private boolean permissionGranted(String pkgName, String permName)
            throws PackageManager.NameNotFoundException {
        PackageInfo appInfo = mContext.getPackageManager().getPackageInfo(pkgName,
                GET_PERMISSIONS);

        for (int i = 0; i < appInfo.requestedPermissions.length; i++) {
            if (appInfo.requestedPermissions[i].equals(permName)
                    && ((appInfo.requestedPermissionsFlags[i] & REQUESTED_PERMISSION_GRANTED)
                    != 0)) {
                return true;
            }
        }
        return false;
    }

    private void installApp(String apk) throws InterruptedException {
        String installResult = SystemUtil.runShellCommand(
                "pm install -r -d data/local/tmp/cts/permissions/" + apk + ".apk");
        assertEquals("Success", installResult.trim());
        Thread.sleep(5000);
    }

    private void uninstallApp(String pkg) throws InterruptedException {
        uninstallApp(pkg, false);
    }

    private void uninstallApp(String pkg, boolean cleanUp) throws InterruptedException {
        String uninstallResult = SystemUtil.runShellCommand("pm uninstall " + pkg);
        if (!cleanUp) {
            assertEquals("Success", uninstallResult.trim());
            Thread.sleep(5000);
        }
    }

    private void grantPermission(String pkg, String permission) {
        mInstrumentation.getUiAutomation().grantRuntimePermission(
                pkg, permission);
    }

    @SecurityTest
    @Test
    public void runtimePermissionShouldBeRevokedIfRemoved() throws Throwable {
        installApp(ADVERSARIAL_PERMISSION_DEFINER_APK_NAME);
        installApp(ADVERSARIAL_PERMISSION_USER_APK_NAME);

        grantPermission(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION);
        assertTrue(permissionGranted(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION));

        // Uninstall app which defines a permission with the same name as in victim app.
        // Install the victim app.
        uninstallApp(ADVERSARIAL_PERMISSION_DEFINER_PKG_NAME);
        installApp(VICTIM_PERMISSION_DEFINER_APK_NAME);
        assertFalse(permissionGranted(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION));
    }

    @Test
    public void runtimePermissionShouldRemainGrantedAfterAppUpdate() throws Throwable {
        installApp(RUNTIME_PERMISSION_DEFINER_APK_NAME);
        installApp(RUNTIME_PERMISSION_USER_APK_NAME);

        grantPermission(RUNTIME_PERMISSION_USER_PKG_NAME, TEST_RUNTIME_PERMISSION);
        assertTrue(permissionGranted(RUNTIME_PERMISSION_USER_PKG_NAME, TEST_RUNTIME_PERMISSION));

        // Install app which defines a permission. This is similar to update the app
        // operation
        installApp(RUNTIME_PERMISSION_DEFINER_APK_NAME);
        assertTrue(permissionGranted(RUNTIME_PERMISSION_USER_PKG_NAME, TEST_RUNTIME_PERMISSION));
    }

    @Test
    public void runtimePermissionDependencyTest() throws Throwable {
        installApp(ADVERSARIAL_PERMISSION_USER_APK_NAME);
        // Should fail to grant permission because its definer is not installed yet
        try {
            grantPermission(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION);
            fail("Should have thrown security exception above");
        } catch (SecurityException expected) {
        }
        assertFalse(permissionGranted(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION));
        // Now install the permission definer; should be able to grant permission to user package
        installApp(ADVERSARIAL_PERMISSION_DEFINER_APK_NAME);
        grantPermission(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION);
        assertTrue(permissionGranted(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION));
        // Now uninstall the permission definer; the user packages' permission should be revoked
        uninstallApp(ADVERSARIAL_PERMISSION_DEFINER_PKG_NAME);
        assertFalse(permissionGranted(ADVERSARIAL_PERMISSION_USER_PKG_NAME, TEST_PERMISSION));
    }

    @SecurityTest
    @Test
    public void installPermissionShouldBeRevokedIfRemoved() throws Throwable {
        installApp(INSTALL_PERMISSION_DEFINER_APK_NAME);
        installApp(INSTALL_PERMISSION_USER_APK_NAME);
        assertTrue(permissionGranted(INSTALL_PERMISSION_USER_PKG_NAME, TEST_INSTALL_PERMISSION));

        // Uninstall the app which defines the install permission, and install another app
        // redefining it as a runtime permission.
        uninstallApp(INSTALL_PERMISSION_DEFINER_PKG_NAME);
        installApp(INSTALL_PERMISSION_ESCALATOR_APK_NAME);
        assertFalse(permissionGranted(INSTALL_PERMISSION_USER_PKG_NAME, TEST_INSTALL_PERMISSION));
    }

    @Test
    public void installPermissionShouldRemainGrantedAfterAppUpdate() throws Throwable {
        installApp(INSTALL_PERMISSION_DEFINER_APK_NAME);
        installApp(INSTALL_PERMISSION_USER_APK_NAME);
        assertTrue(permissionGranted(INSTALL_PERMISSION_USER_PKG_NAME, TEST_INSTALL_PERMISSION));

        // Install the app which defines the install permission again, similar to updating the app.
        installApp(INSTALL_PERMISSION_DEFINER_APK_NAME);
        assertTrue(permissionGranted(INSTALL_PERMISSION_USER_PKG_NAME, TEST_INSTALL_PERMISSION));
    }

    @Test
    public void installPermissionDependencyTest() throws Throwable {
        installApp(INSTALL_PERMISSION_USER_APK_NAME);
        // Should not have the permission auto-granted
        assertFalse(permissionGranted(INSTALL_PERMISSION_USER_PKG_NAME, TEST_INSTALL_PERMISSION));

        // Now install the permission definer; user package should have the permission auto granted
        installApp(INSTALL_PERMISSION_DEFINER_APK_NAME);
        installApp(INSTALL_PERMISSION_USER_APK_NAME);
        assertTrue(permissionGranted(INSTALL_PERMISSION_USER_PKG_NAME, TEST_INSTALL_PERMISSION));

        // Now uninstall the permission definer; the user package's permission should be revoked
        uninstallApp(INSTALL_PERMISSION_DEFINER_PKG_NAME);
        assertFalse(permissionGranted(INSTALL_PERMISSION_USER_PKG_NAME, TEST_INSTALL_PERMISSION));
    }
}
