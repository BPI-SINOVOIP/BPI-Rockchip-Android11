/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.compatibility.common.deviceinfo;

import android.annotation.TargetApi;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.os.Build;
import android.os.Process;
import com.android.compatibility.common.util.DeviceInfoStore;
import com.android.compatibility.common.util.PackageUtil;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * PackageDeviceInfo collector.
 */
@TargetApi(Build.VERSION_CODES.N)
public class PackageDeviceInfo extends DeviceInfo {

    private static final String PLATFORM = "android";
    private static final String PLATFORM_PERMISSION_PREFIX = "android.";

    private static final String PACKAGE = "package";
    private static final String NAME = "name";
    private static final String VERSION_NAME = "version_name";
    private static final String SYSTEM_PRIV = "system_priv";
    private static final String PRIV_APP_DIR = "/system/priv-app";
    private static final String MIN_SDK = "min_sdk";
    private static final String TARGET_SDK = "target_sdk";

    private static final String REQUESTED_PERMISSIONS = "requested_permissions";
    private static final String PERMISSION_NAME = "name";
    private static final String PERMISSION_FLAGS = "flags";
    private static final String PERMISSION_GROUP = "permission_group";
    private static final String PERMISSION_PROTECTION = "protection_level";
    private static final String PERMISSION_PROTECTION_FLAGS = "protection_level_flags";

    private static final String PERMISSION_TYPE = "type";
    private static final int PERMISSION_TYPE_SYSTEM = 1;
    private static final int PERMISSION_TYPE_OEM = 2;
    private static final int PERMISSION_TYPE_CUSTOM = 3;

    private static final String HAS_SYSTEM_UID = "has_system_uid";

    private static final String SHARES_INSTALL_PERMISSION = "shares_install_packages_permission";
    private static final String INSTALL_PACKAGES_PERMISSION = "android.permission.INSTALL_PACKAGES";

    private static final String SHA256_CERT = "sha256_cert";

    private static final String CONFIG_NOTIFICATION_ACCESS = "config_defaultListenerAccessPackages";
    private static final String HAS_DEFAULT_NOTIFICATION_ACCESS = "has_default_notification_access";

    private static final String UID = "uid";
    private static final String IS_ACTIVE_ADMIN = "is_active_admin";

    private static final String CONFIG_ACCESSIBILITY_SERVICE = "config_defaultAccessibilityService";
    private static final String DEFAULT_ACCESSIBILITY_SERVICE = "is_default_accessibility_service";


    @Override
    protected void collectDeviceInfo(DeviceInfoStore store) throws Exception {
        final PackageManager pm = getContext().getPackageManager();

        final List<PackageInfo> allPackages =
                pm.getInstalledPackages(PackageManager.GET_PERMISSIONS);
        final Set<String> defaultNotificationListeners =
                getColonSeparatedPackageList(CONFIG_NOTIFICATION_ACCESS);

        final Set<String> deviceAdminPackages = getActiveDeviceAdminPackages();

        final ComponentName defaultAccessibilityComponent = getDefaultAccessibilityComponent();

        // Platform permission data used to tag permissions information with sourcing information
        final PackageInfo platformInfo = pm.getPackageInfo(PLATFORM , PackageManager.GET_PERMISSIONS);
        final Set<String> platformPermissions = new HashSet<String>();
        for (PermissionInfo permission : platformInfo.permissions) {
          platformPermissions.add(permission.name);
        }

        store.startArray(PACKAGE);
        for (PackageInfo pkg : allPackages) {
            store.startGroup();
            store.addResult(NAME, pkg.packageName);
            store.addResult(VERSION_NAME, pkg.versionName);

            collectPermissions(store, pm, platformPermissions, pkg);
            collectionApplicationInfo(store, pm, pkg);

            store.addResult(HAS_DEFAULT_NOTIFICATION_ACCESS,
                    defaultNotificationListeners.contains(pkg.packageName));

            store.addResult(IS_ACTIVE_ADMIN, deviceAdminPackages.contains(pkg.packageName));

            boolean isDefaultAccessibilityComponent = false;
            if (defaultAccessibilityComponent != null) {
              isDefaultAccessibilityComponent = pkg.packageName.equals(
                      defaultAccessibilityComponent.getPackageName()
              );
            }
            store.addResult(DEFAULT_ACCESSIBILITY_SERVICE, isDefaultAccessibilityComponent);

            String sha256_cert = PackageUtil.computePackageSignatureDigest(pkg.packageName);
            store.addResult(SHA256_CERT, sha256_cert);

            store.endGroup();
        }
        store.endArray(); // "package"
    }

