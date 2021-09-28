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

package com.android.compatibility.common.util;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import android.text.TextUtils;
import android.util.Log;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.test.InstrumentationRegistry;

/**
 * Provides Shell-based utilities such as running a command.
 */
public final class ShellUtils {

    private static final String TAG = "ShellHelper";

    /**
     * Runs a Shell command, returning a trimmed response.
     */
    @NonNull
    public static String runShellCommand(@NonNull String template, Object...args) {
        final String command = String.format(template, args);
        Log.d(TAG, "runShellCommand(): " + command);
        try {
            final String result = SystemUtil
                    .runShellCommand(InstrumentationRegistry.getInstrumentation(), command);
            return TextUtils.isEmpty(result) ? "" : result.trim();
        } catch (Exception e) {
            throw new RuntimeException("Command '" + command + "' failed: ", e);
        }
    }

    /**
     * Tap on the view center, it may change window focus.
     */
    public static void tap(View view) {
        final int[] xy = new int[2];
        view.getLocationOnScreen(xy);
        final int viewWidth = view.getWidth();
        final int viewHeight = view.getHeight();
        final int x = (int) (xy[0] + (viewWidth / 2.0f));
        final int y = (int) (xy[1] + (viewHeight / 2.0f));

        runShellCommand("input touchscreen tap %d %d", x, y);
    }


    private ShellUtils() {
        throw new UnsupportedOperationException("contain static methods only");
    }

    /**
     * Simulates input of key event.
     *
     * @param keyCode key event to fire.
     */
    public static void sendKeyEvent(String keyCode) {
        runShellCommand("input keyevent " + keyCode);
    }

    /**
     * Allows an app to draw overlaid windows.
     */
    public static void setOverlayPermissions(@NonNull String packageName, boolean allowed) {
        final String action = allowed ? "allow" : "ignore";
        runShellCommand("appops set %s SYSTEM_ALERT_WINDOW %s", packageName, action);
    }
}
