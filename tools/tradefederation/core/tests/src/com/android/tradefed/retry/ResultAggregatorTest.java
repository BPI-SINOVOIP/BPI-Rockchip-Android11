/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.tradefed.retry;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.MultiFailureDescription;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.retry.ISupportGranularResults;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/** Unit tests for {@link ResultAggregator}. */
@RunWith(JUnit4.class)
public class ResultAggregatorTest {

    private TestableResultAggregator mAggregator;
    private ILogSaverListener mAggListener;
    private ITestDetailedReceiver mDetailedListener;
    private IInvocationContext mInvocationContext;
    private IInvocationContext mModuleContext;

    private interface ITestDetailedReceiver
            extends ISupportGranularResults, ITestInvocationListener, ILogSaverListener {}

    private class TestableResultAggregator extends ResultAggregator {

        private String mCurrentRunError = null;

        public TestableResultAggregator(
                List<ITestInvocationListener> listeners, RetryStrategy strategy) {
            super(listeners, strategy);
        }

        @Override
        String getInvocationMetricRunError() {
            return mCurrentRunError;
        }

        @Override
        void addInvocationMetricRunError(String errors) {
            mCurrentRunError = errors;
        }
    }

    @Before
    public void setUp() {
        mAggListener = EasyMock.createMock(ILogSaverListener.class);
        mDetailedListener = EasyMock.createMock(ITestDetailedReceiver.class);
        mInvocationContext = new InvocationContext();
        mInvocationContext.addDeviceBuildInfo(
                ConfigurationDef.DEFAULT_DEVICE_NAME, new BuildInfo());
        mModuleContext = new InvocationContext();
    }

    @Test
    public void testForwarding() {
        mDetailedListener = EasyMock.createStrictMock(ITestDetailedReceiver.class);
        LogFile test1Log = new LogFile("test1", "url", LogDataType.TEXT);
        LogFile test2LogBefore = new LogFile("test2-before", "url", LogDataType.TEXT);
        LogFile test2LogAfter = new LogFile("test2-after", "url", LogDataType.TEXT);
        LogFile testRun1LogBefore = new LogFile("test-run1-before", "url", LogDataType.TEXT);
        LogFile testRun1LogAfter = new LogFile("test-run1-after", "url", LogDataType.TEXT);
        LogFile beforeEnd = new LogFile("path", "url", LogDataType.TEXT);
        LogFile betweenAttemptsLog = new LogFile("between-attempts", "url", LogDataType.TEXT);
        LogFile moduleLog = new LogFile("module-log", "url", LogDataType.TEXT);
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.logAssociation("test1-log", test1Log);
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.logAssociation("test2-before-log", test2LogBefore);
        mDetailedListener.testFailed(test2, FailureDescription.create("I failed. retry me."));
        mDetailedListener.logAssociation("test2-after-log", test2LogAfter);
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.logAssociation("test-run1-before-log", testRun1LogBefore);
        mDetailedListener.logAssociation("test-run1-after-log", testRun1LogAfter);
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.logAssociation("between-attempts", betweenAttemptsLog);
        mDetailedListener.logAssociation("module-log", moduleLog);

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.logAssociation("test1-log", test1Log);
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.logAssociation("test2-before-log", test2LogBefore);
        mAggListener.logAssociation("test2-after-log", test2LogAfter);
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.logAssociation("test-run1-before-log", testRun1LogBefore);
        mAggListener.logAssociation("test-run1-after-log", testRun1LogAfter);
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());
        mAggListener.logAssociation("between-attempts", betweenAttemptsLog);
        mAggListener.logAssociation("module-log", moduleLog);
        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();
        mAggListener.logAssociation("before-end", beforeEnd);
        mAggListener.invocationEnded(500L);
        mDetailedListener.logAssociation("before-end", beforeEnd);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.logAssociation("test1-log", test1Log);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.logAssociation("test2-before-log", test2LogBefore);
        mAggregator.testFailed(test2, FailureDescription.create("I failed. retry me."));
        mAggregator.logAssociation("test2-after-log", test2LogAfter);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.logAssociation("test-run1-before-log", testRun1LogBefore);
        mAggregator.testRunFailed("run fail");
        mAggregator.logAssociation("test-run1-after-log", testRun1LogAfter);
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.logAssociation("between-attempts", betweenAttemptsLog);
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.logAssociation("module-log", moduleLog);
        mAggregator.testModuleEnded();
        mAggregator.logAssociation("before-end", beforeEnd);
        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertEquals("run fail", mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_runFailure() {
        mDetailedListener = EasyMock.createStrictMock(ITestDetailedReceiver.class);
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);
        MultiFailureDescription aggFailure =
                new MultiFailureDescription(
                        FailureDescription.create("run fail"),
                        FailureDescription.create("run fail 2"));
        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunFailed(aggFailure);
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunFailed(aggFailure);
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();
        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("run fail");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("run fail 2");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.testModuleEnded();
        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_runFailure_noRerun() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunFailed(EasyMock.eq(FailureDescription.create("run fail")));
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testFailed(test2, FailureDescription.create("I failed. retry me."));
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunFailed(EasyMock.eq(FailureDescription.create("run fail")));
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();
        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("run fail");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();
        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_runFailure_aggregation() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testModuleEnded();

        mDetailedListener.testModuleStarted(mModuleContext);
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testRunFailed(EasyMock.eq(FailureDescription.create("run fail")));
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testFailed(
                test2,
                new MultiFailureDescription(
                        FailureDescription.create("I failed. retry me."),
                        FailureDescription.create("I failed. retry me.")));
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());
        mAggListener.testModuleEnded();

