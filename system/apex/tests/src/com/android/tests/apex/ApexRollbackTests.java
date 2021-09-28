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

package com.android.tests.apex;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;

import com.android.tests.util.ModuleTestUtils;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.ApexInfo;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.time.Duration;
import java.util.Set;

/**
 * Test for automatic recovery of apex update that causes boot loop.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class ApexRollbackTests extends BaseHostJUnit4Test {
    private final ModuleTestUtils mUtils = new ModuleTestUtils(this);

    @Before
    public void setUp() throws Exception {
        mUtils.abandonActiveStagedSession();
        mUtils.uninstallShimApexIfNecessary();
        resetProperties();
    }

    /**
     * Uninstalls any version greater than 1 of shim apex and reboots the device if necessary
     * to complete the uninstall.
     */
    @After
    public void tearDown() throws Exception {
        mUtils.abandonActiveStagedSession();
        mUtils.uninstallShimApexIfNecessary();
        resetProperties();
    }

    private void resetProperties() throws Exception {
        resetProperty("persist.debug.trigger_watchdog.apex");
        resetProperty("persist.debug.trigger_updatable_crashing_for_testing");
        resetProperty("persist.debug.trigger_reboot_after_activation");
        resetProperty("persist.debug.trigger_reboot_twice_after_activation");
    }

    private void resetProperty(String propertyName) throws Exception {
        assertWithMessage("Failed to reset value of property %s", propertyName).that(
                getDevice().setProperty(propertyName, "")).isTrue();
    }

    /**
     * Test for automatic recovery of apex update that causes boot loop.
     */
    @Test
    public void testAutomaticBootLoopRecovery() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());

        File apexFile = mUtils.getTestFile("com.android.apex.cts.shim.v2.apex");

        // To simulate an apex update that causes a boot loop, we install a
        // trigger_watchdog.rc file that arranges for a trigger_watchdog.sh
        // script to be run at boot. The trigger_watchdog.sh script checks if
        // the apex version specified in the property
        // persist.debug.trigger_watchdog.apex is installed. If so,
        // trigger_watchdog.sh repeatedly kills the system server causing a
        // boot loop.
        ITestDevice device = getDevice();
        assertThat(device.setProperty("persist.debug.trigger_watchdog.apex",
                "com.android.apex.cts.shim@2")).isTrue();
        String error = device.installPackage(apexFile, false, "--wait");
        assertThat(error).isNull();

        String sessionIdToCheck = device.executeShellCommand("pm get-stagedsessions --only-ready "
                + "--only-parent --only-sessionid").trim();
        assertThat(sessionIdToCheck).isNotEmpty();

        // After we reboot the device, we expect the device to go into boot
        // loop from trigger_watchdog.sh. Native watchdog should detect and
        // report the boot loop, causing apexd to roll back to the previous
        // version of the apex and force reboot. When the device comes up
        // after the forced reboot, trigger_watchdog.sh will see the different
        // version of the apex and refrain from forcing a boot loop, so the
        // device will be recovered.
        device.reboot();

        ApexInfo ctsShimV1 = new ApexInfo("com.android.apex.cts.shim", 1L);
        ApexInfo ctsShimV2 = new ApexInfo("com.android.apex.cts.shim", 2L);
        Set<ApexInfo> activatedApexes = device.getActiveApexes();
        assertThat(activatedApexes).contains(ctsShimV1);
        assertThat(activatedApexes).doesNotContain(ctsShimV2);

        // Assert that a session has failed with the expected reason
        String sessionInfo = device.executeShellCommand("cmd -w apexservice getStagedSessionInfo "
                    + sessionIdToCheck);
        assertThat(sessionInfo).contains("revertReason: zygote");
    }

    /**
     * Test to verify that a device that does not support checkpointing will not revert a session
     * if it reboots during boot.
     */
    @Test
    public void testSessionNotRevertedWithCheckpointingDisabled() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        assumeFalse("Fs checkpointing is enabled", supportsFsCheckpointing());

        File apexFile = mUtils.getTestFile("com.android.apex.cts.shim.v2.apex");

        ITestDevice device = getDevice();
        assertThat(device.setProperty("persist.debug.trigger_reboot_after_activation",
                "com.android.apex.cts.shim@2.apex")).isTrue();
        assertThat(device.setProperty("debug.trigger_reboot_once_after_activation",
                "1")).isTrue();

        String error = device.installPackage(apexFile, false, "--wait");
        assertThat(error).isNull();

        String sessionIdToCheck = device.executeShellCommand("pm get-stagedsessions --only-ready "
                + "--only-parent --only-sessionid").trim();
        assertThat(sessionIdToCheck).isNotEmpty();

        // After we reboot the device, the apexd session should be activated as normal. After this,
        // trigger_reboot.sh will reboot the device before the system server boots.
        device.reboot();

        ApexInfo ctsShimV1 = new ApexInfo("com.android.apex.cts.shim", 1L);
        ApexInfo ctsShimV2 = new ApexInfo("com.android.apex.cts.shim", 2L);
        String stagedSessionsInfo = device.executeShellCommand("pm get-stagedsessions");
        for (String line: stagedSessionsInfo.split("[\\r\\n]+")) {
            if (line.contains(sessionIdToCheck)) {
                assertThat(line).contains("isApplied = true");
            }
        }

        Set<ApexInfo> activatedApexes = device.getActiveApexes();
        assertThat(activatedApexes).contains(ctsShimV2);
        assertThat(activatedApexes).doesNotContain(ctsShimV1);
    }

    /**
     * Test to verify that rebooting twice when a session is activated will cause the session to
     * be reverted due to filesystem checkpointing.
     */
    @Test
    public void testCheckpointingRevertsSession() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        assumeTrue("Device doesn't support fs checkpointing", supportsFsCheckpointing());

        File apexFile = mUtils.getTestFile("com.android.apex.cts.shim.v2.apex");

        ITestDevice device = getDevice();
        assertThat(device.setProperty("persist.debug.trigger_reboot_after_activation",
                "com.android.apex.cts.shim@2.apex")).isTrue();
        assertThat(device.setProperty("persist.debug.trigger_reboot_twice_after_activation",
                "1")).isTrue();
        String error = device.installPackage(apexFile, false, "--wait");
        assertThat(error).isNull();

        String sessionIdToCheck = device.executeShellCommand("pm get-stagedsessions --only-ready "
                + "--only-parent --only-sessionid").trim();
        assertThat(sessionIdToCheck).isNotEmpty();

        // After we reboot the device, the apexd session should be activated as normal. After this,
        // trigger_reboot.sh will reboot the device before the system server boots. Checkpointing
        // will kick in, and at the next boot any non-finalized sessions will be reverted.
        device.reboot();

        ApexInfo ctsShimV1 = new ApexInfo("com.android.apex.cts.shim", 1L);
        ApexInfo ctsShimV2 = new ApexInfo("com.android.apex.cts.shim", 2L);
        String stagedSessionsInfo = device.executeShellCommand("pm get-stagedsessions");
        for (String line: stagedSessionsInfo.split("[\\r\\n]+")) {
            if (line.contains(sessionIdToCheck)) {
                assertThat(line).contains("isFailed = true");
            }
        }

        Set<ApexInfo> activatedApexes = device.getActiveApexes();
        assertThat(activatedApexes).contains(ctsShimV1);
        assertThat(activatedApexes).doesNotContain(ctsShimV2);
    }

    /**
     * Test to verify that rebooting once upon apex activation does not cause checkpointing to kick
     * in and revert a session, since the checkpointing retry count should be 2.
     */
    @Test
    public void testRebootingOnceDoesNotRevertSession() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        assumeTrue("Device doesn't support fs checkpointing", supportsFsCheckpointing());

        File apexFile = mUtils.getTestFile("com.android.apex.cts.shim.v2.apex");

        ITestDevice device = getDevice();
        assertThat(device.setProperty("persist.debug.trigger_reboot_after_activation",
                "com.android.apex.cts.shim@2.apex")).isTrue();
        assertThat(device.setProperty("debug.trigger_reboot_once_after_activation",
                "1")).isTrue();
        String error = device.installPackage(apexFile, false, "--wait");
        assertThat(error).isNull();

        String sessionIdToCheck = device.executeShellCommand("pm get-stagedsessions --only-ready "
                + "--only-parent --only-sessionid").trim();
        assertThat(sessionIdToCheck).isNotEmpty();

        // After we reboot the device, the apexd session should be activated as normal. After this,
        // trigger_reboot.sh will reboot the device before the system server boots. Checkpointing
        // will kick in, and at the next boot any non-finalized sessions will be reverted.
        device.reboot();

        ApexInfo ctsShimV1 = new ApexInfo("com.android.apex.cts.shim", 1L);
        ApexInfo ctsShimV2 = new ApexInfo("com.android.apex.cts.shim", 2L);
        String stagedSessionsInfo = device.executeShellCommand("pm get-stagedsessions");
        for (String line: stagedSessionsInfo.split("[\\r\\n]+")) {
            if (line.contains(sessionIdToCheck)) {
                assertThat(line).contains("isApplied = true");
            }
        }

        Set<ApexInfo> activatedApexes = device.getActiveApexes();
        assertThat(activatedApexes).contains(ctsShimV2);
        assertThat(activatedApexes).doesNotContain(ctsShimV1);
    }

    // TODO(ioffe): check that we recover from the boot loop in case of userspace reboot.

    /**
     * Test to verify that apexd won't boot loop a device in case {@code sys.init
     * .updatable_crashing} is {@code true} and there is no apex session to revert.
     */
    @Test
    public void testApexdDoesNotBootLoopDeviceIfThereIsNothingToRevert() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        // On next boot trigger setprop sys.init.updatable_crashing 1, which will trigger a
        // revert mechanism in apexd. Since there is nothing to revert, this should be a no-op
        // and device will boot successfully.
        assertThat(getDevice().setProperty("persist.debug.trigger_updatable_crashing_for_testing",
                "1")).isTrue();
        getDevice().reboot();
        assertWithMessage("Device didn't boot in 1 minute").that(
                getDevice().waitForBootComplete(Duration.ofMinutes(1).toMillis())).isTrue();
        // Verify that property was set to true.
        assertThat(getDevice().getBooleanProperty("sys.init.updatable_crashing", false)).isTrue();
    }

    /**
     * Test to verify that if a hard reboot is triggered during userspace reboot boot
     * sequence, an apex update will not be reverted.
     */
    @Test
    public void testFailingUserspaceReboot_doesNotRevertUpdate() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        assumeTrue("Device doesn't support userspace reboot",
                getDevice().getBooleanProperty("init.userspace_reboot.is_supported", false));
        assumeTrue("Device doesn't support fs checkpointing", supportsFsCheckpointing());

        File apexFile = mUtils.getTestFile("com.android.apex.cts.shim.v2.apex");
        // Simulate failure in userspace reboot by triggering a full reboot in the middle of the
        // boot sequence.
        assertThat(getDevice().setProperty("test.apex_revert_test_force_reboot", "1")).isTrue();
        String error = getDevice().installPackage(apexFile, false, "--wait");
        assertWithMessage("Failed to stage com.android.apex.cts.shim.v2.apex : %s", error).that(
                error).isNull();
        // After we reboot the device, apexd will apply the update
        getDevice().rebootUserspace();
        // Verify that hard reboot happened.
        assertThat(getDevice().getIntProperty("sys.init.userspace_reboot.last_finished",
                -1)).isEqualTo(-1);
        Set<ApexInfo> activatedApexes = getDevice().getActiveApexes();
        assertThat(activatedApexes).doesNotContain(new ApexInfo("com.android.apex.cts.shim", 1L));
        assertThat(activatedApexes).contains(new ApexInfo("com.android.apex.cts.shim", 2L));
    }

    /**
     * Test to verify that if a hard reboot is triggered before executing init executes {@code
     * /system/bin/vdc checkpoint markBootAttempt} of userspace reboot boot sequence, apex update
     * still will be installed.
     */
    @Test
    public void testUserspaceRebootFailedShutdownSequence_doesNotRevertUpdate() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        assumeTrue("Device doesn't support userspace reboot",
                getDevice().getBooleanProperty("init.userspace_reboot.is_supported", false));
        assumeTrue("Device doesn't support fs checkpointing", supportsFsCheckpointing());

        File apexFile = mUtils.getTestFile("com.android.apex.cts.shim.v2.apex");
        // Simulate failure in userspace reboot by triggering a full reboot in the middle of the
        // boot sequence.
        assertThat(getDevice().setProperty("test.apex_userspace_reboot_simulate_shutdown_failed",
                "1")).isTrue();
        String error = getDevice().installPackage(apexFile, false, "--wait");
        assertWithMessage("Failed to stage com.android.apex.cts.shim.v2.apex : %s", error).that(
                error).isNull();
        // After the userspace reboot started, we simulate it's failure by rebooting device during
        // on userspace-reboot-requested action. Since boot attempt hasn't been marked yet, next
        // boot will apply the update.
        assertThat(getDevice().getIntProperty("test.apex_userspace_reboot_simulate_shutdown_failed",
                0)).isEqualTo(1);
        getDevice().rebootUserspace();
        // Verify that hard reboot happened.
        assertThat(getDevice().getIntProperty("sys.init.userspace_reboot.last_finished",
                -1)).isEqualTo(-1);
        Set<ApexInfo> activatedApexes = getDevice().getActiveApexes();
        assertThat(activatedApexes).contains(new ApexInfo("com.android.apex.cts.shim", 2L));
    }

    /**
     * Test to verify that if a hard reboot is triggered around the time of
     * executing {@code /system/bin/vdc checkpoint markBootAttempt} of userspace reboot boot
     * sequence, apex update will still be installed.
     */
    @Test
    public void testUserspaceRebootFailedRemount_revertsUpdate() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        assumeTrue("Device doesn't support userspace reboot",
                getDevice().getBooleanProperty("init.userspace_reboot.is_supported", false));
        assumeTrue("Device doesn't support fs checkpointing", supportsFsCheckpointing());

        File apexFile = mUtils.getTestFile("com.android.apex.cts.shim.v2.apex");
        // Simulate failure in userspace reboot by triggering a full reboot in the middle of the
        // boot sequence.
        assertThat(getDevice().setProperty("test.apex_userspace_reboot_simulate_remount_failed",
                "1")).isTrue();
        String error = getDevice().installPackage(apexFile, false, "--wait");
        assertWithMessage("Failed to stage com.android.apex.cts.shim.v2.apex : %s", error).that(
                error).isNull();
        // After we reboot the device, apexd will apply the update
        getDevice().rebootUserspace();
        // Verify that hard reboot happened.
        assertThat(getDevice().getIntProperty("sys.init.userspace_reboot.last_finished",
                -1)).isEqualTo(-1);
        Set<ApexInfo> activatedApexes = getDevice().getActiveApexes();
        assertThat(activatedApexes).doesNotContain(new ApexInfo("com.android.apex.cts.shim", 1L));
        assertThat(activatedApexes).contains(new ApexInfo("com.android.apex.cts.shim", 2L));
    }

    /**
     * Test to verify that boot cleanup logic in apexd is triggered when there is a crash looping
     * process, but there is nothing to revert.
     */
    @Test
    public void testBootCompletedCleanupHappensEvenWhenThereIsCrashingProcess() throws Exception {
        assumeTrue("Device does not support updating APEX", mUtils.isApexUpdateSupported());
        assumeTrue("Device requires root", getDevice().isAdbRoot());
        try {
            // On next boot trigger setprop sys.init.updatable_crashing 1, which will trigger a
            // revert mechanism in apexd. Since there is nothing to revert, this should be a no-op
            // and device will boot successfully.
            getDevice().setProperty("persist.debug.trigger_updatable_crashing_for_testing", "1");
            assertThat(getDevice().pushFile(mUtils.getTestFile("apex.apexd_test_v2.apex"),
                    "/data/apex/active/apexd_test_v2.apex")).isTrue();
            getDevice().reboot();
            assertWithMessage("Timed out waiting for device to boot").that(
                    getDevice().waitForBootComplete(Duration.ofMinutes(2).toMillis())).isTrue();
            // Verify that property was set to true.
            assertThat(
                    getDevice().getBooleanProperty("sys.init.updatable_crashing", false)).isTrue();
            final Set<ITestDevice.ApexInfo> activeApexes = getDevice().getActiveApexes();
            ITestDevice.ApexInfo testApex = new ITestDevice.ApexInfo(
                    "com.android.apex.cts.shim", 2L);
            assertThat(activeApexes).doesNotContain(testApex);
            mUtils.waitForFileDeleted("/data/apex/active/apexd_test_v2.apex",
                    Duration.ofMinutes(3));
        } finally {
            getDevice().executeShellV2Command("rm /data/apex/active/apexd_test_v2.apex");
        }
    }


    private boolean supportsFsCheckpointing() throws Exception {
        CommandResult result = getDevice().executeShellV2Command("sm supports-checkpoint");
        assertWithMessage("Failed to check if fs checkpointing is supported : %s",
                result.getStderr()).that(result.getStatus()).isEqualTo(CommandStatus.SUCCESS);
        return "true".equals(result.getStdout().trim());
    }
}
