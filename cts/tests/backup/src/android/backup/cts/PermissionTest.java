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
 * limitations under the License
 */

package android.backup.cts;

import static android.Manifest.permission.ACCESS_BACKGROUND_LOCATION;
import static android.Manifest.permission.ACCESS_FINE_LOCATION;
import static android.Manifest.permission.READ_CONTACTS;
import static android.Manifest.permission.WRITE_CONTACTS;
import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_FOREGROUND;
import static android.app.AppOpsManager.MODE_IGNORED;
import static android.app.AppOpsManager.permissionToOp;
import static android.content.pm.PackageManager.FLAG_PERMISSION_REVIEW_REQUIRED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_USER_FIXED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_USER_SET;
import static android.content.pm.PackageManager.PERMISSION_DENIED;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.permission.cts.PermissionUtils.grantPermission;

import static com.android.compatibility.common.util.BackupUtils.LOCAL_TRANSPORT_TOKEN;
import static com.android.compatibility.common.util.SystemUtil.callWithShellPermissionIdentity;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.app.AppOpsManager;
import android.content.Context;
import android.os.ParcelFileDescriptor;
import android.platform.test.annotations.AppModeFull;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;

import com.android.compatibility.common.util.BackupUtils;
import com.android.compatibility.common.util.ShellUtils;

import java.io.IOException;
import java.io.InputStream;

/**
 * Verifies that restored permissions are the same with backup value.
 *
 * @see com.android.packageinstaller.permission.service.BackupHelper
 */
@AppModeFull
public class PermissionTest extends BaseBackupCtsTest {

    /** The name of the package of the apps under test */
    private static final String APP = "android.backup.permission";
    private static final String APP22 = "android.backup.permission22";

    /** The apk of the packages */
    private static final String APK_PATH = "/data/local/tmp/cts/backup/";
    private static final String APP_APK = APK_PATH + "CtsPermissionBackupApp.apk";
    private static final String APP22_APK = APK_PATH + "CtsPermissionBackupApp22.apk";

    /** The name of the package for backup */
    private static final String ANDROID_PACKAGE = "android";

    private static final Context sContext = InstrumentationRegistry.getTargetContext();
    private static final long TIMEOUT_MILLIS = 10000;

