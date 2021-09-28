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

import static org.junit.Assert.assertNotNull;

import android.device.collectors.annotations.OptionClass;
import android.os.SystemClock;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import androidx.annotation.VisibleForTesting;

import java.io.IOException;
import java.io.File;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

import org.junit.runner.Description;

/**
 * A {@link BaseMetricListener} that captures video of the screen.
 *
 * <p>This class needs external storage permission. See {@link BaseMetricListener} how to grant
 * external storage permission, especially at install time.
 */
@OptionClass(alias = "screen-record-collector")
public class ScreenRecordCollector extends BaseMetricListener {
    @VisibleForTesting static final int MAX_RECORDING_PARTS = 5;
    private static final long VIDEO_TAIL_BUFFER = 500;

    static final String OUTPUT_DIR = "run_listeners/videos";

    private UiDevice mDevice;
    private static File mDestDir;

    private RecordingThread mCurrentThread;

    // Tracks the test iterations to ensure that each failure gets unique filenames.
    // Key: test description; value: number of iterations.
    private Map<String, Integer> mTestIterations = new HashMap<String, Integer>();

    @Override
    public void onTestRunStart(DataRecord runData, Description description) {
        mDestDir = createAndEmptyDirectory(OUTPUT_DIR);
    }

    @Override
    public void onTestStart(DataRecord testData, Description description) {
        if (mDestDir == null) {
            return;
        }

        // Track the number of iteration for this test.
        amendIterations(description);
        // Start the screen recording operation.
        mCurrentThread = new RecordingThread("test-screen-record", description);
        mCurrentThread.start();
    }

    @Override
    public void onTestEnd(DataRecord testData, Description description) {
        // Skip if not directory.
        if (mDestDir == null) {
            return;
        }

        // Add some extra time to the video end.
        SystemClock.sleep(getTailBuffer());
        // Ctrl + C all screen record processes.
        mCurrentThread.cancel();
        // Wait for the thread to completely die.
        try {
            mCurrentThread.join();
        } catch (InterruptedException ex) {
            Log.e(getTag(), "Interrupted when joining the recording thread.", ex);
        }

        // Add the output files to the data record.
        for (File recording : mCurrentThread.getRecordings()) {
            Log.d(getTag(), String.format("Adding video part: #%s", recording.getName()));
            testData.addFileMetric(
                    String.format("%s_%s", getTag(), recording.getName()), recording);
        }

        // TODO(b/144869954): Delete when tests pass.
    }

    /** Updates the number of iterations performed for a given test {@link Description}. */
    private void amendIterations(Description description) {
        String testName = description.getDisplayName();
        mTestIterations.computeIfPresent(testName, (name, iterations) -> iterations + 1);
        mTestIterations.computeIfAbsent(testName, name -> 1);
    }

    /** Returns the recording's name for part {@code part} of test {@code description}. */
    private File getOutputFile(Description description, int part) {
        final String baseName =
                String.format("%s.%s", description.getClassName(), description.getMethodName());
        // Omit the iteration number for the first iteration.
        int iteration = mTestIterations.get(description.getDisplayName());
        final String fileName =
                String.format(
                        "%s-video%s.mp4",
                        iteration == 1
                                ? baseName
                                : String.join("-", baseName, String.valueOf(iteration)),
                        part == 1 ? "" : part);
        return Paths.get(mDestDir.getAbsolutePath(), fileName).toFile();
    }

    /** Returns a buffer duration for the end of the video. */
    @VisibleForTesting
    public long getTailBuffer() {
        return VIDEO_TAIL_BUFFER;
    }

    /** Returns the currently active {@link UiDevice}. */
    public UiDevice getDevice() {
        if (mDevice == null) {
            mDevice = UiDevice.getInstance(getInstrumentation());
        }
        return mDevice;
    }

    private class RecordingThread extends Thread {
        private final Description mDescription;
        private final List<File> mRecordings;

        private boolean mContinue;

        public RecordingThread(String name, Description description) {
            super(name);

            mContinue = true;
            mRecordings = new ArrayList<>();

            assertNotNull("No test description provided for recording.", description);
            mDescription = description;
        }

        @Override
        public void run() {
            try {
                // Start at i = 1 to encode parts as X.mp4, X2.mp4, X3.mp4, etc.
                for (int i = 1; i <= MAX_RECORDING_PARTS && mContinue; i++) {
                    File output = getOutputFile(mDescription, i);
                    Log.d(
                            getTag(),
                            String.format("Recording screen to %s", output.getAbsolutePath()));
                    mRecordings.add(output);
                    // Make sure not to block on this background command in the main thread so
                    // that the test continues to run, but block in this thread so it does not
                    // trigger a new screen recording session before the prior one completes.
                    getDevice()
                            .executeShellCommand(
                                    String.format("screenrecord %s", output.getAbsolutePath()));
                }
            } catch (IOException e) {
                throw new RuntimeException("Caught exception while screen recording.");
            }
        }

        public void cancel() {
            mContinue = false;

            // Identify the screenrecord PIDs and send SIGINT 2 (Ctrl + C) to each.
            try {
                String[] pids = getDevice().executeShellCommand("pidof screenrecord").split(" ");
                for (String pid : pids) {
                    // Avoid empty process ids, because of weird splitting behavior.
                    if (pid.isEmpty()) {
                        continue;
                    }

                    getDevice().executeShellCommand(String.format("kill -2 %s", pid));
                    Log.d(
                            getTag(),
                            String.format("Sent SIGINT 2 to screenrecord process (%s)", pid));
                }
            } catch (IOException e) {
                throw new RuntimeException("Failed to kill screen recording process.");
            }
        }

        public List<File> getRecordings() {
            return mRecordings;
        }

        private String getTag() {
            return RecordingThread.class.getName();
        }
    }
}
