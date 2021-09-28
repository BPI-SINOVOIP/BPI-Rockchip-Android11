/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.permission2.cts;

import static android.content.pm.PackageInfo.REQUESTED_PERMISSION_GRANTED;
import static android.content.pm.PackageManager.GET_PERMISSIONS;
import static android.content.pm.PackageManager.MATCH_FACTORY_ONLY;
import static android.content.pm.PackageManager.MATCH_UNINSTALLED_PACKAGES;

import static com.google.common.collect.Maps.filterValues;
import static com.google.common.collect.Sets.difference;
import static com.google.common.collect.Sets.intersection;
import static com.google.common.collect.Sets.newHashSet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PermissionInfo;
import android.platform.test.annotations.AppModeFull;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PropertyUtil;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

/**
 * Tests enforcement of signature|privileged permission whitelist:
 * <ul>
 * <li>Report what is granted into the CTS log
 * <li>Ensure all priv permissions are exclusively granted to applications declared in
 * &lt;privapp-permissions&gt;
 * </ul>
 */
@AppModeFull(reason = "This test test platform properties, not capabilities of an apps")
@RunWith(AndroidJUnit4.class)
public class PrivappPermissionsTest {

    private static final String TAG = "PrivappPermissionsTest";

    private static final String PLATFORM_PACKAGE_NAME = "android";

    @Test
    public void privappPermissionsMustBeEnforced() {
        assertEquals("ro.control_privapp_permissions is not set to enforce",
                "enforce", PropertyUtil.getProperty("ro.control_privapp_permissions"));
    }

