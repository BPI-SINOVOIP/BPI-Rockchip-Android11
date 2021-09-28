/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.classloaders.cts;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Set of tests that verify behavior of runtime permissions, including both
 * dynamic granting and behavior of legacy apps.
 */
@AppModeFull(reason = "TODO verify whether or not these should run in instant mode")
@RunWith(DeviceJUnit4ClassRunner.class)
public class UsesLibraryHostTest extends BaseHostJUnit4Test {
    private static final String PKG = "com.android.cts.useslibrary";

    private static final String APK = "CtsUsesLibraryApp.apk";
    private static final String APK_COMPAT = "CtsUsesLibraryAppCompat.apk";


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
    @AppModeFull
    public void testUsesLibrary_full() throws Exception {
        testUsesLibrary(false);
    }
    private void testUsesLibrary(boolean instant) throws Exception {
        new InstallMultiple(instant).addApk(APK).run();
        Utils.runDeviceTests(getDevice(), PKG, ".UsesLibraryTest", "testUsesLibrary");
    }

    @Test
    @AppModeFull
    public void testMissingLibrary_full() throws Exception {
        testMissingLibrary(false);
    }
    public void testMissingLibrary(boolean instant) throws Exception {
        new InstallMultiple(instant).addApk(APK).run();
        Utils.runDeviceTests(getDevice(), PKG, ".UsesLibraryTest", "testMissingLibrary");
    }

    @Test
    @AppModeFull
    public void testDuplicateLibrary_full() throws Exception {
        testDuplicateLibrary(false);
    }
    public void testDuplicateLibrary(boolean instant) throws Exception {
        new InstallMultiple(instant).addApk(APK).run();
        Utils.runDeviceTests(getDevice(), PKG, ".UsesLibraryTest", "testDuplicateLibrary");
    }

    protected class InstallMultiple extends BaseInstallMultiple<InstallMultiple> {
        public InstallMultiple() {
            this(false);
        }
        public InstallMultiple(boolean instant) {
            super(getDevice(), getBuild(), getAbi());
            addArg(instant ? "--instant" : "");
        }
    }
}
