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

package android.content.cts;

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsNotLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.SystemUserOnly;
import android.stats.devicepolicy.EventId;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IBuildReceiver;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

@SystemUserOnly
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull(reason = "instant applications cannot see any other application")
public class ContextCrossProfileHostTest extends BaseContextCrossProfileTest
        implements IBuildReceiver {

    private static final String TEST_WITH_PERMISSION_APK =
            "CtsContextCrossProfileApp.apk";
    private static final String TEST_WITH_PERMISSION_PKG =
            "com.android.cts.context";
    private static final String TEST_SERVICE_WITH_PERMISSION_APK =
            "CtsContextCrossProfileTestServiceApp.apk";
    private static final String TEST_SERVICE_WITH_PERMISSION_PKG =
            "com.android.cts.testService";

    public static final int USER_SYSTEM = 0;

    private IBuildInfo mCtsBuild;
    private File mApkFile;

    private int mParentUserId;
    private final Map<String, String> mTestArgs = new HashMap<>();

    @Before
    public void setUp() throws Exception {
        super.setUp();

        assumeTrue(mSupportsMultiUser);

        mParentUserId = getDevice().getCurrentUser();
        // Automotive uses non-system user as current user always
        if (!getDevice().hasFeature("feature:android.hardware.type.automotive")) {
            assertEquals(USER_SYSTEM, mParentUserId);
        }

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        mApkFile = buildHelper.getTestFile(TEST_WITH_PERMISSION_APK);

        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                mParentUserId, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(TEST_WITH_PERMISSION_PKG);
        getDevice().uninstallPackage(TEST_SERVICE_WITH_PERMISSION_PKG);
        mTestArgs.clear();
        super.tearDown();
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    @Test
    public void testBindServiceAsUser_differentUser_bindsServiceToCorrectUser()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_differentUser_bindsServiceToCorrectUser",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossUsersPermission_bindsService()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossUsersPermission_bindsService",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossUsersPermission_bindsService()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossUsersPermission_bindsService",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossProfilesPermission_bindsService()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossProfilesPermission_bindsService",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossProfilesPermission_throwsException()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossProfilesPermission_throwsException",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossProfilesAppOp_bindsService()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_sameProfileGroup_samePackage_withAcrossProfilesAppOp_bindsService",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossProfilesAppOp_throwsException()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_sameProfileGroup_differentPackage_withAcrossProfilesAppOp_throwsException",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_differentProfileGroup_withInteractAcrossUsersPermission_throwsException()
            throws Exception {
        int userInDifferentProfileGroup = createUser();
        getDevice().startUser(userInDifferentProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInDifferentProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInDifferentProfileGroup, /* extraArgs= */"-t",
                /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInDifferentProfileGroup, /* extraArgs= */"-t",
                /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_differentProfileGroup_withInteractAcrossUsersPermission_throwsException",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_differentProfileGroup_withInteractAcrossProfilesAppOp_throwsException()
            throws Exception {
        int userInDifferentProfileGroup = createUser();
        getDevice().startUser(userInDifferentProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInDifferentProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInDifferentProfileGroup, /* extraArgs= */"-t",
                /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInDifferentProfileGroup, /* extraArgs= */"-t",
                /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_differentProfileGroup_withInteractAcrossProfilesAppOp_throwsException",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_differentProfileGroup_withInteractAcrossProfilesPermission_throwsException()
            throws Exception {
        int userInDifferentProfileGroup = createUser();
        getDevice().startUser(userInDifferentProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInDifferentProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInDifferentProfileGroup, /* extraArgs= */"-t",
                /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInDifferentProfileGroup, /* extraArgs= */"-t",
                /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_differentProfileGroup_withInteractAcrossProfilesPermission_throwsException",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_withNoPermissions_throwsException()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testBindServiceAsUser_sameProfileGroup_withNoPermissions_throwsException",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testBindServiceAsUser_sameProfileGroup_reportsMetric()
            throws Exception {
        assumeTrue(isStatsdEnabled(getDevice()));
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */ true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile,
                /* reinstall= */ true,
                /* grantPermissions= */ true,
                userInSameProfileGroup,
                /* extraArgs= */ "-t",
                /* extraArgs= */ "--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile,
                /* reinstall= */ true,
                /* grantPermissions= */ true,
                userInSameProfileGroup,
                /* extraArgs= */ "-t",
                /* extraArgs= */ "--force-queryable");

        assertMetricsLogged(getDevice(), () -> {
            runDeviceTests(
                    getDevice(),
                    TEST_WITH_PERMISSION_PKG,
                    ".ContextCrossProfileDeviceTest",
                    "testBindServiceAsUser_withInteractAcrossProfilePermission_noAsserts",
                    mParentUserId,
                    mTestArgs,
                    /* timeout= */ 60L,
                    TimeUnit.SECONDS);
        }, new DevicePolicyEventWrapper.Builder(EventId.BIND_CROSS_PROFILE_SERVICE_VALUE)
                .setStrings(TEST_WITH_PERMISSION_PKG)
                .build());
    }

    @Test
    public void testBindServiceAsUser_differentProfileGroup_doesNotReportMetric()
            throws Exception {
        assumeTrue(isStatsdEnabled(getDevice()));
        int userInDifferentProfileGroup = createUser();
        getDevice().startUser(userInDifferentProfileGroup, /* waitFlag= */ true);
        mTestArgs.put("testUser", Integer.toString(userInDifferentProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */ true, /* grantPermissions= */ true,
                userInDifferentProfileGroup, /* extraArgs= */ "-t",
                /* extraArgs= */ "--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile,
                /* reinstall= */ true,
                /* grantPermissions= */ true,
                userInDifferentProfileGroup,
                /* extraArgs= */ "-t",
                /* extraArgs= */ "--force-queryable");

        assertMetricsNotLogged(getDevice(), () -> {
            runDeviceTests(
                    getDevice(),
                    TEST_WITH_PERMISSION_PKG,
                    ".ContextCrossProfileDeviceTest",
                    "testBindServiceAsUser_withInteractAcrossUsersFullPermission_noAsserts",
                    mParentUserId,
                    mTestArgs,
                    /* timeout= */ 60L,
                    TimeUnit.SECONDS);
        }, new DevicePolicyEventWrapper.Builder(EventId.BIND_CROSS_PROFILE_SERVICE_VALUE)
                .setStrings(TEST_WITH_PERMISSION_PKG)
                .build());
    }

    @Test
    public void testBindServiceAsUser_sameUser_doesNotReportMetric()
            throws Exception {
        assumeTrue(isStatsdEnabled(getDevice()));

        mTestArgs.put("testUser", Integer.toString(mParentUserId));

        assertMetricsNotLogged(getDevice(), () -> {
            runDeviceTests(
                    getDevice(),
                    TEST_WITH_PERMISSION_PKG,
                    ".ContextCrossProfileDeviceTest",
                    "testBindServiceAsUser_withInteractAcrossProfilePermission_noAsserts",
                    mParentUserId,
                    mTestArgs,
                    /* timeout= */ 60L,
                    TimeUnit.SECONDS);
        }, new DevicePolicyEventWrapper.Builder(EventId.BIND_CROSS_PROFILE_SERVICE_VALUE)
                .setStrings(TEST_WITH_PERMISSION_PKG)
                .build());
    }

    @Test
    public void testCreateContextAsUser_sameProfileGroup_withInteractAcrossProfilesPermission_throwsException()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testCreateContextAsUser_sameProfileGroup_withInteractAcrossProfilesPermission_throwsException",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testCreateContextAsUser_sameProfileGroup_withInteractAcrossUsersPermission_createsContext()
            throws Exception {
        assumeTrue(supportsManagedUsers());
        int userInSameProfileGroup = createProfile(mParentUserId);
        getDevice().startUser(userInSameProfileGroup, /* waitFlag= */true);
        mTestArgs.put("testUser", Integer.toString(userInSameProfileGroup));
        getDevice().installPackageForUser(
                mApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testServiceApkFile = buildHelper.getTestFile(TEST_SERVICE_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(
                testServiceApkFile, /* reinstall= */true, /* grantPermissions= */true,
                userInSameProfileGroup, /* extraArgs= */"-t", /* extraArgs= */"--force-queryable");

        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".ContextCrossProfileDeviceTest",
                "testCreateContextAsUser_sameProfileGroup_withInteractAcrossUsersPermission_createsContext",
                mParentUserId,
                mTestArgs,
                /* timeout= */60L,
                TimeUnit.SECONDS);
    }

    boolean supportsManagedUsers() {
        try {
            return getDevice().hasFeature("feature:android.software.managed_users");
        } catch (DeviceNotAvailableException e) {
            return false;
        }
    }
}
