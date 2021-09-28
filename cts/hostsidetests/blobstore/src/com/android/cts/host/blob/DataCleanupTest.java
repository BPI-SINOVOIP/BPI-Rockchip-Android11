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
package com.android.cts.host.blob;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Map;

@RunWith(DeviceJUnit4ClassRunner.class)
public class DataCleanupTest extends BaseBlobStoreHostTest {
    private static final String TEST_CLASS = TARGET_PKG + ".DataCleanupTest";

    @Test
    public void testPackageUninstall_openSession() throws Exception {
        installPackage(TARGET_APK);
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testCreateSession");
        final Map<String, String> args = createArgsFromLastTestRun();
        // Verify that previously created session can be accessed.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenSession", args);
        uninstallPackage(TARGET_PKG);
        installPackage(TARGET_APK);
        // Verify that the new package cannot access the session.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenSession_shouldThrow", args);
    }

    @Test
    public void testPackageUninstall_openBlob() throws Exception {
        installPackage(TARGET_APK);
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testCommitBlob");
        final Map<String, String> args = createArgsFromLastTestRun();
        // Verify that previously committed blob can be accessed.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenBlob", args);
        uninstallPackage(TARGET_PKG);
        installPackage(TARGET_APK);
        // Verify that the new package cannot access the blob.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenBlob_shouldThrow", args);
    }

    @Test
    public void testPackageUninstallAndReboot_openSession() throws Exception {
        installPackage(TARGET_APK);
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testCreateSession");
        final Map<String, String> args = createArgsFromLastTestRun();
        // Verify that previously created session can be accessed.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenSession", args);
        uninstallPackage(TARGET_PKG);
        // Reboot the device.
        rebootAndWaitUntilReady();
        installPackage(TARGET_APK);
        // Verify that the new package cannot access the session.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenSession_shouldThrow", args);
    }

    @Test
    public void testPackageUninstallAndReboot_openBlob() throws Exception {
        installPackage(TARGET_APK);
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testCommitBlob");
        final Map<String, String> args = createArgsFromLastTestRun();
        // Verify that previously committed blob can be accessed.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenBlob", args);
        uninstallPackage(TARGET_PKG);
        // Reboot the device.
        rebootAndWaitUntilReady();
        installPackage(TARGET_APK);
        // Verify that the new package cannot access the blob.
        runDeviceTest(TARGET_PKG, TEST_CLASS, "testOpenBlob_shouldThrow", args);
    }
}
