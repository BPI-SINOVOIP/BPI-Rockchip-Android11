/*
 * Copyright 2018 The Android Open Source Project
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

package com.android.car.media.browse;

import androidx.annotation.LayoutRes;

/**
 * Possible view types that would be used by the {@link BrowseAdapter}.
 */
public enum BrowseItemViewType {
    /** A section header */
    HEADER(com.android.car.media.R.layout.media_browse_header_item),
    /** A grid item with a large image. */
    GRID_ITEM(com.android.car.media.R.layout.media_browse_grid_item, 1),
    /** A grid item a medium icon. */
    ICON_GRID_ITEM(com.android.car.media.R.layout.media_browse_grid_icons_item, 1),
    /** A list item with a medium image. */
    LIST_ITEM(com.android.car.media.R.layout.media_browse_list_item),
    /** A list item with a small icon. */
    ICON_LIST_ITEM(com.android.car.media.R.layout.media_browse_list_icons_item),

    /** A spacer view that creates additional padding at the edges of the list, and for headers */
    SPACER(com.android.car.media.R.layout.media_browse_spacer);

    @LayoutRes
    private final int mLayoutId;
    private final int mSpanSize;

    /**
     * {@link BrowseItemViewType} that take the whole width of the {@link
     * androidx.recyclerview.widget.RecyclerView}
     */
    BrowseItemViewType(@LayoutRes int layoutId) {
        mLayoutId = layoutId;
        mSpanSize = -1;
    }

    /**
     * {@link BrowseItemViewType} that only takes the given number of columns.
     */
    BrowseItemViewType(@LayoutRes int layoutId, int spanSize) {
        mLayoutId = layoutId;
        mSpanSize = spanSize;
    }

    /**
     * @param maxSpanSize maximum number of columns of the underlying grid
     * @return number of columns this view wants to use.
     */
    public int getSpanSize(int maxSpanSize) {
        return mSpanSize < 0 ? maxSpanSize : mSpanSize;
    }

    /**
     * Returns the layout that should be inflated to generate this view type.
     */
    @LayoutRes
    public int getLayoutId() {
        return mLayoutId;
    }
}
