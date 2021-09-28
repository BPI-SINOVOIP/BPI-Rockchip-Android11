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

package com.android.cts.devicepolicy;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;

import com.android.cts.devicepolicy.annotations.LockSettingsTest;
import com.android.cts.devicepolicy.annotations.PermissionsTest;
import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Test;

/**
 * Set of tests for managed profile owner use cases that also apply to device owners.
 * Tests that should be run identically in both cases are added in DeviceAndProfileOwnerTest.
 */
public class MixedManagedProfileOwnerTest extends DeviceAndProfileOwnerTest {

    private static final String CLEAR_PROFILE_OWNER_NEGATIVE_TEST_CLASS =
            DEVICE_ADMIN_PKG + ".ClearProfileOwnerNegativeTest";
    private static final String FEATURE_WIFI = "android.hardware.wifi";

    private int mParentUserId = -1;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        // We need managed users to be supported in order to create a profile of the user owner.
        mHasFeature &= hasDeviceFeature("android.software.managed_users");

        if (mHasFeature) {
            removeTestUsers();
            mParentUserId = mPrimaryUserId;
            createManagedProfile();
        }
    }

    private void createManagedProfile() throws Exception {
        mUserId = createManagedProfile(mParentUserId);
        switchUser(mParentUserId);
        startUserAndWait(mUserId);

        installAppAsUser(DEVICE_ADMIN_APK, mUserId);
        setProfileOwnerOrFail(DEVICE_ADMIN_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mUserId);
        startUserAndWait(mUserId);
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            removeUser(mUserId);
        }
        super.tearDown();
    }

    // Most tests for this class are defined in DeviceAndProfileOwnerTest

    /**
     * Verify that screenshots are still possible for activities in the primary user when the policy
     * is set on the profile owner.
     */
    @LargeTest
    @Test
    public void testScreenCaptureDisabled_allowedPrimaryUser() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // disable screen capture in profile
        setScreenCaptureDisabled(mUserId, true);

        // start the ScreenCaptureDisabledActivity in the parent
        installAppAsUser(DEVICE_ADMIN_APK, mParentUserId);
        startSimpleActivityAsUser(mParentUserId);
        executeDeviceTestMethod(".ScreenCaptureDisabledTest", "testScreenCapturePossible");
    }

    @FlakyTest
    @Test
    public void testScreenCaptureDisabled_assist_allowedPrimaryUser() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // disable screen capture in profile
        executeDeviceTestMethod(".ScreenCaptureDisabledTest", "testSetScreenCaptureDisabled_true");
        try {
            // Install and enable assistant in personal side.
            installAppAsUser(ASSIST_APP_APK, mParentUserId);
            waitForBroadcastIdle();
            setVoiceInteractionService(ASSIST_INTERACTION_SERVICE);

            // Start an activity in parent user.
            installAppAsUser(DEVICE_ADMIN_APK, mParentUserId);
            waitForBroadcastIdle();
            startSimpleActivityAsUser(mParentUserId);

            // Verify assistant app can't take a screenshot.
            runDeviceTestsAsUser(
                    DEVICE_ADMIN_PKG,
                    ".AssistScreenCaptureDisabledTest",
                    "testScreenCapturePossible_assist",
                    mPrimaryUserId);
        } finally {
            // enable screen capture in profile
            executeDeviceTestMethod(
                    ".ScreenCaptureDisabledTest",
                    "testSetScreenCaptureDisabled_false");
            clearVoiceInteractionService();
        }
    }

    @Override
    @Test
    public void testDisallowSetWallpaper_allowed() throws Exception {
        // Managed profile doesn't have wallpaper.
    }

    @Override
    @Test
    public void testAudioRestriction() throws Exception {
        // DISALLOW_UNMUTE_MICROPHONE and DISALLOW_ADJUST_VOLUME can only be set by device owners
        // and profile owners on the primary user.
    }

    /** VPN tests don't require physical device for managed profile, thus overriding. */
    @FlakyTest
    @Override
    @Test
    public void testAlwaysOnVpn() throws Exception {
        super.testAlwaysOnVpn();
    }

    /** VPN tests don't require physical device for managed profile, thus overriding. */
    @Override
    @Test
    public void testAlwaysOnVpnLockDown() throws Exception {
        super.testAlwaysOnVpnLockDown();
    }

    /** VPN tests don't require physical device for managed profile, thus overriding. */
    @Override
    @LargeTest
    @Test
    public void testAlwaysOnVpnAcrossReboot() throws Exception {
        super.testAlwaysOnVpnAcrossReboot();
    }

    /** VPN tests don't require physical device for managed profile, thus overriding. */
    @Override
    @Test
    public void testAlwaysOnVpnPackageUninstalled() throws Exception {
        super.testAlwaysOnVpnPackageUninstalled();
    }

    /** VPN tests don't require physical device for managed profile, thus overriding. */
    @Override
    @Test
    public void testAlwaysOnVpnUnsupportedPackage() throws Exception {
        super.testAlwaysOnVpnUnsupportedPackage();
    }

    /** VPN tests don't require physical device for managed profile, thus overriding. */
    @Override
    @Test
    public void testAlwaysOnVpnUnsupportedPackageReplaced() throws Exception {
        super.testAlwaysOnVpnUnsupportedPackageReplaced();
    }

    @Override
    @LockSettingsTest
    @Test
    public void testResetPasswordWithToken() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        // Execute the test method that's guaranteed to succeed. See also test in base class
        // which are tolerant to failure and executed by MixedDeviceOwnerTest and
        // MixedProfileOwnerTest
        executeResetPasswordWithTokenTests(false);
    }

    @Override
    @Test
    public void testSetSystemSetting() {
        // Managed profile owner cannot set currently whitelisted system settings.
    }

    @Override
    @Test
    public void testSetAutoTimeRequired() {
        // Managed profile owner cannot set auto time required
    }

    @Override
    @Test
    public void testSetAutoTimeEnabled() {
        // Managed profile owner cannot set auto time unless it is called by the profile owner of
        // an organization-owned managed profile.
    }

    @Override
    @Test
    public void testSetAutoTimeZoneEnabled() {
        // Managed profile owner cannot set auto time zone unless it is called by the profile
        // owner of an organization-owned managed profile.
    }

    @Test
    public void testCannotClearProfileOwner() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, CLEAR_PROFILE_OWNER_NEGATIVE_TEST_CLASS, mUserId);
    }

    private void markProfileOwnerOnOrganizationOwnedDevice() throws DeviceNotAvailableException {
        getDevice().executeShellCommand(
                String.format("dpm mark-profile-owner-on-organization-owned-device --user %d '%s'",
                    mUserId, DEVICE_ADMIN_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS));

    }

    @Test
    public void testDelegatedCertInstallerDeviceIdAttestation() throws Exception {
        if (!mHasFeature) {
            return;
        }
        setUpDelegatedCertInstallerAndRunTests(() -> {
            runDeviceTestsAsUser("com.android.cts.certinstaller",
                    ".DelegatedDeviceIdAttestationTest",
                    "testGenerateKeyPairWithDeviceIdAttestationExpectingFailure", mUserId);
            // Positive test case lives in
            // OrgOwnedProfileOwnerTest#testDelegatedCertInstallerDeviceIdAttestation
        });
    }
    @Test
    public void testDeviceIdAttestationForProfileOwner() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // Test that Device ID attestation for the profile owner does not work without grant.
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DeviceIdAttestationTest",
                "testFailsWithoutProfileOwnerIdsGrant", mUserId);
        // Positive test case lives in
        // OrgOwnedProfileOwnerTest#testDeviceIdAttestationForProfileOwner
    }

    @Test
    @Override
    public void testSetKeyguardDisabledFeatures() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".KeyguardDisabledFeaturesTest",
                "testSetKeyguardDisabledFeatures_onParentSilentIgnoreWhenCallerIsNotOrgOwnedPO",
                mUserId);
    }

    @Test
    @Override
    public void testSetKeyguardDisabledSecureCameraLogged() {
        // Managed profiles are not allowed to set keyguard disabled secure camera
    }

    @FlakyTest
    @Override
    @Test
    public void testCaCertManagement() throws Exception {
        super.testCaCertManagement();
    }

    @FlakyTest
    @Override
    @Test
    public void testDelegatedCertInstaller() throws Exception {
        super.testDelegatedCertInstaller();
    }

    @FlakyTest
    @Override
    @Test
    public void testPackageInstallUserRestrictions() throws Exception {
        super.testPackageInstallUserRestrictions();
    }

    @Override
    @FlakyTest
    @PermissionsTest
    @Test
    public void testPermissionGrant() throws Exception {
        super.testPermissionGrant();
    }

    @Override
    @FlakyTest
    @PermissionsTest
    @Test
    public void testPermissionMixedPolicies() throws Exception {
        super.testPermissionMixedPolicies();
    }

    @FlakyTest
    @Override
    @Test
    public void testScreenCaptureDisabled_assist() throws Exception {
        super.testScreenCaptureDisabled_assist();
    }

    @Override
    @PermissionsTest
    @FlakyTest(bugId = 145350538)
    @Test
    public void testPermissionPolicy() throws Exception {
        super.testPermissionPolicy();
    }

    @FlakyTest
    @Override
    @Test
    public void testSetMeteredDataDisabledPackages() throws Exception {
        super.testSetMeteredDataDisabledPackages();
    }

    @Override
    @PermissionsTest
    @FlakyTest(bugId = 145350538)
    @Test
    public void testPermissionAppUpdate() throws Exception {
        super.testPermissionAppUpdate();
    }

    @Override
    @PermissionsTest
    @FlakyTest(bugId = 145350538)
    @Test
    public void testPermissionGrantPreMApp() throws Exception {
        super.testPermissionGrantPreMApp();
    }

    @Override
    @PermissionsTest
    @FlakyTest(bugId = 145350538)
    @Test
    public void testPermissionGrantOfDisallowedPermissionWhileOtherPermIsGranted()
            throws Exception {
        super.testPermissionGrantOfDisallowedPermissionWhileOtherPermIsGranted();
    }

    @Override
    @Test
    public void testLockTask() {
        // Managed profiles are not allowed to use lock task
    }

    @Override
    @Test
    public void testLockTaskAfterReboot() {
        // Managed profiles are not allowed to use lock task
    }

    @Override
    @Test
    public void testLockTaskAfterReboot_tryOpeningSettings() {
        // Managed profiles are not allowed to use lock task
    }

    @Override
    @Test
    public void testLockTask_defaultDialer() {
        // Managed profiles are not allowed to use lock task
    }

    @Override
    @Test
    public void testLockTask_emergencyDialer() {
        // Managed profiles are not allowed to use lock task
    }

    @Override
    @Test
    public void testLockTask_exitIfNoLongerWhitelisted() {
        // Managed profiles are not allowed to use lock task
    }

    @Test
    public void testWifiMacAddress() throws Exception {
        if (!mHasFeature || !hasDeviceFeature(FEATURE_WIFI)) {
            return;
        }

        runDeviceTestsAsUser(
                DEVICE_ADMIN_PKG, ".WifiTest", "testCannotGetWifiMacAddress", mUserId);
    }

    //TODO(b/130844684): Remove when removing profile owner on personal device access to device
    // identifiers.
    @Test
    public void testProfileOwnerCanGetDeviceIdentifiers() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DeviceIdentifiersTest",
                "testProfileOwnerCanGetDeviceIdentifiersWithPermission", mUserId);
    }
}
