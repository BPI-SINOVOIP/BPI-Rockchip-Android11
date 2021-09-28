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

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;

import org.junit.Test;

import java.util.concurrent.Callable;

public class ManagedProfileContactsTest extends BaseManagedProfileTest {
    private static final String DIRECTORY_PROVIDER_APK = "CtsContactDirectoryProvider.apk";
    private static final String DIRECTORY_PROVIDER_PKG
            = "com.android.cts.contactdirectoryprovider";
    private static final String PRIMARY_DIRECTORY_PREFIX = "Primary";
    private static final String MANAGED_DIRECTORY_PREFIX = "Managed";
    private static final String DIRECTORY_PRIVOIDER_URI
            = "content://com.android.cts.contact.directory.provider/";
    private static final String SET_CUSTOM_DIRECTORY_PREFIX_METHOD = "set_prefix";

    @LargeTest
    @Test
    public void testManagedContactsUris() throws Exception {
        runManagedContactsTest(() -> {
            ContactsTestSet contactsTestSet = new ContactsTestSet(ManagedProfileContactsTest.this,
                    MANAGED_PROFILE_PKG, mParentUserId, mProfileUserId);

            contactsTestSet.setCallerIdEnabled(true);
            contactsTestSet.setContactsSearchEnabled(true);
            contactsTestSet.checkIfCanLookupEnterpriseContacts(true);
            contactsTestSet.checkIfCanFilterEnterpriseContacts(true);
            contactsTestSet.checkIfCanFilterSelfContacts();
            return null;
        });
    }

    @FlakyTest
    @Test
    public void testManagedQuickContacts() throws Exception {
        runManagedContactsTest(() -> {
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testQuickContact", mParentUserId);
            return null;
        });
    }

    @FlakyTest
    @Test
    public void testManagedContactsPolicies() throws Exception {
        runManagedContactsTest(() -> {
            ContactsTestSet contactsTestSet = new ContactsTestSet(ManagedProfileContactsTest.this,
                    MANAGED_PROFILE_PKG, mParentUserId, mProfileUserId);
            try {
                contactsTestSet.setCallerIdEnabled(true);
                contactsTestSet.setContactsSearchEnabled(false);
                contactsTestSet.checkIfCanLookupEnterpriseContacts(true);
                contactsTestSet.checkIfCanFilterEnterpriseContacts(false);
                contactsTestSet.checkIfCanFilterSelfContacts();
                contactsTestSet.setCallerIdEnabled(false);
                contactsTestSet.setContactsSearchEnabled(true);
                contactsTestSet.checkIfCanLookupEnterpriseContacts(false);
                contactsTestSet.checkIfCanFilterEnterpriseContacts(true);
                contactsTestSet.checkIfCanFilterSelfContacts();
                contactsTestSet.setCallerIdEnabled(false);
                contactsTestSet.setContactsSearchEnabled(false);
                contactsTestSet.checkIfCanLookupEnterpriseContacts(false);
                contactsTestSet.checkIfCanFilterEnterpriseContacts(false);
                contactsTestSet.checkIfCanFilterSelfContacts();
                contactsTestSet.checkIfNoEnterpriseDirectoryFound();
                if (isStatsdEnabled(getDevice())) {
                    assertMetricsLogged(getDevice(), () -> {
                        contactsTestSet.setCallerIdEnabled(true);
                        contactsTestSet.setCallerIdEnabled(false);
                    }, new DevicePolicyEventWrapper
                            .Builder(EventId.SET_CROSS_PROFILE_CALLER_ID_DISABLED_VALUE)
                            .setAdminPackageName(MANAGED_PROFILE_PKG)
                            .setBoolean(false)
                            .build(),
                    new DevicePolicyEventWrapper
                            .Builder(EventId.SET_CROSS_PROFILE_CALLER_ID_DISABLED_VALUE)
                            .setAdminPackageName(MANAGED_PROFILE_PKG)
                            .setBoolean(true)
                            .build());
                    assertMetricsLogged(getDevice(), () -> {
                        contactsTestSet.setContactsSearchEnabled(true);
                        contactsTestSet.setContactsSearchEnabled(false);
                    }, new DevicePolicyEventWrapper
                            .Builder(EventId.SET_CROSS_PROFILE_CONTACTS_SEARCH_DISABLED_VALUE)
                            .setAdminPackageName(MANAGED_PROFILE_PKG)
                            .setBoolean(false)
                            .build(),
                    new DevicePolicyEventWrapper
                            .Builder(
                            EventId.SET_CROSS_PROFILE_CONTACTS_SEARCH_DISABLED_VALUE)
                            .setAdminPackageName(MANAGED_PROFILE_PKG)
                            .setBoolean(true)
                            .build());
                }
                return null;
            } finally {
                // reset policies
                contactsTestSet.setCallerIdEnabled(true);
                contactsTestSet.setContactsSearchEnabled(true);
            }
        });
    }

