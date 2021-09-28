/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.xml.sax.SAXException;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

import javax.xml.parsers.ParserConfigurationException;

@RunWith(JUnit4.class)
public class GameCoreConfigurationXmlParserTest {
    private static final float EPSILON = Math.ulp(16.666f);
    GameCoreConfigurationXmlParser parser = new GameCoreConfigurationXmlParser();

    @Test
    public void testSingleApkInfo() throws ParserConfigurationException, SAXException, IOException {
        try (InputStream input = new ByteArrayInputStream(
                ("<?xml version=\"1.0\"?>\n"
                        + "<game-core>\n"
                        + "    <certification>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "        </apk>\n"
                        + "    </certification>\n"
                        + "    <apk-info>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "            <fileName>foo.apk</fileName>\n"
                        + "            <packageName>com.foo.test</packageName>\n"
                        + "            <layerName>com.foo.test</layerName>\n"
                        + "        </apk>\n"
                        + "    </apk-info>\n"
                        + "</game-core>\n").getBytes())) {

            GameCoreConfiguration config = parser.parse(input);

            assertEquals(1, config.getCertificationRequirements().size());
            CertificationRequirements requirements = config.getCertificationRequirements().get(0);
            assertEquals(16.666, requirements.getFrameTime(), EPSILON);
            assertEquals(0.0, requirements.getJankRate(), EPSILON);
            assertEquals(-1, requirements.getLoadTime());

            assertEquals(1, config.getApkInfo().size());
            ApkInfo apk = config.getApkInfo().get(0);
            assertEquals("foo", apk.getName());
            assertEquals("foo.apk", apk.getFileName());
            assertEquals("com.foo.test", apk.getPackageName());
            assertEquals(null, apk.getActivityName());
            assertEquals(null, apk.getScript());
            assertEquals(10000, apk.getLoadTime());
            assertEquals(10000, apk.getRunTime());
        }
    }

    @Test
    public void testOptionalFields() throws ParserConfigurationException, SAXException,
            IOException {
        try (InputStream input = new ByteArrayInputStream(
                ("<?xml version=\"1.0\"?>\n"
                        + "<game-core>\n"
                        + "    <apk-info>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "            <fileName>foo.apk</fileName>\n"
                        + "            <packageName>com.foo.test</packageName>\n"
                        + "            <activityName>com.foo.test.MyActivity</activityName>\n"
                        + "            <layerName>com.foo.test</layerName>\n"
                        + "            <script>script.sh</script>\n"
                        + "            <loadTime>21</loadTime>\n"
                        + "        <runTime>42</runTime>\n"
                        + "    </apk>\n"
                        + "</apk-info>\n"
                        + "</game-core>\n").getBytes())) {
            List<ApkInfo> apks = parser.parse(input).getApkInfo();
            assertEquals(1, apks.size());
            ApkInfo apk = apks.get(0);
            assertEquals("foo", apk.getName());
            assertEquals("foo.apk", apk.getFileName());
            assertEquals("com.foo.test", apk.getPackageName());
            assertEquals("com.foo.test.MyActivity", apk.getActivityName());
            assertEquals("script.sh", apk.getScript());
            assertEquals(21, apk.getLoadTime());
            assertEquals(42, apk.getRunTime());
        }
    }

    @Test
    public void testMissingRequiredField() throws ParserConfigurationException, SAXException,
            IOException {
        try (InputStream input = new ByteArrayInputStream(
                ("<?xml version=\"1.0\"?>\n"
                        + "<game-core>\n"
                        + "    <certification>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "        </apk>\n"
                        + "    </certification>\n"
                        + "    <apk-info>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "            <fileName>foo.apk</fileName>\n"
                        + "            <packageName>com.foo.test</packageName>\n"
                        + "        </apk>\n"
                        + "    </apk-info>\n"
                        + "    </game-core>\n").getBytes())) {
            parser.parse(input);
            fail("Expected exception");
        } catch (IllegalArgumentException e) {
            // empty
        }

    }

    @Test
    public void testApkWithArguments() throws ParserConfigurationException, SAXException,
            IOException {
        try (InputStream input = new ByteArrayInputStream(
                ("<?xml version=\"1.0\"?>\n"
                        + "<game-core>\n"
                        + "    <apk-info>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "            <fileName>foo.apk</fileName>\n"
                        + "            <packageName>com.foo.test</packageName>\n"
                        + "            <layerName>com.foo.test</layerName>\n"
                        + "            <args>\n"
                        + "                <key1>value1</key1>"
                        + "                <key2 type=\"int\">value2</key2>"
                        + "            </args>\n"
                        + "        </apk>\n"
                        + "    </apk-info>\n"
                        + "</game-core>\n").getBytes())) {
            List<ApkInfo> apks = parser.parse(input).getApkInfo();
            assertEquals(1, apks.size());
            ApkInfo apk = apks.get(0);
            assertEquals("foo", apk.getName());
            assertEquals("foo.apk", apk.getFileName());
            assertEquals("com.foo.test", apk.getPackageName());
            List<ApkInfo.Argument> args = apk.getArgs();
            assertEquals("key1", args.get(0).getKey());
            assertEquals("value1", args.get(0).getValue());
            assertEquals(ApkInfo.Argument.Type.STRING, args.get(0).getType());
            assertEquals("key2", args.get(1).getKey());
            assertEquals("value2", args.get(1).getValue());
            assertEquals(ApkInfo.Argument.Type.INT, args.get(1).getType());
        }
    }

    @Test
    public void testRequirements() throws ParserConfigurationException, SAXException, IOException {
        try (InputStream input = new ByteArrayInputStream(
                ("<?xml version=\"1.0\"?>\n"
                        + "<game-core>\n"
                        + "    <certification>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "            <frameTime>33.333</frameTime>\n"
                        + "            <jankRate>0.1</jankRate>\n"
                        + "            <loadTime>42</loadTime>\n"
                        + "        </apk>\n"
                        + "    </certification>\n"
                        + "    <apk-info>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "            <fileName>foo.apk</fileName>\n"
                        + "            <packageName>com.foo.test</packageName>\n"
                        + "            <layerName>com.foo.test</layerName>\n"
                        + "        </apk>\n"
                        + "    </apk-info>\n"
                        + "</game-core>\n").getBytes())) {
            GameCoreConfiguration config = parser.parse(input);

            assertEquals(1, config.getCertificationRequirements().size());
            CertificationRequirements requirements = config.getCertificationRequirements().get(0);
            assertEquals(33.333, requirements.getFrameTime(), EPSILON);
            assertEquals(0.1, requirements.getJankRate(), EPSILON);
            assertEquals(42, requirements.getLoadTime());
        }
    }

    @Test
    public void testRequirementNeedsMatchingApk()
            throws ParserConfigurationException, SAXException, IOException {
        try (InputStream input = new ByteArrayInputStream(
                ("<?xml version=\"1.0\"?>\n"
                        + "<game-core>\n"
                        + "    <certification>\n"
                        + "        <apk>\n"
                        + "            <name>foo</name>\n"
                        + "        </apk>\n"
                        + "    </certification>\n"
                        + "    <apk-info>\n"
                        + "    </apk-info>\n"
                        + "    </game-core>\n").getBytes())) {
            parser.parse(input);
            fail("Expected exception");
        } catch (IllegalArgumentException e) {
            // empty
        }
    }
}
