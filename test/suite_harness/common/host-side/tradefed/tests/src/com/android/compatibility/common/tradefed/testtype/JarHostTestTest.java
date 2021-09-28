/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.compatibility.common.tradefed.testtype;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner.TestMetrics;
import com.android.tradefed.testtype.HostTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Unit tests for {@link JarHostTest}.
 */
@RunWith(JUnit4.class)
public class JarHostTestTest {

    private static final String TEST_JAR1 = "/testtype/testJar1.jar";
    private JarHostTest mTest;
    private DeviceBuildInfo mStubBuildInfo;
    private TestInformation mTestInfo;
    private File mTestDir = null;
    private ITestInvocationListener mListener;

    /**
     * More testable version of {@link JarHostTest}
     */
    public static class JarHostTestable extends JarHostTest {

        public static File mTestDir;
        public JarHostTestable() {}

        public JarHostTestable(File testDir) {
            mTestDir = testDir;
        }
    }

    @Before
    public void setUp() throws Exception {
        mTest = new JarHostTest();
        mTestDir = FileUtil.createTempDir("jarhostest");
        mListener = EasyMock.createMock(ITestInvocationListener.class);
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("enable-pretty-logs", "false");
        mStubBuildInfo = new DeviceBuildInfo();
        mStubBuildInfo.setTestsDir(mTestDir, "v1");
        mTestInfo = TestInformation.newBuilder().build();
        mTestInfo.executionFiles().put(FilesKey.TESTS_DIRECTORY, mTestDir);
    }

    @After
    public void tearDown() throws Exception {
        FileUtil.recursiveDelete(mTestDir);
    }

    /**
     * Helper to read a file from the res/testtype directory and return it.
     *
     * @param filename the name of the file in the res/testtype directory
     * @param parentDir dir where to put the jar. Null if in default tmp directory.
     * @return the extracted jar file.
     */
    protected File getJarResource(String filename, File parentDir) throws IOException {
        InputStream jarFileStream = getClass().getResourceAsStream(filename);
        File jarFile = FileUtil.createTempFile("test", ".jar", parentDir);
        FileUtil.writeToFile(jarFileStream, jarFile);
        return jarFile;
    }

    /**
     * Test class, we have to annotate with full org.junit.Test to avoid name collision in import.
     */
    @RunWith(JUnit4.class)
    public static class Junit4TestClass  {
        public Junit4TestClass() {}
        @org.junit.Test
        public void testPass1() {}
    }

    /**
     * Test class, we have to annotate with full org.junit.Test to avoid name collision in import.
     */
    @RunWith(JUnit4.class)
    public static class Junit4TestClass2  {
        public Junit4TestClass2() {}
        @Rule public TestMetrics metrics = new TestMetrics();

        @org.junit.Test
        public void testPass2() {
            metrics.addTestMetric("key", "value");
        }
    }

    /**
     * Test that {@link JarHostTest#split()} inherited from {@link HostTest} is still good.
     */
    @Test
    public void testSplit_withoutJar() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("class", "com.android.compatibility.common.tradefed.testtype."
                + "JarHostTestTest$Junit4TestClass");
        setter.setOptionValue("class", "com.android.compatibility.common.tradefed.testtype."
                + "JarHostTestTest$Junit4TestClass2");
        // sharCount is ignored; will split by number of classes
        List<IRemoteTest> res = (List<IRemoteTest>) mTest.split(1, mTestInfo);
        assertEquals(2, res.size());
        assertTrue(res.get(0) instanceof JarHostTest);
        assertTrue(res.get(1) instanceof JarHostTest);
    }

    /**
     * Test that {@link JarHostTest#split()} can split classes coming from a jar.
     */
    @Test
    public void testSplit_withJar() throws Exception {
        File testJar = getJarResource(TEST_JAR1, mTestDir);
        mTest = new JarHostTestable(mTestDir);
        mTest.setBuild(mStubBuildInfo);
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("enable-pretty-logs", "false");
        setter.setOptionValue("jar", testJar.getName());
        // sharCount is ignored; will split by number of classes
        List<IRemoteTest> res = (List<IRemoteTest>) mTest.split(1, mTestInfo);
        assertEquals(2, res.size());
        assertTrue(res.get(0) instanceof JarHostTest);
        assertEquals("[android.ui.cts.TaskSwitchingTest]",
                ((JarHostTest)res.get(0)).getClassNames().toString());
        assertTrue(res.get(1) instanceof JarHostTest);
        assertEquals("[android.ui.cts.InstallTimeTest]",
                ((JarHostTest)res.get(1)).getClassNames().toString());
    }

    /**
     * Testable version of {@link JarHostTest} that allows adding jar to classpath for testing
     * purpose.
     */
    public static class JarHostTestLoader extends JarHostTestable {

        private static File mTestJar;

        public JarHostTestLoader() {}

        public JarHostTestLoader(File testDir, File jar) {
            super(testDir);
            mTestJar = jar;
        }

        @Override
        protected ClassLoader getClassLoader() {
            ClassLoader child = super.getClassLoader();
            try {
                child = new URLClassLoader(Arrays.asList(mTestJar.toURI().toURL())
                        .toArray(new URL[]{}), super.getClassLoader());
            } catch (MalformedURLException e) {
                CLog.e(e);
            }
            return child;
        }
    }

    /**
     * If a jar file is not found, the countTest will fail but we still want to report a
     * testRunStart and End pair for results.
     */
    @Test
    public void testCountTestFails() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("jar", "thisjardoesnotexistatall.jar");
        mListener.testRunStarted(EasyMock.anyObject(), EasyMock.eq(0));
        mListener.testRunEnded(EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.replay(mListener);
        try {
            mTest.run(mTestInfo, mListener);
            fail("Should have thrown an exception.");
        } catch(RuntimeException expected) {
            // expected
        }
        EasyMock.verify(mListener);
    }

    /**
     * Test that metrics from tests in JarHost are reported and accounted for.
     */
    @Test
    public void testJarHostMetrics() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("class", "com.android.compatibility.common.tradefed.testtype."
                + "JarHostTestTest$Junit4TestClass2");
        mListener.testRunStarted(EasyMock.anyObject(), EasyMock.eq(1));
        TestDescription tid = new TestDescription("com.android.compatibility.common.tradefed."
                + "testtype.JarHostTestTest$Junit4TestClass2", "testPass2");
        mListener.testStarted(EasyMock.eq(tid), EasyMock.anyLong());
        Map<String, String> metrics = new HashMap<>();
        metrics.put("key", "value");
        mListener.testEnded(EasyMock.eq(tid), EasyMock.anyLong(),
                EasyMock.eq(TfMetricProtoUtil.upgradeConvert(metrics)));
        mListener.testRunEnded(EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.replay(mListener);
        mTest.run(mTestInfo, mListener);
        EasyMock.verify(mListener);
    }
}
