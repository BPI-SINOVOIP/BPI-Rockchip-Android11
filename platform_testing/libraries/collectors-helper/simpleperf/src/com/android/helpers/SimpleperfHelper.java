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

package com.android.helpers;

import android.os.SystemClock;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;
import androidx.test.InstrumentationRegistry;

import java.io.IOException;
import java.io.File;
import java.nio.file.Path;
import java.nio.file.Paths;

/**
 * SimpleperfHelper is used to start and stop simpleperf sample collection and move the output
 * sample file to the destination folder.
 */
public class SimpleperfHelper {

    private static final String LOG_TAG = SimpleperfHelper.class.getSimpleName();
    private static final String SIMPLEPERF_TMP_FILE_PATH = "/data/local/tmp/perf.data";

    private static final String SIMPLEPERF_START_CMD =
            "simpleperf record -o %s -g --post-unwind=yes -f 500 -a --exclude-perf";
    private static final String SIMPLEPERF_STOP_CMD = "pkill -INT simpleperf";
    private static final String SIMPLEPERF_PROC_ID_CMD = "pidof simpleperf";
    private static final String REMOVE_CMD = "rm %s";
    private static final String MOVE_CMD = "mv %s %s";

    private static final int SIMPLEPERF_STOP_WAIT_COUNT = 12;
    private static final long SIMPLEPERF_STOP_WAIT_TIME = 5000;

    private UiDevice mUiDevice;

    public boolean startCollecting() {
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        try {
            // Cleanup any running simpleperf sessions.
            Log.i(LOG_TAG, "Cleanup simpleperf before starting.");
            if (isSimpleperfRunning()) {
                Log.i(LOG_TAG, "Simpleperf is already running. Stopping simpleperf.");
                if (!stopSimpleperf()) {
                    return false;
                }
            }

            Log.i(LOG_TAG, String.format("Starting simpleperf"));
            new Thread() {
                @Override
                public void run() {
                    UiDevice uiDevice =
                            UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
                    try {
                        uiDevice.executeShellCommand(
                                String.format(SIMPLEPERF_START_CMD, SIMPLEPERF_TMP_FILE_PATH));
                    } catch (IOException e) {
                        Log.e(LOG_TAG, "Failed to start simpleperf.");
                    }
                }
            }.start();

            if (!isSimpleperfRunning()) {
                Log.e(LOG_TAG, "Simpleperf sampling failed to start.");
                return false;
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "Unable to start simpleperf sampling due to :" + e.getMessage());
            return false;
        }
        Log.i(LOG_TAG, "Simpleperf sampling started successfully.");
        return true;
    }

    /**
     * Stop the simpleperf sample collection under /data/local/tmp/perf.data and copy the output to
     * the destination file.
     *
     * @param destinationFile file to copy the simpleperf sample file to.
     * @return true if the trace collection is successful otherwise false.
     */
    public boolean stopCollecting(String destinationFile) {
        Log.i(LOG_TAG, "Stopping simpleperf.");
        try {
            if (stopSimpleperf()) {
                if (!copyFileOutput(destinationFile)) {
                    return false;
                }
            } else {
                Log.e(LOG_TAG, "Simpleperf failed to stop");
                return false;
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "Unable to stop the simpleperf samping due to " + e.getMessage());
            return false;
        }
        return true;
    }

    /**
     * Utility method for sending the signal to stop simpleperf.
     *
     * @return true if simpleperf is successfully stopped.
     */
    public boolean stopSimpleperf() throws IOException {
        if (!isSimpleperfRunning()) {
            Log.e(LOG_TAG, "Simpleperf stop called, but simpleperf is not running.");
            return false;
        }

        String stopOutput = mUiDevice.executeShellCommand(SIMPLEPERF_STOP_CMD);
        Log.i(LOG_TAG, String.format("Simpleperf stop command ran"));
        int waitCount = 0;
        while (isSimpleperfRunning()) {
            if (waitCount < SIMPLEPERF_STOP_WAIT_COUNT) {
                SystemClock.sleep(SIMPLEPERF_STOP_WAIT_TIME);
                waitCount++;
                continue;
            }
            return false;
        }
        Log.e(LOG_TAG, "Simpleperf stopped successfully.");
        return true;
    }

    /**
     * Check if there is a simpleperf instance running.
     *
     * @return true if there is a running simpleperf instance, otherwise false.
     */
    private boolean isSimpleperfRunning() {
        try {
            String simpleperfProcId = mUiDevice.executeShellCommand(SIMPLEPERF_PROC_ID_CMD);
            Log.i(LOG_TAG, String.format("Simpleperf process id - %s", simpleperfProcId));
            if (simpleperfProcId.isEmpty()) {
                return false;
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "Unable to check simpleperf status: " + e.getMessage());
            return false;
        }
        return true;
    }

    /**
     * Copy the temporary simpleperf output file to the given destinationFile.
     *
     * @param destinationFile file to copy simpleperf output into.
     * @return true if the simpleperf file copied successfully, otherwise false.
     */
    private boolean copyFileOutput(String destinationFile) {
        Path path = Paths.get(destinationFile);
        String destDirectory = path.getParent().toString();
        // Check if directory already exists
        File directory = new File(destDirectory);
        if (!directory.exists()) {
            boolean success = directory.mkdirs();
            if (!success) {
                Log.e(
                        LOG_TAG,
                        String.format(
                                "Result output directory %s not created successfully.",
                                destDirectory));
                return false;
            }
        }

        // Copy the collected trace from /data/local/tmp to the destinationFile.
        try {
            String moveResult =
                    mUiDevice.executeShellCommand(
                            String.format(MOVE_CMD, SIMPLEPERF_TMP_FILE_PATH, destinationFile));
            if (!moveResult.isEmpty()) {
                Log.e(
                        LOG_TAG,
                        String.format(
                                "Unable to move simpleperf output file from %s to %s due to %s",
                                SIMPLEPERF_TMP_FILE_PATH, destinationFile, moveResult));
                return false;
            }
        } catch (IOException e) {
            Log.e(
                    LOG_TAG,
                    "Unable to move the simpleperf sample file to destination file."
                            + e.getMessage());
            return false;
        }
        return true;
    }
}
