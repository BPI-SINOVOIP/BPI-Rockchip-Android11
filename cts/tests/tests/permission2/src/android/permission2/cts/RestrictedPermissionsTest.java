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
 * See the License for the specific language governing permissions andf
 * limitations under the License.
 */

package android.permission2.cts;

import static android.Manifest.permission.ACCESS_BACKGROUND_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.READ_SMS;
import static android.permission.cts.PermissionUtils.isGranted;
import static android.permission.cts.PermissionUtils.isPermissionGranted;

import static com.android.compatibility.common.util.SystemUtil.eventually;
import static com.android.compatibility.common.util.SystemUtil.runShellCommand;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.Manifest;
import android.Manifest.permission;
import android.app.AppOpsManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.IntentSender;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageInstaller.Session;
import android.content.pm.PackageInstaller.SessionParams;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.platform.test.annotations.AppModeFull;
import android.util.ArraySet;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.ThrowingRunnable;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.Collections;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nullable;

/**
 * Tests for restricted permission behaviors.
 */
public class RestrictedPermissionsTest {
    private static final String APK_USES_LOCATION_22 =
            "/data/local/tmp/cts/permissions2/CtsLocationPermissionsUserSdk22.apk";

    private static final String APK_USES_LOCATION_29 =
            "/data/local/tmp/cts/permissions2/CtsLocationPermissionsUserSdk29.apk";

    private static final String APK_USES_SMS_CALL_LOG_22 =
            "/data/local/tmp/cts/permissions2/CtsSMSCallLogPermissionsUserSdk22.apk";

    private static final String APK_NAME_USES_SMS_CALL_LOG_29 =
            "CtsSMSCallLogPermissionsUserSdk29.apk";

    private static final String APK_USES_SMS_CALL_LOG_29 =
            "/data/local/tmp/cts/permissions2/CtsSMSCallLogPermissionsUserSdk29.apk";

    private static final String APK_USES_STORAGE_DEFAULT_29 =
            "/data/local/tmp/cts/permissions2/CtsStoragePermissionsUserDefaultSdk29.apk";

    private static final String PKG = "android.permission2.cts.restrictedpermissionuser";

    private static final String APK_USES_SMS_RESTRICTED_SHARED_UID =
            "/data/local/tmp/cts/permissions2/CtsSMSRestrictedWithSharedUid.apk";

    private static final String PKG_USES_SMS_RESTRICTED_SHARED_UID =
            "android.permission2.cts.smswithshareduid.restricted";

    private static final String APK_USES_SMS_NOT_RESTRICTED_SHARED_UID =
            "/data/local/tmp/cts/permissions2/CtsSMSNotRestrictedWithSharedUid.apk";

    private static final String PKG_USES_SMS_NOT_RESTRICTED_SHARED_UID =
            "android.permission2.cts.smswithshareduid.notrestricted";

    private static final long UI_TIMEOUT = 5000L;

    private static @NonNull BroadcastReceiver sCommandReceiver;

    @BeforeClass
    public static void setUpOnce() {
        sCommandReceiver = new CommandBroadcastReceiver();
        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction("installRestrictedPermissionUserApp");
        intentFilter.addAction("uninstallApp");
        getContext().registerReceiver(sCommandReceiver, intentFilter);
    }

    @AfterClass
    public static void tearDownOnce() {
        getContext().unregisterReceiver(sCommandReceiver);
    }

    @Test
    @AppModeFull
    public void testDefaultAllRestrictedPermissionsWhitelistedAtInstall29() throws Exception {
        // Install with no changes to whitelisted permissions, not attempting to grant.
        installRestrictedPermissionUserApp(null /*whitelistedPermissions*/,
                Collections.EMPTY_SET /*grantedPermissions*/);

        // All restricted permission should be whitelisted.
        assertAllRestrictedPermissionWhitelisted();

        // No restricted permission should be granted.
        assertNoRestrictedPermissionGranted();
    }

