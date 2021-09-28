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

package com.android.cts.devicepolicy;

import static com.android.cts.devicepolicy.DeviceAndProfileOwnerTest.DEVICE_ADMIN_COMPONENT_FLATTENED;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;
import android.stats.devicepolicy.EventId;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Ignore;
import org.junit.Test;

/**
 * Tests for organization-owned Profile Owner.
 */
public class OrgOwnedProfileOwnerTest extends BaseDevicePolicyTest {
    private static final String DEVICE_ADMIN_PKG = DeviceAndProfileOwnerTest.DEVICE_ADMIN_PKG;
    private static final String DEVICE_ADMIN_APK = DeviceAndProfileOwnerTest.DEVICE_ADMIN_APK;
    private static final String CERT_INSTALLER_PKG = DeviceAndProfileOwnerTest.CERT_INSTALLER_PKG;
    private static final String CERT_INSTALLER_APK = DeviceAndProfileOwnerTest.CERT_INSTALLER_APK;

    private static final String ADMIN_RECEIVER_TEST_CLASS =
            DeviceAndProfileOwnerTest.ADMIN_RECEIVER_TEST_CLASS;
    private static final String ACTION_WIPE_DATA =
            "com.android.cts.deviceandprofileowner.WIPE_DATA";

    private static final String TEST_APP_APK = "CtsSimpleApp.apk";
    private static final String TEST_APP_PKG = "com.android.cts.launcherapps.simpleapp";
    private static final String DUMMY_IME_APK = "DummyIme.apk";
    private static final String DUMMY_IME_PKG = "com.android.cts.dummyime";
    private static final String DUMMY_IME_COMPONENT = DUMMY_IME_PKG + "/.DummyIme";
    private static final String SIMPLE_SMS_APP_PKG = "android.telephony.cts.sms.simplesmsapp";
    private static final String SIMPLE_SMS_APP_APK = "SimpleSmsApp.apk";
    private static final String DUMMY_LAUNCHER_APK = "DummyLauncher.apk";
    private static final String DUMMY_LAUNCHER_COMPONENT =
            "com.android.cts.dummylauncher/android.app.Activity";
    private static final String QUIET_MODE_TOGGLE_ACTIVITY =
            "com.android.cts.dummylauncher/.QuietModeToggleActivity";
    private static final String EXTRA_QUIET_MODE_STATE =
            "com.android.cts.dummyactivity.QUIET_MODE_STATE";
    public static final String SUSPENSION_CHECKER_CLASS =
            "com.android.cts.suspensionchecker.ActivityLaunchTest";

    protected int mUserId;
    private static final String DISALLOW_CONFIG_LOCATION = "no_config_location";
    private static final String CALLED_FROM_PARENT = "calledFromParent";

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // We need managed users to be supported in order to create a profile of the user owner.
        mHasFeature &= hasDeviceFeature("android.software.managed_users");

