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

import static org.junit.Assert.assertNull;

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Tests for the instant cookie APIs
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull(reason = "Already handles instant installs when needed")
public class InstantCookieHostTest extends BaseHostJUnit4Test {
    private static final String INSTANT_COOKIE_APP_APK = "CtsInstantCookieApp.apk";
    private static final String INSTANT_COOKIE_APP_PKG = "test.instant.cookie";

    private static final String INSTANT_COOKIE_APP_APK_2 = "CtsInstantCookieApp2.apk";
    private static final String INSTANT_COOKIE_APP_PKG_2 = "test.instant.cookie";

    @Before
    public void setUp() throws Exception {
        Utils.prepareSingleUser(getDevice());
        uninstallPackage(INSTANT_COOKIE_APP_PKG);
        clearAppCookieData();
    }

    @After
    public void tearDown() throws Exception {
        uninstallPackage(INSTANT_COOKIE_APP_PKG);
        clearAppCookieData();
    }

    @Test
    public void testCookieUpdateAndRetrieval() throws Exception {
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, false, true));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookieUpdateAndRetrieval");
    }

    @Test
    public void testCookiePersistedAcrossInstantInstalls() throws Exception {
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, false, true));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookiePersistedAcrossInstantInstalls1");
        uninstallPackage(INSTANT_COOKIE_APP_PKG);
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, false, true));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookiePersistedAcrossInstantInstalls2");
    }

    @Test
    public void testCookiePersistedUpgradeFromInstant() throws Exception {
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, false, true));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookiePersistedUpgradeFromInstant1");
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, true, false));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookiePersistedUpgradeFromInstant2");
    }

    @Test
    public void testCookieResetOnNonInstantReinstall() throws Exception {
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, false, false));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookieResetOnNonInstantReinstall1");
        uninstallPackage(INSTANT_COOKIE_APP_PKG);
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, true, false));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookieResetOnNonInstantReinstall2");
    }

    @Test
    public void testCookieValidWhenSignedWithTwoCerts() throws Exception {
        assertNull(installPackage(INSTANT_COOKIE_APP_APK, false, true));
        runDeviceTests(INSTANT_COOKIE_APP_PKG, "test.instant.cookie.CookieTest",
                "testCookiePersistedAcrossInstantInstalls1");
        uninstallPackage(INSTANT_COOKIE_APP_PKG);
        assertNull(installPackage(INSTANT_COOKIE_APP_APK_2, true, true));
        runDeviceTests(INSTANT_COOKIE_APP_PKG_2, "test.instant.cookie.CookieTest",
                "testCookiePersistedAcrossInstantInstalls2");
    }

    private String installPackage(String apk, boolean replace, boolean instant) throws Exception {
        return getDevice().installPackage(getTestAppFile(apk), replace,
                instant ? "--instant" : "--full");
    }

    private File getTestAppFile(String fileName) throws FileNotFoundException {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(getBuild());
        return buildHelper.getTestFile(fileName);
    }

    private void clearAppCookieData() throws Exception {
        getDevice().executeShellCommand("pm clear " + INSTANT_COOKIE_APP_PKG);
    }
}
