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

import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * TODO(b/147496159): add more tests.
 */
public class CtsExtractNativeLibsHostTestBase extends BaseHostJUnit4Test {
    static final String TEST_REMOTE_DIR = "/data/local/tmp/extract_native_libs_test";
    static final String TEST_APK_RESOURCE_PREFIX = "/prebuilt/";
    static final String TEST_HOST_TMP_DIR_PREFIX = "cts_extract_native_libs_host_test";

    static final String TEST_NO_EXTRACT_PKG =
            "com.android.cts.extractnativelibs.app.noextract";
    static final String TEST_NO_EXTRACT_CLASS =
            TEST_NO_EXTRACT_PKG + ".ExtractNativeLibsFalseDeviceTest";
    static final String TEST_NO_EXTRACT_TEST = "testNativeLibsNotExtracted";
    static final String TEST_NO_EXTRACT_APK = "CtsExtractNativeLibsAppFalse.apk";

    static final String TEST_EXTRACT_PKG =
            "com.android.cts.extractnativelibs.app.extract";
    static final String TEST_EXTRACT_CLASS =
            TEST_EXTRACT_PKG + ".ExtractNativeLibsTrueDeviceTest";
    static final String TEST_EXTRACT_TEST = "testNativeLibsExtracted";
    static final String TEST_EXTRACT_APK = "CtsExtractNativeLibsAppTrue.apk";
    static final String TEST_NO_EXTRACT_MISALIGNED_APK =
            "CtsExtractNativeLibsAppFalseWithMisalignedLib.apk";

    /** Setup test dir. */
    @Before
    public void setUp() throws Exception {
        getDevice().executeShellCommand("mkdir " + TEST_REMOTE_DIR);
    }
    /** Uninstall apps after tests. */
    @After
    public void cleanUp() throws Exception {
        uninstallPackage(getDevice(), TEST_NO_EXTRACT_PKG);
        uninstallPackage(getDevice(), TEST_EXTRACT_PKG);
        getDevice().executeShellCommand("rm -r " + TEST_REMOTE_DIR);
    }

    File getFileFromResource(String filenameInResources)
            throws Exception {
        String fullResourceName = TEST_APK_RESOURCE_PREFIX + filenameInResources;
        File tempDir = FileUtil.createTempDir(TEST_HOST_TMP_DIR_PREFIX);
        File file = new File(tempDir, filenameInResources);
        InputStream in = getClass().getResourceAsStream(fullResourceName);
        if (in == null) {
            throw new IllegalArgumentException("Resource not found: " + fullResourceName);
        }
        OutputStream out = new BufferedOutputStream(new FileOutputStream(file));
        byte[] buf = new byte[65536];
        int chunkSize;
        while ((chunkSize = in.read(buf)) != -1) {
            out.write(buf, 0, chunkSize);
        }
        out.close();
        return file;
    }

}
