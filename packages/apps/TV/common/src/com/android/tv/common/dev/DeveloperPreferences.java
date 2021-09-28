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

package com.android.tv.common.dev;

/** A class about the constants for TV Developer preferences. */
public final class DeveloperPreferences {

    /**
     * Allow Google Analytics for eng builds.
     *
     * <p>Defaults to {@code false}.
     */
    public static final DeveloperPreference<Boolean> ALLOW_ANALYTICS_IN_ENG =
            DeveloperPreference.create("tv_allow_analytics_in_eng", false);

    /**
     * Allow Strict mode for debug builds.
     *
     * <p>Defaults to {@code true}.
     */
    public static final DeveloperPreference<Boolean> ALLOW_STRICT_MODE =
            DeveloperPreference.create("tv_allow_strict_mode", true);

    /**
     * When true {@link android.view.KeyEvent}s are logged.
     *
     * <p>Defaults to {@code false}.
     */
    public static final DeveloperPreference<Boolean> LOG_KEYEVENT =
            DeveloperPreference.create("tv_log_keyevent", false);

    /**
     * When true debug keys are used.
     *
     * <p>Defaults to {@code false}.
     */
    public static final DeveloperPreference<Boolean> USE_DEBUG_KEYS =
            DeveloperPreference.create("tv_use_debug_keys", false);

    /**
     * Send {@link com.android.tv.analytics.Tracker} information.
     *
     * <p>Defaults to {@code true}.
     */
    public static final DeveloperPreference<Boolean> USE_TRACKER =
            DeveloperPreference.create("tv_use_tracker", true);

    /**
     * Maximum buffer size in MegaBytes.
     *
     * <p>Defaults to 2MB.
     */
    public static final DeveloperPreference<Integer> MAX_BUFFER_SIZE_MBYTES =
            DeveloperPreference.create("tv.tuner.buffersize_mbytes", 2 * 1024);

    private DeveloperPreferences() {}
}
