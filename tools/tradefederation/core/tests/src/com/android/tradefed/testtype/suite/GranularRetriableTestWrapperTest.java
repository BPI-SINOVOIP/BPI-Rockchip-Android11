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
package com.android.tradefed.testtype.suite;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.metric.BaseDeviceMetricCollector;
import com.android.tradefed.device.metric.DeviceMetricData;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Measurements;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.FileSystemLogSaver;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.retry.BaseRetryDecision;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.retry.RetryStatistics;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link GranularRetriableTestWrapper}. */
@RunWith(JUnit4.class)
public class GranularRetriableTestWrapperTest {

    private static final String RUN_NAME = "test run";
    private static final String RUN_NAME_2 = "test run 2";
    private InvocationContext mModuleInvocationContext;
    private TestInformation mModuleInfo;
    private IRetryDecision mDecision;

    private class BasicFakeTest implements IRemoteTest {

        protected ArrayList<TestDescription> mTestCases;
        protected Set<String> mShouldRun = new HashSet<>();
        protected Map<TestDescription, Integer> mBecomePass = new HashMap<>();
        protected Map<TestDescription, Boolean> mShouldFail;
        private String mRunFailure = null;
        private Integer mClearRunFailureAttempt = null;
        protected int mAttempts = 0;

        public BasicFakeTest() {
            mTestCases = new ArrayList<>();
            TestDescription defaultTestCase = new TestDescription("ClassFoo", "TestFoo");
            mTestCases.add(defaultTestCase);
            mShouldFail = new HashMap<TestDescription, Boolean>();
            mShouldFail.put(defaultTestCase, false);
        }

        public BasicFakeTest(ArrayList<TestDescription> testCases) {
            mTestCases = testCases;
            mShouldFail = new HashMap<TestDescription, Boolean>();
            for (TestDescription testCase : testCases) {
                mShouldFail.put(testCase, false);
            }
            mAttempts = 0;
        }

        public void addFailedTestCase(TestDescription testCase) {
            mShouldFail.put(testCase, true);
        }

        public void addTestBecomePass(TestDescription testCase, int attempt) {
            mBecomePass.put(testCase, attempt);
        }

        public void setRunFailure(String message) {
            mRunFailure = message;
        }

        public void setClearRunFailure(Integer clearRunFailure) {
            mClearRunFailureAttempt = clearRunFailure;
        }

        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceUnresponsiveException {
            listener.testRunStarted(RUN_NAME, mTestCases.size());
            for (TestDescription td : mTestCases) {
                if (!mShouldRun.isEmpty() && !mShouldRun.contains(td.toString())) {
                    continue;
                }
                listener.testStarted(td);
                int passAttempt = -1;
                if (mBecomePass.get(td) != null) {
                    passAttempt = mBecomePass.get(td);
                }
                if (mShouldFail.get(td)) {
                    if (passAttempt == -1 || mAttempts < passAttempt) {
                        listener.testFailed(td, String.format("Fake failure %s", td.toString()));
                    }
                }
                listener.testEnded(td, new HashMap<String, Metric>());

                if (mRunFailure != null) {
                    listener.testRunFailed(mRunFailure);
                    if (mClearRunFailureAttempt != null
                            && mClearRunFailureAttempt == mAttempts + 1) {
                        mRunFailure = null;
                    }
                }
            }
            listener.testRunEnded(0, new HashMap<String, Metric>());
            mAttempts++;
        }
    }

    private class FakeTest extends BasicFakeTest implements ITestFilterReceiver, IDeviceTest {

        private ITestDevice mDevice;

        public FakeTest(ArrayList<TestDescription> testCases) {
            super(testCases);
        }

        public FakeTest() {
            super();
        }

        @Override
        public void addIncludeFilter(String filter) {
            mShouldRun.add(filter);
        }

        @Override
        public void addAllIncludeFilters(Set<String> filters) {}

        @Override
        public void addExcludeFilter(String filters) {}

        @Override
        public void addAllExcludeFilters(Set<String> filters) {}

        @Override
        public void clearIncludeFilters() {
            mShouldRun.clear();
        }

        @Override
        public Set<String> getIncludeFilters() {
            return mShouldRun;
        }

        @Override
        public Set<String> getExcludeFilters() {
            return new HashSet<>();
        }

        @Override
        public void clearExcludeFilters() {}

        @Override
        public void setDevice(ITestDevice device) {
            mDevice = device;
        }

