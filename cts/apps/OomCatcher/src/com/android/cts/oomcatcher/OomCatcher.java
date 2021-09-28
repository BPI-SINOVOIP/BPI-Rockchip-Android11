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
package com.android.cts.oomcatcher;

import android.app.Activity;
import android.os.Bundle;
import android.app.ActivityManager;
import android.content.Context;
import android.content.ComponentCallbacks2;
import android.util.Log;
import java.util.concurrent.atomic.AtomicBoolean;

/*
 * An App to report to logcat the lowmemory status. As soon as the app detects low memory, it
 * immediately reports. In addition, it also reports every second.
 */
public class OomCatcher extends Activity implements ComponentCallbacks2 {

    private static final String LOG_TAG = "OomCatcher";

    private AtomicBoolean isOom = new AtomicBoolean(false);

    Thread logThread;

    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        logThread = new Thread() {
            @Override
            public void run() {
                while (true) {
                    logStatus();
                    try {
                        Thread.sleep(1000); // 1 second
                    } catch (InterruptedException e) {
                        // thread has been killed
                    }
                }
            }
        };
        logThread.setDaemon(true);
        logThread.start();
    }

    public void onDestroy() {
        super.onDestroy();
        if (logThread != null) {
            logThread.interrupt();
        }
    }

    /*
     * Receive memory callbacks from the Android system. All report low memory except for
     * TRIM_MEMORY_UI_HIDDEN, which reports when the app is in the background. We don't care about
     * that, only when the device is at risk of OOMing.
     *
     * For all indications of low memory, onLowMemory() is called.
     */
    @Override
    public void onTrimMemory(int level) {
        Log.i(LOG_TAG, "Memory trim level: " + level);
        switch (level) {
            // low priority messages being ignored
            case TRIM_MEMORY_BACKGROUND: // bg
            case TRIM_MEMORY_RUNNING_MODERATE: // fg
                // fallthrough
                Log.i(LOG_TAG, "ignoring low priority oom messages.");
                break;
            // medium priority messages being ignored
            case TRIM_MEMORY_MODERATE: // bg
            case TRIM_MEMORY_RUNNING_LOW: // fg
                // fallthrough
                Log.i(LOG_TAG, "ignoring medium priority oom messages.");
                break;
            // high priority messages
            case TRIM_MEMORY_COMPLETE: // bg
            case TRIM_MEMORY_RUNNING_CRITICAL: // fg
                // fallthrough
                onLowMemory();
                break;
            case TRIM_MEMORY_UI_HIDDEN:
                Log.i(LOG_TAG, "UI is hidden because the app is in the background.");
                break;
            default:
                Log.i(LOG_TAG, "unknown memory trim message.");
                return;
        }
    }

    /*
     * An earlier API implementation of low memory callbacks. Sets oom status and logs.
     */
    @Override
    public void onLowMemory() {
        isOom.set(true);
        logStatus();
    }

    /*
     * Log to logcat the current lowmemory status of the app.
     */
    private void logStatus() {
        Log.i(LOG_TAG, isOom.get() ? "Low memory" : "Normal memory");
    }
}
