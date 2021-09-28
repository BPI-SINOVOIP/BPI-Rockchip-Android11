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

package com.android.car.media.common.browse;

import static com.android.car.arch.common.LiveDataFunctions.dataOf;

import static java.util.stream.Collectors.toList;

import android.app.Application;
import android.os.Bundle;
import android.support.v4.media.MediaBrowserCompat;
import android.support.v4.media.MediaBrowserCompat.SearchCallback;
import android.support.v4.media.MediaBrowserCompat.SubscriptionCallback;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.android.car.arch.common.FutureData;
import com.android.car.media.common.MediaItemMetadata;
import com.android.car.media.common.source.MediaBrowserConnector.BrowsingState;
import com.android.car.media.common.source.MediaSource;
import com.android.car.media.common.source.MediaSourceViewModel;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;


/**
 * Fulfills media items search and children queries. The latter also provides the last list of
 * results alongside the new list so that differences can be calculated and acted upon.
 */
public class MediaItemsRepository {
    private static final String TAG = "MediaItemsRepository";

    /** One instance per MEDIA_SOURCE_MODE. */
    private static MediaItemsRepository[] sInstances = new MediaItemsRepository[2];

    /** Returns the MediaItemsRepository "singleton" tied to the application for the given mode. */
    public static MediaItemsRepository get(@NonNull Application application, int mode) {
        if (sInstances[mode] == null) {
            sInstances[mode] = new MediaItemsRepository(
                    MediaSourceViewModel.get(application, mode).getBrowsingState());
        }
        return sInstances[mode];
    }

    /** The live data providing the updates for a query. */
    public static class MediaItemsLiveData
            extends LiveData<FutureData<List<MediaItemMetadata>>> {

        private MediaItemsLiveData() {
            this(true);
        }

        private MediaItemsLiveData(boolean initAsLoading) {
            if (initAsLoading) {
                setLoading();
            } else {
                clear();
            }
        }

        private void onDataLoaded(List<MediaItemMetadata> old, List<MediaItemMetadata> list) {
            setValue(FutureData.newLoadedData(old, list));
        }

        private void setLoading() {
            setValue(FutureData.newLoadingData());
        }

        private void clear() {
            setValue(null);
        }
    }

    private static class MediaChildren {
        final String mNodeId;
        final MediaItemsLiveData mLiveData = new MediaItemsLiveData();
        List<MediaItemMetadata> mPreviousValue = Collections.emptyList();

        MediaChildren(String nodeId) {
            mNodeId = nodeId;
        }
    }

    private static class PerMediaSourceCache {
        String mRootId;
        Map<String, MediaChildren> mChildrenByNodeId = new HashMap<>();
    }

    private BrowsingState mBrowsingState;
    private final Map<MediaSource, PerMediaSourceCache> mCaches = new HashMap<>();
    private final MutableLiveData<BrowsingState> mBrowsingStateLiveData = dataOf(null);
    private final MediaItemsLiveData mRootMediaItems = new MediaItemsLiveData();
    private final MediaItemsLiveData mSearchMediaItems = new MediaItemsLiveData(/*loading*/ false);

    private String mSearchQuery;

    @VisibleForTesting
    public MediaItemsRepository(LiveData<BrowsingState> browsingState) {
        browsingState.observeForever(this::onMediaBrowsingStateChanged);
    }

    /**
     * Rebroadcasts browsing state changes before the repository takes any action on them.
     */
    public LiveData<BrowsingState> getBrowsingState() {
        return mBrowsingStateLiveData;
    }

    /**
     * Convenience wrapper for root media items. The live data is the same instance for all
     * media sources.
     */
    public MediaItemsLiveData getRootMediaItems() {
        return mRootMediaItems;
    }

    /**
     * Returns the results from the current search query. The live data is the same instance
     * for all media sources.
     */
    public MediaItemsLiveData getSearchMediaItems() {
        return mSearchMediaItems;
    }

    /** Returns the children of the given node. */
    public MediaItemsLiveData getMediaChildren(String nodeId) {
        PerMediaSourceCache cache = getCache();
        MediaChildren items = cache.mChildrenByNodeId.get(nodeId);
        if (items == null) {
            items = new MediaChildren(nodeId);
            cache.mChildrenByNodeId.put(nodeId, items);
        }

        // Always refresh the subscription (to work around bugs in media apps).
        mBrowsingState.mBrowser.unsubscribe(nodeId);
        mBrowsingState.mBrowser.subscribe(nodeId, mBrowseCallback);

        return items.mLiveData;
    }

