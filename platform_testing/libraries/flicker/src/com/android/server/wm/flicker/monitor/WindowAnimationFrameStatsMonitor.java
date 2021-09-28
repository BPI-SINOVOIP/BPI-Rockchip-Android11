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

package com.android.server.wm.flicker.monitor;

import static android.view.FrameStats.UNDEFINED_TIME_NANO;

import android.app.Instrumentation;
import android.util.Log;
import android.view.FrameStats;

/**
 * Monitors {@link android.view.WindowAnimationFrameStats} to detect janky frames.
 *
 * <p>Adapted from {@link androidx.test.jank.internal.WindowAnimationFrameStatsMonitorImpl} using
 * the same threshold to determine jank.
 */
public class WindowAnimationFrameStatsMonitor implements ITransitionMonitor {

    private static final String TAG = "FLICKER";
    // Maximum normalized error in frame duration before the frame is considered janky
    private static final double MAX_ERROR = 0.5f;
    // Maximum normalized frame duration before the frame is considered a pause
    private static final double PAUSE_THRESHOLD = 15.0f;
    private Instrumentation mInstrumentation;
    private FrameStats mStats;
    private int mNumJankyFrames;
    private long mLongestFrameNano = 0L;

    /** Constructs a WindowAnimationFrameStatsMonitor instance. */
    public WindowAnimationFrameStatsMonitor(Instrumentation instrumentation) {
        mInstrumentation = instrumentation;
    }

    private void analyze() {
        int frameCount = mStats.getFrameCount();
        long refreshPeriodNano = mStats.getRefreshPeriodNano();

        // Skip first frame
        for (int i = 2; i < frameCount; i++) {
            // Handle frames that have not been presented.
            if (mStats.getFramePresentedTimeNano(i) == UNDEFINED_TIME_NANO) {
                // The animation must not have completed. Warn and break out of the loop.
                Log.w(TAG, "Skipping fenced frame.");
                break;
            }
            long frameDurationNano =
                    mStats.getFramePresentedTimeNano(i) - mStats.getFramePresentedTimeNano(i - 1);
            double normalized = (double) frameDurationNano / refreshPeriodNano;
            if (normalized < PAUSE_THRESHOLD) {
                if (normalized > 1.0f + MAX_ERROR) {
                    mNumJankyFrames++;
                }
                mLongestFrameNano = Math.max(mLongestFrameNano, frameDurationNano);
            }
        }
    }

    @Override
    public void start() {
        // Clear out any previous data
        mNumJankyFrames = 0;
        mLongestFrameNano = 0;
        mInstrumentation.getUiAutomation().clearWindowAnimationFrameStats();
    }

    @Override
    public void stop() {
        mStats = mInstrumentation.getUiAutomation().getWindowAnimationFrameStats();
        analyze();
    }

    public boolean jankyFramesDetected() {
        return mStats.getFrameCount() > 0 && mNumJankyFrames > 0;
    }

    @Override
    public String toString() {
        return mStats.toString()
                + " RefreshPeriodNano:"
                + mStats.getRefreshPeriodNano()
                + " NumJankyFrames:"
                + mNumJankyFrames
                + " LongestFrameNano:"
                + mLongestFrameNano;
    }
}
