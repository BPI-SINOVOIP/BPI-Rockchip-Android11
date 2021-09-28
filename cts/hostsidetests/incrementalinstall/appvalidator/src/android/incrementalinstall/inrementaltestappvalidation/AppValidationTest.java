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

package android.incrementalinstall.inrementaltestappvalidation;

import static android.incrementalinstall.common.Consts.INCREMENTAL_TEST_APP_STATUS_RECEIVER_ACTION;
import static android.incrementalinstall.common.Consts.INSTALLED_VERSION_CODE_TAG;
import static android.incrementalinstall.common.Consts.IS_INCFS_INSTALLATION_TAG;
import static android.incrementalinstall.common.Consts.LOADED_COMPONENTS_TAG;
import static android.incrementalinstall.common.Consts.NOT_LOADED_COMPONENTS_TAG;
import static android.incrementalinstall.common.Consts.PACKAGE_TO_LAUNCH_TAG;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.IntentFilter;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.UiDevice;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
@AppModeFull
public class AppValidationTest {

    private Context mContext;
    private String mPackageToLaunch;
    private UiDevice mDevice;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        mPackageToLaunch = InstrumentationRegistry.getArguments().getString(PACKAGE_TO_LAUNCH_TAG);
        mDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    @Test
    public void testAppComponentsInvoked() throws Exception {
        String[] loadedComponents = InstrumentationRegistry.getArguments()
                .getString(LOADED_COMPONENTS_TAG).split(",");
        String[] notLoadedComponents = InstrumentationRegistry.getArguments()
                .getString(NOT_LOADED_COMPONENTS_TAG).split(",");
        StatusReceiver statusReceiver = new StatusReceiver();
        IntentFilter intentFilter = new IntentFilter(INCREMENTAL_TEST_APP_STATUS_RECEIVER_ACTION);
        mContext.registerReceiver(statusReceiver, intentFilter);
        launchTestApp();
        for (String component : loadedComponents) {
            assertTrue(
                    String.format("Component :%s was not loaded, when it should have.", component),
                    statusReceiver.verifyComponentInvoked(component));
        }
        for (String component : notLoadedComponents) {
            assertFalse(
                    String.format("Component :%s was loaded, when it shouldn't have.", component),
                    statusReceiver.verifyComponentInvoked(component));
        }
        mContext.unregisterReceiver(statusReceiver);
    }

    @Test
    public void testInstallationTypeAndVersion() throws Exception {
        boolean isIncfsInstallation = Boolean.parseBoolean(InstrumentationRegistry.getArguments()
                .getString(IS_INCFS_INSTALLATION_TAG));
        int versionCode = Integer.parseInt(InstrumentationRegistry.getArguments()
                .getString(INSTALLED_VERSION_CODE_TAG));
        InstalledAppInfo installedAppInfo = getInstalledAppInfo();
        assertEquals(isIncfsInstallation,
                new PathChecker().isIncFsPath(installedAppInfo.installationPath));
        assertEquals(versionCode, installedAppInfo.versionCode);
    }

    private void launchTestApp() throws Exception {
        mDevice.executeShellCommand(String.format("am start %s/.MainActivity", mPackageToLaunch));
    }

    private InstalledAppInfo getInstalledAppInfo() throws Exception {
        // Output of the command is package:<path>/apk=<package_name> versionCode:<version>, we
        // just need the <path> and <version>.
        String output = mDevice.executeShellCommand(
                "pm list packages -f --show-versioncode " + mPackageToLaunch);
        // outputSplits[0] will contain path information and outputSplits[1] will contain
        // versionCode.
        String[] outputSplits = output.split(" ");
        String installationPath = outputSplits[0].trim().substring("package:".length(),
                output.lastIndexOf("/"));
        int versionCode = Integer.parseInt(
                outputSplits[1].trim().substring("versionCode:".length()));
        return new InstalledAppInfo(installationPath, versionCode);
    }

    private class InstalledAppInfo {

        private final String installationPath;
        private final int versionCode;

        InstalledAppInfo(String installedPath, int versionCode) {
            this.installationPath = installedPath;
            this.versionCode = versionCode;
        }
    }
}
