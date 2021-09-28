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
import android.content.ContentUris;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony;
import android.provider.Telephony.Mms.Draft;
import android.provider.Telephony.Mms.Inbox;
import android.provider.Telephony.Mms.Outbox;
import android.provider.Telephony.Mms.Sent;
import android.provider.Telephony.Sms;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import javax.annotation.Nullable;

@SmallTest
public class MmsTest {

    private static final String[] MMS_ADDRESSES = new String[]{"+1223", "+43234234"};
    private static final String MMS_SUBJECT_ONE = "MMS Subject CTS One";
    private static final String MMS_SUBJECT_TWO = "MMS Subject CTS Two";
    private static final String MMS_BODY = "MMS body CTS";
    private static final String TEXT_PLAIN = "text/plain";

    private ContentResolver mContentResolver;

    @BeforeClass
    public static void ensureDefaultSmsApp() {
        DefaultSmsAppHelper.ensureDefaultSmsApp();
    }

    @AfterClass
    public static void cleanup() {
        ContentResolver contentResolver = getInstrumentation().getContext().getContentResolver();
        contentResolver.delete(Telephony.Mms.CONTENT_URI, null, null);
    }

    @Before
    public void setupTestEnvironment() {
        assumeTelephony();
        cleanup();
        mContentResolver = getInstrumentation().getContext().getContentResolver();
    }

    @Test
    public void testMmsInsert_insertSendReqSucceeds() {
        String expectedSubject = "testMmsInsert_insertSendReqSucceeds";

        Uri uri = insertTestMmsSendReqWithSubject(expectedSubject);

        assertThat(uri).isNotNull();

        Cursor cursor = mContentResolver.query(uri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);

        cursor.moveToNext();
        String actualSubject = cursor.getString(cursor.getColumnIndex(Telephony.Mms.SUBJECT));
        assertThat(actualSubject).isEqualTo(expectedSubject);
    }

