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

package android.permission2.cts;

import static android.permission.cts.PermissionUtils.isGranted;
import static android.permission.cts.PermissionUtils.isPermissionGranted;

import static com.android.compatibility.common.util.SystemUtil.eventually;
import static com.android.compatibility.common.util.SystemUtil.runShellCommand;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.Manifest;
import android.Manifest.permission;
import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.platform.test.annotations.AppModeFull;
import android.util.ArraySet;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.ThrowingRunnable;

import org.junit.After;
import org.junit.Test;

import java.util.Collections;
import java.util.Set;

import javax.annotation.Nullable;

/** Tests for restricted storage-related permissions. */
public class RestrictedStoragePermissionTest {
    private static final String APK_USES_STORAGE_DEFAULT_22 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserDefaultSdk22.apk";

    private static final String APK_USES_STORAGE_DEFAULT_28 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserDefaultSdk28.apk";

    private static final String APK_USES_STORAGE_DEFAULT_29 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserDefaultSdk29.apk";

    private static final String APK_USES_STORAGE_OPT_IN_22 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserOptInSdk22.apk";

    private static final String APK_USES_STORAGE_OPT_IN_28 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserOptInSdk28.apk";

    private static final String APK_USES_STORAGE_OPT_OUT_29 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserOptOutSdk29.apk";

    private static final String APK_USES_STORAGE_OPT_OUT_30 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserOptOutSdk30.apk";

    private static final String APK_USES_STORAGE_PRESERVED_OPT_OUT_30 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsPreservedUserOptOutSdk30.apk";

    private static final String PKG = "android.permission2.cts.restrictedpermissionuser";