    @Test
    @AppModeFull
    public void testSomeRestrictedPermissionsWhitelistedAtInstall29() throws Exception {
        // Whitelist only these permissions.
        final Set<String> whitelistedPermissions = new ArraySet<>(2);
        whitelistedPermissions.add(Manifest.permission.SEND_SMS);
        whitelistedPermissions.add(Manifest.permission.READ_CALL_LOG);

        // Install with some whitelisted permissions, not attempting to grant.
        installRestrictedPermissionUserApp(whitelistedPermissions,
                Collections.EMPTY_SET /*grantedPermissions*/);

        // Some restricted permission should be whitelisted.
        assertRestrictedPermissionWhitelisted(whitelistedPermissions);

        // No restricted permission should be granted.
        assertNoRestrictedPermissionGranted();
    }

    @Test
    @AppModeFull
    public void testNoneRestrictedPermissionWhitelistedAtInstall29() throws Exception {
        // Install with all whitelisted permissions, not attempting to grant.
        installRestrictedPermissionUserApp(Collections.emptySet(),
                Collections.EMPTY_SET /*grantedPermissions*/);

        // No restricted permission should be whitelisted.
        assertNoRestrictedPermissionWhitelisted();

        // No restricted permission should be granted.
        assertNoRestrictedPermissionGranted();
    }

