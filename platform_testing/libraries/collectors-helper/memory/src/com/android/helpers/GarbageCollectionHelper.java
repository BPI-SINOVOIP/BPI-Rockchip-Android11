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
package com.android.helpers;

import android.os.SystemClock;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.annotation.VisibleForTesting;
import androidx.test.InstrumentationRegistry;

import java.io.IOException;
/**
 * GarbageCollectionHelper is a helper for triggerring garbage collection for a list of processes.
 * It should be used before memory metric collectors to reduce noise.
 */
public class GarbageCollectionHelper {
    private static final String TAG = GarbageCollectionHelper.class.getSimpleName();
    // Command to trigger garbage collection for a process. ART uses SIGUSR1 as the signal to GC.
    private static final String GC_CMD = "kill -10 %s";
    // Default time in ms to wait after GC in order for memory to stabilize. The number is
    // somewhat arbitrary but should be a reasonable wait time to reduce noise from the GC
    // finishing up.
    private static final int DEFAULT_POST_GC_WAIT_TIME_MS = 3000;

    private String[] mProcessNames;
    private UiDevice mUiDevice;

    /**
     * Set up the helper before using it.
     *
     * @param procs the list of processes to garbage collect
     */
    public void setUp(String... procs) {
        mProcessNames = procs;
        mUiDevice = initUiDevice();
    }

    @VisibleForTesting
    protected UiDevice initUiDevice() {
        return UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    /**
     * Trigger garbage collection for all processes specified in {@link #setUp} and wait for memory
     * to stabilize.
     */
    public void garbageCollect() {
        garbageCollect(DEFAULT_POST_GC_WAIT_TIME_MS);
    }

    /**
     * Trigger garbage collection for all processes and wait for a caller-specified amount of time
     * after the GC in order for memory to stabilize.
     *
     * @param waitTime time to wait in ms for memory to stabilize
     */
    public void garbageCollect(long waitTime) {
        if (mProcessNames == null || mUiDevice == null) {
            Log.e(TAG,"Process name or UI device is null. Make sure you've called setup.");
            return;
        }

        // Garbage collect each application in sequence.
        for (String procName : mProcessNames) {
            try {
                String pidofOutput = mUiDevice.executeShellCommand(
                        String.format("pidof %s", procName));
                if (!pidofOutput.isEmpty()) {
                    mUiDevice.executeShellCommand(String.format(GC_CMD, pidofOutput));
                }
            } catch (IOException e) {
                Log.e(TAG,"Unable to execute shell command to GC", e);
            }
        }

        // TODO(b/120913945) Look into other ways of determining GC is done
        // We currently use a sleep to wait for memory numbers to stabilize. In particular, the
        // goal is to reduce noise, and currently there is no easy way of determining when a GC is
        // finished other than a wait. The wait can be tuned based off test noisiness (i.e. if a
        // a test's memory metrics are noisy, try a longer sleep time).
        SystemClock.sleep(waitTime);
    }
}
