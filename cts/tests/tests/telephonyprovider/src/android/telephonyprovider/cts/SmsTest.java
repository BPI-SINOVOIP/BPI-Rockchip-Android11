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

import static org.junit.Assert.assertNull;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;


@SmallTest
public class SmsTest {

    private static final String TEST_SMS_BODY = "TEST_SMS_BODY";
    private static final String TEST_ADDRESS = "+19998880001";
    private static final int TEST_THREAD_ID_1 = 101;
    private ContentResolver mContentResolver;
    private SmsTestHelper mSmsTestHelper;

    @BeforeClass
    public static void ensureDefaultSmsApp() {
        DefaultSmsAppHelper.ensureDefaultSmsApp();
    }

    @AfterClass
    public static void cleanup() {
        ContentResolver contentResolver = getInstrumentation().getContext().getContentResolver();
        contentResolver.delete(Telephony.Sms.CONTENT_URI, null, null);
        contentResolver.delete(Telephony.Threads.CONTENT_URI, null, null);
    }

    @Before
    public void setupTestEnvironment() {
        assumeTelephony();
        cleanup();
        mContentResolver = getInstrumentation().getContext().getContentResolver();
        mSmsTestHelper = new SmsTestHelper();
    }

    /**
     * Asserts that a URI returned from an SMS insert operation represents a pass Insert.
     */
    @Test
    public void testSmsInsert() {
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        Cursor cursor = mContentResolver.query(uri, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);
        cursor.moveToNext();

        String actualSmsBody = cursor.getString(cursor.getColumnIndex(Telephony.Sms.BODY));
        assertThat(actualSmsBody).isEqualTo(TEST_SMS_BODY);
    }

    /**
     * The purpose of this test is to perform delete operation and assert that SMS is deleted.
     */
    @Test
    public void testSmsDelete() {
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        int deletedRows = mContentResolver.delete(uri, null, null);

        assertThat(deletedRows).isEqualTo(1);

        Cursor cursor = mContentResolver.query(uri, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(0);
    }

    /**
     * The purpose of this test is to update the message body and verify the message body is
     * updated.
     */
    @Test
    public void testSmsUpdate() {
        String testSmsBodyUpdate = "TEST_SMS_BODY_UPDATED";
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        mSmsTestHelper.assertSmsColumnEquals(Telephony.Sms.BODY, uri, TEST_SMS_BODY);

        ContentValues values = new ContentValues();
        values.put(Telephony.Sms.ADDRESS, TEST_ADDRESS);
        values.put(Telephony.Sms.BODY, testSmsBodyUpdate);

        mContentResolver.update(uri, values, null, null);

        mSmsTestHelper.assertSmsColumnEquals(Telephony.Sms.BODY, uri, testSmsBodyUpdate);
    }

    @Test
    public void testInsertSmsFromSubid_verifySmsFromNotOtherSubId() {
        int subId = 1;

        ContentValues values = new ContentValues();
        values.put(Telephony.Sms.BODY, TEST_SMS_BODY);
        values.put(Telephony.Sms.SUBSCRIPTION_ID, subId);
        Uri uri = mContentResolver.insert(Telephony.Sms.CONTENT_URI, values);

        assertThat(uri).isNotNull();

        mSmsTestHelper.assertSmsColumnEquals(Telephony.Sms.SUBSCRIPTION_ID, uri,
                String.valueOf(subId));
    }

    @Test
    public void testInsertSms_canUpdateSeen() {
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        Cursor cursor = mContentResolver.query(uri, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);

        final ContentValues updateValues = new ContentValues();
        updateValues.put(Telephony.Sms.SEEN, 1);

        int cursorUpdate = mContentResolver.update(uri, updateValues, null, null);

        assertThat(cursorUpdate).isEqualTo(1);

        mSmsTestHelper.assertSmsColumnEquals(Telephony.Sms.SEEN, uri, String.valueOf(1));
    }

    @Test
    public void testInsertSms_canUpdateSmsStatus() {
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        Cursor cursor = mContentResolver.query(uri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);
        // STATUS_FAILED = 64;  0x40
        mSmsTestHelper.assertUpdateSmsStatus(Telephony.Sms.STATUS_FAILED, uri);
        // STATUS_PENDING = 32;  0x20
        mSmsTestHelper.assertUpdateSmsStatus(Telephony.Sms.STATUS_PENDING, uri);
        //  STATUS_COMPLETE = 0; 0x0
        mSmsTestHelper.assertUpdateSmsStatus(Telephony.Sms.STATUS_COMPLETE, uri);
    }

    @Test
    public void testInsertSms_canUpdateSmsType() {
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        Cursor cursor = mContentResolver.query(uri, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);
        // MESSAGE_TYPE_INBOX = 1;  0x1
        mSmsTestHelper.assertUpdateSmsType(Telephony.Sms.MESSAGE_TYPE_INBOX, uri);
        // MESSAGE_TYPE_SENT = 2;  0x2
        mSmsTestHelper.assertUpdateSmsType(Telephony.Sms.MESSAGE_TYPE_SENT, uri);
    }

    // Queries for a thread ID returns the same and correct thread ID.
    @Test
    public void testQueryThreadId_returnSameThreadId() {
        ContentValues values = new ContentValues();
        values.put(Telephony.Sms.THREAD_ID, TEST_THREAD_ID_1);
        values.put(Telephony.Sms.BODY, TEST_SMS_BODY);
        Uri uri = mContentResolver.insert(Telephony.Sms.CONTENT_URI, values);

        assertThat(uri).isNotNull();

        mSmsTestHelper.assertSmsColumnEquals(Telephony.Sms.THREAD_ID, uri,
                String.valueOf(TEST_THREAD_ID_1));
    }

    /**
     * Asserts that content provider bulk insert and returns the same count while query.
     */
    @Test
    public void testSmsBulkInsert() {
        ContentValues[] smsContentValues = new ContentValues[] {
                mSmsTestHelper.createSmsValues(mSmsTestHelper.SMS_ADDRESS_BODY_1),
                mSmsTestHelper.createSmsValues(mSmsTestHelper.SMS_ADDRESS_BODY_2)};

        int count = mContentResolver.bulkInsert(Telephony.Sms.CONTENT_URI, smsContentValues);
        mSmsTestHelper.assertBulkSmsContentEqual(count, smsContentValues);
    }

    /**
     * Asserts that SMS inserted is auto populated with default values as mentioned in the table
     * schema.
     */
    @Test
    public void testDefaultValuesAreInsertedInSmsTable() {
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        Cursor cursor = mContentResolver.query(Telephony.Sms.CONTENT_URI, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);
        cursor.moveToNext();

        assertThat(
            cursor.getInt(cursor.getColumnIndex(Telephony.Sms.READ))).isEqualTo(0);

        assertThat(
            cursor.getInt(cursor.getColumnIndex(Telephony.Sms.STATUS))).isEqualTo(-1);

        assertThat(
            cursor.getInt(cursor.getColumnIndex(Telephony.Sms.DATE_SENT))).isEqualTo(0);

        assertThat(
            cursor.getInt(cursor.getColumnIndex(Telephony.Sms.LOCKED))).isEqualTo(0);

        assertThat(
            cursor.getInt(cursor.getColumnIndex(Telephony.Sms.SUBSCRIPTION_ID))).isEqualTo(-1);

        assertThat(
            cursor.getInt(cursor.getColumnIndex(Telephony.Sms.ERROR_CODE))).isEqualTo(-1);

        assertThat(
            cursor.getInt(cursor.getColumnIndex(Telephony.Sms.SEEN))).isEqualTo(0);
    }

    @Test
    public void testDeleteSms_ifLastSmsDeletedThenThreadIsDeleted() {
        int testThreadId2 = 102;

        Uri uri1 = mSmsTestHelper
                .insertTestSmsWithThread(TEST_SMS_BODY, TEST_ADDRESS, TEST_THREAD_ID_1);
        assertThat(uri1).isNotNull();

        Uri uri2 = mSmsTestHelper
                .insertTestSmsWithThread(TEST_SMS_BODY, TEST_ADDRESS, testThreadId2);
        assertThat(uri2).isNotNull();

        int deletedRows = mContentResolver.delete(uri1, null, null);
        assertThat(deletedRows).isEqualTo(1);

        Cursor cursor = mContentResolver.query(Telephony.Sms.CONTENT_URI, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);
        cursor.moveToNext();

        int thread_id = cursor.getInt(cursor.getColumnIndex(Telephony.Sms.THREAD_ID));
        assertThat(thread_id).isNotEqualTo(TEST_THREAD_ID_1);
    }

    /**
     * Asserts that a Emoji SMS body returned from an SMS insert operation are equal
     */
    @Test
    public void testInsertEmoji_andVerify() {
        String testSmsBodyEmoji = "\uD83D\uDE0D\uD83D\uDE02"
                + "\uD83D\uDE1B\uD83D\uDE00\uD83D\uDE1E☺️\uD83D\uDE1B"
                + "\uD83D\uDE1E☺️\uD83D\uDE0D";

        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, testSmsBodyEmoji);

        assertThat(uri).isNotNull();

        mSmsTestHelper.assertSmsColumnEquals(Telephony.Sms.BODY, uri,
                String.valueOf(testSmsBodyEmoji));
    }

