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
package com.android.tradefed.invoker.shard;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.shard.token.ITokenRequest;
import com.android.tradefed.log.ILogRegistry;
import com.android.tradefed.log.ILogRegistry.EventType;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IReportNotExecuted;
import com.android.tradefed.testtype.StubTest;
import com.android.tradefed.testtype.suite.ITestSuite;
import com.android.tradefed.testtype.suite.ITestSuiteTest.TestSuiteImpl;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/** Unit tests for {@link TestsPoolPoller}. */
@RunWith(JUnit4.class)
public class TestsPoolPollerTest {

    private ITestInvocationListener mListener;
    private ITestDevice mDevice;
    private List<IMetricCollector> mMetricCollectors;
    private ILogRegistry mMockRegistry;
    private IConfiguration mConfiguration;
    private TestInformation mTestInfo;

    @Before
    public void setUp() {
        mListener = Mockito.mock(ITestInvocationListener.class);
        mDevice = Mockito.mock(ITestDevice.class);
        mMockRegistry = Mockito.mock(ILogRegistry.class);
        Mockito.doReturn("serial").when(mDevice).getSerialNumber();
        mConfiguration = new Configuration("test", "test");
        mMetricCollectors = new ArrayList<>();
        IInvocationContext context = new InvocationContext();
        context.addAllocatedDevice("device", mDevice);
        context.addDeviceBuildInfo("device", new BuildInfo());
        mTestInfo = TestInformation.newBuilder().setInvocationContext(context).build();
    }

    /**
     * Tests that {@link TestsPoolPoller#poll()} returns a {@link IRemoteTest} from the pool or null
     * when the pool is empty.
     */
    @Test
    public void testMultiPolling() {
        int numTests = 5;
        List<IRemoteTest> testsList = new ArrayList<>();
        for (int i = 0; i < numTests; i++) {
            testsList.add(new StubTest());
        }
        CountDownLatch tracker = new CountDownLatch(2);
        TestsPoolPoller poller1 = new TestsPoolPoller(testsList, tracker);
        TestsPoolPoller poller2 = new TestsPoolPoller(testsList, tracker);
        // initial size
        assertEquals(numTests, testsList.size());
        assertNotNull(poller1.poll());
        assertEquals(numTests - 1, testsList.size());
        assertNotNull(poller2.poll());
        assertEquals(numTests - 2, testsList.size());
        assertNotNull(poller1.poll());
        assertNotNull(poller1.poll());
        assertNotNull(poller2.poll());
        assertTrue(testsList.isEmpty());
        // once empty poller returns null
        assertNull(poller1.poll());
        assertNull(poller2.poll());
    }

    /**
     * Tests that {@link TestsPoolPoller#run(TestInformation, ITestInvocationListener)} is properly
     * running and redirecting the invocation callbacks.
     */
    @Test
    public void testPollingRun() throws Exception {
        StubTest first = new StubTest();
        OptionSetter setterFirst = new OptionSetter(first);
        setterFirst.setOptionValue("run-a-test", "true");
        int numTests = 5;
        List<IRemoteTest> testsList = new ArrayList<>();
        testsList.add(first);
        for (int i = 0; i < numTests - 1; i++) {
            IRemoteTest test = new StubTest();
            OptionSetter setter = new OptionSetter(test);
            setter.setOptionValue("run-a-test", "true");
            testsList.add(test);
        }
        CountDownLatch tracker = new CountDownLatch(1);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tracker);
        poller.setConfiguration(mConfiguration);
        poller.setMetricCollectors(mMetricCollectors);
        poller.run(mTestInfo, mListener);
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunStarted(Mockito.anyString(), Mockito.anyInt());
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        assertEquals(0, tracker.getCount());

