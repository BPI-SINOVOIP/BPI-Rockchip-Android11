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
package com.android.cellbroadcastreceiver.unit;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.ContentResolver;
import android.content.Context;
import android.content.IContentProvider;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.provider.Telephony;
import android.provider.Telephony.CellBroadcasts;
import android.telephony.SmsCbCmasInfo;
import android.util.Log;
import com.android.cellbroadcastreceiver.CellBroadcastDatabaseHelper;
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

    private CellBroadcastDatabaseHelper mHelper; // the actual class being tested
    private SQLiteOpenHelper mInMemoryDbHelper; // used to give us an in-memory db
    @Mock
    Context mContext;
    @Mock
    ContentResolver mContentResolver;
    @Mock
    IContentProvider mContentProviderClient;
    @Mock
    Cursor mCursor;

    @Before
    public void setUp() {
        Log.d(TAG, "setUp() +");
        MockitoAnnotations.initMocks(this);
        doReturn(mContentResolver).when(mContext).getContentResolver();
        mHelper = new CellBroadcastDatabaseHelper(mContext, false);
        mInMemoryDbHelper = new InMemoryCellBroadcastProviderDbHelperV11();
        Log.d(TAG, "setUp() -");
    }

    @Test
    public void dataBaseHelper_create() {
        Log.d(TAG, "dataBaseHelper_create");
        SQLiteDatabase db = mInMemoryDbHelper.getWritableDatabase();
        Cursor cursor = db.query(CellBroadcastDatabaseHelper.TABLE_NAME,
                null, null, null, null, null, null);
        String[] columns = cursor.getColumnNames();
        Log.d(TAG, "cellbroadcastreceiver columns before upgrade: " + Arrays.toString(columns));
        assertFalse(Arrays.asList(columns).contains(CellBroadcasts.SLOT_INDEX));
        assertEquals(0, cursor.getCount());
    }

    @Test
    public void databaseHelperOnUpgrade_V12() {
        Log.d(TAG, "databaseHelperOnUpgrade_V12");
        SQLiteDatabase db = mInMemoryDbHelper.getWritableDatabase();
        // version 11 -> 12 trigger in onUpgrade
        mHelper.onUpgrade(db, 11, 12);
        // the upgraded db must have the slot index field
        Cursor upgradedCursor = db.query(CellBroadcastDatabaseHelper.TABLE_NAME,
                null, null, null, null, null, null);
        String[] upgradedColumns = upgradedCursor.getColumnNames();
        Log.d(TAG, "cellbroadcastreceiver columns: " + Arrays.toString(upgradedColumns));
        assertTrue(Arrays.asList(upgradedColumns).contains(CellBroadcasts.SLOT_INDEX));
    }

    @Test
    public void testMigration() throws Exception {
        Log.d(TAG, "dataBaseHelper_testMigration");
        // mock a legacy provider for data migration
        doReturn(mContentProviderClient).when(mContentResolver).acquireContentProviderClient(
                Telephony.CellBroadcasts.AUTHORITY_LEGACY);
        MatrixCursor mc = new MatrixCursor(CellBroadcastDatabaseHelper.QUERY_COLUMNS);
        mc.addRow(new Object[]{
                1,              // _ID
                0,              // SLOT_INDEX
                0,              // GEOGRAPHICAL_SCOPE
                "311480",       // PLMN
                0,              // LAC
                0,              // CID
                1234,           // SERIAL_NUMBER
                4379,           // SERVICE CATEGORY
                "en",           // LANGUAGE_CODE
                "Test Message", // MESSAGE_BODY
                System.currentTimeMillis(),// DELIVERY_TIME
                true,           // MESSAGE_READ
                1,              // MESSAGE_FORMAT
                3,              // MESSAGE_PRIORITY
                0,              // ETWS_WARNING_TYPE
                SmsCbCmasInfo.CMAS_CLASS_PRESIDENTIAL_LEVEL_ALERT, // CMAS_MESSAGE_CLASS
                0,              // CMAS_CATEGORY
                0,              // CMAS_RESPONSE_TYPE
                0,              // CMAS_SEVERITY
                0,              // CMAS_URGENCY
                0,              // CMAS_CERTAINTY
        });

        doReturn(mc).when(mContentProviderClient).query(any(), any(), any(), any(), any(), any());
        SQLiteDatabase db = mInMemoryDbHelper.getWritableDatabase();
        // version 11 -> 12 trigger in onUpgrade
        mHelper.onUpgrade(db, 11, 12);
        Cursor cursor = db.query(CellBroadcastDatabaseHelper.TABLE_NAME,
                CellBroadcastDatabaseHelper.QUERY_COLUMNS, null, null, null, null, null);
        assertEquals(0, cursor.getCount());

        mHelper.migrateFromLegacy(db);
        // verify insertion from legacy provider is succeed
        verify(mContentProviderClient).query(any(), any(), any(), any(), any(), any());
        cursor = db.query(CellBroadcastDatabaseHelper.TABLE_NAME,
                CellBroadcastDatabaseHelper.QUERY_COLUMNS, null, null, null, null, null);
        assertEquals(1, cursor.getCount());
    }

    private static class InMemoryCellBroadcastProviderDbHelperV11 extends SQLiteOpenHelper {

        public InMemoryCellBroadcastProviderDbHelperV11() {
            super(InstrumentationRegistry.getTargetContext(),
                    null,    // db file name is null for in-memory db
                    null,    // CursorFactory is null by default
                    1);      // db version is no-op for tests
        }

        @Override
        public void onCreate(SQLiteDatabase db) {
            Log.d(TAG, "IN MEMORY DB CREATED");
            // create database version 11
            db.execSQL("CREATE TABLE " + CellBroadcastDatabaseHelper.TABLE_NAME + " ("
                    + Telephony.CellBroadcasts._ID + " INTEGER PRIMARY KEY AUTOINCREMENT,"
                    + Telephony.CellBroadcasts.GEOGRAPHICAL_SCOPE + " INTEGER,"
                    + Telephony.CellBroadcasts.PLMN + " TEXT,"
                    + Telephony.CellBroadcasts.LAC + " INTEGER,"
                    + Telephony.CellBroadcasts.CID + " INTEGER,"
                    + Telephony.CellBroadcasts.SERIAL_NUMBER + " INTEGER,"
                    + Telephony.CellBroadcasts.SERVICE_CATEGORY + " INTEGER,"
                    + Telephony.CellBroadcasts.LANGUAGE_CODE + " TEXT,"
                    + Telephony.CellBroadcasts.MESSAGE_BODY + " TEXT,"
                    + Telephony.CellBroadcasts.DELIVERY_TIME + " INTEGER,"
                    + Telephony.CellBroadcasts.MESSAGE_READ + " INTEGER,"
                    + Telephony.CellBroadcasts.MESSAGE_FORMAT + " INTEGER,"
                    + Telephony.CellBroadcasts.MESSAGE_PRIORITY + " INTEGER,"
                    + Telephony.CellBroadcasts.ETWS_WARNING_TYPE + " INTEGER,"
                    + Telephony.CellBroadcasts.CMAS_MESSAGE_CLASS + " INTEGER,"
                    + Telephony.CellBroadcasts.CMAS_CATEGORY + " INTEGER,"
                    + Telephony.CellBroadcasts.CMAS_RESPONSE_TYPE + " INTEGER,"
                    + Telephony.CellBroadcasts.CMAS_SEVERITY + " INTEGER,"
                    + Telephony.CellBroadcasts.CMAS_URGENCY + " INTEGER,"
                    + Telephony.CellBroadcasts.CMAS_CERTAINTY + " INTEGER);");
        }

        @Override
        public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        }
    }
}
