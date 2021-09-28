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

/**
 * Abstraction of SQLite database column.
 */
public abstract class Field {

    /** Field position in a row. */
    final int mPos;

    /** Column name of this field. */
    final String mName;

    /** Field type of SQLite. */
    final String mSqLiteType;

    static Field newInstance(int pos, String name, int fieldType) {
        switch (fieldType) {
            case Cursor.FIELD_TYPE_INTEGER:
                return new IntegerField(pos, name);
            case Cursor.FIELD_TYPE_STRING:
                return new StringField(pos, name);
            default:
                throw new IllegalArgumentException("Unknown field type: " + fieldType);
        }
    }

    private Field(int pos, String name, int fieldType) {
        this.mPos = pos;
        this.mName = name;
        this.mSqLiteType = toSqLiteType(fieldType);
    }

    /**
     * Read data from {@link Cursor}.
     *
     * @param cursor {@link Cursor} to read.
     * @return long data read from {@code cursor}.
     */
    public long getLong(Cursor cursor) {
        throw buildException(Cursor.FIELD_TYPE_INTEGER);
    }

    /**
     * Read data from {@link Cursor}.
     *
     * @param cursor {@link Cursor} to read.
     * @return {@link String} data read from {@code cursor}.
     */
    public String getString(Cursor cursor) {
        throw buildException(Cursor.FIELD_TYPE_STRING);
    }

    /**
     * Put data to {@link ContentValues}.
     *
     * @param values {@link ContentValues} to be updated.
     * @param value long data to put.
     */
    public void putLong(ContentValues values, long value) {
        throw buildException(Cursor.FIELD_TYPE_INTEGER);
    }

    /**
     * Put data to {@link ContentValues}.
     *
     * @param values {@link ContentValues} to be updated.
     * @param value {@link String} data to put.
     */
    public void putString(ContentValues values, String value) {
        throw buildException(Cursor.FIELD_TYPE_STRING);
    }

    private UnsupportedOperationException buildException(int expectedFieldType) {
        return new UnsupportedOperationException("Illegal type: " + mName + " is " + mSqLiteType
                + ", expected " + toSqLiteType(expectedFieldType));
    }

    private static String toSqLiteType(int fieldType) {
        switch (fieldType) {
            case Cursor.FIELD_TYPE_NULL:
                return "NULL";
            case Cursor.FIELD_TYPE_INTEGER:
                return "INTEGER";
            case Cursor.FIELD_TYPE_FLOAT:
                return "REAL";
            case Cursor.FIELD_TYPE_STRING:
                return "TEXT";
            case Cursor.FIELD_TYPE_BLOB:
                return "BLOB";
            default:
                throw new IllegalArgumentException("Unknown field type: " + fieldType);
        }
    }

    /**
     * Abstraction of INTEGER type field.
     */
    private static final class IntegerField extends Field {

        IntegerField(int pos, String name) {
            super(pos, name, Cursor.FIELD_TYPE_INTEGER);
        }

        @Override
        public long getLong(Cursor cursor) {
            return cursor.getLong(mPos);
        }

        @Override
        public void putLong(ContentValues values, long value) {
            values.put(mName, value);
        }
    }

    /**
     * Abstraction of STRING type field.
     */
    private static final class StringField extends Field {

        StringField(int pos, String name) {
            super(pos, name, Cursor.FIELD_TYPE_STRING);
        }

        @Override
        public String getString(Cursor cursor) {
            return cursor.getString(mPos);
        }

        @Override
        public void putString(ContentValues values, String value) {
            values.put(mName, value);
        }
    }
}
