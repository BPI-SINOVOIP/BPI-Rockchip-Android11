/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.tradefed.device.metric;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.Mockito.times;

import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.HostTest;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.FileUtil;

import org.easymock.EasyMock;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.lang.annotation.Annotation;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Unit tests for {@link BaseDeviceMetricCollector}. */
@RunWith(JUnit4.class)
public class BaseDeviceMetricCollectorTest {

    private BaseDeviceMetricCollector mBase;
    private IInvocationContext mContext;
    private ITestInvocationListener mMockListener;
    private TestInformation mTestInfo;
    @Captor private ArgumentCaptor<HashMap<String, Metric>> mCapturedMetrics;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTestInfo = TestInformation.newBuilder().build();
        mBase = new BaseDeviceMetricCollector();
        mContext = new InvocationContext();
        mMockListener = Mockito.mock(ITestInvocationListener.class);
    }

    @Test
    public void testInitAndForwarding() {
        mBase.init(mContext, mMockListener);
        mBase.invocationStarted(mContext);
        mBase.testModuleStarted(mContext);
        mBase.testRunStarted("testRun", 1);
        TestDescription test = new TestDescription("class", "method");
        mBase.testStarted(test);
        mBase.testLog("dataname", LogDataType.TEXT, new ByteArrayInputStreamSource("".getBytes()));
        mBase.testFailed(test, "trace");
        mBase.testAssumptionFailure(test, "trace");
        mBase.testIgnored(test);
        mBase.testEnded(test, new HashMap<String, Metric>());
        mBase.testRunFailed("test run failed");
        mBase.testRunStopped(0L);
        mBase.testRunEnded(0L, new HashMap<String, Metric>());
        mBase.testModuleEnded();
        mBase.invocationFailed(new Throwable());
        mBase.invocationEnded(0L);

        Mockito.verify(mMockListener, times(1)).invocationStarted(Mockito.any());
        Mockito.verify(mMockListener, times(1)).testModuleStarted(Mockito.any());
        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq("testRun"), Mockito.eq(1), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1))
                .testLog(Mockito.eq("dataname"), Mockito.eq(LogDataType.TEXT), Mockito.any());
        Mockito.verify(mMockListener, times(1)).testFailed(test, "trace");
        Mockito.verify(mMockListener, times(1)).testAssumptionFailure(test, "trace");
        Mockito.verify(mMockListener, times(1)).testIgnored(test);
        Mockito.verify(mMockListener, times(1))
                .testEnded(
                        Mockito.eq(test),
                        Mockito.anyLong(),
                        Mockito.eq(new HashMap<String, Metric>()));
        Mockito.verify(mMockListener, times(1)).testRunFailed("test run failed");
        Mockito.verify(mMockListener, times(1)).testRunStopped(0L);
        Mockito.verify(mMockListener, times(1)).testRunEnded(0L, new HashMap<String, Metric>());
        Mockito.verify(mMockListener, times(1)).testModuleEnded();
        Mockito.verify(mMockListener, times(1)).invocationFailed(Mockito.<Throwable>any());
        Mockito.verify(mMockListener, times(1)).invocationEnded(0L);

        Assert.assertSame(mMockListener, mBase.getInvocationListener());
        Assert.assertEquals(0, mBase.getDevices().size());
        Assert.assertEquals(0, mBase.getBuildInfos().size());
    }

    /** Test that multiple call to init are rejected. */
    @Test
    public void testMultiInit() {
        mBase.init(mContext, mMockListener);
        try {
            mBase.init(mContext, mMockListener);
            fail("Should have thrown an exception.");
        } catch (IllegalStateException expected) {
            // Expected
        }
    }

    /**
     * Test to ensure that the forwarding of events continues even if an exception occurs in the
     * collection.
     */
    @Test
    public void testForwarding_withException() {
        mBase =
                new BaseDeviceMetricCollector() {
                    @Override
                    public void onTestRunStart(DeviceMetricData runData) {
                        throw new RuntimeException("Failed onTestRunStart.");
                    }

                    @Override
                    public void onTestRunEnd(
                            DeviceMetricData runData, final Map<String, Metric> currentRunMetrics) {
                        throw new RuntimeException("Failed onTestRunEnd");
                    }

                    @Override
                    public void onTestStart(DeviceMetricData testData) {
                        throw new RuntimeException("Failed onTestStart");
                    }

                    @Override
                    public void onTestEnd(
                            DeviceMetricData testData,
                            final Map<String, Metric> currentTestCaseMetrics) {
                        throw new RuntimeException("Failed onTestEnd");
                    }
                };

        mBase.init(mContext, mMockListener);
        mBase.invocationStarted(mContext);
        mBase.testRunStarted("testRun", 1);
        TestDescription test = new TestDescription("class", "method");
        mBase.testStarted(test);
        mBase.testLog("dataname", LogDataType.TEXT, new ByteArrayInputStreamSource("".getBytes()));
        mBase.testFailed(test, "trace");
        mBase.testAssumptionFailure(test, "trace");
        mBase.testIgnored(test);
        mBase.testEnded(test, new HashMap<String, Metric>());
        mBase.testRunFailed("test run failed");
        mBase.testRunStopped(0L);
        mBase.testRunEnded(0L, new HashMap<String, Metric>());
        mBase.invocationFailed(new Throwable());
        mBase.invocationEnded(0L);

        Mockito.verify(mMockListener, times(1)).invocationStarted(Mockito.any());
        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq("testRun"), Mockito.eq(1), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1))
                .testLog(Mockito.eq("dataname"), Mockito.eq(LogDataType.TEXT), Mockito.any());
        Mockito.verify(mMockListener, times(1)).testFailed(test, "trace");
        Mockito.verify(mMockListener, times(1)).testAssumptionFailure(test, "trace");
        Mockito.verify(mMockListener, times(1)).testIgnored(test);
        Mockito.verify(mMockListener, times(1))
                .testEnded(
                        Mockito.eq(test),
                        Mockito.anyLong(),
                        Mockito.eq(new HashMap<String, Metric>()));
        Mockito.verify(mMockListener, times(1)).testRunFailed("test run failed");
        Mockito.verify(mMockListener, times(1)).testRunStopped(0L);
        Mockito.verify(mMockListener, times(1)).testRunEnded(0L, new HashMap<String, Metric>());
        Mockito.verify(mMockListener, times(1)).invocationFailed(Mockito.<Throwable>any());
        Mockito.verify(mMockListener, times(1)).invocationEnded(0L);

        Assert.assertSame(mMockListener, mBase.getInvocationListener());
        Assert.assertEquals(0, mBase.getDevices().size());
        Assert.assertEquals(0, mBase.getBuildInfos().size());
    }

    /**
     * Test that if we specify an include group and the test case does not have any group, we do not
     * run the collector against it.
     */
    @Test
    public void testIncludeTestCase_optionGroup_noAnnotation() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_INCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, null, false);
    }

    /**
     * Test that if we specify an include group and the test case does not match it, we do not run
     * the collector against it.
     */
    @Test
    public void testIncludeTestCase_optionGroup_differentAnnotationGroup() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_INCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, new TestAnnotation("group1"), false);
    }

    /**
     * Test that if we specify an include group and the test case match it, we run the collector
     * against it.
     */
    @Test
    public void testIncludeTestCase_optionGroup_sameAnnotationGroup() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_INCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, new TestAnnotation("group"), true);
    }

    /**
     * Test that if we specify an include group and one of the test case group match it, we run the
     * collector against it.
     */
    @Test
    public void testIncludeTestCase_optionGroup_multiGroup() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_INCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, new TestAnnotation("group1,group2,group"), true);
    }

    /**
     * Test that if we do not specify an include group and the test case have some, we run the
     * collector against it since there is no filtering.
     */
    @Test
    public void testIncludeTestCase_noOptionGroup_multiGroup() throws Exception {
        mBase = new TwoMetricsBaseCollector();

        verifyFiltering(mBase, new TestAnnotation("group1,group2,group"), true);
    }

    /**
     * Test that if we exclude a group and the test case does not belong to any, we run the
     * collection against it.
     */
    @Test
    public void testExcludeTestCase_optionGroup_noAnnotation() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_EXCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, null, true);
    }

    /**
     * Test that if we exclude a group and the test case belong to it, we do not run the collection
     * against it.
     */
    @Test
    public void testExcludeTestCase_optionGroup_sameGroup() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_EXCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, new TestAnnotation("group"), false);
    }

    /**
     * Test that if we exclude a group and the test case belong to it, we do not run the collection
     * against it.
     */
    @Test
    public void testExcludeTestCase_optionGroup_multiGroup() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_EXCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, new TestAnnotation("group1,group2,group"), false);
    }

    /** Test that if we exclude and include a group. It will be excluded. */
    @Test
    public void testExcludeTestCase_includeAndExclude() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_EXCLUDE_GROUP_OPTION, "group");
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_INCLUDE_GROUP_OPTION, "group");

        verifyFiltering(mBase, new TestAnnotation("group1,group2,group"), false);
    }

    /**
     * Validate that the filtering allows or not the collection of metrics based on the annotation
     * of the test case.
     */
    private void verifyFiltering(
            BaseDeviceMetricCollector base, TestAnnotation annot, boolean hasMetric) {
        base.init(mContext, mMockListener);
        base.invocationStarted(mContext);
        base.testRunStarted("testRun", 1);
        TestDescription test = null;
        if (annot != null) {
            test = new TestDescription("class", "method", annot);
        } else {
            test = new TestDescription("class", "method");
        }
        base.testStarted(test);
        base.testEnded(test, new HashMap<String, Metric>());
        base.testRunEnded(0L, new HashMap<String, Metric>());
        base.invocationEnded(0L);

        Mockito.verify(mMockListener, times(1)).invocationStarted(Mockito.any());
        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq("testRun"), Mockito.eq(1), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test), Mockito.anyLong());
        // Metrics should have been skipped, so the map should be empty.
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test), Mockito.anyLong(), mCapturedMetrics.capture());
        Mockito.verify(mMockListener, times(1)).testRunEnded(0L, new HashMap<String, Metric>());
        Mockito.verify(mMockListener, times(1)).invocationEnded(0L);

        Assert.assertSame(mMockListener, mBase.getInvocationListener());
        Assert.assertEquals(0, mBase.getDevices().size());
        Assert.assertEquals(0, mBase.getBuildInfos().size());

        if (hasMetric) {
            // One metric for testStart and one for testEnd
            Assert.assertEquals(2, mCapturedMetrics.getValue().size());
        } else {
            Assert.assertEquals(0, mCapturedMetrics.getValue().size());
        }
    }

    /** Test annotation to test filtering of test cases. */
    public class TestAnnotation implements MetricOption {

        private String mGroup;

        public TestAnnotation(String group) {
            mGroup = group;
        }

        @Override
        public Class<? extends Annotation> annotationType() {
            return MetricOption.class;
        }

        @Override
        public String group() {
            return mGroup;
        }
    }

    /** Test class for testing filtering of metrics. */
    public class TwoMetricsBaseCollector extends BaseDeviceMetricCollector {
        @Override
        public void onTestStart(DeviceMetricData testData) {
            testData.addMetric(
                    "onteststart",
                    Metric.newBuilder()
                            .setMeasurements(Measurements.newBuilder().setSingleString("value1")));
        }

        @Override
        public void onTestEnd(
                DeviceMetricData testData, final Map<String, Metric> currentTestCaseMetrics) {
            testData.addMetric(
                    "ontestend",
                    Metric.newBuilder()
                            .setMeasurements(Measurements.newBuilder().setSingleString("value1")));
        }
    }

    /** Test class actually annotated with groups. */
    @RunWith(JUnit4.class)
    public static class TestRunAnnotated {

        @MetricOption(group = "group1")
        @Test
        public void testOne() {}

        @MetricOption(group = "group1,group2")
        @Test
        public void testTwo() {}

        @MetricOption(group = "group2")
        @Test
        public void testThree() {}
    }

    /**
     * Test when actually running an annotated class that the proper groups only are collected even
     * if the test ran.
     */
    @Test
    public void testActualRunAnnotated_include() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_INCLUDE_GROUP_OPTION, "group1");
        mBase.init(mContext, mMockListener);
        mBase.invocationStarted(mContext);

        HostTest host = new HostTest();
        OptionSetter setterHost = new OptionSetter(host);
        setterHost.setOptionValue("class", TestRunAnnotated.class.getName());
        setterHost.setOptionValue("enable-pretty-logs", "false");

        host.run(mTestInfo, mBase);

        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq(TestRunAnnotated.class.getName()),
                        Mockito.eq(3),
                        Mockito.eq(0),
                        Mockito.anyLong());
        TestDescription test1 = new TestDescription(TestRunAnnotated.class.getName(), "testOne");
        TestDescription test2 = new TestDescription(TestRunAnnotated.class.getName(), "testTwo");
        TestDescription test3 = new TestDescription(TestRunAnnotated.class.getName(), "testThree");

        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test1), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test1), Mockito.anyLong(), mCapturedMetrics.capture());

        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test2), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test2), Mockito.anyLong(), mCapturedMetrics.capture());

        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test3), Mockito.anyLong());
        // Metrics should have been skipped, so the map should be empty.
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test3), Mockito.anyLong(), mCapturedMetrics.capture());
        Mockito.verify(mMockListener, times(1))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());

        List<HashMap<String, Metric>> allValues = mCapturedMetrics.getAllValues();
        // For test1
        assertTrue(allValues.get(0).containsKey("onteststart"));
        assertTrue(allValues.get(0).containsKey("ontestend"));
        // For test2
        assertTrue(allValues.get(1).containsKey("onteststart"));
        assertTrue(allValues.get(1).containsKey("ontestend"));
        // For test3: should not have any metrics.
        assertFalse(allValues.get(2).containsKey("onteststart"));
        assertFalse(allValues.get(2).containsKey("ontestend"));
    }

    /** Test running an actual test annotated for metrics for an exclusion scenario. */
    @Test
    public void testActualRunAnnotated_exclude() throws Exception {
        mBase = new TwoMetricsBaseCollector();
        OptionSetter setter = new OptionSetter(mBase);
        setter.setOptionValue(BaseDeviceMetricCollector.TEST_CASE_EXCLUDE_GROUP_OPTION, "group1");
        mBase.init(mContext, mMockListener);
        mBase.invocationStarted(mContext);

        HostTest host = new HostTest();
        OptionSetter setterHost = new OptionSetter(host);
        setterHost.setOptionValue("class", TestRunAnnotated.class.getName());
        setterHost.setOptionValue("enable-pretty-logs", "false");

        host.run(mTestInfo, mBase);

        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq(TestRunAnnotated.class.getName()),
                        Mockito.eq(3),
                        Mockito.eq(0),
                        Mockito.anyLong());
        TestDescription test1 = new TestDescription(TestRunAnnotated.class.getName(), "testOne");
        TestDescription test2 = new TestDescription(TestRunAnnotated.class.getName(), "testTwo");
        TestDescription test3 = new TestDescription(TestRunAnnotated.class.getName(), "testThree");

        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test1), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test1), Mockito.anyLong(), mCapturedMetrics.capture());

        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test2), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test2), Mockito.anyLong(), mCapturedMetrics.capture());

        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test3), Mockito.anyLong());
        // Metrics should have been skipped, so the map should be empty.
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test3), Mockito.anyLong(), mCapturedMetrics.capture());
        Mockito.verify(mMockListener, times(1))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());

        List<HashMap<String, Metric>> allValues = mCapturedMetrics.getAllValues();
        // For test1
        assertFalse(allValues.get(0).containsKey("onteststart"));
        assertFalse(allValues.get(0).containsKey("ontestend"));
        // For test2
        assertFalse(allValues.get(1).containsKey("onteststart"));
        assertFalse(allValues.get(1).containsKey("ontestend"));
        // For test3: Should be the only WITH metrics since the other two were excluded.
        assertTrue(allValues.get(2).containsKey("onteststart"));
        assertTrue(allValues.get(2).containsKey("ontestend"));
    }

    /**
     * Test that onTestEnd with TestDescription formal supercedes the method signature without a
     * TestDescription.
     */
    @Test
    public void testOnTestEndWithTestDescription() throws Exception {
        mBase =
                new TwoMetricsBaseCollector() {
                    @Override
                    public void onTestEnd(
                            DeviceMetricData testData,
                            final Map<String, Metric> currentTestCaseMetrics,
                            TestDescription test) {
                        testData.addMetric(
                                test.getTestName(),
                                Metric.newBuilder()
                                        .setMeasurements(
                                                Measurements.newBuilder()
                                                        .setSingleString("value1")));
                    }
                };
        mBase.init(mContext, mMockListener);
        mBase.invocationStarted(mContext);
        mBase.testRunStarted("testRun", 1);
        TestDescription test = new TestDescription("class", "method");
        mBase.testStarted(test);
        mBase.testEnded(test, new HashMap<String, Metric>());
        mBase.testRunEnded(0L, new HashMap<String, Metric>());
        mBase.invocationEnded(0L);

        Mockito.verify(mMockListener, times(1)).invocationStarted(Mockito.any());
        Mockito.verify(mMockListener, times(1))
                .testRunStarted(
                        Mockito.eq("testRun"), Mockito.eq(1), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1)).testStarted(Mockito.eq(test), Mockito.anyLong());
        Mockito.verify(mMockListener, times(1))
                .testEnded(Mockito.eq(test), Mockito.anyLong(), mCapturedMetrics.capture());

        Mockito.verify(mMockListener, times(1))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());

        List<HashMap<String, Metric>> allValues = mCapturedMetrics.getAllValues();
        assertTrue(allValues.get(0).containsKey("onteststart"));
        assertTrue(allValues.get(0).containsKey("method"));
        assertTrue(!allValues.get(0).containsKey("ontestend"));
    }

    /**
     * Test can locate a source file existed in tests directory of a device build.
     */
    @Test
    public void testResolveRelativeFilePath_withDeviceBuildInfo() throws Exception {
        IDeviceBuildInfo buildInfo = EasyMock.createStrictMock(IDeviceBuildInfo.class);
        String fileName = "source_file";
        File testsDir = null;
        try {
            testsDir = FileUtil.createTempDir("tests_dir");
            File hostTestCasesDir = FileUtil.getFileForPath(testsDir, "host/testcases");
            FileUtil.mkdirsRWX(hostTestCasesDir);
            File sourceFile = FileUtil.createTempFile(fileName, null, hostTestCasesDir);

            fileName = sourceFile.getName();
            EasyMock.expect(buildInfo.getFile(fileName)).andReturn(null);
            EasyMock.expect(buildInfo.getTestsDir()).andReturn(testsDir);
            EasyMock.expect(buildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR)).andReturn(null);
            EasyMock.replay(buildInfo);

            mContext.addDeviceBuildInfo("abc", buildInfo);
            mBase.init(mContext, mMockListener);

            Assert.assertEquals(
                    sourceFile.getAbsolutePath(),
                    mBase.getFileFromTestArtifacts(fileName)
                            .getAbsolutePath());
            EasyMock.verify(buildInfo);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /**
     * Test file not available in the tests directory of a device build.
     */
    @Test
    public void testUnableResolveRelativeFilePath_withDeviceBuildInfo() throws Exception {
        IDeviceBuildInfo buildInfo = EasyMock.createStrictMock(IDeviceBuildInfo.class);
        String fileName = "source_file";
        File testsDir = null;
        try {
            testsDir = FileUtil.createTempDir("tests_dir");
            File hostTestCasesDir = FileUtil.getFileForPath(testsDir, "host/testcases");
            FileUtil.mkdirsRWX(hostTestCasesDir);
            File sourceFile = FileUtil.createTempFile(fileName, null, hostTestCasesDir);

            fileName = sourceFile.getName();
            String differentFileName = "abc";
            EasyMock.expect(buildInfo.getFile(differentFileName)).andReturn(null);
            EasyMock.expect(buildInfo.getTestsDir()).andReturn(testsDir);
            EasyMock.expect(buildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR)).andReturn(null);
            EasyMock.replay(buildInfo);

            mContext.addDeviceBuildInfo("abc", buildInfo);
            mBase.init(mContext, mMockListener);

            Assert.assertNull(
                    sourceFile.getAbsolutePath(),
                    mBase.getFileFromTestArtifacts(differentFileName));
            EasyMock.verify(buildInfo);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }

    /**
     * Test can locate a source file from the module directory if the directory named after the
     * module name has the file looked up for.
     */
    @Test
    public void testResolveRelativeFilePath_fromModule() throws Exception {
        IDeviceBuildInfo buildInfo = EasyMock.createStrictMock(IDeviceBuildInfo.class);
        String fileName = "source_file";
        File testsDir = null;
        try {
            testsDir = FileUtil.createTempDir("module_name");
            File hostTestCasesDir = FileUtil.getFileForPath(testsDir, "host/testcases");
            FileUtil.mkdirsRWX(hostTestCasesDir);
            File sourceFile = FileUtil.createTempFile(fileName, null, hostTestCasesDir);

            fileName = sourceFile.getName();
            EasyMock.expect(buildInfo.getFile(fileName)).andReturn(null);
            EasyMock.expect(buildInfo.getTestsDir()).andReturn(testsDir);
            EasyMock.expect(buildInfo.getFile(BuildInfoFileKey.TARGET_LINKED_DIR)).andReturn(null);
            EasyMock.replay(buildInfo);

            mContext.addDeviceBuildInfo("abc", buildInfo);
            mContext.addInvocationAttribute(ModuleDefinition.MODULE_NAME, testsDir.getName());
            mBase.init(mContext, mMockListener);

            Assert.assertEquals(
                    sourceFile.getAbsolutePath(),
                    mBase.getFileFromTestArtifacts(fileName).getAbsolutePath());
            EasyMock.verify(buildInfo);
        } finally {
            FileUtil.recursiveDelete(testsDir);
        }
    }
}