        @Override
        public ITestDevice getDevice() {
            return mDevice;
        }
    }

    private class MultiTestOneRunFakeTest extends FakeTest {

        private Map<String, List<TestDescription>> mRunTestsMap;
        private Integer mMaxTestCount;

        public MultiTestOneRunFakeTest() {
            mRunTestsMap = new HashMap<String, List<TestDescription>>();
            mMaxTestCount = 0;
            mAttempts = 0;
        }

        public void setTestCasesByRun(String runName, List<TestDescription> testCases) {
            mRunTestsMap.put(runName, testCases);
            for (TestDescription testCase : testCases) {
                mShouldFail.put(testCase, false);
            }
            mMaxTestCount = Math.max(mMaxTestCount, testCases.size());
        }

        @Override
        public void run(TestInformation testInfo, ITestInvocationListener listener)
                throws DeviceUnresponsiveException {
            Set<String> testRuns = mRunTestsMap.keySet();
            for (int idx = 0; idx < mMaxTestCount; idx++) {
                // Tests in different runs are called alternatively. This example describes the risk
                // condition that a single IRemoteTest has two run names (RUN_NAME, RUN_NAME_2).
                // The test cases in those two runs are called alternatively.
                for (String runName : testRuns) {
                    List<TestDescription> testCases = mRunTestsMap.get(runName);
                    if (idx >= testCases.size()) {
                        continue;
                    }
                    TestDescription td = testCases.get(idx);
                    if (!mShouldRun.isEmpty() && !mShouldRun.contains(td.toString())) {
                        continue;
                    }
                    listener.testRunStarted(runName, testCases.size());
                    listener.testStarted(td);
                    int passAttempt = -1;
                    if (mBecomePass.get(td) != null) {
                        passAttempt = mBecomePass.get(td);
                    }
                    if (mShouldFail.get(td)) {
                        if (passAttempt == -1 || mAttempts < passAttempt) {
                            listener.testFailed(
                                    td, String.format("Fake failure %s", td.toString()));
                        }
                    }
                    listener.testEnded(td, new HashMap<String, Metric>());
                    listener.testRunEnded(0, new HashMap<String, Metric>());
                }
            }
            mAttempts++;
        }
    }

    private GranularRetriableTestWrapper createGranularTestWrapper(
            IRemoteTest test, int maxRunCount) throws Exception {
        return createGranularTestWrapper(test, maxRunCount, new ArrayList<>());
    }

    private GranularRetriableTestWrapper createGranularTestWrapper(
            IRemoteTest test, int maxRunCount, List<IMetricCollector> collectors) throws Exception {
        GranularRetriableTestWrapper granularTestWrapper =
                new GranularRetriableTestWrapper(test, null, null, null, maxRunCount);
        granularTestWrapper.setModuleId("test module");
        granularTestWrapper.setMarkTestsSkipped(false);
        granularTestWrapper.setMetricCollectors(collectors);
        // Setup InvocationContext.
        granularTestWrapper.setInvocationContext(mModuleInvocationContext);
        // Setup logsaver.
        granularTestWrapper.setLogSaver(new FileSystemLogSaver());
        IConfiguration mockModuleConfiguration = Mockito.mock(IConfiguration.class);
        granularTestWrapper.setModuleConfig(mockModuleConfiguration);
        mDecision = new BaseRetryDecision();
        OptionSetter setter = new OptionSetter(mDecision);
        setter.setOptionValue("retry-strategy", "RETRY_ANY_FAILURE");
        setter.setOptionValue("max-testcase-run-count", Integer.toString(maxRunCount));
        mDecision.setInvocationContext(mModuleInvocationContext);
        granularTestWrapper.setRetryDecision(mDecision);
        return granularTestWrapper;
    }

    @Before
    public void setUp() {
        mModuleInvocationContext = new InvocationContext();
        mModuleInfo =
                TestInformation.newBuilder().setInvocationContext(mModuleInvocationContext).build();
    }

    /**
     * Test that the intra module retry does not inhibit DeviceNotAvailableException. They are
     * bubbled up to the top.
     */
    @Test
    public void testIntraModuleRun_catchDeviceNotAvailableException() throws Exception {
        IRemoteTest mockTest = Mockito.mock(IRemoteTest.class);
        Mockito.doThrow(new DeviceNotAvailableException("fake message", "serial"))
                .when(mockTest)
                .run(Mockito.any(), Mockito.any(ITestInvocationListener.class));

        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(mockTest, 1);
        try {
            granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
            fail("Should have thrown an exception.");
        } catch (DeviceNotAvailableException expected) {
            // Expected
            assertEquals("fake message", expected.getMessage());
        }
    }

