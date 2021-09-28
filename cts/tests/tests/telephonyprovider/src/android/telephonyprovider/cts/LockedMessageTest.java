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

package android.telephonyprovider.cts;

import static android.telephonyprovider.cts.DefaultSmsAppHelper.assumeTelephony;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.google.common.truth.Truth.assertThat;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;

@SmallTest
public class LockedMessageTest {
    private static final String MMS_SUBJECT_ONE = "MMS Subject CTS One";
    private static final String SMS_SUBJECT_TWO = "SMS Subject CTS One";
    private static final String TEST_SMS_BODY = "TEST_SMS_BODY";
    private static final String TEST_ADDRESS = "+19998880001";
    private static final int TEST_THREAD_ID = 101;

    private ContentResolver mContentResolver;

    @BeforeClass
    public static void ensureDefaultSmsApp() {
        DefaultSmsAppHelper.ensureDefaultSmsApp();
    }

    @AfterClass
    public static void cleanup() {
        ContentResolver contentResolver = getInstrumentation().getContext().getContentResolver();
        contentResolver.delete(Telephony.Mms.CONTENT_URI, null, null);
        contentResolver.delete(Telephony.Sms.CONTENT_URI, null, null);
    }

    @Before
    public void setupTestEnvironment() {
        assumeTelephony();
        cleanup();
        mContentResolver = getInstrumentation().getContext().getContentResolver();
    }

    /**
     * The purpose of this test is to check at least one locked message found in the union of MMS
     * and SMS messages.
     */
    @Test
    @Ignore
    public void testLockedMessage_atMostOneLockedMessage() {
        Uri mmsUri = insertMmsWithLockedMessage();
        String mmsId = mmsUri.getLastPathSegment();

        Uri smsUri = insertSmsWithLockedMessage();
        String smsId = smsUri.getLastPathSegment();

        Cursor cursor = mContentResolver.query(Telephony.MmsSms.CONTENT_LOCKED_URI, null, null,
                null);
        assertThat(cursor.getCount()).isEqualTo(1);
        assertThat(cursor.moveToFirst()).isEqualTo(true);
        assertThat(cursor.getColumnCount()).isEqualTo(1);
        assertThat(cursor.getColumnNames()).isEqualTo(new String[]{Telephony.BaseMmsColumns._ID});
        assertThat(cursor.getInt(cursor.getColumnIndex(Telephony.BaseMmsColumns._ID)))
                .isNotEqualTo(Integer.parseInt(mmsId));
        assertThat(cursor.getInt(cursor.getColumnIndex(Telephony.BaseMmsColumns._ID)))
                .isEqualTo(Integer.parseInt(smsId));
    }

    /**
     * The purpose of this test is to check there is no locked message found in the union of MMS and
     * SMS messages.
     */
    @Test
    public void testLockedMessage_getNoLockedMessage() {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE, Telephony.Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri).isNotNull();
        Cursor mmsCursor = mContentResolver.query(mmsUri, null, null, null);
        assertThat(mmsCursor.getCount()).isEqualTo(1);

        Uri smsUri = insertIntoSmsTable(SMS_SUBJECT_TWO);
        assertThat(smsUri).isNotNull();
        Cursor smCursor = mContentResolver.query(smsUri, null, null, null);
        assertThat(smCursor.getCount()).isEqualTo(1);

        Cursor cursor = mContentResolver.query(Telephony.MmsSms.CONTENT_LOCKED_URI, null, null,
                null);
        assertThat(cursor.getCount()).isEqualTo(0);
    }

    private Uri insertMmsWithLockedMessage() {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE, Telephony.Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri).isNotNull();

        Cursor cursor = mContentResolver.query(mmsUri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);

        final ContentValues updateValues = new ContentValues();
        updateValues.put(Telephony.Mms.LOCKED, 1);

        int cursorUpdate = mContentResolver.update(mmsUri, updateValues, null, null);
        assertThat(cursorUpdate).isEqualTo(1);

        Cursor cursorAfterReadUpdate = mContentResolver.query(mmsUri, null, null, null);

        assertThat(cursorAfterReadUpdate.getCount()).isEqualTo(1);
        assertThat(cursorAfterReadUpdate.moveToFirst()).isEqualTo(true);
        assertThat(
                cursorAfterReadUpdate.getInt(
                        cursorAfterReadUpdate.getColumnIndex(Telephony.Mms.LOCKED)))
                .isEqualTo(
                        1);
        return mmsUri;
    }

    private Uri insertSmsWithLockedMessage() {
        Uri smsUri = insertIntoSmsTable(SMS_SUBJECT_TWO);
        assertThat(smsUri).isNotNull();

        Cursor cursor = mContentResolver.query(smsUri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);

        final ContentValues updateValues = new ContentValues();
        updateValues.put(Telephony.Sms.LOCKED, 1);

        int cursorUpdate = mContentResolver.update(smsUri, updateValues, null, null);
        assertThat(cursorUpdate).isEqualTo(1);

        Cursor cursorAfterReadUpdate = mContentResolver.query(smsUri, null, null, null);

        assertThat(cursorAfterReadUpdate.getCount()).isEqualTo(1);
        assertThat(cursorAfterReadUpdate.moveToFirst()).isEqualTo(true);
        assertThat(
                cursorAfterReadUpdate.getInt(
                        cursorAfterReadUpdate.getColumnIndex(Telephony.Sms.LOCKED)))
                .isEqualTo(
                        1);
        return smsUri;
    }

    private Uri insertIntoMmsTable(String subject, int messageType) {
        final ContentValues mmsValues = new ContentValues();
        mmsValues.put(Telephony.Mms.TEXT_ONLY, 1);
        mmsValues.put(Telephony.Mms.MESSAGE_TYPE, messageType);
        mmsValues.put(Telephony.Mms.SUBJECT, subject);
        mmsValues.put(Telephony.Mms.THREAD_ID, TEST_THREAD_ID);
        final Uri mmsUri = mContentResolver.insert(Telephony.Mms.CONTENT_URI, mmsValues);
        return mmsUri;
    }

    private Uri insertIntoSmsTable(String subject) {
        final ContentValues smsValues = new ContentValues();
        smsValues.put(Telephony.Sms.SUBJECT, subject);
        smsValues.put(Telephony.Sms.ADDRESS, TEST_ADDRESS);
        smsValues.put(Telephony.Sms.BODY, TEST_SMS_BODY);
        smsValues.put(Telephony.Sms.THREAD_ID, TEST_THREAD_ID);
        final Uri smsUri = mContentResolver.insert(Telephony.Sms.CONTENT_URI, smsValues);
        return smsUri;
    }

}
