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

/** Unit tests for {@link SoParser} */
@RunWith(JUnit4.class)
public class SoParserTest {
    // ref: android/frameworks/native/opengl/libs/libEGL.map.txt
    private static final String TEST_NDK_SO = "libEGL.so";
    private static final String TEST_NDK_SO_TXT = "libEGL.so.pb.txt";

    // ref: android/cts/tests/aslr/AndroidTest.xml
    private static final String TEST_CTS_GTEST_EXE = "CtsAslrMallocTestCases32";
    private static final String TEST_CTS_GTEST_EXE_PB_TXT = "CtsAslrMallocTestCases32.pb.txt";

    /**
     * Test {@link SoParser} with an NDK SO file
     *
     * @throws Exception
     */
    @Test
    public void testNdkSo() throws Exception {
        testSoParser(TEST_NDK_SO, TEST_NDK_SO_TXT, true);
    }

    /**
     * Test {@link SoParser} with an CTS GTEST EXE file
     *
     * @throws Exception
     */
    @Test
    public void testCtsGtestExe() throws Exception {
        testSoParser(TEST_CTS_GTEST_EXE, TEST_CTS_GTEST_EXE_PB_TXT, false);
    }

    private void testSoParser(String fileName, String txtProtobufFileName, boolean parseInternalApi)
            throws Exception {
        File aFile = ClassUtils.getResrouceFile(getClass(), fileName);
        SoParser aParser = new SoParser(aFile);
        aParser.setPackageName(fileName);
        aParser.setParseInternalApi(parseInternalApi);
        AppInfo appInfo = aParser.getAppInfo();

        AppInfo.Builder expectedAppInfoBuilder = AppInfo.newBuilder();
        TextFormat.getParser()
                .merge(
                        ClassUtils.getResrouceContentString(getClass(), txtProtobufFileName),
                        expectedAppInfoBuilder);
        assertTrue(
                String.format(
                        "SoParser does not return the same AppInfo of %s as %s.\n%s",
                        fileName, txtProtobufFileName, TextFormat.printToString(appInfo)),
                appInfo.equals(expectedAppInfoBuilder.build()));
    }
}
