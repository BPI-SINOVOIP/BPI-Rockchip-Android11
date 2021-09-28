/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.junit.Assume.assumeTrue;

import org.junit.Before;
import org.junit.Test;

import java.util.Collections;
import java.util.Map;

/**
 * Tests to verify
 * {@link android.content.pm.crossprofile.CrossProfileApps#canRequestInteractAcrossProfiles()},
 * {@link android.content.pm.crossprofile.CrossProfileApps#canInteractAcrossProfiles()}, and
 * {@link
 * android.content.pm.crossprofile.CrossProfileApps#createRequestInteractAcrossProfilesIntent()}.
 *
 * The rest of the tests for {@link android.content.pm.crossprofile.CrossProfileApps}
 * can be found in {@link CrossProfileAppsHostSideTest}.
 */
public class CrossProfileAppsPermissionHostSideTest extends BaseDevicePolicyTest {
    private static final String TEST_WITH_REQUESTED_PERMISSION_PACKAGE =
            "com.android.cts.crossprofileappstest";
    private static final String TEST_WITH_REQUESTED_PERMISSION_CLASS =
            ".CrossProfileAppsPermissionToInteractTest";
    private static final String TEST_WITH_REQUESTED_PERMISSION_APK = "CtsCrossProfileAppsTests.apk";
    private static final String TEST_WITH_REQUESTED_PERMISSION_RECEIVER_TEST_CLASS =
            TEST_WITH_REQUESTED_PERMISSION_PACKAGE + ".AdminReceiver";

    private static final String TEST_WITH_NO_REQUESTED_PERMISSION_PACKAGE =
            "com.android.cts.crossprofilenopermissionappstest";
    private static final String TEST_WITH_NO_REQUESTED_PERMISSION_CLASS =
            ".CrossProfileAppsWithNoPermission";
    private static final String TEST_WITH_NO_REQUESTED_PERMISSION_APK =
            "CtsCrossProfileAppsWithNoPermissionTests.apk";

    private static final String MANAGED_PROFILE_PKG = "com.android.cts.managedprofile";
    private static final String MANAGED_PROFILE_APK = "CtsManagedProfileApp.apk";
    private static final String ADMIN_RECEIVER_TEST_CLASS =
            MANAGED_PROFILE_PKG + ".BaseManagedProfileTest$BasicAdminReceiver";
    private static final String PARAM_CROSS_PROFILE_PACKAGE = "crossProfilePackage";

