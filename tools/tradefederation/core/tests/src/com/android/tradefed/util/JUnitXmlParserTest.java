/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.tradefed.util;

import static org.junit.Assert.fail;

import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.util.xml.AbstractXmlParser.ParseException;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.InputStream;
import java.util.HashMap;

/** Simple unit tests for {@link JUnitXmlParser}. */
@RunWith(JUnit4.class)
public class JUnitXmlParserTest {
    private static final String TEST_PARSE_FILE = "JUnitXmlParserTest_testParse.xml";
    private static final String TEST_PARSE_FILE2 = "JUnitXmlParserTest_error.xml";
    private static final String TEST_PARSE_FILE3 = "JUnitXmlParserTest_error2.xml";
    private static final String BAZEL_SH_TEST_XML = "JUnitXmlParserTest_bazelShTest.xml";

    private ITestInvocationListener mMockListener;

    @Before
    public void setUp() {
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
    }

    /** Test behavior when data to parse is empty */
    @Test
    public void testEmptyParse() {
        try {
            new JUnitXmlParser(mMockListener).parse(new ByteArrayInputStream(new byte[0]));
            fail("ParseException not thrown");
        } catch (ParseException e) {
            // expected
        }
    }

    /** Simple success test for xml parsing */
    @Test
    public void testParse() throws ParseException {
        mMockListener.testRunStarted("runName", 3);
        TestDescription test1 = new TestDescription("PassTest", "testPass");
        mMockListener.testStarted(test1);
        mMockListener.testEnded(test1, new HashMap<String, Metric>());

        TestDescription test2 = new TestDescription("PassTest", "testPass2");
        mMockListener.testStarted(test2);
        mMockListener.testEnded(test2, new HashMap<String, Metric>());

        TestDescription test3 = new TestDescription("FailTest", "testFail");
        mMockListener.testStarted(test3);
        mMockListener.testFailed(
                EasyMock.eq(test3), EasyMock.contains("java.lang.NullPointerException"));
        mMockListener.testEnded(test3, new HashMap<String, Metric>());

        mMockListener.testRunEnded(5000L, new HashMap<String, Metric>());
        EasyMock.replay(mMockListener);
        new JUnitXmlParser("runName", mMockListener).parse(extractTestXml(TEST_PARSE_FILE));
        EasyMock.verify(mMockListener);
    }

    /** Test parsing the <error> and <skipped> tag in the junit xml. */
    @Test
    public void testParseErrorAndSkipped() throws ParseException {
        mMockListener.testRunStarted("suiteName", 3);
        TestDescription test1 = new TestDescription("PassTest", "testPass");
        mMockListener.testStarted(test1);
        mMockListener.testEnded(test1, new HashMap<String, Metric>());

        TestDescription test2 = new TestDescription("SkippedTest", "testSkip");
        mMockListener.testStarted(test2);
        mMockListener.testIgnored(test2);
        mMockListener.testEnded(test2, new HashMap<String, Metric>());

        TestDescription test3 = new TestDescription("ErrorTest", "testFail");
        mMockListener.testStarted(test3);
        mMockListener.testFailed(
                EasyMock.eq(test3),
                EasyMock.eq(
                        "java.lang.NullPointerException\n    "
                                + "at FailTest.testFail:65\n        "));
        mMockListener.testEnded(test3, new HashMap<String, Metric>());

        mMockListener.testRunEnded(918686L, new HashMap<String, Metric>());
        EasyMock.replay(mMockListener);
        new JUnitXmlParser(mMockListener).parse(extractTestXml(TEST_PARSE_FILE2));
        EasyMock.verify(mMockListener);
    }

    @Test
    public void testParseError_format() throws ParseException {
        mMockListener.testRunStarted("normal_integration_tests", 1);
        TestDescription test1 = new TestDescription("JUnitXmlParser", "normal_integration_tests");
        mMockListener.testStarted(test1);
        mMockListener.testFailed(EasyMock.eq(test1), EasyMock.eq("exited with error code 134."));
        mMockListener.testEnded(test1, new HashMap<String, Metric>());
        mMockListener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.replay(mMockListener);
        new JUnitXmlParser(mMockListener).parse(extractTestXml(TEST_PARSE_FILE3));
        EasyMock.verify(mMockListener);
    }

    /** Test parsing the XML from an sh_test rule in Bazel. */
    @Test
    public void testParseBazelShTestXml() throws ParseException {
        mMockListener.testRunStarted("//pkg:target", 1);
        TestDescription test = new TestDescription(
                JUnitXmlParser.class.getSimpleName(),  // TODO(b/120500865): remove this kludge
                "pkg/target");
        mMockListener.testStarted(test);
        mMockListener.testEnded(test, new HashMap<String, Metric>());

        mMockListener.testRunEnded(0L, new HashMap<String, Metric>());
        EasyMock.replay(mMockListener);
        new JUnitXmlParser("//pkg:target", mMockListener).parse(extractTestXml(BAZEL_SH_TEST_XML));
        EasyMock.verify(mMockListener);
    }

    private InputStream extractTestXml(String fileName) {
        return getClass().getResourceAsStream(File.separator + "util" +
                File.separator + fileName);
    }
}
