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

package com.android.cts.managedprofile;

import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_DEFAULT;

import static com.google.common.truth.Truth.assertThat;

import android.Manifest;
import android.app.AppOpsManager;
import android.app.UiAutomation;
import android.app.admin.DeviceAdminReceiver;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.os.Process;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.test.InstrumentationRegistry;

import com.google.common.collect.Sets;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/** App-side tests for interacting across profiles. */
public class CrossProfileTest extends BaseManagedProfileTest {
    private static final String MANAGE_APP_OPS_MODES_PERMISSION =
            "android.permission.MANAGE_APP_OPS_MODES";
    private static final String UPDATE_APP_OPS_STATS_PERMISSION =
            "android.permission.UPDATE_APP_OPS_STATS";
    private static final String INTERACT_ACROSS_USERS_PERMISSION =
            "android.permission.INTERACT_ACROSS_USERS";

    private static final ComponentName NON_ADMIN_RECEIVER =
            new ComponentName(
                    NonAdminReceiver.class.getPackage().getName(),
                    NonAdminReceiver.class.getName());

    private static final Set<String> TEST_CROSS_PROFILE_PACKAGES =
            Collections.singleton("test.package.name");

    private static final Set<String> ALL_CROSS_PROFILE_PACKAGES = Sets.newHashSet(
            "com.android.cts.dummyapps.dummyapp1",
            "com.android.cts.dummyapps.dummyapp2",
            "com.android.cts.dummyapps.dummyapp3",
            "com.android.cts.dummyapps.dummyapp4");
    private static final Set<String> SUBLIST_CROSS_PROFILE_PACKAGES =
            Sets.newHashSet(
                    "com.android.cts.dummyapps.dummyapp1",
                    "com.android.cts.dummyapps.dummyapp2");
    private static final Set<String> DIFF_CROSS_PROFILE_PACKAGES =
            Sets.newHashSet(
                    "com.android.cts.dummyapps.dummyapp3",
                    "com.android.cts.dummyapps.dummyapp4");

    private static final UiAutomation sUiAutomation =
            InstrumentationRegistry.getInstrumentation().getUiAutomation();

