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

package com.android.cellbroadcastservice.tests;

import static com.android.cellbroadcastservice.CellBroadcastProvider.QUERY_COLUMNS;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.doReturn;

import android.content.ContentValues;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony.CellBroadcasts;
import android.test.mock.MockContentResolver;
import android.test.mock.MockContext;
import android.util.Log;

import com.android.cellbroadcastservice.CellBroadcastProvider;
import com.android.cellbroadcastservice.CellBroadcastProvider.CellBroadcastPermissionChecker;

import junit.framework.TestCase;

import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

public class CellBroadcastProviderTest extends TestCase {
    private static final String TAG = CellBroadcastProviderTest.class.getSimpleName();

    public static final Uri CONTENT_URI = Uri.parse("content://cellbroadcasts");

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
    private static final int ETWS_IS_PRIMARY = 0;
    private static final int CMAS_MESSAGE_CLASS = 1;
    private static final int CMAS_CATEGORY = 6;
    private static final int CMAS_RESPONSE_TYPE = 1;
    private static final int CMAS_SEVERITY = 2;
    private static final int CMAS_URGENCY = 3;
    private static final int CMAS_CERTAINTY = 4;
    private static final int RECEIVED_TIME = 1562792637;
    private static final int MESSAGE_BROADCASTED = 1;
    private static final int MESSAGE_NOT_DISPLAYED = 0;
    private static final String GEOMETRIES_COORDINATES = "polygon|0,0|0,1|1,1|1,0;circle|0,0|100";

    private static final String SELECT_BY_ID = CellBroadcasts._ID + "=?";

    private CellBroadcastProviderTestable mCellBroadcastProviderTestable;
    private MockContextWithProvider mContext;
    private MockContentResolver mContentResolver;

    @Mock
    private CellBroadcastPermissionChecker mMockPermissionChecker;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        MockitoAnnotations.initMocks(this);
        doReturn(true).when(mMockPermissionChecker).hasFullAccessPermission();