    private BackupUtils mBackupUtils =
            new BackupUtils() {
                @Override
                protected InputStream executeShellCommand(String command) throws IOException {
                    ParcelFileDescriptor pfd =
                            getInstrumentation().getUiAutomation().executeShellCommand(command);
                    return new ParcelFileDescriptor.AutoCloseInputStream(pfd);
                }
            };

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        resetApp(APP);
        resetApp(APP22);
    }

    /**
     * Test backup and restore of regular runtime permission.
     */
    public void testGrantDeniedRuntimePermission() throws Exception {
        grantPermission(APP, ACCESS_FINE_LOCATION);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> {
            assertEquals(PERMISSION_GRANTED, checkPermission(APP, ACCESS_FINE_LOCATION));
            assertEquals(PERMISSION_DENIED, checkPermission(APP, READ_CONTACTS));
        });
    }

    /**
     * Test backup and restore of pre-M regular runtime permission.
     */
    public void testGrantDeniedRuntimePermission22() throws Exception {
        setAppOp(APP22, READ_CONTACTS, MODE_IGNORED);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP22);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> {
            assertEquals(MODE_IGNORED, getAppOp(APP22, READ_CONTACTS));
            assertEquals(MODE_ALLOWED, getAppOp(APP22, ACCESS_FINE_LOCATION));
        });
    }

    /**
     * Test backup and restore of foreground runtime permission.
     */
    public void testNoTriStateRuntimePermission() throws Exception {
        // Set a marker
        grantPermission(APP, WRITE_CONTACTS);

        // revoked is the default state. Hence mark the permissions as user set, so the permissions
        // are even backed up
        setFlag(APP, ACCESS_FINE_LOCATION, FLAG_PERMISSION_USER_SET);
        setFlag(APP, ACCESS_BACKGROUND_LOCATION, FLAG_PERMISSION_USER_SET);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> {
            // Wait until marker is set
            assertEquals(PERMISSION_GRANTED, checkPermission(APP, WRITE_CONTACTS));

            assertEquals(PERMISSION_DENIED, checkPermission(APP, ACCESS_FINE_LOCATION));
            assertEquals(PERMISSION_DENIED, checkPermission(APP, ACCESS_BACKGROUND_LOCATION));
            assertEquals(MODE_IGNORED, getAppOp(APP, ACCESS_FINE_LOCATION));
        });
    }

    /**
     * Test backup and restore of foreground runtime permission.
     */
    public void testNoTriStateRuntimePermission22() throws Exception {
        setAppOp(APP22, ACCESS_FINE_LOCATION, MODE_IGNORED);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP22);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> assertEquals(MODE_IGNORED, getAppOp(APP22, ACCESS_FINE_LOCATION)));
    }

    /**
     * Test backup and restore of foreground runtime permission.
     */
    public void testGrantForegroundRuntimePermission() throws Exception {
        grantPermission(APP, ACCESS_FINE_LOCATION);

        // revoked is the default state. Hence mark the permission as user set, so the permissions
        // are even backed up
        setFlag(APP, ACCESS_BACKGROUND_LOCATION, FLAG_PERMISSION_USER_SET);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> {
            assertEquals(PERMISSION_GRANTED, checkPermission(APP, ACCESS_FINE_LOCATION));
            assertEquals(PERMISSION_DENIED, checkPermission(APP, ACCESS_BACKGROUND_LOCATION));
            assertEquals(MODE_FOREGROUND, getAppOp(APP, ACCESS_FINE_LOCATION));
        });
    }

    /**
     * Test backup and restore of foreground runtime permission.
     */
    public void testGrantForegroundRuntimePermission22() throws Exception {
        setAppOp(APP22, ACCESS_FINE_LOCATION, MODE_FOREGROUND);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP22);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> assertEquals(MODE_FOREGROUND, getAppOp(APP22, ACCESS_FINE_LOCATION)));
    }

    /**
     * Test backup and restore of foreground runtime permission.
     */
    public void testGrantForegroundAndBackgroundRuntimePermission() throws Exception {
        grantPermission(APP, ACCESS_FINE_LOCATION);
        grantPermission(APP, ACCESS_BACKGROUND_LOCATION);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> {
            assertEquals(PERMISSION_GRANTED, checkPermission(APP, ACCESS_FINE_LOCATION));
            assertEquals(PERMISSION_GRANTED, checkPermission(APP, ACCESS_BACKGROUND_LOCATION));
            assertEquals(MODE_ALLOWED, getAppOp(APP, ACCESS_FINE_LOCATION));
        });
    }

    /**
     * Test backup and restore of foreground runtime permission.
     */
    public void testGrantForegroundAndBackgroundRuntimePermission22() throws Exception {
        // Set a marker
        setAppOp(APP22, WRITE_CONTACTS, MODE_IGNORED);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP22);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> {
            // Wait for marker
            assertEquals(MODE_IGNORED, getAppOp(APP22, WRITE_CONTACTS));

            assertEquals(MODE_ALLOWED, getAppOp(APP22, ACCESS_FINE_LOCATION));
        });
    }

    /**
     * Restore if the permission was reviewed
     */
    public void testRestorePermReviewed() throws Exception {
        clearFlag(APP22, WRITE_CONTACTS, FLAG_PERMISSION_REVIEW_REQUIRED);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP22);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> assertFalse(
                isFlagSet(APP22, WRITE_CONTACTS, FLAG_PERMISSION_REVIEW_REQUIRED)));
    }

    /**
     * Restore if the permission was user set
     */
    public void testRestoreUserSet() throws Exception {
        setFlag(APP, WRITE_CONTACTS, FLAG_PERMISSION_USER_SET);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> assertTrue(isFlagSet(APP, WRITE_CONTACTS, FLAG_PERMISSION_USER_SET)));
    }

    /**
     * Restore if the permission was user fixed
     */
    public void testRestoreUserFixed() throws Exception {
        setFlag(APP, WRITE_CONTACTS, FLAG_PERMISSION_USER_FIXED);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> assertTrue(isFlagSet(APP, WRITE_CONTACTS, FLAG_PERMISSION_USER_FIXED)));
    }

    /**
     * Restoring of a flag should not grant the permission
     */
    public void testRestoreOfFlagDoesNotGrantPermission() throws Exception {
        setFlag(APP, WRITE_CONTACTS, FLAG_PERMISSION_USER_FIXED);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);
        resetApp(APP);
        mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

        eventually(() -> assertEquals(PERMISSION_DENIED, checkPermission(APP, WRITE_CONTACTS)));
    }

    /**
     * Test backup and delayed restore of regular runtime permission.
     */
    public void testDelayedRestore() throws Exception {
        grantPermission(APP, ACCESS_FINE_LOCATION);

        setAppOp(APP22, READ_CONTACTS, MODE_IGNORED);

        mBackupUtils.backupNowAndAssertSuccess(ANDROID_PACKAGE);

        uninstall(APP);
        uninstall(APP22);

        try {
            mBackupUtils.restoreAndAssertSuccess(LOCAL_TRANSPORT_TOKEN, ANDROID_PACKAGE);

            install(APP_APK);

            eventually(() -> assertEquals(PERMISSION_GRANTED,
                    checkPermission(APP, ACCESS_FINE_LOCATION)));

            install(APP22_APK);

            eventually(() -> assertEquals(MODE_IGNORED, getAppOp(APP22, READ_CONTACTS)));
        } finally {
            install(APP_APK);
            install(APP22_APK);
        }
    }

    private void install(String apk) {
        ShellUtils.runShellCommand("pm install -r " + apk);
    }

    private void uninstall(String packageName) {
        ShellUtils.runShellCommand("pm uninstall " + packageName);
    }

    private void resetApp(String packageName) {
        ShellUtils.runShellCommand("pm clear " + packageName);
        ShellUtils.runShellCommand("appops reset " + packageName);
    }

    /**
     * Make sure that a {@link Runnable} eventually finishes without throwing a {@link
     * Exception}.
     *
     * @param r The {@link Runnable} to run.
     */
    public static void eventually(@NonNull Runnable r) {
        long start = System.currentTimeMillis();

        while (true) {
            try {
                r.run();
                return;
            } catch (Throwable e) {
                if (System.currentTimeMillis() - start < TIMEOUT_MILLIS) {
                    try {
                        Thread.sleep(100);
                    } catch (InterruptedException ignored) {
                        throw new RuntimeException(e);
                    }
                } else {
                    throw e;
                }
            }
        }
    }

    private void setFlag(String app, String permission, int flag) {
        runWithShellPermissionIdentity(
                () -> sContext.getPackageManager().updatePermissionFlags(permission, app,
                        flag, flag, sContext.getUser()));
    }

    private void clearFlag(String app, String permission, int flag) {
        runWithShellPermissionIdentity(
                () -> sContext.getPackageManager().updatePermissionFlags(permission, app,
                        flag, 0, sContext.getUser()));
    }

    private boolean isFlagSet(String app, String permission, int flag) {
        try {
            return (callWithShellPermissionIdentity(
                    () -> sContext.getPackageManager().getPermissionFlags(permission, app,
                            sContext.getUser())) & flag) == flag;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private int checkPermission(String app, String permission) {
        return sContext.getPackageManager().checkPermission(permission, app);
    }

    private void setAppOp(String app, String permission, int mode) {
        runWithShellPermissionIdentity(
                () -> sContext.getSystemService(AppOpsManager.class).setUidMode(
                        permissionToOp(permission),
                        sContext.getPackageManager().getPackageUid(app, 0), mode));
    }

    private int getAppOp(String app, String permission) {
        try {
            return callWithShellPermissionIdentity(
                    () -> sContext.getSystemService(AppOpsManager.class).unsafeCheckOpRaw(
                            permissionToOp(permission),
                            sContext.getPackageManager().getPackageUid(app, 0), app));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