    private static void collectPermissions(DeviceInfoStore store,
                                           PackageManager pm,
                                           Set<String> systemPermissions,
                                           PackageInfo pkg) throws IOException
    {
        store.startArray(REQUESTED_PERMISSIONS);
        if (pkg.requestedPermissions != null && pkg.requestedPermissions.length > 0) {
            for (String permission : pkg.requestedPermissions) {
                try {
                    final PermissionInfo pi = pm.getPermissionInfo(permission, 0);

                    store.startGroup();
                    store.addResult(PERMISSION_NAME, permission);
                    writePermissionsDetails(pi, store);

                    if (permission == null) continue;

                    final boolean isPlatformPermission = systemPermissions.contains(permission);
                    if (isPlatformPermission) {
                      final boolean isAndroidPermission = permission.startsWith(PLATFORM_PERMISSION_PREFIX);
                      if (isAndroidPermission) {
                        store.addResult(PERMISSION_TYPE, PERMISSION_TYPE_SYSTEM);
                      } else {
                        store.addResult(PERMISSION_TYPE, PERMISSION_TYPE_OEM);
                      }
                    } else {
                      store.addResult(PERMISSION_TYPE, PERMISSION_TYPE_CUSTOM);
                    }

                    store.endGroup();
                } catch (PackageManager.NameNotFoundException e) {
                    // ignore unrecognized permission and continue
                }
            }
        }
        store.endArray();
    }

    private static void collectionApplicationInfo(DeviceInfoStore store,
                                                  PackageManager pm,
                                                  PackageInfo pkg) throws IOException {
        final ApplicationInfo appInfo = pkg.applicationInfo;
        if (appInfo != null) {
            String dir = appInfo.sourceDir;
            store.addResult(SYSTEM_PRIV, dir != null && dir.startsWith(PRIV_APP_DIR));

            store.addResult(MIN_SDK, appInfo.minSdkVersion);
            store.addResult(TARGET_SDK, appInfo.targetSdkVersion);

            store.addResult(HAS_SYSTEM_UID, appInfo.uid < Process.FIRST_APPLICATION_UID);

            final boolean canInstall = sharesUidWithInstallerPackage(pm, appInfo.uid);
            store.addResult(SHARES_INSTALL_PERMISSION, canInstall);

            store.addResult(UID, appInfo.uid);
        }
    }

    private static boolean sharesUidWithInstallerPackage(PackageManager pm, int uid) {
        final String[] sharesUidWith = pm.getPackagesForUid(uid);

        if (sharesUidWith == null) {
            return false;
        }

        // Approx 20 permissions per package for rough estimate of sizing
        final int capacity = sharesUidWith.length * 20;
        final List<String> sharedPermissions = new ArrayList<>(capacity);
        for (String pkg :sharesUidWith){
            try {
                final PackageInfo info = pm.getPackageInfo(pkg, PackageManager.GET_PERMISSIONS);

                if (info.requestedPermissions == null) {
                    continue;
                }

                for (String p : info.requestedPermissions) {
                    if (p != null) {
                        sharedPermissions.add(p);
                    }
                }
            } catch (PackageManager.NameNotFoundException e) {
                // ignore, continue
            }
        }

        return sharedPermissions.contains(PackageDeviceInfo.INSTALL_PACKAGES_PERMISSION);
    }

    private static void writePermissionsDetails(PermissionInfo pi, DeviceInfoStore store)
            throws IOException {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            store.addResult(PERMISSION_FLAGS, pi.flags);
        } else {
            store.addResult(PERMISSION_FLAGS, 0);
        }

        store.addResult(PERMISSION_GROUP, pi.group);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            store.addResult(PERMISSION_PROTECTION, pi.getProtection());
            store.addResult(PERMISSION_PROTECTION_FLAGS, pi.getProtectionFlags());
        } else {
            store.addResult(PERMISSION_PROTECTION,
                    pi.protectionLevel & PermissionInfo.PROTECTION_MASK_BASE);
            store.addResult(PERMISSION_PROTECTION_FLAGS,
                    pi.protectionLevel & ~PermissionInfo.PROTECTION_MASK_BASE);
        }
    }

    private Set<String> getActiveDeviceAdminPackages() {
        final DevicePolicyManager dpm = (DevicePolicyManager)
                getContext().getSystemService(Context.DEVICE_POLICY_SERVICE);

        final List<ComponentName> components = dpm.getActiveAdmins();
        if (components == null) {
            return new HashSet<>(0);
        }

        final HashSet<String> packages = new HashSet<>(components.size());
        for (ComponentName component : components) {
            packages.add(component.getPackageName());
        }

        return packages;
    }

    private ComponentName getDefaultAccessibilityComponent() {
        final String defaultAccessibilityServiceComponent =
                getRawDeviceConfig(CONFIG_ACCESSIBILITY_SERVICE);
        return ComponentName.unflattenFromString(defaultAccessibilityServiceComponent);
    }

    /**
     * Parses and returns a set of package ids from a configuration value
     * e.g config_defaultListenerAccessPackages
     **/
    private Set<String> getColonSeparatedPackageList(String name) {
        String raw = getRawDeviceConfig(name);
        String[] packages = raw.split(":");
        return new HashSet<>(Arrays.asList(packages));
    }

    /** Returns the value of a device configuration setting available in android.internal.R.* **/
    private String getRawDeviceConfig(String name) {
        return getContext()
                .getResources()
                .getString(getDeviceResourcesIdentifier(name, "string"));
    }

    private int getDeviceResourcesIdentifier(String name, String type) {
        return getContext()
                .getResources()
                .getIdentifier(name, type, "android");
    }
}