    private int mProfileId;

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        assumeTrue(mSupportsMultiUser && mHasManagedUserFeature);
    }

    @Test
    public void testCanRequestInteractAcrossProfiles_fromPersonalProfile_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);
        addDefaultCrossProfilePackage(mProfileId, TEST_WITH_REQUESTED_PERMISSION_PACKAGE);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanRequestInteractAcrossProfiles_returnsTrue",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanRequestInteractAcrossProfiles_fromWorkProfile_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);
        addDefaultCrossProfilePackage(mProfileId, TEST_WITH_REQUESTED_PERMISSION_PACKAGE);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanRequestInteractAcrossProfiles_returnsTrue",
                mProfileId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanRequestInteractAcrossProfiles_noOtherProfiles_ReturnsFalse()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanRequestInteractAcrossProfiles_returnsFalse",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanRequestInteractAcrossProfiles_packageNotWhitelisted_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanRequestInteractAcrossProfiles_returnsTrue",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanRequestInteractAcrossProfiles_packageNotInstalled_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        mProfileId = createManagedProfile(mPrimaryUserId);
        getDevice().startUser(mProfileId, /*waitFlag= */true);
        installAppAsUser(MANAGED_PROFILE_APK, mProfileId);
        setProfileOwnerOrFail(MANAGED_PROFILE_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS,
                mProfileId);
        addDefaultCrossProfilePackage(mProfileId, TEST_WITH_REQUESTED_PERMISSION_PACKAGE);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanRequestInteractAcrossProfiles_returnsTrue",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanRequestInteractAcrossProfiles_permissionNotRequested_returnsFalse()
            throws Exception {
        installAppAsUser(TEST_WITH_NO_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_NO_REQUESTED_PERMISSION_APK);
        addDefaultCrossProfilePackage(mProfileId, TEST_WITH_NO_REQUESTED_PERMISSION_PACKAGE);

        runDeviceTestsAsUser(
                TEST_WITH_NO_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_NO_REQUESTED_PERMISSION_CLASS,
                "testCanRequestInteractAcrossProfiles_permissionNotRequested_returnsFalse",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanRequestInteractAcrossProfiles_profileOwner_returnsFalse()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        mProfileId = createManagedProfile(mPrimaryUserId);
        getDevice().startUser(mProfileId, /* waitFlag= */ true);
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mProfileId);
        final String receiverComponentName =
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE + "/"
                        + TEST_WITH_REQUESTED_PERMISSION_RECEIVER_TEST_CLASS;
        setProfileOwnerOrFail(receiverComponentName, mProfileId);
        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testSetCrossProfilePackages_noAsserts",
                mProfileId,
                createCrossProfilePackageParam(TEST_WITH_REQUESTED_PERMISSION_PACKAGE));

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanRequestInteractAcrossProfiles_returnsFalse",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanInteractAcrossProfiles_withAppOpEnabled_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanInteractAcrossProfiles_withAppOpEnabled_returnsTrue",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanInteractAcrossProfiles_withCrossProfilesPermission_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanInteractAcrossProfiles_withCrossProfilesPermission_returnsTrue",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanInteractAcrossProfiles_withCrossUsersPermission_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanInteractAcrossProfiles_withCrossUsersPermission_returnsTrue",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanInteractAcrossProfiles_withCrossUsersFullPermission_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanInteractAcrossProfiles_withCrossUsersFullPermission_returnsTrue",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanInteractAcrossProfiles_fromWorkProfile_returnsTrue()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanInteractAcrossProfiles_withAppOpEnabled_returnsTrue",
                mProfileId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanInteractAcrossProfiles_withAppOpDisabled_returnsFalse()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanInteractAcrossProfiles_withAppOpDisabled_returnsFalse",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCanInteractAcrossProfiles_withNoOtherProfile_returnsFalse()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCanInteractAcrossProfiles_withNoOtherProfile_returnsFalse",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCreateRequestInteractAcrossProfilesIntent_canRequestInteraction_returnsIntent()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);
        addDefaultCrossProfilePackage(mProfileId, TEST_WITH_REQUESTED_PERMISSION_PACKAGE);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCreateRequestInteractAcrossProfilesIntent_canRequestInteraction_returnsIntent",
                mPrimaryUserId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCreateRequestInteractAcrossProfilesIntent_fromWorkProfile_returnsIntent()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);
        addManagedProfileAndInstallRequiredPackages(TEST_WITH_REQUESTED_PERMISSION_APK);
        addDefaultCrossProfilePackage(mProfileId, TEST_WITH_REQUESTED_PERMISSION_PACKAGE);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCreateRequestInteractAcrossProfilesIntent_canRequestInteraction_returnsIntent",
                mProfileId,
                Collections.EMPTY_MAP);
    }

    @Test
    public void testCreateRequestInteractAcrossProfilesIntent_canNotRequestInteraction_throwsSecurityException()
            throws Exception {
        installAppAsUser(TEST_WITH_REQUESTED_PERMISSION_APK, mPrimaryUserId);

        runDeviceTestsAsUser(
                TEST_WITH_REQUESTED_PERMISSION_PACKAGE,
                TEST_WITH_REQUESTED_PERMISSION_CLASS,
                "testCreateRequestInteractAcrossProfilesIntent_canNotRequestInteraction_throwsSecurityException",
                mProfileId,
                Collections.EMPTY_MAP);
    }

    private void addManagedProfileAndInstallRequiredPackages(String testPackage) throws Exception {
        mProfileId = createManagedProfile(mPrimaryUserId);
        getDevice().startUser(mProfileId, /*waitFlag= */true);

        installAppAsUser(testPackage, mProfileId);

        installAppAsUser(MANAGED_PROFILE_APK, mProfileId);
        setProfileOwnerOrFail(MANAGED_PROFILE_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS,
                mProfileId);
    }

    private void addDefaultCrossProfilePackage(int userId, String packageName)
            throws Exception {
        runDeviceTestsAsUser(
                MANAGED_PROFILE_PKG,
                ".CrossProfileUtils",
                "testSetCrossProfilePackages",
                userId,
                createCrossProfilePackageParam(packageName));
    }

    private Map<String, String> createCrossProfilePackageParam(String packageName) {
        return Collections.singletonMap(PARAM_CROSS_PROFILE_PACKAGE, packageName);
    }
}
