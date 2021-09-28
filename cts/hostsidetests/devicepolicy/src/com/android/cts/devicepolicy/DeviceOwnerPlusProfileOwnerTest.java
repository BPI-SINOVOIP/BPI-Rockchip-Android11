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
 * limitations under the License.
 */

package com.android.cts.devicepolicy;

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;
import android.stats.devicepolicy.EventId;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper.Builder;

import org.junit.Ignore;
import org.junit.Test;

import java.util.List;

/**
 * Tests for device owner and profile owner as well as multiple users. Device owner is setup
 * {@link #setUp()} and it is always the {@link #COMP_DPC_PKG}. Each test is required to call
 * {@link #setupManagedSecondaryUser} to create another user on each test case.
 * As combining a profile owner with a device owner is not supported, this class contains
 * negative test cases to ensure this combination cannot be set up.
 */
public class DeviceOwnerPlusProfileOwnerTest extends BaseDevicePolicyTest {
    private static final String BIND_DEVICE_ADMIN_SERVICE_GOOD_SETUP_TEST =
            "com.android.cts.comp.BindDeviceAdminServiceGoodSetupTest";
    private static final String MANAGED_PROFILE_PROVISIONING_TEST =
            "com.android.cts.comp.provisioning.ManagedProfileProvisioningTest";
    private static final String BIND_DEVICE_ADMIN_SERVICE_FAILS_TEST =
            "com.android.cts.comp.BindDeviceAdminServiceFailsTest";
    private static final String DEVICE_WIDE_LOGGING_TEST =
            "com.android.cts.comp.DeviceWideLoggingFeaturesTest";
    private static final String AFFILIATION_TEST =
            "com.android.cts.comp.provisioning.AffiliationTest";
    private static final String USER_RESTRICTION_TEST =
            "com.android.cts.comp.provisioning.UserRestrictionTest";
    private static final String MANAGEMENT_TEST =
            "com.android.cts.comp.ManagementTest";

    private static final String COMP_DPC_PKG = "com.android.cts.comp";
    private static final DevicePolicyEventWrapper WIPE_DATA_WITH_REASON_DEVICE_POLICY_EVENT =
            new Builder(EventId.WIPE_DATA_WITH_REASON_VALUE)
                    .setAdminPackageName(COMP_DPC_PKG)
                    .setInt(0)
                    .setStrings("notCalledFromParent")
                    .build();
    private static final String COMP_DPC_APK = "CtsCorpOwnedManagedProfile.apk";
    private static final String COMP_DPC_ADMIN =
            COMP_DPC_PKG + "/com.android.cts.comp.AdminReceiver";
    private static final String COMP_DPC_PKG2 = "com.android.cts.comp2";
    private static final String COMP_DPC_APK2 = "CtsCorpOwnedManagedProfile2.apk";
    private static final String COMP_DPC_ADMIN2 =
            COMP_DPC_PKG2 + "/com.android.cts.comp.AdminReceiver";

    @Override
    public void setUp() throws Exception {
        super.setUp();
        // We need managed user to be supported in order to create a profile of the user owner.
        mHasFeature = mHasFeature && hasDeviceFeature("android.software.managed_users");
        if (mHasFeature) {
            // Set device owner.
            installAppAsUser(COMP_DPC_APK, mPrimaryUserId);
            if (!setDeviceOwner(COMP_DPC_ADMIN, mPrimaryUserId, /*expectFailure*/ false)) {
                removeAdmin(COMP_DPC_ADMIN, mPrimaryUserId);
                fail("Failed to set device owner");
            }
            runDeviceTestsAsUser(
                    COMP_DPC_PKG,
                    MANAGEMENT_TEST,
                    "testIsDeviceOwner",
                    mPrimaryUserId);
        }
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            assertTrue("Failed to remove device owner.",
                    removeAdmin(COMP_DPC_ADMIN, mPrimaryUserId));
        }

