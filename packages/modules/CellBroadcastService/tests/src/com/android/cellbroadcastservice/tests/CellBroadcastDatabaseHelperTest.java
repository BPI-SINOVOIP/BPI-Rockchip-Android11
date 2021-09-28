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
package com.android.cellbroadcastservice.tests;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.provider.Telephony.CellBroadcasts;
import android.util.Log;
import com.android.cellbroadcastservice.CellBroadcastProvider;
import java.util.Arrays;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class CellBroadcastDatabaseHelperTest {

    private final static String TAG = CellBroadcastDatabaseHelperTest.class.getSimpleName();

    private CellBroadcastProvider.CellBroadcastDatabaseHelper mHelper;
    private SQLiteOpenHelper mInMemoryDbHelper; // used to give us an in-memory db
    @Mock
    Context mContext;

    @Before
    public void setUp() {
        Log.d(TAG, "setUp() +");
        MockitoAnnotations.initMocks(this);
        mHelper = new CellBroadcastProvider.CellBroadcastDatabaseHelper(mContext);
        mInMemoryDbHelper = new InMemoryCellBroadcastProviderDbHelperV1();
        Log.d(TAG, "setUp() -");
    }

    @Test
    public void dataBaseHelper_create() {
        Log.d(TAG, "dataBaseHelper_create");
        SQLiteDatabase db = mInMemoryDbHelper.getWritableDatabase();
        Cursor cursor = db.query(CellBroadcastProvider.CELL_BROADCASTS_TABLE_NAME,
                null, null, null, null, null, null);
        String[] columns = cursor.getColumnNames();
        Log.d(TAG, "cellbroadcastservice columns before upgrade: " + Arrays.toString(columns));
        assertFalse(Arrays.asList(columns).contains(CellBroadcasts.SLOT_INDEX));
        assertFalse(Arrays.asList(columns).contains(CellBroadcasts.LOCATION_CHECK_TIME));
        assertFalse(Arrays.asList(columns).contains(CellBroadcasts.MESSAGE_DISPLAYED));
        assertFalse(Arrays.asList(columns).contains(CellBroadcasts.DATA_CODING_SCHEME));
        assertFalse(Arrays.asList(columns).contains("etws_is_primary"));
        assertEquals(0, cursor.getCount());
    }

    @Test
    public void databaseHelperOnUpgrade_V1ToV4() {
        Log.d(TAG, "databaseHelperOnUpgrade_V1ToV4");
        SQLiteDatabase db = mInMemoryDbHelper.getWritableDatabase();
        // version 1 -> 4 trigger in onUpgrade
        mHelper.onUpgrade(db, 1, CellBroadcastProvider.DATABASE_VERSION);
        // the upgraded db must have the slot index field
        Cursor upgradedCursor = db.query(CellBroadcastProvider.CELL_BROADCASTS_TABLE_NAME,
                null, null, null, null, null, null);
        String[] upgradedColumns = upgradedCursor.getColumnNames();
        Log.d(TAG, "cellbroadcastservice columns: " + Arrays.toString(upgradedColumns));
        assertTrue(Arrays.asList(upgradedColumns).contains(CellBroadcasts.SLOT_INDEX));
        assertTrue(Arrays.asList(upgradedColumns).contains(CellBroadcasts.LOCATION_CHECK_TIME));
        assertTrue(Arrays.asList(upgradedColumns).contains(CellBroadcasts.MESSAGE_DISPLAYED));
        assertTrue(Arrays.asList(upgradedColumns).contains(CellBroadcasts.DATA_CODING_SCHEME));
        assertTrue(Arrays.asList(upgradedColumns).contains("etws_is_primary"));
    }

    private static class InMemoryCellBroadcastProviderDbHelperV1 extends SQLiteOpenHelper {

        public InMemoryCellBroadcastProviderDbHelperV1() {
            super(InstrumentationRegistry.getTargetContext(),
                    null,    // db file name is null for in-memory db
                    null,    // CursorFactory is null by default
                    1);      // db version is no-op for tests
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            Log.d(TAG, "IN MEMORY DB CREATED");
            // create database version 1
            db.execSQL("CREATE TABLE " + CellBroadcastProvider.CELL_BROADCASTS_TABLE_NAME + " ("
                    + CellBroadcasts._ID + " INTEGER PRIMARY KEY AUTOINCREMENT,"
                    + CellBroadcasts.SUBSCRIPTION_ID + " INTEGER,"
                    + CellBroadcasts.GEOGRAPHICAL_SCOPE + " INTEGER,"
                    + CellBroadcasts.PLMN + " TEXT,"
                    + CellBroadcasts.LAC + " INTEGER,"
                    + CellBroadcasts.CID + " INTEGER,"
                    + CellBroadcasts.SERIAL_NUMBER + " INTEGER,"
                    + CellBroadcasts.SERVICE_CATEGORY + " INTEGER,"
                    + CellBroadcasts.LANGUAGE_CODE + " TEXT,"
                    + CellBroadcasts.MESSAGE_BODY + " TEXT,"
                    + CellBroadcasts.MESSAGE_FORMAT + " INTEGER,"
                    + CellBroadcasts.MESSAGE_PRIORITY + " INTEGER,"
                    + CellBroadcasts.ETWS_WARNING_TYPE + " INTEGER,"
                    + CellBroadcasts.CMAS_MESSAGE_CLASS + " INTEGER,"
                    + CellBroadcasts.CMAS_CATEGORY + " INTEGER,"
                    + CellBroadcasts.CMAS_RESPONSE_TYPE + " INTEGER,"
                    + CellBroadcasts.CMAS_SEVERITY + " INTEGER,"
                    + CellBroadcasts.CMAS_URGENCY + " INTEGER,"
                    + CellBroadcasts.CMAS_CERTAINTY + " INTEGER,"
                    + CellBroadcasts.RECEIVED_TIME + " BIGINT,"
                    + CellBroadcasts.MESSAGE_BROADCASTED + " BOOLEAN DEFAULT 0,"
                    + CellBroadcasts.GEOMETRIES + " TEXT,"
                    + CellBroadcasts.MAXIMUM_WAIT_TIME + " INTEGER);");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        }
    }
}

