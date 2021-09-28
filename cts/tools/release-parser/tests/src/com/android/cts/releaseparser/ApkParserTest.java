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
 * limitations under the License
 */

package com.android.cts.releaseparser;

import com.android.cts.releaseparser.ReleaseProto.*;
import com.google.protobuf.TextFormat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

import static org.junit.Assert.*;

/** Unit tests for {@link ApkParser} */
@RunWith(JUnit4.class)
public class ApkParserTest {
    private static final String PB_TXT = ".pb.txt";
    // HelloActivity.apk's source code: android/cts/tests/tests/jni/AndroidManifest.xml
    private static final String TEST_SIMPLE_APK = "HelloActivity.apk";

    // Shell.apk's source code:
    // android/frameworks/base/packages/Shell/AndroidManifest.xml
    private static final String TEST_SYS_APK = "Shell.apk";

    // CtsJniTestCases.apk's source code:
    // android/development/samples/HelloActivity/AndroidManifest.xml
    private static final String TEST_SO_APK = "CtsJniTestCases.apk";

    /**
     * Test {@link ApkParser} with an simple APK
     *
     * @throws Exception
     */
    @Test
    public void testSimpleApk() throws Exception {
        testApkParser(TEST_SIMPLE_APK);
    }

    /**
     * Test {@link ApkParser} with an Sys(priv-app) APK
     *
     * @throws Exception
     */
    @Test
    public void testSysApk() throws Exception {
        testApkParser(TEST_SYS_APK);
    }

    /**
     * Test {@link ApkParser} with an APK with Shared Objects/Nactive Code
     *
     * @throws Exception
     */
    @Test
    public void testSoApk() throws Exception {
        testApkParser(TEST_SO_APK);
    }

    private void testApkParser(String fileName) throws Exception {
        File apkFile = ClassUtils.getResrouceFile(getClass(), fileName);
        ApkParser aParser = new ApkParser(apkFile);
        Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
        fileEntryBuilder.setName(fileName);

        Entry fileEntry = fileEntryBuilder.build();
        Entry.Builder expectedfileEntryBuilder = Entry.newBuilder();

        String txtProtobufFileName = fileName + PB_TXT;
        TextFormat.getParser()
                .merge(
                        ClassUtils.openResourceAsStreamReader(getClass(), txtProtobufFileName),
                        expectedfileEntryBuilder);
        assertTrue(
                String.format(
                        "ApkParser does not return the same Entry of %s as %s.\n%s",
                        fileName, txtProtobufFileName, TextFormat.printToString(fileEntry)),
                fileEntry.equals(expectedfileEntryBuilder.build()));
    }
}
