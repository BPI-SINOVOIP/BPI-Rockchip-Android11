/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.media.testmediaapp.prefs;

/** Various enums saved in the prefs. */
public class TmaEnumPrefs {

    /** Interface for all enums that can be saved in the preferences. */
    public interface EnumPrefValue {
        /** Title for UI display. */
        String getTitle();

        /** Stable id for persisting the value. */
        String getId();
    }

    public enum TmaAccountType implements EnumPrefValue {
        NONE("None", "none"),
        FREE("Free", "free"),
        PAID("Paid", "paid");

        private final PrefValueImpl mPrefValue;

        TmaAccountType(String displayTitle, String id) {
            mPrefValue = new PrefValueImpl(displayTitle, id);
        }

        @Override
        public String getTitle() {
            return mPrefValue.getTitle();
        }

        @Override
        public String getId() {
            return mPrefValue.getId();
        }
    }

    /** For simulating various reply speeds. */
    public enum TmaReplyDelay implements EnumPrefValue {
        NONE("None", "none", 0),
        SHORT("Short", "short", 50),
        SHORT_PLUS("Short+", "short+", 150),
        MEDIUM("Medium", "medium", 500),
        MEDIUM_PLUS("Medium+", "medium+", 2000),
        LONG("Long", "long", 5000),
        EXTRA_LONG("Extra-Long", "extra-long", 10000);

        private final PrefValueImpl mPrefValue;
        public final int mReplyDelayMs;

        TmaReplyDelay(String displayTitle, String id, int delayMs) {
            mPrefValue = new PrefValueImpl(displayTitle + "(" + delayMs + ")", id);
            mReplyDelayMs = delayMs;
        }

        @Override
        public String getTitle() {
            return mPrefValue.getTitle();
        }

        @Override
        public String getId() {
            return mPrefValue.getId();
        }
    }


    public enum TmaBrowseNodeType implements EnumPrefValue {
        NULL("Null (error)", "null"),
        EMPTY("Empty", "empty"),
        QUEUE_ONLY("Queue only", "queue-only"),
        SINGLE_TAB("Single browse-able tab", "single-tab"),
        NODE_CHILDREN("Only browse-able content", "nodes"),
        LEAF_CHILDREN("Only playable content (basic working and error cases)", "leaves"),
        MIXED_CHILDREN("Mixed content (apps are not supposed to do that)", "mixed"),
        UNTAGGED("Untagged media items (not playable or browsable)", "untagged");

        private final PrefValueImpl mPrefValue;

        TmaBrowseNodeType(String displayTitle, String id) {
            mPrefValue = new PrefValueImpl(displayTitle, id);
        }

        @Override
        public String getTitle() {
            return mPrefValue.getTitle();
        }

        @Override
        public String getId() {
            return mPrefValue.getId();
        }
    }

    /* To simulate the events order after login. Media apps should update playback state first, then
     * load the browse tree. But sometims some apps (e.g., GPB) don't follow this order strictly. */
    public enum TmaLoginEventOrder implements EnumPrefValue {
        PLAYBACK_STATE_UPDATE_FIRST("Update playback state first", "state-first"),
        BROWSE_TREE_LOAD_FRIST("Load browse tree first", "tree-first");

        private final PrefValueImpl mPrefValue;

        TmaLoginEventOrder(String displayTitle, String id) {
            mPrefValue = new PrefValueImpl(displayTitle, id);
        }

        @Override
        public String getTitle() {
            return mPrefValue.getTitle();
        }

        @Override
        public String getId() {
            return mPrefValue.getId();
        }
    }

    private static class PrefValueImpl implements EnumPrefValue {

        private final String mDisplayTitle;
        private final String mId;

        /** If the app had to be localized, a resource id should be used for the title. */
        PrefValueImpl(String displayTitle, String id) {
            mDisplayTitle = displayTitle;
            mId = id;
        }

        @Override
        public String getTitle() {
            return mDisplayTitle;
        }

        @Override
        public String getId() {
            return mId;
        }
    }
}