    @Test
    public void testMmsDelete() {
        String expectedSubject = "testMmsDelete";

        Uri uri = insertTestMmsSendReqWithSubject(expectedSubject);

        assertThat(uri).isNotNull();

        int deletedRows = mContentResolver.delete(uri, null, null);

        assertThat(deletedRows).isEqualTo(1);

        Cursor cursor = mContentResolver.query(uri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(0);
    }

    @Test
    public void testMmsQuery_canViewNotificationIndMessages() {
        int messageType = PduHeaders.MESSAGE_TYPE_NOTIFICATION_IND;
        String expectedSubject = "testMmsQuery_canViewNotificationIndMessages";

        Uri uri = insertTestMms(expectedSubject, messageType);

        assertThat(uri).isNotNull();

        assertThatMmsInsertSucceeded(uri, expectedSubject);
    }

    @Test
    public void testMmsInsert_canInsertTextMms() {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri).isNotNull();

        final long mmsId = ContentUris.parseId(mmsUri);
        final Uri partUri = Telephony.Mms.Part.getPartUriForMessage(String.valueOf(mmsId));

        Uri insertPartUri = insertIntoMmsPartTable(mmsUri, partUri, mmsId, MMS_BODY);
        assertThat(insertPartUri).isNotNull();

        insertIntoMmsAddressTable(mmsId, MMS_ADDRESSES);

        Cursor mmsCursor = mContentResolver.query(mmsUri, null, null, null);
        assertThat(mmsCursor.getCount()).isEqualTo(1);

        assertThat(mmsCursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(mmsCursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);

        Cursor partCursor = mContentResolver.query(partUri, null, null, null);
        assertThat(partCursor.getCount()).isEqualTo(1);
        assertThat(partCursor.moveToFirst()).isEqualTo(true);
        assertThat(partCursor.getLong(partCursor.getColumnIndex(Telephony.Mms.Part.MSG_ID)))
                .isEqualTo(
                        mmsId);
        assertThat(partCursor.getInt(partCursor.getColumnIndex(Telephony.Mms.Part.SEQ))).isEqualTo(
                0);
        assertThat(partCursor.getString(partCursor.getColumnIndex(Telephony.Mms.Part.CONTENT_TYPE)))
                .isEqualTo(
                        TEXT_PLAIN);
        assertThat(partCursor.getString(partCursor.getColumnIndex(Telephony.Mms.Part.TEXT)))
                .isEqualTo(
                        MMS_BODY);
    }

    @Test
    public void testInsertMessageBoxUri_canSaveAndQueryMmsSent() {
        Uri mmsUri = saveMmsWithMessageBoxUri(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri).isNotNull();

        Uri mmsUri1 = saveMmsWithMessageBoxUri(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri1).isNotNull();

        Cursor msgCursor = mContentResolver.query(Sent.CONTENT_URI, null, null, null,
                Telephony.Mms.DATE_SENT + " ASC");
        assertThat(msgCursor.getCount()).isEqualTo(2);

        assertThat(msgCursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);

        assertThat(msgCursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_SENT);
    }

    @Test
    public void testInsertMessageBoxUri_canSaveAndQueryMmsDraft() {
        Uri mmsUri = saveMmsWithMessageBoxUri(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_DRAFT);
        assertThat(mmsUri).isNotNull();

        Uri mmsUri1 = saveMmsWithMessageBoxUri(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_DRAFT);
        assertThat(mmsUri1).isNotNull();

        Cursor msgCursor = mContentResolver.query(Draft.CONTENT_URI, null, null, null,
                Telephony.Mms.DATE_SENT + " ASC");
        assertThat(msgCursor.getCount()).isEqualTo(2);

        assertThat(msgCursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_DRAFT);

        assertThat(msgCursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_DRAFT);
    }

    @Test
    public void testInsertMessageBoxUri_canSaveAndQueryMmsInbox() {
        Uri mmsUri = saveMmsWithMessageBoxUri(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_INBOX);
        assertThat(mmsUri).isNotNull();

        Uri mmsUri1 = saveMmsWithMessageBoxUri(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_INBOX);
        assertThat(mmsUri1).isNotNull();

        Cursor msgCursor = mContentResolver.query(Inbox.CONTENT_URI, null, null, null,
                Telephony.Mms.DATE_SENT + " ASC");
        assertThat(msgCursor.getCount()).isEqualTo(2);

        assertThat(msgCursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_INBOX);

        assertThat(msgCursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_INBOX);
    }

    @Test
    public void testInsertMessageBoxUri_canSaveAndQueryMmsOutbox() {
        Uri mmsUri = saveMmsWithMessageBoxUri(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_OUTBOX);
        assertThat(mmsUri).isNotNull();

        Uri mmsUri1 = saveMmsWithMessageBoxUri(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_OUTBOX);
        assertThat(mmsUri1).isNotNull();

        Cursor msgCursor = mContentResolver.query(Outbox.CONTENT_URI, null, null, null,
                Telephony.Mms.DATE_SENT + " ASC");
        assertThat(msgCursor.getCount()).isEqualTo(2);

        assertThat(msgCursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_OUTBOX);

        assertThat(msgCursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(msgCursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_OUTBOX);
    }

    @Test
    public void testMmsQuery_retrieveAllWhenSelectorNotSpecified() {
        insertIntoMmsTableWithDifferentMessageBoxUriOfMms();

        Cursor cursor = mContentResolver
                .query(Telephony.Mms.CONTENT_URI, null, null, null,
                        Telephony.Mms.DATE_SENT + " ASC");

        assertThat(cursor.getCount() >= 5).isEqualTo(true);
        assertThat(cursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);

        assertThat(cursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_SENT);

        assertThat(cursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_OUTBOX);

        assertThat(cursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_INBOX);

        assertThat(cursor.moveToNext()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_DRAFT);

        assertThat(cursor.moveToNext()).isEqualTo(false);

    }

    private void assertThatMmsCursorValues(Cursor cursor,
            String subject, int msgType) {
        assertThat(cursor.getString(cursor.getColumnIndex(Telephony.Mms.SUBJECT)))
                .isEqualTo(subject);
        assertThat(cursor.getInt(cursor.getColumnIndex(Telephony.Mms.MESSAGE_TYPE)))
                .isEqualTo(msgType);
    }

    @Test
    public void testMmsQuery_retrieveAllFailedMmsWhenSelectorSpecified() {
        insertIntoMmsTableWithDifferentStatusOfMms();

        String selection = Telephony.Mms.MESSAGE_TYPE + "=" + Sms.MESSAGE_TYPE_FAILED;

        Cursor cursor = mContentResolver
                .query(Telephony.Mms.CONTENT_URI, null, selection, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);
        assertThat(cursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_FAILED);
    }

    @Test
    public void testMmsQuery_retrieveAllSentMmsWhenSelectorSpecified() {
        insertIntoMmsTableWithDifferentStatusOfMms();

        String selection = Telephony.Mms.MESSAGE_TYPE + "=" + Sms.MESSAGE_TYPE_SENT;

        Cursor cursor = mContentResolver
                .query(Telephony.Mms.CONTENT_URI, null, selection, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);
        assertThat(cursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);
    }

    @Test
    public void testMmsQuery_retrieveAllQueuedMmsWhenSelectorSpecified() {
        insertIntoMmsTableWithDifferentStatusOfMms();

        String selection = Telephony.Mms.MESSAGE_TYPE + "=" + Sms.MESSAGE_TYPE_QUEUED;

        Cursor cursor = mContentResolver
                .query(Telephony.Mms.CONTENT_URI, null, selection, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);
        assertThat(cursor.moveToFirst()).isEqualTo(true);
        assertThatMmsCursorValues(cursor, MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_QUEUED);
    }

    @Test
    public void testMmsUpdate() {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri).isNotNull();

        Cursor cursor = mContentResolver.query(mmsUri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);
        assertThat(cursor.moveToFirst()).isEqualTo(true);
        assertThat(cursor.getString(cursor.getColumnIndex(Telephony.Mms.SUBJECT)))
                .isEqualTo(MMS_SUBJECT_ONE);

        final ContentValues updateValues = new ContentValues();
        updateValues.put(Telephony.Mms.SUBJECT, MMS_SUBJECT_TWO);

        int cursorUpdate = mContentResolver.update(mmsUri, updateValues, null, null);
        assertThat(cursorUpdate).isEqualTo(1);

        Cursor cursorAfterUpdate = mContentResolver.query(mmsUri, null, null, null);

        assertThat(cursorAfterUpdate.getCount()).isEqualTo(1);
        assertThat(cursorAfterUpdate.moveToFirst()).isEqualTo(true);
        assertThat(
                cursorAfterUpdate.getString(
                        cursorAfterUpdate.getColumnIndex(Telephony.Mms.SUBJECT)))
                .isEqualTo(MMS_SUBJECT_TWO);

    }

    @Test
    public void testMmsUpdate_canUpdateReadAndSeenStatus() {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri).isNotNull();

        Cursor cursor = mContentResolver.query(mmsUri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);

        final ContentValues updateValues = new ContentValues();
        updateValues.put(Telephony.Mms.READ, 1);
        updateValues.put(Telephony.Mms.SEEN, 1);

        int cursorUpdate = mContentResolver.update(mmsUri, updateValues, null, null);
        assertThat(cursorUpdate).isEqualTo(1);

        Cursor cursorAfterReadUpdate = mContentResolver.query(mmsUri, null, null, null);

        assertThat(cursorAfterReadUpdate.getCount()).isEqualTo(1);
        assertThat(cursorAfterReadUpdate.moveToFirst()).isEqualTo(true);
        assertThat(
                cursorAfterReadUpdate.getInt(
                        cursorAfterReadUpdate.getColumnIndex(Telephony.Mms.READ)))
                .isEqualTo(
                        1);
        assertThat(
                cursorAfterReadUpdate.getInt(
                        cursorAfterReadUpdate.getColumnIndex(Telephony.Mms.SEEN)))
                .isEqualTo(
                        1);
    }

