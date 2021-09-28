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
package android.device.collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.device.collectors.PerfettoListener.WakeLockAcquirer;
import android.device.collectors.PerfettoListener.WakeLockContext;
import android.device.collectors.PerfettoListener.WakeLockReleaser;
import android.os.Bundle;
import androidx.test.runner.AndroidJUnit4;
import com.android.helpers.PerfettoHelper;
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
 * Android Unit tests for {@link PerfettoListener}.
 *
 * To run:
 * atest CollectorDeviceLibTest:android.device.collectors.PerfettoListenerTest
 */
@RunWith(AndroidJUnit4.class)
public class PerfettoListenerTest {

    // A {@code Description} to pass when faking a test run start call.
    private static final Description FAKE_DESCRIPTION = Description.createSuiteDescription("run");

    private static final Description FAKE_TEST_DESCRIPTION = Description
            .createTestDescription("class", "method");

    private Description mRunDesc;
    private Description mTest1Desc;
    private Description mTest2Desc;
    private PerfettoListener mListener;
    @Mock private Instrumentation mInstrumentation;
    private Map<String, Integer> mInvocationCount;
    private DataRecord mDataRecord;

    @Spy private PerfettoHelper mPerfettoHelper;

    @Mock private WakeLockContext mWakeLockContext;
    @Mock private WakeLockAcquirer mWakelLockAcquirer;
    @Mock private WakeLockReleaser mWakeLockReleaser;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mRunDesc = Description.createSuiteDescription("run");
        mTest1Desc = Description.createTestDescription("run", "test1");
        mTest2Desc = Description.createTestDescription("run", "test2");
        doAnswer(
                        invocation -> {
                            Runnable runnable = (Runnable) invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mWakeLockContext)
                .run(any());
    }

    private PerfettoListener initListener(Bundle b) {
        mInvocationCount = new HashMap<>();

        PerfettoListener listener =
                spy(
                        new PerfettoListener(
                                b,
                                mPerfettoHelper,
                                mInvocationCount,
                                mWakeLockContext,
                                () -> null,
                                mWakelLockAcquirer,
                                mWakeLockReleaser));

        mDataRecord = listener.createDataRecord();
        listener.setInstrumentation(mInstrumentation);
        return listener;
    }

