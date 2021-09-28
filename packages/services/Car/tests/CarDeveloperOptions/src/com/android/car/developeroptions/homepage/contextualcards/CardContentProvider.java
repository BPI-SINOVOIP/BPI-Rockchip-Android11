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

package com.android.car.developeroptions.homepage.contextualcards;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.settingslib.utils.ThreadUtils;

/**
 * Provider stores and manages user interaction feedback for homepage contextual cards.
 */
public class CardContentProvider extends ContentProvider {

    public static final String CARD_AUTHORITY = "com.android.car.developeroptions.homepage.CardContentProvider";

    public static final Uri REFRESH_CARD_URI = new Uri.Builder()
                    .scheme(ContentResolver.SCHEME_CONTENT)
                    .authority(CardContentProvider.CARD_AUTHORITY)
                    .appendPath(CardDatabaseHelper.CARD_TABLE)
                    .build();

    public static final Uri DELETE_CARD_URI = new Uri.Builder()
            .scheme(ContentResolver.SCHEME_CONTENT)
            .authority(CardContentProvider.CARD_AUTHORITY)
            .appendPath(CardDatabaseHelper.CardColumns.CARD_DISMISSED)
            .build();

    private static final String TAG = "CardContentProvider";
    /** URI matcher for ContentProvider queries. */
    private static final UriMatcher URI_MATCHER = new UriMatcher(UriMatcher.NO_MATCH);
    /** URI matcher type for cards table */
    private static final int MATCH_CARDS = 100;

    static {
        URI_MATCHER.addURI(CARD_AUTHORITY, CardDatabaseHelper.CARD_TABLE, MATCH_CARDS);
    }

    private CardDatabaseHelper mDBHelper;

    @Override
    public boolean onCreate() {
        mDBHelper = CardDatabaseHelper.getInstance(getContext());
        return true;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        final ContentValues[] cvs = {values};
        bulkInsert(uri, cvs);
        return uri;
    }

    @Override
    public int bulkInsert(Uri uri, ContentValues[] values) {
        final StrictMode.ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
        int numInserted = 0;
        final SQLiteDatabase database = mDBHelper.getWritableDatabase();

        try {
            maybeEnableStrictMode();

            final String table = getTableFromMatch(uri);
            database.beginTransaction();

            // Here deletion first is avoiding redundant insertion. According to cl/215350754
            database.delete(table, null /* whereClause */, null /* whereArgs */);
            for (ContentValues value : values) {
                long ret = database.insert(table, null /* nullColumnHack */, value);
                if (ret != -1L) {
                    numInserted++;
                } else {
                    Log.e(TAG, "The row " + value.getAsString(CardDatabaseHelper.CardColumns.NAME)
                            + " insertion failed! Please check your data.");
                }
            }
            database.setTransactionSuccessful();
            getContext().getContentResolver().notifyChange(uri, null /* observer */);
        } finally {
            database.endTransaction();
            StrictMode.setThreadPolicy(oldPolicy);
        }
        return numInserted;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("delete operation not supported currently.");
    }

    @Override
    public String getType(Uri uri) {
        throw new UnsupportedOperationException("getType operation not supported currently.");
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {
        final StrictMode.ThreadPolicy oldPolicy = StrictMode.getThreadPolicy();
        try {
            maybeEnableStrictMode();

            final SQLiteQueryBuilder queryBuilder = new SQLiteQueryBuilder();
            final String table = getTableFromMatch(uri);
            queryBuilder.setTables(table);
            final SQLiteDatabase database = mDBHelper.getReadableDatabase();
            final Cursor cursor = queryBuilder.query(database,
                    projection, selection, selectionArgs, null /* groupBy */, null /* having */,
                    sortOrder);

            cursor.setNotificationUri(getContext().getContentResolver(), uri);
            return cursor;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("update operation not supported currently.");
    }

    @VisibleForTesting
    void maybeEnableStrictMode() {
        if (Build.IS_DEBUGGABLE && ThreadUtils.isMainThread()) {
            enableStrictMode();
        }
    }

    @VisibleForTesting
    void enableStrictMode() {
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder().detectAll().build());
    }

    @VisibleForTesting
    String getTableFromMatch(Uri uri) {
        final int match = URI_MATCHER.match(uri);
        String table;
        switch (match) {
            case MATCH_CARDS:
                table = CardDatabaseHelper.CARD_TABLE;
                break;
            default:
                throw new IllegalArgumentException("Unknown Uri format: " + uri);
        }
        return table;
    }
}
