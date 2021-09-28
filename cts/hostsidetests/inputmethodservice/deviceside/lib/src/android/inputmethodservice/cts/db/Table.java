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
import android.database.Cursor;

import androidx.annotation.NonNull;

import java.util.stream.Stream;

/**
 * Abstraction of SQLite database table.
 * @param <E> type of table entities.
 */
public abstract class Table<E> {

    public final String mName;
    private final Entity<E> mEntity;

    protected Table(String name, Entity<E> entity) {
        mName = name;
        mEntity = entity;
    }

    /**
     * @return name of this table.
     */
    public String name() {
        return mName;
    }

    /**
     * Build {@link ContentValues} object from {@code entity}.
     *
     * @param entity an input data to be converted.
     * @return a converted {@link ContentValues} object.
     */
    public abstract ContentValues buildContentValues(E entity);

    /**
     * Build {@link Stream} object from {@link Cursor} comes from Content Provider.
     *
     * @param cursor a {@link Cursor} object to be converted.
     * @return a converted {@link Stream} object.
     */
    public abstract Stream<E> buildStream(Cursor cursor);

    /**
     * Returns SQL statement to create this table, such that
     * "CREATE TABLE IF NOT EXISTS table_name \
     *  (_id INTEGER PRIMARY KEY AUTOINCREMENT, column2_name column2_type, ...)"
     */
    @NonNull
    public String createTableSql() {
        return "CREATE TABLE IF NOT EXISTS " + mName + " " + mEntity.createEntitySql();
    }

    protected Field getField(String fieldName) {
        return mEntity.getField(fieldName);
    }
}