    /** Sets the search query. Results will be given through {@link #getSearchMediaItems}. */
    public void setSearchQuery(String query) {
        mSearchQuery = query;
        if (TextUtils.isEmpty(mSearchQuery)) {
            clearSearchResults();
        } else {
            mSearchMediaItems.setLoading();
            mBrowsingState.mBrowser.search(mSearchQuery, null, mSearchCallback);
        }
    }

    private void clearSearchResults() {
        mSearchMediaItems.clear();
    }

    private MediaSource getMediaSource() {
        return (mBrowsingState != null) ? mBrowsingState.mMediaSource : null;
    }

    private void onMediaBrowsingStateChanged(BrowsingState newBrowsingState) {
        mBrowsingState = newBrowsingState;
        if (mBrowsingState == null) {
            Log.e(TAG, "Null browsing state (no media source!)");
            return;
        }
        mBrowsingStateLiveData.setValue(mBrowsingState);
        switch (mBrowsingState.mConnectionStatus) {
            case CONNECTING:
                mRootMediaItems.setLoading();
                break;
            case CONNECTED:
                String rootId = mBrowsingState.mBrowser.getRoot();
                getCache().mRootId = rootId;
                getMediaChildren(rootId);
                break;
            case DISCONNECTING:
                unsubscribeNodes();
                clearSearchResults();
                clearNodes();
                break;
            case REJECTED:
            case SUSPENDED:
                onBrowseData(getCache().mRootId, null);
                clearSearchResults();
                clearNodes();
        }
    }

    private PerMediaSourceCache getCache() {
        PerMediaSourceCache cache = mCaches.get(getMediaSource());
        if (cache == null) {
            cache = new PerMediaSourceCache();
            mCaches.put(getMediaSource(), cache);
        }
        return cache;
    }

    /** Does NOT clear the cache. */
    private void unsubscribeNodes() {
        PerMediaSourceCache cache = getCache();
        for (String nodeId : cache.mChildrenByNodeId.keySet()) {
            mBrowsingState.mBrowser.unsubscribe(nodeId);
        }
    }

    /** Does NOT unsubscribe nodes. */
    private void clearNodes() {
        PerMediaSourceCache cache = getCache();
        cache.mChildrenByNodeId.clear();
    }

    private void onBrowseData(@NonNull String parentId, @Nullable List<MediaItemMetadata> list) {
        PerMediaSourceCache cache = getCache();
        MediaChildren children = cache.mChildrenByNodeId.get(parentId);
        if (children == null) {
            if (Log.isLoggable(TAG, Log.WARN)) {
                Log.w(TAG, "Browse parent not in the cache: " + parentId);
            }
            return;
        }

        List<MediaItemMetadata> old = children.mPreviousValue;
        children.mPreviousValue = list;
        children.mLiveData.onDataLoaded(old, list);

        if (Objects.equals(parentId, cache.mRootId)) {
            mRootMediaItems.onDataLoaded(old, list);
        }
    }

    private void onSearchData(@Nullable List<MediaItemMetadata> list) {
        mSearchMediaItems.onDataLoaded(null, list);
    }

    private final SubscriptionCallback mBrowseCallback = new SubscriptionCallback() {
        @Override
        public void onChildrenLoaded(@NonNull String parentId,
                @NonNull List<MediaBrowserCompat.MediaItem> children) {

            onBrowseData(parentId, children.stream()
                    .filter(Objects::nonNull)
                    .map(MediaItemMetadata::new)
                    .collect(Collectors.toList()));
        }

        @Override
        public void onChildrenLoaded(@NonNull String parentId,
                @NonNull List<MediaBrowserCompat.MediaItem> children,
                @NonNull Bundle options) {
            onChildrenLoaded(parentId, children);
        }

        @Override
        public void onError(@NonNull String parentId) {
            onBrowseData(parentId, null);
        }

        @Override
        public void onError(@NonNull String parentId, @NonNull Bundle options) {
            onError(parentId);
        }
    };

    private final SearchCallback mSearchCallback = new SearchCallback() {
        @Override
        public void onSearchResult(@NonNull String query, Bundle extras,
                @NonNull List<MediaBrowserCompat.MediaItem> items) {
            super.onSearchResult(query, extras, items);
            if (Objects.equals(mSearchQuery, query)) {
                onSearchData(items.stream()
                        .filter(Objects::nonNull)
                        .map(MediaItemMetadata::new)
                        .collect(toList()));
            }
        }

        @Override
        public void onError(@NonNull String query, Bundle extras) {
            super.onError(query, extras);
            if (Objects.equals(mSearchQuery, query)) {
                onSearchData(null);
            }
        }
    };
}
