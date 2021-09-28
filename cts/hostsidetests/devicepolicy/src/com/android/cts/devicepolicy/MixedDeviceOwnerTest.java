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

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;
import android.stats.devicepolicy.EventId;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Ignore;
import org.junit.Test;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Set of tests for device owner use cases that also apply to profile owners.
 * Tests that should be run identically in both cases are added in DeviceAndProfileOwnerTest.
 */
public class MixedDeviceOwnerTest extends DeviceAndProfileOwnerTest {

    private static final String DELEGATION_NETWORK_LOGGING = "delegation-network-logging";

    private static final String ARG_SECURITY_LOGGING_BATCH_NUMBER = "batchNumber";
    private static final int SECURITY_EVENTS_BATCH_SIZE = 100;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        if (mHasFeature) {
            mUserId = mPrimaryUserId;

            installAppAsUser(DEVICE_ADMIN_APK, mUserId);
            if (!setDeviceOwner(DEVICE_ADMIN_COMPONENT_FLATTENED, mUserId, /*expectFailure*/
                    false)) {
                removeAdmin(DEVICE_ADMIN_COMPONENT_FLATTENED, mUserId);
                getDevice().uninstallPackage(DEVICE_ADMIN_PKG);
                fail("Failed to set device owner");
            }
        }
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            assertTrue("Failed to remove device owner",
                    removeAdmin(DEVICE_ADMIN_COMPONENT_FLATTENED, mUserId));
        }
        super.tearDown();
    }

    @Test
    public void testLockTask_unaffiliatedUser() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }

        final int userId = createSecondaryUserAsProfileOwner();
        runDeviceTestsAsUser(
                DEVICE_ADMIN_PKG,
                ".AffiliationTest",
                "testLockTaskMethodsThrowExceptionIfUnaffiliated",
                userId);

        setUserAsAffiliatedUserToPrimary(userId);
        runDeviceTestsAsUser(
                DEVICE_ADMIN_PKG,
                ".AffiliationTest",
                "testSetLockTaskPackagesClearedIfUserBecomesUnaffiliated",
                userId);
    }

    @FlakyTest(bugId = 127270520)
    @Test
    public void testLockTask_affiliatedSecondaryUser() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }
        final int userId = createSecondaryUserAsProfileOwner();
        switchToUser(userId);
        setUserAsAffiliatedUserToPrimary(userId);
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".LockTaskTest", userId);
    }

    @Test
    public void testDelegatedCertInstallerDeviceIdAttestation() throws Exception {
        if (!mHasFeature) {
            return;
        }

        setUpDelegatedCertInstallerAndRunTests(() ->
                runDeviceTestsAsUser("com.android.cts.certinstaller",
                        ".DelegatedDeviceIdAttestationTest",
                        "testGenerateKeyPairWithDeviceIdAttestationExpectingSuccess", mUserId));
    }

    @FlakyTest
    @Override
    @Test
    public void testCaCertManagement() throws Exception {
        super.testCaCertManagement();
    }

    @FlakyTest(bugId = 141161038)
    @Override
    @Test
    public void testCannotRemoveUserIfRestrictionSet() throws Exception {
        super.testCannotRemoveUserIfRestrictionSet();
    }

    @FlakyTest
    @Override
    @Test
    public void testInstallCaCertLogged() throws Exception {
        super.testInstallCaCertLogged();
    }

    @FlakyTest(bugId = 137088260)
    @Test
    public void testWifi() throws Exception {
        if (!mHasFeature || !hasDeviceFeature("android.hardware.wifi")) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".WifiTest", "testGetWifiMacAddress", mUserId);
        if (isStatsdEnabled(getDevice())) {
            assertMetricsLogged(getDevice(), () -> {
                executeDeviceTestMethod(".WifiTest", "testGetWifiMacAddress");
            }, new DevicePolicyEventWrapper.Builder(EventId.GET_WIFI_MAC_ADDRESS_VALUE)
                    .setAdminPackageName(DEVICE_ADMIN_PKG)
                    .build());
        }
    }

    @Test
    public void testAdminConfiguredNetworks() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".AdminConfiguredNetworksTest", mPrimaryUserId);
    }

    @Test
    public void testSetTime() throws Exception {
        if (!mHasFeature) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".TimeManagementTest", "testSetTime");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_TIME_VALUE)
                .setAdminPackageName(DEVICE_ADMIN_PKG)
                .build());

        executeDeviceTestMethod(".TimeManagementTest", "testSetTime_failWhenAutoTimeEnabled");
    }

    @Test
    public void testSetTimeZone() throws Exception {
        if (!mHasFeature) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".TimeManagementTest", "testSetTimeZone");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_TIME_ZONE_VALUE)
                .setAdminPackageName(DEVICE_ADMIN_PKG)
                .build());

        executeDeviceTestMethod(".TimeManagementTest", "testSetTimeZone_failIfAutoTimeZoneEnabled");
    }

    Map<String, DevicePolicyEventWrapper[]> getAdditionalDelegationTests() {
        final Map<String, DevicePolicyEventWrapper[]> result = new HashMap<>();
        DevicePolicyEventWrapper[] expectedMetrics = new DevicePolicyEventWrapper[] {
                new DevicePolicyEventWrapper.Builder(EventId.SET_NETWORK_LOGGING_ENABLED_VALUE)
                        .setAdminPackageName(DELEGATE_APP_PKG)
                        .setBoolean(true)
                        .setInt(1)
                        .build(),
                new DevicePolicyEventWrapper.Builder(EventId.RETRIEVE_NETWORK_LOGS_VALUE)
                        .setAdminPackageName(DELEGATE_APP_PKG)
                        .setBoolean(true)
                        .build(),
                new DevicePolicyEventWrapper.Builder(EventId.SET_NETWORK_LOGGING_ENABLED_VALUE)
                        .setAdminPackageName(DELEGATE_APP_PKG)
                        .setBoolean(true)
                        .setInt(0)
                        .build(),
        };
        result.put(".NetworkLoggingDelegateTest", expectedMetrics);
        return result;
    }

    @Override
    List<String> getAdditionalDelegationScopes() {
        final List<String> result = new ArrayList<>();
        result.add(DELEGATION_NETWORK_LOGGING);
        return result;
    }

    @Test
    public void testLockScreenInfo() throws Exception {
        if (!mHasFeature) {
            return;
        }

        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".LockScreenInfoTest", mUserId);

        if (isStatsdEnabled(getDevice())) {
            assertMetricsLogged(getDevice(), () -> {
                executeDeviceTestMethod(".LockScreenInfoTest", "testSetAndGetLockInfo");
            }, new DevicePolicyEventWrapper.Builder(EventId.SET_DEVICE_OWNER_LOCK_SCREEN_INFO_VALUE)
                    .setAdminPackageName(DEVICE_ADMIN_PKG)
                    .build());
        }
    }

    @Test
    public void testFactoryResetProtectionPolicy() throws Exception {
        if (!mHasFeature) {
            return;
        }

        try {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".DeviceFeatureUtils",
                    "testHasFactoryResetProtectionPolicy", mUserId);
        } catch (AssertionError e) {
            // Unable to continue running tests because factory reset protection policy is not
            // supported on the device
            return;
        } catch (Exception e) {
            // Also skip test in case of other exceptions
            return;
        }

        assertMetricsLogged(getDevice(), () -> {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".FactoryResetProtectionPolicyTest", mUserId);
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_FACTORY_RESET_PROTECTION_VALUE)
                .setAdminPackageName(DEVICE_ADMIN_PKG)
                .build());
    }

    @Test
    public void testCommonCriteriaMode() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".CommonCriteriaModeTest", mUserId);
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
    public void testInstallUpdateLogged() throws Exception {
        if (!mHasFeature || !isDeviceAb() || !isStatsdEnabled(getDevice())) {
            return;
        }
        pushUpdateFileToDevice("wrongHash.zip");
        assertMetricsLogged(getDevice(), () -> {
            runDeviceTestsAsUser(DEVICE_ADMIN_PKG, ".systemupdate.InstallUpdateTest",
                    "testInstallUpdate_failWrongHash", mUserId);
        }, new DevicePolicyEventWrapper.Builder(EventId.INSTALL_SYSTEM_UPDATE_VALUE)
                    .setAdminPackageName(DEVICE_ADMIN_PKG)
                    .setBoolean(/* isDeviceAb */ true)
                    .build(),
            new DevicePolicyEventWrapper.Builder(EventId.INSTALL_SYSTEM_UPDATE_ERROR_VALUE)
                    .setInt(UPDATE_ERROR_UPDATE_FILE_INVALID)
                    .build());
    }

    @FlakyTest(bugId = 137093665)
    @Test
    public void testSecurityLoggingWithSingleUser() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // Backup stay awake setting because testGenerateLogs() will turn it off.
        final String stayAwake = getDevice().getSetting("global", "stay_on_while_plugged_in");
        try {
            // Turn logging on.
            executeDeviceTestMethod(".SecurityLoggingTest", "testEnablingSecurityLogging");
            // Reboot to ensure ro.device_owner is set to true in logd and logging is on.
            rebootAndWaitUntilReady();
            waitForUserUnlock(mUserId);

            // Generate various types of events on device side and check that they are logged.
            executeDeviceTestMethod(".SecurityLoggingTest", "testGenerateLogs");
            getDevice().executeShellCommand("whoami"); // Generate adb command securty event
            getDevice().executeShellCommand("dpm force-security-logs");
            executeDeviceTestMethod(".SecurityLoggingTest", "testVerifyGeneratedLogs");

            // Reboot the device, so the security event ids are reset.
            rebootAndWaitUntilReady();

            // Verify event ids are consistent across a consecutive batch.
            for (int batchNumber = 0; batchNumber < 3; batchNumber++) {
                generateDummySecurityLogs();
                getDevice().executeShellCommand("dpm force-security-logs");
                executeDeviceTestMethod(".SecurityLoggingTest", "testVerifyLogIds",
                        Collections.singletonMap(ARG_SECURITY_LOGGING_BATCH_NUMBER,
                                Integer.toString(batchNumber)));
            }

            // Immediately attempting to fetch events again should fail.
            executeDeviceTestMethod(".SecurityLoggingTest",
                    "testSecurityLoggingRetrievalRateLimited");
        } finally {
            // Turn logging off.
            executeDeviceTestMethod(".SecurityLoggingTest", "testDisablingSecurityLogging");
            // Restore stay awake setting.
            if (stayAwake != null) {
                getDevice().setSetting("global", "stay_on_while_plugged_in", stayAwake);
            }
        }
    }

    @Test
    public void testSecurityLoggingEnabledLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".SecurityLoggingTest", "testEnablingSecurityLogging");
            executeDeviceTestMethod(".SecurityLoggingTest", "testDisablingSecurityLogging");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_SECURITY_LOGGING_ENABLED_VALUE)
                .setAdminPackageName(DEVICE_ADMIN_PKG)
                .setBoolean(true)
                .build(),
            new DevicePolicyEventWrapper.Builder(EventId.SET_SECURITY_LOGGING_ENABLED_VALUE)
                    .setAdminPackageName(DEVICE_ADMIN_PKG)
                    .setBoolean(false)
                    .build());
    }

    @Test
    public void testSecurityLoggingWithTwoUsers() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }

        final int userId = createUser();
        try {
            // The feature can be enabled, but in a "paused" state. Attempting to retrieve logs
            // should throw security exception.
            executeDeviceTestMethod(".SecurityLoggingTest", "testEnablingSecurityLogging");
            executeDeviceTestMethod(".SecurityLoggingTest",
                    "testRetrievingSecurityLogsThrowsSecurityException");
            executeDeviceTestMethod(".SecurityLoggingTest",
                    "testRetrievingPreviousSecurityLogsThrowsSecurityException");
        } finally {
            removeUser(userId);
            executeDeviceTestMethod(".SecurityLoggingTest", "testDisablingSecurityLogging");
        }
    }

    @Test
    public void testLocationPermissionGrantNotifies() throws Exception {
        if (!mHasFeature) {
            return;
        }
        installAppPermissionAppAsUser();
        configureNotificationListener();
        executeDeviceTestMethod(".PermissionsTest", "testUserNotifiedOfLocationPermissionGrant");
    }

    private void configureNotificationListener() throws DeviceNotAvailableException {
        getDevice().executeShellCommand("cmd notification allow_listener "
                + "com.android.cts.deviceandprofileowner/.NotificationListener");
    }

    private void generateDummySecurityLogs() throws Exception {
        // Trigger security events of type TAG_ADB_SHELL_CMD.
        for (int i = 0; i < SECURITY_EVENTS_BATCH_SIZE; i++) {
            getDevice().executeShellCommand("echo just_testing_" + i);
        }
    }
    private int createSecondaryUserAsProfileOwner() throws Exception {
        final int userId = createUserAndWaitStart();
        installAppAsUser(INTENT_RECEIVER_APK, userId);
        installAppAsUser(DEVICE_ADMIN_APK, userId);
        setProfileOwnerOrFail(DEVICE_ADMIN_COMPONENT_FLATTENED, userId);
        return userId;
    }

    private void switchToUser(int userId) throws Exception {
        switchUser(userId);
        waitForBroadcastIdle();
        wakeupAndDismissKeyguard();
    }

    private void setUserAsAffiliatedUserToPrimary(int userId) throws Exception {
        // Setting the same affiliation ids on both users
        runDeviceTestsAsUser(
                DEVICE_ADMIN_PKG, ".AffiliationTest", "testSetAffiliationId1", mPrimaryUserId);
        runDeviceTestsAsUser(
                DEVICE_ADMIN_PKG, ".AffiliationTest", "testSetAffiliationId1", userId);
    }
}