    /**
     * Test that the intra module "run" method catches DeviceUnresponsiveException and doesn't raise
     * it again.
     */
    @Test
    public void testIntraModuleRun_catchDeviceUnresponsiveException() throws Exception {
        FakeTest test =
                new FakeTest() {
                    @Override
                    public void run(TestInformation testInfo, ITestInvocationListener listener)
                            throws DeviceUnresponsiveException {
                        listener.testRunStarted("test run", 1);
                        throw new DeviceUnresponsiveException("fake message", "serial");
                    }
                };
        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(test, 1);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
        TestRunResult attempResults =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME).get(0);
        assertTrue(attempResults.isRunFailure());
    }

    /**
     * Test that the "run" method has built-in retry logic and each run has an individual
     * ModuleListener and TestRunResult.
     */
    @Test
    public void testRun_withMultipleRun() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase = new TestDescription("Class", "Test");
        TestDescription fakeTestCase2 = new TestDescription("Class", "Test2");
        TestDescription fakeTestCase3 = new TestDescription("Class", "Test3");
        testCases.add(fakeTestCase);
        testCases.add(fakeTestCase2);
        testCases.add(fakeTestCase3);
        FakeTest test = new FakeTest(testCases);
        test.addFailedTestCase(fakeTestCase);
        test.addFailedTestCase(fakeTestCase2);

        int maxRunCount = 5;
        GranularRetriableTestWrapper granularTestWrapper =
                createGranularTestWrapper(test, maxRunCount);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
        // Verify the test runs several times but under the same run name
        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        assertEquals(
                maxRunCount, granularTestWrapper.getTestRunResultCollected().get(RUN_NAME).size());
        assertEquals(1, granularTestWrapper.getFinalTestRunResults().size());
        Map<TestDescription, TestResult> testResults =
                granularTestWrapper.getFinalTestRunResults().get(0).getTestResults();
        assertTrue(testResults.containsKey(fakeTestCase));
        assertTrue(testResults.containsKey(fakeTestCase2));
        assertTrue(testResults.containsKey(fakeTestCase3));
        // Verify the final TestRunResult is a merged value of every retried TestRunResults.
        assertEquals(TestStatus.FAILURE, testResults.get(fakeTestCase).getStatus());
        assertEquals(TestStatus.FAILURE, testResults.get(fakeTestCase2).getStatus());
        assertEquals(TestStatus.PASSED, testResults.get(fakeTestCase3).getStatus());

        // Ensure that the PASSED test was only run the first time.
        assertTrue(
                granularTestWrapper
                        .getTestRunResultCollected()
                        .get(RUN_NAME)
                        .get(0)
                        .getTestResults()
                        .containsKey(fakeTestCase3));
        for (int i = 1; i < maxRunCount; i++) {
            assertFalse(
                    granularTestWrapper
                            .getTestRunResultCollected()
                            .get(RUN_NAME)
                            .get(i)
                            .getTestResults()
                            .containsKey(fakeTestCase3));
        }

        // Since tests stay failed, we have two failure in our monitoring.
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertEquals(0, stats.mRetrySuccess);
        assertEquals(2, stats.mRetryFailure);
    }

    /** Test when a test becomes pass after failing */
    @Test
    public void testRun_withMultipleRun_becomePass() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase = new TestDescription("Class", "Test");
        TestDescription fakeTestCase2 = new TestDescription("Class", "Test2");
        TestDescription fakeTestCase3 = new TestDescription("Class", "Test3");
        testCases.add(fakeTestCase);
        testCases.add(fakeTestCase2);
        testCases.add(fakeTestCase3);
        FakeTest test = new FakeTest(testCases);
        test.addFailedTestCase(fakeTestCase);
        test.addFailedTestCase(fakeTestCase2);
        // At attempt 3, the test case will become pass.
        test.addTestBecomePass(fakeTestCase, 3);

        int maxRunCount = 5;
        GranularRetriableTestWrapper granularTestWrapper =
                createGranularTestWrapper(test, maxRunCount);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
        // Verify the test runs several times but under the same run name
        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        assertEquals(
                maxRunCount, granularTestWrapper.getTestRunResultCollected().get(RUN_NAME).size());
        assertEquals(1, granularTestWrapper.getFinalTestRunResults().size());
        Map<TestDescription, TestResult> testResults =
                granularTestWrapper.getFinalTestRunResults().get(0).getTestResults();
        assertTrue(testResults.containsKey(fakeTestCase));
        assertTrue(testResults.containsKey(fakeTestCase2));
        assertTrue(testResults.containsKey(fakeTestCase3));
        // Verify the final TestRunResult is a merged value of every retried TestRunResults.
        assertEquals(TestStatus.PASSED, testResults.get(fakeTestCase).getStatus()); // became pass
        assertEquals(TestStatus.FAILURE, testResults.get(fakeTestCase2).getStatus());
        assertEquals(TestStatus.PASSED, testResults.get(fakeTestCase3).getStatus());

        // Ensure that the PASSED test was only run the first time.
        assertTrue(
                granularTestWrapper
                        .getTestRunResultCollected()
                        .get(RUN_NAME)
                        .get(0)
                        .getTestResults()
                        .containsKey(fakeTestCase3));
        for (int i = 1; i < maxRunCount; i++) {
            assertFalse(
                    granularTestWrapper
                            .getTestRunResultCollected()
                            .get(RUN_NAME)
                            .get(i)
                            .getTestResults()
                            .containsKey(fakeTestCase3));
        }

        // One success since one test recover, one test never recover so one failure
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertEquals(1, stats.mRetrySuccess);
        assertEquals(1, stats.mRetryFailure);
    }

    /** Test when all tests become pass, we stop intra-module retry early. */
    @Test
    public void testRun_withMultipleRun_AllBecomePass() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase = new TestDescription("Class", "Test");
        TestDescription fakeTestCase2 = new TestDescription("Class", "Test2");
        TestDescription fakeTestCase3 = new TestDescription("Class", "Test3");
        testCases.add(fakeTestCase);
        testCases.add(fakeTestCase2);
        testCases.add(fakeTestCase3);
        FakeTest test = new FakeTest(testCases);
        test.addFailedTestCase(fakeTestCase);
        test.addFailedTestCase(fakeTestCase2);
        // At attempt 3, the test case will become pass.
        test.addTestBecomePass(fakeTestCase, 3);
        test.addTestBecomePass(fakeTestCase2, 2);

        int maxRunCount = 5;
        GranularRetriableTestWrapper granularTestWrapper =
                createGranularTestWrapper(test, maxRunCount);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
        // Verify the test runs several times but under the same run name
        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        assertEquals(4, granularTestWrapper.getTestRunResultCollected().get(RUN_NAME).size());
        assertEquals(1, granularTestWrapper.getFinalTestRunResults().size());
        Map<TestDescription, TestResult> testResults =
                granularTestWrapper.getFinalTestRunResults().get(0).getTestResults();
        assertTrue(testResults.containsKey(fakeTestCase));
        assertTrue(testResults.containsKey(fakeTestCase2));
        assertTrue(testResults.containsKey(fakeTestCase3));
        // Verify the final TestRunResult is a merged value of every retried TestRunResults.
        assertEquals(TestStatus.PASSED, testResults.get(fakeTestCase).getStatus()); // became pass
        assertEquals(TestStatus.PASSED, testResults.get(fakeTestCase2).getStatus());
        assertEquals(TestStatus.PASSED, testResults.get(fakeTestCase3).getStatus());

        // Ensure that the PASSED test was only run the first time.
        assertTrue(
                granularTestWrapper
                        .getTestRunResultCollected()
                        .get(RUN_NAME)
                        .get(0)
                        .getTestResults()
                        .containsKey(fakeTestCase3));
        for (int i = 1; i < 4; i++) {
            assertFalse(
                    granularTestWrapper
                            .getTestRunResultCollected()
                            .get(RUN_NAME)
                            .get(i)
                            .getTestResults()
                            .containsKey(fakeTestCase3));
        }
        // Ensure that once tests start passing they stop running
        assertTrue(
                granularTestWrapper
                        .getTestRunResultCollected()
                        .get(RUN_NAME)
                        .get(0)
                        .getTestResults()
                        .containsKey(fakeTestCase2));
        assertTrue(
                granularTestWrapper
                        .getTestRunResultCollected()
                        .get(RUN_NAME)
                        .get(1)
                        .getTestResults()
                        .containsKey(fakeTestCase2));
        assertTrue(
                granularTestWrapper
                        .getTestRunResultCollected()
                        .get(RUN_NAME)
                        .get(2)
                        .getTestResults()
                        .containsKey(fakeTestCase2));
        assertFalse(
                granularTestWrapper
                        .getTestRunResultCollected()
                        .get(RUN_NAME)
                        .get(3)
                        .getTestResults()
                        .containsKey(fakeTestCase2));

        // One success since one test recover, one test never recover so one failure\
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertEquals(2, stats.mRetrySuccess);
        assertEquals(0, stats.mRetryFailure);
    }

    /**
     * Test that if IRemoteTest doesn't implement ITestFilterReceiver, the "run" method will not
     * retry.
     */
    @Test
    public void testRun_retryAllTestCasesIfNotSupportTestFilterReceiver() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase1 = new TestDescription("Class1", "Test1");
        TestDescription fakeTestCase2 = new TestDescription("Class2", "Test2");
        testCases.add(fakeTestCase1);
        testCases.add(fakeTestCase2);
        BasicFakeTest test = new BasicFakeTest(testCases);
        // Only the first testcase is failed.
        test.addFailedTestCase(fakeTestCase1);
        // Run each testcases (if has failure) max to 3 times.
        int maxRunCount = 3;
        GranularRetriableTestWrapper granularTestWrapper =
                createGranularTestWrapper(test, maxRunCount);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());

        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        // Expect only 1 run since it does not support ITestFilterReceiver
        assertEquals(1, granularTestWrapper.getTestRunResultCollected().get(RUN_NAME).size());
        List<TestRunResult> resultCollector =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME);
        // Check that all test cases where rerun
        for (TestRunResult runResult : resultCollector) {
            assertEquals(2, runResult.getNumTests());
            assertEquals(
                    TestStatus.FAILURE, runResult.getTestResults().get(fakeTestCase1).getStatus());
            assertEquals(
                    TestStatus.PASSED, runResult.getTestResults().get(fakeTestCase2).getStatus());
        }
    }

    /**
     * Test that if one run attempt includes multiple runs (IRemoteTest has multiple run names), the
     * GranularRetriableWrapper retries all the failed testcases from each run in the next attempt.
     */
    @Test
    public void testRun_retryMultiTestsForOneRun() throws Exception {
        MultiTestOneRunFakeTest test = new MultiTestOneRunFakeTest();
        TestDescription fakeTestCase1 = new TestDescription("Class1", "Test1");
        TestDescription fakeTestCase2 = new TestDescription("Class2", "Test2");
        TestDescription fakeTestCase3 = new TestDescription("Class3", "Test3");
        TestDescription fakeTestCase4 = new TestDescription("Class4", "Test4");
        List<TestDescription> testCasesForRun1 = Arrays.asList(fakeTestCase1, fakeTestCase2);
        List<TestDescription> testCasesForRun2 = Arrays.asList(fakeTestCase3, fakeTestCase4);
        test.setTestCasesByRun(RUN_NAME, testCasesForRun1);
        test.setTestCasesByRun(RUN_NAME_2, testCasesForRun2);
        test.addFailedTestCase(fakeTestCase1);
        test.addFailedTestCase(fakeTestCase3);
        test.addTestBecomePass(fakeTestCase1, 1);
        int maxRunCount = 5;

        GranularRetriableTestWrapper granularTestWrapper =
                createGranularTestWrapper(test, maxRunCount);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
        // Two runs.
        assertEquals(2, granularTestWrapper.getTestRunResultCollected().size());
        List<TestRunResult> resultCollector1 =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME);
        // 1st test run passes after one retry.
        assertEquals(2, resultCollector1.size());
        List<TestRunResult> resultCollector2 =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME_2);
        // 2nd test run doesn't pass after all retries.
        assertEquals(maxRunCount, resultCollector2.size());

        List<TestRunResult> finalResult = granularTestWrapper.getFinalTestRunResults();
        assertEquals(2, finalResult.size());
        TestRunResult runResult1 = finalResult.get(0);
        TestRunResult runResult2 = finalResult.get(1);

        // Verify the final result includes two completed test runs. The failed test in the 1st run
        // passes after one retry, and the second test run retried maxRunCount times and stil has
        // failed test cases.
        assertEquals(RUN_NAME, runResult1.getName());
        assertEquals(RUN_NAME_2, runResult2.getName());
        assertEquals(TestStatus.PASSED, runResult1.getTestResults().get(fakeTestCase1).getStatus());
        assertEquals(TestStatus.PASSED, runResult1.getTestResults().get(fakeTestCase2).getStatus());
        assertEquals(
                TestStatus.FAILURE, runResult2.getTestResults().get(fakeTestCase3).getStatus());
        assertEquals(TestStatus.PASSED, runResult2.getTestResults().get(fakeTestCase4).getStatus());
    }

    /** Test the retry for Run level. */
    @Test
    public void testIntraModuleRun_runRetry() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase1 = new TestDescription("Class1", "Test1");
        TestDescription fakeTestCase2 = new TestDescription("Class2", "Test2");
        testCases.add(fakeTestCase1);
        testCases.add(fakeTestCase2);
        FakeTest test = new FakeTest(testCases);
        test.setRunFailure("I failed!");
        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(test, 3);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());

        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        List<TestRunResult> allResults =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME);
        assertEquals(3, allResults.size());
        for (int i = 0; i < 3; i++) {
            TestRunResult res = allResults.get(i);
            // All attempts are run failures
            assertTrue(res.isRunFailure());
            // All tests cases are rerun each time.
            assertEquals(2, res.getNumCompleteTests());
        }

        // No Test cases tracking since it was a run retry.
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertEquals(0, stats.mRetrySuccess);
        assertEquals(0, stats.mRetryFailure);
    }

    /**
     * Test the retry for Run level when the failure eventually clears. We stop retrying when no
     * more failure.
     */
    @Test
    public void testIntraModuleRun_runRetry_clear() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase1 = new TestDescription("Class1", "Test1");
        TestDescription fakeTestCase2 = new TestDescription("Class2", "Test2");
        testCases.add(fakeTestCase1);
        testCases.add(fakeTestCase2);
        FakeTest test = new FakeTest(testCases);
        test.setRunFailure("I failed!");
        test.setClearRunFailure(3);
        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(test, 7);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());

        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        List<TestRunResult> allResults =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME);
        assertEquals(4, allResults.size());
        for (int i = 0; i < 3; i++) {
            TestRunResult res = allResults.get(i);
            // All attempts are run failures until now
            assertTrue(res.isRunFailure());
            // All tests cases are rerun each time.
            assertEquals(2, res.getNumCompleteTests());
        }
        TestRunResult lastRes = allResults.get(3);
        assertFalse(lastRes.isRunFailure());
        // All tests cases are rerun each time.
        assertEquals(2, lastRes.getNumCompleteTests());

        // No Test cases tracking since it was a run retry.
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertEquals(0, stats.mRetrySuccess);
        assertEquals(0, stats.mRetryFailure);
    }

    /** Test the retry with iterations, it doesn't require any failure to rerun. */
    @Test
    public void testIntraModuleRun_iterations() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase1 = new TestDescription("Class1", "Test1");
        TestDescription fakeTestCase2 = new TestDescription("Class2", "Test2");
        testCases.add(fakeTestCase1);
        testCases.add(fakeTestCase2);
        FakeTest test = new FakeTest(testCases);
        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(test, 3);
        IRetryDecision decision = new BaseRetryDecision();
        OptionSetter setter = new OptionSetter(decision);
        setter.setOptionValue("retry-strategy", "ITERATIONS");
        setter.setOptionValue("max-testcase-run-count", Integer.toString(3));
        decision.setInvocationContext(mModuleInvocationContext);
        granularTestWrapper.setRetryDecision(decision);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());

        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        List<TestRunResult> allResults =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME);
        assertEquals(3, allResults.size());
        for (int i = 0; i < 3; i++) {
            TestRunResult res = allResults.get(i);
            // All attempts are not failure
            assertFalse(res.isRunFailure());
            // All tests cases are rerun each time.
            assertEquals(2, res.getNumCompleteTests());
        }

        // No Test cases tracking since it was a run retry.
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertEquals(0, stats.mRetrySuccess);
        assertEquals(0, stats.mRetryFailure);
    }

    /** When re-running until failure, stop when failure is encountered. */
    @Test
    public void testIntraModuleRun_untilFailure() throws Exception {
        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase1 = new TestDescription("Class1", "Test1");
        TestDescription fakeTestCase2 = new TestDescription("Class2", "Test2");
        testCases.add(fakeTestCase1);
        testCases.add(fakeTestCase2);
        FakeTest test = new FakeTest(testCases);
        test.addFailedTestCase(fakeTestCase2);
        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(test, 3);
        IRetryDecision decision = new BaseRetryDecision();
        OptionSetter setter = new OptionSetter(decision);
        setter.setOptionValue("retry-strategy", "RERUN_UNTIL_FAILURE");
        setter.setOptionValue("max-testcase-run-count", Integer.toString(3));
        decision.setInvocationContext(mModuleInvocationContext);
        granularTestWrapper.setRetryDecision(decision);

        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());

        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        List<TestRunResult> allResults =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME);
        // We didn't run all attempts because test case failed.
        assertEquals(1, allResults.size());

        TestRunResult res = allResults.get(0);
        // All attempts are not failure
        assertFalse(res.isRunFailure());
        // All tests cases are rerun each time.
        assertEquals(2, res.getNumCompleteTests());

        // No stats since no retry occurred.
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertNotNull(stats);
        assertEquals(0, stats.mRetrySuccess);
        assertEquals(0, stats.mRetryFailure);
    }

    /**
     * Test to run with retrying on any failure when a run failure and test case failure exists.
     * First we retry like a run until cleared, then retry for use cases until clear.
     */
    @Test
    public void testIntraModuleRun_runAnyFailure() throws Exception {
        List<IMetricCollector> collectors = new ArrayList<>();
        // Add a disabled collector to ensure it's never called
        CalledMetricCollector notCalledCollector = new CalledMetricCollector();
        notCalledCollector.setDisable(true);
        notCalledCollector.mName = "not-called";
        collectors.add(notCalledCollector);
        CalledMetricCollector calledCollector = new CalledMetricCollector();
        calledCollector.mName = "called";
        collectors.add(calledCollector);

        ArrayList<TestDescription> testCases = new ArrayList<>();
        TestDescription fakeTestCase1 = new TestDescription("Class1", "Test1");
        TestDescription fakeTestCase2 = new TestDescription("Class2", "Test2");
        testCases.add(fakeTestCase1);
        testCases.add(fakeTestCase2);
        FakeTest test = new FakeTest(testCases);
        test.setRunFailure("I failed!");
        test.setClearRunFailure(3);
        // Failed test cases do not affect retry of runs
        test.addFailedTestCase(fakeTestCase1);
        test.addTestBecomePass(fakeTestCase1, 5);
        GranularRetriableTestWrapper granularTestWrapper =
                createGranularTestWrapper(test, 7, collectors);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());

        assertEquals(1, granularTestWrapper.getTestRunResultCollected().size());
        List<TestRunResult> allResults =
                granularTestWrapper.getTestRunResultCollected().get(RUN_NAME);
        assertEquals(6, allResults.size());
        for (int i = 0; i < 3; i++) {
            TestRunResult res = allResults.get(i);
            // All attempts are run failures until now
            assertTrue(res.isRunFailure());
            // All tests cases are rerun each time.
            assertEquals(2, res.getNumCompleteTests());
        }
        // At attempt 4 the run become pass but we continue retrying because of test case failure.
        TestRunResult lastRes = allResults.get(3);
        assertFalse(lastRes.isRunFailure());
        // All tests cases are rerun each time.
        assertEquals(2, lastRes.getNumCompleteTests());
        assertEquals(1, lastRes.getFailedTests().size());
        assertTrue(lastRes.getRunProtoMetrics().containsKey("called"));
        assertFalse(lastRes.getRunProtoMetrics().containsKey("not-called"));

        lastRes = allResults.get(4);
        assertFalse(lastRes.isRunFailure());
        // The passed test does not rerun now that there is no run failure.
        assertEquals(1, lastRes.getNumCompleteTests());
        assertEquals(1, lastRes.getFailedTests().size());
        assertTrue(lastRes.getRunProtoMetrics().containsKey("called"));
        assertFalse(lastRes.getRunProtoMetrics().containsKey("not-called"));

        lastRes = allResults.get(5);
        assertFalse(lastRes.isRunFailure());
        // All tests cases are rerun each time.
        assertEquals(1, lastRes.getNumCompleteTests());
        // The failed test final pass
        assertEquals(0, lastRes.getFailedTests().size());
        assertTrue(lastRes.getRunProtoMetrics().containsKey("called"));
        assertFalse(lastRes.getRunProtoMetrics().containsKey("not-called"));

        // Check that failure are cleared
        RetryStatistics stats = mDecision.getRetryStatistics();
        assertEquals(1, stats.mRetrySuccess);
        assertEquals(0, stats.mRetryFailure);
    }

    /**
     * Test to reboot device at the last intra-module retry.
     */
    @Test
    public void testIntraModuleRun_rebootAtLastIntraModuleRetry() throws Exception {
        IRetryDecision decision = new BaseRetryDecision();
        OptionSetter setter = new OptionSetter(decision);
        setter.setOptionValue("reboot-at-last-retry", "true");
        setter.setOptionValue("retry-strategy", "RETRY_ANY_FAILURE");
        setter.setOptionValue("max-testcase-run-count", Integer.toString(2));
        decision.setInvocationContext(mModuleInvocationContext);
        FakeTest test = new FakeTest();
        test.setRunFailure("I failed!");
        ITestDevice mMockDevice = EasyMock.createMock(ITestDevice.class);
        test.setDevice(mMockDevice);
        mModuleInvocationContext.addAllocatedDevice("default-device1", mMockDevice);
        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(test, 2);
        granularTestWrapper.setRetryDecision(decision);
        EasyMock.expect(mMockDevice.getIDevice()).andStubReturn(EasyMock.createMock(IDevice.class));
        mMockDevice.reboot();
        EasyMock.replay(mMockDevice);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
        EasyMock.verify(mMockDevice);
    }

    /**
     * Test to reboot multi-devices at the last intra-module retry.
     */
    @Test
    public void testIntraModuleRun_rebootMultiDevicesAtLastIntraModuleRetry() throws Exception {
        IRetryDecision decision = new BaseRetryDecision();
        OptionSetter setter = new OptionSetter(decision);
        setter.setOptionValue("reboot-at-last-retry", "true");
        setter.setOptionValue("retry-strategy", "RETRY_ANY_FAILURE");
        setter.setOptionValue("max-testcase-run-count", Integer.toString(3));
        decision.setInvocationContext(mModuleInvocationContext);
        FakeTest test = new FakeTest();
        test.setRunFailure("I failed!");
        ITestDevice mMockDevice = EasyMock.createMock(ITestDevice.class);
        ITestDevice mMockDevice2 = EasyMock.createMock(ITestDevice.class);
        test.setDevice(mMockDevice);
        mModuleInvocationContext.addAllocatedDevice("default-device1", mMockDevice);
        mModuleInvocationContext.addAllocatedDevice("default-device2", mMockDevice2);
        GranularRetriableTestWrapper granularTestWrapper = createGranularTestWrapper(test, 3);
        granularTestWrapper.setRetryDecision(decision);
        EasyMock.expect(mMockDevice.getIDevice()).andStubReturn(EasyMock.createMock(IDevice.class));
        EasyMock.expect(mMockDevice2.getIDevice()).andStubReturn(EasyMock.createMock(IDevice.class));
        mMockDevice.reboot();
        mMockDevice2.reboot();
        EasyMock.replay(mMockDevice, mMockDevice2);
        granularTestWrapper.run(mModuleInfo, new CollectingTestListener());
        EasyMock.verify(mMockDevice, mMockDevice2);
    }

    /** Collector that track if it was called or not */
    public static class CalledMetricCollector extends BaseDeviceMetricCollector {

        @Option(name = "name")
        public String mName;

        @Override
        public void onTestRunStart(DeviceMetricData runData) {
            runData.addMetric(
                    mName,
                    Metric.newBuilder()
                            .setMeasurements(Measurements.newBuilder().setSingleString(mName)));
        }

        @Override
        public void onTestRunEnd(
                DeviceMetricData runData, final Map<String, Metric> currentRunMetrics) {
            runData.addMetric(
                    mName,
                    Metric.newBuilder()
                            .setMeasurements(Measurements.newBuilder().setSingleString(mName)));
        }

        @Override
        public void onTestStart(DeviceMetricData testData) {
            testData.addMetric(
                    mName,
                    Metric.newBuilder()
                            .setMeasurements(Measurements.newBuilder().setSingleString(mName)));
        }

        @Override
        public void onTestEnd(
                DeviceMetricData testData, final Map<String, Metric> currentTestCaseMetrics) {
            testData.addMetric(
                    mName,
                    Metric.newBuilder()
                            .setMeasurements(Measurements.newBuilder().setSingleString(mName)));
        }
    }
}
