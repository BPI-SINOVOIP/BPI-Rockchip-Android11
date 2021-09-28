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
package com.android.tradefed.build;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link AppDeviceBuildInfo}. */
@RunWith(JUnit4.class)
public class AppDeviceBuildInfoTest {

    private AppDeviceBuildInfo mAppBuildInfo;

    @Before
    public void setUp() {
        mAppBuildInfo = new AppDeviceBuildInfo("8888", "build_target");
    }

    /**
     * Test that when setting the device and app build, the AppDeviceBuildInfo gets all their file.
     */
    @Test
    public void testSettingBuilds() {
        IAppBuildInfo appBuild = new AppBuildInfo("5555", "build_target2");
        appBuild.addAppPackageFile(new File("package"), "v1");
        appBuild.addAppPackageFile(new File("package2"), "v1");
        assertEquals(2, appBuild.getAppPackageFiles().size());
        assertEquals(0, mAppBuildInfo.getAppPackageFiles().size());
        mAppBuildInfo.setAppBuild(appBuild);
        // AppBuild gave its file to the main build
        assertEquals(2, mAppBuildInfo.getAppPackageFiles().size());

        IDeviceBuildInfo deviceBuild = new DeviceBuildInfo("3333", "build_target3");
        deviceBuild.setBootloaderImageFile(new File("bootloader"), "v2");
        assertNull(mAppBuildInfo.getBootloaderImageFile());

        mAppBuildInfo.setDeviceBuild(deviceBuild);
        // deviceBuild gave its file to the main build
        assertNotNull(mAppBuildInfo.getBootloaderImageFile());
    }
}
