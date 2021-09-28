/*
 * Copyright (C) 2021 The Android Open Source Project
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

package android.security.cts;

import android.platform.test.annotations.AppModeInstant;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;
import android.platform.test.annotations.SecurityTest;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test that collects test results from test package android.security.cts.CVE_2021_0305.
 *
 * When this test builds, it also builds a support APK containing
 * {@link android.sample.cts.CVE_2021_0305.SampleDeviceTest}, the results of which are
 * collected from the hostside and reported accordingly.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CVE_2021_0305 extends BaseHostJUnit4Test {
    private static final String TEST_PKG = "android.security.cts.CVE_2021_0305";
    private static final String TEST_CLASS = TEST_PKG + "." + "DeviceTest";
    private static final String TEST_APP = "CVE-2021-0305.apk";

    @Before
    public void setUp() throws Exception {
        uninstallPackage(getDevice(), TEST_PKG);
    }

    @Test
    @SecurityTest(minPatchLevel = "2020-09")
    @AppModeFull
    public void testRunDeviceTestsPassesFull() throws Exception {
        installPackage();
        Assert.assertTrue(runDeviceTests(TEST_PKG, TEST_CLASS, "testClick"));
    }

    private void installPackage() throws Exception {
        installPackage(TEST_APP, new String[0]);
    }
}

