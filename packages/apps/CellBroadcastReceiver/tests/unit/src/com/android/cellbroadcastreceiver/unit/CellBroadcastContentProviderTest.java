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

import static org.mockito.Mockito.doReturn;
import static com.google.common.truth.Truth.assertThat;

import android.content.ContentValues;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony.CellBroadcasts;
import android.telephony.SmsCbCmasInfo;
import android.telephony.SmsCbEtwsInfo;
import android.telephony.SmsCbLocation;
import android.telephony.SmsCbMessage;
import android.test.mock.MockContentResolver;
import android.test.mock.MockContext;
import android.util.Log;
import com.android.cellbroadcastreceiver.CellBroadcastDatabaseHelper;
import junit.framework.TestCase;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

public class CellBroadcastContentProviderTest extends TestCase {
    private static final String TAG = CellBroadcastContentProviderTest.class.getSimpleName();

    public static final Uri CONTENT_URI = Uri.parse("content://cellbroadcasts-app");

    private static final int GEO_SCOPE = 1;
    private static final String PLMN = "123456";
    private static final int LAC = 13;
    private static final int CID = 123;
    private static final int SERIAL_NUMBER = 17984;
    private static final int SERVICE_CATEGORY = 4379;
    private static final String LANGUAGE_CODE = "en";
    private static final String MESSAGE_BODY = "AMBER Alert: xxxx";
    private static final int MESSAGE_FORMAT = 1;
    private static final int MESSAGE_PRIORITY = 3;
    private static final int ETWS_WARNING_TYPE = 1;
    private static final int CMAS_MESSAGE_CLASS = 1;
    private static final int CMAS_CATEGORY = 6;
    private static final int CMAS_RESPONSE_TYPE = 1;
    private static final int CMAS_SEVERITY = 2;
    private static final int CMAS_URGENCY = 3;
    private static final int CMAS_CERTAINTY = 4;

