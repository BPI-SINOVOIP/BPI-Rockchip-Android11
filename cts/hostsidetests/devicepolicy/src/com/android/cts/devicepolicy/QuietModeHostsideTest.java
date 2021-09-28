package com.android.cts.devicepolicy;

import static com.google.common.truth.Truth.assertThat;

import android.platform.test.annotations.LargeTest;

import com.android.tradefed.device.DeviceNotAvailableException;

import org.junit.Test;

import java.io.FileNotFoundException;
import java.util.HashMap;
import java.util.Map;

/**
 * CTS to verify toggling quiet mode in work profile by using
 * {@link android.os.UserManager#requestQuietModeEnabled(boolean, android.os.UserHandle)}.
 */
public class QuietModeHostsideTest extends BaseDevicePolicyTest {
    private static final String TEST_PACKAGE = "com.android.cts.launchertests";
    private static final String TEST_CLASS = ".QuietModeTest";
    private static final String PARAM_TARGET_USER = "TARGET_USER";
    private static final String PARAM_ORIGINAL_DEFAULT_LAUNCHER = "ORIGINAL_DEFAULT_LAUNCHER";
    private static final String TEST_APK = "CtsLauncherAppsTests.apk";

    private static final String TEST_LAUNCHER_PACKAGE = "com.android.cts.launchertests.support";
    private static final String TEST_LAUNCHER_APK = "CtsLauncherAppsTestsSupport.apk";
    private static final String ENABLED_TEST_APK = "CtsCrossProfileEnabledApp.apk";
    private static final String USER_ENABLED_TEST_APK = "CtsCrossProfileUserEnabledApp.apk";
    private static final String ENABLED_NO_PERMS_TEST_APK = "CtsCrossProfileEnabledNoPermsApp.apk";
    private static final String QUIET_MODE_ENABLED_TEST_APK = "CtsModifyQuietModeEnabledApp.apk";
    private static final String NOT_ENABLED_TEST_APK = "CtsCrossProfileNotEnabledApp.apk";
    private static final String ENABLED_TEST_PACKAGE = "com.android.cts.crossprofileenabledapp";
    private static final String USER_ENABLED_TEST_PACKAGE =
            "com.android.cts.crossprofileuserenabledapp";
    private static final String ENABLED_NO_PERMS_TEST_PACKAGE =
            "com.android.cts.crossprofileenablednopermsapp";
    private static final String NOT_ENABLED_TEST_PACKAGE =
            "com.android.cts.crossprofilenotenabledapp";
    private static final String QUIET_MODE_ENABLED_TEST_PACKAGE =
            "com.android.cts.modifyquietmodeenabledapp";

    private int mProfileId;
    private String mOriginalLauncher;

    @Override
    public void setUp() throws Exception {
        super.setUp();

        mHasFeature = mHasFeature & hasDeviceFeature("android.software.managed_users");

        if (mHasFeature) {
            mOriginalLauncher = getDefaultLauncher();

            installAppAsUser(TEST_APK, mPrimaryUserId);
            installAppAsUser(TEST_LAUNCHER_APK, mPrimaryUserId);

            waitForBroadcastIdle();

            createAndStartManagedProfile();
            installAppAsUser(TEST_APK, mProfileId);

            waitForBroadcastIdle();
            wakeupAndDismissKeyguard();
        }
    }

    @Override
    public void tearDown() throws Exception {
        if (mHasFeature) {
            uninstallRequiredApps();
            getDevice().uninstallPackage(TEST_LAUNCHER_PACKAGE);
        }
        super.tearDown();
    }