    @Test
    @AppModeFull
    public void testDefaultAllRestrictedPermissionsWhitelistedAtInstall22() throws Exception {
        // Install with no changes to whitelisted permissions
        runShellCommand("pm install -g --force-queryable " + APK_USES_SMS_CALL_LOG_22);

        // All restricted permission should be whitelisted.
        assertAllRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void testSomeRestrictedPermissionsWhitelistedAtInstall22() throws Exception {
        // Whitelist only these permissions.
        final Set<String> whitelistedPermissions = new ArraySet<>(2);
        whitelistedPermissions.add(Manifest.permission.SEND_SMS);
        whitelistedPermissions.add(Manifest.permission.READ_CALL_LOG);

        // Install with some whitelisted permissions
        installApp(APK_USES_SMS_CALL_LOG_22, whitelistedPermissions, null /*grantedPermissions*/);

        // Some restricted permission should be whitelisted.
        assertRestrictedPermissionWhitelisted(whitelistedPermissions);
    }

    @Test
    @AppModeFull
    public void testNoneRestrictedPermissionWhitelistedAtInstall22() throws Exception {
        // Install with all whitelisted permissions
        installApp(APK_USES_SMS_CALL_LOG_22, Collections.emptySet(),
                null /*grantedPermissions*/);

        // No restricted permission should be whitelisted.
        assertNoRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void testLocationBackgroundPermissionWhitelistedAtInstall29() throws Exception {
        installApp(APK_USES_LOCATION_29, null, new ArraySet<>(Arrays.asList(ACCESS_FINE_LOCATION,
                ACCESS_BACKGROUND_LOCATION)));
        assertAllRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void testLocationBackgroundPermissionNotWhitelistedAtInstall29() throws Exception {
        installApp(APK_USES_LOCATION_29, Collections.emptySet(),
                Collections.singleton(ACCESS_FINE_LOCATION));
        assertNoRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void testLocationBackgroundPermissionWhitelistedAtInstall22() throws Exception {
        installApp(APK_USES_LOCATION_22, null, new ArraySet<>(Arrays.asList(ACCESS_FINE_LOCATION,
                ACCESS_BACKGROUND_LOCATION)));
        assertAllRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void testLocationBackgroundPermissionNotWhitelistedAtInstall22() throws Exception {
        installApp(APK_USES_LOCATION_22, Collections.emptySet(),
                Collections.singleton(ACCESS_FINE_LOCATION));
        assertNoRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void testSomeRestrictedPermissionsGrantedAtInstall() throws Exception {
        // Grant only these permissions.
        final Set<String> grantedPermissions = new ArraySet<>(1);
        grantedPermissions.add(Manifest.permission.SEND_SMS);
        grantedPermissions.add(Manifest.permission.READ_CALL_LOG);

        // Install with no whitelisted permissions attempting to grant.
        installRestrictedPermissionUserApp(null /*whitelistedPermissions*/, grantedPermissions);

        // All restricted permission should be whitelisted.
        assertAllRestrictedPermissionWhitelisted();

        // Some restricted permission should be granted.
        assertRestrictedPermissionGranted(grantedPermissions);
    }

    @Test
    @AppModeFull
    public void testCanGrantSoftRestrictedNotWhitelistedPermissions() throws Exception {
        try {
            final Set<String> grantedPermissions = new ArraySet<>();
            grantedPermissions.add(Manifest.permission.READ_EXTERNAL_STORAGE);
            grantedPermissions.add(permission.WRITE_EXTERNAL_STORAGE);

            installApp(APK_USES_STORAGE_DEFAULT_29, Collections.emptySet(), grantedPermissions);

            assertRestrictedPermissionGranted(grantedPermissions);
        } finally {
            uninstallApp();
        }
    }

    @Test
    @AppModeFull
    public void testAllRestrictedPermissionsGrantedAtInstall() throws Exception {
        // Install with whitelisted permissions attempting to grant.
        installRestrictedPermissionUserApp(null /*whitelistedPermissions*/,
                null);

        // All restricted permission should be whitelisted.
        assertAllRestrictedPermissionWhitelisted();

        // Some restricted permission should be granted.
        assertAllRestrictedPermissionGranted();
    }

    @Test
    @AppModeFull
    public void testWhitelistAccessControl() throws Exception {
        // Install with no whitelisted permissions not attempting to grant.
        installRestrictedPermissionUserApp(Collections.emptySet(), null);

        assertWeCannotReadOrWriteWhileShellCanReadAndWrite(
                PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM);

        assertWeCannotReadOrWriteWhileShellCanReadAndWrite(
                PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE);

        assertWeCannotReadOrWriteWhileShellCanReadAndWrite(
                PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER);
    }

    @Test
    @AppModeFull
    public void onSideLoadRestrictedPermissionsWhitelistingDefault() throws Exception {
        installRestrictedPermissionUserApp(new SessionParams(SessionParams.MODE_FULL_INSTALL));

        // All restricted permissions whitelisted on side-load by default
        assertAllRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void onSideLoadAllRestrictedPermissionsWhitelisted() throws Exception {
        SessionParams params = new SessionParams(SessionParams.MODE_FULL_INSTALL);
        params.setWhitelistedRestrictedPermissions(SessionParams.RESTRICTED_PERMISSIONS_ALL);

        installRestrictedPermissionUserApp(params);

        assertAllRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void onSideLoadWhitelistSomePermissions() throws Exception {
        Set<String> whitelistedPermissions = new ArraySet<>();
        whitelistedPermissions.add(Manifest.permission.SEND_SMS);
        whitelistedPermissions.add(Manifest.permission.READ_CALL_LOG);

        SessionParams params = new SessionParams(SessionParams.MODE_FULL_INSTALL);
        params.setWhitelistedRestrictedPermissions(whitelistedPermissions);

        installRestrictedPermissionUserApp(params);

        assertRestrictedPermissionWhitelisted(whitelistedPermissions);
    }

    @Test
    @AppModeFull
    public void onSideLoadWhitelistNoPermissions() throws Exception {
        SessionParams params = new SessionParams(SessionParams.MODE_FULL_INSTALL);
        params.setWhitelistedRestrictedPermissions(Collections.emptySet());

        installRestrictedPermissionUserApp(params);

        assertNoRestrictedPermissionWhitelisted();
    }

    @Test
    @AppModeFull
    public void shareUidBetweenRestrictedAndNotRestrictedApp() throws Exception {
        runShellCommand(
                "pm install -g --force-queryable --restrict-permissions "
                + APK_USES_SMS_RESTRICTED_SHARED_UID);
        runShellCommand("pm install -g --force-queryable "
                + APK_USES_SMS_NOT_RESTRICTED_SHARED_UID);

        eventually(
                () -> assertThat(isGranted(PKG_USES_SMS_RESTRICTED_SHARED_UID, READ_SMS)).isTrue());
        // The apps share a UID, hence the whitelisting is shared too
        assertThat(isGranted(PKG_USES_SMS_NOT_RESTRICTED_SHARED_UID, READ_SMS)).isTrue();
    }

    private static void installRestrictedPermissionUserApp(@NonNull SessionParams params)
            throws Exception {
        final CountDownLatch installLatch = new CountDownLatch(1);

        // Create an install result receiver.
        final BroadcastReceiver installReceiver = new BroadcastReceiver() {
            public void onReceive(Context context, Intent intent) {
                if (intent.getIntExtra(PackageInstaller.EXTRA_STATUS,
                        PackageInstaller.STATUS_FAILURE_INVALID)
                            == PackageInstaller.STATUS_SUCCESS) {
                    installLatch.countDown();
                }
            }
        };

        // Register the result receiver.
        final String action = "android.permission2.cts.ACTION_INSTALL_COMMIT";
        final IntentFilter intentFilter = new IntentFilter(action);
        getContext().registerReceiver(installReceiver, intentFilter);

        try {
            // Create a session.
            final PackageInstaller packageInstaller = getContext()
                    .getPackageManager().getPackageInstaller();
            final int sessionId = packageInstaller.createSession(params);
            final Session session = packageInstaller.openSession(sessionId);

            // Write the apk.
            try (
                    InputStream in = new BufferedInputStream(new FileInputStream(
                        new File(APK_USES_SMS_CALL_LOG_29)));
                    OutputStream out = session.openWrite(
                            APK_NAME_USES_SMS_CALL_LOG_29, 0, -1);
            ) {
                final byte[] buf = new byte[8192];
                int size;
                while ((size = in.read(buf)) != -1) {
                    out.write(buf, 0, size);
                }
            }

            final Intent intent = new Intent(action);
            final IntentSender intentSender = PendingIntent.getBroadcast(getContext(),
                    1, intent, PendingIntent.FLAG_ONE_SHOT).getIntentSender();

            // Commit as shell to avoid confirm UI
            runWithShellPermissionIdentity(() -> {
                session.commit(intentSender);
                installLatch.await(UI_TIMEOUT, TimeUnit.MILLISECONDS);
            });
        } finally {
            getContext().unregisterReceiver(installReceiver);
        }
    }

    private void assertWeCannotReadOrWriteWhileShellCanReadAndWrite(int whitelist)
            throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
        try {
            packageManager.getWhitelistedRestrictedPermissions(PKG, whitelist);
            fail();
        } catch (SecurityException expected) {
            /*ignore*/
        }
        try {
            packageManager.addWhitelistedRestrictedPermission(PKG,
                    permission.SEND_SMS, whitelist);
            fail();
        } catch (SecurityException expected) {
            /*ignore*/
        }
        runWithShellPermissionIdentity(() -> {
            packageManager.addWhitelistedRestrictedPermission(PKG,
                    permission.SEND_SMS, whitelist);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    whitelist)).contains(permission.SEND_SMS);
            packageManager.removeWhitelistedRestrictedPermission(PKG,
                    permission.SEND_SMS, whitelist);
            assertThat(packageManager.getWhitelistedRestrictedPermissions(PKG,
                    whitelist)).doesNotContain(permission.SEND_SMS);
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
        final PackageInfo packageInfo = packageManager.getPackageInfo(PKG,
                PackageManager.GET_PERMISSIONS);
        return packageInfo.requestedPermissions;
    }

    private void assertAllRestrictedPermissionWhitelisted() throws Exception {
        assertRestrictedPermissionWhitelisted(getRestrictedPermissionsOfApp());
    }

    private void assertNoRestrictedPermissionWhitelisted() throws Exception {
        assertRestrictedPermissionWhitelisted(
                Collections.EMPTY_SET /*expectedWhitelistedPermissions*/);
    }

    /**
     * Assert that the passed in restrictions are whitelisted and that their app-op is set
     * correctly.
     *
     * @param expectedWhitelistedPermissions The expected white listed permissions
     */
    private void assertRestrictedPermissionWhitelisted(
            @NonNull Set<String> expectedWhitelistedPermissions) throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
    eventually(() -> runWithShellPermissionIdentity(() -> {
            final AppOpsManager appOpsManager = getContext().getSystemService(AppOpsManager.class);
            final PackageInfo packageInfo = packageManager.getPackageInfo(PKG,
                    PackageManager.GET_PERMISSIONS);

            final Set<String> whitelistedPermissions = packageManager
                .getWhitelistedRestrictedPermissions(PKG,
                        PackageManager.FLAG_PERMISSION_WHITELIST_SYSTEM
                        | PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER
                        | PackageManager.FLAG_PERMISSION_WHITELIST_UPGRADE);

            assertThat(whitelistedPermissions).isNotNull();
            assertThat(whitelistedPermissions).named("Whitelisted permissions")
                    .containsExactlyElementsIn(expectedWhitelistedPermissions);

            // Also assert that apps ops are properly set
            for (String permission : getRestrictedPermissionsOfApp()) {
                String op = AppOpsManager.permissionToOp(permission);
                ArraySet<Integer> possibleModes = new ArraySet<>();

                if (permission.equals(Manifest.permission.ACCESS_BACKGROUND_LOCATION)) {
                    op = AppOpsManager.OPSTR_FINE_LOCATION;

                    // If permission is denied app-op might be allowed/fg or ignored. It does
                    // not matter. If permission is granted, it has to be allowed/fg.
                    if (isPermissionGranted(PKG, Manifest.permission.ACCESS_FINE_LOCATION)) {
                        if (expectedWhitelistedPermissions.contains(permission)
                                && isPermissionGranted(PKG, permission)) {
                            possibleModes.add(AppOpsManager.MODE_ALLOWED);
                        } else {
                            possibleModes.add(AppOpsManager.MODE_FOREGROUND);
                        }
                    } else {
                        possibleModes.add(AppOpsManager.MODE_IGNORED);
                        possibleModes.add(AppOpsManager.MODE_ALLOWED);
                        possibleModes.add(AppOpsManager.MODE_FOREGROUND);
                    }
                } else {
                    if (expectedWhitelistedPermissions.contains(permission)) {
                        // If permission is denied app-op might be allowed or ignored. It does not
                        // matter. If permission is granted, it has to be allowed.
                        possibleModes.add(AppOpsManager.MODE_ALLOWED);
                        if (!isPermissionGranted(PKG, permission)) {
                            possibleModes.add(AppOpsManager.MODE_IGNORED);
                        }
                    } else {
                        possibleModes.add(AppOpsManager.MODE_IGNORED);
                    }
                }

                assertThat(appOpsManager.unsafeCheckOpRawNoThrow(op,
                        packageInfo.applicationInfo.uid, PKG)).named(op).isIn(possibleModes);
            }
        }));
    }

    private void assertAllRestrictedPermissionGranted() throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
        final PackageInfo packageInfo = packageManager.getPackageInfo(
                PKG, PackageManager.GET_PERMISSIONS);
        if (packageInfo.requestedPermissions != null) {
            final int permissionCount = packageInfo.requestedPermissions.length;
            for (int i = 0; i < permissionCount; i++) {
                final String permission = packageInfo.requestedPermissions[i];
                final PermissionInfo permissionInfo = packageManager.getPermissionInfo(
                        permission, 0);
                if ((permissionInfo.flags & PermissionInfo.FLAG_HARD_RESTRICTED) != 0) {
                    assertThat((packageInfo.requestedPermissionsFlags[i]
                            & PackageInfo.REQUESTED_PERMISSION_GRANTED)).isNotEqualTo(0);
                }
            }
        }
    }

    private void assertNoRestrictedPermissionGranted() throws Exception {
        assertRestrictedPermissionGranted(Collections.EMPTY_SET);
    }

    private void assertRestrictedPermissionGranted(@NonNull Set<String> expectedGrantedPermissions)
            throws Exception {
        final PackageManager packageManager = getContext().getPackageManager();
        final PackageInfo packageInfo = packageManager.getPackageInfo(
                PKG, PackageManager.GET_PERMISSIONS);
        if (packageInfo.requestedPermissions != null) {
            final int permissionCount = packageInfo.requestedPermissions.length;
            for (int i = 0; i < permissionCount; i++) {
                final String permission = packageInfo.requestedPermissions[i];
                final PermissionInfo permissionInfo = packageManager.getPermissionInfo(
                        permission, 0);
                if ((permissionInfo.flags & PermissionInfo.FLAG_HARD_RESTRICTED) != 0
                        || (permissionInfo.flags & PermissionInfo.FLAG_SOFT_RESTRICTED) != 0) {
                    if (expectedGrantedPermissions.contains(permission)) {
                        assertThat((packageInfo.requestedPermissionsFlags[i]
                                & PackageInfo.REQUESTED_PERMISSION_GRANTED)).isNotEqualTo(0);
                    } else {
                        assertThat((packageInfo.requestedPermissionsFlags[i]
                                & PackageInfo.REQUESTED_PERMISSION_GRANTED)).isEqualTo(0);
                    }
                }
            }
        }
    }

    /**
     * Install {@link #APK_USES_SMS_CALL_LOG_29}.
     *
     * @param whitelistedPermissions The permission to be whitelisted. {@code null} == all
     * @param grantedPermissions The permission to be granted. {@code null} == all
     */
    private void installRestrictedPermissionUserApp(@Nullable Set<String> whitelistedPermissions,
            @Nullable Set<String> grantedPermissions) throws Exception {
        installApp(APK_USES_SMS_CALL_LOG_29, whitelistedPermissions, grantedPermissions);
    }

    /**
     * Install app and grant all permission.
     *
     * @param app The app to be installed
     * @param whitelistedPermissions The permission to be whitelisted. {@code null} == all
     */
    private void installApp(@NonNull String app, @Nullable Set<String> whitelistedPermissions)
            throws Exception {
        installApp(app, whitelistedPermissions, null /*grantedPermissions*/);
    }

    /**
     * Install an app.
     *
     * @param app The app to be installed
     * @param whitelistedPermissions The permission to be whitelisted. {@code null} == all
     * @param grantedPermissions The permission to be granted. {@code null} == all
     */
    private void installApp(@NonNull String app, @Nullable Set<String> whitelistedPermissions,
            @Nullable Set<String> grantedPermissions) throws Exception {
        // Install the app and whitelist/grant all permission if requested.
        String installResult = runShellCommand("pm install -r --force-queryable "
                + "--restrict-permissions " + app);
        assertThat(installResult.trim()).isEqualTo("Success");

        final Set<String> adjustedWhitelistedPermissions;
        if (whitelistedPermissions == null) {
            adjustedWhitelistedPermissions = getRestrictedPermissionsOfApp();
        } else {
            adjustedWhitelistedPermissions = whitelistedPermissions;
        }

        final Set<String> adjustedGrantedPermissions;
        if (grantedPermissions == null) {
            adjustedGrantedPermissions = getRestrictedPermissionsOfApp();
        } else {
            adjustedGrantedPermissions = grantedPermissions;
        }

        // Whitelist subset of permissions if requested
        runWithShellPermissionIdentity(() -> {
            final PackageManager packageManager = getContext().getPackageManager();
            for (String permission : adjustedWhitelistedPermissions) {
                packageManager.addWhitelistedRestrictedPermission(PKG, permission,
                        PackageManager.FLAG_PERMISSION_WHITELIST_INSTALLER);
            }
        });

        // Grant subset of permissions if requested
        runWithShellPermissionIdentity(() -> {
            final PackageManager packageManager = getContext().getPackageManager();
            for (String permission : adjustedGrantedPermissions) {
                packageManager.grantRuntimePermission(PKG, permission,
                        getContext().getUser());
                packageManager.updatePermissionFlags(permission, PKG,
                        PackageManager.FLAG_PERMISSION_REVOKED_COMPAT, 0, getContext().getUser());
            }
        });

        // Mark all permissions as reviewed as for pre-22 apps the restriction state might not be
        // applied until reviewed
        runWithShellPermissionIdentity(() -> {
            final PackageManager packageManager = getContext().getPackageManager();
            for (String permission : getRequestedPermissionsOfApp()) {
                packageManager.updatePermissionFlags(permission, PKG,
                        PackageManager.FLAG_PERMISSION_REVIEW_REQUIRED, 0,
                        getContext().getUser());
            }
        });
    }

    @After
    public void uninstallApp() {
        runShellCommand("pm uninstall " + PKG);
        runShellCommand("pm uninstall " + PKG_USES_SMS_NOT_RESTRICTED_SHARED_UID);
        runShellCommand("pm uninstall " + PKG_USES_SMS_RESTRICTED_SHARED_UID);
    }

    private static @NonNull Context getContext() {
        return InstrumentationRegistry.getInstrumentation().getContext();
    }

    private static void runWithShellPermissionIdentity(@NonNull ThrowingRunnable command)
            throws Exception {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity();
        try {
            command.run();
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }
}