    /**
     * Verifies that subqueries are not allowed with a restricted view
     */
    @Test
    public void testSubqueryNotAllowed() {
        Uri uri = mSmsTestHelper.insertTestSms(TEST_ADDRESS, TEST_SMS_BODY);
        assertThat(uri).isNotNull();

        DefaultSmsAppHelper.stopBeingDefaultSmsApp();
        {
            // selection
            Cursor cursor1 = mContentResolver.query(Telephony.Sms.CONTENT_URI,
                    null, "seen=(SELECT seen FROM sms LIMIT 1)", null, null);
            assertNull(cursor1);
            Cursor cursor2 = mContentResolver.query(Telephony.MmsSms.CONTENT_CONVERSATIONS_URI,
                    null, "seen=(SELECT seen FROM sms LIMIT 1)", null, null);
            assertNull(cursor2);
        }

        {
            // projection
            Cursor cursor1 = mContentResolver.query(Telephony.Sms.CONTENT_URI,
                    new String[] {"(SELECT seen from sms LIMIT 1) AS d"}, null, null, null);
            assertNull(cursor1);
            Cursor cursor2 = mContentResolver.query(Telephony.MmsSms.CONTENT_CONVERSATIONS_URI,
                    new String[] {"(SELECT seen from sms LIMIT 1) AS d"}, null, null, null);
            assertNull(cursor2);
        }

        {
            // sort order
            Cursor cursor1 = mContentResolver.query(Telephony.Sms.CONTENT_URI,
                    null, null, null,
                    "CASE (SELECT count(seen) FROM sms) WHEN 0 THEN 1 ELSE 0 END DESC");
            assertNull(cursor1);
            Cursor cursor2 = mContentResolver.query(Telephony.MmsSms.CONTENT_CONVERSATIONS_URI,
                    null, null, null,
                    "CASE (SELECT count(seen) FROM sms) WHEN 0 THEN 1 ELSE 0 END DESC");
            assertNull(cursor2);
        }

        DefaultSmsAppHelper.ensureDefaultSmsApp();
    }
}