    private void insertIntoMmsTableWithDifferentMessageBoxUriOfMms() {
        Uri mmsUri1 = saveMmsWithMessageBoxUri(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri1).isNotNull();
        Uri mmsUri2 = saveMmsWithMessageBoxUri(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri2).isNotNull();
        Uri mmsUri3 = saveMmsWithMessageBoxUri(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_OUTBOX);
        assertThat(mmsUri3).isNotNull();
        Uri mmsUri4 = saveMmsWithMessageBoxUri(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_INBOX);
        assertThat(mmsUri4).isNotNull();
        Uri mmsUri15 = saveMmsWithMessageBoxUri(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_DRAFT);
        assertThat(mmsUri15).isNotNull();
    }


    private void insertIntoMmsTableWithDifferentStatusOfMms() {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE, Sms.MESSAGE_TYPE_SENT);
        assertThat(mmsUri).isNotNull();
        mmsUri = insertIntoMmsTable(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_INBOX);
        assertThat(mmsUri).isNotNull();
        mmsUri = insertIntoMmsTable(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_OUTBOX);
        assertThat(mmsUri).isNotNull();
        mmsUri = insertIntoMmsTable(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_ALL);
        assertThat(mmsUri).isNotNull();
        mmsUri = insertIntoMmsTable(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_FAILED);
        assertThat(mmsUri).isNotNull();
        mmsUri = insertIntoMmsTable(MMS_SUBJECT_TWO, Sms.MESSAGE_TYPE_QUEUED);
        assertThat(mmsUri).isNotNull();
    }

    private Uri insertIntoMmsTable(String subject, int messageType) {
        final ContentValues mmsValues = new ContentValues();
        mmsValues.put(Telephony.Mms.TEXT_ONLY, 1);
        mmsValues.put(Telephony.Mms.MESSAGE_TYPE, messageType);
        mmsValues.put(Telephony.Mms.SUBJECT, subject);
        final Uri mmsUri = mContentResolver.insert(Telephony.Mms.CONTENT_URI, mmsValues);
        return mmsUri;
    }

    private Uri saveMmsWithMessageBoxUri(String subject, int messageType) {
        Uri mmsUri = Telephony.Mms.CONTENT_URI;

        final ContentValues mmsValues = new ContentValues();
        mmsValues.put(Telephony.Mms.TEXT_ONLY, 1);
        mmsValues.put(Telephony.Mms.MESSAGE_TYPE, messageType);
        mmsValues.put(Telephony.Mms.SUBJECT, subject);
        mmsValues.put(Telephony.Mms.DATE_SENT, System.currentTimeMillis());

        switch (messageType) {
            case Sms.MESSAGE_TYPE_SENT:
                mmsUri = Sent.CONTENT_URI;
                break;
            case Sms.MESSAGE_TYPE_INBOX:
                mmsUri = Inbox.CONTENT_URI;
                break;
            case Sms.MESSAGE_TYPE_DRAFT:
                mmsUri = Draft.CONTENT_URI;
                break;
            case Sms.MESSAGE_TYPE_OUTBOX:
                mmsUri = Outbox.CONTENT_URI;
                break;
            default:
                mmsUri = Telephony.Mms.CONTENT_URI;
                break;
        }

        final Uri msgUri = mContentResolver.insert(mmsUri, mmsValues);
        return msgUri;
    }

    private Uri insertIntoMmsPartTable(Uri mmsUri, Uri partUri, long mmsId, String body) {
        final String srcName = String.format("text.%06d.txt", 0);
        // Insert body part.
        final ContentValues values = new ContentValues();
        values.put(Telephony.Mms.Part.MSG_ID, mmsId);
        values.put(Telephony.Mms.Part.SEQ, 0);
        values.put(Telephony.Mms.Part.CONTENT_TYPE, TEXT_PLAIN);
        values.put(Telephony.Mms.Part.NAME, srcName);
        values.put(Telephony.Mms.Part.CONTENT_ID, "<" + srcName + ">");
        values.put(Telephony.Mms.Part.CONTENT_LOCATION, srcName);
        values.put(Telephony.Mms.Part.CHARSET, 111);
        values.put(Telephony.Mms.Part.TEXT, body);
        Uri insertPartUri = mContentResolver.insert(partUri, values);
        return insertPartUri;
    }

    private void insertIntoMmsAddressTable(long mmsId, String[] addresses) {
        final Uri addrUri = Telephony.Mms.Addr.getAddrUriForMessage(String.valueOf(mmsId));
        for (int i = 0; i < addresses.length; ++i) {
            ContentValues addrValues = new ContentValues();
            addrValues.put(Telephony.Mms.Addr.TYPE, i);
            addrValues.put(Telephony.Mms.Addr.CHARSET, i + 1);
            addrValues.put(Telephony.Mms.Addr.ADDRESS, addresses[i]);
            addrValues.put(Telephony.Mms.Addr.MSG_ID, mmsId);
            Uri insertAddressUri = mContentResolver.insert(addrUri, addrValues);
            assertThat(insertAddressUri).isNotNull();
        }
    }

    /**
     * Asserts that a URI returned from an MMS insert operation represents a failed insert.
     *
     * When an insert fails, the resulting URI could be in several states. In many cases the
     * resulting URI will be null. However, if an insert fails due to appops privileges it will
     * return a dummy URI. This URI should either point to no rows, or to a single row. If it does
     * point to a row, it the subject should not match the subject of the attempted insert.
     *
     * In normal circumstances, the environment should be clean before the test, so as long as our
     * subjects are unique, we should not have a false test failure. However, if the environment is
     * not clean, we could lead to a false test failure if the returned dummy URI subject happens to
     * match the subject of our attempted insert.
     */
    private void assertThatMmsInsertFailed(@Nullable Uri uriReturnedFromInsert,
            String subjectOfAttemptedInsert) {
        if (uriReturnedFromInsert == null) {
            return;
        }
        Cursor cursor = mContentResolver.query(uriReturnedFromInsert, null, null, null);
        if (cursor.getCount() == 0) {
            return; // insert failed, so test passes
        }
        assertThat(cursor.getCount()).isEqualTo(1);
        cursor.moveToNext();
        assertThat(cursor.getString(cursor.getColumnIndex(Telephony.Mms.SUBJECT))).isNotEqualTo(
                subjectOfAttemptedInsert);
    }

    private void assertThatMmsInsertSucceeded(@Nullable Uri uriReturnedFromInsert,
            String subjectOfAttemptedInsert) {
        assertThat(uriReturnedFromInsert).isNotNull();

        Cursor cursor = mContentResolver.query(uriReturnedFromInsert, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);

        cursor.moveToNext();
        String actualSubject = cursor.getString(cursor.getColumnIndex(Telephony.Mms.SUBJECT));
        assertThat(actualSubject).isEqualTo(subjectOfAttemptedInsert);
    }

    /**
     * @return the URI returned from the insert operation
     */
    private Uri insertTestMmsSendReqWithSubject(String subject) {
        return insertTestMms(subject, PduHeaders.MESSAGE_TYPE_SEND_REQ);
    }

    private Uri insertTestMms(String subject, int messageType) {
        Uri uri = Telephony.Mms.CONTENT_URI;
        ContentValues values = new ContentValues();
        values.put(Telephony.Mms.SUBJECT, subject);
        values.put(Telephony.Mms.MESSAGE_TYPE, messageType);
        return mContentResolver.insert(uri, values);
    }
}

