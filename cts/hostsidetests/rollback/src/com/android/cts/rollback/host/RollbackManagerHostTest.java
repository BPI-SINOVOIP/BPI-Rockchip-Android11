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

package com.android.cts.rollback.host;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.CoreMatchers.endsWith;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assume.assumeThat;
import static org.junit.Assume.assumeTrue;

import com.android.ddmlib.Log;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * CTS host tests for RollbackManager APIs.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class RollbackManagerHostTest extends BaseHostJUnit4Test {

    private static final String TAG = "RollbackManagerHostTest";

    private static final String SHIM_APEX_PACKAGE_NAME = "com.android.apex.cts.shim";

    /**
     * Runs the helper app test method on device.
     * Throws an exception if the test method fails.
     * <p>
     * For example, <code>run("testApkOnlyEnableRollback");</code>
     */
    private void run(String method) throws Exception {
        assertThat(runDeviceTests("com.android.cts.rollback.host.app",
                "com.android.cts.rollback.host.app.HostTestHelper",
                method)).isTrue();
    }

    /**
     * Runs the helper app test method on device targeted for
     * com.android.cts.rollback.host.app2.HostTestHelper.
     */
    private void run2(String method) throws Exception {
        assertThat(runDeviceTests("com.android.cts.rollback.host.app2",
                "com.android.cts.rollback.host.app2.HostTestHelper",
                method)).isTrue();
    }

    /**
     * Return {@code true} if and only if device supports updating apex.
     */
    private boolean isApexUpdateSupported() throws Exception {
        return "true".equals(getDevice().getProperty("ro.apex.updatable"));
    }

    /**
     * Uninstalls a shim apex only if it's latest version is installed on /data partition (i.e.
     * it has a version higher than {@code 1}).
     *
     * <p>This is purely to optimize tests run time, since uninstalling an apex requires a reboot.
     */
    private void uninstallShimApexIfNecessary() throws Exception {
        if (!isApexUpdateSupported()) {
            // Device doesn't support updating apex. Nothing to uninstall.
            return;
        }

        if (getShimApex().sourceDir.startsWith("/data")) {
            final String errorMessage = getDevice().uninstallPackage(SHIM_APEX_PACKAGE_NAME);
            if (errorMessage == null) {
                Log.i(TAG, "Uninstalling shim apex");
                getDevice().reboot();
            } else {
                Log.e(TAG, "Failed to uninstall shim APEX : " + errorMessage);
            }
        }
        assertThat(getShimApex().versionCode).isEqualTo(1L);
    }

    /**
     * Get {@link ITestDevice.ApexInfo} for the installed shim apex.
     */
    private ITestDevice.ApexInfo getShimApex() throws DeviceNotAvailableException {
        return getDevice().getActiveApexes().stream().filter(
                apex -> apex.name.equals(SHIM_APEX_PACKAGE_NAME)).findAny().orElseThrow(
                () -> new AssertionError("Can't find " + SHIM_APEX_PACKAGE_NAME));
    }

    private boolean isCheckpointSupported() throws Exception {
        try {
            run("isCheckpointSupported");
            return true;
        } catch (AssertionError ignore) {
            return false;
        }
    }

    /**
     * Uninstalls any version greater than 1 of shim apex and reboots the device if necessary
     * to complete the uninstall.
     *
     * <p>This is needed because the apex cannot be deleted using PackageInstaller API.
     *
     * Also abandon sessions left by previous tests so staged-installs won't fail.
     */
    @Before
    @After
    public void cleanUp() throws Exception {
        getDevice().executeShellCommand("for i in $(pm list staged-sessions --only-sessionid "
                + "--only-parent); do pm install-abandon $i; done");
        getDevice().executeShellCommand("pm uninstall com.android.cts.install.lib.testapp.A");
        getDevice().executeShellCommand("pm uninstall com.android.cts.install.lib.testapp.B");
        run("cleanUp");
        uninstallShimApexIfNecessary();
    }

    /**
     * Tests staged rollbacks involving only apks.
     */
    @Test
    public void testApkOnlyStagedRollback() throws Exception {
        run("testApkOnlyStagedRollback_Phase1");
        getDevice().reboot();
        run("testApkOnlyStagedRollback_Phase2");
        getDevice().reboot();
        run("testApkOnlyStagedRollback_Phase3");
    }

    /**
     * Tests multiple staged rollbacks involving only apks.
     */
    @Test
    public void testApkOnlyMultipleStagedRollback() throws Exception {
        assumeTrue(isCheckpointSupported());
        run("testApkOnlyMultipleStagedRollback_Phase1");
        getDevice().reboot();
        run("testApkOnlyMultipleStagedRollback_Phase2");
        getDevice().reboot();
        run("testApkOnlyMultipleStagedRollback_Phase3");
    }

    /**
     * Tests multiple staged partial rollbacks involving only apks.
     */
    @Test
    public void testApkOnlyMultipleStagedPartialRollback() throws Exception {
        assumeTrue(isCheckpointSupported());
        run("testApkOnlyMultipleStagedPartialRollback_Phase1");
        getDevice().reboot();
        run("testApkOnlyMultipleStagedPartialRollback_Phase2");
        getDevice().reboot();
        run("testApkOnlyMultipleStagedPartialRollback_Phase3");
    }

    /**
     * Tests staged rollbacks involving only apex.
     */
    @Test
    public void testApexOnlyStagedRollback() throws Exception {
        assumeTrue("Device does not support updating APEX", isApexUpdateSupported());

        run("testApexOnlyStagedRollback_Phase1");
        getDevice().reboot();
        run("testApexOnlyStagedRollback_Phase2");
        getDevice().reboot();
        run("testApexOnlyStagedRollback_Phase3");
        getDevice().reboot();
        run("testApexOnlyStagedRollback_Phase4");
    }

    /**
     * Tests staged rollbacks to system version involving only apex.
     */
    @Test
    public void testApexOnlySystemVersionStagedRollback() throws Exception {
        assumeTrue("Device does not support updating APEX", isApexUpdateSupported());

        run("testApexOnlySystemVersionStagedRollback_Phase1");
        getDevice().reboot();
        run("testApexOnlySystemVersionStagedRollback_Phase2");
        getDevice().reboot();
        run("testApexOnlySystemVersionStagedRollback_Phase3");
    }

    /**
     * Tests staged rollbacks involving apex and apk.
     */
    @Test
    public void testApexAndApkStagedRollback() throws Exception {
        assumeTrue("Device does not support updating APEX", isApexUpdateSupported());

        run("testApexAndApkStagedRollback_Phase1");
        getDevice().reboot();
        run("testApexAndApkStagedRollback_Phase2");
        getDevice().reboot();
        run("testApexAndApkStagedRollback_Phase3");
        getDevice().reboot();
        run("testApexAndApkStagedRollback_Phase4");
    }

    /**
     * Tests that apex update expires existing rollbacks for that apex.
     */
    @Test
    public void testApexRollbackExpiration() throws Exception {
        assumeTrue("Device does not support updating APEX", isApexUpdateSupported());

        run("testApexRollbackExpiration_Phase1");
        getDevice().reboot();
        run("testApexRollbackExpiration_Phase2");
        getDevice().reboot();
        run("testApexRollbackExpiration_Phase3");
    }

    /**
     * Tests staged rollbacks involving apex with rotated keys.
     */
    @Test
    public void testApexKeyRotationStagedRollback() throws Exception {
        assumeTrue("Device does not support updating APEX", isApexUpdateSupported());

        run("testApexKeyRotationStagedRollback_Phase1");
        getDevice().reboot();
        run("testApexKeyRotationStagedRollback_Phase2");
        getDevice().reboot();
        run("testApexKeyRotationStagedRollback_Phase3");
    }

    /**
     * Tests installer B can't rollback a package installed by A.
     */
    @Test
    public void testApkRollbackByAnotherInstaller() throws Exception {
        run("testApkRollbackByAnotherInstaller_Phase1");
        run2("testApkRollbackByAnotherInstaller_Phase2");
    }

    /**
     * Tests that rollbacks are invalidated upon fingerprint changes.
     */
    @Test
    public void testFingerprintChange() throws Exception {
        assumeThat(getDevice().getBuildFlavor(), not(endsWith("-user")));

        try {
            getDevice().executeShellCommand("setprop persist.pm.mock-upgrade true");

            run("testFingerprintChange_Phase1");
            getDevice().reboot();
            run("testFingerprintChange_Phase2");
        } finally {
            getDevice().executeShellCommand("setprop persist.pm.mock-upgrade false");
        }
    }
}
