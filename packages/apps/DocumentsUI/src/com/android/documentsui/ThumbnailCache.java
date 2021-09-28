/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.documentsui;

import static androidx.core.util.Preconditions.checkNotNull;

import android.content.ComponentCallbacks2;
import android.graphics.Bitmap;
import android.graphics.Point;
import android.net.Uri;
import android.util.LruCache;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.core.util.Pools;

import com.android.documentsui.base.Shared;
import com.android.documentsui.base.UserId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Objects;
import java.util.TreeMap;

/**
 * An LRU cache that supports finding the thumbnail of the requested uri with a different size than
 * the requested one.
 */
public class ThumbnailCache {

    private static final SizeComparator SIZE_COMPARATOR = new SizeComparator();

    /**
     * A 2-dimensional index into {@link #mCache} entries. {@link CacheKey} is the key to
     * {@link #mCache}. TreeMap is used to search the closest size to a given size and a given uri.
     */
    private final HashMap<SizeIndexKey, TreeMap<Point, CacheKey>> mSizeIndex;
    private final Cache mCache;

    /**
     * Creates a thumbnail LRU cache.
     *
     * @param maxCacheSizeInBytes the maximum size of thumbnails in bytes this cache can hold.
     */
    public ThumbnailCache(int maxCacheSizeInBytes) {
        mSizeIndex = new HashMap<>();
        mCache = new Cache(maxCacheSizeInBytes);
    }

    /**
     * Obtains thumbnail given a uri and a size.
     *
     * @param uri the uri of the thumbnail in need
     * @param size the desired size of the thumbnail
     * @return the thumbnail result
     */
    public Result getThumbnail(Uri uri, UserId userId, Point size) {
        TreeMap<Point, CacheKey> sizeMap;
        sizeMap = mSizeIndex.get(new SizeIndexKey(uri, userId));
        if (sizeMap == null || sizeMap.isEmpty()) {
            // There is not any thumbnail for this uri.
            return Result.obtainMiss();
        }

        // Look for thumbnail of the same size.
        CacheKey cacheKey = sizeMap.get(size);
        if (cacheKey != null) {
            Entry entry = mCache.get(cacheKey);
            if (entry != null) {
                return Result.obtain(Result.CACHE_HIT_EXACT, size, entry);
            }
        }

        // Look for thumbnail of bigger sizes.
        Point otherSize = sizeMap.higherKey(size);
        if (otherSize != null) {
            cacheKey = sizeMap.get(otherSize);

            if (cacheKey != null) {
                Entry entry = mCache.get(cacheKey);
                if (entry != null) {
                    return Result.obtain(Result.CACHE_HIT_LARGER, otherSize, entry);
                }
            }
        }

        // Look for thumbnail of smaller sizes.
        otherSize = sizeMap.lowerKey(size);
        if (otherSize != null) {
            cacheKey = sizeMap.get(otherSize);

            if (cacheKey != null) {
                Entry entry = mCache.get(cacheKey);
                if (entry != null) {
                    return Result.obtain(Result.CACHE_HIT_SMALLER, otherSize, entry);
                }
            }
        }

        // Cache miss.
        return Result.obtainMiss();
    }

    /**
     * Puts a thumbnail for the given uri and size in to the cache.
     * @param uri the uri of the thumbnail
     * @param size the size of the thumbnail
     * @param thumbnail the thumbnail to put in cache
     * @param lastModified last modified value of the thumbnail to track its validity
     */
    public void putThumbnail(Uri uri, UserId userId, Point size, Bitmap thumbnail,
            long lastModified) {
        CacheKey cacheKey = new CacheKey(uri, userId, size);

        TreeMap<Point, CacheKey> sizeMap;
        synchronized (mSizeIndex) {
            sizeMap = mSizeIndex.get(new SizeIndexKey(uri, userId));
            if (sizeMap == null) {
                sizeMap = new TreeMap<>(SIZE_COMPARATOR);
                mSizeIndex.put(new SizeIndexKey(uri, userId), sizeMap);
            }
        }

        Entry entry = new Entry(thumbnail, lastModified);
        mCache.put(cacheKey, entry);
        synchronized (sizeMap) {
            sizeMap.put(size, cacheKey);
        }
    }

    /**
     * Removes all thumbnail cache associated to the given uri and user.
     * @param uri the uri which thumbnail cache to remove
     */
    public void removeUri(Uri uri, UserId userId) {
        TreeMap<Point, CacheKey> sizeMap;
        synchronized (mSizeIndex) {
            sizeMap = mSizeIndex.get(new SizeIndexKey(uri, userId));
        }

        if (sizeMap != null) {
            // Create an array to hold all values to avoid ConcurrentModificationException because
            // removeKey() will be called by LruCache but we can't modify the map while we're
            // iterating over the collection of values.
            for (CacheKey index : sizeMap.values().toArray(new CacheKey[0])) {
                mCache.remove(index);
            }
        }
    }

    private void removeKey(CacheKey cacheKey) {
        TreeMap<Point, CacheKey> sizeMap;
        synchronized (mSizeIndex) {
            sizeMap = mSizeIndex.get(new SizeIndexKey(cacheKey.uri, cacheKey.userId));
        }

        assert (sizeMap != null);
        synchronized (sizeMap) {
            sizeMap.remove(cacheKey.point);
        }
    }