        if (mHasFeature) {
            removeTestUsers();
            createManagedProfile();
        }
    }

    private void createManagedProfile() throws Exception {
        mUserId = createManagedProfile(mPrimaryUserId);
        switchUser(mPrimaryUserId);
        startUserAndWait(mUserId);

        installAppAsUser(DEVICE_ADMIN_APK, mUserId);
        setProfileOwnerOrFail(DEVICE_ADMIN_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mUserId);
        startUserAndWait(mUserId);
        restrictManagedProfileRemoval();
    }

    @Override
    public void tearDown() throws Exception {
        // Managed profile and other test users will be removed by BaseDevicePolicyTest.tearDown()
        super.tearDown();
    }

    private void restrictManagedProfileRemoval() throws DeviceNotAvailableException {
            getDevice().executeShellCommand(
                    String.format("dpm mark-profile-owner-on-organization-owned-device --user %d '%s'",
                            mUserId, DEVICE_ADMIN_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS));
    }

    @Test
    public void testCannotRemoveManagedProfile() throws DeviceNotAvailableException {
        if (!mHasFeature) {
            return;
        }

        assertThat(getDevice().removeUser(mUserId)).isFalse();
    }

    @Test
    public void testCanRelinquishControlOverDevice() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".LockScreenInfoTest", "testSetAndGetLockInfo",
                mUserId);

        removeOrgOwnedProfile();
        assertHasNoUser(mUserId);

        try {
            installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
            assertTrue(setDeviceOwner(DEVICE_ADMIN_COMPONENT_FLATTENED,
                    mPrimaryUserId, /*expectFailure*/false));
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".LockScreenInfoTest", "testLockInfoIsNull",
                    mPrimaryUserId);
        } finally {
            removeAdmin(DEVICE_ADMIN_COMPONENT_FLATTENED, mPrimaryUserId);
            getDevice().uninstallPackage(DEVICE_ADMIN_PKG);
        }
    }

    @Test
    public void testLockScreenInfo() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".LockScreenInfoTest", mUserId);
    }

    @Test
    public void testProfileOwnerCanGetDeviceIdentifiers() throws Exception {
        // The Profile Owner should have access to all device identifiers.
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DeviceIdentifiersTest",
                "testProfileOwnerCanGetDeviceIdentifiersWithPermission", mUserId);
    }

    @Test
    public void testDevicePolicyManagerParentSupport() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".OrgOwnedProfileOwnerParentTest", mUserId);
    }

    @Test
    public void testUserRestrictionSetOnParentLogged() throws Exception {
        if (!mHasFeature|| !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DevicePolicyLoggingParentTest",
                    "testUserRestrictionLogged", mUserId);
                }, new DevicePolicyEventWrapper.Builder(EventId.ADD_USER_RESTRICTION_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setStrings(DISALLOW_CONFIG_LOCATION, CALLED_FROM_PARENT)
                        .build(),
                new DevicePolicyEventWrapper.Builder(EventId.REMOVE_USER_RESTRICTION_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setStrings(DISALLOW_CONFIG_LOCATION, CALLED_FROM_PARENT)
                        .build());
    }

    @Test
    public void testUserRestrictionsSetOnParentAreNotPersisted() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }
        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                "testAddUserRestrictionDisallowConfigDateTime_onParent", mUserId);
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                "testHasUserRestrictionDisallowConfigDateTime", mPrimaryUserId);
        removeOrgOwnedProfile();
        assertHasNoUser(mUserId);

        // User restrictions are not persist after organization-owned profile owner is removed
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                "testUserRestrictionDisallowConfigDateTimeIsNotPersisted", mPrimaryUserId);
    }

    @Test
    public void testPerProfileUserRestrictionOnParent() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                "testPerProfileUserRestriction_onParent", mUserId);
    }

    @Test
    public void testPerDeviceUserRestrictionOnParent() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                "testPerDeviceUserRestriction_onParent", mUserId);
    }

    @Test
    public void testCameraDisabledOnParentIsEnforced() throws Exception {
        if (!mHasFeature) {
            return;
        }

        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
        try {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                    "testAddUserRestrictionCameraDisabled_onParent", mUserId);
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                    "testCannotOpenCamera", mPrimaryUserId);
        } finally {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                    "testRemoveUserRestrictionCameraEnabled_onParent", mUserId);
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".UserRestrictionsParentTest",
                    "testCanOpenCamera", mPrimaryUserId);
        }
    }

    @Test
    public void testCameraDisabledOnParentLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
                    runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DevicePolicyLoggingParentTest",
                            "testCameraDisabledLogged", mUserId);
                }, new DevicePolicyEventWrapper.Builder(EventId.SET_CAMERA_DISABLED_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setBoolean(true)
                        .setStrings(CALLED_FROM_PARENT)
                        .build(),
                new DevicePolicyEventWrapper.Builder(EventId.SET_CAMERA_DISABLED_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setBoolean(false)
                        .setStrings(CALLED_FROM_PARENT)
                        .build());
    }

    @FlakyTest(bugId = 137093665)
    @Test
    public void testSecurityLogging() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // Backup stay awake setting because testGenerateLogs() will turn it off.
        final String stayAwake = getDevice().getSetting("global", "stay_on_while_plugged_in");
        try {
            // Turn logging on.
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".SecurityLoggingTest",
                    "testEnablingSecurityLogging", mUserId);
            // Reboot to ensure ro.device_owner is set to true in logd and logging is on.
            rebootAndWaitUntilReady();
            waitForUserUnlock(mUserId);

            // Generate various types of events on device side and check that they are logged.
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG,".SecurityLoggingTest",
                    "testGenerateLogs", mUserId);
            getDevice().executeShellCommand("whoami"); // Generate adb command securty event
            getDevice().executeShellCommand("dpm force-security-logs");
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".SecurityLoggingTest",
                    "testVerifyGeneratedLogs", mUserId);

            // Immediately attempting to fetch events again should fail.
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".SecurityLoggingTest",
                    "testSecurityLoggingRetrievalRateLimited", mUserId);
        } finally {
            // Turn logging off.
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".SecurityLoggingTest",
                    "testDisablingSecurityLogging", mUserId);
            // Restore stay awake setting.
            if (stayAwake != null) {
                getDevice().setSetting("global", "stay_on_while_plugged_in", stayAwake);
            }
        }
    }

    private void failToCreateUser() throws Exception {
        String command ="pm create-user " + "TestUser_" + System.currentTimeMillis();
        String commandOutput = getDevice().executeShellCommand(command);

        String[] tokens = commandOutput.split("\\s+");
        assertTrue(tokens.length > 0);
        assertEquals("Error:", tokens[0]);
    }

    @Test
    public void testSetTime() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".TimeManagementTest", "testSetTime", mUserId);
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".TimeManagementTest",
                "testSetTime_failWhenAutoTimeEnabled", mUserId);
    }

    @Test
    public void testSetTimeZone() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".TimeManagementTest", "testSetTimeZone", mUserId);
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".TimeManagementTest",
                "testSetTimeZone_failIfAutoTimeZoneEnabled", mUserId);
    }

    @FlakyTest(bugId = 137088260)
    @Test
    public void testWifi() throws Exception {
        if (!mHasFeature || !hasDeviceFeature("android.hardware.wifi")) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".WifiTest", "testGetWifiMacAddress", mUserId);
    }

    @Test
    public void testFactoryResetProtectionPolicy() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".FactoryResetProtectionPolicyTest", mUserId);
    }

    @LargeTest
    @Test
    @Ignore("b/145932189")
    public void testSystemUpdatePolicy() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".systemupdate.SystemUpdatePolicyTest", mUserId);
    }

    @Test
    public void testInstallUpdate() throws Exception {
        if (!mHasFeature) {
            return;
        }

        pushUpdateFileToDevice("notZip.zi");
        pushUpdateFileToDevice("empty.zip");
        pushUpdateFileToDevice("wrongPayload.zip");
        pushUpdateFileToDevice("wrongHash.zip");
        pushUpdateFileToDevice("wrongSize.zip");
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".systemupdate.InstallUpdateTest", mUserId);
    }

    @Test
    public void testIsDeviceOrganizationOwnedWithManagedProfile() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DeviceOwnershipTest",
                "testCallingIsOrganizationOwnedWithManagedProfileExpectingTrue",
                mUserId);

        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DeviceOwnershipTest",
                "testCallingIsOrganizationOwnedWithManagedProfileExpectingTrue",
                mPrimaryUserId);
    }

    @Test
    public void testCommonCriteriaMode() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".CommonCriteriaModeTest", mUserId);
    }

    @Test
    public void testAdminConfiguredNetworks() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".AdminConfiguredNetworksTest", mUserId);
    }

    @Test
    public void testApplicationHiddenParent() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".ApplicationHiddenParentTest", mUserId);
    }

    @Test
    public void testSetKeyguardDisabledFeatures() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".KeyguardDisabledFeaturesTest",
                "testSetKeyguardDisabledFeatures_onParent", mUserId);
    }

    private void removeOrgOwnedProfile() throws Exception {
        sendWipeProfileBroadcast(mUserId);
        waitUntilUserRemoved(mUserId);
    }

    private void sendWipeProfileBroadcast(int userId) throws Exception {
        final String cmd = "am broadcast --receiver-foreground --user " + userId
                + " -a " + ACTION_WIPE_DATA
                + " com.android.cts.deviceandprofileowner/.WipeDataReceiver";
        getDevice().executeShellCommand(cmd);
    }

    @Test
    public void testPersonalAppsSuspensionNormalApp() throws Exception {
        if (!mHasFeature) {
            return;
        }

        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
        // Initially the app should be launchable.
        assertCanStartPersonalApp(DEVICE_ADMIN_PKG, true);
        setPersonalAppsSuspended(true);
        // Now the app should be suspended and not launchable
        assertCanStartPersonalApp(DEVICE_ADMIN_PKG, false);
        setPersonalAppsSuspended(false);
        // Should be launchable again.
        assertCanStartPersonalApp(DEVICE_ADMIN_PKG, true);
    }

    @Test
    public void testPersonalAppsSuspensionInstalledApp() throws Exception {
        if (!mHasFeature) {
            return;
        }

        setPersonalAppsSuspended(true);

        installAppAsUser(DUMMY_IME_APK, mPrimaryUserId);

        // Wait until package install broadcast is processed
        waitForBroadcastIdle();

        assertCanStartPersonalApp(DUMMY_IME_PKG, false);
        setPersonalAppsSuspended(false);
    }

    private void setPersonalAppsSuspended(boolean suspended) throws DeviceNotAvailableException {
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".PersonalAppsSuspensionTest",
                suspended ? "testSuspendPersonalApps" : "testUnsuspendPersonalApps", mUserId);
    }

    @Test
    public void testPersonalAppsSuspensionSms() throws Exception {
        if (!mHasFeature || !mHasTelephony) {
            return;
        }

        // Install an SMS app and make it the default.
        installAppAsUser(SIMPLE_SMS_APP_APK, mPrimaryUserId);
        addSmsRole(SIMPLE_SMS_APP_PKG, mPrimaryUserId);
        try {
            setPersonalAppsSuspended(true);
            // Default sms app should not be suspended.
            assertCanStartPersonalApp(SIMPLE_SMS_APP_PKG, true);
            setPersonalAppsSuspended(false);
        } finally {
            removeSmsRole(SIMPLE_SMS_APP_PKG, mPrimaryUserId);
        }
    }

    private void addSmsRole(String app, int userId) throws Exception {
        executeShellCommand(String.format(
                "cmd role add-role-holder --user %d android.app.role.SMS %s", userId, app));
    }

    private void removeSmsRole(String app, int userId) throws Exception {
        executeShellCommand(String.format(
                "cmd role remove-role-holder --user %d android.app.role.SMS %s", userId, app));
    }

    @Test
    public void testPersonalAppsSuspensionIme() throws Exception {
        if (!mHasFeature) {
            return;
        }

        installAppAsUser(DUMMY_IME_APK, mPrimaryUserId);
        setupIme(DUMMY_IME_COMPONENT, mPrimaryUserId);
        setPersonalAppsSuspended(true);
        // Active IME should not be suspended.
        assertCanStartPersonalApp(DUMMY_IME_PKG, true);
        setPersonalAppsSuspended(false);
    }

    @Test
    public void testCanRestrictAccountManagementOnParentProfile() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".AccountManagementParentTest",
                "testSetAccountManagementDisabledOnParent", mUserId);
        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
        try {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".AccountManagementParentTest",
                    "testAccountManagementDisabled", mPrimaryUserId);
        } finally {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".AccountManagementParentTest",
                    "testEnableAccountManagement", mUserId);
        }
    }

    private void setupIme(String imeComponent, int userId) throws Exception {
        // Wait until IMS service is registered by the system.
        waitForOutput("Failed waiting for IME to become available",
                String.format("ime list --user %d -s -a", userId),
                s -> s.contains(imeComponent), 10 /* seconds */);

        executeShellCommand("ime enable " + imeComponent);
        executeShellCommand("ime set " + imeComponent);
    }

    private void assertCanStartPersonalApp(String packageName, boolean canStart)
            throws DeviceNotAvailableException {
        runDeviceTestsAsUser(packageName, "com.android.cts.suspensionchecker.ActivityLaunchTest",
                canStart ? "testCanStartActivity" : "testCannotStartActivity", mPrimaryUserId);
    }

    @Test
    public void testScreenCaptureDisabled() throws Exception {
        if (!mHasFeature) {
            return;
        }
        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
        setPoAsUser(mPrimaryUserId);

        try {
            setScreenCaptureDisabled(true);
        } finally {
            setScreenCaptureDisabled(false);
        }
    }

    private void takeScreenCaptureAsUser(int userId, String testMethodName) throws Exception {
        installAppAsUser(TEST_APP_APK, /* grantPermissions */ true, /* dontKillApp */ true, userId);
        startActivityAsUser(userId, TEST_APP_PKG, TEST_APP_PKG + ".SimpleActivity");
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".ScreenCaptureDisabledTest",
                testMethodName, userId);
        forceStopPackageForUser(TEST_APP_PKG, userId);
    }

    private void setScreenCaptureDisabled(boolean disabled) throws Exception {
        String testMethodName = disabled
                ? "testSetScreenCaptureDisabledOnParent_true"
                : "testSetScreenCaptureDisabledOnParent_false";
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".ScreenCaptureDisabledTest",
                testMethodName, mUserId);

        testMethodName = disabled
                ? "testScreenCaptureImpossible"
                : "testScreenCapturePossible";

        // Test personal profile
        takeScreenCaptureAsUser(mPrimaryUserId, testMethodName);

        // Test managed profile. This should not be disabled when screen capture is disabled on
        // the parent by the profile owner of an organization-owned device.
        takeScreenCaptureAsUser(mUserId, "testScreenCapturePossible");
    }

    private void assertHasNoUser(int userId) throws DeviceNotAvailableException {
        int numWaits = 0;
        final int MAX_NUM_WAITS = 15;
        while (listUsers().contains(userId) && (numWaits < MAX_NUM_WAITS)) {
            try {
                Thread.sleep(1000);
                numWaits += 1;
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        assertThat(listUsers()).doesNotContain(userId);
    }

    private void setPoAsUser(int userId) throws Exception {
        installAppAsUser(DEVICE_ADMIN_APK, true, true, userId);
        assertTrue("Failed to set profile owner",
                setProfileOwner(DEVICE_ADMIN_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS,
                        userId, /* expectFailure */ false));
    }

    @Test
    public void testSetPersonalAppsSuspendedLogged() throws Exception {
        if (!mHasFeature|| !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
                    runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DevicePolicyLoggingTest",
                            "testSetPersonalAppsSuspendedLogged", mUserId);
                }, new DevicePolicyEventWrapper.Builder(EventId.SET_PERSONAL_APPS_SUSPENDED_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setBoolean(true)
                        .build(),
                new DevicePolicyEventWrapper.Builder(EventId.SET_PERSONAL_APPS_SUSPENDED_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setBoolean(false)
                        .build());
    }

    @Test
    public void testSetManagedProfileMaximumTimeOffLogged() throws Exception {
        if (!mHasFeature|| !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
                    runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".PersonalAppsSuspensionTest",
                            "testSetManagedProfileMaximumTimeOff", mUserId);
                }, new DevicePolicyEventWrapper.Builder(
                        EventId.SET_MANAGED_PROFILE_MAXIMUM_TIME_OFF_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setTimePeriod(123456789)
                        .build(),
                new DevicePolicyEventWrapper.Builder(
                        EventId.SET_MANAGED_PROFILE_MAXIMUM_TIME_OFF_VALUE)
                        .setAdminPackageName(DEVICE_ADMIN_PKG)
                        .setTimePeriod(0)
                        .build());
    }

    @Test
    public void testWorkProfileMaximumTimeOff() throws Exception {
        if (!mHasFeature) {
            return;
        }
        installAppAsUser(DEVICE_ADMIN_APK, mPrimaryUserId);
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".PersonalAppsSuspensionTest",
                "testSetManagedProfileMaximumTimeOff1Sec", mUserId);

        final String defaultLauncher = getDefaultLauncher();
        try {
            installAppAsUser(DUMMY_LAUNCHER_APK, true, true, mPrimaryUserId);
            setAndStartLauncher(DUMMY_LAUNCHER_COMPONENT);
            toggleQuietMode(true);
            // Verify that at some point personal app becomes impossible to launch.
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, SUSPENSION_CHECKER_CLASS,
                    "testWaitForActivityNotLaunchable", mPrimaryUserId);
            toggleQuietMode(false);
            // Ensure the profile is properly started before wipe broadcast is sent in teardown.
            waitForUserUnlock(mUserId);
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".PersonalAppsSuspensionTest",
                    "testPersonalAppsSuspendedByTimeout", mUserId);
        } finally {
            setAndStartLauncher(defaultLauncher);
        }
    }

    @Test
    public void testDelegatedCertInstallerDeviceIdAttestation() throws Exception {
        if (!mHasFeature) {
            return;
        }
        installAppAsUser(CERT_INSTALLER_APK, mUserId);

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DelegatedCertInstallerHelper",
                "testManualSetCertInstallerDelegate", mUserId);

        runDeviceTestsAsUser(CERT_INSTALLER_PKG, ".DelegatedDeviceIdAttestationTest",
                "testGenerateKeyPairWithDeviceIdAttestationExpectingSuccess", mUserId);
    }

    @Test
    public void testDeviceIdAttestationForProfileOwner() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // Test that Device ID attestation works for org-owned profile owner.
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DeviceIdAttestationTest",
                "testSucceedsWithProfileOwnerIdsGrant", mUserId);

    }

    private void toggleQuietMode(boolean quietModeEnable) throws Exception {
        final String str;
        // TV launcher uses intent filter priority to prevent 3p launchers replacing it
        // this causes the activity that toggles quiet mode to be suspended
        // and the profile would never start
        if (hasDeviceFeature("android.software.leanback")) {
            str = quietModeEnable ? String.format("am stop-user -f %d", mUserId)
                    : String.format("am start-user %d", mUserId);
        } else {
            str = String.format("am start-activity -n %s --ez %s %s",
                    QUIET_MODE_TOGGLE_ACTIVITY, EXTRA_QUIET_MODE_STATE, quietModeEnable);
        }
        executeShellCommand(str);
    }

    private void setAndStartLauncher(String component) throws Exception {
        String output = getDevice().executeShellCommand(String.format(
                "cmd package set-home-activity --user %d %s", mPrimaryUserId, component));
        assertTrue("failed to set home activity", output.contains("Success"));
        output = getDevice().executeShellCommand(
                String.format("cmd shortcut clear-default-launcher --user %d", mPrimaryUserId));
        assertTrue("failed to clear default launcher", output.contains("Success"));
        executeShellCommand("am start -W -n " + component);
    }
}
