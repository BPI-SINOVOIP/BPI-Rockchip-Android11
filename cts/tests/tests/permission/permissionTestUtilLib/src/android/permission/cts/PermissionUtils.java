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
import static android.Manifest.permission.PACKAGE_USAGE_STATS;
import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_FOREGROUND;
import static android.app.AppOpsManager.MODE_IGNORED;
import static android.app.AppOpsManager.OPSTR_GET_USAGE_STATS;
import static android.app.AppOpsManager.permissionToOp;
import static android.content.pm.PackageManager.FLAG_PERMISSION_REVIEW_REQUIRED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_REVOKE_ON_UPGRADE;
import static android.content.pm.PackageManager.FLAG_PERMISSION_REVOKE_WHEN_REQUESTED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_USER_FIXED;
import static android.content.pm.PackageManager.FLAG_PERMISSION_USER_SET;
import static android.content.pm.PackageManager.GET_PERMISSIONS;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.content.pm.PermissionInfo.PROTECTION_DANGEROUS;

import static com.android.compatibility.common.util.SystemUtil.callWithShellPermissionIdentity;
import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.app.AppOpsManager;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PermissionInfo;
import android.os.Build;
import android.os.Process;
import android.os.UserHandle;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Common utils for permission tests
 */
public class PermissionUtils {
    private static String GRANT_RUNTIME_PERMISSIONS = "android.permission.GRANT_RUNTIME_PERMISSIONS";
    private static String MANAGE_APP_OPS_MODES = "android.permission.MANAGE_APP_OPS_MODES";

    private static final int TESTED_FLAGS = FLAG_PERMISSION_USER_SET | FLAG_PERMISSION_USER_FIXED
            | FLAG_PERMISSION_REVOKE_ON_UPGRADE | FLAG_PERMISSION_REVIEW_REQUIRED
            | FLAG_PERMISSION_REVOKE_WHEN_REQUESTED;

    private static final Context sContext =
            InstrumentationRegistry.getInstrumentation().getTargetContext();
    private static final UiAutomation sUiAutomation =
            InstrumentationRegistry.getInstrumentation().getUiAutomation();

    private PermissionUtils() {
        // this class should never be instantiated
    }

    /**
     * Get the state of an app-op.
     *
     * @param packageName The package the app-op belongs to
     * @param permission The permission the app-op belongs to
     *
     * @return The mode the op is on
     */
    public static int getAppOp(@NonNull String packageName, @NonNull String permission)
            throws Exception {
        return sContext.getSystemService(AppOpsManager.class).unsafeCheckOpRaw(
                        permissionToOp(permission),
                        sContext.getPackageManager().getPackageUid(packageName, 0), packageName);
    }

    /**
     * Install an APK.
     *
     * @param apkFile The apk to install
     */
    public static void install(@NonNull String apkFile) {
        final int sdkVersion = Build.VERSION.SDK_INT
                + (Build.VERSION.RELEASE_OR_CODENAME.equals("REL") ? 0 : 1);
        boolean forceQueryable = sdkVersion > Build.VERSION_CODES.Q;
        runShellCommand("pm install -r --force-sdk "
                + (forceQueryable ? "--force-queryable " : "")
                + apkFile);
    }

    /**
     * Uninstall a package.
     *
     * @param packageName Name of package to be uninstalled
     */
    public static void uninstallApp(@NonNull String packageName) {
        runShellCommand("pm uninstall " + packageName);
    }

    /**
     * Set a new state for an app-op (using the permission-name)
     *
     * @param packageName The package the app-op belongs to
     * @param permission The permission the app-op belongs to
     * @param mode The new mode
     */
    public static void setAppOp(@NonNull String packageName, @NonNull String permission, int mode) {
        setAppOpByName(packageName, permissionToOp(permission), mode);
    }

    /**
     * Set a new state for an app-op (using the app-op-name)
     *
     * @param packageName The package the app-op belongs to
     * @param op The name of the op
     * @param mode The new mode
     */
    public static void setAppOpByName(@NonNull String packageName, @NonNull String op, int mode) {
        runWithShellPermissionIdentity(
                () -> sContext.getSystemService(AppOpsManager.class).setUidMode(op,
                        sContext.getPackageManager().getPackageUid(packageName, 0), mode),
                MANAGE_APP_OPS_MODES);
    }

    /**
     * Checks a permission. Does <u>not</u> check the appOp.
     *
     * <p>Users should use {@link #isGranted} instead.
     *
     * @param packageName The package that might have the permission granted
     * @param permission The permission that might be granted
     *
     * @return {@code true} iff the permission is granted
     */
    public static boolean isPermissionGranted(@NonNull String packageName,
            @NonNull String permission) throws Exception {
        return sContext.checkPermission(permission, Process.myPid(),
                sContext.getPackageManager().getPackageUid(packageName, 0))
                == PERMISSION_GRANTED;
    }

    /**
     * Checks if a permission is granted for a package.
     *
     * <p>This correctly handles pre-M apps by checking the app-ops instead.
     * <p>This also correctly handles the location background permission, but does not handle any
     * other background permission
     *
     * @param packageName The package that might have the permission granted
     * @param permission The permission that might be granted
     *
     * @return {@code true} iff the permission is granted
     */
    public static boolean isGranted(@NonNull String packageName, @NonNull String permission)
            throws Exception {
        if (!isPermissionGranted(packageName, permission)) {
            return false;
        }

        if (permission.equals(ACCESS_BACKGROUND_LOCATION)) {
            // The app-op for background location is encoded into the mode of the foreground
            // location
            return getAppOp(packageName, ACCESS_COARSE_LOCATION) == MODE_ALLOWED;
        } else {
            int mode = getAppOp(packageName, permission);
            return mode == MODE_ALLOWED || mode == MODE_FOREGROUND;
        }
    }

