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

package com.android.cellbroadcastservice;

import static com.android.cellbroadcastservice.CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__FAILED_TO_INSERT_TO_DB;

import android.content.ContentProvider;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.UriMatcher;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.net.Uri;
import android.provider.Telephony.CellBroadcasts;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;

import java.util.Arrays;

/**
 * The content provider that provides access of cell broadcast message to application.
 * Permission {@link com.android.cellbroadcastservice.FULL_ACCESS_CELL_BROADCAST_HISTORY} is
 * required for querying the cell broadcast message. Only the Cell Broadcast module should have this
 * permission.
 */
public class CellBroadcastProvider extends ContentProvider {
    private static final String TAG = CellBroadcastProvider.class.getSimpleName();

    private static final boolean DBG = Log.isLoggable(TAG, Log.DEBUG);

    /** Database name. */
    private static final String DATABASE_NAME = "cellbroadcasts.db";

    /** Database version. */
    @VisibleForTesting
    public static final int DATABASE_VERSION = 4;

    /** URI matcher for ContentProvider queries. */
    private static final UriMatcher sUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);

    /** URI matcher type to get all cell broadcasts. */
    private static final int ALL = 0;

    /**
     * URI matcher type for get all message history, this is used primarily for default
     * cellbroadcast app or messaging app to display message history. some information is not
     * exposed for messaging history, e.g, messages which are out of broadcast geometrics will not
     * be delivered to end users thus will not be returned as message history query result.
     */
    private static final int MESSAGE_HISTORY = 1;

    /** MIME type for the list of all cell broadcasts. */
    private static final String LIST_TYPE = "vnd.android.cursor.dir/cellbroadcast";

    /** Table name of cell broadcast message. */
    @VisibleForTesting
    public static final String CELL_BROADCASTS_TABLE_NAME = "cell_broadcasts";

    /** Authority string for content URIs. */
    @VisibleForTesting
    public static final String AUTHORITY = "cellbroadcasts";

    /** Content uri of this provider. */
    public static final Uri CONTENT_URI = Uri.parse("content://cellbroadcasts");

    /**
     * Local definition of the query columns for instantiating
     * {@link android.telephony.SmsCbMessage} objects.
     */
    public static final String[] QUERY_COLUMNS = {
            CellBroadcasts._ID,
            CellBroadcasts.SLOT_INDEX,
            CellBroadcasts.SUBSCRIPTION_ID,
            CellBroadcasts.GEOGRAPHICAL_SCOPE,
            CellBroadcasts.PLMN,
            CellBroadcasts.LAC,
            CellBroadcasts.CID,
            CellBroadcasts.SERIAL_NUMBER,
            CellBroadcasts.SERVICE_CATEGORY,
            CellBroadcasts.LANGUAGE_CODE,
            CellBroadcasts.DATA_CODING_SCHEME,
            CellBroadcasts.MESSAGE_BODY,
            CellBroadcasts.MESSAGE_FORMAT,
            CellBroadcasts.MESSAGE_PRIORITY,
            CellBroadcasts.ETWS_WARNING_TYPE,
            // TODO: Remove the hardcode and make this system API in S.
            // CellBroadcasts.ETWS_IS_PRIMARY,
            "etws_is_primary",
            CellBroadcasts.CMAS_MESSAGE_CLASS,
            CellBroadcasts.CMAS_CATEGORY,
            CellBroadcasts.CMAS_RESPONSE_TYPE,
            CellBroadcasts.CMAS_SEVERITY,
            CellBroadcasts.CMAS_URGENCY,
            CellBroadcasts.CMAS_CERTAINTY,
            CellBroadcasts.RECEIVED_TIME,
            CellBroadcasts.LOCATION_CHECK_TIME,
            CellBroadcasts.MESSAGE_BROADCASTED,
            CellBroadcasts.MESSAGE_DISPLAYED,
            CellBroadcasts.GEOMETRIES,
            CellBroadcasts.MAXIMUM_WAIT_TIME
    };

    @VisibleForTesting
    public CellBroadcastPermissionChecker mPermissionChecker;

    /** The database helper for this content provider. */
    @VisibleForTesting
    public SQLiteOpenHelper mDbHelper;

    static {
        sUriMatcher.addURI(AUTHORITY, null, ALL);
        sUriMatcher.addURI(AUTHORITY, "history", MESSAGE_HISTORY);
    }

    public CellBroadcastProvider() {}

    @VisibleForTesting
    public CellBroadcastProvider(CellBroadcastPermissionChecker permissionChecker) {
        mPermissionChecker = permissionChecker;
    }

    @Override
    public boolean onCreate() {
        mDbHelper = new CellBroadcastDatabaseHelper(getContext());
        mPermissionChecker = new CellBroadcastPermissionChecker();
        return true;
    }

    /**
     * Return the MIME type of the data at the specified URI.
     *
     * @param uri the URI to query.
     * @return a MIME type string, or null if there is no type.
     */
    @Override
    public String getType(Uri uri) {
        int match = sUriMatcher.match(uri);
        switch (match) {
            case ALL:
                return LIST_TYPE;
            default:
                return null;
        }
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        checkReadPermission(uri);

        if (DBG) {
            Log.d(TAG, "query:"
                    + " uri = " + uri
                    + " projection = " + Arrays.toString(projection)
                    + " selection = " + selection
                    + " selectionArgs = " + Arrays.toString(selectionArgs)
                    + " sortOrder = " + sortOrder);
        }
        SQLiteQueryBuilder qb = new SQLiteQueryBuilder();
        qb.setStrict(true); // a little protection from injection attacks
        qb.setTables(CELL_BROADCASTS_TABLE_NAME);

        String orderBy;
        if (!TextUtils.isEmpty(sortOrder)) {
            orderBy = sortOrder;
        } else {
            orderBy = CellBroadcasts.RECEIVED_TIME + " DESC";
        }

        int match = sUriMatcher.match(uri);
        switch (match) {
            case ALL:
                return getReadableDatabase().query(
                        CELL_BROADCASTS_TABLE_NAME, projection, selection, selectionArgs,
                        null /* groupBy */, null /* having */, orderBy);
            case MESSAGE_HISTORY:
                // limit projections to certain columns. limit result to broadcasted messages only.
                qb.appendWhere(CellBroadcasts.MESSAGE_BROADCASTED  + "=1");
                return qb.query(getReadableDatabase(), projection, selection, selectionArgs, null,
                        null, orderBy);
            default:
                throw new IllegalArgumentException(
                        "Query method doesn't support this uri = " + uri);
        }
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        checkWritePermission();

        if (DBG) {
            Log.d(TAG, "insert:"
                    + " uri = " + uri
                    + " contentValue = " + values);
        }

        switch (sUriMatcher.match(uri)) {
            case ALL:
                long row = getWritableDatabase().insertOrThrow(CELL_BROADCASTS_TABLE_NAME, null,
                        values);
                if (row > 0) {
                    Uri newUri = ContentUris.withAppendedId(CONTENT_URI, row);
                    getContext().getContentResolver()
                            .notifyChange(CONTENT_URI, null /* observer */);
                    return newUri;
                } else {
                    String errorString = "uri=" + uri.toString() + " values=" + values;
                    // 1000 character limit for error logs
                    if (errorString.length() > 1000) {
                        errorString = errorString.substring(0, 1000);
                    }
                    CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                            CELL_BROADCAST_MESSAGE_ERROR__TYPE__FAILED_TO_INSERT_TO_DB,
                            errorString);
                    Log.e(TAG, "Insert record failed because of unknown reason. " + errorString);
                    return null;
                }
            default:
                String errorString = "Insert method doesn't support this uri="
                        + uri.toString() + " values=" + values;
                // 1000 character limit for error logs
                if (errorString.length() > 1000) {
                    errorString = errorString.substring(0, 1000);
                }
                CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                        CELL_BROADCAST_MESSAGE_ERROR__TYPE__FAILED_TO_INSERT_TO_DB, errorString);
                throw new IllegalArgumentException(errorString);
        }
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        checkWritePermission();

        if (DBG) {
            Log.d(TAG, "delete:"
                    + " uri = " + uri
                    + " selection = " + selection
                    + " selectionArgs = " + Arrays.toString(selectionArgs));
        }

        switch (sUriMatcher.match(uri)) {
            case ALL:
                return getWritableDatabase().delete(CELL_BROADCASTS_TABLE_NAME,
                        selection, selectionArgs);
            default:
                throw new IllegalArgumentException(
                        "Delete method doesn't support this uri = " + uri);
        }
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        checkWritePermission();

        if (DBG) {
            Log.d(TAG, "update:"
                    + " uri = " + uri
                    + " values = {" + values + "}"
                    + " selection = " + selection
                    + " selectionArgs = " + Arrays.toString(selectionArgs));
        }

        switch (sUriMatcher.match(uri)) {
            case ALL:
                int rowCount = getWritableDatabase().update(
                        CELL_BROADCASTS_TABLE_NAME,
                        values,
                        selection,
                        selectionArgs);
                if (rowCount > 0) {
                    getContext().getContentResolver().notifyChange(uri, null /* observer */);
                }
                return rowCount;
            default:
                throw new IllegalArgumentException(
                        "Update method doesn't support this uri = " + uri);
        }
    }

    /**
     * Returns a string used to create the cell broadcast table. This is exposed so the unit test
     * can construct its own in-memory database to match the cell broadcast db.
     */
    @VisibleForTesting
    public static String getStringForCellBroadcastTableCreation(String tableName) {
        return "CREATE TABLE " + tableName + " ("
                + CellBroadcasts._ID + " INTEGER PRIMARY KEY AUTOINCREMENT,"
                + CellBroadcasts.SUBSCRIPTION_ID + " INTEGER,"
                + CellBroadcasts.SLOT_INDEX + " INTEGER DEFAULT 0,"
                + CellBroadcasts.GEOGRAPHICAL_SCOPE + " INTEGER,"
                + CellBroadcasts.PLMN + " TEXT,"
                + CellBroadcasts.LAC + " INTEGER,"
                + CellBroadcasts.CID + " INTEGER,"
                + CellBroadcasts.SERIAL_NUMBER + " INTEGER,"
                + CellBroadcasts.SERVICE_CATEGORY + " INTEGER,"
                + CellBroadcasts.LANGUAGE_CODE + " TEXT,"
                + CellBroadcasts.DATA_CODING_SCHEME + " INTEGER DEFAULT 0,"
                + CellBroadcasts.MESSAGE_BODY + " TEXT,"
                + CellBroadcasts.MESSAGE_FORMAT + " INTEGER,"
                + CellBroadcasts.MESSAGE_PRIORITY + " INTEGER,"
                + CellBroadcasts.ETWS_WARNING_TYPE + " INTEGER,"
                // TODO: Use system API CellBroadcasts.ETWS_IS_PRIMARY in S.
                + "etws_is_primary" + " BOOLEAN DEFAULT 0,"
                + CellBroadcasts.CMAS_MESSAGE_CLASS + " INTEGER,"
                + CellBroadcasts.CMAS_CATEGORY + " INTEGER,"
                + CellBroadcasts.CMAS_RESPONSE_TYPE + " INTEGER,"
                + CellBroadcasts.CMAS_SEVERITY + " INTEGER,"
                + CellBroadcasts.CMAS_URGENCY + " INTEGER,"
                + CellBroadcasts.CMAS_CERTAINTY + " INTEGER,"
                + CellBroadcasts.RECEIVED_TIME + " BIGINT,"
                + CellBroadcasts.LOCATION_CHECK_TIME + " BIGINT DEFAULT -1,"
                + CellBroadcasts.MESSAGE_BROADCASTED + " BOOLEAN DEFAULT 0,"
                + CellBroadcasts.MESSAGE_DISPLAYED + " BOOLEAN DEFAULT 0,"
                + CellBroadcasts.GEOMETRIES + " TEXT,"
                + CellBroadcasts.MAXIMUM_WAIT_TIME + " INTEGER);";
    }

    private SQLiteDatabase getWritableDatabase() {
        return mDbHelper.getWritableDatabase();
    }

    private SQLiteDatabase getReadableDatabase() {
        return mDbHelper.getReadableDatabase();
    }

    private void checkWritePermission() {
        if (!mPermissionChecker.hasFullAccessPermission()) {
            throw new SecurityException(
                    "No permission to write CellBroadcast provider");
        }
    }

    private void checkReadPermission(Uri uri) {
        int match = sUriMatcher.match(uri);
        switch (match) {
            case ALL:
                if (!mPermissionChecker.hasFullAccessPermission()) {
                    throw new SecurityException(
                            "No permission to read CellBroadcast provider");
                }
                break;
            case MESSAGE_HISTORY:
                // The normal read permission android.permission.READ_CELL_BROADCASTS
                // is defined in AndroidManifest.xml and is enfored by the platform.
                // So no additional check is required here.
                break;
            default:
                return;
        }
    }

    @VisibleForTesting
    public static class CellBroadcastDatabaseHelper extends SQLiteOpenHelper {
        public CellBroadcastDatabaseHelper(Context context) {
            super(context, DATABASE_NAME, null /* factory */, DATABASE_VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            db.execSQL(getStringForCellBroadcastTableCreation(CELL_BROADCASTS_TABLE_NAME));
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
            if (DBG) {
                Log.d(TAG, "onUpgrade: oldV=" + oldVersion + " newV=" + newVersion);
            }
            if (oldVersion < 2) {
                db.execSQL("ALTER TABLE " + CELL_BROADCASTS_TABLE_NAME + " ADD COLUMN "
                        + CellBroadcasts.SLOT_INDEX + " INTEGER DEFAULT 0;");
                Log.d(TAG, "add slotIndex column");
            }

            if (oldVersion < 3) {
                db.execSQL("ALTER TABLE " + CELL_BROADCASTS_TABLE_NAME + " ADD COLUMN "
                        + CellBroadcasts.DATA_CODING_SCHEME + " INTEGER DEFAULT 0;");
                db.execSQL("ALTER TABLE " + CELL_BROADCASTS_TABLE_NAME + " ADD COLUMN "
                        + CellBroadcasts.LOCATION_CHECK_TIME + " BIGINT DEFAULT -1;");
                // Specifically for upgrade, the message displayed should be true. For newly arrived
                // message, default should be false.
                db.execSQL("ALTER TABLE " + CELL_BROADCASTS_TABLE_NAME + " ADD COLUMN "
                        + CellBroadcasts.MESSAGE_DISPLAYED + " BOOLEAN DEFAULT 1;");
                Log.d(TAG, "add dcs, location check time, and message displayed column.");
            }

            if (oldVersion < 4) {
                db.execSQL("ALTER TABLE " + CELL_BROADCASTS_TABLE_NAME + " ADD COLUMN "
                        // TODO: Use system API CellBroadcasts.ETWS_IS_PRIMARY in S.
                        + "etws_is_primary" + " BOOLEAN DEFAULT 0;");
                Log.d(TAG, "add ETWS is_primary column.");
            }
        }
    }

    /**
     * Cell broadcast permission checker.
     */
    public class CellBroadcastPermissionChecker {
        /**
         * @return {@code true} if the caller has permission to fully access the cell broadcast
         * provider.
         */
        public boolean hasFullAccessPermission() {
            int status = getContext().checkCallingOrSelfPermission(
                    "com.android.cellbroadcastservice.FULL_ACCESS_CELL_BROADCAST_HISTORY");
            return status == PackageManager.PERMISSION_GRANTED;
        }
    }
}