    private CellBroadcastContentProviderTestable mCellBroadcastProviderTestable;
    private MockContextWithProvider mContext;
    private MockContentResolver mContentResolver;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);

        mCellBroadcastProviderTestable = new CellBroadcastContentProviderTestable();
        mContext = new MockContextWithProvider(mCellBroadcastProviderTestable);
        mContentResolver = mContext.getContentResolver();
    }

    @Override
    protected void tearDown() throws Exception {
        mCellBroadcastProviderTestable.closeDatabase();
        super.tearDown();
    }

    @Test
    public void testInsert() {
        try {
            ContentValues msg = new ContentValues();
            msg.put(CellBroadcasts.SERIAL_NUMBER, SERIAL_NUMBER);
            mContentResolver.insert(CONTENT_URI, msg);
            fail();
        } catch (UnsupportedOperationException ex) {
            // pass the test
        }
    }

    @Test
    public void testDelete() {
        try {
            mContentResolver.delete(CONTENT_URI, null, null);
            fail();
        } catch (UnsupportedOperationException ex) {
            // pass the test
        }
    }

    @Test
    public void testUpdate() {
        try {
            mContentResolver.update(CONTENT_URI, null, null);
            fail();
        } catch (UnsupportedOperationException ex) {
            // pass the test
        }
    }

    @Test
    public void testDeleteAll() {
        // Insert a cell broadcast message
        mCellBroadcastProviderTestable.insertNewBroadcast(fakeSmsCbMessage());
        // Verify that the record is inserted into the database correctly.
        Cursor cursor = mContentResolver.query(CONTENT_URI,
                CellBroadcastDatabaseHelper.QUERY_COLUMNS, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);
        // delete all
        mCellBroadcastProviderTestable.deleteAllBroadcasts();
        cursor = mContentResolver.query(CONTENT_URI, CellBroadcastDatabaseHelper.QUERY_COLUMNS,
                null, null, null);
        assertThat(cursor.getCount()).isEqualTo(0);
    }

    @Test
    public void testDeleteBroadcast() {
        // Insert two cell broadcast message
        mCellBroadcastProviderTestable.insertNewBroadcast(fakeSmsCbMessage());
        mCellBroadcastProviderTestable.insertNewBroadcast(fakeSmsCbMessage());
        // Verify that the record is inserted into the database correctly.
        Cursor cursor = mContentResolver.query(CONTENT_URI,
                CellBroadcastDatabaseHelper.QUERY_COLUMNS, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(2);
        // delete one message
        mCellBroadcastProviderTestable.deleteBroadcast(1);
        cursor = mContentResolver.query(CONTENT_URI, CellBroadcastDatabaseHelper.QUERY_COLUMNS,
                null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);
    }

    @Test
    public void testInsertQuery() {
        // Insert a cell broadcast message
        mCellBroadcastProviderTestable.insertNewBroadcast(fakeSmsCbMessage());

        // Verify that the record is inserted into the database correctly.
        Cursor cursor = mContentResolver.query(CONTENT_URI,
                CellBroadcastDatabaseHelper.QUERY_COLUMNS, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);

        cursor.moveToNext();
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.GEOGRAPHICAL_SCOPE)))
                .isEqualTo(GEO_SCOPE);
        assertThat(cursor.getString(cursor.getColumnIndexOrThrow(CellBroadcasts.PLMN)))
                .isEqualTo(PLMN);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.LAC))).isEqualTo(LAC);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.CID))).isEqualTo(CID);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.SERIAL_NUMBER)))
                .isEqualTo(SERIAL_NUMBER);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.SERVICE_CATEGORY)))
                .isEqualTo(SERVICE_CATEGORY);
        assertThat(cursor.getString(cursor.getColumnIndexOrThrow(CellBroadcasts.LANGUAGE_CODE)))
                .isEqualTo(LANGUAGE_CODE);
        assertThat(cursor.getString(cursor.getColumnIndexOrThrow(CellBroadcasts.MESSAGE_BODY)))
                .isEqualTo(MESSAGE_BODY);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.MESSAGE_PRIORITY)))
                .isEqualTo(MESSAGE_PRIORITY);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.ETWS_WARNING_TYPE)))
                .isEqualTo(ETWS_WARNING_TYPE);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.CMAS_MESSAGE_CLASS)))
                .isEqualTo(CMAS_MESSAGE_CLASS);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.CMAS_CATEGORY)))
                .isEqualTo(CMAS_CATEGORY);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.CMAS_RESPONSE_TYPE)))
                .isEqualTo(CMAS_RESPONSE_TYPE);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.CMAS_SEVERITY)))
                .isEqualTo(CMAS_SEVERITY);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.CMAS_URGENCY)))
                .isEqualTo(CMAS_URGENCY);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.CMAS_CERTAINTY)))
                .isEqualTo(CMAS_CERTAINTY);
    }

    /**
     * This is used to give the CellBroadcastContentProviderTest a mocked context which takes a
     * CellBroadcastProvider and attaches it to the ContentResolver.
     */
    private class MockContextWithProvider extends MockContext {
        private final MockContentResolver mResolver;

        MockContextWithProvider(CellBroadcastContentProviderTestable cellBroadcastProvider) {
            mResolver = new MockContentResolver();
            cellBroadcastProvider.initializeForTesting(this);

            // Add given cellBroadcastProvider to mResolver, so that mResolver can send queries
            // to the provider.
            mResolver.addProvider("cellbroadcasts-app", cellBroadcastProvider);
        }

        @Override
        public MockContentResolver getContentResolver() {
            return mResolver;
        }


        @Override
        public Object getSystemService(String name) {
            Log.d(TAG, "getSystemService: returning null");
            return null;
        }

        @Override
        public int checkCallingOrSelfPermission(String permission) {
            return PackageManager.PERMISSION_GRANTED;
        }
    }

    private SmsCbMessage fakeSmsCbMessage() {
        return new SmsCbMessage(MESSAGE_FORMAT, GEO_SCOPE, SERIAL_NUMBER,
                new SmsCbLocation(PLMN, LAC, CID), SERVICE_CATEGORY, LANGUAGE_CODE, 0 ,
                MESSAGE_BODY, MESSAGE_PRIORITY, new SmsCbEtwsInfo(ETWS_WARNING_TYPE, false,
                false, false, null),
                new SmsCbCmasInfo(CMAS_MESSAGE_CLASS, CMAS_CATEGORY, CMAS_RESPONSE_TYPE,
                        CMAS_SEVERITY, CMAS_URGENCY, CMAS_CERTAINTY), 0, null,
                System.currentTimeMillis(), 1, 0);
    }
 }
