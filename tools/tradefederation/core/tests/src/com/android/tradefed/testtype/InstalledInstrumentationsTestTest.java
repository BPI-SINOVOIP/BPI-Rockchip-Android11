/*
 * Copyright (C) 2012 The Android Open Source Project
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

import com.android.tradefed.config.ArgsOptionParser;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.metric.BaseDeviceMetricCollector;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestRunResult;

import org.easymock.EasyMock;
import org.easymock.IAnswer;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;

/** Unit tests for {@link InstalledInstrumentationsTest}. */
@RunWith(JUnit4.class)
public class InstalledInstrumentationsTestTest {

    private static final String TEST_PKG = "com.example.tests";
    private static final String TEST_COVERAGE_TARGET = "com.example";
    private static final String TEST_RUNNER = "android.support.runner.AndroidJUnitRunner";
    private static final String ABI = "forceMyAbiSettingPlease";
    private static final String INSTR_OUTPUT_FORMAT = "instrumentation:%s/%s (target=%s)\r\n";
    private static final String PM_LIST_ERROR_OUTPUT = "Error: Could not access the Package "
            + "Manager.  Is the system running?";
    private ITestDevice mMockTestDevice;
    private ITestInvocationListener mMockListener;
    private List<MockInstrumentationTest> mMockInstrumentationTests;
    private InstalledInstrumentationsTest mInstalledInstrTest;
    private TestInformation mTestInfo;

