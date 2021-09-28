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

package com.android.tests.apex.sdkextensions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.android.tests.apex.ApexE2EBaseHostTest;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.util.CommandResult;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

@RunWith(DeviceJUnit4ClassRunner.class)
public class SdkExtensionsHostTest extends ApexE2EBaseHostTest {

    private static final String APP_FILENAME = "sdkextensions_e2e_test_app.apk";
    private static final String APP_PACKAGE = "com.android.tests.apex.sdkextensions";
    private static final String MEDIA_FILENAME = "test_com.android.media.apex";
    private static final String SDKEXTENSIONS_FILENAME = "test_com.android.sdkext.apex";

    @Before
    public void installTestApp() throws Exception {
        File testAppFile = mUtils.getTestFile(APP_FILENAME);
        String installResult = getDevice().installPackage(testAppFile, true);
        assertNull(installResult);
    }

    @After
    public void uninstallTestApp() throws Exception {
        String uninstallResult = getDevice().uninstallPackage(APP_PACKAGE);
        assertNull(uninstallResult);
    }

    @Override
    protected List<String> getAllApexFilenames() {
        return List.of(SDKEXTENSIONS_FILENAME, MEDIA_FILENAME);
    }

    @Test
    public void testDefault() throws Exception {
        assertVersion0();
    }

    @Test
    public void upgradeOneApex() throws Exception {
        // On the system image, sdkextensions is the only apex with sdkinfo, and it's version 0.
        // This test verifies that installing media with sdk version 45 doesn't bump the version.
        assertVersion0();
        installApex(MEDIA_FILENAME);
        reboot(false);
        assertVersion0();
    }

    @Test
    public void upgradeTwoApexes() throws Exception {
        // On the system image, sdkextensions is the only apex with sdkinfo, and it's version 0.
        // This test verifies that installing media with sdk version 45 *and* a new sdkext does bump
        // the version.
        assertVersion0();
        installApexes(MEDIA_FILENAME, SDKEXTENSIONS_FILENAME);
        reboot(false);
        assertVersion45();
    }

    @Override
    public void additionalCheck() throws Exception {
        // This method is run after the default test in the base class, which just installs sdkext.
        assertVersion45();
    }

    private int getExtensionVersionR() throws Exception {
        int appValue = Integer.parseInt(broadcast("GET_SDK_VERSION"));
        int syspropValue = getExtensionVersionRFromSysprop();
        assertEquals(appValue, syspropValue);
        return appValue;
    }

    private int getExtensionVersionRFromSysprop() throws Exception {
        CommandResult res = getDevice().executeShellV2Command("getprop build.version.extensions.r");
        assertEquals(0, (int) res.getExitCode());
        String syspropString = res.getStdout().replace("\n", "");
        return Integer.parseInt(syspropString);
    }

    private String broadcast(String action) throws Exception {
        String command = getBroadcastCommand(action);
        CommandResult res = getDevice().executeShellV2Command(command);
        assertEquals(0, (int) res.getExitCode());
        Matcher matcher = Pattern.compile("data=\"([^\"]+)\"").matcher(res.getStdout());
        assertTrue("Unexpected output from am broadcast: " + res.getStdout(), matcher.find());
        return matcher.group(1);
    }

    private void assertVersion0() throws Exception {
        assertEquals(0, getExtensionVersionR());
        assertEquals("true", broadcast("MAKE_CALLS_0"));
    }

    private void assertVersion45() throws Exception {
        assertEquals(45, getExtensionVersionR());
        assertEquals("true", broadcast("MAKE_CALLS_45"));
    }

    private static String getBroadcastCommand(String action) {
        String cmd = "am broadcast";
        cmd += " -a com.android.tests.apex.sdkextensions." + action;
        cmd += " -n com.android.tests.apex.sdkextensions/.Receiver";
        return cmd;
    }
}
