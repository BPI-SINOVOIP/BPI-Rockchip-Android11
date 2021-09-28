/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
package android.appsecurity.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.platform.test.annotations.AppModeFull;
import com.android.ddmlib.Log.LogLevel;
import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Set of tests that verify that corrupt APKs are properly rejected by PackageManager and
 * do not cause the system to crash.
 */
@AppModeFull(reason = "the corrupt APKs were provided as-is and we cannot modify them to comply with instant mode")
public class CorruptApkTests extends DeviceTestCase implements IBuildReceiver {

    private static final String CORRUPT_APK_PACKAGE_NAME = "android.content.cts.corruptapk";
    private static final String FAIL_COMPRESSED_ARSC_PLATFORM_CONFIG_ID = "132742131";
    private static final String COMPRESSED_ARSC_Q = "CtsCorruptApkTests_Compressed_Q.apk";
    private static final String COMPRESSED_ARSC_R = "CtsCorruptApkTests_Compressed_R.apk";
    private static final String UNALIGNED_ARSC_Q = "CtsCorruptApkTests_Unaligned_Q.apk";
    private static final String UNALIGNED_ARSC_R = "CtsCorruptApkTests_Unaligned_R.apk";

    private IBuildInfo mBuildInfo;

    /** A container for information about the system_server process. */
    private class SystemServerInformation {
        final long mPid;
        final long mStartTime;

        SystemServerInformation(long pid, long startTime) {
            this.mPid = pid;
            this.mStartTime = startTime;
        }

        @Override
        public boolean equals(Object actual) {
            return (actual instanceof SystemServerInformation)
                && mPid == ((SystemServerInformation) actual).mPid
                && mStartTime == ((SystemServerInformation) actual).mStartTime;
        }
    }

    /** Retrieves the process id and elapsed run time of system_server. */
    private SystemServerInformation retrieveInfo() throws DeviceNotAvailableException {
        final ITestDevice device = getDevice();

        // Retrieve the process id of system_server
        String pidResult = device.executeShellCommand("pidof system_server").trim();
        assertNotNull("Failed to retrieve pid of system_server", pidResult);
        long pid = 0;
        try {
            pid = Long.parseLong(pidResult);
        } catch (NumberFormatException | IndexOutOfBoundsException e) {
            fail("Unable to parse pid of system_server '" + pidResult + "'");
        }

        // Retrieve the start time of system_server
        long startTime = 0;
        String pidStats = device.executeShellCommand("cat /proc/" + pid + "/stat");
        assertNotNull("Failed to retrieve stat of system_server with pid '" + pid + "'", pidStats);
        try {
            String startTimeJiffies = pidStats.split("\\s+")[21];
            startTime = Long.parseLong(startTimeJiffies);
        } catch (NumberFormatException | IndexOutOfBoundsException e) {
            fail("Unable to parse system_server stat file '" + pidStats + "'");
        }

        return new SystemServerInformation(pid, startTime);
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuildInfo = buildInfo;
    }

   /** Uninstall any test APKs already present on device. */
    private void uninstallApks() throws DeviceNotAvailableException {
        final ITestDevice device = getDevice();
        device.uninstallPackage("com.android.appsecurity.b71360999");
        device.uninstallPackage("com.android.appsecurity.b71361168");
        device.uninstallPackage("com.android.appsecurity.b79488511");
        // WARNING: PlatformCompat overrides for package parsing changes must be reset before the
        // package is uninstalled because overrides cannot be reset if the package is not present.
        device.executeShellCommand("am compat reset-all " + CORRUPT_APK_PACKAGE_NAME);
        device.uninstallPackage(CORRUPT_APK_PACKAGE_NAME);
    }

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        uninstallApks();
    }

    @After
    @Override
    public void tearDown() throws Exception {
        super.tearDown();
        uninstallApks();
    }

    private String install(String apk) throws DeviceNotAvailableException, FileNotFoundException  {
        return getDevice().installPackage(
                new CompatibilityBuildHelper(mBuildInfo).getTestFile(apk),
                true /* reinstall */);
    }

    /**
     * Asserts that installing the application does not cause a native error causing system_server
     * to crash (typically the result of a buffer overflow or an out-of-bounds read).
     */
    private void assertInstallDoesNotCrashSystem(String apk) throws Exception {
        SystemServerInformation beforeInfo = retrieveInfo();

        final String result = install(apk);
        CLog.logAndDisplay(LogLevel.INFO, "Result: '" + result + "'");
        if (result != null) {
            assertFalse("Install package segmentation faulted",
                result.toLowerCase().contains("segmentation fault"));
        }

        assertEquals("system_server restarted", beforeInfo, retrieveInfo());
    }

    /** Installing the APK described in b/71360999 must not crash the device. */
    public void testSafeInstallOfCorruptAPK_b71360999() throws Exception {
        assertInstallDoesNotCrashSystem("CtsCorruptApkTests_b71360999.apk");
    }

    /** Installing the APK described in b/71361168 must not crash the device. */
    public void testSafeInstallOfCorruptAPK_b71361168() throws Exception {
        assertInstallDoesNotCrashSystem("CtsCorruptApkTests_b71361168.apk");
    }

    /** Installing the APK described in b/79488511 must not crash the device. */
    public void testSafeInstallOfCorruptAPK_b79488511() throws Exception {
        assertInstallDoesNotCrashSystem("CtsCorruptApkTests_b79488511.apk");
    }

    /** APKs that target pre-R and have a compressed resources.arsc can be installed. */
    public void testFailInstallCompressedARSC_Q() throws Exception {
        assertNull(install(COMPRESSED_ARSC_Q));
    }

    public void testFailInstallCompressedARSC_Q_PlatformConfig_enabled() throws Exception {
        assertNull(install(COMPRESSED_ARSC_Q));
        getDevice().executeShellCommand(String.format("am compat enable %s %s",
                FAIL_COMPRESSED_ARSC_PLATFORM_CONFIG_ID, CORRUPT_APK_PACKAGE_NAME));
        assertNotNull(install(COMPRESSED_ARSC_Q));
    }

    /** APKs that target R+ and have a compressed resources.arsc must not be installed. */
    public void testFailInstallCompressedARSC_R() throws Exception {
        assertNotNull(install(COMPRESSED_ARSC_R));
    }

    /** APKs that target pre-R and have a unaligned resources.arsc can be installed. */
    public void testFailInstallUnalignedARSC_Q() throws Exception {
        assertNull(install(UNALIGNED_ARSC_Q));
    }

    public void testFailInstallUnalignedARSC_Q_PlatformConfig_enabled() throws Exception {
        assertNull(install(UNALIGNED_ARSC_Q));
        getDevice().executeShellCommand(String.format("am compat enable %s %s",
                FAIL_COMPRESSED_ARSC_PLATFORM_CONFIG_ID, CORRUPT_APK_PACKAGE_NAME));
        assertNotNull(install(UNALIGNED_ARSC_Q));
    }

    /** APKs that target R+ and have a unaligned resources.arsc must not be installed. */
    public void testFailInstallUnalignedARSC_R() throws Exception {
        assertNotNull(install(UNALIGNED_ARSC_R));
    }
}