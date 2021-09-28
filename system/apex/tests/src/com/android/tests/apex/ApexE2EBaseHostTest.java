/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.RequiresDevice;

import com.android.tests.util.ModuleTestUtils;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.device.ITestDevice.ApexInfo;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;

import java.io.File;
import java.time.Duration;
import java.util.List;
import java.util.Set;

/**
 * Base test to check if Apex can be staged, activated and uninstalled successfully.
 */
public abstract class ApexE2EBaseHostTest extends BaseHostJUnit4Test {

    private static final String OPTION_APEX_FILE_NAME = "apex_file_name";

    private static final Duration BOOT_COMPLETE_TIMEOUT = Duration.ofMinutes(2);

    private static final String USERSPACE_REBOOT_SUPPORTED_PROP =
            "init.userspace_reboot.is_supported";

    /* protected so that derived tests can have access to test utils automatically */
    protected final ModuleTestUtils mUtils = new ModuleTestUtils(this);

    @Option(name = OPTION_APEX_FILE_NAME,
            description = "The file name of the apex module.",
            importance = Importance.IF_UNSET,
            mandatory = true
    )
    protected String mApexFileName = null;

    @Before
    public void setUp() throws Exception {
        assumeTrue("Updating APEX is not supported", mUtils.isApexUpdateSupported());
        mUtils.abandonActiveStagedSession();
        uninstallAllApexes();
    }

    @After
    public void tearDown() throws Exception {
        assumeTrue("Updating APEX is not supported", mUtils.isApexUpdateSupported());
        mUtils.abandonActiveStagedSession();
        uninstallAllApexes();
    }

    protected List<String> getAllApexFilenames() {
        return List.of(mApexFileName);
    }

    @Test
    public final void testStageActivateUninstallApexPackage()  throws Exception {
        stageActivateUninstallApexPackage(false/*userspaceReboot*/);
    }

    @Test
    @RequiresDevice // TODO(b/147726967): Remove when Userspace reboot works on cuttlefish
    public final void testStageActivateUninstallApexPackageWithUserspaceReboot()  throws Exception {
        assumeTrue("Userspace reboot not supported on the device",
                getDevice().getBooleanProperty(USERSPACE_REBOOT_SUPPORTED_PROP, false));
        stageActivateUninstallApexPackage(true/*userspaceReboot*/);
    }

    private void stageActivateUninstallApexPackage(boolean userspaceReboot)  throws Exception {
        ApexInfo apex = installApex(mApexFileName);

        reboot(userspaceReboot); // for install to take affect
        Set<ApexInfo> activatedApexes = getDevice().getActiveApexes();
        assertWithMessage("Failed to activate %s", apex).that(activatedApexes).contains(apex);

        additionalCheck();
    }

    private void uninstallAllApexes() throws Exception {
        for (String filename : getAllApexFilenames()) {
            ApexInfo apex = mUtils.getApexInfo(mUtils.getTestFile(filename));
            uninstallApex(apex.name);
        }
    }

    protected final ApexInfo installApex(String filename) throws Exception {
        File testAppFile = mUtils.getTestFile(filename);

        String installResult = getDevice().installPackage(testAppFile, false, "--wait");
        assertWithMessage("failed to install test app %s. Reason: %s", filename, installResult)
                .that(installResult).isNull();

        ApexInfo testApexInfo = mUtils.getApexInfo(testAppFile);
        Assert.assertNotNull(testApexInfo);
        return testApexInfo;
    }

    protected final void installApexes(String... filenames) throws Exception {
        // We don't use the installApex method from the super class here, because that won't install
        // the two apexes into the same session.
        String[] args = new String[filenames.length + 1];
        args[0] = "install-multi-package";
        for (int i = 0; i < filenames.length; i++) {
            args[i + 1] = mUtils.getTestFile(filenames[i]).getAbsolutePath();
        }
        String stdout = getDevice().executeAdbCommand(args);
        assertThat(stdout).isNotNull();
    }

    protected final void reboot(boolean userspaceReboot) throws Exception {
        if (userspaceReboot) {
            assertThat(getDevice().setProperty("test.userspace_reboot.requested", "1")).isTrue();
            getDevice().rebootUserspace();
        } else {
            getDevice().reboot();
        }
        boolean success = getDevice().waitForBootComplete(BOOT_COMPLETE_TIMEOUT.toMillis());
        assertWithMessage("Device didn't boot in %s", BOOT_COMPLETE_TIMEOUT).that(success).isTrue();
        if (userspaceReboot) {
            // If userspace reboot fails and fallback to hard reboot is triggered then
            // test.userspace_reboot.requested won't be set.
            boolean res = getDevice().getBooleanProperty("test.userspace_reboot.requested", false);
            String message = "Userspace reboot failed, fallback to full reboot was triggered. ";
            message += "Boot reason: " + getDevice().getProperty("sys.boot.reason.last");
            assertWithMessage(message).that(res).isTrue();
        }
    }

    /**
     * Do some additional check, invoked by {@link #testStageActivateUninstallApexPackage()}.
     */
    public void additionalCheck() throws Exception {};

    protected final void uninstallApex(String apexName) throws Exception {
        String res = getDevice().uninstallPackage(apexName);
        if (res != null) {
            // Uninstall failed. Most likely this means that there were no apex installed. No need
            // to reboot.
            CLog.i("Uninstall of %s failed: %s, likely already on factory version", apexName, res);
        } else {
            // Uninstall succeeded. Need to reboot.
            getDevice().reboot(); // for the uninstall to take affect
        }
    }
}
