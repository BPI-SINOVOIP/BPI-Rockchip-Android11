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
public class FileParserTest {
    private static final String PB_TXT = ".pb.txt";
    // ref: android/frameworks/base/test-runner
    private static final String TEST_VDEX = "android.test.runner.vdex";
    // ref: android/frameworks
    private static final String TEST_ART = "boot-framework.art";
    // ref: android/frameworks
    private static final String TEST_OAT = "boot-framework.oat";
    // ref: android/frameworks/base/test-runner
    private static final String TEST_ODEX = "android.test.runner.odex";
    // ref: android/build/make/core/Makefile, system_build_prop
    private static final String TEST_BUILD_PROP = "build.prop";
    // ref: android/frameworks/base/data/etc/platform.xml
    private static final String TEST_PLATFORM_XML = "platform.xml";
    // ref: android/frameworks/native/data/etc/android.hardware.vulkan.version-1_0_3.xml
    private static final String TEST_DEVICE_FEATURE_XML = "android.hardware.vulkan.version.xml";

    /**
     * Test {@link VdexParser} with an Vdex file
     *
     * @throws Exception
     */
    @Test
    public void testVdex() throws Exception {
        String fileName = TEST_VDEX;
        File aFile = ClassUtils.getResrouceFile(getClass(), fileName);
        VdexParser aParser = new VdexParser(aFile);

        Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
        fileEntryBuilder.setName(fileName);
        testFileParser(fileName, fileEntryBuilder.build());
    }

    /**
     * Test {@link ArtParser} with an Art file
     *
     * @throws Exception
     */
    @Test
    public void testArt() throws Exception {
        String fileName = TEST_ART;
        File aFile = ClassUtils.getResrouceFile(getClass(), fileName);
        ArtParser aParser = new ArtParser(aFile);

        Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
        fileEntryBuilder.setName(fileName);
        testFileParser(fileName, fileEntryBuilder.build());
    }

    /**
     * Test {@link OatParser} with an Oat file
     *
     * @throws Exception
     */
    @Test
    public void testOat() throws Exception {
        String fileName = TEST_OAT;
        File aFile = ClassUtils.getResrouceFile(getClass(), fileName);
        OatParser aParser = new OatParser(aFile);

        Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
        fileEntryBuilder.setName(fileName);
        testFileParser(fileName, fileEntryBuilder.build());
    }

    /**
     * Test {@link OdexParser} with an Odex file
     *
     * @throws Exception
     */
    @Test
    public void testOdex() throws Exception {
        String fileName = TEST_ODEX;
        File aFile = ClassUtils.getResrouceFile(getClass(), fileName);
        OdexParser aParser = new OdexParser(aFile);

        Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
        fileEntryBuilder.setName(fileName);
        testFileParser(fileName, fileEntryBuilder.build());
    }

    /**
     * Test {@link BuildPropParser} with build.prop file
     *
     * @throws Exception
     */
    @Test
    public void testBuildProp() throws Exception {
        String fileName = TEST_BUILD_PROP;
        File aFile = ClassUtils.getResrouceFile(getClass(), fileName);
        BuildPropParser aParser = new BuildPropParser(aFile);

        Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
        fileEntryBuilder.setName(fileName);
        testFileParser(fileName, fileEntryBuilder.build());
    }

    /**
     * Test {@link XmlParser} with a platform.xml file
     *
     * @throws Exception
     */
    @Test
    public void testPlatformXml() throws Exception {
        testXmlParser(TEST_PLATFORM_XML);
    }

    /**
     * Test {@link XmlParser} with a feature xml file
     *
     * @throws Exception
     */
    @Test
    public void testFeatureXml() throws Exception {
        testXmlParser(TEST_DEVICE_FEATURE_XML);
    }

    private void testXmlParser(String fileName) throws Exception {
        File aFile = ClassUtils.getResrouceFile(getClass(), fileName);
        XmlParser aParser = new XmlParser(aFile);

        Entry.Builder fileEntryBuilder = aParser.getFileEntryBuilder();
        fileEntryBuilder.setName(fileName);
        testFileParser(fileName, fileEntryBuilder.build());
    }

    private void testFileParser(String fileName, Entry fileEntry) throws Exception {
        Entry.Builder expectedEntryBuilder = Entry.newBuilder();
        String txtProtobufFileName = fileName + PB_TXT;
        TextFormat.getParser()
                .merge(
                        ClassUtils.openResourceAsStreamReader(getClass(), txtProtobufFileName),
                        expectedEntryBuilder);
        assertTrue(
                String.format(
                        "Parser does not return the same Entry message of %s with %s.\n%s\n%s",
                        fileName,
                        txtProtobufFileName,
                        TextFormat.printToString(fileEntry),
                        TextFormat.printToString(expectedEntryBuilder)),
                fileEntry.equals(expectedEntryBuilder.build()));
    }
}
