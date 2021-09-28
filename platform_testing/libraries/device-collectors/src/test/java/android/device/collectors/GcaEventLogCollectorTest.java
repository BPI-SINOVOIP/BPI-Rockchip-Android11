/*
 * Copyright (C) 2021 The Android Open Source Project
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
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;

import android.app.Instrumentation;
import android.device.collectors.util.SendToInstrumentation;
import android.os.Bundle;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.junit.runner.notification.Failure;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class GcaEventLogCollectorTest {

    private static final Description RUN_DESCRIPTION =
            Description.createSuiteDescription("test-run");
    private static final Description TEST_DESCRIPTION =
            Description.createTestDescription("TestClass", "testCase1");
    private static final Description TEST_FAILURE_DESCRIPTION =
            Description.createTestDescription("TestClass", "testCaseFailed");
    @Mock private Instrumentation mMockInstrumentation;
    private File mEventLogDir;
    private File mDestDir;
    private GcaEventLogCollector mCollector;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mEventLogDir = new File("/sdcard", "camera_events");
        mDestDir = new File("/sdcard", "camera_events_dest");
    }

    @After
    public void tearDown() {
        mCollector.recursiveDelete(mEventLogDir);
        mCollector.recursiveDelete(mDestDir);
    }

    private GcaEventLogCollector initListener(Bundle bundle) {
        mCollector = new GcaEventLogCollector(bundle);
        GcaEventLogCollector listener = Mockito.spy(mCollector);
        listener.setInstrumentation(mMockInstrumentation);
        Mockito.doReturn(mDestDir).when(listener).createAndEmptyDirectory(Mockito.anyString());
        Mockito.doReturn(new byte[10]).when(listener).executeCommandBlocking(Mockito.anyString());
        Mockito.doNothing().when(listener).copyEventLogToSharedStorage(Mockito.anyString());
        return listener;
    }

    @Test
    public void testGcaEventLogCollectionOnTestEnd_includeFailure() throws Exception {
        Bundle bundle = new Bundle();
        bundle.putString(GcaEventLogCollector.COLLECT_TEST_FAILURE_CAMERA_LOGS, "true");
        GcaEventLogCollector listener = initListener(bundle);

        listener.testRunStarted(RUN_DESCRIPTION);
        // Test case 1
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);
        // Test case 2 - Failed
        listener.testStarted(TEST_FAILURE_DESCRIPTION);
        Failure failure =
                new Failure(TEST_FAILURE_DESCRIPTION, new RuntimeException("Test Failed"));
        listener.testFailure(failure);
        listener.testFinished(TEST_FAILURE_DESCRIPTION);
        listener.testRunFinished(new Result());

        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());
        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation, times(2))
                .sendStatus(
                        Mockito.eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS),
                        capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        // Include the log collected from the failed test.
        assertEquals(2, capturedBundle.size());
        Bundle check = capturedBundle.get(0);
        assertEquals(1, check.size());
        for (String key : check.keySet()) {
            assertTrue(
                    key.contains(
                            String.format(
                                    "session_%s_%s",
                                    TEST_DESCRIPTION.getClassName(),
                                    TEST_DESCRIPTION.getMethodName())));
        }
        Bundle checkFail = capturedBundle.get(1);
        assertEquals(1, checkFail.size());
        for (String key : checkFail.keySet()) {
            assertTrue(
                    key.contains(
                            String.format(
                                    "session_%s_%s",
                                    TEST_FAILURE_DESCRIPTION.getClassName(),
                                    TEST_FAILURE_DESCRIPTION.getMethodName())));
        }
    }

    @Test
    public void testGcaEventLogCollectOnTestEnd_skipFailure() throws Exception {
        Bundle bundle = new Bundle();
        bundle.putString(GcaEventLogCollector.COLLECT_TEST_FAILURE_CAMERA_LOGS, "false");
        GcaEventLogCollector listener = initListener(bundle);

        listener.testRunStarted(RUN_DESCRIPTION);
        // 1st iteration
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);
        // 2nd iteration
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);
        // Failed test
        listener.testStarted(TEST_FAILURE_DESCRIPTION);
        Failure failure =
                new Failure(TEST_FAILURE_DESCRIPTION, new RuntimeException("Test Failed"));
        listener.testFailure(failure);
        listener.testFinished(TEST_FAILURE_DESCRIPTION);
        listener.testRunFinished(new Result());

        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());

        assertEquals(0, resultBundle.size());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mMockInstrumentation, times(2))
                .sendStatus(
                        Mockito.eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS),
                        capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        // Include the log collected from the failed test.
        assertEquals(2, capturedBundle.size());
        Bundle check1 = capturedBundle.get(0);
        assertEquals(1, check1.size());
        for (String key : check1.keySet()) {
            assertTrue(
                    key.contains(
                            String.format(
                                    "session_%s_%s",
                                    TEST_DESCRIPTION.getClassName(),
                                    TEST_DESCRIPTION.getMethodName())));
        }
        Bundle check2 = capturedBundle.get(1);
        assertEquals(1, check2.size());
        for (String key : check2.keySet()) {
            assertTrue(
                    key.contains(
                            String.format(
                                    "session_%s_%s_2_",
                                    TEST_DESCRIPTION.getClassName(),
                                    TEST_DESCRIPTION.getMethodName())));
        }
    }

    @Test
    public void testGcaEventLogCollectionOnTestRunEnd_runEndOnly() throws Exception {
        Bundle bundle = new Bundle();
        bundle.putString(GcaEventLogCollector.COLLECT_CAMERA_LOGS_PER_RUN, "true");
        GcaEventLogCollector listener = initListener(bundle);

        listener.testRunStarted(RUN_DESCRIPTION);
        // Test case 1
        listener.testStarted(TEST_DESCRIPTION);
        listener.testFinished(TEST_DESCRIPTION);
        // Test case 2
        listener.testStarted(TEST_FAILURE_DESCRIPTION);
        Failure failure =
                new Failure(TEST_FAILURE_DESCRIPTION, new RuntimeException("Test Failed"));
        listener.testFailure(failure);
        listener.testFinished(TEST_FAILURE_DESCRIPTION);
        listener.testRunFinished(new Result());

        Bundle resultBundle = new Bundle();
        listener.instrumentationRunFinished(System.out, resultBundle, new Result());

        assertEquals(1, resultBundle.size());
        for (String key : resultBundle.keySet()) {
            assertTrue(key.contains("session_run_"));
        }
    }
}
