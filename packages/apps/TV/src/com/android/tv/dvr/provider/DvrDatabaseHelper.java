/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tv.dvr.provider;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.database.sqlite.SQLiteStatement;
import android.provider.BaseColumns;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.Log;

import com.android.tv.common.dagger.annotations.ApplicationContext;
import com.android.tv.common.flags.DvrFlags;
import com.android.tv.dvr.data.ScheduledRecording;
import com.android.tv.dvr.data.SeriesRecording;
import com.android.tv.dvr.provider.DvrContract.Schedules;
import com.android.tv.dvr.provider.DvrContract.SeriesRecordings;

import com.google.common.collect.ObjectArrays;

import javax.inject.Inject;
import javax.inject.Singleton;

/** A data class for one recorded contents. */
@Singleton
public class DvrDatabaseHelper extends SQLiteOpenHelper {
    private static final String TAG = "DvrDatabaseHelper";
    private static final boolean DEBUG = false;

    private static final int DATABASE_VERSION = 18;
    private static final String DB_NAME = "dvr.db";
    private static final String NOT_NULL = " NOT NULL";
    private static final String PRIMARY_KEY_AUTOINCREMENT = " PRIMARY KEY AUTOINCREMENT";

    private static final int SQL_DATA_TYPE_LONG = 0;
    private static final int SQL_DATA_TYPE_INT = 1;
    private static final int SQL_DATA_TYPE_STRING = 2;

    private static final ColumnInfo[] COLUMNS_SCHEDULES =
            new ColumnInfo[] {
                new ColumnInfo(Schedules._ID, SQL_DATA_TYPE_LONG, PRIMARY_KEY_AUTOINCREMENT),
                new ColumnInfo(
                        Schedules.COLUMN_PRIORITY,
                        SQL_DATA_TYPE_LONG,
                        defaultConstraint(ScheduledRecording.DEFAULT_PRIORITY)),
                new ColumnInfo(Schedules.COLUMN_TYPE, SQL_DATA_TYPE_STRING, NOT_NULL),
                new ColumnInfo(Schedules.COLUMN_INPUT_ID, SQL_DATA_TYPE_STRING, NOT_NULL),
                new ColumnInfo(Schedules.COLUMN_CHANNEL_ID, SQL_DATA_TYPE_LONG, NOT_NULL),
                new ColumnInfo(Schedules.COLUMN_PROGRAM_ID, SQL_DATA_TYPE_LONG),
                new ColumnInfo(Schedules.COLUMN_PROGRAM_TITLE, SQL_DATA_TYPE_STRING),
                new ColumnInfo(
                        Schedules.COLUMN_START_TIME_UTC_MILLIS,
                        SQL_DATA_TYPE_LONG,
                        NOT_NULL),
                new ColumnInfo(
                        Schedules.COLUMN_END_TIME_UTC_MILLIS,
                        SQL_DATA_TYPE_LONG,
                        NOT_NULL),
                new ColumnInfo(Schedules.COLUMN_SEASON_NUMBER, SQL_DATA_TYPE_STRING),
                new ColumnInfo(Schedules.COLUMN_EPISODE_NUMBER, SQL_DATA_TYPE_STRING),
                new ColumnInfo(Schedules.COLUMN_EPISODE_TITLE, SQL_DATA_TYPE_STRING),
                new ColumnInfo(Schedules.COLUMN_PROGRAM_DESCRIPTION, SQL_DATA_TYPE_STRING),
                new ColumnInfo(Schedules.COLUMN_PROGRAM_LONG_DESCRIPTION, SQL_DATA_TYPE_STRING),
                new ColumnInfo(Schedules.COLUMN_PROGRAM_POST_ART_URI, SQL_DATA_TYPE_STRING),
                new ColumnInfo(Schedules.COLUMN_PROGRAM_THUMBNAIL_URI, SQL_DATA_TYPE_STRING),
                new ColumnInfo(Schedules.COLUMN_STATE, SQL_DATA_TYPE_STRING, NOT_NULL),
                new ColumnInfo(
                        Schedules.COLUMN_FAILED_REASON,
                        SQL_DATA_TYPE_STRING),
                new ColumnInfo(
                        Schedules.COLUMN_SERIES_RECORDING_ID,
                        SQL_DATA_TYPE_LONG)
            };