        // Ensure that the configuration set is the one that we passed.
        assertEquals(mConfiguration, first.getConfiguration());
    }

    /**
     * Tests that {@link TestsPoolPoller#run(TestInformation, ITestInvocationListener)} will
     * continue to run tests even if one of them throws a {@link RuntimeException}.
     */
    @Test
    public void testRun_runtimeException() throws Exception {
        List<IRemoteTest> testsList = new ArrayList<>();
        // Add one bad test first that will throw an exception.
        IRemoteTest badTest = new StubTest();
        OptionSetter setter = new OptionSetter(badTest);
        setter.setOptionValue("test-throw-runtime", "true");
        testsList.add(badTest);
        // Add tests that can run
        int numTests = 5;
        for (int i = 0; i < numTests; i++) {
            IRemoteTest test = new StubTest();
            OptionSetter s = new OptionSetter(test);
            s.setOptionValue("run-a-test", "true");
            testsList.add(test);
        }
        CountDownLatch tracker = new CountDownLatch(1);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tracker);
        poller.setMetricCollectors(mMetricCollectors);
        poller.run(mTestInfo, mListener);
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunStarted(Mockito.anyString(), Mockito.anyInt());
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        assertEquals(0, tracker.getCount());
    }

    /**
     * Tests that {@link TestsPoolPoller#run(TestInformation, ITestInvocationListener)} will
     * continue to run tests even if one of them throws a {@link DeviceUnresponsiveException}.
     */
    @Test
    public void testRun_deviceUnresponsive() throws Exception {
        List<IRemoteTest> testsList = new ArrayList<>();
        // Add one bad test first that will throw an exception.
        IRemoteTest badTest = new StubTest();
        OptionSetter setter = new OptionSetter(badTest);
        setter.setOptionValue("test-throw-unresponsive", "true");
        testsList.add(badTest);
        // Add tests that can run
        int numTests = 5;
        for (int i = 0; i < numTests; i++) {
            IRemoteTest test = new StubTest();
            OptionSetter s = new OptionSetter(test);
            s.setOptionValue("run-a-test", "true");
            testsList.add(test);
        }
        CountDownLatch tracker = new CountDownLatch(1);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tracker);
        poller.setMetricCollectors(mMetricCollectors);
        poller.run(mTestInfo, mListener);
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunStarted(Mockito.anyString(), Mockito.anyInt());
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        assertEquals(0, tracker.getCount());
    }

    /**
     * Tests that {@link TestsPoolPoller#run(TestInformation, ITestInvocationListener)} will stop to
     * run tests if one of them throws a {@link DeviceNotAvailableException}.
     */
    @Test
    public void testRun_dnae() throws Exception {
        List<IRemoteTest> testsList = new ArrayList<>();
        // Add one bad test first that will throw an exception.
        IRemoteTest badTest = new StubTest();
        OptionSetter setter = new OptionSetter(badTest);
        setter.setOptionValue("test-throw-not-available", "true");
        testsList.add(badTest);
        // Add tests that can run
        int numTests = 5;
        for (int i = 0; i < numTests; i++) {
            IRemoteTest test = new StubTest();
            OptionSetter s = new OptionSetter(test);
            s.setOptionValue("run-a-test", "true");
            testsList.add(test);
        }
        CountDownLatch tracker = new CountDownLatch(1);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tracker);
        poller.setMetricCollectors(mMetricCollectors);
        poller.setLogRegistry(mMockRegistry);
        try {
            poller.run(mTestInfo, mListener);
            fail("Should have thrown an exception.");
        } catch (DeviceNotAvailableException expected) {
            // expected
        }
        // We expect no callbacks on these, poller should stop early.
        Mockito.verify(mListener, Mockito.times(0))
                .testRunStarted(Mockito.anyString(), Mockito.anyInt());
        Mockito.verify(mListener, Mockito.times(0))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, String>>any());
        assertEquals(0, tracker.getCount());
        Mockito.verify(mMockRegistry)
                .logEvent(
                        Mockito.eq(LogLevel.DEBUG),
                        Mockito.eq(EventType.SHARD_POLLER_EARLY_TERMINATION),
                        Mockito.any());
    }

    /**
     * Test that a runner that implements {@link IReportNotExecuted} can report the non-executed
     * tests when the DNAE occurs.
     */
    @Test
    public void testRun_dnae_reportNotExecuted() throws Exception {
        List<IRemoteTest> testsList = new ArrayList<>();
        // Add one bad test first that will throw an exception.
        IRemoteTest badTest = new StubTest();
        OptionSetter setter = new OptionSetter(badTest);
        setter.setOptionValue("test-throw-not-available", "true");
        testsList.add(badTest);
        // Add tests that from a suite that can report their not executed tests.
        int numTests = 5;
        ITestSuite suite = new TestSuiteImpl(numTests);
        testsList.addAll(suite.split(3, mTestInfo));
        CountDownLatch tracker = new CountDownLatch(1);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tracker);
        poller.setMetricCollectors(mMetricCollectors);
        poller.setLogRegistry(mMockRegistry);
        try {
            poller.run(mTestInfo, mListener);
            fail("Should have thrown an exception.");
        } catch (DeviceNotAvailableException expected) {
            // expected
        }
        // We expect all the non-executed tests to be reported.
        Mockito.verify(mListener, Mockito.times(1))
                .testRunStarted(
                        Mockito.eq("test"), Mockito.eq(0), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mListener, Mockito.times(1))
                .testRunStarted(
                        Mockito.eq("test1"), Mockito.eq(0), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mListener, Mockito.times(1))
                .testRunStarted(
                        Mockito.eq("test2"), Mockito.eq(0), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mListener, Mockito.times(1))
                .testRunStarted(
                        Mockito.eq("test3"), Mockito.eq(0), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mListener, Mockito.times(1))
                .testRunStarted(
                        Mockito.eq("test4"), Mockito.eq(0), Mockito.eq(0), Mockito.anyLong());
        Mockito.verify(mListener, Mockito.times(5))
                .testRunFailed(
                        FailureDescription.create(
                                IReportNotExecuted.NOT_EXECUTED_FAILURE,
                                FailureStatus.NOT_EXECUTED));
        Mockito.verify(mListener, Mockito.times(5))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        assertEquals(0, tracker.getCount());
        Mockito.verify(mMockRegistry)
                .logEvent(
                        Mockito.eq(LogLevel.DEBUG),
                        Mockito.eq(EventType.SHARD_POLLER_EARLY_TERMINATION),
                        Mockito.any());
    }

    /**
     * If a device not available exception is thrown from a tests, and the poller is not the last
     * one alive, we wait and attempt to recover the device. In case of success, execution will
     * proceed.
     */
    @Test
    public void testRun_dnae_NotLastDevice() throws Exception {
        List<IRemoteTest> testsList = new ArrayList<>();
        // Add one bad test first that will throw an exception.
        IRemoteTest badTest = new StubTest();
        OptionSetter setter = new OptionSetter(badTest);
        setter.setOptionValue("test-throw-not-available", "true");
        testsList.add(badTest);
        // Add tests that can run
        int numTests = 5;
        for (int i = 0; i < numTests; i++) {
            IRemoteTest test = new StubTest();
            OptionSetter s = new OptionSetter(test);
            s.setOptionValue("run-a-test", "true");
            testsList.add(test);
        }
        CountDownLatch tracker = new CountDownLatch(3);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tracker);
        poller.setMetricCollectors(mMetricCollectors);

        poller.run(mTestInfo, mListener);
        // The callbacks from all the other tests because the device was recovered
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunStarted(Mockito.anyString(), Mockito.anyInt());
        Mockito.verify(mListener, Mockito.times(numTests)).testStarted(Mockito.any());
        Mockito.verify(mListener, Mockito.times(numTests))
                .testEnded(Mockito.any(), Mockito.<HashMap<String, Metric>>any());
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        Mockito.verify(mDevice).waitForDeviceAvailable(Mockito.anyLong());
        Mockito.verify(mDevice).reboot();
        assertEquals(2, tracker.getCount());
    }

    /**
     * If a device not available exception is thrown from a tests, and the poller is not the last
     * one alive, we wait and attempt to recover the device. In case of failure, execution will not
     * proceed and we log an event.
     */
    @Test
    public void testRun_dnae_NotLastDevice_offline() throws Exception {
        List<IRemoteTest> testsList = new ArrayList<>();
        // Add one bad test first that will throw an exception.
        IRemoteTest badTest = new StubTest();
        OptionSetter setter = new OptionSetter(badTest);
        setter.setOptionValue("test-throw-not-available", "true");
        testsList.add(badTest);
        // Add tests that can run
        int numTests = 5;
        for (int i = 0; i < numTests; i++) {
            IRemoteTest test = new StubTest();
            OptionSetter s = new OptionSetter(test);
            s.setOptionValue("run-a-test", "true");
            testsList.add(test);
        }
        CountDownLatch tracker = new CountDownLatch(3);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tracker);
        poller.setMetricCollectors(mMetricCollectors);
        poller.setLogRegistry(mMockRegistry);

        Mockito.doThrow(new DeviceNotAvailableException("test", "serial"))
                .when(mDevice)
                .waitForDeviceAvailable(Mockito.anyLong());
        try {
            poller.run(mTestInfo, mListener);
            fail("Should have thrown an exception.");
        } catch (DeviceNotAvailableException expected) {
            assertEquals(StubTest.DNAE_MESSAGE, expected.getMessage());
        }
        // The callbacks from all the other tests are not called because device was unavailable.
        Mockito.verify(mListener, Mockito.times(0))
                .testRunStarted(Mockito.anyString(), Mockito.anyInt());
        Mockito.verify(mListener, Mockito.times(0)).testStarted(Mockito.any());
        Mockito.verify(mListener, Mockito.times(0))
                .testEnded(Mockito.any(), Mockito.<HashMap<String, String>>any());
        Mockito.verify(mListener, Mockito.times(0))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, String>>any());
        Mockito.verify(mDevice).waitForDeviceAvailable(Mockito.anyLong());
        Mockito.verify(mDevice, Mockito.times(0)).reboot();
        assertEquals(2, tracker.getCount());

        Mockito.verify(mMockRegistry)
                .logEvent(
                        Mockito.eq(LogLevel.DEBUG),
                        Mockito.eq(EventType.SHARD_POLLER_EARLY_TERMINATION),
                        Mockito.any());
    }

    @Test
    public void testPolling_tokens_notExecuted() throws Exception {
        int numTests = 5;
        List<IRemoteTest> testsList = new ArrayList<>();
        for (int i = 0; i < numTests; i++) {
            IRemoteTest test = new StubTest();
            OptionSetter setter = new OptionSetter(test);
            setter.setOptionValue("run-a-test", "true");
            testsList.add(test);
        }

        ITokenRequest tokenTest = new TokenTestClass();
        ITokenRequest tokenTest2 = new TokenTestClass();

        Collection<ITokenRequest> tokenList = new ArrayList<>();
        tokenList.add(tokenTest);
        tokenList.add(tokenTest2);

        CountDownLatch tracker = new CountDownLatch(1);
        TestsPoolPoller poller = new TestsPoolPoller(testsList, tokenList, tracker);
        poller.setMetricCollectors(mMetricCollectors);
        poller.run(mTestInfo, mListener);
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunStarted(Mockito.anyString(), Mockito.anyInt());
        Mockito.verify(mListener, Mockito.times(numTests))
                .testRunEnded(Mockito.anyLong(), Mockito.<HashMap<String, Metric>>any());
        assertEquals(0, tracker.getCount());

        // The two tokens test did not execute.
        Mockito.verify(mListener, Mockito.times(2))
                .testFailed(
                        new TestDescription("token.class", "token.test"),
                        "Test did not run. No token '[SIM_CARD]' matching it on any device.");
    }
}