    @Test
    @AppModeFull
    public void testTargetingSdk22DefaultWhitelistedHasFullAccess() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_22, null /*whitelistedPermissions*/);

        // Check expected storage mode
        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk22OptInWhitelistedHasIsolatedAccess() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_IN_22, null /*whitelistedPermissions*/);

        // Check expected storage mode
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk28DefaultWhitelistedHasFullAccess() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_28, null /*whitelistedPermissions*/);

        // Check expected storage mode
        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk28OptInWhitelistedHasIsolatedAccess() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_IN_28, null /*whitelistedPermissions*/);

        // Check expected storage mode
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29DefaultWhitelistedHasIsolatedAccess() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_29, Collections.emptySet());

        // Check expected storage mode
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29DefaultNotWhitelistedHasIsolatedAccess() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_29, null /*whitelistedPermissions*/);

        // Check expected storage mode
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29OptOutWhitelistedHasFullAccess() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_OUT_29, null /*whitelistedPermissions*/);

        // Check expected storage mode
        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29OptOutNotWhitelistedHasIsolatedAccess() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_OUT_29, Collections.emptySet());

        // Check expected storage mode
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29CanOptOutViaUpdate() throws Exception {
        installApp(APK_USES_STORAGE_DEFAULT_29, null);
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29CanOptOutViaDowngradeTo28() throws Exception {
        installApp(APK_USES_STORAGE_DEFAULT_29, null);
        installApp(APK_USES_STORAGE_DEFAULT_28, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk30_cannotOptOut() throws Exception {
        // Apps that target R and above cannot opt out of isolated storage.
        installApp(APK_USES_STORAGE_OPT_OUT_30, null);

        // Check expected storage mode
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk28CanRemoveOptInViaUpdate() throws Exception {
        installApp(APK_USES_STORAGE_OPT_IN_28, null);
        installApp(APK_USES_STORAGE_DEFAULT_28, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk28CanRemoveOptInByOptingOut() throws Exception {
        installApp(APK_USES_STORAGE_OPT_IN_28, null);
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk28DoesNotLoseAccessWhenOptingIn() throws Exception {
        installApp(APK_USES_STORAGE_DEFAULT_28, null);
        assertHasFullStorageAccess();
        installApp(APK_USES_STORAGE_OPT_IN_28, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk28DoesNotLoseAccessViaUpdate() throws Exception {
        installApp(APK_USES_STORAGE_DEFAULT_28, null);
        assertHasFullStorageAccess();
        installApp(APK_USES_STORAGE_DEFAULT_29, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29DoesNotLoseAccessViaUpdate() throws Exception {
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);
        assertHasFullStorageAccess();
        installApp(APK_USES_STORAGE_DEFAULT_29, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29DoesNotLoseAccessWhenOptingIn() throws Exception {
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);
        assertHasFullStorageAccess();
        installApp(APK_USES_STORAGE_OPT_IN_28, null);

        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk29LosesAccessViaUpdateToTargetSdk30() throws Exception {
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);
        assertHasFullStorageAccess();

        installApp(APK_USES_STORAGE_OPT_OUT_30, null); // opt-out is a no-op on 30
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testTargetingSdk28LosesAccessViaUpdateToTargetSdk30() throws Exception {
        installApp(APK_USES_STORAGE_DEFAULT_28, null);
        assertHasFullStorageAccess();

        installApp(APK_USES_STORAGE_OPT_OUT_30, null); // opt-out is a no-op on 30
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testCannotControlStorageWhitelistPostInstall1() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_28, null /*whitelistedPermissions*/);

        // Check expected state of restricted permissions.
        assertCannotUnWhitelistStorage();
    }

    @Test
    @AppModeFull
    public void testCannotControlStorageWhitelistPostInstall2() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_28, Collections.emptySet());

        // Check expected state of restricted permissions.
        assertCannotWhitelistStorage();
    }

    @Test
    @AppModeFull
    public void cannotGrantStorageTargetingSdk22NotWhitelisted() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_22, Collections.emptySet());

        eventually(() -> {
            // Could not grant permission+app-op as targetSDK<29 and not whitelisted
            assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isFalse();

            // Permissions are always granted for pre-23 apps
            assertThat(isPermissionGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE))
                    .isTrue();
        });
    }

    @Test
    @AppModeFull
    public void cannotGrantStorageTargetingSdk22OptInNotWhitelisted() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_IN_22, Collections.emptySet());

        eventually(() -> {
            // Could not grant permission as targetSDK<29 and not whitelisted
            assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isFalse();

            // Permissions are always granted for pre-23 apps
            assertThat(isPermissionGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE))
                    .isTrue();
        });
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk22Whitelisted() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_22, null);

        // Could grant permission
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk22OptInWhitelisted() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_IN_22, null);

        // Could grant permission
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void cannotGrantStorageTargetingSdk28NotWhitelisted() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_28, Collections.emptySet());

        // Could not grant permission as targetSDK<29 and not whitelisted
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isFalse());
    }

    @Test
    @AppModeFull
    public void cannotGrantStorageTargetingSdk28OptInNotWhitelisted() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_IN_28, Collections.emptySet());

        // Could not grant permission as targetSDK<29 and not whitelisted
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isFalse());
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk28Whitelisted() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_28, null);

        // Could grant permission
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk28OptInWhitelisted() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_IN_28, null);

        // Could grant permission
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk29NotWhitelisted() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_29, Collections.emptySet());

        // Could grant permission as targetSDK=29 apps can always grant
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk29OptOutNotWhitelisted() throws Exception {
        // Install with no whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_OUT_29, Collections.emptySet());

        // Could grant permission as targetSDK=29 apps can always grant
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk29Whitelisted() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_DEFAULT_29, null);

        // Could grant permission as targetSDK=29 apps can always grant
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void canGrantStorageTargetingSdk29OptOutWhitelisted() throws Exception {
        // Install with whitelisted permissions.
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);

        // Could grant permission as targetSDK=29 apps can always grant
        eventually(() ->
                assertThat(isGranted(PKG, Manifest.permission.READ_EXTERNAL_STORAGE)).isTrue());
    }

    @Test
    @AppModeFull
    public void restrictedWritePermDoesNotImplyIsolatedStorageAccess() throws Exception {
        // Install with whitelisted read permissions.
        installApp(
                APK_USES_STORAGE_OPT_OUT_29,
                Collections.singleton(Manifest.permission.READ_EXTERNAL_STORAGE));

        // It does not matter that write is restricted as the storage access level is only
        // controlled by the read perm
        assertHasFullStorageAccess();
    }

    @Test
    @AppModeFull
    public void whitelistedWritePermDoesNotImplyFullStorageAccess() throws Exception {
        // Install with whitelisted read permissions.
        installApp(
                APK_USES_STORAGE_OPT_OUT_29,
                Collections.singleton(Manifest.permission.WRITE_EXTERNAL_STORAGE));

        // It does not matter that write is white listed as the storage access level is only
        // controlled by the read perm
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testStorageTargetingSdk30CanPreserveLegacyOnUpdateFromLegacy() throws Exception {
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);
        assertHasFullStorageAccess();

        // Updating with the flag preserves legacy
        installApp(APK_USES_STORAGE_PRESERVED_OPT_OUT_30, null);
        assertHasFullStorageAccess();

        // And with the flag still preserves legacy
        installApp(APK_USES_STORAGE_PRESERVED_OPT_OUT_30, null);
        assertHasFullStorageAccess();

        // But without the flag loses legacy
        installApp(APK_USES_STORAGE_OPT_OUT_30, null);
        assertHasIsolatedStorageAccess();

        // And again with the flag doesn't bring back legacy
        installApp(APK_USES_STORAGE_PRESERVED_OPT_OUT_30, null);
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testStorageTargetingSdk30CannotPreserveLegacyAfterLegacyUninstall()
            throws Exception {
        installApp(APK_USES_STORAGE_OPT_OUT_29, null);
        assertHasFullStorageAccess();

        runShellCommand("pm uninstall " + PKG);

        installApp(APK_USES_STORAGE_PRESERVED_OPT_OUT_30, null);
        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testStorageTargetingSdk30CannotPreserveLegacyOnUpdateFromNonLegacy()
            throws Exception {
        installApp(APK_USES_STORAGE_DEFAULT_29, null);
        installApp(APK_USES_STORAGE_PRESERVED_OPT_OUT_30, null);

        assertHasIsolatedStorageAccess();
    }

    @Test
    @AppModeFull
    public void testStorageTargetingSdk30CannotPreserveLegacyOnInstall() throws Exception {
        installApp(APK_USES_STORAGE_PRESERVED_OPT_OUT_30, null);

        assertHasIsolatedStorageAccess();
    }

    private void assertHasFullStorageAccess() throws Exception {
        runWithShellPermissionIdentity(() -> {
            AppOpsManager appOpsManager = getContext().getSystemService(AppOpsManager.class);
            final int uid = getContext().getPackageManager().getPackageUid(PKG, 0);
            eventually(() -> assertThat(appOpsManager.unsafeCheckOpRawNoThrow(
                    AppOpsManager.OPSTR_LEGACY_STORAGE,
                    uid, PKG)).isEqualTo(AppOpsManager.MODE_ALLOWED));
        });
    }

    private void assertHasIsolatedStorageAccess() throws Exception {
        runWithShellPermissionIdentity(() -> {
            AppOpsManager appOpsManager = getContext().getSystemService(AppOpsManager.class);
            final int uid = getContext().getPackageManager().getPackageUid(PKG, 0);
            eventually(() -> assertThat(appOpsManager.unsafeCheckOpRawNoThrow(
                    AppOpsManager.OPSTR_LEGACY_STORAGE,
                    uid, PKG)).isNotEqualTo(AppOpsManager.MODE_ALLOWED));
        });
    }

    private void assertCannotWhitelistStorage() throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();

        runWithShellPermissionIdentity(() -> {
            // Assert added only to none whitelist.
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM
                            | PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .doesNotContain(permission.READ_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM
                            | PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .doesNotContain(permission.WRITE_EXTERNAL_STORAGE);
        });

        // Assert we cannot add.
        try {
            packageManager.addWhitelistedRestrictedPermission(
                    PKG,
                    permission.READ_EXTERNAL_STORAGE,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER);
            fail();
        } catch (SecurityException expected) {
        }
        try {
            packageManager.addWhitelistedRestrictedPermission(
                    PKG,
                    permission.WRITE_EXTERNAL_STORAGE,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER);
            fail();
        } catch (SecurityException expected) {
        }

        runWithShellPermissionIdentity(() -> {
            // Assert added only to none whitelist.
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM
                            | PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .doesNotContain(permission.READ_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM
                            | PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .doesNotContain(permission.WRITE_EXTERNAL_STORAGE);
        });
    }

    private void assertCannotUnWhitelistStorage() throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();

        runWithShellPermissionIdentity(() -> {
            // Assert added only to install whitelist.
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .contains(permission.READ_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .contains(permission.WRITE_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM))
                    .doesNotContain(permission.READ_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM))
                    .doesNotContain(permission.WRITE_EXTERNAL_STORAGE);
        });

        try {
            // Assert we cannot remove.
            packageManager.removeWhitelistedRestrictedPermission(
                    PKG,
                    permission.READ_EXTERNAL_STORAGE,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER);
            fail();
        } catch (SecurityException expected) {
        }
        try {
            packageManager.removeWhitelistedRestrictedPermission(
                    PKG,
                    permission.WRITE_EXTERNAL_STORAGE,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER);
            fail();
        } catch (SecurityException expected) {
        }

        runWithShellPermissionIdentity(() -> {
            // Assert added only to install whitelist.
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .contains(permission.READ_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER))
                    .contains(permission.WRITE_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM))
                    .doesNotContain(permission.READ_EXTERNAL_STORAGE);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE
                            | PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM))
                    .doesNotContain(permission.WRITE_EXTERNAL_STORAGE);
        });
    }

    private @NonNull Set<String> getPermissionsOfAppWithAnyOfFlags(int flags) throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
        final Set<String> restrictedPermissions = new ArraySet<>();
        for (String permission : getRequestedPermissionsOfApp()) {
            PermissionInfo permInfo = packageManager.getPermissionInfo(permission, 0);

            if ((permInfo.flags & flags) != 0) {
                restrictedPermissions.add(permission);
            }
        }
        return restrictedPermissions;
    }

    private @NonNull Set<String> getRestrictedPermissionsOfApp() throws Exception {
        return getPermissionsOfAppWithAnyOfFlags(
                PermissionInfo.FLAG_HARD_RESTRICTED | PermissionInfo.FLAG_SOFT_RESTRICTED);
    }

    private @NonNull String[] getRequestedPermissionsOfApp() throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
        final PackageInfo packageInfo =
                packageManager.getPackageInfo(PKG, PackageManager.GET_PERMISSIONS);
        return packageInfo.requestedPermissions;
    }

    private static @NonNull Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getContext();
    }

    private static void runWithShellPermissionIdentity(@NonNull ThrowingRunnable command)
            throws Exception {
        InstrumentationRegistry.getInstrumentation()
                .getUiAutomation()
                .adoptShellPermissionIdentity();
        try {
            command.run();
        } finally {
            InstrumentationRegistry.getInstrumentation()
                    .getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    /**
     * Install an app.
     *
     * @param app The app to be installed
     * @param whitelistedPermissions The permission to be whitelisted. {@code null} == all
     * @param grantedPermissions The permission to be granted. {@code null} == all
     */
    private void installApp(
            @NonNull String app,
            @Nullable Set<String> whitelistedPermissions)
            throws Exception {
        // Install the app and whitelist/grant all permission if requested.
        String installResult = runShellCommand("pm install -r --restrict-permissions " + app);
        assertThat(installResult.trim()).isEqualTo("Success");

        final Set<String> adjustedWhitelistedPermissions;
        if (whitelistedPermissions == null) {
            adjustedWhitelistedPermissions = getRestrictedPermissionsOfApp();
        } else {
            adjustedWhitelistedPermissions = whitelistedPermissions;
        }

        final Set<String> adjustedGrantedPermissions = getRestrictedPermissionsOfApp();

        // Whitelist subset of permissions if requested
        runWithShellPermissionIdentity(() -> {
            final PackageManager packageManager = getContext().getPackageManager();
            for (String permission : adjustedWhitelistedPermissions) {
                packageManager.addWhitelistedRestrictedPermission(
                        PKG,
                        permission,
                        PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER);
            }
        });

        // Grant subset of permissions if requested
        runWithShellPermissionIdentity(() -> {
            final PackageManager packageManager = getContext().getPackageManager();
            for (String permission : adjustedGrantedPermissions) {
                packageManager.grantRuntimePermission(PKG, permission, getContext().getUser());
                packageManager.updatePermissionFlags(
                        permission,
                        PKG,
                        PackageManager.FLAG_PERMISSION_REVOKED_COMPAT,
                        0,
                        getContext().getUser());
            }
        });

        // Mark all permissions as reviewed as for pre-22 apps the restriction state might not be
        // applied until reviewed
        runWithShellPermissionIdentity(() -> {
            final PackageManager packageManager = getContext().getPackageManager();
            for (String permission : getRequestedPermissionsOfApp()) {
                packageManager.updatePermissionFlags(
                        permission,
                        PKG,
                        PackageManager.FLAG_PERMISSION_REVIEW_REQUIRED,
                        0,
                        getContext().getUser());
            }
        });
    }

    @After
    public void uninstallApp() {
        runShellCommand("pm uninstall " + PKG);
    }
}