    /**
     * Grant a permission to an app.
     *
     * <p>This correctly handles pre-M apps by setting the app-ops.
     * <p>This also correctly handles the location background permission, but does not handle any
     * other background permission
     *
     * @param packageName The app that should have the permission granted
     * @param permission The permission to grant
     */
    public static void grantPermission(@NonNull String packageName, @NonNull String permission)
            throws Exception {
        sUiAutomation.grantRuntimePermission(packageName, permission);

        if (permission.equals(ACCESS_BACKGROUND_LOCATION)) {
            // The app-op for background location is encoded into the mode of the foreground
            // location
            if (isPermissionGranted(packageName, ACCESS_COARSE_LOCATION)) {
                setAppOp(packageName, ACCESS_COARSE_LOCATION, MODE_ALLOWED);
            } else {
                setAppOp(packageName, ACCESS_COARSE_LOCATION, MODE_FOREGROUND);
            }
        } else if (permission.equals(ACCESS_COARSE_LOCATION)) {
            // The app-op for location depends on the state of the bg location
            if (isPermissionGranted(packageName, ACCESS_BACKGROUND_LOCATION)) {
                setAppOp(packageName, ACCESS_COARSE_LOCATION, MODE_ALLOWED);
            } else {
                setAppOp(packageName, ACCESS_COARSE_LOCATION, MODE_FOREGROUND);
            }
        } else if (permission.equals(PACKAGE_USAGE_STATS)) {
            setAppOpByName(packageName, OPSTR_GET_USAGE_STATS, MODE_ALLOWED);
        } else if (permissionToOp(permission) != null) {
            setAppOp(packageName, permission, MODE_ALLOWED);
        }
    }

    /**
     * Revoke a permission from an app.
     *
     * <p>This correctly handles pre-M apps by setting the app-ops.
     * <p>This also correctly handles the location background permission, but does not handle any
     * other background permission
     *
     * @param packageName The app that should have the permission revoked
     * @param permission The permission to revoke
     */
    public static void revokePermission(@NonNull String packageName, @NonNull String permission)
            throws Exception {
        sUiAutomation.revokeRuntimePermission(packageName, permission);

        if (permission.equals(ACCESS_BACKGROUND_LOCATION)) {
            // The app-op for background location is encoded into the mode of the foreground
            // location
            if (isGranted(packageName, ACCESS_COARSE_LOCATION)) {
                setAppOp(packageName, ACCESS_COARSE_LOCATION, MODE_FOREGROUND);
            }
        } else if (permission.equals(PACKAGE_USAGE_STATS)) {
            setAppOpByName(packageName, OPSTR_GET_USAGE_STATS, MODE_IGNORED);
        } else if (permissionToOp(permission) != null) {
            setAppOp(packageName, permission, MODE_IGNORED);
        }
    }

    /**
     * Clear permission state (not app-op state) of package.
     *
     * @param packageName Package to clear
     */
    public static void clearAppState(@NonNull String packageName) {
        runShellCommand("pm clear --user current " + packageName);
    }

    /**
     * Get the flags of a permission.
     *
     * @param packageName Package the permission belongs to
     * @param permission Name of the permission
     *
     * @return Permission flags
     */
    public static int getPermissionFlags(@NonNull String packageName, @NonNull String permission) {
        try {
            return callWithShellPermissionIdentity(
                    () -> sContext.getPackageManager().getPermissionFlags(permission, packageName,
                            UserHandle.getUserHandleForUid(Process.myUid())) & TESTED_FLAGS,
                    GRANT_RUNTIME_PERMISSIONS);
        } catch (Exception e) {
            throw new IllegalStateException(e);
        }
    }

    /**
     * Set the flags of a permission.
     *
     * @param packageName Package the permission belongs to
     * @param permission Name of the permission
     * @param mask Mask of permissions to set
     * @param flags Permissions to set
     */
    public static void setPermissionFlags(@NonNull String packageName, @NonNull String permission,
            int mask, int flags) {
        runWithShellPermissionIdentity(
                () -> sContext.getPackageManager().updatePermissionFlags(permission, packageName,
                        mask, flags, UserHandle.getUserHandleForUid(Process.myUid())),
                GRANT_RUNTIME_PERMISSIONS);
    }

    /**
     * Get all permissions an app requests. This includes the split permissions.
     *
     * @param packageName The package that requests the permissions.
     *
     * @return The permissions requested by the app
     */
    public static @NonNull List<String> getPermissions(@NonNull String packageName)
            throws Exception {
        PackageInfo appInfo = sContext.getPackageManager().getPackageInfo(packageName,
                GET_PERMISSIONS);

        return Arrays.asList(appInfo.requestedPermissions);
    }

    /**
     * Get all runtime permissions that an app requests. This includes the split permissions.
     *
     * @param packageName The package that requests the permissions.
     *
     * @return The runtime permissions requested by the app
     */
    public static @NonNull List<String> getRuntimePermissions(@NonNull String packageName)
            throws Exception {
        ArrayList<String> runtimePermissions = new ArrayList<>();

        for (String perm : getPermissions(packageName)) {
            PermissionInfo info = sContext.getPackageManager().getPermissionInfo(perm, 0);
            if ((info.getProtection() & PROTECTION_DANGEROUS) != 0) {
                runtimePermissions.add(perm);
            }
        }

        return runtimePermissions;
    }

}
