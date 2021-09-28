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

package com.android.cts.appdataisolation.appb;

import static com.android.cts.appdataisolation.common.FileUtils.assertDirDoesNotExist;
import static com.android.cts.appdataisolation.common.FileUtils.assertDirIsAccessible;
import static com.android.cts.appdataisolation.common.FileUtils.assertDirIsNotAccessible;
import static com.android.cts.appdataisolation.common.FileUtils.assertFileIsAccessible;

import android.content.Context;
import android.content.pm.ApplicationInfo;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;

@SmallTest
public class AppBTests {

    private static final String APPA_PKG = "com.android.cts.appdataisolation.appa";

    private Context mContext;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
    }

    @Test
    public void testCanNotAccessAppADataDir() {
        ApplicationInfo applicationInfo = mContext.getApplicationInfo();
        assertDirDoesNotExist(replacePackageBWithPackageA(applicationInfo.dataDir));
        assertDirDoesNotExist(replacePackageBWithPackageA(applicationInfo.deviceProtectedDataDir));
        assertDirDoesNotExist("/data/data/" + APPA_PKG);
        assertDirDoesNotExist("/data/misc/profiles/cur/0/" + APPA_PKG);
        assertDirIsNotAccessible("/data/misc/profiles/ref");
    }

    @Test
    public void testCanAccessAppADataDir() {
        ApplicationInfo applicationInfo = mContext.getApplicationInfo();
        assertDirIsAccessible(replacePackageBWithPackageA(applicationInfo.dataDir));
        assertDirIsAccessible(replacePackageBWithPackageA(applicationInfo.deviceProtectedDataDir));
        assertDirIsAccessible("/data/data/" + APPA_PKG);
        assertFileIsAccessible("/data/misc/profiles/cur/0/" + APPA_PKG + "/primary.prof");
        assertDirIsNotAccessible("/data/misc/profiles/ref");
    }

    @Test
    public void testCanNotAccessAppAExternalDirs() {
        String appAExternalDir = replacePackageBWithPackageA(
                mContext.getExternalFilesDir("").getParentFile().getAbsolutePath());
        String appAObbDir = replacePackageBWithPackageA(mContext.getObbDir().getAbsolutePath());
        assertDirDoesNotExist(appAExternalDir);
        assertDirDoesNotExist(appAObbDir);
    }

    @Test
    public void testCanAccessAppAExternalDirs() {
        String appAExternalDir = replacePackageBWithPackageA(
                mContext.getExternalFilesDir("").getAbsolutePath());
        String appAObbDir = replacePackageBWithPackageA(mContext.getObbDir().getAbsolutePath());
        assertDirIsAccessible(appAExternalDir);
        assertDirIsAccessible(appAObbDir);
    }

    private String replacePackageBWithPackageA(String path) {
        return path.replace(mContext.getPackageName(), APPA_PKG);
    }

}
