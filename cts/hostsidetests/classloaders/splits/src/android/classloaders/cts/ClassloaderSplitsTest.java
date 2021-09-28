/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
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

@RunWith(DeviceJUnit4ClassRunner.class)
public class ClassloaderSplitsTest extends BaseHostJUnit4Test {
    private static final String PKG = "com.android.cts.classloadersplitapp";
    private static final String TEST_CLASS = PKG + ".SplitAppTest";

    /* The feature hierarchy looks like this:

        APK_BASE (PathClassLoader)
          ^
          |
        APK_FEATURE_A (DelegateLastClassLoader)
          ^
          |
        APK_FEATURE_B (PathClassLoader)

     */

    private static final String APK_BASE = "CtsClassloaderSplitApp.apk";
    private static final String APK_FEATURE_A = "CtsClassloaderSplitAppFeatureA.apk";
    private static final String APK_FEATURE_B = "CtsClassloaderSplitAppFeatureB.apk";

    @Before
    public void setUp() throws Exception {
        getDevice().uninstallPackage(PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(PKG);
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testBaseClassLoader_full() throws Exception {
        testBaseClassLoader(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testBaseClassLoader_instant() throws Exception {
        testBaseClassLoader(true);
    }
    private void testBaseClassLoader(boolean instant) throws Exception {
        new InstallMultiple(instant).addApk(APK_BASE).run();
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testBaseClassLoader");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testFeatureAClassLoader_full() throws Exception {
        testFeatureAClassLoader(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testFeatureAClassLoader_instant() throws Exception {
        testFeatureAClassLoader(true);
    }
    private void testFeatureAClassLoader(boolean instant) throws Exception {
        new InstallMultiple(instant).addApk(APK_BASE).addApk(APK_FEATURE_A).run();
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testBaseClassLoader");
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testFeatureAClassLoader");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testFeatureBClassLoader_full() throws Exception {
        testFeatureBClassLoader(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testFeatureBClassLoader_instant() throws Exception {
        testFeatureBClassLoader(true);
    }
    private void testFeatureBClassLoader(boolean instant) throws Exception {
        new InstallMultiple(instant)
                .addApk(APK_BASE).addApk(APK_FEATURE_A).addApk(APK_FEATURE_B).run();
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testBaseClassLoader");
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testFeatureAClassLoader");
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testFeatureBClassLoader");
    }

    @Test
    @AppModeFull(reason = "b/109878606; instant applications can't send broadcasts to manifest receivers")
    public void testReceiverClassLoaders_full() throws Exception {
        testReceiverClassLoaders(false);
    }
    private void testReceiverClassLoaders(boolean instant) throws Exception {
        new InstallMultiple(instant)
                .addApk(APK_BASE).addApk(APK_FEATURE_A).addApk(APK_FEATURE_B).run();
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testBaseClassLoader");
        runDeviceTests(getDevice(), PKG, TEST_CLASS, "testAllReceivers");
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
