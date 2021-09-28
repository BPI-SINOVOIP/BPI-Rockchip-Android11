/*
 * Copyright (c) 2019, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.car.media.testmediaapp;

/**
 * Copy of constants defined in com.android.car.media.common.MediaConstants until they can be moved
 * to a shared location available to all media apps. This makes un-bundling TestMediaApp easier.
 */
public class MediaKeys {

    /** Integer extra indicating the recommended size (in pixels) for media art bitmaps. */
    public static final String EXTRA_MEDIA_ART_SIZE_HINT_PIXELS =
            "android.media.extras.MEDIA_ART_SIZE_HINT_PIXELS";

    /**
     * Bundle extra holding the Pending Intent to launch to let users resolve the current error.
     * See {@link #ERROR_RESOLUTION_ACTION_LABEL} for more details.
     */
    static final String ERROR_RESOLUTION_ACTION_INTENT =
            "android.media.extras.ERROR_RESOLUTION_ACTION_INTENT";


    /**
     * Bundle extra indicating the label of the button users can tap to resolve an error state.
     */
    static final String ERROR_RESOLUTION_ACTION_LABEL =
            "android.media.extras.ERROR_RESOLUTION_ACTION_LABEL";

    /**
     * Bundle extra indicating the presentation hint for playable media items. See {@link
     * #CONTENT_STYLE_LIST_ITEM_HINT_VALUE} or {@link #CONTENT_STYLE_GRID_ITEM_HINT_VALUE}
     */
    static final String CONTENT_STYLE_PLAYABLE_HINT =
            "android.media.browse.CONTENT_STYLE_PLAYABLE_HINT";

    /**
     * Bundle extra indicating the presentation hint for browsable media items. See {@link
     * #CONTENT_STYLE_LIST_ITEM_HINT_VALUE} or {@link #CONTENT_STYLE_GRID_ITEM_HINT_VALUE}
     */
    static final String CONTENT_STYLE_BROWSABLE_HINT =
            "android.media.browse.CONTENT_STYLE_BROWSABLE_HINT";

    /**
     * Value for {@link #CONTENT_STYLE_PLAYABLE_HINT} and {@link #CONTENT_STYLE_BROWSABLE_HINT} that
     * hints the corresponding items should be presented as lists.
     */
    static final int CONTENT_STYLE_LIST_ITEM_HINT_VALUE = 1;

    /**
     * Value for {@link #CONTENT_STYLE_PLAYABLE_HINT} and {@link #CONTENT_STYLE_BROWSABLE_HINT} that
     * hints the corresponding items should be presented as grids.
     */
    static final int CONTENT_STYLE_GRID_ITEM_HINT_VALUE = 2;

    /**
     * Value for {@link #CONTENT_STYLE_BROWSABLE_HINT} that hints the corresponding items should be
     * presented as a "category" list, where media items are browsable and represented by a
     * meaningful icon.
     */
    public static final int CONTENT_STYLE_CATEGORY_LIST_ITEM_HINT_VALUE = 3;

    /**
     * Value for {@link #CONTENT_STYLE_BROWSABLE_HINT} that hints the corresponding items should be
     * presented as a "category" grid, where media items are browsable and represented by a
     * meaningful icon.
     */
    public static final int CONTENT_STYLE_CATEGORY_GRID_ITEM_HINT_VALUE = 4;
}