    private AppOpsManager mAppOpsManager;
    private PackageManager mPackageManager;
    private UserManager mUserManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mAppOpsManager = mContext.getSystemService(AppOpsManager.class);
        mPackageManager = mContext.getPackageManager();
        mUserManager = mContext.getSystemService(UserManager.class);
    }

    @Override
    protected void tearDown() throws Exception {
        explicitlyResetInteractAcrossProfilesAppOps();
        resetCrossProfilePackages();
        super.tearDown();
    }

    public void testSetCrossProfilePackages_notProfileOwner_throwsSecurityException() {
        try {
            mDevicePolicyManager.setCrossProfilePackages(
                    NON_ADMIN_RECEIVER, TEST_CROSS_PROFILE_PACKAGES);
            fail("SecurityException excepted.");
        } catch (SecurityException ignored) {}
    }

    public void testGetCrossProfilePackages_notProfileOwner_throwsSecurityException() {
        try {
            mDevicePolicyManager.getCrossProfilePackages(NON_ADMIN_RECEIVER);
            fail("SecurityException expected.");
        } catch (SecurityException ignored) {}
    }

    public void testGetCrossProfilePackages_notSet_returnsEmpty() {
        assertThat(mDevicePolicyManager.getCrossProfilePackages(ADMIN_RECEIVER_COMPONENT))
                .isEmpty();
    }

    public void testGetCrossProfilePackages_whenSet_returnsEqual() {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, TEST_CROSS_PROFILE_PACKAGES);
        assertThat(mDevicePolicyManager.getCrossProfilePackages(ADMIN_RECEIVER_COMPONENT))
                .isEqualTo(TEST_CROSS_PROFILE_PACKAGES);
    }

    public void testGetCrossProfilePackages_whenSetTwice_returnsLatestNotConcatenated() {
        final Set<String> packages1 = Collections.singleton("test.package.name.1");
        final Set<String> packages2 = Collections.singleton("test.package.name.2");

        mDevicePolicyManager.setCrossProfilePackages(ADMIN_RECEIVER_COMPONENT, packages1);
        mDevicePolicyManager.setCrossProfilePackages(ADMIN_RECEIVER_COMPONENT, packages2);

        assertThat(mDevicePolicyManager.getCrossProfilePackages(ADMIN_RECEIVER_COMPONENT))
                .isEqualTo(packages2);
    }

    /**
     * Sets each of the packages in {@link #ALL_CROSS_PROFILE_PACKAGES} as cross-profile. This can
     * then be used for writing host-side tests. Note that the state is cleared after running any
     * test in this class, so this method should not be used to attempt to perform a sequence of
     * device-side calls.
     */
    public void testSetCrossProfilePackages_noAsserts() throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
    }

    public void testSetCrossProfilePackages_firstTime_doesNotResetAnyAppOps() throws Exception {
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
        assertThatPackagesHaveAppOpMode(ALL_CROSS_PROFILE_PACKAGES, MODE_ALLOWED);
    }

    public void testSetCrossProfilePackages_unchanged_doesNotResetAnyAppOps() throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);

        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);

        assertThatPackagesHaveAppOpMode(ALL_CROSS_PROFILE_PACKAGES, MODE_ALLOWED);
    }

    public void testSetCrossProfilePackages_noPackagesUnset_doesNotResetAnyAppOps()
            throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, SUBLIST_CROSS_PROFILE_PACKAGES);
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);

        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);

        assertThatPackagesHaveAppOpMode(ALL_CROSS_PROFILE_PACKAGES, MODE_ALLOWED);
    }

    public void testSetCrossProfilePackages_somePackagesUnset_doesNotResetAppOpsIfStillSet()
            throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);

        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, SUBLIST_CROSS_PROFILE_PACKAGES);

        assertThatPackagesHaveAppOpMode(SUBLIST_CROSS_PROFILE_PACKAGES, MODE_ALLOWED);
    }

    public void testSetCrossProfilePackages_resetsAppOpOfUnsetPackages() throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);

        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, SUBLIST_CROSS_PROFILE_PACKAGES);

        assertThatPackagesHaveAppOpMode(DIFF_CROSS_PROFILE_PACKAGES, MODE_DEFAULT);
    }

    public void testSetCrossProfilePackages_resetsAppOpOfUnsetPackagesOnOtherProfile()
            throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);

        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, SUBLIST_CROSS_PROFILE_PACKAGES);

        assertThatPackagesHaveAppOpMode(
                DIFF_CROSS_PROFILE_PACKAGES, MODE_DEFAULT, UserHandle.of(0));
    }

    /**
     * Sets each of the packages in {@link #ALL_CROSS_PROFILE_PACKAGES} as cross-profile, then sets
     * them again to {@link #SUBLIST_CROSS_PROFILE_PACKAGES}, with all app-ops explicitly set as
     * allowed before-hand. This should result in resetting packages {@link
     * #DIFF_CROSS_PROFILE_PACKAGES}. This can then be used for writing host-side tests.
     */
    public void testSetCrossProfilePackages_resetsAppOps_noAsserts() throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);

        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, SUBLIST_CROSS_PROFILE_PACKAGES);
    }

    /**
     * Assumes that the host-side test performs the assertion of the broadcast itself. The broadcast
     * should be sent to packages {@link #DIFF_CROSS_PROFILE_PACKAGES} and not to packages {@link
     * #SUBLIST_CROSS_PROFILE_PACKAGES}.
     */
    public void testSetCrossProfilePackages_sendsBroadcastWhenResettingAppOps_noAsserts()
            throws Exception {
        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, ALL_CROSS_PROFILE_PACKAGES);
        explicitlySetInteractAcrossProfilesAppOps(MODE_ALLOWED);

        mDevicePolicyManager.setCrossProfilePackages(
                ADMIN_RECEIVER_COMPONENT, SUBLIST_CROSS_PROFILE_PACKAGES);
    }

    private void explicitlySetInteractAcrossProfilesAppOps(int mode) throws Exception {
        for (String packageName : ALL_CROSS_PROFILE_PACKAGES) {
            explicitlySetInteractAcrossProfilesAppOp(packageName, mode);
        }
    }

    private void explicitlySetInteractAcrossProfilesAppOp(String packageName, int mode)
            throws Exception {
        for (UserHandle profile : mUserManager.getUserProfiles()) {
            if (isPackageInstalledForUser(packageName, profile)) {
                explicitlySetInteractAcrossProfilesAppOp(packageName, mode, profile);
            }
        }
    }

    private boolean isPackageInstalledForUser(String packageName, UserHandle user) {
        try {
            sUiAutomation.adoptShellPermissionIdentity(INTERACT_ACROSS_USERS_PERMISSION);
            mContext.createContextAsUser(user, /* flags= */ 0).getPackageManager()
                    .getPackageInfo(packageName, /* flags= */ 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        } finally {
            sUiAutomation.dropShellPermissionIdentity();
        }
    }

    private void explicitlySetInteractAcrossProfilesAppOp(
            String packageName, int mode, UserHandle userHandle) throws Exception {
        sUiAutomation.adoptShellPermissionIdentity(
                MANAGE_APP_OPS_MODES_PERMISSION,
                UPDATE_APP_OPS_STATS_PERMISSION,
                INTERACT_ACROSS_USERS_PERMISSION);
        mAppOpsManager.setMode(
                AppOpsManager.permissionToOp(Manifest.permission.INTERACT_ACROSS_PROFILES),
                getUidForPackageName(packageName, userHandle),
                packageName,
                mode);
        sUiAutomation.dropShellPermissionIdentity();
    }

    private int getUidForPackageName(String packageName, UserHandle userHandle) throws Exception {
        return mContext.createContextAsUser(userHandle, /* flags= */ 0)
                .getPackageManager()
                .getPackageUid(packageName, /* flags= */ 0);
    }

    private void assertThatPackagesHaveAppOpMode(
            Set<String> packageNames, int mode, UserHandle userHandle)
            throws Exception {
        for (String unsetCrossProfilePackage : packageNames) {
            assertThat(getCrossProfileAppOp(unsetCrossProfilePackage, userHandle)).isEqualTo(mode);
        }
    }

    private void assertThatPackagesHaveAppOpMode(Set<String> packageNames, int mode)
            throws Exception {
        assertThatPackagesHaveAppOpMode(packageNames, mode, Process.myUserHandle());
    }

    private int getCrossProfileAppOp(String packageName, UserHandle userHandle) throws Exception {
        sUiAutomation.adoptShellPermissionIdentity(
                MANAGE_APP_OPS_MODES_PERMISSION,
                UPDATE_APP_OPS_STATS_PERMISSION,
                INTERACT_ACROSS_USERS_PERMISSION);
        final int result = mAppOpsManager.unsafeCheckOpNoThrow(
                AppOpsManager.permissionToOp(Manifest.permission.INTERACT_ACROSS_PROFILES),
                getUidForPackageName(packageName, userHandle),
                packageName);
        sUiAutomation.dropShellPermissionIdentity();
        return result;
    }

    private void explicitlyResetInteractAcrossProfilesAppOps() throws Exception {
        explicitlySetInteractAcrossProfilesAppOps(AppOpsManager.MODE_DEFAULT);
    }

    private void resetCrossProfilePackages() {
        mDevicePolicyManager.setCrossProfilePackages(ADMIN_RECEIVER_COMPONENT, new HashSet<>());
    }

    private static class NonAdminReceiver extends DeviceAdminReceiver {}
}
