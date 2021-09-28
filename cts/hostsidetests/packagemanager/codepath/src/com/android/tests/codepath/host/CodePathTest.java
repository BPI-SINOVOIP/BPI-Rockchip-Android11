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
 * limitations under the License
 */

package com.android.tests.codepath.host;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.PackageInfo;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashMap;
import java.util.Map;

@RunWith(DeviceJUnit4ClassRunner.class)
public class CodePathTest extends BaseHostJUnit4Test {
    private static final String TEST_APK = "CodePathTestApp.apk";
    private static final String TEST_PACKAGE = "com.android.tests.codepath.app";
    private static final String TEST_CLASS = TEST_PACKAGE + "." + "CodePathDeviceTest";
    private static final String TEST_METHOD = "testCodePathMatchesExpected";
    private static final String CODE_PATH_ROOT = "/data/app";
    private static final long DEFAULT_TIMEOUT = 10 * 60 * 1000L;

    /** Uninstall apps after tests. */
    @After
    public void cleanUp() throws Exception {
        uninstallPackage(getDevice(), TEST_PACKAGE);
        Assert.assertFalse(isPackageInstalled(TEST_PACKAGE));
    }

    @Test
    @AppModeFull
    public void testAppInstallsWithReboot() throws Exception {
        installPackage(TEST_APK);
        Assert.assertTrue(isPackageInstalled(TEST_PACKAGE));
        final String codePathFromDumpsys = getCodePathFromDumpsys(TEST_PACKAGE);
        Assert.assertTrue(codePathFromDumpsys.startsWith(CODE_PATH_ROOT));
        runDeviceTest(codePathFromDumpsys);
        // Code paths should still be valid after reboot
        getDevice().reboot();
        final String codePathFromDumpsysAfterReboot = getCodePathFromDumpsys(TEST_PACKAGE);
        Assert.assertEquals(codePathFromDumpsys, codePathFromDumpsysAfterReboot);
        runDeviceTest(codePathFromDumpsys);
    }

    private String getCodePathFromDumpsys(String packageName)
            throws DeviceNotAvailableException {
        PackageInfo packageInfo = getDevice().getAppPackageInfo(packageName);
        return packageInfo.getCodePath();
    }

    private void runDeviceTest(String codePathToMatch) throws Exception {
        final Map<String, String> testArgs = new HashMap<>();
        testArgs.put("expectedCodePath", codePathToMatch);
        runDeviceTests(getDevice(), null, TEST_PACKAGE, TEST_CLASS, TEST_METHOD,
                null, DEFAULT_TIMEOUT, DEFAULT_TIMEOUT,
                0L, true, false, testArgs);
    }
}