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

package com.android.car.media.browse;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.media.R;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.ui.recyclerview.DelegatingContentLimitingAdapter;

import java.util.List;

/**
 * Provides list limiting functionality to {@link BrowseAdapter}.
 */
public class LimitedBrowseAdapter extends DelegatingContentLimitingAdapter<BrowseViewHolder> {

    private final BrowseAdapter mBrowseAdapter;
    private final GridLayoutManager mLayoutManager;
    private final int mMaxSpanSize;

    @Nullable private String mAnchorId;

    public LimitedBrowseAdapter(BrowseAdapter browseAdapter, GridLayoutManager manager,
            BrowseAdapter.Observer browseAdapterObserver) {
        super(browseAdapter, R.id.browse_list_uxr_config);

        mBrowseAdapter = browseAdapter;
        mLayoutManager = manager;
        mMaxSpanSize = manager.getSpanCount();

        mLayoutManager.setSpanSizeLookup(mSpanSizeLookup);
        mBrowseAdapter.registerObserver(browseAdapterObserver);
        mBrowseAdapter.setOnListChangedListener(((previousList, currentList) -> {
            updateUnderlyingDataChanged(currentList.size(), validateAnchor());
        }));
    }

    private final GridLayoutManager.SpanSizeLookup mSpanSizeLookup =
            new GridLayoutManager.SpanSizeLookup() {
                @Override
                public int getSpanSize(int position) {
                    if (getItemViewType(position) == getScrollingLimitedMessageViewType()) {
                        return mMaxSpanSize;
                    }

                    int itemIndex = positionToIndex(position);
                    return mBrowseAdapter.getSpanSize(itemIndex, mMaxSpanSize);
                }
            };

    /**
     * @see BrowseAdapter#submitItems(MediaItemMetadata, List)
     */
    public void submitItems(@Nullable MediaItemMetadata parentItem,
            @Nullable List<MediaItemMetadata> items) {
        mBrowseAdapter.submitItems(parentItem, items);

        if (items == null) {
            updateUnderlyingDataChanged(0, 0);
            return;
        }
        // We can't take any action with the new items as they must first go through the
        // AsyncListDiffer of ListAdapter. This is handled in the OnListChangedListener.
    }

    private int validateAnchor() {
        if (mAnchorId == null) {
            return 0;
        }

        List<BrowseViewData> items = mBrowseAdapter.getCurrentList();
        for (int i = 0; i < items.size(); i++) {
            MediaItemMetadata mediaItem = items.get(i).mMediaItem;
            if (mediaItem != null && mAnchorId.equals(mediaItem.getId())) {
                return i;
            }
        }

        // The anchor isn't present in the new list, reset it.
        mAnchorId = null;
        return 0;
    }


    @Override
    public int computeAnchorIndexWhenRestricting() {
        List<BrowseViewData> items = mBrowseAdapter.getCurrentList();
        if (items.size() <= 0) {
            mAnchorId = null;
            return 0;
        }

        int anchorIndex = (getFirstVisibleItemPosition() + getLastVisibleItemPosition()) / 2;
        if (0 <= anchorIndex && anchorIndex < items.size()) {
            MediaItemMetadata mediaItem = items.get(anchorIndex).mMediaItem;
            mAnchorId = mediaItem != null ? mediaItem.getId() : null;
            return anchorIndex;
        } else {
            mAnchorId = null;
            return 0;
        }
    }

    private int getFirstVisibleItemPosition() {
        int firstItem = mLayoutManager.findFirstCompletelyVisibleItemPosition();
        if (firstItem == RecyclerView.NO_POSITION) {
            firstItem = mLayoutManager.findFirstVisibleItemPosition();
        }
        return firstItem;
    }

    private int getLastVisibleItemPosition() {
        int lastItem = mLayoutManager.findLastCompletelyVisibleItemPosition();
        if (lastItem == RecyclerView.NO_POSITION) {
            lastItem = mLayoutManager.findLastVisibleItemPosition();
        }
        return lastItem;
    }
}
