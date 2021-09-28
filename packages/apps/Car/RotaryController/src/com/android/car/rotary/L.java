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

package com.android.car.rotary;

import android.os.Build;
import android.util.Log;

import androidx.annotation.NonNull;

/**
 * Utility class for logging.
 */
class L {
    private static final String TAG = "RotaryController";

    /** Logs verbose level logs if loggable or on a debug build. */
    static void v(@NonNull String msg) {
        if (Log.isLoggable(TAG, Log.VERBOSE) || Build.IS_DEBUGGABLE) {
            Log.v(TAG, msg);
        }
    }

    /** Logs debug level logs if loggable or on a debug build. */
    static void d(@NonNull String msg) {
        if (Log.isLoggable(TAG, Log.DEBUG) || Build.IS_DEBUGGABLE) {
            Log.d(TAG, msg);
        }
    }

    /** Logs info level logs if loggable or on a debug build. */
    static void i(@NonNull String msg) {
        if (Log.isLoggable(TAG, Log.INFO) || Build.IS_DEBUGGABLE) {
            Log.i(TAG, msg);
        }
    }

    /** Logs warning level logs if loggable or on a debug build. */
    static void w(@NonNull String msg) {
        if (Log.isLoggable(TAG, Log.WARN) || Build.IS_DEBUGGABLE) {
            Log.w(TAG, msg);
        }
    }

    /** Logs error level logs if loggable or on a debug build. */
    static void e(@NonNull String msg) {
        if (Log.isLoggable(TAG, Log.ERROR) || Build.IS_DEBUGGABLE) {
            Log.e(TAG, msg);
        }
    }

    /** Logs conditional logs if loggable or on a debug build. */
    static void successOrFailure(@NonNull String msg, boolean success) {
        if (success) {
            d(msg + " succeeded");
        } else {
            w(msg + " failed");
        }
    }
}
