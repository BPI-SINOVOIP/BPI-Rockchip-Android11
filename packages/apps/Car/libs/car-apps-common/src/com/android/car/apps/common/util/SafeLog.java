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

package com.android.car.apps.common.util;

import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Convenience logging methods that respect whitelisted tags.
 */
public class SafeLog {

    private SafeLog() { }

    /** Log message if tag is whitelisted for {@code Log.VERBOSE}. */
    public static void logv(@NonNull String tag, @NonNull String message) {
        if (Log.isLoggable(tag, Log.VERBOSE)) {
            Log.v(tag, message);
        }
    }

    /** Log message if tag is whitelisted for {@code Log.INFO}. */
    public static void logi(@NonNull String tag, @NonNull String message) {
        if (Log.isLoggable(tag, Log.INFO)) {
            Log.i(tag, message);
        }
    }

    /** Log message if tag is whitelisted for {@code Log.DEBUG}. */
    public static void logd(@NonNull String tag, @NonNull String message) {
        if (Log.isLoggable(tag, Log.DEBUG)) {
            Log.d(tag, message);
        }
    }

    /** Log message if tag is whitelisted for {@code Log.WARN}. */
    public static void logw(@NonNull String tag, @NonNull String message) {
        if (Log.isLoggable(tag, Log.WARN)) {
            Log.w(tag, message);
        }
    }

    /** Log message if tag is whitelisted for {@code Log.ERROR}. */
    public static void loge(@NonNull String tag, @NonNull String message) {
        loge(tag, message, /* exception = */ null);
    }

    /** Log message and optional exception if tag is whitelisted for {@code Log.ERROR}. */
    public static void loge(@NonNull String tag, @NonNull String message,
            @Nullable Exception exception) {
        if (Log.isLoggable(tag, Log.ERROR)) {
            Log.e(tag, message, exception);
        }
    }
}