    @LargeTest
    @Test
    public void testQuietMode_defaultForegroundLauncher() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        // Add a lockscreen to test the case that profile with unified challenge can still
        // be turned on without asking the user to enter the lockscreen password.
        changeUserCredential(/* newCredential= */ TEST_PASSWORD, /* oldCredential= */ null,
                mPrimaryUserId);
        try {
            runDeviceTestsAsUser(
                    TEST_PACKAGE,
                    TEST_CLASS,
                    "testTryEnableQuietMode_defaultForegroundLauncher",
                    mPrimaryUserId,
                    createParams(mProfileId));
        } finally {
            changeUserCredential(/* newCredential= */ null, /* oldCredential= */ TEST_PASSWORD,
                    mPrimaryUserId);
        }
    }

    @LargeTest
    @Test
    public void testQuietMode_notForegroundLauncher() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(
                TEST_PACKAGE,
                TEST_CLASS,
                "testTryEnableQuietMode_notForegroundLauncher",
                mPrimaryUserId,
                createParams(mProfileId));
    }

    @LargeTest
    @Test
    public void testQuietMode_notDefaultLauncher() throws Exception {
        if (!mHasFeature) {
            return;
        }
        runDeviceTestsAsUser(
                TEST_PACKAGE,
                TEST_CLASS,
                "testTryEnableQuietMode_notDefaultLauncher",
                mPrimaryUserId,
                createParams(mProfileId));
    }

    @LargeTest
    @Test
    public void testBroadcastManagedProfileAvailable_withoutCrossProfileAppsOp() throws Exception {
        checkBroadcastManagedProfileAvailable(/* withCrossProfileAppOps= */ false);
    }


    @LargeTest
    @Test
    public void testBroadcastManagedProfileAvailable_withCrossProfileAppsOp() throws Exception {
        checkBroadcastManagedProfileAvailable(/* withCrossProfileAppOps= */ true);
    }

    private void checkBroadcastManagedProfileAvailable(boolean withCrossProfileAppOps)
            throws Exception {
        if (!mHasFeature) {
            return;
        }
        installCrossProfileApps();
        if (withCrossProfileAppOps) {
            enableCrossProfileAppsOp();
        }
        clearLogcat();
        runDeviceTestsAsUser(
                TEST_PACKAGE,
                TEST_CLASS,
                "testTryEnableQuietMode",
                mPrimaryUserId,
                createParams(mProfileId));
        waitForBroadcastIdle();
        verifyBroadcastSent("android.intent.action.MANAGED_PROFILE_UNAVAILABLE",
                /* needPermissions= */ !withCrossProfileAppOps);

        clearLogcat();
        runDeviceTestsAsUser(
                TEST_PACKAGE,
                TEST_CLASS,
                "testTryDisableQuietMode",
                mPrimaryUserId,
                createParams(mProfileId));
        waitForBroadcastIdle();
        verifyBroadcastSent("android.intent.action.MANAGED_PROFILE_AVAILABLE",
                /* needPermissions= */ !withCrossProfileAppOps);

        clearLogcat();
        removeUser(mProfileId);
        waitForBroadcastIdle();
        verifyBroadcastSent("android.intent.action.MANAGED_PROFILE_REMOVED",
                /* needPermissions= */ false);
    }

    private void clearLogcat() throws DeviceNotAvailableException {
        getDevice().executeAdbCommand("logcat", "-c");
    }

    private void verifyBroadcastSent(String actionName, boolean needPermissions)
            throws DeviceNotAvailableException {
        final String result = getDevice().executeAdbCommand("logcat", "-d");
        assertThat(result).contains(
                buildReceivedBroadcastRegex(actionName, "CrossProfileEnabledAppReceiver"));
        assertThat(result).contains(
                buildReceivedBroadcastRegex(actionName, "CrossProfileUserEnabledAppReceiver"));
        String noPermsString = buildReceivedBroadcastRegex(actionName,
                "CrossProfileEnabledNoPermsAppReceiver");
        if (needPermissions) {
            assertThat(result).doesNotContain(noPermsString);
        } else {
            assertThat(result).contains(noPermsString);
        }
        assertThat(result).doesNotContain(
                buildReceivedBroadcastRegex(actionName,
                        "CrossProfileNotEnabledAppReceiver"));
        assertThat(result).contains(
                buildReceivedBroadcastRegex(actionName, "ModifyQuietModeEnabledAppReceiver"));
    }

    private String buildReceivedBroadcastRegex(String actionName, String className) {
        return String.format("%s: onReceive(%s)", className, actionName);
    }

    @LargeTest
    @Test
    public void testQuietMode_noCredentialRequest() throws Exception {
        if (!mHasFeature || !mHasSecureLockScreen) {
            return;
        }
        // Set a separate work challenge so turning on the profile requires entering the
        // separate challenge.
        changeUserCredential(/* newCredential= */ TEST_PASSWORD, /* oldCredential= */ null,
                mProfileId);
        runDeviceTestsAsUser(
                TEST_PACKAGE,
                TEST_CLASS,
                "testTryEnableQuietMode_noCredentialRequest",
                mPrimaryUserId,
                createParams(mProfileId));
    }

    private void createAndStartManagedProfile() throws Exception {
        mProfileId = createManagedProfile(mPrimaryUserId);
        switchUser(mPrimaryUserId);
        startUser(mProfileId);
    }

    private void uninstallRequiredApps()
            throws DeviceNotAvailableException {
        getDevice().uninstallPackage(TEST_PACKAGE);
        getDevice().uninstallPackage(ENABLED_TEST_PACKAGE);
        getDevice().uninstallPackage(USER_ENABLED_TEST_PACKAGE);
        getDevice().uninstallPackage(ENABLED_NO_PERMS_TEST_PACKAGE);
        getDevice().uninstallPackage(NOT_ENABLED_TEST_PACKAGE);
        getDevice().uninstallPackage(QUIET_MODE_ENABLED_TEST_PACKAGE);
    }

    private void installCrossProfileApps()
            throws FileNotFoundException, DeviceNotAvailableException {
        installCrossProfileApp(ENABLED_TEST_APK, /* grantPermissions= */ true);
        installCrossProfileApp(USER_ENABLED_TEST_APK, /* grantPermissions= */ true);
        installCrossProfileApp(NOT_ENABLED_TEST_APK, /* grantPermissions= */ true);
        installCrossProfileApp(ENABLED_NO_PERMS_TEST_APK, /* grantPermissions= */  false);
        installCrossProfileApp(QUIET_MODE_ENABLED_TEST_APK, /* grantPermissions= */  true);
    }

    private void enableCrossProfileAppsOp() throws DeviceNotAvailableException {
        enableCrossProfileAppsOp(ENABLED_NO_PERMS_TEST_PACKAGE, mPrimaryUserId);
    }

    private void installCrossProfileApp(String apkName, boolean grantPermissions)
            throws FileNotFoundException, DeviceNotAvailableException {
        installAppAsUser(apkName, grantPermissions, mPrimaryUserId);
        installAppAsUser(apkName, grantPermissions, mProfileId);
    }

    private void enableCrossProfileAppsOp(String packageName, int userId)
            throws DeviceNotAvailableException {
        getDevice().executeShellCommand(
                String.format("appops set --user %s %s android:interact_across_profiles 0",
                        userId, packageName));
        assertThat(getDevice().executeShellCommand(
                String.format("appops get --user %s %s android:interact_across_profiles",
                        userId, packageName))).contains("INTERACT_ACROSS_PROFILES: allow");
    }

    private Map<String, String> createParams(int targetUserId) throws Exception {
        Map<String, String> params = new HashMap<>();
        params.put(PARAM_TARGET_USER, Integer.toString(getUserSerialNumber(targetUserId)));
        params.put(PARAM_ORIGINAL_DEFAULT_LAUNCHER, mOriginalLauncher);
        return params;
    }
}
