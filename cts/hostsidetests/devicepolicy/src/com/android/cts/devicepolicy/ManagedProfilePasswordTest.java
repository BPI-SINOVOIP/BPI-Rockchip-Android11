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

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;
import android.stats.devicepolicy.EventId;

import com.android.cts.devicepolicy.annotations.LockSettingsTest;
import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Test;

import java.util.concurrent.TimeUnit;

public class ManagedProfilePasswordTest extends BaseManagedProfileTest {
    private static final String USER_STATE_LOCKED = "RUNNING_LOCKED";
    private static final long TIMEOUT_USER_LOCKED_MILLIS = TimeUnit.MINUTES.toMillis(3);
    // Password needs to be in sync with ResetPasswordWithTokenTest.PASSWORD1
    private static final String RESET_PASSWORD_TEST_DEFAULT_PASSWORD = "123456";

    @FlakyTest
    @Test
    public void testLockNowWithKeyEviction() throws Exception {
        if (!mHasFeature || !mSupportsFbe || !mHasSecureLockScreen) {
            return;
        }
        changeUserCredential(TEST_PASSWORD, null, mProfileUserId);
        lockProfile();
    }

    @Test
    public void testPasswordMinimumRestrictions() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".PasswordMinimumRestrictionsTest",
                mProfileUserId);
    }

    @FlakyTest
    @Test
    public void testResetPasswordWithTokenBeforeUnlock() throws Exception {
        if (!mHasFeature || !mSupportsFbe || !mHasSecureLockScreen) {
            return;
        }

        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ResetPasswordWithTokenTest",
                "testSetupWorkProfile", mProfileUserId);
        lockProfile();
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ResetPasswordWithTokenTest",
                "testResetPasswordBeforeUnlock", mProfileUserId);
        // Password needs to be in sync with ResetPasswordWithTokenTest.PASSWORD1
        verifyUserCredential(RESET_PASSWORD_TEST_DEFAULT_PASSWORD, mProfileUserId);
    }

    @FlakyTest
    @Test
    public void testClearPasswordWithTokenBeforeUnlock() throws Exception {
        if (!mHasFeature || !mSupportsFbe || !mHasSecureLockScreen) {
            return;
        }

        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ResetPasswordWithTokenTest",
                "testSetupWorkProfile", mProfileUserId);
        lockProfile();
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ResetPasswordWithTokenTest",
                "testClearPasswordBeforeUnlock", mProfileUserId);
        // Make sure profile has no password
        verifyUserCredential("", mProfileUserId);
    }

    /**
     * Test password reset token is still functional after the primary user clears and
     * re-adds back its device lock. This is to detect a regression where the work profile
     * undergoes an untrusted credential reset (causing synthetic password to change, invalidating
     * existing password reset token) if it has unified work challenge and the primary user clears
     * the device lock.
     */
    @FlakyTest
    @Test
    public void testResetPasswordTokenUsableAfterClearingLock() throws Exception {
        if (!mHasFeature || !mSupportsFbe || !mHasSecureLockScreen) {
            return;
        }
        final String devicePassword = TEST_PASSWORD;

        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ResetPasswordWithTokenTest",
                "testSetResetPasswordToken", mProfileUserId);
        try {
            changeUserCredential(devicePassword, null, mParentUserId);
            changeUserCredential(null, devicePassword, mParentUserId);
            changeUserCredential(devicePassword, null, mParentUserId);
            lockProfile();
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ResetPasswordWithTokenTest",
                    "testResetPasswordBeforeUnlock", mProfileUserId);
            verifyUserCredential(RESET_PASSWORD_TEST_DEFAULT_PASSWORD, mProfileUserId);
        } finally {
            changeUserCredential(null, devicePassword, mParentUserId);
            // Cycle the device screen to flush stale password information from keyguard,
            // otherwise it will still ask for the non-existent password.
            // return screen to be on for cts test runs
            executeShellCommand("input keyevent KEYCODE_WAKEUP");
            executeShellCommand("input keyevent KEYCODE_SLEEP");
            executeShellCommand("input keyevent KEYCODE_WAKEUP");
        }
    }

    @LockSettingsTest
    @Test
    public void testIsUsingUnifiedPassword() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }

        // Freshly created profile has no separate challenge.
        verifyUnifiedPassword(true);

        // Set separate challenge and verify that the API reports it correctly.
        changeUserCredential(
                TEST_PASSWORD /* newCredential */, null /* oldCredential */, mProfileUserId);
        verifyUnifiedPassword(false);
    }

    @FlakyTest
    @LargeTest
    @LockSettingsTest
    @Test
    public void testUnlockWorkProfile_deviceWidePassword() throws Exception {
        if (!mHasFeature || !mSupportsFbe || !mHasSecureLockScreen) {
            return;
        }
        try {
            // Add a device password after the work profile has been created.
            changeUserCredential(TEST_PASSWORD, /* oldCredential= */ null, mPrimaryUserId);
            // Lock the profile with key eviction.
            lockProfile();
            // Turn on work profile, by unlocking the profile with the device password.
            verifyUserCredential(TEST_PASSWORD, mPrimaryUserId);

            // Verify profile user is running unlocked by running a sanity test on the work profile.
            installAppAsUser(SIMPLE_APP_APK, mProfileUserId);
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".SanityTest", mProfileUserId);
        } finally {
            // Clean up
            changeUserCredential(/* newCredential= */ null, TEST_PASSWORD, mPrimaryUserId);
        }
    }

    @FlakyTest
    @LargeTest
    @LockSettingsTest
    @Test
    public void testRebootDevice_unifiedPassword() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        // Waiting before rebooting prevents flakiness.
        waitForBroadcastIdle();
        changeUserCredential(TEST_PASSWORD, /* oldCredential= */ null, mPrimaryUserId);
        try {
            rebootAndWaitUntilReady();
            verifyUserCredential(TEST_PASSWORD, mPrimaryUserId);
            waitForUserUnlock(mProfileUserId);
            installAppAsUser(SIMPLE_APP_APK, mProfileUserId);
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".SanityTest", mProfileUserId);
        } finally {
            changeUserCredential(/* newCredential= */ null, TEST_PASSWORD, mPrimaryUserId);
            // Work-around for http://b/113866275 - password prompt being erroneously shown at the
            // end.
            pressPowerButton();
        }
    }

    @LargeTest
    @LockSettingsTest
    @Test
    public void testRebootDevice_separatePasswords() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        // Waiting before rebooting prevents flakiness.
        waitForBroadcastIdle();
        final String profilePassword = "profile";
        final String primaryPassword = TEST_PASSWORD;
        int managedProfileUserId = getFirstManagedProfileUserId();
        changeUserCredential(
                profilePassword, /* oldCredential= */ null, managedProfileUserId);
        changeUserCredential(primaryPassword, /* oldCredential= */ null, mPrimaryUserId);
        try {
            rebootAndWaitUntilReady();
            verifyUserCredential(profilePassword, managedProfileUserId);
            verifyUserCredential(primaryPassword, mPrimaryUserId);
            installAppAsUser(SIMPLE_APP_APK, mProfileUserId);
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".SanityTest", mProfileUserId);
        } finally {
            changeUserCredential(
                    /* newCredential= */ null, profilePassword, managedProfileUserId);
            changeUserCredential(/* newCredential= */ null, primaryPassword, mPrimaryUserId);
            // Work-around for http://b/113866275 - password prompt being erroneously shown at the
            // end.
            pressPowerButton();
        }
    }

    @Test
    public void testCreateSeparateChallengeChangedLogged() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            changeUserCredential(
                    TEST_PASSWORD /* newCredential */, null /* oldCredential */, mProfileUserId);
        }, new DevicePolicyEventWrapper.Builder(EventId.SEPARATE_PROFILE_CHALLENGE_CHANGED_VALUE)
                .setBoolean(true)
                .build());
    }

    private void verifyUnifiedPassword(boolean unified) throws DeviceNotAvailableException {
        final String testMethod =
                unified ? "testUsingUnifiedPassword" : "testNotUsingUnifiedPassword";
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".IsUsingUnifiedPasswordTest",
                testMethod, mProfileUserId);
    }

    private void lockProfile() throws Exception {
        final String cmd = "am broadcast --receiver-foreground --user " + mProfileUserId
                + " -a com.android.cts.managedprofile.LOCK_PROFILE"
                + " com.android.cts.managedprofile/.LockProfileReceiver";
        getDevice().executeShellCommand(cmd);
        waitUntilProfileLocked();
    }

    private void waitUntilProfileLocked() throws Exception {
        final String cmd = String.format("am get-started-user-state %d", mProfileUserId);
        tryWaitForSuccess(
                () -> getDevice().executeShellCommand(cmd).startsWith(USER_STATE_LOCKED),
                "The managed profile has not been locked after calling "
                        + "lockNow(FLAG_SECURE_USER_DATA)",
                TIMEOUT_USER_LOCKED_MILLIS);
    }
}