    /*
     * Verify perfetto start and stop collection methods called exactly once for single test.
     */
    @Test
    public void testPerfettoPerTestSuccessFlow() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);
        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());
        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(1)).stopCollecting(anyLong(), anyString());

    }

    /*
     * Verify stop collecting called exactly once when the test failed and the
     * skip test failure mmetrics is enabled.
     */
    @Test
    public void testPerfettoPerTestFailureFlowDefault() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.SKIP_TEST_FAILURE_METRICS, "false");
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());
        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());

        // Test fail behaviour
        Failure failureDesc = new Failure(FAKE_TEST_DESCRIPTION,
                new Exception());
        mListener.onTestFail(mDataRecord, mTest1Desc, failureDesc);
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(1)).stopCollecting(anyLong(), anyString());

    }

    /*
     * Verify stop perfetto called exactly once when the test failed and the
     * skip test failure metrics is enabled.
     */
    @Test
    public void testPerfettoPerTestFailureFlowWithSkipMmetrics() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.SKIP_TEST_FAILURE_METRICS, "true");
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopPerfetto();
        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());

        // Test fail behaviour
        Failure failureDesc = new Failure(FAKE_TEST_DESCRIPTION,
                new Exception());
        mListener.onTestFail(mDataRecord, mTest1Desc, failureDesc);
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(1)).stopPerfetto();

    }

    /*
     * Verify perfetto start and stop collection methods called exactly once for test run.
     * and not during each test method.
     */
    @Test
    public void testPerfettoPerRunSuccessFlow() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.COLLECT_PER_RUN, "true");
        mListener = initListener(b);
        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(0)).stopCollecting(anyLong(), anyString());
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        verify(mPerfettoHelper, times(1)).stopCollecting(anyLong(), anyString());
    }

    @Test
    public void testRunWithWakeLockHoldsAndReleasesAWakelock() {
        Bundle b = new Bundle();
        mListener = initListener(b);
        mListener.runWithWakeLock(() -> {});
        verify(mWakelLockAcquirer, times(1)).acquire(any());
        verify(mWakeLockReleaser, times(1)).release(any());
    }

    @Test
    public void testRunWithWakeLockHoldsAndReleasesAWakelockWhenThereIsAnException() {
        Bundle b = new Bundle();
        mListener = initListener(b);
        try {
            mListener.runWithWakeLock(
                    () -> {
                        throw new RuntimeException("thrown on purpose");
                    });
        } catch (RuntimeException expected) {
            verify(mWakelLockAcquirer, times(1)).acquire(any());
            verify(mWakeLockReleaser, times(1)).release(any());
            return;
        }

        fail();
    }

    /*
     * Verify no wakelock is held and released when option is disabled (per run case).
     */
    @Test
    public void testPerfettoDoesNotHoldWakeLockPerRun() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.COLLECT_PER_RUN, "true");
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        mListener.testStarted(mTest1Desc);
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        verify(mWakeLockContext, never()).run(any());
    }

    /*
     * Verify no wakelock is held and released when option is disabled (per test case).
     */
    @Test
    public void testPerfettoDoesNotHoldWakeLockPerTest() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        mListener.testStarted(mTest1Desc);
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        verify(mWakeLockContext, never()).run(any());
    }

    /*
     * Verify a wakelock is held and released onTestRunStart when enabled.
     */
    @Test
    public void testHoldWakeLockOnTestRunStart() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.HOLD_WAKELOCK_WHILE_COLLECTING, "true");
        b.putString(PerfettoListener.COLLECT_PER_RUN, "true");
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        // Verify wakelock was held and released
        verify(mWakeLockContext, times(1)).run(any());
    }

    /*
     * Verify a wakelock is held and released on onTestStart.
     */
    @Test
    public void testHoldWakeLockOnTestStart() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.HOLD_WAKELOCK_WHILE_COLLECTING, "true");
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        // There shouldn't be a wakelock held/released onTestRunStart
        verify(mWakeLockContext, times(0)).run(any());
        mListener.testStarted(mTest1Desc);
        // There should be a wakelock held/released onTestStart
        verify(mWakeLockContext, times(1)).run(any());
    }

    /*
     * Verify wakelock is held and released in onTestEnd when option is enabled.
     */
    @Test
    public void testHoldWakeLockOnTestEnd() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.HOLD_WAKELOCK_WHILE_COLLECTING, "true");
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        mListener.testStarted(mTest1Desc);
        // There is one wakelock after the test/run starts
        verify(mWakeLockContext, times(1)).run(any());
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        // There should be a wakelock held and released onTestEnd
        verify(mWakeLockContext, times(2)).run(any());
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        // There shouldn't be more wakelocks are held onTestRunEnd
        verify(mWakeLockContext, times(2)).run(any());
    }

    /*
     * Verify wakelock is held and released in onTestRunEnd when option is enabled.
     */
    @Test
    public void testHoldWakeLockOnTestRunEnd() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.HOLD_WAKELOCK_WHILE_COLLECTING, "true");
        b.putString(PerfettoListener.COLLECT_PER_RUN, "true");
        mListener = initListener(b);

        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        mListener.testStarted(mTest1Desc);
        // There is one wakelock after the test/run start
        verify(mWakeLockContext, times(1)).run(any());

        mListener.onTestEnd(mDataRecord, mTest1Desc);
        // There shouldn't be a wakelock held/released onTestEnd.
        verify(mWakeLockContext, times(1)).run(any());
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        // There should be a new wakelock held/released onTestRunEnd.
        verify(mWakeLockContext, times(2)).run(any());
    }

    /*
     * Verify stop is not called if Perfetto start did not succeed.
     */
    @Test
    public void testPerfettoPerRunFailureFlow() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.COLLECT_PER_RUN, "true");
        mListener = initListener(b);
        doReturn(false).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());

        // Test run start behavior
        mListener.onTestRunStart(mListener.createDataRecord(), FAKE_DESCRIPTION);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());
        mListener.onTestRunEnd(mListener.createDataRecord(), new Result());
        verify(mPerfettoHelper, times(0)).stopCollecting(anyLong(), anyString());
    }

    /*
     * Verify perfetto stop is not invoked if start did not succeed.
     */
    @Test
    public void testPerfettoStartFailureFlow() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);
        doReturn(false).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());

        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(0)).stopCollecting(anyLong(), anyString());
    }

    /*
     * Verify test method invocation count is updated successfully based on the number of times the
     * test method is invoked.
     */
    @Test
    public void testPerfettoInvocationCount() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);
        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());

        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test1 invocation 1 start behavior
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(1)).stopCollecting(anyLong(), anyString());

        // Test1 invocation 2 start behaviour
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(2)).startCollecting(anyString(), anyBoolean());
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(2)).stopCollecting(anyLong(), anyString());

        // Test2 invocation 1 start behaviour
        mListener.testStarted(mTest2Desc);
        verify(mPerfettoHelper, times(3)).startCollecting(anyString(), anyBoolean());
        mDataRecord = mListener.createDataRecord();
        mListener.onTestEnd(mDataRecord, mTest2Desc);
        verify(mPerfettoHelper, times(3)).stopCollecting(anyLong(), anyString());

        // Check if the the test count is incremented properly.
        assertEquals(2, (int) mInvocationCount.get(mListener.getTestFileName(mTest1Desc)));
        assertEquals(1, (int) mInvocationCount.get(mListener.getTestFileName(mTest2Desc)));

    }
    
    /*
     * Verify perfetto start and stop collection methods called when the text
     * proto config option is enabled
     */
    @Test
    public void testPerfettoSuccessFlowWithTextConfig() throws Exception {
        Bundle b = new Bundle();
        b.putString(PerfettoListener.PERFETTO_CONFIG_TEXT_PROTO, "true");
        mListener = initListener(b);
        doReturn(true).when(mPerfettoHelper).startCollecting(anyString(), anyBoolean());
        doReturn(true).when(mPerfettoHelper).stopCollecting(anyLong(), anyString());
        // Test run start behavior
        mListener.testRunStarted(mRunDesc);

        // Test test start behavior
        mListener.testStarted(mTest1Desc);
        verify(mPerfettoHelper, times(1)).startCollecting(anyString(), anyBoolean());
        mListener.onTestEnd(mDataRecord, mTest1Desc);
        verify(mPerfettoHelper, times(1)).stopCollecting(anyLong(), anyString());

    }

}