        super.tearDown();
    }

    /**
     * Both device owner and profile are the same package ({@link #COMP_DPC_PKG}).
     */
    @LargeTest
    @Test
    public void testCannotAddManagedProfileWithDeviceOwner() throws Exception {
        if (!mHasFeature) {
            return;
        }

        assertCannotCreateManagedProfile(mPrimaryUserId);
    }

    /**
     * DISABLED: Test currently disabled because of a bug in managed provisioning.
     * TODO: Re-enable once b/146048940 is fixed.
     * Same as {@link #testCannotAddManagedProfileWithDeviceOwner} except
     * creating managed profile through ManagedProvisioning like normal flow
     */
    @FlakyTest
    @Test
    @Ignore
    public void testCannotAddManagedProfileViaManagedProvisioning()
            throws Exception {
        if (!mHasFeature) {
            return;
        }
        int profileUserId = provisionCorpOwnedManagedProfile();
        assertFalse(profileUserId >= 0);
    }

    /**
     * Test that isProvisioningAllowed returns false when called with
     * ACTION_PROVISION_MANAGED_PROFILE when there's a device owner.
     */
    @Test
    public void testProvisioningNotAllowedWithDeviceOwner() throws Exception {
        if (!mHasFeature) {
            return;
        }

        assertProvisionManagedProfileNotAllowed(COMP_DPC_PKG);
    }

    /**
     * Both device owner and profile are the same package ({@link #COMP_DPC_PKG}), as setup
     * by createAndManagedUser.
     */
    @FlakyTest
    @Test
    public void testBindDeviceAdminServiceAsUser_secondaryUser() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }
        int secondaryUserId = setupManagedSecondaryUser();

        installAppAsUser(COMP_DPC_APK2, mPrimaryUserId);
        installAppAsUser(COMP_DPC_APK2, secondaryUserId);

        // Shouldn't be possible to bind to each other, as they are not affiliated.
        verifyBindDeviceAdminServiceAsUserFails(secondaryUserId);

        // Set the same affiliation ids, and check that DO and PO can now bind to each other.
        setSameAffiliationId(secondaryUserId);
        verifyBindDeviceAdminServiceAsUser(secondaryUserId);
    }

    @FlakyTest(bugId = 141161038)
    @Test
    public void testCannotRemoveUserIfRestrictionSet() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }
        int secondaryUserId = setupManagedSecondaryUser();
        addDisallowRemoveUserRestriction();
        assertFalse(getDevice().removeUser(secondaryUserId));

        clearDisallowRemoveUserRestriction();
        assertTrue(getDevice().removeUser(secondaryUserId));
    }

    @Test
    public void testCannotAddProfileIfRestrictionSet() throws Exception {
        if (!mHasFeature) {
            return;
        }
        // by default, disallow add managed profile users restriction is set.
        assertCannotCreateManagedProfile(mPrimaryUserId);
    }

    private void sendWipeProfileBroadcast(int userId) throws Exception {
        final String cmd = "am broadcast --receiver-foreground --user " + userId
                + " -a com.android.cts.comp.WIPE_DATA"
                + " com.android.cts.comp/.WipeDataReceiver";
        getDevice().executeShellCommand(cmd);
    }

    @Test
    public void testWipeData_secondaryUser() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1)) {
            return;
        }
        int secondaryUserId = setupManagedSecondaryUser();
        addDisallowRemoveUserRestriction();
        // The PO of the managed user should be allowed to delete it, even though the disallow
        // remove user restriction is set.
        sendWipeProfileBroadcast(secondaryUserId);
        waitUntilUserRemoved(secondaryUserId);
    }

    @Test
    public void testWipeData_secondaryUserLogged() throws Exception {
        if (!mHasFeature || !canCreateAdditionalUsers(1) || !isStatsdEnabled(getDevice())) {
            return;
        }
        int secondaryUserId = setupManagedSecondaryUser();
        addDisallowRemoveUserRestriction();
        assertMetricsLogged(getDevice(), () -> {
            sendWipeProfileBroadcast(secondaryUserId);
            waitUntilUserRemoved(secondaryUserId);
        }, WIPE_DATA_WITH_REASON_DEVICE_POLICY_EVENT);
    }

    @Test
    public void testNetworkAndSecurityLoggingAvailableIfAffiliated() throws Exception {
        if (!mHasFeature) {
            return;
        }

        if (!canCreateAdditionalUsers(2)) {
            return;
        }
        // If secondary users are allowed, create an affiliated one, to check that this still
        // works if having both an affiliated user and an affiliated managed profile.
        final int secondaryUserId = setupManagedSecondaryUser();

        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                DEVICE_WIDE_LOGGING_TEST,
                "testEnablingNetworkAndSecurityLogging",
                mPrimaryUserId);
        try {
            // No affiliation ids have been set on the profile, the features shouldn't be available.
            runDeviceTestsAsUser(
                    COMP_DPC_PKG,
                    DEVICE_WIDE_LOGGING_TEST,
                    "testRetrievingLogsThrowsSecurityException",
                    mPrimaryUserId);

            // Affiliate the DO and the secondary user.
            setSameAffiliationId(secondaryUserId);
            runDeviceTestsAsUser(
                    COMP_DPC_PKG,
                    DEVICE_WIDE_LOGGING_TEST,
                    "testRetrievingLogsDoesNotThrowException",
                    mPrimaryUserId);

            setDifferentAffiliationId(secondaryUserId);
            runDeviceTestsAsUser(
                    COMP_DPC_PKG,
                    DEVICE_WIDE_LOGGING_TEST,
                    "testRetrievingLogsThrowsSecurityException",
                    mPrimaryUserId);
        } finally {
            runDeviceTestsAsUser(
                COMP_DPC_PKG,
                DEVICE_WIDE_LOGGING_TEST,
                "testDisablingNetworkAndSecurityLogging",
                mPrimaryUserId);
        }
    }

    @FlakyTest
    @Test
    public void testRequestBugreportAvailableIfAffiliated() throws Exception {
        if (!mHasFeature) {
            return;
        }

        if (!canCreateAdditionalUsers(2)) {
            return;
        }

        final int secondaryUserId = setupManagedSecondaryUser();

        // No affiliation ids have been set on the secondary user, the feature shouldn't be
        // available.
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                DEVICE_WIDE_LOGGING_TEST,
                "testRequestBugreportThrowsSecurityException",
                mPrimaryUserId);

        // Affiliate the DO and the secondary user.
        setSameAffiliationId(secondaryUserId);
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                DEVICE_WIDE_LOGGING_TEST,
                "testRequestBugreportDoesNotThrowException",
                mPrimaryUserId);

        setDifferentAffiliationId(secondaryUserId, COMP_DPC_PKG);
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                DEVICE_WIDE_LOGGING_TEST,
                "testRequestBugreportThrowsSecurityException",
                mPrimaryUserId);
    }

    private void verifyBindDeviceAdminServiceAsUser(int profileOwnerUserId) throws Exception {
        // Installing a non managing app (neither device owner nor profile owner).
        installAppAsUser(COMP_DPC_APK2, mPrimaryUserId);
        installAppAsUser(COMP_DPC_APK2, profileOwnerUserId);

        // Testing device owner -> profile owner.
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                BIND_DEVICE_ADMIN_SERVICE_GOOD_SETUP_TEST,
                mPrimaryUserId);
        // Testing profile owner -> device owner.
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                BIND_DEVICE_ADMIN_SERVICE_GOOD_SETUP_TEST,
                profileOwnerUserId);
    }

    private void verifyBindDeviceAdminServiceAsUserFails(int profileOwnerUserId) throws Exception {
        // Installing a non managing app (neither device owner nor profile owner).
        installAppAsUser(COMP_DPC_APK2, mPrimaryUserId);
        installAppAsUser(COMP_DPC_APK2, profileOwnerUserId);

        // Testing device owner -> profile owner.
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                BIND_DEVICE_ADMIN_SERVICE_FAILS_TEST,
                mPrimaryUserId);
        // Testing profile owner -> device owner.
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                BIND_DEVICE_ADMIN_SERVICE_FAILS_TEST,
                profileOwnerUserId);
    }

    private void setSameAffiliationId(
            int profileOwnerUserId, String profileOwnerPackage) throws Exception {
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                AFFILIATION_TEST,
                "testSetAffiliationId1",
                mPrimaryUserId);
        runDeviceTestsAsUser(
                profileOwnerPackage,
                AFFILIATION_TEST,
                "testSetAffiliationId1",
                profileOwnerUserId);
    }

    private void setSameAffiliationId(int profileOwnerUserId) throws Exception {
        setSameAffiliationId(profileOwnerUserId, COMP_DPC_PKG);
    }

    private void setDifferentAffiliationId(
            int profileOwnerUserId, String profileOwnerPackage) throws Exception {
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                AFFILIATION_TEST,
                "testSetAffiliationId1",
                mPrimaryUserId);
        runDeviceTestsAsUser(
                profileOwnerPackage,
                AFFILIATION_TEST,
                "testSetAffiliationId2",
                profileOwnerUserId);
    }

    private void setDifferentAffiliationId(int profileOwnerUserId) throws Exception {
        setDifferentAffiliationId(profileOwnerUserId, COMP_DPC_PKG);
    }

    private void assertProvisionManagedProfileNotAllowed(String packageName) throws Exception {
        runDeviceTestsAsUser(
                packageName,
                MANAGEMENT_TEST,
                "testProvisionManagedProfileNotAllowed",
                mPrimaryUserId);
    }

    /** Returns the user id of the newly created managed profile */
    private int setupManagedProfile(String apkName, String packageName,
            String adminReceiverClassName) throws Exception {
        final int userId = createManagedProfile(mPrimaryUserId);
        installAppAsUser(apkName, userId);
        setProfileOwnerOrFail(adminReceiverClassName, userId);
        startUserAndWait(userId);
        runDeviceTestsAsUser(
                packageName,
                MANAGEMENT_TEST,
                "testIsManagedProfile",
                userId);
        return userId;
    }

    /** Returns the user id of the newly created secondary user */
    private int setupManagedSecondaryUser() throws Exception {
        assertTrue(canCreateAdditionalUsers(1));

        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                MANAGEMENT_TEST,
                "testCreateSecondaryUser",
                mPrimaryUserId);
        List<Integer> newUsers = getUsersCreatedByTests();
        assertEquals(1, newUsers.size());
        int secondaryUserId = newUsers.get(0);
        getDevice().startUser(secondaryUserId, /* waitFlag= */ true);
        return secondaryUserId;
    }

    /** Returns the user id of the newly created secondary user */
    private int provisionCorpOwnedManagedProfile() throws Exception {
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                MANAGED_PROFILE_PROVISIONING_TEST,
                "testProvisioningCorpOwnedManagedProfile",
                mPrimaryUserId);
        return getFirstManagedProfileUserId();
    }

    /**
     * Add {@link android.os.UserManager#DISALLOW_REMOVE_USER}.
     */
    private void addDisallowRemoveUserRestriction() throws Exception {
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                USER_RESTRICTION_TEST,
                "testAddDisallowRemoveUserRestriction",
                mPrimaryUserId);
    }

    /**
     * Clear {@link android.os.UserManager#DISALLOW_REMOVE_USER}.
     */
    private void clearDisallowRemoveUserRestriction() throws Exception {
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                USER_RESTRICTION_TEST,
                "testClearDisallowRemoveUserRestriction",
                mPrimaryUserId);
    }

    private void assertOtherProfilesEqualsBindTargetUsers(int otherProfileUserId) throws Exception {
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                MANAGEMENT_TEST,
                "testOtherProfilesEqualsBindTargetUsers",
                mPrimaryUserId);
        runDeviceTestsAsUser(
                COMP_DPC_PKG,
                MANAGEMENT_TEST,
                "testOtherProfilesEqualsBindTargetUsers",
                otherProfileUserId);
    }
}
