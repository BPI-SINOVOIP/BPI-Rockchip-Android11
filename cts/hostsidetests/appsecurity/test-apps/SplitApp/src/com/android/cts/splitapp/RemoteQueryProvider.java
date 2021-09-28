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

package com.android.cts.splitapp;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

import java.io.File;

public class RemoteQueryProvider extends ContentProvider {
    private static final String AUTHORITY = "com.android.cts.splitapp";

    private static final int FILES_FILTER = 1;

    private static final String[] CURSOR_COLUMNS = {"_id", "filename", "exists"};

    public static final String FILES_MIME_TYPE =
            "vnd.android.cursor.item/vnd.com.android.cts.splitapp.file_status";

    private static final UriMatcher URI_MATCHER;
    static {
        URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);
        URI_MATCHER.addURI(AUTHORITY, "files/*", FILES_FILTER);
    }

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        int match = URI_MATCHER.match(uri);
        if (match == FILES_FILTER) {
            return fileQuery(uri.getLastPathSegment());
        }
        return null;
    }

    private Cursor fileQuery(String fileName) {
        Context deCtx = getContext().createDeviceProtectedStorageContext();
        File probe = new File(deCtx.getFilesDir(), fileName);

        MatrixCursor cursor = new MatrixCursor(CURSOR_COLUMNS);
        cursor.addRow(new Object[] {0, fileName, probe.exists() ? 1 : 0});
        return cursor;
    }

    @Override
    public String getType(Uri uri) {
        int match = URI_MATCHER.match(uri);
        if (match == FILES_FILTER) {
            return FILES_MIME_TYPE;
        }
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new IllegalArgumentException("This provider does not support insert");
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new IllegalArgumentException("This provider does not support delete");
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new IllegalArgumentException("This provider does not support update");
    }
}
