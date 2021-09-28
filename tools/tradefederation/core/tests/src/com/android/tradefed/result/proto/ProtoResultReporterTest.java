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
package com.android.tradefed.result.proto;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.proto.TestRecordProto.ChildReference;
import com.android.tradefed.result.proto.TestRecordProto.DebugInfo;
import com.android.tradefed.result.proto.TestRecordProto.DebugInfoContext;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.result.proto.TestRecordProto.TestRecord;
import com.android.tradefed.result.proto.TestRecordProto.TestStatus;
import com.android.tradefed.testtype.suite.ModuleDefinition;
import com.android.tradefed.util.SerializationUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.HashMap;

/** Unit tests for {@link ProtoResultReporter}. */
@RunWith(JUnit4.class)
public class ProtoResultReporterTest {

    private ProtoResultReporter mReporter;
    private TestRecord mFinalRecord;
    private IInvocationContext mInvocationContext;

    public class TestableProtoResultReporter extends ProtoResultReporter {
        @Override
        public void processFinalProto(TestRecord finalRecord) {
            mFinalRecord = finalRecord;
        }
    }

    @Before
    public void setUp() {
        mReporter = new TestableProtoResultReporter();
        mInvocationContext = new InvocationContext();
        mInvocationContext.setConfigurationDescriptor(new ConfigurationDescriptor());
    }

    /** Test an invocation with the proto being populated. */
    @Test
    public void testFinalizeProto() {
        // Invocation start
        mReporter.invocationStarted(mInvocationContext);
        // Run modules
        mReporter.testModuleStarted(createModuleContext("arm64 module1"));
        mReporter.testRunStarted("run1", 2);

        TestDescription test1 = new TestDescription("class1", "test1");
        mReporter.testStarted(test1, 5L);
        mReporter.testEnded(test1, 10L, new HashMap<String, Metric>());

        TestDescription test2 = new TestDescription("class1", "test2");
        mReporter.testStarted(test2, 11L);
        mReporter.testFailed(test2, "I failed");
        HashMap<String, Metric> metrics = new HashMap<String, Metric>();
        metrics.put("metric1", TfMetricProtoUtil.stringToMetric("value1"));
        // test log
        mReporter.logAssociation("log1", new LogFile("path", "url", false, LogDataType.TEXT, 5));

        mReporter.testEnded(test2, 60L, metrics);
        // run log
        mReporter.logAssociation(
                "run_log1", new LogFile("path", "url", false, LogDataType.LOGCAT, 5));
        mReporter.testRunEnded(50L, new HashMap<String, Metric>());

        mReporter.testModuleEnded();
        mReporter.testModuleStarted(createModuleContext("arm32 module1"));
        mReporter.testModuleEnded();
        // Invocation ends
        mReporter.invocationFailed(new NullPointerException());
        mReporter.invocationEnded(500L);

        //  ------ Verify that everything was populated ------
        assertNotNull(mFinalRecord.getTestRecordId());
        assertNotNull(mFinalRecord.getStartTime().getSeconds());
        assertNotNull(mFinalRecord.getEndTime().getSeconds());
        assertNotNull(mFinalRecord.getDebugInfo());

        // The invocation has 2 modules
        assertEquals(2, mFinalRecord.getChildrenCount());
        ChildReference child1 = mFinalRecord.getChildrenList().get(0);

        TestRecord module1 = child1.getInlineTestRecord();
        // module1 has one test run
        assertEquals(1, module1.getChildrenCount());
        TestRecord run1 = module1.getChildrenList().get(0).getInlineTestRecord();
        // run1 has 2 test cases
        assertEquals(2, run1.getChildrenCount());
        TestRecord testRecord1 = run1.getChildrenList().get(0).getInlineTestRecord();
        assertEquals(TestStatus.PASS, testRecord1.getStatus());
        TestRecord testRecord2 = run1.getChildrenList().get(1).getInlineTestRecord();
        assertEquals(TestStatus.FAIL, testRecord2.getStatus());
        assertEquals("I failed", testRecord2.getDebugInfo().getTrace());
        // test 2 has 1 metric and 1 log artifact
        assertEquals(1, testRecord2.getMetricsMap().size());
        assertEquals(1, testRecord2.getArtifactsMap().size());
        // run 1 has one log file
        assertEquals(1, run1.getArtifactsMap().size());
    }

    /** Test when testRunFailed in called from within a test context (testStarted/TestEnded). */
    @Test
    public void testRunFail_interleavedWithTest() {
        mReporter.invocationStarted(mInvocationContext);
        // Run modules
        mReporter.testRunStarted("run1", 2);

        TestDescription test1 = new TestDescription("class1", "test1");
        mReporter.testStarted(test1, 5L);
        // test run failed inside a test
        mReporter.testRunFailed("run failure");

        mReporter.testEnded(test1, 10L, new HashMap<String, Metric>());

        mReporter.testRunEnded(50L, new HashMap<String, Metric>());
        // Invocation ends
        mReporter.invocationEnded(500L);

        assertEquals(1, mFinalRecord.getChildrenCount());
        TestRecord run1 = mFinalRecord.getChildrenList().get(0).getInlineTestRecord();
        assertEquals("run failure", run1.getDebugInfo().getErrorMessage());
    }

    /** Test an invocation with the proto invocation failure being populated. */
    @Test
    public void testInvocationFailure() throws Exception {
        // Invocation start
        mReporter.invocationStarted(mInvocationContext);
        // Invocation ends
        FailureDescription invocationFailure = FailureDescription.create("error");
        invocationFailure.setFailureStatus(FailureStatus.INFRA_FAILURE);
        invocationFailure.setCause(new NullPointerException("error"));
        mReporter.invocationFailed(invocationFailure);
        mReporter.invocationEnded(500L);

        //  ------ Verify that everything was populated ------
        assertNotNull(mFinalRecord.getTestRecordId());
        assertNotNull(mFinalRecord.getStartTime().getSeconds());
        assertNotNull(mFinalRecord.getEndTime().getSeconds());
        assertNotNull(mFinalRecord.getDebugInfo());

        DebugInfo invocFailure = mFinalRecord.getDebugInfo();
        assertEquals("error", invocFailure.getErrorMessage());
        assertEquals(FailureStatus.INFRA_FAILURE, invocFailure.getFailureStatus());
        assertNotNull(invocFailure.getDebugInfoContext());
        DebugInfoContext debugContext = invocFailure.getDebugInfoContext();
        String serializedException = debugContext.getErrorType();
        NullPointerException npe =
                (NullPointerException) SerializationUtil.deserialize(serializedException);
        assertEquals("error", npe.getMessage());
    }

    /** Helper to create a module context. */
    private IInvocationContext createModuleContext(String moduleId) {
        IInvocationContext context = new InvocationContext();
        context.addInvocationAttribute(ModuleDefinition.MODULE_ID, moduleId);
        context.setConfigurationDescriptor(new ConfigurationDescriptor());
        return context;
    }
}
