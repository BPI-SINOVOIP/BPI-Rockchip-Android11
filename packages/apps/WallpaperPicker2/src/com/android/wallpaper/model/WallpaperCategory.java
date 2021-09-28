/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.wallpaper.model;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;

import com.android.wallpaper.asset.Asset;
import com.android.wallpaper.asset.ResourceAsset;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Default category for a collection of WallpaperInfo objects.
 */
public class WallpaperCategory extends Category {

    public static final String TAG_NAME = "category";

    protected final Object mWallpapersLock;
    private final List<WallpaperInfo> mWallpapers;
    private Asset mThumbAsset;
    private int mFeaturedThumbnailIndex;

    public WallpaperCategory(String title, String collectionId, List<WallpaperInfo> wallpapers,
                             int priority) {
        this(title, collectionId, 0, wallpapers, priority);
    }

    public WallpaperCategory(String title, String collectionId, int featuredThumbnailIndex,
                             List<WallpaperInfo> wallpapers, int priority) {
        super(title, collectionId, priority);
        mWallpapers = wallpapers;
        mWallpapersLock = new Object();
        mFeaturedThumbnailIndex = featuredThumbnailIndex;
    }

    public WallpaperCategory(String title, String collectionId, Asset thumbAsset,
            List<WallpaperInfo> wallpapers, int priority) {
        super(title, collectionId, priority);
        mWallpapers = wallpapers;
        mWallpapersLock = new Object();
        mThumbAsset = thumbAsset;
    }

    /**
     * Fetches wallpapers for this category and passes them to the receiver. Subclasses may use a
     * context to fetch wallpaper info.
     */
    public void fetchWallpapers(Context unused, WallpaperReceiver receiver, boolean forceReload) {
        // Perform a shallow clone so as not to pass the reference to the list along to clients.
        receiver.onWallpapersReceived(new ArrayList<>(mWallpapers));
    }

    @Override
    public void show(Activity srcActivity, PickerIntentFactory factory, int requestCode) {
        srcActivity.startActivityForResult(
                factory.newIntent(srcActivity, getCollectionId()), requestCode);
    }

    @Override
    public boolean isEnumerable() {
        return true;
    }

    /**
     * Returns the mutable list of wallpapers backed by this WallpaperCategory. All reads and writes
     * on the returned list must be synchronized with {@code mWallpapersLock}.
     */
    protected List<WallpaperInfo> getMutableWallpapers() {
        return mWallpapers;
    }

    /**
     * Returns an unmodifiable view the list of wallpapers in this WallpaperCategory.
     */
    public List<WallpaperInfo> getUnmodifiableWallpapers() {
        return Collections.unmodifiableList(mWallpapers);
    }

    @Override
    public Asset getThumbnail(Context context) {
        synchronized (mWallpapersLock) {
            if (mThumbAsset == null && mWallpapers.size() > 0) {
                mThumbAsset = mWallpapers.get(mFeaturedThumbnailIndex).getThumbAsset(context);
            }
        }
        return mThumbAsset;
    }

    @Override
    public boolean supportsThirdParty() {
        return false;
    }

    @Override
    public boolean containsThirdParty(String packageName) {
        return false;
    }

    /**
     * Builder used to construct a {@link WallpaperCategory} object from an XML's
     * {@link AttributeSet}.
     */
    public static class Builder {
        private final List<WallpaperInfo> mWallpapers = new ArrayList<>();
        private final Resources mPartnerRes;
        private String mId;
        private String mTitle;
        private int mPriority;
        private String mFeaturedId;
        @IdRes private int mThumbResId;

        public Builder(Resources partnerRes, AttributeSet attrs) {
            mPartnerRes = partnerRes;
            mId = attrs.getAttributeValue(null, "id");
            @StringRes int titleResId = attrs.getAttributeResourceValue(null, "title", 0);
            mTitle = titleResId != 0 ? mPartnerRes.getString(titleResId) : "";
            mFeaturedId = attrs.getAttributeValue(null, "featured");
            mPriority = attrs.getAttributeIntValue(null, "priority", -1);
            mThumbResId = attrs.getAttributeResourceValue(null, "thumbnail", 0);
        }

        /**
         * Add the given {@link WallpaperInfo} to this category
         * @return this for chaining
         */
        public Builder addWallpaper(WallpaperInfo info) {
            mWallpapers.add(info);
            return this;
        }

        /**
         * If no priority was parsed from the XML attributes for this category, set the priority to
         * the given value.
         * @return this for chaining
         */
        public Builder setPriorityIfEmpty(int priority) {
            if (mPriority < 0) {
                mPriority = priority;
            }
            return this;
        }

        /**
         * Build a {@link WallpaperCategory} with this builder's information
         */
        public WallpaperCategory build() {
            if (mThumbResId != 0) {
                return new WallpaperCategory(mTitle, mId,
                        new ResourceAsset(mPartnerRes, mThumbResId), mWallpapers, mPriority);
            } else {
                int featuredIndex = 0;
                for (int i = 0; i < mWallpapers.size(); i++) {
                    if (mWallpapers.get(i).getWallpaperId().equals(mFeaturedId)) {
                        featuredIndex = i;
                        break;
                    }
                }
                return new WallpaperCategory(mTitle, mId, featuredIndex, mWallpapers, mPriority);
            }
        }

        /**
         * Build a {@link PlaceholderCategory} with this builder's information.
         */
        public Category buildPlaceholder() {
            return new PlaceholderCategory(mTitle, mId, mPriority);
        }

        public String getId() {
            return mId;
        }
    }
}