    private static final ColumnInfo[] COLUMNS_TIME_OFFSET =
            new ColumnInfo[] {
                new ColumnInfo(
                        Schedules.COLUMN_START_OFFSET_MILLIS,
                        SQL_DATA_TYPE_LONG,
                        defaultConstraint(ScheduledRecording.DEFAULT_TIME_OFFSET)),
                new ColumnInfo(
                        Schedules.COLUMN_END_OFFSET_MILLIS,
                        SQL_DATA_TYPE_LONG,
                        defaultConstraint(ScheduledRecording.DEFAULT_TIME_OFFSET))
            };

    private static final ColumnInfo[] COLUMNS_SCHEDULES_WITH_TIME_OFFSET =
            ObjectArrays.concat(COLUMNS_SCHEDULES, COLUMNS_TIME_OFFSET, ColumnInfo.class);

    @VisibleForTesting
    static final String SQL_CREATE_SCHEDULES =
            buildCreateSchedulesSql(Schedules.TABLE_NAME, COLUMNS_SCHEDULES);
    private static final String SQL_INSERT_SCHEDULES =
            buildInsertSql(Schedules.TABLE_NAME, COLUMNS_SCHEDULES);
    private static final String SQL_UPDATE_SCHEDULES =
            buildUpdateSql(Schedules.TABLE_NAME, COLUMNS_SCHEDULES);

    private static final String SQL_CREATE_SCHEDULES_WITH_TIME_OFFSET =
            buildCreateSchedulesSql(Schedules.TABLE_NAME, COLUMNS_SCHEDULES_WITH_TIME_OFFSET);
    private static final String SQL_INSERT_SCHEDULES_WITH_TIME_OFFSET =
            buildInsertSql(Schedules.TABLE_NAME, COLUMNS_SCHEDULES_WITH_TIME_OFFSET);
    private static final String SQL_UPDATE_SCHEDULES_WITH_TIME_OFFSET =
            buildUpdateSql(Schedules.TABLE_NAME, COLUMNS_SCHEDULES_WITH_TIME_OFFSET);

    private static final String SQL_DELETE_SCHEDULES = buildDeleteSql(Schedules.TABLE_NAME);
    @VisibleForTesting
    static final String SQL_DROP_SCHEDULES = buildDropSql(Schedules.TABLE_NAME);

