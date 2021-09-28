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

package com.android.car.apps.common.log;

import android.os.Build;
import android.util.Log;

import androidx.annotation.NonNull;

import java.util.Arrays;
import java.util.List;

/**
 * Util class for logging.
 */
public class L {
    private static final List<String> TYPE_LIST = Arrays.asList("eng", "userdebug");
    /**
     * Logs verbose level logs if loggable.
     *
     * <p>@see String#format(String, Object...) for formatting log string.
     */
    public static void v(String tag, @NonNull String msg, Object... args) {
        if (Log.isLoggable(tag, Log.VERBOSE) || TYPE_LIST.contains(Build.TYPE)) {
            Log.v(tag, String.format(msg, args));
        }
    }

    /**
     * Logs debug level logs if loggable.
     *
     * <p>@see String#format(String, Object...) for formatting log string.
     */
    public static void d(String tag, @NonNull String msg, Object... args) {
        if (Log.isLoggable(tag, Log.DEBUG) || TYPE_LIST.contains(Build.TYPE)) {
            Log.d(tag, String.format(msg, args));
        }
    }

    /**
     * Logs info level logs if loggable.
     *
     * <p>@see String#format(String, Object...) for formatting log string.
     */
    public static void i(String tag, @NonNull String msg, Object... args) {
        if (Log.isLoggable(tag, Log.INFO) || TYPE_LIST.contains(Build.TYPE)) {
            Log.i(tag, String.format(msg, args));
        }
    }

    /**
     * Logs warning level logs if loggable.
     *
     * <p>@see String#format(String, Object...) for formatting log string.
     */
    public static void w(String tag, @NonNull String msg, Object... args) {
        if (Log.isLoggable(tag, Log.WARN) || TYPE_LIST.contains(Build.TYPE)) {
            Log.w(tag, String.format(msg, args));
        }
    }

    /**
     * Logs error level logs if loggable.
     *
     * <p>@see String#format(String, Object...) for formatting log string.
     */
    public static void e(String tag, @NonNull String msg, Object... args) {
        if (Log.isLoggable(tag, Log.ERROR) || TYPE_LIST.contains(Build.TYPE)) {
            Log.e(tag, String.format(msg, args));
        }
    }

    /**
     * Logs warning level logs if loggable.
     *
     * <p>@see String#format(String, Object...) for formatting log string.
     */
    public static void e(String tag, Exception e, @NonNull String msg, Object... args) {
        if (Log.isLoggable(tag, Log.ERROR) || TYPE_LIST.contains(Build.TYPE)) {
            Log.e(tag, String.format(msg, args), e);
        }
    }
}