    @Before
    public void setUp() throws Exception {
        mMockTestDevice = EasyMock.createMock(ITestDevice.class);
        EasyMock.expect(mMockTestDevice.getSerialNumber()).andStubReturn("foo");
        mMockListener = EasyMock.createMock(ITestInvocationListener.class);
        mMockInstrumentationTests = new ArrayList<MockInstrumentationTest>();
        mInstalledInstrTest = createInstalledInstrumentationsTest();
        mInstalledInstrTest.setDevice(mMockTestDevice);
        IInvocationContext context = new InvocationContext();
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    /** Test the run normal case. Simple verification that expected data is passed along, etc. */
    @Test
    public void testRun() throws Exception {
        injectShellResponse(String.format(INSTR_OUTPUT_FORMAT, TEST_PKG, TEST_RUNNER,
                TEST_COVERAGE_TARGET), 1);

        ArgsOptionParser p = new ArgsOptionParser(mInstalledInstrTest);
        p.parse("--size", "small", "--force-abi", ABI);
        EasyMock.replay(mMockTestDevice, mMockListener);
        mInstalledInstrTest.run(mTestInfo, mMockListener);
        assertEquals(1, mMockInstrumentationTests.size());
        MockInstrumentationTest mockInstrumentationTest = mMockInstrumentationTests.get(0);
        assertEquals(mMockListener, mockInstrumentationTest.getListener());
        assertEquals(TEST_PKG, mockInstrumentationTest.getPackageName());
        assertEquals(TEST_RUNNER, mockInstrumentationTest.getRunnerName());
        assertEquals("small", mockInstrumentationTest.getTestSize());
        assertEquals(ABI, mockInstrumentationTest.getForceAbi());

        EasyMock.verify(mMockListener, mMockTestDevice);
    }

    @Test
    public void testRun_retry() throws Exception {
        injectShellResponse(
                String.format(INSTR_OUTPUT_FORMAT, TEST_PKG, TEST_RUNNER, TEST_COVERAGE_TARGET), 1);

        ArgsOptionParser p = new ArgsOptionParser(mInstalledInstrTest);
        p.parse("--size", "small", "--force-abi", ABI);
        List<TestRunResult> previousResults = new ArrayList<>();
        TestRunResult result = new TestRunResult();
        result.testRunStarted(TEST_PKG, 1);
        TestDescription testDesc = new TestDescription("com.example.tests.class", "testMethod");
        result.testStarted(testDesc);
        result.testFailed(testDesc, "failed");
        result.testEnded(testDesc, new HashMap<String, Metric>());
        result.testRunEnded(5L, new HashMap<String, Metric>());
        previousResults.add(result);
        EasyMock.replay(mMockTestDevice, mMockListener);
        assertTrue(mInstalledInstrTest.shouldRetry(0, previousResults));
        mInstalledInstrTest.run(mTestInfo, mMockListener);
        assertEquals(1, mMockInstrumentationTests.size());
        MockInstrumentationTest mockInstrumentationTest = mMockInstrumentationTests.get(0);
        assertEquals(mMockListener, mockInstrumentationTest.getListener());
        assertEquals(TEST_PKG, mockInstrumentationTest.getPackageName());
        assertEquals(TEST_RUNNER, mockInstrumentationTest.getRunnerName());
        assertEquals("small", mockInstrumentationTest.getTestSize());
        assertEquals(ABI, mockInstrumentationTest.getForceAbi());
        assertEquals(1, mockInstrumentationTest.getIncludeFilters().size());
        assertTrue(mockInstrumentationTest.getIncludeFilters().contains(testDesc.toString()));
        EasyMock.verify(mMockListener, mMockTestDevice);
    }

    /** Tests the run of sharded InstalledInstrumentationsTests. */
    @Test
    public void testShardedRun() throws Exception {
        final String shardableRunner = "android.support.test.runner.AndroidJUnitRunner";
        final String nonshardableRunner = "android.test.InstrumentationTestRunner";

        final String shardableTestPkg = "com.example.shardabletest";
        final String nonshardableTestPkg1 = "com.example.nonshardabletest1";
        final String nonshardableTestPkg2 = "com.example.nonshardabletest2";


        String shardableInstr = String.format(INSTR_OUTPUT_FORMAT, shardableTestPkg,
                shardableRunner, TEST_COVERAGE_TARGET);
        String nonshardableInstr1 = String.format(INSTR_OUTPUT_FORMAT, nonshardableTestPkg1,
                nonshardableRunner, TEST_COVERAGE_TARGET);
        String nonshardableInstr2 = String.format(INSTR_OUTPUT_FORMAT, nonshardableTestPkg2,
                nonshardableRunner, TEST_COVERAGE_TARGET);

        injectShellResponse(String.format("%s%s%s", shardableInstr,
                nonshardableInstr1, nonshardableInstr2), 2);

        // Instantiate InstalledInstrumentationTest shards
        EasyMock.replay(mMockTestDevice, mMockListener);
        InstalledInstrumentationsTest shard0 = createInstalledInstrumentationsTest();
        shard0.setDevice(mMockTestDevice);
        shard0.setShardIndex(0);
        shard0.setTotalShards(2);
        InstalledInstrumentationsTest shard1 = createInstalledInstrumentationsTest();
        shard1.setDevice(mMockTestDevice);
        shard1.setShardIndex(1);
        shard1.setTotalShards(2);

        // Run tests in first shard. There should be only two tests run: a test shard, and a
        // nonshardable test.

        shard0.run(mTestInfo, mMockListener);
        assertEquals(2, mMockInstrumentationTests.size());
        assertEquals(nonshardableTestPkg1, mMockInstrumentationTests.get(0).getPackageName());
        assertEquals(shardableTestPkg, mMockInstrumentationTests.get(1).getPackageName());
        assertEquals("0", mMockInstrumentationTests.get(1).getInstrumentationArg("shardIndex"));
        assertEquals("2", mMockInstrumentationTests.get(1).getInstrumentationArg("numShards"));
        mMockInstrumentationTests.clear();

        // Run tests in second shard. All tests should be accounted for.
        shard1.run(mTestInfo, mMockListener);
        assertEquals(2, mMockInstrumentationTests.size());
        assertEquals(nonshardableTestPkg2, mMockInstrumentationTests.get(0).getPackageName());
        assertEquals(shardableTestPkg, mMockInstrumentationTests.get(1).getPackageName());
        assertEquals("1", mMockInstrumentationTests.get(1).getInstrumentationArg("shardIndex"));
        assertEquals("2", mMockInstrumentationTests.get(1).getInstrumentationArg("numShards"));

        EasyMock.verify(mMockListener, mMockTestDevice);
    }

    @Test
    public void testRun_withCollectors() throws Exception {
        injectShellResponse(
                String.format(INSTR_OUTPUT_FORMAT, TEST_PKG, TEST_RUNNER, TEST_COVERAGE_TARGET), 1);

        ArgsOptionParser p = new ArgsOptionParser(mInstalledInstrTest);
        p.parse("--size", "small", "--force-abi", ABI);
        List<IMetricCollector> collectors = new ArrayList<>();
        collectors.add(new BaseDeviceMetricCollector());
        mInstalledInstrTest.setMetricCollectors(collectors);
        EasyMock.replay(mMockTestDevice, mMockListener);
        mInstalledInstrTest.run(mTestInfo, mMockListener);
        assertEquals(1, mMockInstrumentationTests.size());
        MockInstrumentationTest mockInstrumentationTest = mMockInstrumentationTests.get(0);
        assertEquals(mMockListener, mockInstrumentationTest.getListener());
        assertEquals(TEST_PKG, mockInstrumentationTest.getPackageName());
        assertEquals(TEST_RUNNER, mockInstrumentationTest.getRunnerName());
        assertEquals("small", mockInstrumentationTest.getTestSize());
        assertEquals(ABI, mockInstrumentationTest.getForceAbi());
        assertEquals(1, mockInstrumentationTest.getCollectors().size());

        EasyMock.verify(mMockListener, mMockTestDevice);
    }

    /**
     * Method to mock the executeShellCommand response
     *
     * @param shellResponse value to be returned by executeShellCommand
     * @param numExpectedCalls number of invocation expected
     * @throws DeviceNotAvailableException
     */
    private void injectShellResponse(final String shellResponse, int numExpectedCalls)
            throws DeviceNotAvailableException {
        IAnswer<Object> shellAnswer = new IAnswer<Object>() {
            @Override
            public String answer() throws Throwable {
                return shellResponse;
            }
        };
        mMockTestDevice.executeShellCommand(EasyMock.<String> anyObject());
        EasyMock.expectLastCall().andAnswer(shellAnswer).times(numExpectedCalls);
    }

    /**
     * Utility method for creating an InstalledInstrumentationsTest for testing.
     *
     * InstalledInstrumentationsTests need to create a MockInstrumentationTest, and we need to be
     * able to keep track of all mocks created in this manner.
     */
    private InstalledInstrumentationsTest createInstalledInstrumentationsTest() {
        InstalledInstrumentationsTest test = new InstalledInstrumentationsTest() {
            @Override
            InstrumentationTest createInstrumentationTest() {
                MockInstrumentationTest test = new MockInstrumentationTest();
                mMockInstrumentationTests.add(test);
                return test;
            }
        };
        return test;
    }

    /** Test that IllegalArgumentException is thrown when attempting run without setting device. */
    @Test
    public void testRun_noDevice() throws Exception {
        mInstalledInstrTest.setDevice(null);
        try {
            mInstalledInstrTest.run(mTestInfo, mMockListener);
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    /**
     * Test that IllegalArgumentException is thrown when attempting run when no instrumentations are
     * present.
     */
    @Test
    public void testRun_noInstr() throws Exception {
        injectShellResponse(PM_LIST_ERROR_OUTPUT, 1);
        EasyMock.replay(mMockTestDevice, mMockListener);
        try {
            mInstalledInstrTest.run(mTestInfo, mMockListener);
            fail("IllegalArgumentException not thrown");
        } catch (IllegalArgumentException e) {
            // expected
        }
    }

    @Test
    public void testSplit() {
        Collection<IRemoteTest> tests = mInstalledInstrTest.split(5);
        assertEquals(5, tests.size());
    }
}