    private static final ColumnInfo[] COLUMNS_SERIES_RECORDINGS =
            new ColumnInfo[] {
                new ColumnInfo(SeriesRecordings._ID, SQL_DATA_TYPE_LONG, PRIMARY_KEY_AUTOINCREMENT),
                new ColumnInfo(
                        SeriesRecordings.COLUMN_PRIORITY,
                        SQL_DATA_TYPE_LONG,
                        defaultConstraint(SeriesRecording.DEFAULT_PRIORITY)),
                new ColumnInfo(SeriesRecordings.COLUMN_TITLE, SQL_DATA_TYPE_STRING, NOT_NULL),
                new ColumnInfo(SeriesRecordings.COLUMN_SHORT_DESCRIPTION, SQL_DATA_TYPE_STRING),
                new ColumnInfo(SeriesRecordings.COLUMN_LONG_DESCRIPTION, SQL_DATA_TYPE_STRING),
                new ColumnInfo(SeriesRecordings.COLUMN_INPUT_ID, SQL_DATA_TYPE_STRING, NOT_NULL),
                new ColumnInfo(SeriesRecordings.COLUMN_CHANNEL_ID, SQL_DATA_TYPE_LONG, NOT_NULL),
                new ColumnInfo(SeriesRecordings.COLUMN_SERIES_ID, SQL_DATA_TYPE_STRING, NOT_NULL),
                new ColumnInfo(
                        SeriesRecordings.COLUMN_START_FROM_SEASON,
                        SQL_DATA_TYPE_INT,
                        defaultConstraint(SeriesRecordings.THE_BEGINNING)),
                new ColumnInfo(
                        SeriesRecordings.COLUMN_START_FROM_EPISODE,
                        SQL_DATA_TYPE_INT,
                        defaultConstraint(SeriesRecordings.THE_BEGINNING)),
                new ColumnInfo(
                        SeriesRecordings.COLUMN_CHANNEL_OPTION,
                        SQL_DATA_TYPE_STRING,
                        defaultConstraint(SeriesRecordings.OPTION_CHANNEL_ONE)),
                new ColumnInfo(SeriesRecordings.COLUMN_CANONICAL_GENRE, SQL_DATA_TYPE_STRING),
                new ColumnInfo(SeriesRecordings.COLUMN_POSTER_URI, SQL_DATA_TYPE_STRING),
                new ColumnInfo(SeriesRecordings.COLUMN_PHOTO_URI, SQL_DATA_TYPE_STRING),
                new ColumnInfo(SeriesRecordings.COLUMN_STATE, SQL_DATA_TYPE_STRING)
            };

    @VisibleForTesting
    static final String SQL_CREATE_SERIES_RECORDINGS =
            buildCreateSql(SeriesRecordings.TABLE_NAME, COLUMNS_SERIES_RECORDINGS, null);
    private static final String SQL_INSERT_SERIES_RECORDINGS =
            buildInsertSql(SeriesRecordings.TABLE_NAME, COLUMNS_SERIES_RECORDINGS);
    private static final String SQL_UPDATE_SERIES_RECORDINGS =
            buildUpdateSql(SeriesRecordings.TABLE_NAME, COLUMNS_SERIES_RECORDINGS);
    private static final String SQL_DELETE_SERIES_RECORDINGS =
            buildDeleteSql(SeriesRecordings.TABLE_NAME);
    @VisibleForTesting
    static final String SQL_DROP_SERIES_RECORDINGS =
            buildDropSql(SeriesRecordings.TABLE_NAME);

    private final DvrFlags mDvrFlags;

    private static String defaultConstraint(int value) {
        return defaultConstraint(String.valueOf(value));
    }

    private static String defaultConstraint(long value) {
        return defaultConstraint(String.valueOf(value));
    }

    private static String defaultConstraint(String value) {
        return " DEFAULT " + value;
    }

    private static String foreignKeyConstraint(
            String column,
            String referenceTable,
            String referenceColumn) {
        return ",FOREIGN KEY(" + column + ") "
                + "REFERENCES " + referenceTable + "(" + referenceColumn + ") "
                + "ON UPDATE CASCADE ON DELETE SET NULL";
    }

    private static String buildCreateSchedulesSql(String tableName, ColumnInfo[] columns) {
        return buildCreateSql(
                tableName,
                columns,
                foreignKeyConstraint(
                        Schedules.COLUMN_SERIES_RECORDING_ID,
                        SeriesRecordings.TABLE_NAME,
                        SeriesRecordings._ID));
    }

    private static String buildCreateSql(
            String tableName,
            ColumnInfo[] columns,
            String foreignKeyConstraint) {
        StringBuilder sb = new StringBuilder();
        sb.append("CREATE TABLE ").append(tableName).append("(");
        boolean appendComma = false;
        for (ColumnInfo columnInfo : columns) {
            if (appendComma) {
                sb.append(",");
            }
            appendComma = true;
            sb.append(columnInfo.name);
            switch (columnInfo.type) {
                case SQL_DATA_TYPE_LONG:
                case SQL_DATA_TYPE_INT:
                    sb.append(" INTEGER");
                    break;
                case SQL_DATA_TYPE_STRING:
                    sb.append(" TEXT");
                    break;
            }
            sb.append(columnInfo.constraint);
        }
        if (foreignKeyConstraint != null) {
            sb.append(foreignKeyConstraint);
        }
        sb.append(");");
        return sb.toString();
    }

