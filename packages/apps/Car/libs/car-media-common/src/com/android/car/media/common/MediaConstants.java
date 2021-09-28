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

package com.android.car.media.common;

import androidx.media.MediaBrowserServiceCompat;

/**
 * Holds constants used when dealing with MediaBrowserServices that support the
 * content style API for media.
 */
public final class MediaConstants {

    /**
     * Integer extra indicating the recommended size (in pixels) for media art bitmaps. The value
     * is passed in the rootHints Bundle of {@link MediaBrowserServiceCompat#onGetRoot} and can be
     * retrieved with: rootHints.getInt("android.media.extras.MEDIA_ART_SIZE_HINT_PIXELS", 0).
     */
    public static final String EXTRA_MEDIA_ART_SIZE_HINT_PIXELS =
            "android.media.extras.MEDIA_ART_SIZE_HINT_PIXELS";

    /**
     * Bundle extra holding the Pending Intent to launch to let users resolve the current error.
     * See {@link #ERROR_RESOLUTION_ACTION_LABEL} for more details.
     */
    public static final String ERROR_RESOLUTION_ACTION_INTENT =
            "android.media.extras.ERROR_RESOLUTION_ACTION_INTENT";

    /**
     * Bundle extra indicating the messaged displayed to users describing an error state.
     * Used to provide more information for {@link #ERROR_RESOLUTION_ACTION_LABEL}.
     */
    public static final String ERROR_RESOLUTION_ACTION_MESSAGE =
            "android.media.extras.ERROR_RESOLUTION_ACTION_MESSAGE";

    /**
     * Bundle extra indicating the label of the button users can tap to resolve an error state.
     * A more detailed explanation should be provided to the user via
     * {@link PlaybackStateCompat.Builder#setErrorMessage}.
     */
    public static final String ERROR_RESOLUTION_ACTION_LABEL =
            "android.media.extras.ERROR_RESOLUTION_ACTION_LABEL";

    /**
     * Bundle extra indicating the presentation hint for playable media items. See {@link
     * #CONTENT_STYLE_LIST_ITEM_HINT_VALUE} or {@link #CONTENT_STYLE_GRID_ITEM_HINT_VALUE}
     */
    public static final String CONTENT_STYLE_PLAYABLE_HINT =
            "android.media.browse.CONTENT_STYLE_PLAYABLE_HINT";

    /**
     * Bundle extra indicating the presentation hint for playable media items. See {@link
     * #CONTENT_STYLE_LIST_ITEM_HINT_VALUE} or {@link #CONTENT_STYLE_GRID_ITEM_HINT_VALUE}
     *
     * @deprecated this flag has been replaced by {@link #CONTENT_STYLE_PLAYABLE_HINT}
     */
    @Deprecated
    public static final String CONTENT_STYLE_PLAYABLE_HINT_PRERELEASE =
            "android.auto.media.CONTENT_STYLE_PLAYABLE_HINT";

    /**
     * Bundle extra indicating the presentation hint for browsable media items. See {@link
     * #CONTENT_STYLE_LIST_ITEM_HINT_VALUE} or {@link #CONTENT_STYLE_GRID_ITEM_HINT_VALUE}
     */
    public static final String CONTENT_STYLE_BROWSABLE_HINT =
            "android.media.browse.CONTENT_STYLE_BROWSABLE_HINT";

    /**
     * Bundle extra indicating the presentation hint for browsable media items. See {@link
     * #CONTENT_STYLE_LIST_ITEM_HINT_VALUE} or {@link #CONTENT_STYLE_GRID_ITEM_HINT_VALUE}
     *
     * @deprecated this flag has been replaced by {@link #CONTENT_STYLE_BROWSABLE_HINT}
     */
    @Deprecated
    public static final String CONTENT_STYLE_BROWSABLE_HINT_PRERELEASE =
            "android.auto.media.CONTENT_STYLE_BROWSABLE_HINT";

