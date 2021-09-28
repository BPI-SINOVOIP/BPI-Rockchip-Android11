/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.device.collectors;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.os.Bundle;
import androidx.test.runner.AndroidJUnit4;
import com.android.helpers.SimpleperfHelper;
import java.util.HashMap;
import java.util.Map;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

/**
 * Android Unit tests for {@link SimpleperfListener}.
 *
 * <p>To run: atest CollectorDeviceLibTest:android.device.collectors.SimpleperfListenerTest
 */
@RunWith(AndroidJUnit4.class)
public class SimpleperfListenerTest {

    // A {@code Description} to pass when faking a test run start call.
    private static final Description FAKE_DESCRIPTION = Description.createSuiteDescription("run");

    private static final Description FAKE_TEST_DESCRIPTION =
            Description.createTestDescription("class", "method");

    private Description mRunDesc;
    private Description mTest1Desc;
    private Description mTest2Desc;
    private SimpleperfListener mListener;
    @Mock private Instrumentation mInstrumentation;
    private Map<String, Integer> mInvocationCount;
    private DataRecord mDataRecord;

    @Spy private SimpleperfHelper mSimpleperfHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mRunDesc = Description.createSuiteDescription("run");
        mTest1Desc = Description.createTestDescription("run", "test1");
        mTest2Desc = Description.createTestDescription("run", "test2");
    }

    private SimpleperfListener initListener(Bundle b) {
        mInvocationCount = new HashMap<>();

        SimpleperfListener listener =
                spy(new SimpleperfListener(b, mSimpleperfHelper, mInvocationCount));

        mDataRecord = listener.createDataRecord();
        listener.setInstrumentation(mInstrumentation);
        return listener;
    }

    /*
     * Verify simpleperf start and stop collection methods called exactly once for single test.
     */
    @Test
    public void testSimpleperfPerTestSuccessFlow() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);
        doReturn(true).when(mSimpleperfHelper).startCollecting();
        doReturn(true).when(mSimpleperfHelper).stopCollecting(anyString());
        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mSimpleperfHelper, times(1)).startCollecting();
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mSimpleperfHelper, times(1)).stopCollecting(anyString());
    }

    /*
     * Verify stop collecting called exactly once when the test failed and the
     * skip test failure mmetrics is enabled.
     */
    @Test
    public void testSimpleperfPerTestFailureFlowDefault() throws Exception {
        Bundle b = new Bundle();
        b.putString(SimpleperfListener.SKIP_TEST_FAILURE_METRICS, "false");
        mListener = initListener(b);

        doReturn(true).when(mSimpleperfHelper).startCollecting();
        doReturn(true).when(mSimpleperfHelper).stopCollecting(anyString());
        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mSimpleperfHelper, times(1)).startCollecting();

        // Test fail behaviour
        Failure failureDesc = new Failure(FAKE_TEST_DESCRIPTION, new Exception());
        mListener.onTestFail(mDataRecord, mTest1Desc, failureDesc);
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mSimpleperfHelper, times(1)).stopCollecting(anyString());
    }

    /*
     * Verify stop simpleperf called exactly once when the test failed and the
     * skip test failure metrics is enabled.
     */
    @Test
    public void testSimpleperfPerTestFailureFlowWithSkipMmetrics() throws Exception {
        Bundle b = new Bundle();
        b.putString(SimpleperfListener.SKIP_TEST_FAILURE_METRICS, "true");
        mListener = initListener(b);

        doReturn(true).when(mSimpleperfHelper).startCollecting();
        doReturn(true).when(mSimpleperfHelper).stopSimpleperf();
        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mSimpleperfHelper, times(1)).startCollecting();

        // Test fail behaviour
        Failure failureDesc = new Failure(FAKE_TEST_DESCRIPTION, new Exception());
        mListener.onTestFail(mDataRecord, mTest1Desc, failureDesc);
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mSimpleperfHelper, times(1)).stopSimpleperf();
    }

    /*
     * Verify simpleperf start and stop collection methods called exactly once for test run.
     * and not during each test method.
     */
    @Test
    public void testSimpleperfPerRunSuccessFlow() throws Exception {
        Bundle b = new Bundle();
        b.putString(SimpleperfListener.COLLECT_PER_RUN, "true");
        mListener = initListener(b);
        doReturn(true).when(mSimpleperfHelper).startCollecting();
        doReturn(true).when(mSimpleperfHelper).stopCollecting(anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        verify(mSimpleperfHelper, times(1)).startCollecting();
        mListener.testStarted(mTest1Desc);
        verify(mSimpleperfHelper, times(1)).startCollecting();
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mSimpleperfHelper, times(0)).stopCollecting(anyString());
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        verify(mSimpleperfHelper, times(1)).stopCollecting(anyString());
    }

    /*
     * Verify stop is not called if Simpleperf start did not succeed.
     */
    @Test
    public void testSimpleperfPerRunFailureFlow() throws Exception {
        Bundle b = new Bundle();
        b.putString(SimpleperfListener.COLLECT_PER_RUN, "true");
        mListener = initListener(b);
        doReturn(false).when(mSimpleperfHelper).startCollecting();

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        verify(mSimpleperfHelper, times(1)).startCollecting();
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        verify(mSimpleperfHelper, times(0)).stopCollecting(anyString());
    }

    /*
     * Verify simpleperf stop is not invoked if start did not succeed.
     */
    @Test
    public void testSimpleperfStartFailureFlow() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);
        doReturn(false).when(mSimpleperfHelper).startCollecting();

        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mSimpleperfHelper, times(1)).startCollecting();
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mSimpleperfHelper, times(0)).stopCollecting(anyString());
    }

    /*
     * Verify test method invocation count is updated successfully based on the number of times the
     * test method is invoked.
     */
    @Test
    public void testSimpleperfInvocationCount() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);
        doReturn(true).when(mSimpleperfHelper).startCollecting();
        doReturn(true).when(mSimpleperfHelper).stopCollecting(anyString());

        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test1 invocation 1 start behavior
        mListener.testStarted(mTest1Desc);
        verify(mSimpleperfHelper, times(1)).startCollecting();
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mSimpleperfHelper, times(1)).stopCollecting(anyString());

        // Test1 invocation 2 start behaviour
        mListener.testStarted(mTest1Desc);
        verify(mSimpleperfHelper, times(2)).startCollecting();
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mSimpleperfHelper, times(2)).stopCollecting(anyString());

        // Test2 invocation 1 start behaviour
        mListener.testStarted(mTest2Desc);
        verify(mSimpleperfHelper, times(3)).startCollecting();
        mDataRecord = mListener.createDataRecord();
        mListener.onTestEnd(mDataRecord, mTest2Desc);
        verify(mSimpleperfHelper, times(3)).stopCollecting(anyString());

        // Check if the test count is incremented properly.
        assertEquals(2, (int) mInvocationCount.get(mListener.getTestFileName(mTest1Desc)));
        assertEquals(1, (int) mInvocationCount.get(mListener.getTestFileName(mTest2Desc)));
    }
}
