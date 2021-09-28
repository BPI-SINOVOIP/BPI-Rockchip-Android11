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

package android.extractnativelibs.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * Host test to incremental install test apps and run device tests to verify the effect of
 * extractNativeLibs.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CtsExtractNativeLibsHostTestIncremental extends CtsExtractNativeLibsHostTestBase {
    private static final String IDSIG_SUFFIX = ".idsig";

    @Override
    public void setUp() throws Exception {
        assumeTrue(isIncrementalInstallSupported());
        super.setUp();
    }

    private boolean isIncrementalInstallSupported() throws Exception {
        return getDevice().hasFeature("android.software.incremental_delivery");
    }
    /** Test with a app that has extractNativeLibs=false using Incremental install. */
    @Test
    @AppModeFull
    public void testNoExtractNativeLibsIncremental() throws Exception {
        installPackageIncremental(TEST_NO_EXTRACT_APK);
        assertTrue(isPackageInstalled(TEST_NO_EXTRACT_PKG));
        assertTrue(runDeviceTests(
                TEST_NO_EXTRACT_PKG, TEST_NO_EXTRACT_CLASS, TEST_NO_EXTRACT_TEST));
    }

    /** Test with a app that has extractNativeLibs=true using Incremental install. */
    @Test
    @AppModeFull
    public void testExtractNativeLibsIncremental() throws Exception {
        installPackageIncremental(TEST_EXTRACT_APK);
        assertTrue(isPackageInstalled(TEST_EXTRACT_PKG));
        assertTrue(runDeviceTests(
                TEST_EXTRACT_PKG, TEST_EXTRACT_CLASS, TEST_EXTRACT_TEST));
    }

    /** Test with a app that has extractNativeLibs=false but with mis-aligned lib files,
     *  using Incremental install. */
    @Test
    @AppModeFull
    public void testExtractNativeLibsIncrementalFails() throws Exception {
        String result = installIncrementalPackageFromResource(TEST_NO_EXTRACT_MISALIGNED_APK);
        assertTrue(result.contains("Failed to extract native libraries"));
        assertFalse(isPackageInstalled(TEST_NO_EXTRACT_PKG));
    }

    private void installPackageIncremental(String apkName) throws Exception {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(getBuild());
        final File apk = buildHelper.getTestFile(apkName);
        assertNotNull(apk);
        final File v4Signature = buildHelper.getTestFile(apkName + IDSIG_SUFFIX);
        assertNotNull(v4Signature);
        installPackageIncrementalFromFiles(apk, v4Signature);
    }

    private String installPackageIncrementalFromFiles(File apk, File v4Signature) throws Exception {
        final String remoteApkPath = TEST_REMOTE_DIR + "/" + apk.getName();
        final String remoteIdsigPath = remoteApkPath + IDSIG_SUFFIX;
        assertTrue(getDevice().pushFile(apk, remoteApkPath));
        assertTrue(getDevice().pushFile(v4Signature, remoteIdsigPath));
        return getDevice().executeShellCommand("pm install-incremental -t -g " + remoteApkPath);
    }

    private String installIncrementalPackageFromResource(String apkFilenameInRes)
            throws Exception {
        final File apkFile = getFileFromResource(apkFilenameInRes);
        final File v4SignatureFile = getFileFromResource(
                apkFilenameInRes + IDSIG_SUFFIX);
        return installPackageIncrementalFromFiles(apkFile, v4SignatureFile);
    }
}