    private static String buildSelectSql(ColumnInfo[] columns) {
        StringBuilder sb = new StringBuilder();
        sb.append(" SELECT ");
        boolean appendComma = false;
        for (ColumnInfo columnInfo : columns) {
            if (appendComma) {
                sb.append(",");
            }
            appendComma = true;
            sb.append(columnInfo.name);
        }
        return sb.toString();
    }

    private static String buildInsertSql(String tableName, ColumnInfo[] columns) {
        StringBuilder sb = new StringBuilder();
        sb.append("INSERT INTO ").append(tableName).append(" (");
        boolean appendComma = false;
        for (ColumnInfo columnInfo : columns) {
            if (appendComma) {
                sb.append(",");
            }
            appendComma = true;
            sb.append(columnInfo.name);
        }
        sb.append(") VALUES (?");
        for (int i = 1; i < columns.length; ++i) {
            sb.append(",?");
        }
        sb.append(")");
        return sb.toString();
    }

    private static String buildUpdateSql(String tableName, ColumnInfo[] columns) {
        StringBuilder sb = new StringBuilder();
        sb.append("UPDATE ").append(tableName).append(" SET ");
        boolean appendComma = false;
        for (ColumnInfo columnInfo : columns) {
            if (appendComma) {
                sb.append(",");
            }
            appendComma = true;
            sb.append(columnInfo.name).append("=?");
        }
        sb.append(" WHERE ").append(BaseColumns._ID).append("=?");
        return sb.toString();
    }

    private static String buildDeleteSql(String tableName) {
        return "DELETE FROM " + tableName + " WHERE " + BaseColumns._ID + "=?";
    }

    private static String buildDropSql(String tableName) {
        return "DROP TABLE IF EXISTS " + tableName;
    }

    @Inject
    public DvrDatabaseHelper(@ApplicationContext Context context, DvrFlags dvrFlags) {
        super(context,
                DB_NAME,
                null,
                (dvrFlags.startEarlyEndLateEnabled() ? DATABASE_VERSION + 1 : DATABASE_VERSION));
        mDvrFlags = dvrFlags;
    }

    @Override
    public void onConfigure(SQLiteDatabase db) {
        db.setForeignKeyConstraintsEnabled(true);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        if (mDvrFlags.startEarlyEndLateEnabled()) {
            if (DEBUG) Log.d(TAG, "Executing SQL: " + SQL_CREATE_SCHEDULES_WITH_TIME_OFFSET);
            db.execSQL(SQL_CREATE_SCHEDULES_WITH_TIME_OFFSET);
        } else {
            if (DEBUG) Log.d(TAG, "Executing SQL: " + SQL_CREATE_SCHEDULES);
            db.execSQL(SQL_CREATE_SCHEDULES);
        }
        if (DEBUG) Log.d(TAG, "Executing SQL: " + SQL_CREATE_SERIES_RECORDINGS);
        db.execSQL(SQL_CREATE_SERIES_RECORDINGS);
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        if (oldVersion < 17) {
            if (DEBUG) Log.d(TAG, "Executing SQL: " + SQL_DROP_SCHEDULES);
            db.execSQL(SQL_DROP_SCHEDULES);
            if (DEBUG) Log.d(TAG, "Executing SQL: " + SQL_DROP_SERIES_RECORDINGS);
            db.execSQL(SQL_DROP_SERIES_RECORDINGS);
            onCreate(db);
            return;
        }
        if (oldVersion < 18) {
            db.execSQL(
                    "ALTER TABLE "
                            + Schedules.TABLE_NAME
                            + " ADD COLUMN "
                            + Schedules.COLUMN_FAILED_REASON
                            + " TEXT DEFAULT null;");
        }
        if (mDvrFlags.startEarlyEndLateEnabled() && oldVersion < 19) {
            db.execSQL("ALTER TABLE " + Schedules.TABLE_NAME + " ADD COLUMN "
                    + Schedules.COLUMN_START_OFFSET_MILLIS + " INTEGER NOT NULL DEFAULT '0';");
            db.execSQL("ALTER TABLE " + Schedules.TABLE_NAME + " ADD COLUMN "
                    + Schedules.COLUMN_END_OFFSET_MILLIS + " INTEGER NOT NULL DEFAULT '0';");
        }
    }

