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

package com.android.pump.util;

import android.content.ContentResolver;
import android.content.UriMatcher;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;

import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;
import androidx.collection.ArraySet;

import com.android.pump.concurrent.Executors;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.AbstractMap.SimpleEntry;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Executor;

@AnyThread
public class ImageLoader {
    private static final String TAG = Clog.tag(ImageLoader.class);

    // TODO Replace with Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q throughout the code.
    private static boolean isAtLeastRunningQ() {
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.P
                || (Build.VERSION.SDK_INT == Build.VERSION_CODES.P
                && Build.VERSION.PREVIEW_SDK_INT > 0);
    }

    private static final UriMatcher VIDEO_THUMBNAIL_URI_MATCHER =
            new UriMatcher(UriMatcher.NO_MATCH);
    static {
        VIDEO_THUMBNAIL_URI_MATCHER.addURI("media", "*/video/media/#/thumbnail", 0);
    }

    private final BitmapCache mBitmapCache = new BitmapCache();
    private final OrientationCache mOrientationCache = new OrientationCache();
    private final ContentResolver mContentResolver;
    private final Executor mExecutor;
    private final Set<Map.Entry<Executor, Callback>> mCallbacks = new ArraySet<>();
    private final Map<Uri, List<Map.Entry<Executor, Callback>>> mLoadCallbacks = new ArrayMap<>();

    @FunctionalInterface
    public interface Callback {
        void onImageLoaded(@NonNull Uri uri, @Nullable Bitmap bitmap);
    }

    public ImageLoader(@NonNull ContentResolver contentResolver, @NonNull Executor executor) {
        mContentResolver = contentResolver;
        mExecutor = executor;
    }

    public void addCallback(@NonNull Callback callback) {
        addCallback(callback, Executors.uiThreadExecutor());
    }

    public void addCallback(@NonNull Callback callback, @NonNull Executor executor) {
        synchronized (this) { // TODO(b/123708613) other lock
            if (!mCallbacks.add(new SimpleEntry<>(executor, callback))) {
                throw new IllegalArgumentException("Callback " + callback + " already added");
            }
        }
    }

    public void removeCallback(@NonNull Callback callback) {
        removeCallback(callback, Executors.uiThreadExecutor());
    }

    public void removeCallback(@NonNull Callback callback, @NonNull Executor executor) {
        synchronized (this) { // TODO(b/123708613) other lock
            if (!mCallbacks.remove(new SimpleEntry<>(executor, callback))) {
                throw new IllegalArgumentException("Callback " + callback + " not found");
            }
        }
    }

    public void loadImage(@NonNull Uri uri, @NonNull Callback callback) {
        loadImage(uri, callback, Executors.uiThreadExecutor());
    }

    public void loadImage(@NonNull Uri uri, @NonNull Callback callback,
            @NonNull Executor executor) {
        Bitmap bitmap;
        Runnable loader = null;
        synchronized (this) { // TODO(b/123708613) other lock
            bitmap = mBitmapCache.get(uri);
            if (bitmap == null) {
                List<Map.Entry<Executor, Callback>> callbacks = mLoadCallbacks.get(uri);
                if (callbacks == null) {
                    callbacks = new LinkedList<>();
                    mLoadCallbacks.put(uri, callbacks);
                    loader = new ImageLoaderTask(uri);
                }
                callbacks.add(new SimpleEntry<>(executor, callback));
            }
        }
        if (bitmap != null) {
            executor.execute(() -> callback.onImageLoaded(uri, bitmap));
        } else if (loader != null) {
            mExecutor.execute(loader);
        }
    }

    public @Orientation int getOrientation(@NonNull Uri uri) {
        return mOrientationCache.get(uri);
    }

    private class ImageLoaderTask implements Runnable {
        private final Uri mUri;

        private ImageLoaderTask(@NonNull Uri uri) {
            mUri = uri;
        }

        @Override
        public void run() {
            try {
                Bitmap bitmap;
                if (isAtLeastRunningQ() || !isVideoThumbnailUri(mUri)) {
                    byte[] data;
                    if (Scheme.isContent(mUri)) {
                        data = readFromContent(mUri);
                    } else if (Scheme.isFile(mUri)) {
                        data = IoUtils.readFromFile(new File(mUri.getPath()));
                    } else if (Scheme.isHttp(mUri) || Scheme.isHttps(mUri)) {
                        data = Http.get(mUri.toString());
                    } else {
                        throw new IllegalArgumentException(
                                "Unknown scheme '" + mUri.getScheme() + "'");
                    }
                    bitmap = decodeBitmapFromByteArray(data);
                } else {
                    // TODO This will always return a bitmap which is inconsistent with Q.
                    bitmap = MediaStore.Video.Thumbnails.getThumbnail(mContentResolver,
                            Long.parseLong(mUri.getPathSegments().get(3)),
                            MediaStore.Video.Thumbnails.MINI_KIND, null);
                }
                Set<Map.Entry<Executor, Callback>> callbacks;
                List<Map.Entry<Executor, Callback>> loadCallbacks;
                synchronized (ImageLoader.this) { // TODO(b/123708613) proper lock
                    if (bitmap != null) {
                        mBitmapCache.put(mUri, bitmap);
                        mOrientationCache.put(mUri, bitmap);
                    }
                    callbacks = new ArraySet<>(mCallbacks);
                    loadCallbacks = mLoadCallbacks.remove(mUri);
                }
                for (Map.Entry<Executor, Callback> callback : callbacks) {
                    callback.getKey().execute(() ->
                            callback.getValue().onImageLoaded(mUri, bitmap));
                }
                for (Map.Entry<Executor, Callback> callback : loadCallbacks) {
                    callback.getKey().execute(() ->
                            callback.getValue().onImageLoaded(mUri, bitmap));
                }
            } catch (IOException | OutOfMemoryError e) {
                Clog.e(TAG, "Failed to load image " + mUri, e);
                // TODO(b/123708676) remove from mLoadCallbacks
            }
        }

        private @Nullable Bitmap decodeBitmapFromByteArray(@NonNull byte[] data) {
            BitmapFactory.Options options = new BitmapFactory.Options();

            options.inJustDecodeBounds = true;
            BitmapFactory.decodeByteArray(data, 0, data.length, options);

            options.inJustDecodeBounds = false;
            options.inSampleSize = 1; // TODO(b/123708796) add scaling
            return BitmapFactory.decodeByteArray(data, 0, data.length, options);
        }

        private @NonNull byte[] readFromContent(@NonNull Uri uri) throws IOException {
            // TODO(b/123708796) set EXTRA_SIZE in opts
            AssetFileDescriptor assetFileDescriptor =
                    mContentResolver.openTypedAssetFileDescriptor(uri, "image/*", null);
            if (assetFileDescriptor == null) {
                throw new FileNotFoundException(uri.toString());
            }
            try {
                return IoUtils.readFromAssetFileDescriptor(assetFileDescriptor);
            } finally {
                IoUtils.close(assetFileDescriptor);
            }
        }

        private boolean isVideoThumbnailUri(@NonNull Uri uri) {
            return VIDEO_THUMBNAIL_URI_MATCHER.match(uri) != UriMatcher.NO_MATCH;
        }
    }
}
