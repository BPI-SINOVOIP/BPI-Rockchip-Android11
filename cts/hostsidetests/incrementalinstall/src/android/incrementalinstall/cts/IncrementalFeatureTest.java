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

package android.incrementalinstall.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.util.CddTest;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * Test that all devices launched with R have the Incremental feature enabled. Test that the feature
 * is working properly by installing a package.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class IncrementalFeatureTest extends BaseHostJUnit4Test {
    private static final String FEATURE_INCREMENTAL_DELIVERY =
            "android.software.incremental_delivery";
    private static final String TEST_REMOTE_DIR = "/data/local/tmp/incremental_feature_test";
    private static final String TEST_APP_PACKAGE_NAME =
            "android.incrementalinstall.incrementaltestapp";
    private static final String TEST_APP_BASE_APK_NAME = "IncrementalTestApp.apk";
    private static final String IDSIG_SUFFIX = ".idsig";
    private static final String TEST_APP_BASE_APK_IDSIG_NAME =
            TEST_APP_BASE_APK_NAME + IDSIG_SUFFIX;

    @Before
    public void setUp() throws Exception {
        // Test devices that are launched with R and above; ignore non-targeted devices
        assumeTrue(getDevice().getLaunchApiLevel() >= 30 /* Build.VERSION_CODES.R */);
    }

    @CddTest(requirement="4/C-1-1")
    @Test
    public void testFeatureAvailable() throws Exception {
        assertTrue(getDevice().hasFeature(FEATURE_INCREMENTAL_DELIVERY));
    }

    @CddTest(requirement="4/C-1-1,C-3-1")
    @Test
    public void testFeatureWorkingWithInstallation() throws Exception {
        getDevice().executeShellCommand("mkdir " + TEST_REMOTE_DIR);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(getBuild());
        final File apk = buildHelper.getTestFile(TEST_APP_BASE_APK_NAME);
        assertNotNull(apk);
        final File idsig = buildHelper.getTestFile(TEST_APP_BASE_APK_IDSIG_NAME);
        assertNotNull(idsig);
        final String remoteApkPath = TEST_REMOTE_DIR + "/" + apk.getName();
        final String remoteIdsigPath = remoteApkPath + IDSIG_SUFFIX;
        assertTrue(getDevice().pushFile(apk, remoteApkPath));
        assertTrue(getDevice().pushFile(idsig, remoteIdsigPath));
        String installResult = getDevice().executeShellCommand(
                "pm install-incremental -t -g " + remoteApkPath);
        assertEquals("Success\n", installResult);
        assertTrue(getDevice().isPackageInstalled(TEST_APP_PACKAGE_NAME));
        getDevice().uninstallPackage(TEST_APP_PACKAGE_NAME);
    }
}
