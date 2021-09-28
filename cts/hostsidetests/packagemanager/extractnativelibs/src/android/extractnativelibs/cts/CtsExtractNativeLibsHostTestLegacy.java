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
import static org.junit.Assert.assertTrue;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * Host test to install test apps and run device tests to verify the effect of extractNativeLibs.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CtsExtractNativeLibsHostTestLegacy extends CtsExtractNativeLibsHostTestBase {
    /** Test with a app that has extractNativeLibs=false. */
    @Test
    @AppModeFull
    public void testNoExtractNativeLibsLegacy() throws Exception {
        installPackage(TEST_NO_EXTRACT_APK);
        assertTrue(isPackageInstalled(TEST_NO_EXTRACT_PKG));
        assertTrue(runDeviceTests(
                TEST_NO_EXTRACT_PKG, TEST_NO_EXTRACT_CLASS, TEST_NO_EXTRACT_TEST));
    }

    /** Test with a app that has extractNativeLibs=true. */
    @Test
    @AppModeFull
    public void testExtractNativeLibsLegacy() throws Exception {
        installPackage(TEST_EXTRACT_APK);
        assertTrue(isPackageInstalled(TEST_EXTRACT_PKG));
        assertTrue(runDeviceTests(
                TEST_EXTRACT_PKG, TEST_EXTRACT_CLASS, TEST_EXTRACT_TEST));
    }

    /** Test with a app that has extractNativeLibs=false but with mis-aligned lib files */
    @Test
    @AppModeFull
    public void testNoExtractNativeLibsFails() throws Exception {
        File apk = getFileFromResource(TEST_NO_EXTRACT_MISALIGNED_APK);
        String result = getDevice().installPackage(apk, false, true, "");
        assertTrue(result.contains("Failed to extract native libraries"));
        assertFalse(isPackageInstalled(TEST_NO_EXTRACT_PKG));
    }

}