    @Test
    public void privappPermissionsNeedToBeWhitelisted() throws Exception {
        Set<String> platformPrivPermissions = new HashSet<>();
        PackageManager pm = InstrumentationRegistry.getContext().getPackageManager();
        PackageInfo platformPackage = pm.getPackageInfo(PLATFORM_PACKAGE_NAME,
                PackageManager.GET_PERMISSIONS);

        for (PermissionInfo permission : platformPackage.permissions) {
            int protectionLevel = permission.protectionLevel;
            if ((protectionLevel & PermissionInfo.PROTECTION_FLAG_PRIVILEGED) != 0) {
                platformPrivPermissions.add(permission.name);
            }
        }

        List<PackageInfo> installedPackages = pm
                .getInstalledPackages(MATCH_UNINSTALLED_PACKAGES | GET_PERMISSIONS);
        installedPackages.sort(Comparator.comparing(p -> p.packageName));

        Map<String, Set<String>> packagesGrantedNotInWhitelist = new HashMap<>();
        Map<String, Set<String>> packagesNotGrantedNotRemovedNotInDenylist = new HashMap<>();
        for (PackageInfo pkg : installedPackages) {
            String packageName = pkg.packageName;
            if (!pkg.applicationInfo.isPrivilegedApp()
                    || PLATFORM_PACKAGE_NAME.equals(packageName)) {
                continue;
            }

            PackageInfo factoryPkg = pm
                    .getPackageInfo(packageName, MATCH_FACTORY_ONLY | GET_PERMISSIONS
                        | MATCH_UNINSTALLED_PACKAGES);

            assertNotNull("No system image version found for " + packageName, factoryPkg);

            Set<String> factoryRequestedPrivPermissions;
            if (factoryPkg.requestedPermissions == null) {
                factoryRequestedPrivPermissions = Collections.emptySet();
            } else {
                factoryRequestedPrivPermissions = intersection(
                        newHashSet(factoryPkg.requestedPermissions), platformPrivPermissions);
            }

            Map<String, Boolean> requestedPrivPermissions = new ArrayMap<>();
            if (pkg.requestedPermissions != null) {
                for (int i = 0; i < pkg.requestedPermissions.length; i++) {
                    String permission = pkg.requestedPermissions[i];
                    if (platformPrivPermissions.contains(permission)) {
                        requestedPrivPermissions.put(permission,
                                (pkg.requestedPermissionsFlags[i] & REQUESTED_PERMISSION_GRANTED)
                                        != 0);
                    }
                }
            }

            // If an app is requesting any privileged permissions, log the details and verify
            // that granted permissions are whitelisted
            if (!factoryRequestedPrivPermissions.isEmpty() && !requestedPrivPermissions.isEmpty()) {
                Set<String> granted = filterValues(requestedPrivPermissions,
                        isGranted -> isGranted).keySet();

                Set<String> factoryNotGranted = difference(factoryRequestedPrivPermissions,
                        granted);

                // priv permissions that the system package requested, but the current package not
                // anymore
                Set<String> removed = difference(factoryRequestedPrivPermissions,
                        requestedPrivPermissions.keySet());

                Set<String> whitelist = getPrivAppPermissions(packageName);
                Set<String> denylist = getPrivAppDenyPermissions(packageName);
                String msg = "Application " + packageName + "\n"
                        + "  Factory requested permissions:\n"
                        + getPrintableSet("    ", factoryRequestedPrivPermissions)
                        + "  Granted:\n"
                        + getPrintableSet("    ", granted)
                        + "  Removed:\n"
                        + getPrintableSet("    ", removed)
                        + "  Whitelisted:\n"
                        + getPrintableSet("    ", whitelist)
                        + "  Denylisted:\n"
                        + getPrintableSet("    ", denylist)
                        + "  Factory not granted:\n"
                        + getPrintableSet("    ", factoryNotGranted);

                for (String line : msg.split("\n")) {
                    Log.i(TAG, line);

                    // Prevent log from truncating output
                    Thread.sleep(10);
                }

                Set<String> grantedNotInWhitelist = difference(granted, whitelist);
                Set<String> factoryNotGrantedNotRemovedNotInDenylist = difference(difference(
                        factoryNotGranted, removed), denylist);

                if (!grantedNotInWhitelist.isEmpty()) {
                    packagesGrantedNotInWhitelist.put(packageName, grantedNotInWhitelist);
                }

                if (!factoryNotGrantedNotRemovedNotInDenylist.isEmpty()) {
                    packagesNotGrantedNotRemovedNotInDenylist.put(packageName,
                            factoryNotGrantedNotRemovedNotInDenylist);
                }
            }
        }
        StringBuilder message = new StringBuilder();
        if (!packagesGrantedNotInWhitelist.isEmpty()) {
            message.append("Not whitelisted permissions are granted: "
                    + packagesGrantedNotInWhitelist.toString());
        }
        if (!packagesNotGrantedNotRemovedNotInDenylist.isEmpty()) {
            if (message.length() != 0) {
                message.append(", ");
            }
            message.append("Requested permissions not granted: "
                    + packagesNotGrantedNotRemovedNotInDenylist.toString());
        }
        if (!packagesGrantedNotInWhitelist.isEmpty()
                || !packagesNotGrantedNotRemovedNotInDenylist.isEmpty()) {
            fail(message.toString());
        }
    }

    private <T> String getPrintableSet(String indendation, Set<T> set) {
        if (set.isEmpty()) {
            return "";
        }

        StringBuilder sb = new StringBuilder();

        for (T e : new TreeSet<>(set)) {
            if (!TextUtils.isEmpty(e.toString().trim())) {
                sb.append(indendation);
                sb.append(e);
                sb.append("\n");
            }
        }

        return sb.toString();
    }

    private Set<String> getPrivAppPermissions(String packageName) throws IOException {
        String output = SystemUtil.runShellCommand(
                InstrumentationRegistry.getInstrumentation(),
                "cmd package get-privapp-permissions " + packageName).trim();
        if (output.startsWith("{") && output.endsWith("}")) {
            String[] split = output.substring(1, output.length() - 1).split("\\s*,\\s*");
            return new LinkedHashSet<>(Arrays.asList(split));
        }
        return Collections.emptySet();
    }

    private Set<String> getPrivAppDenyPermissions(String packageName) throws IOException {
        String output = SystemUtil.runShellCommand(
                InstrumentationRegistry.getInstrumentation(),
                "cmd package get-privapp-deny-permissions " + packageName).trim();
        if (output.startsWith("{") && output.endsWith("}")) {
            String[] split = output.substring(1, output.length() - 1).split("\\s*,\\s*");
            return new LinkedHashSet<>(Arrays.asList(split));
        }
        return Collections.emptySet();
    }

}
