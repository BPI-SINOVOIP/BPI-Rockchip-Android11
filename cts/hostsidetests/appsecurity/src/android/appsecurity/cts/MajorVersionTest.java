/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.appsecurity.cts;

import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test that install of apps using major version codes is being handled properly.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class MajorVersionTest extends BaseAppSecurityTest {
    private static final String PKG = "com.android.cts.majorversion";
    private static final String APK_000000000000ffff = "CtsMajorVersion000000000000ffff.apk";
    private static final String APK_00000000ffffffff = "CtsMajorVersion00000000ffffffff.apk";
    private static final String APK_000000ff00000000 = "CtsMajorVersion000000ff00000000.apk";
    private static final String APK_000000ffffffffff = "CtsMajorVersion000000ffffffffff.apk";

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        getDevice().uninstallPackage(PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(PKG);
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallMinorVersion_full() throws Exception {
        testInstallMinorVersion(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallMinorVersion_instant() throws Exception {
        testInstallMinorVersion(true);
    }
    private void testInstallMinorVersion(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_000000000000ffff).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallMajorVersion_full() throws Exception {
        testInstallMajorVersion(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallMajorVersion_instant() throws Exception {
        testInstallMajorVersion(true);
    }
    private void testInstallMajorVersion(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_000000ff00000000).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallUpdateAcrossMinorMajorVersion_full() throws Exception {
        testInstallUpdateAcrossMinorMajorVersion(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallUpdateAcrossMinorMajorVersion_instant() throws Exception {
        testInstallUpdateAcrossMinorMajorVersion(true);
    }
    private void testInstallUpdateAcrossMinorMajorVersion(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_000000000000ffff).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
        new InstallMultiple(instant).addFile(APK_00000000ffffffff).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
        new InstallMultiple(instant).addFile(APK_000000ff00000000).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
        new InstallMultiple(instant).addFile(APK_000000ffffffffff).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallDowngradeAcrossMajorMinorVersion_full() throws Exception {
        testInstallDowngradeAcrossMajorMinorVersion(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallDowngradeAcrossMajorMinorVersion_instant() throws Exception {
        testInstallDowngradeAcrossMajorMinorVersion(true);
    }
    private void testInstallDowngradeAcrossMajorMinorVersion(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_000000ffffffffff).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
        new InstallMultiple(instant).addFile(APK_00000000ffffffff)
                .runExpectingFailure("INSTALL_FAILED_VERSION_DOWNGRADE");
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
        new InstallMultiple(instant).addFile(APK_000000ff00000000)
                .runExpectingFailure("INSTALL_FAILED_VERSION_DOWNGRADE");
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
        new InstallMultiple(instant).addFile(APK_000000000000ffff)
                .runExpectingFailure("INSTALL_FAILED_VERSION_DOWNGRADE");
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));
        runVersionDeviceTests("testCheckVersion");
    }

    private void runVersionDeviceTests(String testMethodName) throws Exception {
        runDeviceTests(PKG, PKG + ".VersionTest", testMethodName);
    }
}
