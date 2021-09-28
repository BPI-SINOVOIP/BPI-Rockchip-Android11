package com.android.cts.devicepolicy;

import static android.stats.devicepolicy.EventId.CROSS_PROFILE_APPS_GET_TARGET_USER_PROFILES_VALUE;
import static android.stats.devicepolicy.EventId.CROSS_PROFILE_APPS_START_ACTIVITY_AS_USER_VALUE;
import static android.stats.devicepolicy.EventId.START_ACTIVITY_BY_INTENT_VALUE;

import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.assertMetricsLogged;
import static com.android.cts.devicepolicy.metrics.DevicePolicyEventLogVerifier.isStatsdEnabled;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;

import android.platform.test.annotations.FlakyTest;
import android.platform.test.annotations.LargeTest;

import com.android.cts.devicepolicy.metrics.DevicePolicyEventWrapper;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.util.StreamUtil;

import org.junit.Test;

import java.io.FileNotFoundException;
import java.util.Collections;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.annotation.Nullable;

/**
 * In the test, managed profile and secondary user are created. We then verify
 * {@link android.content.pm.crossprofile.CrossProfileApps} APIs in different directions, like
 * primary user to managed profile.
 *
 * Tests to verify
 * {@link android.content.pm.crossprofile.CrossProfileApps#canRequestInteractAcrossProfiles()},
 * {@link android.content.pm.crossprofile.CrossProfileApps#canInteractAcrossProfiles()}, and
 * {@link
 * android.content.pm.crossprofile.CrossProfileApps#createRequestInteractAcrossProfilesIntent()}
 * can be found in {@link CrossProfileAppsPermissionHostSideTest}.
 */
public class CrossProfileAppsHostSideTest extends BaseDevicePolicyTest {
    private static final String TEST_PACKAGE = "com.android.cts.crossprofileappstest";
    private static final String NON_TARGET_USER_TEST_CLASS = ".CrossProfileAppsNonTargetUserTest";
    private static final String TARGET_USER_TEST_CLASS = ".CrossProfileAppsTargetUserTest";
    private static final String START_ACTIVITY_TEST_CLASS = ".CrossProfileAppsStartActivityTest";
    private static final String PARAM_TARGET_USER = "TARGET_USER";
    private static final String EXTRA_TEST_APK = "CtsCrossProfileAppsTests.apk";
    private static final String SIMPLE_APP_APK ="CtsSimpleApp.apk";

    private int mProfileId;
    private int mSecondaryUserId;
    private boolean mHasManagedUserFeature;
    private boolean mCanTestMultiUser;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        // We need managed users to be supported in order to create a profile of the user owner.
        mHasManagedUserFeature = hasDeviceFeature("android.software.managed_users");
        installRequiredApps(mPrimaryUserId);