        mCellBroadcastProviderTestable = new CellBroadcastProviderTestable(mMockPermissionChecker);
        mContext = new MockContextWithProvider(mCellBroadcastProviderTestable);
        mContentResolver = mContext.getContentResolver();
    }

    @Override
    protected void tearDown() throws Exception {
        mCellBroadcastProviderTestable.closeDatabase();
        super.tearDown();
    }

    @Test
    public void testUpdate() {
        // Insert a cellbroadcast to the database.
        ContentValues cv = fakeCellBroadcast();
        Uri uri = mContentResolver.insert(CONTENT_URI, cv);
        assertThat(uri).isNotNull();

        // Change some fields of this cell broadcast.
        int messageDisplayed = 1 - cv.getAsInteger(CellBroadcasts.MESSAGE_DISPLAYED);
        int receivedTime = 1234555555;
        cv.put(CellBroadcasts.MESSAGE_DISPLAYED, messageDisplayed);
        cv.put(CellBroadcasts.RECEIVED_TIME, receivedTime);
        mContentResolver.update(CONTENT_URI, cv, SELECT_BY_ID,
                new String[] { uri.getLastPathSegment() });

        // Query and check if the update is succeeded.
        Cursor cursor = mContentResolver.query(CONTENT_URI, QUERY_COLUMNS,
                SELECT_BY_ID, new String[] { uri.getLastPathSegment() }, null /* orderBy */);
        cursor.moveToNext();

        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.RECEIVED_TIME)))
                .isEqualTo(receivedTime);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.MESSAGE_DISPLAYED)))
                .isEqualTo(messageDisplayed);
        cursor.close();
    }

    @Test
    public void testUpdate_WithoutWritePermission_fail() {
        ContentValues cv = fakeCellBroadcast();
        Uri uri = mContentResolver.insert(CONTENT_URI, cv);
        assertThat(uri).isNotNull();

        // Revoke the write permission
        doReturn(false).when(mMockPermissionChecker).hasFullAccessPermission();

        try {
            mContentResolver.update(CONTENT_URI, cv, SELECT_BY_ID,
                    new String[] { uri.getLastPathSegment() });
            fail();
        } catch (SecurityException ex) {
            // pass the test
        }
    }

    @Test
    public void testGetAllCellBroadcast() {
        // Insert some cell broadcasts which message_displayed is false
        int messageNotDisplayedCount = 5;
        ContentValues cv = fakeCellBroadcast();
        cv.put(CellBroadcasts.MESSAGE_DISPLAYED, 0);
        for (int i = 0; i < messageNotDisplayedCount; i++) {
            mContentResolver.insert(CONTENT_URI, cv);
        }

        // Insert some cell broadcasts which message_displayed is true
        int messageDisplayedCount = 6;
        cv.put(CellBroadcasts.MESSAGE_DISPLAYED, 1);
        for (int i = 0; i < messageDisplayedCount; i++) {
            mContentResolver.insert(CONTENT_URI, cv);
        }

        // Query the broadcast with message_displayed is false
        Cursor cursor = mContentResolver.query(
                CONTENT_URI,
                QUERY_COLUMNS,
                String.format("%s=?", CellBroadcasts.MESSAGE_DISPLAYED), /* selection */
                new String[] {"0"}, /* selectionArgs */
                null /* sortOrder */);
        assertThat(cursor.getCount()).isEqualTo(messageNotDisplayedCount);
    }

    @Test
    public void testDelete_withoutWritePermission_throwSecurityException() {
        Uri uri = mContentResolver.insert(CONTENT_URI, fakeCellBroadcast());
        assertThat(uri).isNotNull();

        // Revoke the write permission
        doReturn(false).when(mMockPermissionChecker).hasFullAccessPermission();

        try {
            mContentResolver.delete(CONTENT_URI, SELECT_BY_ID,
                    new String[] { uri.getLastPathSegment() });
            fail();
        } catch (SecurityException ex) {
            // pass the test
        }
    }


    @Test
    public void testDelete_oneRecord_success() {
        // Insert a cellbroadcast to the database.
        ContentValues cv = fakeCellBroadcast();
        Uri uri = mContentResolver.insert(CONTENT_URI, cv);
        assertThat(uri).isNotNull();

        String[] selectionArgs = new String[] { uri.getLastPathSegment() };

        // Ensure the cell broadcast is inserted.
        Cursor cursor = mContentResolver.query(CONTENT_URI, QUERY_COLUMNS,
                SELECT_BY_ID, selectionArgs, null /* orderBy */);
        assertThat(cursor.getCount()).isEqualTo(1);
        cursor.close();

        // Delete the cell broadcast
        int rowCount = mContentResolver.delete(CONTENT_URI, SELECT_BY_ID,
                selectionArgs);
        assertThat(rowCount).isEqualTo(1);

        // Ensure the cell broadcast is deleted.
        cursor = mContentResolver.query(CONTENT_URI, QUERY_COLUMNS, SELECT_BY_ID,
                selectionArgs, null /* orderBy */);
        assertThat(cursor.getCount()).isEqualTo(0);
        cursor.close();
    }

    @Test
    public void testDelete_all_success() {
        // Insert a cellbroadcast to the database.
        mContentResolver.insert(CONTENT_URI, fakeCellBroadcast());
        mContentResolver.insert(CONTENT_URI, fakeCellBroadcast());

        // Ensure the cell broadcast are inserted.
        Cursor cursor = mContentResolver.query(CONTENT_URI, QUERY_COLUMNS,
                null /* selection */, null /* selectionArgs */, null /* orderBy */);
        assertThat(cursor.getCount()).isEqualTo(2);
        cursor.close();

        // Delete all cell broadcasts.
        int rowCount = mContentResolver.delete(
                CONTENT_URI, null /* selection */, null /* selectionArgs */);
        assertThat(rowCount).isEqualTo(2);
        cursor.close();

        // Ensure all cell broadcasts are deleted.
        cursor = mContentResolver.query(CONTENT_URI, QUERY_COLUMNS,
                null /* selection */, null /* selectionArgs */, null /* orderBy */);
        assertThat(cursor.getCount()).isEqualTo(0);
        cursor.close();
    }

    @Test
    public void testInsert_withoutWritePermission_fail() {
        doReturn(false).when(mMockPermissionChecker).hasFullAccessPermission();

        try {
            mContentResolver.insert(CONTENT_URI, fakeCellBroadcast());
            fail();
        } catch (SecurityException ex) {
            // pass the test
        }
    }

    @Test
    public void testInsertAndQuery() {
        // Insert a cell broadcast message
        Uri uri = mContentResolver.insert(CONTENT_URI, fakeCellBroadcast());

        // Verify that the return uri is not null and the record is inserted into the database
        // correctly.
        assertThat(uri).isNotNull();
        Cursor cursor = mContentResolver.query(
                CONTENT_URI, QUERY_COLUMNS, SELECT_BY_ID,
                new String[] { uri.getLastPathSegment() }, null /* orderBy */);
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
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.MESSAGE_FORMAT)))
                .isEqualTo(MESSAGE_FORMAT);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.MESSAGE_PRIORITY)))
                .isEqualTo(MESSAGE_PRIORITY);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.ETWS_WARNING_TYPE)))
                .isEqualTo(ETWS_WARNING_TYPE);
        // TODO: Use system API CellBroadcasts.ETWS_IS_PRIMARY in S.
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow("etws_is_primary")))
                .isEqualTo(ETWS_IS_PRIMARY);
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
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.RECEIVED_TIME)))
                .isEqualTo(RECEIVED_TIME);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.MESSAGE_BROADCASTED)))
                .isEqualTo(MESSAGE_BROADCASTED);
        assertThat(cursor.getInt(cursor.getColumnIndexOrThrow(CellBroadcasts.MESSAGE_DISPLAYED)))
                .isEqualTo(MESSAGE_NOT_DISPLAYED);
        assertThat(cursor.getString(cursor.getColumnIndexOrThrow(
                CellBroadcasts.GEOMETRIES))).isEqualTo(GEOMETRIES_COORDINATES);
    }

    /**
     * This is used to give the CellBroadcastProviderTest a mocked context which takes a
     * CellBroadcastProvider and attaches it to the ContentResolver.
     */
    private class MockContextWithProvider extends MockContext {
        private final MockContentResolver mResolver;

        MockContextWithProvider(CellBroadcastProviderTestable cellBroadcastProvider) {
            mResolver = new MockContentResolver();
            cellBroadcastProvider.initializeForTesting(this);

            // Add given cellBroadcastProvider to mResolver, so that mResolver can send queries
            // to the provider.
            mResolver.addProvider(CellBroadcastProvider.AUTHORITY, cellBroadcastProvider);
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

    private static ContentValues fakeCellBroadcast() {
        ContentValues cv = new ContentValues();
        cv.put(CellBroadcasts.GEOGRAPHICAL_SCOPE, GEO_SCOPE);
        cv.put(CellBroadcasts.PLMN, PLMN);
        cv.put(CellBroadcasts.LAC, LAC);
        cv.put(CellBroadcasts.CID, CID);
        cv.put(CellBroadcasts.SERIAL_NUMBER, SERIAL_NUMBER);
        cv.put(CellBroadcasts.SERVICE_CATEGORY, SERVICE_CATEGORY);
        cv.put(CellBroadcasts.LANGUAGE_CODE, LANGUAGE_CODE);
        cv.put(CellBroadcasts.MESSAGE_BODY, MESSAGE_BODY);
        cv.put(CellBroadcasts.MESSAGE_FORMAT, MESSAGE_FORMAT);
        cv.put(CellBroadcasts.MESSAGE_PRIORITY, MESSAGE_PRIORITY);
        cv.put(CellBroadcasts.ETWS_WARNING_TYPE, ETWS_WARNING_TYPE);
        // TODO: Use system API CellBroadcasts.ETWS_IS_PRIMARY in S.
        cv.put("etws_is_primary", ETWS_IS_PRIMARY);
        cv.put(CellBroadcasts.CMAS_MESSAGE_CLASS, CMAS_MESSAGE_CLASS);
        cv.put(CellBroadcasts.CMAS_CATEGORY, CMAS_CATEGORY);
        cv.put(CellBroadcasts.CMAS_RESPONSE_TYPE, CMAS_RESPONSE_TYPE);
        cv.put(CellBroadcasts.CMAS_SEVERITY, CMAS_SEVERITY);
        cv.put(CellBroadcasts.CMAS_URGENCY, CMAS_URGENCY);
        cv.put(CellBroadcasts.CMAS_CERTAINTY, CMAS_CERTAINTY);
        cv.put(CellBroadcasts.RECEIVED_TIME, RECEIVED_TIME);
        cv.put(CellBroadcasts.MESSAGE_BROADCASTED, MESSAGE_BROADCASTED);
        cv.put(CellBroadcasts.MESSAGE_DISPLAYED, MESSAGE_NOT_DISPLAYED);
        cv.put(CellBroadcasts.GEOMETRIES, GEOMETRIES_COORDINATES);
        return cv;
    }
}