    private void setDirectoryPrefix(String directoryName, int userId)
            throws DeviceNotAvailableException {
        String command = "content call --uri " + DIRECTORY_PRIVOIDER_URI
                + " --user " + userId
                + " --method " + SET_CUSTOM_DIRECTORY_PREFIX_METHOD
                + " --arg " + directoryName;
        LogUtil.CLog.d("Output for command " + command + ": "
                + getDevice().executeShellCommand(command));
    }

    private void runManagedContactsTest(Callable<Void> callable) throws Exception {
        if (!mHasFeature) {
            return;
        }

        try {
            // Allow cross profile contacts search.
            // TODO test both on and off.
            getDevice().executeShellCommand(
                    "settings put --user " + mProfileUserId
                    + " secure managed_profile_contact_remote_search 1");

            // Wait for updating cache
            waitForBroadcastIdle();

            // Add test account
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testAddTestAccount", mParentUserId);
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testAddTestAccount", mProfileUserId);

            // Install directory provider to both primary and managed profile
            installAppAsUser(DIRECTORY_PROVIDER_APK, USER_ALL);
            setDirectoryPrefix(PRIMARY_DIRECTORY_PREFIX, mParentUserId);
            setDirectoryPrefix(MANAGED_DIRECTORY_PREFIX, mProfileUserId);

            // Check enterprise directory API works
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testGetDirectoryListInPrimaryProfile", mParentUserId);

            // Insert Primary profile Contacts
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testPrimaryProfilePhoneAndEmailLookup_insertedAndfound", mParentUserId);
            // Insert Managed profile Contacts
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testManagedProfilePhoneAndEmailLookup_insertedAndfound", mProfileUserId);
            // Insert a primary contact with same phone & email as other
            // enterprise contacts
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testPrimaryProfileDuplicatedPhoneEmailContact_insertedAndfound",
                    mParentUserId);
            // Insert a enterprise contact with same phone & email as other
            // primary contacts
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testManagedProfileDuplicatedPhoneEmailContact_insertedAndfound",
                    mProfileUserId);

