/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.cts.providerapp;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.BaseColumns;
import android.util.Log;

import java.util.Arrays;

public class DummyProvider extends ContentProvider {
    private static final String TAG = DummyProvider.class.getName();

    private static final String AUTHORITY = "com.android.cts.providerapp";
    private static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY);

    private static final Uri TEST_URI1 = Uri.withAppendedPath(CONTENT_URI, "test1");
    private static final Uri TEST_URI2 = Uri.withAppendedPath(CONTENT_URI, "test2");

    private static final String CALL_NOTIFY = "notify";

    @Override
    public boolean onCreate() {
        Log.d(TAG, "onCreate");
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        Log.d(TAG, "query uri: " + uri);
        if (!CONTENT_URI.equals(uri)) {
            throw new UnsupportedOperationException();
        }

        final MatrixCursor cursor = new MatrixCursor(new String[] {
                BaseColumns._ID, BaseColumns._COUNT
        }, 2);
        cursor.addRow(new Integer[] {100, 2});
        cursor.addRow(new Integer[] {101, 3});
        cursor.setNotificationUris(getContext().getContentResolver(),
                Arrays.asList(TEST_URI1, TEST_URI2));
        return cursor;
    }

    @Override
    public Bundle call(String method, String arg, Bundle extras) {
        Log.d(TAG, "call method: " + method + ", arg: " + arg);
        if (!CALL_NOTIFY.equals(method)) {
            throw new UnsupportedOperationException();
        }

        switch (arg) {
            case "1":
                getContext().getContentResolver().notifyChange(TEST_URI1, null);
                break;
            case "2":
                getContext().getContentResolver().notifyChange(TEST_URI2, null);
                break;
            default:
                throw new UnsupportedOperationException();
        }
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection,
            String[] selectionArgs) {
        return 0;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }
}