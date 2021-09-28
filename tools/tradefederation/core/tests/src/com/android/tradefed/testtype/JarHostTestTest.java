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
 * limitations under the License.
 */
package com.android.tradefed.testtype;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.DeviceBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner.TestMetrics;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.easymock.Capture;
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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link HostTest} jar handling functionalities. */
@RunWith(JUnit4.class)
public class JarHostTestTest {

    private static final String TEST_JAR1 = "/testtype/testJar1.jar";
    private static final String TEST_JAR2 = "/testtype/testJarJunit4.jar";
    private HostTest mTest;
    private DeviceBuildInfo mStubBuildInfo;
    private TestInformation mTestInfo;
    private File mTestDir = null;
    private ITestInvocationListener mListener;

    @Before
    public void setUp() throws Exception {
        mTest = new HostTest();
        mTestDir = FileUtil.createTempDir("jarhostest");
        mListener = EasyMock.createMock(ITestInvocationListener.class);
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("enable-pretty-logs", "false");
        mStubBuildInfo = new DeviceBuildInfo();
        mStubBuildInfo.setTestsDir(mTestDir, "v1");
        IInvocationContext context = new InvocationContext();
        context.addDeviceBuildInfo("device", mStubBuildInfo);
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
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
    public static class Junit4TestClass {
        public Junit4TestClass() {}

        @org.junit.Test
        public void testPass1() {}
    }

    /**
     * Test class, we have to annotate with full org.junit.Test to avoid name collision in import.
     */
    @RunWith(JUnit4.class)
    public static class Junit4TestClass2 {
        public Junit4TestClass2() {}

        @Rule public TestMetrics metrics = new TestMetrics();

        @org.junit.Test
        public void testPass2() {
            metrics.addTestMetric("key", "value");
        }
    }

    /** Test that {@link HostTest#split(int)} can split classes coming from a jar. */
    @Test
    public void testSplit_withJar() throws Exception {
        File testJar = getJarResource(TEST_JAR1, mTestDir);
        mTest = new HostTestLoader(testJar);
        mTest.setBuild(mStubBuildInfo);
        ITestDevice device = EasyMock.createNiceMock(ITestDevice.class);
        mTest.setDevice(device);
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("enable-pretty-logs", "false");
        setter.setOptionValue("jar", testJar.getName());
        // full class count without sharding
        mTest.setTestInformation(mTestInfo);
        assertEquals(238, mTest.countTestCases());

        List<IRemoteTest> tests = new ArrayList<>(mTest.split(5, mTestInfo));
        // HostTest sharding does not respect the shard-count hint (expected)
        assertEquals(8, tests.size());

        // 5 shards total the number of tests.
        int total = 0;
        IRemoteTest shard1 = tests.get(0);
        assertTrue(shard1 instanceof HostTest);
        ((HostTest) shard1).setBuild(new BuildInfo());
        ((HostTest) shard1).setDevice(device);
        assertEquals(28, ((HostTest) shard1).countTestCases());
        total += ((HostTest) shard1).countTestCases();

        IRemoteTest shard2 = tests.get(1);
        assertTrue(shard2 instanceof HostTest);
        ((HostTest) shard2).setBuild(new BuildInfo());
        ((HostTest) shard2).setDevice(device);
        assertEquals(30, ((HostTest) shard2).countTestCases());
        total += ((HostTest) shard2).countTestCases();

        IRemoteTest shard3 = tests.get(2);
        assertTrue(shard3 instanceof HostTest);
        ((HostTest) shard3).setBuild(new BuildInfo());
        ((HostTest) shard3).setDevice(device);
        assertEquals(30, ((HostTest) shard3).countTestCases());
        total += ((HostTest) shard3).countTestCases();

        IRemoteTest shard4 = tests.get(3);
        assertTrue(shard4 instanceof HostTest);
        ((HostTest) shard4).setBuild(new BuildInfo());
        ((HostTest) shard4).setDevice(device);
        assertEquals(30, ((HostTest) shard4).countTestCases());
        total += ((HostTest) shard4).countTestCases();

        IRemoteTest shard5 = tests.get(4);
        assertTrue(shard5 instanceof HostTest);
        ((HostTest) shard5).setBuild(new BuildInfo());
        ((HostTest) shard5).setDevice(device);
        assertEquals(30, ((HostTest) shard5).countTestCases());
        total += ((HostTest) shard5).countTestCases();

        IRemoteTest shard6 = tests.get(5);
        assertTrue(shard6 instanceof HostTest);
        ((HostTest) shard6).setBuild(new BuildInfo());
        ((HostTest) shard6).setDevice(device);
        assertEquals(30, ((HostTest) shard6).countTestCases());
        total += ((HostTest) shard6).countTestCases();

        IRemoteTest shard7 = tests.get(6);
        assertTrue(shard7 instanceof HostTest);
        ((HostTest) shard7).setBuild(new BuildInfo());
        ((HostTest) shard7).setDevice(device);
        assertEquals(30, ((HostTest) shard7).countTestCases());
        total += ((HostTest) shard7).countTestCases();

        IRemoteTest shard8 = tests.get(7);
        assertTrue(shard8 instanceof HostTest);
        ((HostTest) shard8).setBuild(new BuildInfo());
        ((HostTest) shard8).setDevice(device);
        assertEquals(30, ((HostTest) shard8).countTestCases());
        total += ((HostTest) shard8).countTestCases();

        assertEquals(238, total);
    }

    /** Avoid collision between --class and --jar when they reference common classes. */
    @Test
    public void testSplit_countWithFilter() throws Exception {
        File testJar = getJarResource(TEST_JAR1, mTestDir);
        mTest = new HostTestLoader(testJar);
        mTest.setBuild(mStubBuildInfo);
        ITestDevice device = EasyMock.createNiceMock(ITestDevice.class);
        mTest.setDevice(device);
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("enable-pretty-logs", "false");
        setter.setOptionValue("jar", testJar.getName());
        // Explicitly request a class from the jar
        setter.setOptionValue("class", "android.ui.cts.TestClass8");
        // full class count without sharding should be 238
        mTest.setTestInformation(mTestInfo);
        assertEquals(238, mTest.countTestCases());
    }

    /**
     * Testable version of {@link HostTest} that allows adding jar to classpath for testing purpose.
     */
    public static class HostTestLoader extends HostTest {

        private static File mTestJar;

        public HostTestLoader() {}

        public HostTestLoader(File jar) {
            mTestJar = jar;
        }

        @Override
        protected ClassLoader getClassLoader() {
            ClassLoader child = super.getClassLoader();
            try {
                child =
                        new URLClassLoader(
                                Arrays.asList(mTestJar.toURI().toURL()).toArray(new URL[] {}),
                                super.getClassLoader());
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
        mTest.setBuild(new BuildInfo());

        mListener.testRunStarted(HostTest.class.getName(), 0);
        Capture<FailureDescription> captured = new Capture<>();
        mListener.testRunFailed(EasyMock.capture(captured));
        mListener.testRunEnded(0L, new HashMap<String, Metric>());

        EasyMock.replay(mListener);
        try {
            mTest.run(mTestInfo, mListener);
            fail("Should have thrown an exception.");
        } catch (IllegalArgumentException expected) {
            // expected
            assertEquals(
                    "java.io.FileNotFoundException: "
                            + "Could not find an artifact file associated with "
                            + "thisjardoesnotexistatall.jar",
                    expected.getMessage());
        }
        EasyMock.verify(mListener);
        assertTrue(
                captured.getValue()
                        .getErrorMessage()
                        .contains(
                                "java.io.FileNotFoundException: "
                                        + "Could not find an artifact file associated with "
                                        + "thisjardoesnotexistatall.jar"));
    }

    /** Test that metrics from tests in JarHost are reported and accounted for. */
    @Test
    public void testJarHostMetrics() throws Exception {
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("class", Junit4TestClass2.class.getName());
        mListener.testRunStarted(EasyMock.anyObject(), EasyMock.eq(1));
        TestDescription tid = new TestDescription(Junit4TestClass2.class.getName(), "testPass2");
        mListener.testStarted(EasyMock.eq(tid));
        Map<String, String> metrics = new HashMap<>();
        metrics.put("key", "value");
        mListener.testEnded(
                EasyMock.eq(tid), EasyMock.eq(TfMetricProtoUtil.upgradeConvert(metrics)));
        mListener.testRunEnded(EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());
        EasyMock.replay(mListener);
        mTest.run(mTestInfo, mListener);
        EasyMock.verify(mListener);
    }

    @Test
    public void testRunWithJar() throws Exception {
        File testJar = getJarResource(TEST_JAR2, mTestDir);
        mTest = new HostTest();
        mTest.setBuild(mStubBuildInfo);
        ITestDevice device = EasyMock.createNiceMock(ITestDevice.class);
        mTest.setDevice(device);
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("enable-pretty-logs", "false");
        setter.setOptionValue("jar", testJar.getName());
        // full class count without sharding
        mTest.setTestInformation(mTestInfo);
        assertEquals(2, mTest.countTestCases());

        mListener.testRunStarted("com.android.tradefed.JUnit4TfUnitTest", 2);
        TestDescription testOne =
                new TestDescription("com.android.tradefed.JUnit4TfUnitTest", "testOne");
        TestDescription testTwo =
                new TestDescription("com.android.tradefed.JUnit4TfUnitTest", "testTwo");
        mListener.testStarted(testOne);
        mListener.testEnded(EasyMock.eq(testOne), EasyMock.<HashMap<String, Metric>>anyObject());
        mListener.testStarted(testTwo);
        mListener.testEnded(EasyMock.eq(testTwo), EasyMock.<HashMap<String, Metric>>anyObject());
        mListener.testRunEnded(EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        EasyMock.replay(mListener);
        mTest.run(mTestInfo, mListener);
        EasyMock.verify(mListener);
    }

    @Test
    public void testRunWithClassFromExternalJar() throws Exception {
        File testJar = getJarResource(TEST_JAR2, mTestDir);
        mTestInfo
                .getContext()
                .addInvocationAttribute(
                        ModuleDefinition.MODULE_NAME, FileUtil.getBaseName(testJar.getName()));
        mTest = new HostTest();
        mTest.setBuild(mStubBuildInfo);
        ITestDevice device = EasyMock.createNiceMock(ITestDevice.class);
        mTest.setDevice(device);
        OptionSetter setter = new OptionSetter(mTest);
        setter.setOptionValue("enable-pretty-logs", "false");
        setter.setOptionValue("class", "com.android.tradefed.JUnit4TfUnitTest");
        // full class count without sharding
        mTest.setTestInformation(mTestInfo);
        assertEquals(2, mTest.countTestCases());

        mListener.testRunStarted("com.android.tradefed.JUnit4TfUnitTest", 2);
        TestDescription testOne =
                new TestDescription("com.android.tradefed.JUnit4TfUnitTest", "testOne");
        TestDescription testTwo =
                new TestDescription("com.android.tradefed.JUnit4TfUnitTest", "testTwo");
        mListener.testStarted(testOne);
        mListener.testEnded(EasyMock.eq(testOne), EasyMock.<HashMap<String, Metric>>anyObject());
        mListener.testStarted(testTwo);
        mListener.testEnded(EasyMock.eq(testTwo), EasyMock.<HashMap<String, Metric>>anyObject());
        mListener.testRunEnded(EasyMock.anyLong(), EasyMock.<HashMap<String, Metric>>anyObject());

        EasyMock.replay(mListener);
        mTest.run(mTestInfo, mListener);
        EasyMock.verify(mListener);
    }
}