            callable.call();

        } finally {
            // Clean up in managed profile and primary profile
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testCurrentProfileContacts_removeContacts", mProfileUserId);
            runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".ContactsTest",
                    "testCurrentProfileContacts_removeContacts", mParentUserId);
            getDevice().uninstallPackage(DIRECTORY_PROVIDER_PKG);
        }
    }

    /*
     * Container for running ContactsTest under multi-user environment
     */
    private static class ContactsTestSet {

        private ManagedProfileContactsTest mManagedProfileContactsTest;
        private String mManagedProfilePackage;
        private int mParentUserId;
        private int mProfileUserId;

        public ContactsTestSet(ManagedProfileContactsTest managedProfileContactsTest,
                String managedProfilePackage, int parentUserId, int profileUserId) {
            mManagedProfileContactsTest = managedProfileContactsTest;
            mManagedProfilePackage = managedProfilePackage;
            mParentUserId = parentUserId;
            mProfileUserId = profileUserId;
        }

        private void runDeviceTestsAsUser(String pkgName, String testClassName,
                String testMethodName, Integer userId) throws DeviceNotAvailableException {
            mManagedProfileContactsTest.runDeviceTestsAsUser(pkgName, testClassName, testMethodName,
                    userId);
        }

        // Enable / Disable
        public void setCallerIdEnabled(boolean enabled) throws DeviceNotAvailableException {
            if (enabled) {
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testSetCrossProfileCallerIdDisabled_false", mProfileUserId);
            } else {
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testSetCrossProfileCallerIdDisabled_true", mProfileUserId);
            }
        }

        // Enable / Disable cross profile contacts search
        public void setContactsSearchEnabled(boolean enabled) throws DeviceNotAvailableException {
            if (enabled) {
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testSetCrossProfileContactsSearchDisabled_false", mProfileUserId);
            } else {
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testSetCrossProfileContactsSearchDisabled_true", mProfileUserId);
            }
        }

        public void checkIfCanLookupEnterpriseContacts(boolean expected)
                throws DeviceNotAvailableException {
            // Primary user cannot use ordinary phone/email lookup api to access
            // managed contacts
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfilePhoneLookup_canNotAccessEnterpriseContact", mParentUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEmailLookup_canNotAccessEnterpriseContact", mParentUserId);
            // Primary user can use ENTERPRISE_CONTENT_FILTER_URI to access
            // primary contacts
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterprisePhoneLookup_canAccessPrimaryContact",
                    mParentUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterpriseEmailLookup_canAccessPrimaryContact",
                    mParentUserId);
            // When there exist contacts with the same phone/email in primary &
            // enterprise,
            // primary user can use ENTERPRISE_CONTENT_FILTER_URI to access the
            // primary contact.
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterpriseEmailLookupDuplicated_canAccessPrimaryContact",
                    mParentUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterprisePhoneLookupDuplicated_canAccessPrimaryContact",
                    mParentUserId);

            // Managed user cannot use ordinary phone/email lookup api to access
            // primary contacts
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfilePhoneLookup_canNotAccessPrimaryContact", mProfileUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEmailLookup_canNotAccessPrimaryContact", mProfileUserId);
            // Managed user can use ENTERPRISE_CONTENT_FILTER_URI to access
            // enterprise contacts
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterprisePhoneLookup_canAccessEnterpriseContact",
                    mProfileUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterpriseEmailLookup_canAccessEnterpriseContact",
                    mProfileUserId);
            // Managed user cannot use ENTERPRISE_CONTENT_FILTER_URI to access
            // primary contacts
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterprisePhoneLookup_canNotAccessPrimaryContact",
                    mProfileUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterpriseEmailLookup_canNotAccessPrimaryContact",
                    mProfileUserId);
            // When there exist contacts with the same phone/email in primary &
            // enterprise,
            // managed user can use ENTERPRISE_CONTENT_FILTER_URI to access the
            // enterprise contact.
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterpriseEmailLookupDuplicated_canAccessEnterpriseContact",
                    mProfileUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterprisePhoneLookupDuplicated_canAccessEnterpriseContact",
                    mProfileUserId);

            // Check if phone lookup can access primary directories
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterprisePhoneLookup_canAccessPrimaryDirectories",
                    mParentUserId);

            // Check if email lookup can access primary directories
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterpriseEmailLookup_canAccessPrimaryDirectories",
                    mParentUserId);

            if (expected) {
                // Primary user can use ENTERPRISE_CONTENT_FILTER_URI to access
                // managed profile contacts
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterprisePhoneLookup_canAccessEnterpriseContact",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseEmailLookup_canAccessEnterpriseContact",
                        mParentUserId);

                // Make sure SIP enterprise lookup works too.
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseSipLookup_canAccessEnterpriseContact",
                        mParentUserId);

                // Check if phone lookup can access enterprise directories
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterprisePhoneLookup_canAccessManagedDirectories",
                        mParentUserId);

                // Check if email lookup can access enterprise directories
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseEmailLookup_canAccessManagedDirectories",
                        mParentUserId);
            } else {
                // Primary user cannot use ENTERPRISE_CONTENT_FILTER_URI to
                // access managed contacts
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterprisePhoneLookup_canNotAccessEnterpriseContact",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterprisePhoneLookup_canNotAccessManagedDirectories",
                        mParentUserId);

                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseEmailLookup_canNotAccessManagedDirectories",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterprisePhoneLookup_canNotAccessManagedDirectories",
                        mParentUserId);
            }
        }

        public void checkIfCanFilterSelfContacts() throws DeviceNotAvailableException {
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterpriseCallableFilter_canAccessPrimaryDirectories",
                    mParentUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterpriseCallableFilter_canAccessManagedDirectories",
                    mProfileUserId);

            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterpriseEmailFilter_canAccessPrimaryDirectories",
                    mParentUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testEnterpriseProfileEnterpriseEmailFilter_canAccessManagedDirectories",
                    mProfileUserId);

            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterpriseContactFilter_canAccessPrimaryDirectories",
                    mParentUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterpriseContactFilter_canAccessManagedDirectories",
                    mProfileUserId);

            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterprisePhoneFilter_canAccessPrimaryDirectories",
                    mParentUserId);
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testManagedProfileEnterprisePhoneFilter_canAccessManagedDirectories",
                    mProfileUserId);
        }

        public void checkIfCanFilterEnterpriseContacts(boolean expected)
                throws DeviceNotAvailableException {
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testFilterUriWhenDirectoryParamMissing", mParentUserId);
            if (expected) {
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseCallableFilter_canAccessManagedDirectories",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseEmailFilter_canAccessManagedDirectories",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseContactFilter_canAccessManagedDirectories",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterprisePhoneFilter_canAccessManagedDirectories",
                        mParentUserId);
            } else {
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseCallableFilter_canNotAccessManagedDirectories",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseEmailFilter_canNotAccessManagedDirectories",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterpriseContactFilter_canNotAccessManagedDirectories",
                        mParentUserId);
                runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                        "testPrimaryProfileEnterprisePhoneFilter_canNotAccessManagedDirectories",
                        mParentUserId);
            }
        }

        public void checkIfNoEnterpriseDirectoryFound() throws DeviceNotAvailableException {
            runDeviceTestsAsUser(mManagedProfilePackage, ".ContactsTest",
                    "testPrimaryProfileEnterpriseDirectories_canNotAccessManagedDirectories",
                    mParentUserId);
        }
    }
}
