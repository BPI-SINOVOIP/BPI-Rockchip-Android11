/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.net.util;

import android.os.SystemClock;


/**
 * @hide
 */
public class Stopwatch {
    private long mStartTimeNs;
    private long mStopTimeNs;

    public boolean isStarted() {
        return (mStartTimeNs > 0);
    }

    public boolean isStopped() {
        return (mStopTimeNs > 0);
    }

    public boolean isRunning() {
        return (isStarted() && !isStopped());
    }

    /**
     * Start the Stopwatch.
     */
    public Stopwatch start() {
        if (!isStarted()) {
            mStartTimeNs = SystemClock.elapsedRealtimeNanos();
        }
        return this;
    }

    /**
     * Restart the Stopwatch.
     */
    public Stopwatch restart() {
        mStartTimeNs = SystemClock.elapsedRealtimeNanos();
        mStopTimeNs = 0;
        return this;
    }

    /**
     * Stop the Stopwatch.
     * @return the total time recorded, in microseconds, or 0 if not started.
     */
    public long stop() {
        if (isRunning()) {
            mStopTimeNs = SystemClock.elapsedRealtimeNanos();
        }
        // Return either the delta after having stopped, or 0.
        return (mStopTimeNs - mStartTimeNs) / 1000;
    }

    /**
     * Return the total time recorded to date, in microseconds.
     * If the Stopwatch is not running, returns the same value as stop(),
     * i.e. either the total time recorded before stopping or 0.
     */
    public long lap() {
        if (isRunning()) {
            return (SystemClock.elapsedRealtimeNanos() - mStartTimeNs) / 1000;
        } else {
            return stop();
        }
    }

    /**
     * Reset the Stopwatch. It will be stopped when this method returns.
     */
    public void reset() {
        mStartTimeNs = 0;
        mStopTimeNs = 0;
    }
}
