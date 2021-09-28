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
public class UseProcessTest extends BaseAppSecurityTest {
    private static final String PKG = "com.android.cts.useprocess";
    private static final String APK_SUCCESS = "CtsUseProcessSuccess.apk";
    private static final String APK_FAIL_APPLICATION = "CtsUseProcessFailApplication.apk";
    private static final String APK_FAIL_ACTIVITY = "CtsUseProcessFailActivity.apk";
    private static final String APK_FAIL_SERVICE = "CtsUseProcessFailService.apk";
    private static final String APK_FAIL_RECEIVER = "CtsUseProcessFailReceiver.apk";
    private static final String APK_FAIL_PROVIDER = "CtsUseProcessFailProvider.apk";

    private static final String SUCCESS_UNIT_TEST_CLASS
            = "com.android.cts.useprocess.AccessNetworkTest";

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
    public void testInstallUsePackageSuccess_full() throws Exception {
        testInstallUsePackageSuccess(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallUsePackageSuccess_instant() throws Exception {
        testInstallUsePackageSuccess(true);
    }
    private void testInstallUsePackageSuccess(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_SUCCESS).run();
        assertTrue(getDevice().getInstalledPackageNames().contains(PKG));

        Utils.runDeviceTestsAsCurrentUser(getDevice(), PKG, SUCCESS_UNIT_TEST_CLASS, null);
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallUsePackageFailApplication_full() throws Exception {
        testInstallUsePackageFailApplication(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallUsePackageFailApplication_instant() throws Exception {
        testInstallUsePackageFailApplication(true);
    }
    private void testInstallUsePackageFailApplication(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_FAIL_APPLICATION).runExpectingFailure(
                "Failure [INSTALL_FAILED_PROCESS_NOT_DEFINED: Scanning Failed.: " +
                        "Can't install because application");
        assertTrue(!getDevice().getInstalledPackageNames().contains(PKG));
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallUsePackageFailActivity_full() throws Exception {
        testInstallUsePackageFailActivity(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallUsePackageFailActivity_instant() throws Exception {
        testInstallUsePackageFailActivity(true);
    }
    private void testInstallUsePackageFailActivity(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_FAIL_ACTIVITY).runExpectingFailure(
                "Failure [INSTALL_FAILED_PROCESS_NOT_DEFINED: Scanning Failed.: " +
                        "Can't install because activity com.android.cts.useprocess.DummyActivity");
        assertTrue(!getDevice().getInstalledPackageNames().contains(PKG));
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallUsePackageFailService_full() throws Exception {
        testInstallUsePackageFailService(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallUsePackageFailService_instant() throws Exception {
        testInstallUsePackageFailService(true);
    }
    private void testInstallUsePackageFailService(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_FAIL_SERVICE).runExpectingFailure(
                "Failure [INSTALL_FAILED_PROCESS_NOT_DEFINED: Scanning Failed.: " +
                        "Can't install because service com.android.cts.useprocess.DummyService");
        assertTrue(!getDevice().getInstalledPackageNames().contains(PKG));
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallUsePackageFailReceiver_full() throws Exception {
        testInstallUsePackageFailReceiver(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallUsePackageFailReceiver_instant() throws Exception {
        testInstallUsePackageFailReceiver(true);
    }
    private void testInstallUsePackageFailReceiver(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_FAIL_RECEIVER).runExpectingFailure(
                "Failure [INSTALL_FAILED_PROCESS_NOT_DEFINED: Scanning Failed.: " +
                        "Can't install because receiver com.android.cts.useprocess.DummyReceiver");
        assertTrue(!getDevice().getInstalledPackageNames().contains(PKG));
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testInstallUsePackageFailProvider_full() throws Exception {
        testInstallUsePackageFailProvider(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testInstallUsePackageFailProvider_instant() throws Exception {
        testInstallUsePackageFailProvider(true);
    }
    private void testInstallUsePackageFailProvider(boolean instant) throws Exception {
        new InstallMultiple(instant).addFile(APK_FAIL_PROVIDER).runExpectingFailure(
                "Failure [INSTALL_FAILED_PROCESS_NOT_DEFINED: Scanning Failed.: " +
                        "Can't install because provider com.android.cts.useprocess.DummyProvider");
        assertTrue(!getDevice().getInstalledPackageNames().contains(PKG));
    }
}
