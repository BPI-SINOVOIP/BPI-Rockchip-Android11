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
package android.device.collectors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.endsWith;
import static org.mockito.ArgumentMatchers.matches;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Instrumentation;
import android.device.collectors.util.SendToInstrumentation;
import android.os.Bundle;
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
 * Android Unit tests for {@link ScreenshotOnFailureCollector}.
 *
 * <p>To run: atest
 * CollectorDeviceLibTest:android.device.collectors.ScreenshotOnFailureCollectorTest
 */
@RunWith(AndroidJUnit4.class)
public class ScreenshotOnFailureCollectorTest {

    private static final int NUM_TEST_CASE = 20;

    private File mLogDir;
    private File mPngLogFile;
    private File mUixLogFile;
    private Description mRunDesc;
    private Description mTestDesc;
    private ScreenshotOnFailureCollector mListener;

    @Mock
    private Instrumentation mInstrumentation;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLogDir = new File("tmp/");
        mPngLogFile = new File("unique_png_log_file.log");
        mUixLogFile = new File("unique_uix_log_file.log");
        mRunDesc = Description.createSuiteDescription("run");
        mTestDesc = Description.createTestDescription("run", "test");
    }

    @After
    public void tearDown() {
        if (mPngLogFile != null) {
            mPngLogFile.delete();
        }
        if (mUixLogFile != null) {
            mUixLogFile.delete();
        }
        if (mLogDir != null) {
            mLogDir.delete();
        }
    }

    private ScreenshotOnFailureCollector initListener(Bundle b) throws IOException {
        ScreenshotOnFailureCollector listener = spy(new ScreenshotOnFailureCollector(b));
        listener.setInstrumentation(mInstrumentation);
        doNothing().when(listener).screenshotToStream(any());
        doReturn(mLogDir).when(listener).createAndEmptyDirectory(anyString());
        doReturn(mPngLogFile).when(listener).takeScreenshot(anyString());
        doReturn(mUixLogFile).when(listener).collectUiXml(any());
        return listener;
    }

    @Test
    public void testSavePngAsFile() throws Exception {
        Bundle b = new Bundle();
        mListener = initListener(b);

        // Test run start behavior
        mListener.testRunStarted(mRunDesc);
        verify(mListener).createAndEmptyDirectory(ScreenshotOnFailureCollector.DEFAULT_DIR);

        for (int i = 1; i <= NUM_TEST_CASE; i++) {
            mListener.testStarted(mTestDesc);

            mListener.testFailure(new Failure(mTestDesc, new RuntimeException("I failed")));

            // Test test end behavior
            mListener.testFinished(mTestDesc);
            verify(mListener, times(i)).takeScreenshot(any());
        }

        // Test run end behavior
        mListener.testRunFinished(new Result());

        Bundle resultBundle = new Bundle();
        mListener.instrumentationRunFinished(System.out, resultBundle, new Result());

        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mInstrumentation, times(NUM_TEST_CASE))
                .sendStatus(Mockito.eq(
                        SendToInstrumentation.INST_STATUS_IN_PROGRESS), capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        assertEquals(NUM_TEST_CASE, capturedBundle.size());

        int pngFileCount = 0;
        for (Bundle bundle : capturedBundle) {
            for (String key : bundle.keySet()) {
                if (key.contains(mPngLogFile.getName())) pngFileCount++;
            }
        }
        assertEquals(NUM_TEST_CASE, pngFileCount);
    }

    @Test
    public void testSaveUixAsFile() throws Exception {
        Bundle b = new Bundle();
        b.putString("include-ui-xml", "true");
        mListener = initListener(b);

        // Run through a sequence of `NUM_TEST_CASE` failing tests.
        mListener.testRunStarted(mRunDesc);
        verify(mListener).createAndEmptyDirectory(ScreenshotOnFailureCollector.DEFAULT_DIR);
        for (int i = 1; i <= NUM_TEST_CASE; i++) {
            mListener.testStarted(mTestDesc);
            mListener.testFailure(new Failure(mTestDesc, new RuntimeException("I failed")));
            mListener.testFinished(mTestDesc);
            // Test `i` UI XML's were saved before this.
            verify(mListener, times(i)).collectUiXml(any());
        }
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

        // Ensure that `NUM_TEST_CASE` files were reported.
        int uixFileCount = 0;
        for (Bundle bundle : capturedBundle) {
            for (String key : bundle.keySet()) {
                if (key.contains(mUixLogFile.getName())) uixFileCount++;
            }
        }
        assertEquals(NUM_TEST_CASE, uixFileCount);
    }

    @Test
    public void testNoUixWithoutOption() throws Exception {
        Bundle b = new Bundle();
        b.putString("include-ui-xml", "false");
        mListener = initListener(b);

        // Run through a sequence of `NUM_TEST_CASE` failing tests.
        mListener.testRunStarted(mRunDesc);
        verify(mListener).createAndEmptyDirectory(ScreenshotOnFailureCollector.DEFAULT_DIR);
        for (int i = 1; i <= NUM_TEST_CASE; i++) {
            mListener.testStarted(mTestDesc);
            mListener.testFailure(new Failure(mTestDesc, new RuntimeException("I failed")));
            mListener.testFinished(mTestDesc);
        }
        mListener.testRunFinished(new Result());
        verify(mListener, Mockito.never()).collectUiXml(any());

        Bundle resultBundle = new Bundle();
        mListener.instrumentationRunFinished(System.out, resultBundle, new Result());
        ArgumentCaptor<Bundle> capture = ArgumentCaptor.forClass(Bundle.class);
        Mockito.verify(mInstrumentation, times(NUM_TEST_CASE))
                .sendStatus(
                        Mockito.eq(SendToInstrumentation.INST_STATUS_IN_PROGRESS),
                        capture.capture());
        List<Bundle> capturedBundle = capture.getAllValues();
        assertEquals(NUM_TEST_CASE, capturedBundle.size());

        // Ensure no matching files were reported.
        int uixFileCount = 0;
        for (Bundle bundle : capturedBundle) {
            for(String key : bundle.keySet()) {
                assertFalse(key.contains(mUixLogFile.getName()));
            }
        }
    }

    @Test
    public void testSavesIterations() throws Exception {
        Bundle b = new Bundle();
        b.putString("include-ui-xml", "true");
        mListener = initListener(b);

        // Run through a sequence of `NUM_TEST_CASE` failing tests.
        mListener.testRunStarted(mRunDesc);
        verify(mListener).createAndEmptyDirectory(ScreenshotOnFailureCollector.DEFAULT_DIR);
        for (int i = 1; i <= NUM_TEST_CASE; i++) {
            mListener.testStarted(mTestDesc);
            mListener.testFailure(new Failure(mTestDesc, new RuntimeException("I failed")));
            mListener.testFinished(mTestDesc);
        }
        mListener.testRunFinished(new Result());

        // Verifies that screenshots are saved with iterations.
        InOrder screenshotSaveVerifier = inOrder(mListener);
        // The first saved screenshot should not have an iteration number.
        screenshotSaveVerifier.verify(mListener).takeScreenshot(matches("^.*[^1].png$"));
        // The second and later saved screenshots should contain the iteration number.
        for (int i = 1; i < NUM_TEST_CASE; i++) {
            screenshotSaveVerifier
                    .verify(mListener)
                    .takeScreenshot(endsWith(String.format("%d-screenshot-on-failure.png", i + 1)));
        }

        // Verifies that XMLs are saved with iterations that start with 1.
        InOrder xmlSaveVerifier = inOrder(mListener);
        // The first saved XML should not have an iteration number.
        xmlSaveVerifier.verify(mListener).takeScreenshot(matches("^.*[^1]$"));
        // The second and later saved XML should contain the iteration number.
        for (int i = 1; i < NUM_TEST_CASE; i++) {
            xmlSaveVerifier.verify(mListener).collectUiXml(endsWith(String.valueOf(i + 1)));
        }
    }
}
