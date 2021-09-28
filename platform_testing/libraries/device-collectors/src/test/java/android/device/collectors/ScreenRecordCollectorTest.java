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
package android.device.collectors;

import static org.junit.Assert.assertEquals;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.endsWith;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.device.collectors.util.SendToInstrumentation;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.test.uiautomator.UiDevice;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.Description;
import org.junit.runner.notification.Failure;
import org.junit.runner.Result;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.io.File;
import java.io.IOException;
import java.util.List;

/**
 * Android Unit tests for {@link ScreenRecordCollector}.
 *
 * <p>To run: atest CollectorDeviceLibTest:android.device.collectors.ScreenRecordCollectorTest
 */
@RunWith(AndroidJUnit4.class)
public class ScreenRecordCollectorTest {

    private static final int NUM_TEST_CASE = 10;

    private File mLogDir;
    private Description mRunDesc;
    private Description mTestDesc;
    private ScreenRecordCollector mListener;

    @Mock private Instrumentation mInstrumentation;

    @Mock private UiDevice mDevice;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLogDir = new File("tmp/");
        mRunDesc = Description.createSuiteDescription("run");
        mTestDesc = Description.createTestDescription("run", "test");
    }

    @After
    public void tearDown() {
        if (mLogDir != null) {
            mLogDir.delete();
        }
    }

    private ScreenRecordCollector initListener() throws IOException {
        ScreenRecordCollector listener = spy(new ScreenRecordCollector());
        listener.setInstrumentation(mInstrumentation);
        doReturn(mLogDir).when(listener).createAndEmptyDirectory(anyString());
        doReturn(0L).when(listener).getTailBuffer();
        doReturn(mDevice).when(listener).getDevice();
        doReturn("1234").when(mDevice).executeShellCommand(eq("pidof screenrecord"));
        doReturn("").when(mDevice).executeShellCommand(not(eq("pidof screenrecord")));
        return listener;
    }

    /**
     * Test that screen recording is properly started and ended for each test over the course of a
     * test run.
     */
    @Test
    public void testScreenRecord() throws Exception {
        mListener = initListener();

        // Verify output directories are created on test run start.
        mListener.testRunStarted(mRunDesc);
        verify(mListener).createAndEmptyDirectory(ScreenRecordCollector.OUTPUT_DIR);

        // Walk through a number of test cases to simulate behavior.
        for (int i = 1; i <= NUM_TEST_CASE; i++) {
            mListener.testStarted(mTestDesc);
            // Delay verification by 100 ms to ensure the thread was started.
            SystemClock.sleep(100);
            // Expect all recordings to be finished because of mocked commands.
            verify(mDevice, times(i)).executeShellCommand(matches("screenrecord .*video.mp4"));
            for (int r = 2; r < ScreenRecordCollector.MAX_RECORDING_PARTS; r++) {
                verify(mDevice, times(i))
                        .executeShellCommand(
                                matches(String.format("screenrecord .*video%d.mp4", r)));
            }

            // Alternate between pass and fail for variety.
            if (i % 2 == 0) {
                mListener.testFailure(new Failure(mTestDesc, new RuntimeException("I failed")));
            }

            // Verify all processes are killed when the test ends.
            mListener.testFinished(mTestDesc);
            verify(mDevice, times(i)).executeShellCommand(eq("pidof screenrecord"));
            verify(mDevice, times(i)).executeShellCommand(matches("kill -2 1234"));
        }

        // Verify files are reported
        mListener.testRunFinished(new Result());

        Bundle resultBundle = new Bundle();
        mListener.instrumentationRunFinished(System.out, resultBundle, new Result());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mInstrumentation, times(NUM_TEST_CASE))
                .sendStatus(
                        Mockito.eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS),
                        capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        assertEquals(NUM_TEST_CASE, capturedBundle.size());

        int videoCount = 0;
        for (Bundle bundle : capturedBundle) {
            for (String key : bundle.keySet()) {
                if (key.contains("mp4")) videoCount++;
            }
        }
        assertEquals(NUM_TEST_CASE * ScreenRecordCollector.MAX_RECORDING_PARTS, videoCount);
    }

    /** Test that screen recording is properly done for multiple tests and labels iterations. */
    @Test
    public void testScreenRecord_multipleTests() throws Exception {
        mListener = initListener();

        // Run through a sequence of `NUM_TEST_CASE` failing tests.
        mListener.testRunStarted(mRunDesc);

        // Walk through a number of test cases to simulate behavior.
        for (int i = 1; i <= NUM_TEST_CASE; i++) {
            mListener.testStarted(mTestDesc);
            SystemClock.sleep(100);
            mListener.testFinished(mTestDesc);
        }
        mListener.testRunFinished(new Result());

        // Verify that videos are saved with iterations.
        InOrder videoVerifier = inOrder(mDevice);
        // The first video should not have an iteration number.
        videoVerifier
                .verify(mDevice, times(ScreenRecordCollector.MAX_RECORDING_PARTS))
                .executeShellCommand(matches("^.*[^1]-video.*.mp4$"));
        // The subsequent videos should have an iteration number.
        for (int i = 1; i < NUM_TEST_CASE; i++) {
            videoVerifier
                    .verify(mDevice)
                    .executeShellCommand(endsWith(String.format("%d-video.mp4", i + 1)));
            // Verify the iteration-specific and part-specific interactions too.
            for (int p = 2; p <= ScreenRecordCollector.MAX_RECORDING_PARTS; p++) {
                videoVerifier
                        .verify(mDevice)
                        .executeShellCommand(endsWith(String.format("%d-video%d.mp4", i + 1, p)));
            }
        }
    }
}
