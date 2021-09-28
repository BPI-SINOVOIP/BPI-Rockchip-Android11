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

package com.android.car.media.widgets;

import android.content.Context;
import android.util.Size;
import android.widget.ImageView;

import androidx.annotation.NonNull;

import com.android.car.apps.common.imaging.ImageBinder.PlaceholderType;
import com.android.car.apps.common.imaging.ImageViewBinder;
import com.android.car.media.MediaAppConfig;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.ui.toolbar.TabLayout;

/**
 * An entity representing a media item to be included in the tab bar at the top of the UI.
 */
public class MediaItemTab extends TabLayout.Tab {
    private final MediaItemMetadata mItem;

    @SuppressWarnings("FieldCanBeLocal")
    private ImageViewBinder<MediaItemMetadata.ArtworkRef> mArtBinder;

    /**
     * Creates a new tab for the given media item.
     */
    MediaItemTab(@NonNull MediaItemMetadata item) {
        super(null, item.getTitle());
        mItem = item;
    }

    @Override
    protected void bindIcon(ImageView imageView) {
        Context context = imageView.getContext();
        Size maxArtSize = MediaAppConfig.getMediaItemsBitmapMaxSize(context);
        mArtBinder = new ImageViewBinder<>(PlaceholderType.NONE, maxArtSize, imageView, true);
        mArtBinder.setImage(context, mItem.getArtworkKey());
    }

    /**
     * Returns the item represented by this view
     */
    @NonNull
    public MediaItemMetadata getItem() {
        return mItem;
    }
}
