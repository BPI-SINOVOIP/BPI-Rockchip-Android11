/*
 * Copyright (C) 2014 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;
import android.stats.devicepolicy.EventId;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.util.LocationModeSetter;
import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Ignore;
import org.junit.Test;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Set of tests for Device Owner use cases.
 */
public class DeviceOwnerTest extends BaseDevicePolicyTest {

    private static final String DEVICE_OWNER_PKG = "com.android.cts.deviceowner";
    private static final String DEVICE_OWNER_APK = "CtsDeviceOwnerApp.apk";

    private static final String MANAGED_PROFILE_PKG = "com.android.cts.managedprofile";
    private static final String MANAGED_PROFILE_APK = "CtsManagedProfileApp.apk";
    private static final String MANAGED_PROFILE_ADMIN =
            MANAGED_PROFILE_PKG + ".BaseManagedProfileTest$BasicAdminReceiver";

    private static final String FEATURE_BACKUP = "android.software.backup";

    private static final String INTENT_RECEIVER_PKG = "com.android.cts.intent.receiver";
    private static final String INTENT_RECEIVER_APK = "CtsIntentReceiverApp.apk";

    private static final String SIMPLE_APP_APK ="CtsSimpleApp.apk";
    private static final String SIMPLE_APP_PKG = "com.android.cts.launcherapps.simpleapp";
    private static final String SIMPLE_APP_ACTIVITY = SIMPLE_APP_PKG + ".SimpleActivity";

    private static final String SIMPLE_SMS_APP_PKG = "android.telephony.cts.sms.simplesmsapp";
    private static final String SIMPLE_SMS_APP_APK = "SimpleSmsApp.apk";

    private static final String WIFI_CONFIG_CREATOR_PKG =
            "com.android.cts.deviceowner.wificonfigcreator";
    private static final String WIFI_CONFIG_CREATOR_APK = "CtsWifiConfigCreator.apk";

    private static final String ADMIN_RECEIVER_TEST_CLASS =
            DEVICE_OWNER_PKG + ".BasicAdminReceiver";
    private static final String DEVICE_OWNER_COMPONENT = DEVICE_OWNER_PKG + "/"
            + ADMIN_RECEIVER_TEST_CLASS;

    private static final String TEST_APP_APK = "CtsEmptyTestApp.apk";
    private static final String TEST_APP_PKG = "android.packageinstaller.emptytestapp.cts";
    private static final String TEST_APP_LOCATION = "/data/local/tmp/cts/packageinstaller/";

    private static final String ARG_NETWORK_LOGGING_BATCH_COUNT = "batchCount";

    private static final String LAUNCHER_TESTS_HAS_LAUNCHER_ACTIVITY_APK =
            "CtsHasLauncherActivityApp.apk";

    private static final int TYPE_NONE = 0;

    /**
     * Copied from {@link android.app.admin.SystemUpdatePolicy}
     */
    private static final int TYPE_INSTALL_AUTOMATIC = 1;
    private static final int TYPE_INSTALL_WINDOWED = 2;
    private static final int TYPE_POSTPONE = 3;

    /** CreateAndManageUser is available and an additional user can be created. */
    private boolean mHasCreateAndManageUserFeature;

