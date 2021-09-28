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

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.text.TextUtils;
import android.util.Log;

import com.android.car.media.testmediaapp.prefs.TmaPrefs;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class TmaPublicProvider extends ContentProvider {

    private static final String TAG = "TmaAssetProvider";

    private static final int DEFAULT_BUFFER_SIZE = 1024 * 4;

    private static final String AUTHORITY = "com.android.car.media.testmediaapp.public";

    private static final String FILES = "/files/";
    private static final String ASSETS = "/assets/";

    private static final String CONTENT_URI_PREFIX =
            ContentResolver.SCHEME_CONTENT + "://" + AUTHORITY + "/";

    private static final String RESOURCE_URI_PREFIX =
            ContentResolver.SCHEME_ANDROID_RESOURCE + "://" + AUTHORITY + "/";


    public static String buildUriString(String localArt) {
        String prefix = localArt.startsWith("drawable") ? RESOURCE_URI_PREFIX : CONTENT_URI_PREFIX;
        return prefix + localArt;
    }

    private int mAssetDelay = 0;


    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        String path = uri.getPath();

        if (TextUtils.isEmpty(path) || !path.startsWith(FILES)) {
            throw new FileNotFoundException(path);
        }

        Log.i(TAG, "TmaAssetProvider#openFile uri: " + uri + " path: " + path);

        File localFile = new File(getContext().getFilesDir(), path);
        if (!localFile.exists()) {
            downloadFile(localFile, path.substring(FILES.length()));
        }

        return ParcelFileDescriptor.open(localFile,ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public AssetFileDescriptor openAssetFile(Uri uri, String mode) throws FileNotFoundException {
        String path = uri.getPath();
        if (TextUtils.isEmpty(path) || !path.startsWith(ASSETS)) {
            // The ImageDecoder and media center code always try to open as asset first, but
            // super delegates to openFile...
            return super.openAssetFile(uri, mode);
        }

        Log.i(TAG, "TmaAssetProvider#openAssetFile uri: " + uri + " path: " + path);

        try {
            Thread.sleep(mAssetDelay + (int)(mAssetDelay * (Math.random())));
        } catch (InterruptedException ignored) {
        }

        try {
            return getContext().getAssets().openFd(path.substring(ASSETS.length()));
        } catch (IOException e) {
            Log.e(TAG, "openAssetFile failed: " + e);
            return null;
        }
    }

    private void downloadFile(File localFile, String assetsPath) {
        try {
            localFile.getParentFile().mkdirs();

            InputStream input = getContext().getAssets().open(assetsPath);
            OutputStream output = new FileOutputStream(localFile);

            byte[] buffer = new byte[DEFAULT_BUFFER_SIZE];
            int n;
            while (-1 != (n = input.read(buffer))) {
                output.write(buffer, 0, n);
            }

        } catch (IOException e) {
            Log.e(TAG, "downloadFile failed: " + e);
        }
    }

    @Override
    public boolean onCreate() {
        TmaPrefs.getInstance(getContext()).mAssetReplyDelay.registerChangeListener(
                (oldValue, newValue) -> mAssetDelay = newValue.mReplyDelayMs);
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