    /**
     * Bundle extra indicating that media app supports MediaBrowserCompat.onSearch
     */
    public static final String MEDIA_SEARCH_SUPPORTED = "android.media.browse.SEARCH_SUPPORTED";

    /**
     * Bundle extra indicating that media app supports MediaBrowserCompat.onSearch for pre-release
     * versions.
     *
     * @deprecated this flag has been replaced by {@link #MEDIA_SEARCH_SUPPORTED}
     */
    @Deprecated
    public static final String MEDIA_SEARCH_SUPPORTED_PRERELEASE =
            "android.auto.media.SEARCH_SUPPORTED";

    /** Bundle extra indicating the media item belongs to a group with the given title. */
    public static final String CONTENT_STYLE_GROUP_TITLE_HINT =
            "android.media.browse.CONTENT_STYLE_GROUP_TITLE_HINT";

    /**
     * Bundle extra indicating the media item belongs to a group with the given title.
     *
     * @deprecated this flag has been replaced by {@link #CONTENT_STYLE_GROUP_TITLE_HINT}
     */
    @Deprecated
    public static final String CONTENT_STYLE_GROUP_TITLE_HINT_PRERELEASE =
            "android.auto.media.CONTENT_STYLE_GROUP_TITLE_HINT";

    /**
     * Value for {@link #CONTENT_STYLE_PLAYABLE_HINT} and {@link #CONTENT_STYLE_BROWSABLE_HINT} that
     * hints the corresponding items should be presented as lists.
     */
    public static final int CONTENT_STYLE_LIST_ITEM_HINT_VALUE = 1;

    /**
     * Value for {@link #CONTENT_STYLE_PLAYABLE_HINT} and {@link #CONTENT_STYLE_BROWSABLE_HINT} that
     * hints the corresponding items should be presented as grids.
     */
    public static final int CONTENT_STYLE_GRID_ITEM_HINT_VALUE = 2;

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

    /**
     * These constants are from
     * @see <a href=https://developer.android.com/training/auto/audio/#required-actions></a>
     *
     * @deprecated this flag has been replaced by {@link #PLAYBACK_SLOT_RESERVATION_SKIP_TO_NEXT}
     */
    @Deprecated
    public static final String SLOT_RESERVATION_SKIP_TO_NEXT =
            "com.google.android.gms.car.media.ALWAYS_RESERVE_SPACE_FOR.ACTION_SKIP_TO_NEXT";
    /**
     * @deprecated this flag has been replaced by {@link #PLAYBACK_SLOT_RESERVATION_SKIP_TO_NEXT}
     */
    @Deprecated
    public static final String SLOT_RESERVATION_SKIP_TO_PREV =
            "com.google.android.gms.car.media.ALWAYS_RESERVE_SPACE_FOR.ACTION_SKIP_TO_PREVIOUS";

    /**
     * These constants are from
     * @see <a href=https://developer.android.com/training/auto/audio/#required-actions></a>
     */
    public static final String PLAYBACK_SLOT_RESERVATION_SKIP_TO_NEXT =
            "android.media.playback.ALWAYS_RESERVE_SPACE_FOR.ACTION_SKIP_TO_NEXT";
    public static final String PLAYBACK_SLOT_RESERVATION_SKIP_TO_PREV =
            "android.media.playback.ALWAYS_RESERVE_SPACE_FOR.ACTION_SKIP_TO_PREVIOUS";

    /**
     * Bundle extra of type 'boolean' indicating that an item should show the 'explicit' symbol.
     */
    public static final String EXTRA_IS_EXPLICIT = "android.media.IS_EXPLICIT";

    /** Bundle extra value indicating that an item should show the corresponding metadata */
    public static long EXTRA_METADATA_ENABLED_VALUE = 1;

    /**
     * Bundle extra of type 'boolean' indicating than an item should show the 'downloaded' symbol.
     */
    public static final String EXTRA_DOWNLOAD_STATUS = "android.media.extra.DOWNLOAD_STATUS";
}
