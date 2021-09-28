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

package com.android.settings.intelligence.search;

/**
 * Shared constant variables and identifiers.
 */
public class SearchCommon {
    /**
     * Instance state key for the currently entered query.
     */
    public static final String STATE_QUERY = "state_query";
    /**
     * Instance state key for whether or not saved queries are being shown.
     */
    public static final String STATE_SHOWING_SAVED_QUERY = "state_showing_saved_query";
    /**
     * Instance state key for whether or not a query has been entered yet.
     */
    public static final String STATE_NEVER_ENTERED_QUERY = "state_never_entered_query";

    /**
     * Identifier constants for the search loaders.
     */
    public static final class SearchLoaderId {
        /**
         * Loader identifier to get search results.
         */
        public static final int SEARCH_RESULT = 1;
        /**
         * Loader identifier to save an entered query.
         */
        public static final int SAVE_QUERY_TASK = 2;
        /**
         * Loader identifier to remove an entered query.
         */
        public static final int REMOVE_QUERY_TASK = 3;
        /**
         * Loader identifier to get currently saved queries.
         */
        public static final int SAVED_QUERIES = 4;
    }
}