    /**
     * Copied from {@link android.app.admin.DevicePolicyManager}
     */
    private static final String GLOBAL_SETTING_AUTO_TIME = "auto_time";
    private static final String GLOBAL_SETTING_AUTO_TIME_ZONE = "auto_time_zone";
    private static final String GLOBAL_SETTING_DATA_ROAMING = "data_roaming";
    private static final String GLOBAL_SETTING_USB_MASS_STORAGE_ENABLED =
            "usb_mass_storage_enabled";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        if (mHasFeature) {
            installAppAsUser(DEVICE_OWNER_APK, mPrimaryUserId);
            if (!setDeviceOwner(DEVICE_OWNER_COMPONENT, mPrimaryUserId,
                    /*expectFailure*/ false)) {
                removeAdmin(DEVICE_OWNER_COMPONENT, mPrimaryUserId);
                getDevice().uninstallPackage(DEVICE_OWNER_PKG);
                fail("Failed to set device owner");
            }

            // Enable the notification listener
            getDevice().executeShellCommand("cmd notification allow_listener com.android.cts.deviceowner/com.android.cts.deviceowner.NotificationListener");
        }
        mHasCreateAndManageUserFeature = mHasFeature && canCreateAdditionalUsers(1)
                && hasDeviceFeature("android.software.managed_users");
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            assertTrue("Failed to remove device owner.",
                    removeAdmin(DEVICE_OWNER_COMPONENT, mPrimaryUserId));
            getDevice().uninstallPackage(DEVICE_OWNER_PKG);
            switchUser(USER_SYSTEM);
            removeTestUsers();
        }

        super.tearDown();
    }

    @Test
    public void testDeviceOwnerSetup() throws Exception {
        executeDeviceOwnerTest("DeviceOwnerSetupTest");
    }

    @Test
    public void testProxyStaticProxyTest() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("proxy.StaticProxyTest");
    }

    @Test
    public void testProxyPacProxyTest() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("proxy.PacProxyTest");
    }

    @Test
    public void testRemoteBugreportWithTwoUsers() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }
        final int userId = createUser();
        try {
            executeDeviceTestMethod(".RemoteBugreportTest",
                    "testRequestBugreportThrowsSecurityException");
        } finally {
            removeUser(userId);
        }
    }

    @FlakyTest(bugId = 137071121)
    @Test
    public void testCreateAndManageUser_LowStorage() throws Exception {
        if (!mHasCreateAndManageUserFeature) {
            return;
        }

        try {
            // Force low storage
            getDevice().setSetting("global", "sys_storage_threshold_percentage", "100");
            getDevice().setSetting("global", "sys_storage_threshold_max_bytes",
                    String.valueOf(Long.MAX_VALUE));

            // The next createAndManageUser should return USER_OPERATION_ERROR_LOW_STORAGE.
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testCreateAndManageUser_LowStorage");
        } finally {
            getDevice().executeShellCommand(
                    "settings delete global sys_storage_threshold_percentage");
            getDevice().executeShellCommand(
                    "settings delete global sys_storage_threshold_max_bytes");
        }
    }

    @Test
    public void testCreateAndManageUser_MaxUsers() throws Exception {
        if (!mHasCreateAndManageUserFeature) {
            return;
        }

        int maxUsers = getDevice().getMaxNumberOfUsersSupported();
        // Primary user is already there, so we can create up to maxUsers -1.
        for (int i = 0; i < maxUsers - 1; i++) {
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testCreateAndManageUser");
        }
        // The next createAndManageUser should return USER_OPERATION_ERROR_MAX_USERS.
        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_MaxUsers");
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser.
     * {@link android.app.admin.DevicePolicyManager#getSecondaryUsers} is tested.
     */
    @Test
    public void testCreateAndManageUser_GetSecondaryUsers() throws Exception {
        if (!mHasCreateAndManageUserFeature) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_GetSecondaryUsers");
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method and switch
     * to the user.
     * {@link android.app.admin.DevicePolicyManager#switchUser} is tested.
     */
    @FlakyTest(bugId = 131743223)
    @Test
    public void testCreateAndManageUser_SwitchUser() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_SwitchUser");
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method and switch
     * to the user to test stop user while target user is in foreground.
     * {@link android.app.admin.DevicePolicyManager#stopUser} is tested.
     */
    @Test
    public void testCreateAndManageUser_CannotStopCurrentUser() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_CannotStopCurrentUser");
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method and start
     * the user in background to test APIs on that user.
     * {@link android.app.admin.DevicePolicyManager#startUserInBackground} is tested.
     */
    @Test
    public void testCreateAndManageUser_StartInBackground() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_StartInBackground");
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method and start
     * the user in background to test APIs on that user.
     * {@link android.app.admin.DevicePolicyManager#startUserInBackground} is tested.
     */
    @Test
    public void testCreateAndManageUser_StartInBackground_MaxRunningUsers() throws Exception {
        if (!mHasCreateAndManageUserFeature) {
            return;
        }

        int maxUsers = getDevice().getMaxNumberOfUsersSupported();
        int maxRunningUsers = getDevice().getMaxNumberOfRunningUsersSupported();

        // Primary user is already running, so we can create and start up to minimum of above - 1.
        int usersToCreateAndStart = Math.min(maxUsers, maxRunningUsers) - 1;
        for (int i = 0; i < usersToCreateAndStart; i++) {
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testCreateAndManageUser_StartInBackground");
        }

        if (maxUsers > maxRunningUsers) {
            // The next startUserInBackground should return USER_OPERATION_ERROR_MAX_RUNNING_USERS.
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testCreateAndManageUser_StartInBackground_MaxRunningUsers");
        } else {
            // The next createAndManageUser should return USER_OPERATION_ERROR_MAX_USERS.
            executeDeviceTestMethod(".CreateAndManageUserTest", "testCreateAndManageUser_MaxUsers");
        }
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method and start
     * the user in background to test APIs on that user.
     * {@link android.app.admin.DevicePolicyManager#stopUser} is tested.
     */
    @Test
    public void testCreateAndManageUser_StopUser() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_StopUser");
        assertNewUserStopped();
    }

    /**
     * Test creating an ephemeral user using the DevicePolicyManager's createAndManageUser method
     * and start the user in background, user is then stopped. The user should be removed
     * automatically even when DISALLOW_REMOVE_USER is set.
     */
    @Test
    public void testCreateAndManageUser_StopEphemeralUser_DisallowRemoveUser() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_StopEphemeralUser_DisallowRemoveUser");
        assertEquals(0, getUsersCreatedByTests().size());
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method, affiliate
     * the user and start the user in background to test APIs on that user.
     * {@link android.app.admin.DevicePolicyManager#logoutUser} is tested.
     */
    @Test
    public void testCreateAndManageUser_LogoutUser() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_LogoutUser");
        assertNewUserStopped();
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method, affiliate
     * the user and start the user in background to test APIs on that user.
     * {@link android.app.admin.DevicePolicyManager#isAffiliatedUser} is tested.
     */
    @Test
    public void testCreateAndManageUser_Affiliated() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_Affiliated");
    }

    /**
     * Test creating an ephemeral user using the DevicePolicyManager's createAndManageUser method,
     * affiliate the user and start the user in background to test APIs on that user.
     * {@link android.app.admin.DevicePolicyManager#isEphemeralUser} is tested.
     */
    @Test
    public void testCreateAndManageUser_Ephemeral() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_Ephemeral");

        List<Integer> newUsers = getUsersCreatedByTests();
        assertEquals(1, newUsers.size());
        int newUserId = newUsers.get(0);

        // Get the flags of the new user and check the user is ephemeral.
        int flags = getUserFlags(newUserId);
        assertEquals("Ephemeral flag must be set", FLAG_EPHEMERAL, flags & FLAG_EPHEMERAL);
    }

    /**
     * Test creating an user using the DevicePolicyManager's createAndManageUser method, affiliate
     * the user and start the user in background to test APIs on that user.
     * {@link android.app.admin.DevicePolicyManager#LEAVE_ALL_SYSTEM_APPS_ENABLED} is tested.
     */
    @Test
    public void testCreateAndManageUser_LeaveAllSystemApps() throws Exception {
        if (!mHasCreateAndManageUserFeature || !canStartAdditionalUsers(1)) {
            return;
        }

        executeDeviceTestMethod(".CreateAndManageUserTest",
                "testCreateAndManageUser_LeaveAllSystemApps");
    }

    @Test
    public void testCreateAndManageUser_SkipSetupWizard() throws Exception {
        if (mHasCreateAndManageUserFeature) {
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testCreateAndManageUser_SkipSetupWizard");
       }
    }

    @Test
    public void testCreateAndManageUser_AddRestrictionSet() throws Exception {
        if (mHasCreateAndManageUserFeature) {
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testCreateAndManageUser_AddRestrictionSet");
        }
    }

    @Test
    public void testCreateAndManageUser_RemoveRestrictionSet() throws Exception {
        if (mHasCreateAndManageUserFeature) {
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testCreateAndManageUser_RemoveRestrictionSet");
        }
    }

    @FlakyTest(bugId = 126955083)
    @Test
    public void testUserAddedOrRemovedBroadcasts() throws Exception {
        if (mHasCreateAndManageUserFeature) {
            executeDeviceTestMethod(".CreateAndManageUserTest",
                    "testUserAddedOrRemovedBroadcasts");
        }
    }

    @Test
    public void testUserSession() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("UserSessionTest");
    }

    @Test
    public void testNetworkLoggingWithTwoUsers() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }

        final int userId = createUser();
        try {
            // The feature can be enabled, but in a "paused" state. Attempting to retrieve logs
            // should throw security exception.
            executeDeviceTestMethod(".NetworkLoggingTest",
                    "testRetrievingNetworkLogsThrowsSecurityException");
        } finally {
            removeUser(userId);
        }
    }

    @FlakyTest(bugId = 137092833)
    @Test
    public void testNetworkLoggingWithSingleUser() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceTestMethod(".NetworkLoggingTest", "testProvidingWrongBatchTokenReturnsNull");
        executeDeviceTestMethod(".NetworkLoggingTest", "testNetworkLoggingAndRetrieval",
                Collections.singletonMap(ARG_NETWORK_LOGGING_BATCH_COUNT, Integer.toString(1)));
    }

    @Test
    public void testNetworkLogging_multipleBatches() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceTestMethod(".NetworkLoggingTest", "testNetworkLoggingAndRetrieval",
                Collections.singletonMap(ARG_NETWORK_LOGGING_BATCH_COUNT, Integer.toString(2)));
    }

    @LargeTest
    @Test
    public void testNetworkLogging_rebootResetsId() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // First batch: retrieve and verify the events.
        executeDeviceTestMethod(".NetworkLoggingTest", "testNetworkLoggingAndRetrieval",
                Collections.singletonMap(ARG_NETWORK_LOGGING_BATCH_COUNT, Integer.toString(1)));
        // Reboot the device, so the security event IDs are re-set.
        rebootAndWaitUntilReady();
        // Make sure BOOT_COMPLETED is completed before proceeding.
        waitForBroadcastIdle();
        // First batch after reboot: retrieve and verify the events.
        executeDeviceTestMethod(".NetworkLoggingTest", "testNetworkLoggingAndRetrieval",
                Collections.singletonMap(ARG_NETWORK_LOGGING_BATCH_COUNT, Integer.toString(1)));
    }


    @Test
    public void testSetAffiliationId_IllegalArgumentException() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceTestMethod(".AffiliationTest", "testSetAffiliationId_null");
        executeDeviceTestMethod(".AffiliationTest", "testSetAffiliationId_containsEmptyString");
    }

    @Test
    @Ignore("b/145932189")
    public void testSetSystemUpdatePolicyLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".SystemUpdatePolicyTest", "testSetAutomaticInstallPolicy");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_SYSTEM_UPDATE_POLICY_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setInt(TYPE_INSTALL_AUTOMATIC)
                    .build());
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".SystemUpdatePolicyTest", "testSetWindowedInstallPolicy");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_SYSTEM_UPDATE_POLICY_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setInt(TYPE_INSTALL_WINDOWED)
                    .build());
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".SystemUpdatePolicyTest", "testSetPostponeInstallPolicy");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_SYSTEM_UPDATE_POLICY_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setInt(TYPE_POSTPONE)
                    .build());
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".SystemUpdatePolicyTest", "testSetEmptytInstallPolicy");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_SYSTEM_UPDATE_POLICY_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setInt(TYPE_NONE)
                    .build());
    }

    @FlakyTest(bugId = 127101449)
    @Test
    public void testWifiConfigLockdown() throws Exception {
        final boolean hasWifi = hasDeviceFeature("android.hardware.wifi");
        if (hasWifi && mHasFeature) {
            try (LocationModeSetter locationModeSetter = new LocationModeSetter(getDevice())) {
                installAppAsUser(WIFI_CONFIG_CREATOR_APK, mPrimaryUserId);
                locationModeSetter.setLocationEnabled(true);
                executeDeviceOwnerTest("WifiConfigLockdownTest");
            } finally {
                getDevice().uninstallPackage(WIFI_CONFIG_CREATOR_PKG);
            }
        }
    }

    /**
     * Execute WifiSetHttpProxyTest as device owner.
     */
    @Test
    public void testWifiSetHttpProxyTest() throws Exception {
        final boolean hasWifi = hasDeviceFeature("android.hardware.wifi");
        if (hasWifi && mHasFeature) {
            try (LocationModeSetter locationModeSetter = new LocationModeSetter(getDevice())) {
                locationModeSetter.setLocationEnabled(true);
                executeDeviceOwnerTest("WifiSetHttpProxyTest");
            }
        }
    }

    @Test
    public void testCannotSetDeviceOwnerAgain() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // verify that we can't set the same admin receiver as device owner again
        assertFalse(setDeviceOwner(
                DEVICE_OWNER_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS, mPrimaryUserId,
                /*expectFailure*/ true));

        // verify that we can't set a different admin receiver as device owner
        try {
            installAppAsUser(MANAGED_PROFILE_APK, mPrimaryUserId);
            assertFalse(setDeviceOwner(
                    MANAGED_PROFILE_PKG + "/" + MANAGED_PROFILE_ADMIN, mPrimaryUserId,
                    /*expectFailure*/ true));
        } finally {
            // Remove the device owner in case the test fails.
            removeAdmin(MANAGED_PROFILE_PKG + "/" + MANAGED_PROFILE_ADMIN, mPrimaryUserId);
            getDevice().uninstallPackage(MANAGED_PROFILE_PKG);
        }
    }

    // Execute HardwarePropertiesManagerTest as a device owner.
    @Test
    public void testHardwarePropertiesManagerAsDeviceOwner() throws Exception {
        if (!mHasFeature) {
            return;
        }

        executeDeviceTestMethod(".HardwarePropertiesManagerTest", "testHardwarePropertiesManager");
    }

    // Execute VrTemperatureTest as a device owner.
    @Test
    public void testVrTemperaturesAsDeviceOwner() throws Exception {
        if (!mHasFeature) {
            return;
        }

        executeDeviceTestMethod(".VrTemperatureTest", "testVrTemperatures");
    }

    @Test
    public void testIsManagedDeviceProvisioningAllowed() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // This case runs when DO is provisioned
        // mHasFeature == true and provisioned, can't provision DO again.
        executeDeviceTestMethod(".PreDeviceOwnerTest", "testIsProvisioningAllowedFalse");
    }

    /**
     * Can provision Managed Profile when DO is set by default if they are the same admin.
     */
    @Test
    public void testIsManagedProfileProvisioningAllowed_deviceOwnerIsSet() throws Exception {
        if (!mHasFeature) {
            return;
        }
        if (!hasDeviceFeature("android.software.managed_users")) {
            return;
        }
        executeDeviceTestMethod(".PreDeviceOwnerTest",
                "testIsProvisioningNotAllowedForManagedProfileAction");
    }

    @FlakyTest(bugId = 137096267)
    @Test
    public void testAdminActionBookkeeping() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("AdminActionBookkeepingTest");
        if (isStatsdEnabled(getDevice())) {
            assertMetricsLogged(getDevice(), () -> {
                executeDeviceTestMethod(".AdminActionBookkeepingTest", "testRetrieveSecurityLogs");
            }, new DevicePolicyEventWrapper.Builder(EventId.RETRIEVE_SECURITY_LOGS_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .build(),
            new DevicePolicyEventWrapper.Builder(
                    EventId.RETRIEVE_PRE_REBOOT_SECURITY_LOGS_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .build());
            assertMetricsLogged(getDevice(), () -> {
                executeDeviceTestMethod(".AdminActionBookkeepingTest", "testRequestBugreport");
            }, new DevicePolicyEventWrapper.Builder(EventId.REQUEST_BUGREPORT_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .build());
        }
    }

    @Test
    public void testBluetoothRestriction() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("BluetoothRestrictionTest");
    }

    @Test
    public void testSetTime() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("SetTimeTest");
    }

    @Test
    public void testSetLocationEnabled() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("SetLocationEnabledTest");
    }

    @Test
    public void testDeviceOwnerProvisioning() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("DeviceOwnerProvisioningTest");
    }

    /**
     *  Only allow provisioning flow to be disabled if Android TV device
     */
    @Test
    public void testAllowProvisioningProperty() throws Exception {
        boolean isProvisioningAllowedForNormalUsers =
                getBooleanSystemProperty("ro.config.allowuserprovisioning", true);
        boolean isTv = hasDeviceFeature("android.software.leanback");
        assertTrue(isProvisioningAllowedForNormalUsers || isTv);
    }

    @Test
    public void testDisallowFactoryReset() throws Exception {
        if (!mHasFeature) {
            return;
        }
        int adminVersion = 24;
        changeUserRestrictionOrFail("no_factory_reset", true, mPrimaryUserId,
                DEVICE_OWNER_PKG);
        try {
            installAppAsUser(DeviceAdminHelper.getDeviceAdminApkFileName(adminVersion),
                    mPrimaryUserId);
            setDeviceAdmin(DeviceAdminHelper.getAdminReceiverComponent(adminVersion),
                    mPrimaryUserId);
            runDeviceTestsAsUser(
                    DeviceAdminHelper.getDeviceAdminApkPackage(adminVersion),
                    DeviceAdminHelper.getDeviceAdminJavaPackage() + ".WipeDataTest",
                    "testWipeDataThrowsSecurityException", mPrimaryUserId);
        } finally {
            removeAdmin(DeviceAdminHelper.getAdminReceiverComponent(adminVersion), mPrimaryUserId);
            getDevice().uninstallPackage(DeviceAdminHelper.getDeviceAdminApkPackage(adminVersion));
        }
    }

    @Test
    public void testBackupServiceEnabling() throws Exception {
        final boolean hasBackupService = getDevice().hasFeature(FEATURE_BACKUP);
        // The backup service cannot be enabled if the backup feature is not supported.
        if (!mHasFeature || !hasBackupService) {
            return;
        }
        executeDeviceTestMethod(".BackupServicePoliciesTest",
                "testEnablingAndDisablingBackupService");
    }

    @Test
    public void testDeviceOwnerCanGetDeviceIdentifiers() throws Exception {
        // The Device Owner should have access to all device identifiers.
        if (!mHasFeature) {
            return;
        }
        executeDeviceTestMethod(".DeviceIdentifiersTest",
                "testDeviceOwnerCanGetDeviceIdentifiersWithPermission");
    }

    @Test
    public void testPackageInstallCache() throws Exception {
        if (!mHasFeature) {
            return;
        }
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(getBuild());
        final File apk = buildHelper.getTestFile(TEST_APP_APK);
        try {
            getDevice().uninstallPackage(TEST_APP_PKG);
            assertTrue(getDevice().pushFile(apk, TEST_APP_LOCATION + apk.getName()));

            // Install the package in primary user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageInstall", mPrimaryUserId);

            assertMetricsLogged(getDevice(), () -> {
                runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                        "testKeepPackageCache", mPrimaryUserId);
            }, new DevicePolicyEventWrapper.Builder(EventId.SET_KEEP_UNINSTALLED_PACKAGES_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setBoolean(false)
                    .setStrings(TEST_APP_PKG)
                    .build());

            // Remove the package in primary user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageUninstall", mPrimaryUserId);

            assertMetricsLogged(getDevice(), () -> {
                // Should be able to enable the cached package in primary user
                runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                        "testInstallExistingPackage", mPrimaryUserId);
            }, new DevicePolicyEventWrapper.Builder(EventId.INSTALL_EXISTING_PACKAGE_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setBoolean(false)
                    .setStrings(TEST_APP_PKG)
                    .build());
        } finally {
            String command = "rm " + TEST_APP_LOCATION + apk.getName();
            getDevice().executeShellCommand(command);
            getDevice().uninstallPackage(TEST_APP_PKG);
        }
    }

    @LargeTest
    @Test
    public void testPackageInstallCache_multiUser() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }
        final int userId = createAffiliatedSecondaryUser();
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(getBuild());
        final File apk = buildHelper.getTestFile(TEST_APP_APK);
        try {
            getDevice().uninstallPackage(TEST_APP_PKG);
            assertTrue(getDevice().pushFile(apk, TEST_APP_LOCATION + apk.getName()));

            // Install the package in primary user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageInstall", mPrimaryUserId);

            // Should be able to enable the package in secondary user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testInstallExistingPackage", userId);

            // Remove the package in both user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageUninstall", mPrimaryUserId);
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageUninstall", userId);

            // Install the package in secondary user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageInstall", userId);

            // Should be able to enable the package in primary user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testInstallExistingPackage", mPrimaryUserId);

            // Keep the package in cache
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testKeepPackageCache", mPrimaryUserId);

            // Remove the package in both user
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageUninstall", mPrimaryUserId);
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testPackageUninstall", userId);

            // Should be able to enable the cached package in both users
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testInstallExistingPackage", userId);
            runDeviceTestsAsUser(DEVICE_OWNER_PKG, ".PackageInstallTest",
                    "testInstallExistingPackage", mPrimaryUserId);
        } finally {
            String command = "rm " + TEST_APP_LOCATION + apk.getName();
            getDevice().executeShellCommand(command);
            getDevice().uninstallPackage(TEST_APP_PKG);
        }
    }

    @Test
    public void testAirplaneModeRestriction() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("AirplaneModeRestrictionTest");
    }

    @Test
    public void testOverrideApn() throws Exception {
        if (!mHasFeature || !hasDeviceFeature("android.hardware.telephony")) {
            return;
        }
        executeDeviceOwnerTest("OverrideApnTest");
    }

    @FlakyTest(bugId = 134487729)
    @Test
    public void testPrivateDnsPolicy() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("PrivateDnsPolicyTest");
    }

    @Test
    public void testSetKeyguardDisabledLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".DevicePolicyLoggingTest", "testSetKeyguardDisabledLogged");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_KEYGUARD_DISABLED_VALUE)
                .setAdminPackageName(DEVICE_OWNER_PKG)
                .build());
    }

    @Test
    public void testSetStatusBarDisabledLogged() throws Exception {
        if (!mHasFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".DevicePolicyLoggingTest", "testSetStatusBarDisabledLogged");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_STATUS_BAR_DISABLED_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setBoolean(true)
                    .build(),
            new DevicePolicyEventWrapper.Builder(EventId.SET_STATUS_BAR_DISABLED_VALUE)
                    .setAdminPackageName(DEVICE_OWNER_PKG)
                    .setBoolean(true)
                    .build());
    }

    @Test
    public void testDefaultSmsApplication() throws Exception {
        if (!mHasFeature || !mHasTelephony) {
            return;
        }

        installAppAsUser(SIMPLE_SMS_APP_APK, mPrimaryUserId);

        executeDeviceTestMethod(".DefaultSmsApplicationTest", "testSetDefaultSmsApplication");

        getDevice().uninstallPackage(SIMPLE_SMS_APP_PKG);
    }

    @Test
    public void testNoHiddenActivityFoundTest() throws Exception {
        if (!mHasFeature) {
            return;
        }
        try {
            // Install app to primary user
            installAppAsUser(BaseLauncherAppsTest.LAUNCHER_TESTS_APK, mPrimaryUserId);
            installAppAsUser(BaseLauncherAppsTest.LAUNCHER_TESTS_SUPPORT_APK, mPrimaryUserId);
            installAppAsUser(LAUNCHER_TESTS_HAS_LAUNCHER_ACTIVITY_APK, mPrimaryUserId);

            // Run test to check if launcher api shows hidden app
            String mSerialNumber = Integer.toString(getUserSerialNumber(USER_SYSTEM));
            runDeviceTestsAsUser(BaseLauncherAppsTest.LAUNCHER_TESTS_PKG,
                    BaseLauncherAppsTest.LAUNCHER_TESTS_CLASS,
                    "testDoPoNoTestAppInjectedActivityFound",
                    mPrimaryUserId, Collections.singletonMap(BaseLauncherAppsTest.PARAM_TEST_USER,
                            mSerialNumber));
        } finally {
            getDevice().uninstallPackage(LAUNCHER_TESTS_HAS_LAUNCHER_ACTIVITY_APK);
            getDevice().uninstallPackage(BaseLauncherAppsTest.LAUNCHER_TESTS_SUPPORT_APK);
            getDevice().uninstallPackage(BaseLauncherAppsTest.LAUNCHER_TESTS_APK);
        }
    }

    @Test
    public void testSetGlobalSettingLogged() throws Exception {
        if (!mHasFeature) {
            return;
        }
        assertMetricsLogged(getDevice(), () -> {
            executeDeviceTestMethod(".DevicePolicyLoggingTest", "testSetGlobalSettingLogged");
        }, new DevicePolicyEventWrapper.Builder(EventId.SET_GLOBAL_SETTING_VALUE)
                .setAdminPackageName(DEVICE_OWNER_PKG)
                .setStrings(GLOBAL_SETTING_AUTO_TIME, "1")
                .build(),
        new DevicePolicyEventWrapper.Builder(EventId.SET_GLOBAL_SETTING_VALUE)
                .setAdminPackageName(DEVICE_OWNER_PKG)
                .setStrings(GLOBAL_SETTING_AUTO_TIME_ZONE, "1")
                .build(),
        new DevicePolicyEventWrapper.Builder(EventId.SET_GLOBAL_SETTING_VALUE)
                .setAdminPackageName(DEVICE_OWNER_PKG)
                .setStrings(GLOBAL_SETTING_DATA_ROAMING, "1")
                .build(),
        new DevicePolicyEventWrapper.Builder(EventId.SET_GLOBAL_SETTING_VALUE)
                .setAdminPackageName(DEVICE_OWNER_PKG)
                .setStrings(GLOBAL_SETTING_USB_MASS_STORAGE_ENABLED, "1")
                .build());
    }

    @Test
    public void testSetUserControlDisabledPackages() throws Exception {
        if (!mHasFeature) {
            return;
        }
        try {
            installAppAsUser(SIMPLE_APP_APK, mPrimaryUserId);
            // launch the app once before starting the test.
            startActivityAsUser(mPrimaryUserId, SIMPLE_APP_PKG, SIMPLE_APP_ACTIVITY);
            assertMetricsLogged(getDevice(),
                    () -> executeDeviceTestMethod(".UserControlDisabledPackagesTest",
                            "testSetUserControlDisabledPackages"),
                    new DevicePolicyEventWrapper.Builder(
                            EventId.SET_USER_CONTROL_DISABLED_PACKAGES_VALUE)
                            .setAdminPackageName(DEVICE_OWNER_PKG)
                            .setStrings(new String[] {SIMPLE_APP_PKG})
                            .build());
            forceStopPackageForUser(SIMPLE_APP_PKG, mPrimaryUserId);
            executeDeviceTestMethod(".UserControlDisabledPackagesTest",
                    "testForceStopWithUserControlDisabled");
            // Reboot and verify protected packages are persisted
            rebootAndWaitUntilReady();
            // The simple app package seems to be set into stopped state on reboot.
            // Launch the activity again to get it out of stopped state.
            startActivityAsUser(mPrimaryUserId, SIMPLE_APP_PKG, SIMPLE_APP_ACTIVITY);
            forceStopPackageForUser(SIMPLE_APP_PKG, mPrimaryUserId);
            executeDeviceTestMethod(".UserControlDisabledPackagesTest",
                    "testForceStopWithUserControlDisabled");
            executeDeviceTestMethod(".UserControlDisabledPackagesTest",
                    "testClearSetUserControlDisabledPackages");
            forceStopPackageForUser(SIMPLE_APP_PKG, mPrimaryUserId);
            executeDeviceTestMethod(".UserControlDisabledPackagesTest",
                    "testForceStopWithUserControlEnabled");
        } finally {
            getDevice().uninstallPackage(SIMPLE_APP_APK);
        }
    }

    private void executeDeviceOwnerTest(String testClassName) throws Exception {
        if (!mHasFeature) {
            return;
        }
        String testClass = DEVICE_OWNER_PKG + "." + testClassName;
        runDeviceTestsAsUser(DEVICE_OWNER_PKG, testClass, mPrimaryUserId);
    }

    private void executeDeviceTestMethod(String className, String testName) throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_OWNER_PKG, className, testName,
                /* deviceOwnerUserId */ mPrimaryUserId);
    }

    private int createAffiliatedSecondaryUser() throws Exception {
        final int userId = createUser();
        installAppAsUser(INTENT_RECEIVER_APK, userId);
        installAppAsUser(DEVICE_OWNER_APK, userId);
        setProfileOwnerOrFail(DEVICE_OWNER_COMPONENT, userId);

        switchUser(userId);
        waitForBroadcastIdle();
        wakeupAndDismissKeyguard();

        // Setting the same affiliation ids on both users
        runDeviceTestsAsUser(
                DEVICE_OWNER_PKG, ".AffiliationTest", "testSetAffiliationId1", mPrimaryUserId);
        runDeviceTestsAsUser(
                DEVICE_OWNER_PKG, ".AffiliationTest", "testSetAffiliationId1", userId);
        return userId;
    }

    private void executeDeviceTestMethod(String className, String testName,
            Map<String, String> params) throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(DEVICE_OWNER_PKG, className, testName,
                /* deviceOwnerUserId */ mPrimaryUserId, params);
    }

    private void assertNewUserStopped() throws Exception {
        List<Integer> newUsers = getUsersCreatedByTests();
        assertEquals(1, newUsers.size());
        int newUserId = newUsers.get(0);

        assertFalse(getDevice().isUserRunning(newUserId));
    }
}