        mAggListener.testModuleStarted(mModuleContext);
        mAggListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testRunFailed(EasyMock.eq(FailureDescription.create("run fail")));
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();
        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();

        mAggregator.testModuleStarted(mModuleContext);
        mAggregator.testRunStarted("run2", 1, 0);
        mAggregator.testRunFailed(FailureDescription.create("run fail"));
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();
        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_noModules() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);
        LogFile afterRunLog = new LogFile("after-run", "url", LogDataType.TEXT);

        mDetailedListener = EasyMock.createStrictMock(ITestDetailedReceiver.class);
        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.logAssociation("after-run", afterRunLog);

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(900L, new HashMap<String, Metric>());

        mAggListener.logAssociation("after-run", afterRunLog);
        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.logAssociation("after-run", afterRunLog);
        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertEquals("I failed", mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_singleRun_noModules_runFailures() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);
        MultiFailureDescription aggFailure =
                new MultiFailureDescription(
                        FailureDescription.create("I failed"),
                        FailureDescription.create("I failed 2"));
        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunFailed(aggFailure);
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunFailed(aggFailure);
        mAggListener.testRunEnded(900L, new HashMap<String, Metric>());

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed 2");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_noModules_runFailures() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);
        MultiFailureDescription aggFailure =
                new MultiFailureDescription(
                        FailureDescription.create("I failed"),
                        FailureDescription.create("I failed 2"));
        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunFailed(EasyMock.eq(aggFailure));
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());

        mAggListener.testRunFailed(EasyMock.eq(aggFailure));
        mAggListener.testRunEnded(900L, new HashMap<String, Metric>());
        mAggListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed 2");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // run 2
        mAggregator.testRunStarted("run2", 1, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    /** Test aggregation of results coming from a module first then from a simple test run. */
    @Test
    public void testForwarding_module_noModule() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();

        // Detailed receives the breakdown for non-module
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testFailed(test1, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results for non-module
        mAggListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(900L, new HashMap<String, Metric>());

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();

        // New run that is not a module
        mAggregator.testRunStarted("run2", 1, 0);
        mAggregator.testStarted(test1);
        mAggregator.testFailed(test1, "I failed. retry me.");
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.testRunStarted("run2", 1, 1);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertEquals("I failed", mAggregator.getInvocationMetricRunError());
    }

    /** Test aggregation of results coming from a simple test run first then from a module. */
    @Test
    public void testForwarding_noModule_module() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();

        // Detailed receives the breakdown for non-module
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testFailed(test1, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results for non-module
        mAggListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(900L, new HashMap<String, Metric>());

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        // First run that is not a module
        mAggregator.testRunStarted("run2", 1, 0);
        mAggregator.testStarted(test1);
        mAggregator.testFailed(test1, "I failed. retry me.");
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.testRunStarted("run2", 1, 1);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        // Module start
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_noModule_module_runFailure() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);
        MultiFailureDescription aggFailure =
                new MultiFailureDescription(
                        FailureDescription.create("I failed"),
                        FailureDescription.create("I failed 2"));
        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();

        // Detailed receives the breakdown for non-module
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testFailed(test1, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunFailed(aggFailure);
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results for non-module
        mAggListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunFailed(aggFailure);
        mAggListener.testRunEnded(900L, new HashMap<String, Metric>());

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        // First run that is not a module
        mAggregator.testRunStarted("run2", 1, 0);
        mAggregator.testStarted(test1);
        mAggregator.testFailed(test1, "I failed. retry me.");
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.testRunStarted("run2", 1, 1);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed 2");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        // Module start
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    /** Test when two modules follow each others. */
    @Test
    public void testForwarding_module_module() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();

        // second module
        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testFailed(test1, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results for non-module
        mAggListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);

        // Module 1 starts
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();

        // Module 2 starts
        mAggregator.testModuleStarted(mModuleContext);
        mAggregator.testRunStarted("run2", 1, 0);
        mAggregator.testStarted(test1);
        mAggregator.testFailed(test1, "I failed. retry me.");
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.testRunStarted("run2", 1, 1);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }

    @Test
    public void testForwarding_module_pass_fail_fail() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        mAggListener.testModuleStarted(mModuleContext);
        mDetailedListener.testModuleStarted(mModuleContext);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(2), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.testModuleEnded();
        mDetailedListener.testModuleEnded();

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);

        // Module 1 starts
        mAggregator.testModuleStarted(mModuleContext);
        // Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("failed2");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Attempt 3
        mAggregator.testRunStarted("run1", 2, 2);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("failed3");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        mAggregator.testModuleEnded();

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertEquals(
                "There were 2 failures:\n  failed2\n  failed3",
                mAggregator.getInvocationMetricRunError());
    }

    /**
     * Ensure that we handle the aggregation properly when a single IRemoteTest generates several
     * testRuns and we retry them all (due to no filter support). In this case, the retry attempt
     * will not be right after the original attempt.
     */
    @Test
    public void testForwarding_noModules_runFailures_unordered() {
        TestDescription test1 = new TestDescription("classname", "test1");
        TestDescription test2 = new TestDescription("classname", "test2");
        ILogSaver logger = EasyMock.createMock(ILogSaver.class);

        EasyMock.expect(mDetailedListener.supportGranularResults()).andStubReturn(true);

        // Invocation level
        mAggListener.setLogSaver(logger);
        mAggListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mAggListener.getSummary()).andStubReturn(null);
        mDetailedListener.setLogSaver(logger);
        mDetailedListener.invocationStarted(mInvocationContext);
        EasyMock.expect(mDetailedListener.getSummary()).andStubReturn(null);

        // Detailed receives the breakdown
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testFailed(test2, "I failed. retry me.");
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        // TODO: Fix detailed reporting that should clear this failure.
        mDetailedListener.testRunFailed(FailureDescription.create("I failed"));
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());
        mDetailedListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(1), EasyMock.anyLong());
        mDetailedListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mDetailedListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mDetailedListener.testRunEnded(450L, new HashMap<String, Metric>());

        // Aggregated listeners receives the aggregated results
        mAggListener.testRunStarted(
                EasyMock.eq("run1"), EasyMock.eq(2), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testStarted(EasyMock.eq(test2), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test2),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(900L, new HashMap<String, Metric>());
        mAggListener.testRunStarted(
                EasyMock.eq("run2"), EasyMock.eq(1), EasyMock.eq(0), EasyMock.anyLong());
        mAggListener.testStarted(EasyMock.eq(test1), EasyMock.anyLong());
        mAggListener.testEnded(
                EasyMock.eq(test1),
                EasyMock.anyLong(),
                EasyMock.<HashMap<String, Metric>>anyObject());
        mAggListener.testRunEnded(450L, new HashMap<String, Metric>());

        mAggListener.invocationEnded(500L);
        mDetailedListener.invocationEnded(500L);

        EasyMock.replay(mAggListener, mDetailedListener);
        mAggregator =
                new TestableResultAggregator(
                        Arrays.asList(mAggListener, mDetailedListener),
                        RetryStrategy.RETRY_ANY_FAILURE);
        mAggregator.setLogSaver(logger);
        mAggregator.invocationStarted(mInvocationContext);
        // Run 1 - Attempt 1
        mAggregator.testRunStarted("run1", 2, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testStarted(test2);
        mAggregator.testFailed(test2, "I failed. retry me.");
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunFailed("I failed");
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Run 2 - Attempt 1
        mAggregator.testRunStarted("run2", 1, 0);
        mAggregator.testStarted(test1);
        mAggregator.testEnded(test1, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());
        // Run 1 - Attempt 2
        mAggregator.testRunStarted("run1", 2, 1);
        mAggregator.testStarted(test2);
        mAggregator.testEnded(test2, new HashMap<String, Metric>());
        mAggregator.testRunEnded(450L, new HashMap<String, Metric>());

        mAggregator.invocationEnded(500L);
        EasyMock.verify(mAggListener, mDetailedListener);
        assertNull(mAggregator.getInvocationMetricRunError());
    }
}
