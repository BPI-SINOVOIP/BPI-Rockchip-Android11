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

package android.inputmethodservice.cts.db;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

import androidx.annotation.NonNull;

import java.util.List;

/**
 * Abstraction of SQLite database.
 */
public abstract class Database {

    private final SQLiteOpenHelper mHelper;

    /**
     * Create {@link Database}.
     *
     * @param context to use for locating paths to the the database.
     * @param name of the database file, or null for an in-memory database.
     * @param version number of the database (starting at 1).
     */
    public Database(Context context, String name, int version) {
        mHelper = new SQLiteOpenHelper(context, name, null /* factory */, version) {
            @Override
            public void onCreate(SQLiteDatabase db) {
                db.beginTransaction();
                try {
                    for (Table table : getTables()) {
                        db.execSQL(table.createTableSql());
                    }
                    db.setTransactionSuccessful();
                } finally {
                    db.endTransaction();
                }
            }

            @Override
            public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
                // nothing to do so far.
            }
        };
    }

    @NonNull
    protected abstract List<Table> getTables();

    /**
     * A wrapper method for {@link SQLiteDatabase#query(String, String[], String, String[], String,
     * String, String)}.
     *
     * @param table the table name to compile the query against.
     * @param projection a list of which columns to return. Passing {@code null} will return all
     *                   columns, which is discouraged to prevent reading data from storage that
     *                   isn't going to be used.
     * @param selection a filter declaring which rows to return, formatted as an SQL WHERE clause
     *                  (excluding the WHERE itself). Passing {@code null} will return all rows for
     *                  the given table.
     * @param selectionArgs you may include {@code ?}s in selection, which will be replaced by the
     *                      values from selectionArgs, in order that they appear in the selection.
     *                      The values will be bound as Strings.
     * @param orderBy how to order the rows, formatted as an SQL ORDER BY clause (excluding the
     *                ORDER BY itself). Passing null will use the default sort order, which may be
     *                unordered.
     * @return A {@link Cursor} object, which is positioned before the first entry. Note that
     *         {@link Cursor}s are not synchronized, see the documentation for more details.
     */
    public Cursor query(String table, String[] projection, String selection, String[] selectionArgs,
            String orderBy) {
        return mHelper.getReadableDatabase()
                .query(table, projection, selection, selectionArgs, null /* groupBy */,
                        null /* having */, orderBy);
    }

    /**
     * A wrapper method for {@link SQLiteDatabase#insert(String, String, ContentValues)}.
     *
     * @param table the table to insert the row into.
     * @param values this map contains the initial column values for the row. The keys should be the
     *               column names and the values the column values.
     * @return the row ID of the newly inserted row, or {@code -1} if an error occurred.
     */
    public long insert(String table, ContentValues values) {
        return mHelper.getWritableDatabase().insert(table, null /* nullColumnHack */, values);
    }

    /**
     * A wrapper method for {@link SQLiteDatabase#delete(String, String, String[])}.
     *
     * @param table the table to delete from.
     * @param selection the optional WHERE clause to apply when deleting.  Passing {@code null} will
     *                  delete all rows.
     * @param selectionArgs You may include {@code ?}s in the where clause, which will be replaced
     *                      by the values from whereArgs. The values will be bound as Strings.
     * @return the number of rows affected if a whereClause is passed in, {@code 0} otherwise. To
     *         remove all rows and get a count pass {@code 1} as the {@code selection}.
     */
    public int delete(String table, String selection, String[] selectionArgs) {
        return mHelper.getWritableDatabase().delete(table, selection, selectionArgs);
    }

    /**
     * A wrapper method for {@link SQLiteDatabase#update(String, ContentValues, String, String[])}.
     *
     * @param table the table to update in
     * @param values a map from column names to new column values. {@code null} is a valid value
     *               that will be translated to NULL.
     * @param selection the optional WHERE clause to apply when updating. Passing {@code null} will
     *                  update all rows.
     * @param selectionArgs You may include {@code ?}s in the where clause, which will be replaced
     *                      by the values from whereArgs. The values will be bound as Strings.
     * @return the number of rows affected
     */
    public int update(String table, ContentValues values, String selection,
            String[] selectionArgs) {
        return mHelper.getWritableDatabase().update(table, values, selection, selectionArgs);
    }

    /**
     * Close any open database object.
     */
    public void close() {
        mHelper.close();
    }
}
