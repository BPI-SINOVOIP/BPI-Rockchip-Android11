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

package com.android.tests.stagedinstall.host;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.endsWith;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeThat;
import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.LargeTest;

import com.android.ddmlib.Log;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class StagedInstallTest extends BaseHostJUnit4Test {

    private static final String TAG = "StagedInstallTest";

    private static final String SHIM_APEX_PACKAGE_NAME = "com.android.apex.cts.shim";

    private static final String PACKAGE_NAME = "com.android.tests.stagedinstall";

    private static final String BROADCAST_RECEIVER_COMPONENT = PACKAGE_NAME + "/"
            + PACKAGE_NAME + ".LauncherActivity";

    private String mDefaultLauncher = null;

    @Rule
    public final FailedTestLogHook mFailedTestLogHook = new FailedTestLogHook(this);

    /**
     * Runs the given phase of a test by calling into the device.
     * Throws an exception if the test phase fails.
     * <p>
     * For example, <code>runPhase("testInstallStagedApkCommit");</code>
     */
    private void runPhase(String phase) throws Exception {
        assertThat(runDeviceTests(PACKAGE_NAME,
                "com.android.tests.stagedinstall.StagedInstallTest",
                phase)).isTrue();
    }

    // We do not assert the success of cleanup phase since it might fail due to flaky reasons.
    private void cleanUp() throws Exception {
        try {
            runDeviceTests(PACKAGE_NAME,
                    "com.android.tests.stagedinstall.StagedInstallTest",
                    "cleanUp");
        } catch (AssertionError e) {
            Log.e(TAG, e);
        }
    }

    @Before
    public void setUp() throws Exception {
        cleanUp();
        uninstallShimApexIfNecessary();
        storeDefaultLauncher();
    }

    @After
    public void tearDown() throws Exception {
        cleanUp();
        uninstallShimApexIfNecessary();
        setDefaultLauncher(mDefaultLauncher);
    }

    /**
     * Tests for staged install involving only one apk.
     */
    @Test
    @LargeTest
    public void testInstallStagedApk() throws Exception {
        assumeSystemUser();

        setDefaultLauncher(BROADCAST_RECEIVER_COMPONENT);
        runPhase("testInstallStagedApk_Commit");
        getDevice().reboot();
        runPhase("testInstallStagedApk_VerifyPostReboot");
        runPhase("testInstallStagedApk_AbandonSessionIsNoop");
    }

    @Test
    public void testFailInstallIfNoPermission() throws Exception {
        runPhase("testFailInstallIfNoPermission");
    }

    @Test
    @LargeTest
    public void testAbandonStagedApkBeforeReboot() throws Exception {
        runPhase("testAbandonStagedApkBeforeReboot_CommitAndAbandon");
        getDevice().reboot();
        runPhase("testAbandonStagedApkBeforeReboot_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testAbandonStagedApkBeforeReady() throws Exception {
        runPhase("testAbandonStagedApkBeforeReady_CommitAndAbandon");
        getDevice().reboot();
        runPhase("testAbandonStagedApkBeforeReady_VerifyPostReboot");
    }

    @Test
    public void testStageAnotherSessionImmediatelyAfterAbandon() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());
        runPhase("testStageAnotherSessionImmediatelyAfterAbandon");
    }

    @Test
    public void testStageAnotherSessionImmediatelyAfterAbandonMultiPackage() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());
        runPhase("testStageAnotherSessionImmediatelyAfterAbandonMultiPackage");
    }

    @Test
    public void testNoSessionUpdatedBroadcastSentForStagedSessionAbandon() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());
        runPhase("testNoSessionUpdatedBroadcastSentForStagedSessionAbandon");
    }

    @Test
    @LargeTest
    public void testInstallMultipleStagedApks() throws Exception {
        assumeSystemUser();

        setDefaultLauncher(BROADCAST_RECEIVER_COMPONENT);
        runPhase("testInstallMultipleStagedApks_Commit");
        getDevice().reboot();
        runPhase("testInstallMultipleStagedApks_VerifyPostReboot");
    }

    private void assumeSystemUser() throws Exception {
        String systemUser = "0";
        assumeThat("Current user is not system user",
                getDevice().executeShellCommand("am get-current-user").trim(), equalTo(systemUser));
    }

    @Test
    public void testGetActiveStagedSessions() throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testGetActiveStagedSessions");
    }

    /**
     * Verifies that active staged session fulfils conditions stated at
     * {@link PackageInstaller.SessionInfo#isStagedSessionActive}
     */
    @Test
    public void testIsStagedSessionActive() throws Exception {
        runPhase("testIsStagedSessionActive");
    }

    @Test
    public void testGetActiveStagedSessionsNoSessionActive() throws Exception {
        runPhase("testGetActiveStagedSessionsNoSessionActive");
    }

    @Test
    public void testGetActiveStagedSessions_MultiApkSession() throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testGetActiveStagedSessions_MultiApkSession");
    }

    @Test
    public void testStagedInstallDowngrade_DowngradeNotRequested_Fails() throws Exception {
        runPhase("testStagedInstallDowngrade_DowngradeNotRequested_Fails_Commit");
    }

    @Test
    @LargeTest
    public void testStagedInstallDowngrade_DowngradeRequested_DebugBuild() throws Exception {
        assumeThat(getDevice().getBuildFlavor(), not(endsWith("-user")));

        runPhase("testStagedInstallDowngrade_DowngradeRequested_Commit");
        getDevice().reboot();
        runPhase("testStagedInstallDowngrade_DowngradeRequested_DebugBuild_VerifyPostReboot");
    }

    @Test
    public void testStagedInstallDowngrade_DowngradeRequested_UserBuild() throws Exception {
        assumeThat(getDevice().getBuildFlavor(), endsWith("-user"));
        assumeFalse("Device is debuggable", isDebuggable());

        runPhase("testStagedInstallDowngrade_DowngradeRequested_Fails_Commit");
    }

    @Test
    public void testShimApexShouldPreInstalledIfUpdatingApexIsSupported() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        final ITestDevice.ApexInfo shimApex = getShimApex();
        assertThat(shimApex.versionCode).isEqualTo(1);
    }

    @Test
    @LargeTest
    public void testInstallStagedApex() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        setDefaultLauncher(BROADCAST_RECEIVER_COMPONENT);
        runPhase("testInstallStagedApex_Commit");
        getDevice().reboot();
        runPhase("testInstallStagedApex_VerifyPostReboot");
    }

    @Test
    // Don't mark as @LargeTest since we want at least one test to install apex during pre-submit.
    public void testInstallStagedApexAndApk() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        setDefaultLauncher(BROADCAST_RECEIVER_COMPONENT);
        runPhase("testInstallStagedApexAndApk_Commit");
        getDevice().reboot();
        runPhase("testInstallStagedApexAndApk_VerifyPostReboot");
    }

    @Test
    public void testsFailsNonStagedApexInstall() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testsFailsNonStagedApexInstall");
    }

    @Test
    public void testInstallStagedNonPreInstalledApex_Fails() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testInstallStagedNonPreInstalledApex_Fails");
    }

    @Test
    public void testInstallStagedDifferentPackageNameWithInstalledApex_Fails() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testInstallStagedDifferentPackageNameWithInstalledApex_Fails");
    }

    @Test
    @LargeTest
    public void testStageApkWithSameNameAsApexShouldFail() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testStageApkWithSameNameAsApexShouldFail_Commit");
        getDevice().reboot();
        runPhase("testStageApkWithSameNameAsApexShouldFail_VerifyPostReboot");
    }

    @Test
    public void testNonStagedInstallApkWithSameNameAsApexShouldFail() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());
        runPhase("testNonStagedInstallApkWithSameNameAsApexShouldFail");
    }

    @Test
    @LargeTest
    public void testStagedInstallDowngradeApex_DowngradeNotRequested_Fails() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV3Apex();
        runPhase("testStagedInstallDowngradeApex_DowngradeNotRequested_Fails_Commit");
        getDevice().reboot();
        runPhase("testStagedInstallDowngradeApex_DowngradeNotRequested_Fails_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testStagedInstallDowngradeApex_DowngradeRequested_DebugBuild() throws Exception {
        assumeThat(getDevice().getBuildFlavor(), not(endsWith("-user")));
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV3Apex();
        runPhase("testStagedInstallDowngradeApex_DowngradeRequested_DebugBuild_Commit");
        getDevice().reboot();
        runPhase("testStagedInstallDowngradeApex_DowngradeRequested_DebugBuild_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testStagedInstallDowngradeApex_DowngradeRequested_UserBuild_Fails()
            throws Exception {
        assumeThat(getDevice().getBuildFlavor(), endsWith("-user"));
        assumeFalse("Device is debuggable", isDebuggable());
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV3Apex();
        runPhase("testStagedInstallDowngradeApex_DowngradeRequested_UserBuild_Fails_Commit");
        getDevice().reboot();
        runPhase("testStagedInstallDowngradeApex_DowngradeRequested_UserBuild_Fails_"
                + "VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testStagedInstallDowngradeApexToSystemVersion_DebugBuild() throws Exception {
        assumeThat(getDevice().getBuildFlavor(), not(endsWith("-user")));
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV2Apex();
        runPhase("testStagedInstallDowngradeApexToSystemVersion_DebugBuild_Commit");
        getDevice().reboot();
        runPhase("testStagedInstallDowngradeApexToSystemVersion_DebugBuild_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testInstallStagedApex_SameGrade() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());
        installV3Apex();
        installV3Apex();
    }

    @Test
    @LargeTest
    public void testInstallStagedApex_SameGrade_NewOneWins() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV2Apex();

        runPhase("testInstallStagedApex_SameGrade_NewOneWins_Commit");
        getDevice().reboot();
        runPhase("testInstallStagedApex_SameGrade_NewOneWins_VerifyPostReboot");
    }

    @Test
    public void testInstallApex_DeviceDoesNotSupportApex_Fails() throws Exception {
        assumeFalse("Device supports updating APEX", isUpdatingApexSupported());

        runPhase("testInstallApex_DeviceDoesNotSupportApex_Fails");
    }

    private void installV2Apex()throws Exception {
        runPhase("testInstallV2Apex_Commit");
        getDevice().reboot();
        runPhase("testInstallV2Apex_VerifyPostReboot");
    }

    private void installV2SignedBobApex() throws Exception {
        runPhase("testInstallV2SignedBobApex_Commit");
        getDevice().reboot();
        runPhase("testInstallV2SignedBobApex_VerifyPostReboot");
    }

    private void installV3Apex()throws Exception {
        runPhase("testInstallV3Apex_Commit");
        getDevice().reboot();
        runPhase("testInstallV3Apex_VerifyPostReboot");
    }

    @Test
    public void testFailsInvalidApexInstall() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());
        runPhase("testFailsInvalidApexInstall_Commit");
        runPhase("testFailsInvalidApexInstall_AbandonSessionIsNoop");
    }

    @Test
    public void testStagedApkSessionCallbacks() throws Exception {
        runPhase("testStagedApkSessionCallbacks");
    }

    @Test
    @LargeTest
    public void testInstallStagedApexWithoutApexSuffix() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testInstallStagedApexWithoutApexSuffix_Commit");
        getDevice().reboot();
        runPhase("testInstallStagedApexWithoutApexSuffix_VerifyPostReboot");
    }

    @Test
    public void testRejectsApexDifferentCertificate() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testRejectsApexDifferentCertificate");
    }

    /**
     * Tests for staged install involving rotated keys.
     *
     * Here alice means the original default key that cts.shim.v1 package was signed with and
     * bob is the new key alice rotates to. Where ambiguous, we will refer keys as alice and bob
     * instead of "old key" and "new key".
     *
     * By default, rotated keys have rollback capability enabled for old keys. When we remove
     * rollback capability from a key, it is called "Distrusting Event" and the distrusted key can
     * not update the app anymore.
     */

    // Should not be able to update with a key that has not been rotated.
    @Test
    public void testUpdateWithDifferentKeyButNoRotation() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testUpdateWithDifferentKeyButNoRotation");
    }

    // Should be able to update with a key that has been rotated.
    @Test
    @LargeTest
    public void testUpdateWithDifferentKey() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testUpdateWithDifferentKey_Commit");
        getDevice().reboot();
        runPhase("testUpdateWithDifferentKey_VerifyPostReboot");
    }

    // Should not be able to update with a key that is no longer trusted (i.e, has no
    // rollback capability)
    @Test
    @LargeTest
    public void testUntrustedOldKeyIsRejected() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV2SignedBobApex();
        runPhase("testUntrustedOldKeyIsRejected");
    }

    // Should be able to update with an old key which is trusted
    @Test
    @LargeTest
    public void testTrustedOldKeyIsAccepted() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testTrustedOldKeyIsAccepted_Commit");
        getDevice().reboot();
        runPhase("testTrustedOldKeyIsAccepted_CommitPostReboot");
        getDevice().reboot();
        runPhase("testTrustedOldKeyIsAccepted_VerifyPostReboot");
    }

    // Should be able to update further with rotated key
    @Test
    @LargeTest
    public void testAfterRotationNewKeyCanUpdateFurther() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV2SignedBobApex();
        runPhase("testAfterRotationNewKeyCanUpdateFurther_CommitPostReboot");
        getDevice().reboot();
        runPhase("testAfterRotationNewKeyCanUpdateFurther_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testAfterRotationNewKeyCanUpdateFurtherWithoutLineage() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        installV2SignedBobApex();
        runPhase("testAfterRotationNewKeyCanUpdateFurtherWithoutLineage");
    }

    /**
     * Tests for staging and installing multiple staged sessions.
     */

    // Should fail to stage multiple sessions when check-point is not available
    @Test
    public void testFailStagingMultipleSessionsIfNoCheckPoint() throws Exception {
        assumeFalse(isCheckpointSupported());
        runPhase("testFailStagingMultipleSessionsIfNoCheckPoint");
    }

    @Test
    public void testFailOverlappingMultipleStagedInstall_BothSinglePackage_Apk() throws Exception {
        runPhase("testFailOverlappingMultipleStagedInstall_BothSinglePackage_Apk");
    }

    @Test
    public void testAllowNonOverlappingMultipleStagedInstall_MultiPackageSinglePackage_Apk()
            throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testAllowNonOverlappingMultipleStagedInstall_MultiPackageSinglePackage_Apk");
    }

    @Test
    public void testFailOverlappingMultipleStagedInstall_BothMultiPackage_Apk() throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testFailOverlappingMultipleStagedInstall_BothMultiPackage_Apk");
    }

    // Test for installing multiple staged sessions at the same time
    @Test
    @LargeTest
    public void testMultipleStagedInstall_ApkOnly() throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testMultipleStagedInstall_ApkOnly_Commit");
        getDevice().reboot();
        runPhase("testMultipleStagedInstall_ApkOnly_VerifyPostReboot");
    }

    // If apk installation fails in one staged session, then all staged session should fail.
    @Test
    @LargeTest
    public void testInstallMultipleStagedSession_PartialFail_ApkOnly() throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testInstallMultipleStagedSession_PartialFail_ApkOnly_Commit");
        getDevice().reboot();
        runPhase("testInstallMultipleStagedSession_PartialFail_ApkOnly_VerifyPostReboot");
    }

    // Failure reason of staged install should be be persisted for single sessions
    @Test
    @LargeTest
    public void testFailureReasonPersists_SingleSession() throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testFailureReasonPersists_SingleSession_Commit");
        getDevice().reboot();
        runPhase("testFailureReasonPersists_SingleSession_VerifyPostReboot");
    }

    // Failure reason of staged install should be be persisted for multi session staged install
    @Test
    @LargeTest
    public void testFailureReasonPersists_MultiSession() throws Exception {
        assumeTrue(isCheckpointSupported());
        runPhase("testFailureReasonPersists_MultipleSession_Commit");
        getDevice().reboot();
        runPhase("testFailureReasonPersists_MultipleSession_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testSamegradeSystemApex() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testSamegradeSystemApex_Commit");
        getDevice().reboot();
        runPhase("testSamegradeSystemApex_VerifyPostReboot");
    }

    @Test
    @LargeTest
    public void testInstallApkChangingFingerprint() throws Exception {
        assumeThat(getDevice().getBuildFlavor(), not(endsWith("-user")));

        try {
            getDevice().executeShellCommand("setprop persist.pm.mock-upgrade true");
            runPhase("testInstallApkChangingFingerprint");
            getDevice().reboot();
            runPhase("testInstallApkChangingFingerprint_VerifyAborted");
        } finally {
            getDevice().executeShellCommand("setprop persist.pm.mock-upgrade false");
        }
    }

    @Test
    @LargeTest
    public void testInstallStagedNoHashtreeApex() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testInstallStagedNoHashtreeApex_Commit");
        getDevice().reboot();
        runPhase("testInstallStagedNoHashtreeApex_VerifyPostReboot");
    }

    /**
     * Should fail to verify apex targeting older dev sdk
     */
    @Test
    public void testApexTargetingOldDevSdkFailsVerification() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testApexTargetingOldDevSdkFailsVerification");
    }

    /**
     * Apex should fail to install if apk-in-apex fails to get scanned
     */
    @Test
    @LargeTest
    public void testApexFailsToInstallIfApkInApexFailsToScan() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testApexFailsToInstallIfApkInApexFailsToScan_Commit");
        getDevice().reboot();
        runPhase("testApexFailsToInstallIfApkInApexFailsToScan_VerifyPostReboot");
    }

    @Test
    public void testCorruptedApexFailsVerification_b146895998() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testCorruptedApexFailsVerification_b146895998");
    }

    /**
     * Should fail to pass apk signature check
     */
    @Test
    public void testApexWithUnsignedApkFailsVerification() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testApexWithUnsignedApkFailsVerification");
    }

    /**
     * Should fail to verify apex with unsigned payload
     */
    @Test
    public void testApexWithUnsignedPayloadFailsVerification() throws Exception {
        assumeTrue("Device does not support updating APEX", isUpdatingApexSupported());

        runPhase("testApexWithUnsignedPayloadFailsVerification");
    }

    private boolean isUpdatingApexSupported() throws Exception {
        final String updatable = getDevice().getProperty("ro.apex.updatable");
        return updatable != null && updatable.equals("true");
    }

    /**
     * Uninstalls a shim apex only if it's latest version is installed on /data partition (i.e.
     * it has a version higher than {@code 1}).
     *
     * <p>This is purely to optimize tests run time. Since uninstalling an apex requires a reboot,
     * and only a small subset of tests successfully install an apex, this code avoids ~10
     * unnecessary reboots.
     */
    private void uninstallShimApexIfNecessary() throws Exception {
        if (!isUpdatingApexSupported()) {
            // Device doesn't support updating apex. Nothing to uninstall.
            return;
        }
        if (getShimApex().sourceDir.startsWith("/system")) {
            // System version is active, nothing to uninstall.
            return;
        }
        // Non system version is active, need to uninstall it and reboot the device.
        Log.i(TAG, "Uninstalling shim apex");
        final String errorMessage = getDevice().uninstallPackage(SHIM_APEX_PACKAGE_NAME);
        if (errorMessage != null) {
            Log.e(TAG, "Failed to uninstall " + SHIM_APEX_PACKAGE_NAME + " : " + errorMessage);
        } else {
            getDevice().reboot();
            final ITestDevice.ApexInfo shim = getShimApex();
            assertThat(shim.versionCode).isEqualTo(1L);
            assertThat(shim.sourceDir).startsWith("/system");
        }
    }

    private ITestDevice.ApexInfo getShimApex() throws DeviceNotAvailableException {
        return getDevice().getActiveApexes().stream().filter(
                apex -> apex.name.equals(SHIM_APEX_PACKAGE_NAME)).findAny().orElseThrow(
                () -> new AssertionError("Can't find " + SHIM_APEX_PACKAGE_NAME));
    }

    /**
     * Store the component name of the default launcher. This value will be used to reset the
     * default launcher to its correct component upon test completion.
     */
    private void storeDefaultLauncher() throws DeviceNotAvailableException {
        final String PREFIX = "Launcher: ComponentInfo{";
        final String POSTFIX = "}";
        for (String s : getDevice().executeShellCommand("cmd shortcut get-default-launcher")
                .split("\n")) {
            if (s.startsWith(PREFIX) && s.endsWith(POSTFIX)) {
                mDefaultLauncher = s.substring(PREFIX.length(), s.length() - POSTFIX.length());
            }
        }
    }

    /**
     * Set the default launcher to a given component.
     * If set to the broadcast receiver component of this test app, this will allow the test app to
     * receive SESSION_COMMITTED broadcasts.
     */
    private void setDefaultLauncher(String launcherComponent) throws DeviceNotAvailableException {
        assertThat(launcherComponent).isNotEmpty();
        getDevice().executeShellCommand("cmd package set-home-activity " + launcherComponent);
    }

    private static final class FailedTestLogHook extends TestWatcher {

        private final BaseHostJUnit4Test mInstance;
        private String mStagedSessionsBeforeTest;

        private FailedTestLogHook(BaseHostJUnit4Test instance) {
            this.mInstance = instance;
        }

        @Override
        protected void failed(Throwable e, Description description) {
            String stagedSessionsAfterTest = getStagedSessions();
            Log.e(TAG, "Test " + description + " failed.\n"
                    + "Staged sessions before test started:\n" + mStagedSessionsBeforeTest + "\n"
                    + "Staged sessions after test failed:\n" + stagedSessionsAfterTest);
        }

        @Override
        protected void starting(Description description) {
            mStagedSessionsBeforeTest = getStagedSessions();
        }

        private String getStagedSessions() {
            try {
                return mInstance.getDevice().executeShellV2Command("pm get-stagedsessions").getStdout();
            } catch (DeviceNotAvailableException e) {
                Log.e(TAG, e);
                return "Failed to get staged sessions";
            }
        }
    }

    private boolean isCheckpointSupported() throws Exception {
        try {
            runPhase("isCheckpointSupported");
            return true;
        } catch (AssertionError ignore) {
            return false;
        }
    }

    private boolean isDebuggable() throws Exception {
        return getDevice().getIntProperty("ro.debuggable", 0) == 1;
    }
}