    public void onTrimMemory(int level) {
        if (level >= ComponentCallbacks2.TRIM_MEMORY_MODERATE) {
            mCache.evictAll();
        } else if (level >= ComponentCallbacks2.TRIM_MEMORY_BACKGROUND) {
            mCache.trimToSize(mCache.size() / 2);
        }
    }

    /**
     * A class that holds thumbnail and cache status.
     */
    public static final class Result {

        @Retention(RetentionPolicy.SOURCE)
        @IntDef({CACHE_MISS, CACHE_HIT_EXACT, CACHE_HIT_SMALLER, CACHE_HIT_LARGER})
        @interface Status {}
        /**
         * Indicates there is no thumbnail for the requested uri. The thumbnail will be null.
         */
        public static final int CACHE_MISS = 0;
        /**
         * Indicates the thumbnail matches the requested size and requested uri.
         */
        public static final int CACHE_HIT_EXACT = 1;
        /**
         * Indicates the thumbnail is in a smaller size than the requested one from the requested
         * uri.
         */
        public static final int CACHE_HIT_SMALLER = 2;
        /**
         * Indicates the thumbnail is in a larger size than the requested one from the requested
         * uri.
         */
        public static final int CACHE_HIT_LARGER = 3;

        private static final Pools.SimplePool<Result> sPool = new Pools.SimplePool<>(1);

        private @Status int mStatus;
        private @Nullable Bitmap mThumbnail;
        private @Nullable Point mSize;
        private long mLastModified;

        private static Result obtainMiss() {
            return obtain(CACHE_MISS, null, null, 0);
        }

        private static Result obtain(@Status int status, Point size, Entry entry) {
            return obtain(status, entry.mThumbnail, size, entry.mLastModified);
        }

        private static Result obtain(@Status int status, @Nullable Bitmap thumbnail,
                @Nullable Point size, long lastModified) {
            Shared.checkMainLoop();

            Result instance = sPool.acquire();
            instance = (instance != null ? instance : new Result());

            instance.mStatus = status;
            instance.mThumbnail = thumbnail;
            instance.mSize = size;
            instance.mLastModified = lastModified;

            return instance;
        }

        private Result() {}

        public void recycle() {
            Shared.checkMainLoop();

            mStatus = -1;
            mThumbnail = null;
            mSize = null;
            mLastModified = -1;

            boolean released = sPool.release(this);
            // This assert is used to guarantee we won't generate too many instances that can't be
            // held in the pool, which indicates our pool size is too small.
            //
            // Right now one instance is enough because we expect all instances are only used in
            // main thread.
            assert (released);
        }

        public @Status int getStatus() {
            return mStatus;
        }

        public @Nullable Bitmap getThumbnail() {
            return mThumbnail;
        }

        public @Nullable Point getSize() {
            return mSize;
        }

        public long getLastModified() {
            return mLastModified;
        }

        public boolean isHit() {
            return (mStatus != CACHE_MISS);
        }

        public boolean isExactHit() {
            return (mStatus == CACHE_HIT_EXACT);
        }
    }

    private static final class Entry {
        private final Bitmap mThumbnail;
        private final long mLastModified;

        private Entry(Bitmap thumbnail, long lastModified) {
            mThumbnail = thumbnail;
            mLastModified = lastModified;
        }
    }

    private final class Cache extends LruCache<CacheKey, Entry> {

        private Cache(int maxSizeBytes) {
            super(maxSizeBytes);
        }

        @Override
        protected int sizeOf(CacheKey key, Entry value) {
            return value.mThumbnail.getByteCount();
        }

        @Override
        protected void entryRemoved(
                boolean evicted, CacheKey key, Entry oldValue, Entry newValue) {
            if (newValue == null) {
                removeKey(key);
            }
        }
    }

    private static final class SizeComparator implements Comparator<Point> {
        @Override
        public int compare(Point size0, Point size1) {
            // Assume all sizes are roughly square, so we only compare them in one dimension.
            return size0.x - size1.x;
        }
    }

    private static class SizeIndexKey {
        final Uri uri;
        final UserId userId;

        SizeIndexKey(Uri uri, UserId userId) {
            this.uri = checkNotNull(uri);
            this.userId = checkNotNull(userId);
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) {
                return false;
            }

            if (this == o) {
                return true;
            }

            if (o instanceof SizeIndexKey) {
                SizeIndexKey other = (SizeIndexKey) o;
                return Objects.equals(uri, other.uri)
                        && Objects.equals(userId, other.userId);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(uri, userId);
        }
    }

    private static class CacheKey {
        final Uri uri;
        final UserId userId;
        final Point point;

        CacheKey(Uri uri, UserId userId, Point point) {
            this.uri = checkNotNull(uri);
            this.userId = checkNotNull(userId);
            this.point = checkNotNull(point);
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) {
                return false;
            }

            if (this == o) {
                return true;
            }

            if (o instanceof CacheKey) {
                CacheKey other = (CacheKey) o;
                return Objects.equals(uri, other.uri)
                        && Objects.equals(userId, other.userId)
                        && Objects.equals(point, other.point);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(uri, userId, point);
        }
    }
}
