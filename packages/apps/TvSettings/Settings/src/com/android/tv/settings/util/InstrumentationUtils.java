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

package com.android.tv.settings.util;

import android.app.tvsettings.TvSettingsEnums;
import android.util.Log;

import com.android.tv.twopanelsettings.slices.TvSettingsStatsLog;

/**
 * Utility class for instrumentation methods.
 */
public final class InstrumentationUtils {

    private static final String TAG = "InstrumentationUtils";

    /**
     * Log the PAGE_FOCUSED event to statsd.
     *
     * @param pageId the id of the focused page
     * @param forward whether the page is focused in the forward navigation (deeper into the
     *                setting tree)
     */
    public static void logPageFocused(int pageId, Boolean forward) {
        // It is necessary to use try-catch as StatsLog.write() could crash in extreme conditions.
        try {
            TvSettingsStatsLog.write(
                    TvSettingsStatsLog.TVSETTINGS_UI_INTERACTED,
                    forward != null
                            ? (forward
                                    ? TvSettingsEnums.PAGE_FOCUSED_FORWARD
                                    : TvSettingsEnums.PAGE_FOCUSED_BACKWARD)
                            : TvSettingsEnums.PAGE_FOCUSED,
                    pageId);
        } catch (Exception e) {
            Log.e(TAG, "Unable to log PAGE_FOCUSED for id: " + pageId + " " + e);
        }
    }

    /**
     * Log the ENTRY_SELECTED event with additional information to statsd.
     *
     * @param entryId the id of the selected entry
     */
    public static void logEntrySelected(int entryId) {
        // It is necessary to use try-catch as StatsLog.write() could crash in extreme conditions.
        try {
            TvSettingsStatsLog.write(
                    TvSettingsStatsLog.TVSETTINGS_UI_INTERACTED,
                    TvSettingsEnums.ENTRY_SELECTED,
                    entryId);
        } catch (Exception e) {
            Log.e(TAG, "Unable to log ENTRY_SELECTED for id: " + entryId + " " + e);
        }
    }

    /**
     * Log the TOGGLE_INTERACTED event to statsd.
     *
     * @param toggleId the id of the interacted toggle
     * @param toggledOn whether the toggle is being flipped on
     */
    public static void logToggleInteracted(int toggleId, Boolean toggledOn) {
        // It is necessary to use try-catch as StatsLog.write() could crash in extreme conditions.
        try {
            TvSettingsStatsLog.write(
                    TvSettingsStatsLog.TVSETTINGS_UI_INTERACTED,
                    toggledOn != null
                            ? (toggledOn
                                    ? TvSettingsEnums.TOGGLED_ON
                                    : TvSettingsEnums.TOGGLED_OFF)
                            : TvSettingsEnums.TOGGLE_INTERACTED,
                    toggleId);
        } catch (Exception e) {
            Log.e(TAG, "Unable to log TOGGLE_INTERACTED for id: " + toggleId + " " + e);
        }
    }

    /** Prevent this class from being accidentally instantiated. */
    private InstrumentationUtils() {
    }
}