        if (mHasManagedUserFeature) {
            createAndStartManagedProfile();
            installRequiredApps(mProfileId);
        }
        waitForBroadcastIdle();
        if (canCreateAdditionalUsers(1)) {
            mSecondaryUserId = createUser();
            installRequiredApps(mSecondaryUserId);
            mCanTestMultiUser = true;
        }
        waitForBroadcastIdle();
    }

    private void installRequiredApps(int userId)
            throws FileNotFoundException, DeviceNotAvailableException {
        installAppAsUser(EXTRA_TEST_APK, userId);
        installAppAsUser(SIMPLE_APP_APK, userId);
    }

    @FlakyTest
    @LargeTest
    @Test
    public void testPrimaryUserToPrimaryUser() throws Exception {
        verifyCrossProfileAppsApi(mPrimaryUserId, mPrimaryUserId, NON_TARGET_USER_TEST_CLASS);
    }

    @FlakyTest
    @LargeTest
    @Test
    public void testPrimaryUserToManagedProfile() throws Exception {
        if (!mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(mPrimaryUserId, mProfileId, TARGET_USER_TEST_CLASS);
    }

    @LargeTest
    @Test
    public void testManagedProfileToPrimaryUser() throws Exception {
        if (!mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, TARGET_USER_TEST_CLASS);
    }

    @LargeTest
    @Test
    public void testStartActivityComponent() throws Exception {
        if (!mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartMainActivityByComponent");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartNonMainActivityByComponent");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCannotStartNotExportedActivityByComponent");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCannotStartActivityInOtherPackageByComponent");
    }

    @LargeTest
    @Test
    public void testStartActivityIntent() throws Exception {
        if (!mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCannotStartActivityWithImplicitIntent");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartMainActivityByIntent");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartMainActivityByIntent_withOptionsBundle");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartNonMainActivityByIntent");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartNotExportedActivityByIntent");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCannotStartActivityInOtherPackageByIntent");
    }

    @LargeTest
    @Test
    public void testStartActivityIntentPermissions() throws Exception {
        if (!mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCannotStartActivityByIntentWithNoPermissions");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartActivityByIntentWithInteractAcrossProfilesPermission");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartActivityByIntentWithInteractAcrossUsersPermission");
        verifyCrossProfileAppsApi(mProfileId, mPrimaryUserId, START_ACTIVITY_TEST_CLASS, "testCanStartActivityByIntentWithInteractAcrossUsersFullPermission");
    }

    @LargeTest
    @Test
    public void testStartActivityIntent_isLogged() throws Exception {
        if (!mHasManagedUserFeature) {
            return;
        }
        assertMetricsLogged(
                getDevice(),
                () -> verifyCrossProfileAppsApi(
                        mProfileId,
                        mPrimaryUserId,
                        START_ACTIVITY_TEST_CLASS,
                        "testStartActivityByIntent_noAsserts"),
                new DevicePolicyEventWrapper
                        .Builder(START_ACTIVITY_BY_INTENT_VALUE)
                        .setStrings(TEST_PACKAGE)
                        .setBoolean(true) // from work profile
                        .build());
    }

    @LargeTest
    @Test
    public void testStartActivityIntent_sameTaskByDefault() throws Exception {
        // TODO(b/171957840): replace with device-side test using an inter-process communication
        //  library.
        if (!mHasManagedUserFeature) {
            return;
        }
        getDevice().clearLogcat();
        verifyCrossProfileAppsApi(
                mProfileId,
                mPrimaryUserId,
                START_ACTIVITY_TEST_CLASS,
                "testStartActivityIntent_sameTaskByDefault");
        assertThat(findTaskId("CrossProfileSameTaskLauncherActivity"))
                .isEqualTo(findTaskId("NonMainActivity"));
    }

    private int findTaskId(String className) throws Exception {
        final Matcher matcher =
                Pattern.compile(className + "#taskId#" + "(.*?)" + "#").matcher(readLogcat());
        boolean isFound = matcher.find();
        if (!isFound) {
            fail("Task not found for " + className);
            return -1;
        }
        return Integer.parseInt(matcher.group(1));
    }

    @LargeTest
    @Test
    public void testStartActivityIntent_crossProfile_returnsResult() throws Exception {
        // TODO(b/171957840): replace with device-side test using an inter-process communication
        //  library.
        if (!mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(
                mProfileId,
                mPrimaryUserId,
                START_ACTIVITY_TEST_CLASS,
                "testStartActivityIntent_crossProfile_returnsResult");
    }

    @LargeTest
    @Test
    public void testPrimaryUserToSecondaryUser() throws Exception {
        if (!mCanTestMultiUser) {
            return;
        }
        verifyCrossProfileAppsApi(mPrimaryUserId, mSecondaryUserId, NON_TARGET_USER_TEST_CLASS);
    }

    @LargeTest
    @Test
    public void testSecondaryUserToManagedProfile() throws Exception {
        if (!mCanTestMultiUser || !mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(mSecondaryUserId, mProfileId, NON_TARGET_USER_TEST_CLASS);

    }

    @LargeTest
    @Test
    public void testManagedProfileToSecondaryUser() throws Exception {
        if (!mCanTestMultiUser || !mHasManagedUserFeature) {
            return;
        }
        verifyCrossProfileAppsApi(mProfileId, mSecondaryUserId, NON_TARGET_USER_TEST_CLASS);
    }

    @LargeTest
    @Test
    public void testStartMainActivity_logged() throws Exception {
        if (!mHasManagedUserFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(
                getDevice(),
                () -> {
                    runDeviceTest(
                            mProfileId,
                            mPrimaryUserId,
                            TARGET_USER_TEST_CLASS,
                            "testStartMainActivity_noAsserts");
                },
                new DevicePolicyEventWrapper
                        .Builder(CROSS_PROFILE_APPS_START_ACTIVITY_AS_USER_VALUE)
                        .setStrings(new String[] {"com.android.cts.crossprofileappstest"})
                        .build());
    }

    @LargeTest
    @Test
    public void testGetTargetUserProfiles_logged() throws Exception {
        if (!mHasManagedUserFeature || !isStatsdEnabled(getDevice())) {
            return;
        }
        assertMetricsLogged(
                getDevice(),
                () -> {
                    runDeviceTest(
                            mProfileId,
                            mPrimaryUserId,
                            TARGET_USER_TEST_CLASS,
                            "testGetTargetUserProfiles_noAsserts");
                },
                new DevicePolicyEventWrapper
                        .Builder(CROSS_PROFILE_APPS_GET_TARGET_USER_PROFILES_VALUE)
                        .setStrings(new String[] {"com.android.cts.crossprofileappstest"})
                        .build());
    }

    private void verifyCrossProfileAppsApi(int fromUserId, int targetUserId, String testClass)
            throws Exception {
        verifyCrossProfileAppsApi(fromUserId, targetUserId, testClass, /* testMethod= */ null);
    }

    private void verifyCrossProfileAppsApi(int fromUserId, int targetUserId, String testClass, String testMethod)
            throws Exception {
        runDeviceTest(fromUserId, targetUserId, testClass, testMethod);
    }

    private void runDeviceTest(
            int fromUserId, int targetUserId, String testClass, @Nullable String testMethod)
            throws Exception {
        runDeviceTestsAsUser(
                TEST_PACKAGE,
                testClass,
                testMethod,
                fromUserId,
                createTargetUserParam(targetUserId));
    }

    private void createAndStartManagedProfile() throws Exception {
        mProfileId = createManagedProfile(mPrimaryUserId);
        switchUser(mPrimaryUserId);
        startUser(mProfileId);
    }

    private Map<String, String> createTargetUserParam(int targetUserId) throws Exception {
        return Collections.singletonMap(PARAM_TARGET_USER,
                Integer.toString(getUserSerialNumber(targetUserId)));
    }

    private String readLogcat() throws Exception {
        getDevice().stopLogcat();
        final String logcat;
        try (InputStreamSource logcatStream = getDevice().getLogcat()) {
            logcat = StreamUtil.getStringFromSource(logcatStream);
        }
        getDevice().startLogcat();
        return logcat;
    }
}