    @Override
    public void onDowngrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        if (oldVersion > DATABASE_VERSION) {
            String schedulesBackup = "schedules_backup";
            db.execSQL(buildCreateSchedulesSql(schedulesBackup, COLUMNS_SCHEDULES));
            db.execSQL("INSERT INTO " + schedulesBackup +
                    buildSelectSql(COLUMNS_SCHEDULES) + " FROM " + Schedules.TABLE_NAME);
            db.execSQL(SQL_DROP_SCHEDULES);
            db.execSQL(SQL_CREATE_SCHEDULES);
            db.execSQL("INSERT INTO " + Schedules.TABLE_NAME +
                    buildSelectSql(COLUMNS_SCHEDULES) + " FROM " + schedulesBackup);
            db.execSQL(buildDropSql(schedulesBackup));
        }
    }

    /** Handles the query request and returns a {@link Cursor}. */
    public Cursor query(String tableName, String[] projections) {
        SQLiteDatabase db = getReadableDatabase();
        SQLiteQueryBuilder builder = new SQLiteQueryBuilder();
        builder.setTables(tableName);
        return builder.query(db, projections, null, null, null, null, null);
    }

    /** Inserts schedules. */
    public void insertSchedules(ScheduledRecording... scheduledRecordings) {
        SQLiteDatabase db = getWritableDatabase();
        db.beginTransaction();
        try {
            if (mDvrFlags.startEarlyEndLateEnabled()) {
                SQLiteStatement statement =
                        db.compileStatement(SQL_INSERT_SCHEDULES_WITH_TIME_OFFSET);
                for (ScheduledRecording r : scheduledRecordings) {
                    statement.clearBindings();
                    ContentValues values = ScheduledRecording.toContentValuesWithTimeOffset(r);
                    bindColumns(statement, COLUMNS_SCHEDULES_WITH_TIME_OFFSET, values);
                    statement.execute();
                }
            } else {
                SQLiteStatement statement = db.compileStatement(SQL_INSERT_SCHEDULES);
                for (ScheduledRecording r : scheduledRecordings) {
                    statement.clearBindings();
                    ContentValues values = ScheduledRecording.toContentValues(r);
                    bindColumns(statement, COLUMNS_SCHEDULES, values);
                    statement.execute();
                }
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    /** Update schedules. */
    public void updateSchedules(ScheduledRecording... scheduledRecordings) {
        SQLiteDatabase db = getWritableDatabase();
        db.beginTransaction();
        try {
            if (mDvrFlags.startEarlyEndLateEnabled()) {
                SQLiteStatement statement =
                        db.compileStatement(SQL_UPDATE_SCHEDULES_WITH_TIME_OFFSET);
                for (ScheduledRecording r : scheduledRecordings) {
                    statement.clearBindings();
                    ContentValues values = ScheduledRecording.toContentValuesWithTimeOffset(r);
                    bindColumns(statement, COLUMNS_SCHEDULES_WITH_TIME_OFFSET, values);
                    statement.bindLong(COLUMNS_SCHEDULES_WITH_TIME_OFFSET.length + 1, r.getId());
                    statement.execute();
                }
            } else {
                SQLiteStatement statement = db.compileStatement(SQL_UPDATE_SCHEDULES);
                for (ScheduledRecording r : scheduledRecordings) {
                    statement.clearBindings();
                    ContentValues values = ScheduledRecording.toContentValues(r);
                    bindColumns(statement, COLUMNS_SCHEDULES, values);
                    statement.bindLong(COLUMNS_SCHEDULES.length + 1, r.getId());
                    statement.execute();
                }
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    /** Delete schedules. */
    public void deleteSchedules(ScheduledRecording... scheduledRecordings) {
        SQLiteDatabase db = getWritableDatabase();
        SQLiteStatement statement = db.compileStatement(SQL_DELETE_SCHEDULES);
        db.beginTransaction();
        try {
            for (ScheduledRecording r : scheduledRecordings) {
                statement.clearBindings();
                statement.bindLong(1, r.getId());
                statement.execute();
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    /** Inserts series recordings. */
    public void insertSeriesRecordings(SeriesRecording... seriesRecordings) {
        SQLiteDatabase db = getWritableDatabase();
        SQLiteStatement statement = db.compileStatement(SQL_INSERT_SERIES_RECORDINGS);
        db.beginTransaction();
        try {
            for (SeriesRecording r : seriesRecordings) {
                statement.clearBindings();
                ContentValues values = SeriesRecording.toContentValues(r);
                bindColumns(statement, COLUMNS_SERIES_RECORDINGS, values);
                statement.execute();
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    /** Update series recordings. */
    public void updateSeriesRecordings(SeriesRecording... seriesRecordings) {
        SQLiteDatabase db = getWritableDatabase();
        SQLiteStatement statement = db.compileStatement(SQL_UPDATE_SERIES_RECORDINGS);
        db.beginTransaction();
        try {
            for (SeriesRecording r : seriesRecordings) {
                statement.clearBindings();
                ContentValues values = SeriesRecording.toContentValues(r);
                bindColumns(statement, COLUMNS_SERIES_RECORDINGS, values);
                statement.bindLong(COLUMNS_SERIES_RECORDINGS.length + 1, r.getId());
                statement.execute();
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    /** Delete series recordings. */
    public void deleteSeriesRecordings(SeriesRecording... seriesRecordings) {
        SQLiteDatabase db = getWritableDatabase();
        SQLiteStatement statement = db.compileStatement(SQL_DELETE_SERIES_RECORDINGS);
        db.beginTransaction();
        try {
            for (SeriesRecording r : seriesRecordings) {
                statement.clearBindings();
                statement.bindLong(1, r.getId());
                statement.execute();
            }
            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    private void bindColumns(
            SQLiteStatement statement, ColumnInfo[] columns, ContentValues values) {
        for (int i = 0; i < columns.length; ++i) {
            ColumnInfo columnInfo = columns[i];
            Object value = values.get(columnInfo.name);
            switch (columnInfo.type) {
                case SQL_DATA_TYPE_LONG:
                    if (value == null) {
                        statement.bindNull(i + 1);
                    } else {
                        statement.bindLong(i + 1, (Long) value);
                    }
                    break;
                case SQL_DATA_TYPE_INT:
                    if (value == null) {
                        statement.bindNull(i + 1);
                    } else {
                        statement.bindLong(i + 1, (Integer) value);
                    }
                    break;
                case SQL_DATA_TYPE_STRING:
                    {
                        if (TextUtils.isEmpty((String) value)) {
                            statement.bindNull(i + 1);
                        } else {
                            statement.bindString(i + 1, (String) value);
                        }
                        break;
                    }
            }
        }
    }

    private static class ColumnInfo {
        final String name;
        final int type;
        final String constraint;

        ColumnInfo(String name, int type) {
            this(name, type, "");
        }

        ColumnInfo(String name, int type, String constraint) {
            this.name = name;
            this.type = type;
            this.constraint = constraint;
        }
    }
}
