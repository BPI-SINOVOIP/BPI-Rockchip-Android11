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

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import javax.annotation.Nullable;

public class MmsPartTest {

    private static final String MMS_SUBJECT_ONE = "MMS Subject CTS One";
    private static final String MMS_BODY = "MMS body CTS";
    private static final String MMS_BODY_UPDATE = "MMS body CTS Update";
    private static final String TEXT_PLAIN = "text/plain";
    private static final String SRC_NAME = String.format("text.%06d.txt", 0);
    /**
     * Parts must be inserted in relation to a message, this message ID is used for inserting a part
     * when the message ID is not important in relation to the current test.
     */
    private static final String TEST_MESSAGE_ID = "100";
    private ContentResolver mContentResolver;

    @BeforeClass
    public static void ensureDefaultSmsApp() {
        DefaultSmsAppHelper.ensureDefaultSmsApp();
    }

    @AfterClass
    public static void cleanup() {
        ContentResolver contentResolver = getInstrumentation().getContext().getContentResolver();
        contentResolver.delete(Telephony.Mms.Part.CONTENT_URI, null, null);
    }

    @Before
    public void setupTestEnvironment() {
        assumeTelephony();
        cleanup();
        mContentResolver = getInstrumentation().getContext().getContentResolver();
    }

    @Test
    public void testMmsPartInsert_cannotInsertPartWithDataColumn() {
        ContentValues values = new ContentValues();
        values.put(Telephony.Mms.Part._DATA, "/dev/urandom");
        values.put(Telephony.Mms.Part.NAME, "testMmsPartInsert_cannotInsertPartWithDataColumn");

        Uri uri = insertTestMmsPartWithValues(values);
        assertThat(uri).isNull();
    }

    @Test
    public void testMmsPartInsert_canInsertPartWithoutDataColumn() {
        String name = "testMmsInsert_canInsertPartWithoutDataColumn";

        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE);
        assertThat(mmsUri).isNotNull();
        final long mmsId = ContentUris.parseId(mmsUri);

        //Creating part uri using mmsId.
        final Uri partUri = Telephony.Mms.Part.getPartUriForMessage(String.valueOf(mmsId));
        Uri insertPartUri = insertIntoMmsPartTable(mmsUri, partUri, mmsId, MMS_BODY, name);
        assertThatMmsPartInsertSucceeded(insertPartUri, name, MMS_BODY);
    }

    @Test
    public void testMmsPart_deletedPartIdsAreNotReused() {
        long id1 = insertAndVerifyMmsPartReturningId("testMmsPart_deletedPartIdsAreNotReused_1");

        deletePartById(id1);

        long id2 = insertAndVerifyMmsPartReturningId("testMmsPart_deletedPartIdsAreNotReused_2");

        assertThat(id2).isGreaterThan(id1);
    }

    @Test
    public void testMmsPartUpdate() {
        //Inserting data to MMS table.
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE);
        assertThat(mmsUri).isNotNull();
        final long mmsId = ContentUris.parseId(mmsUri);

        //Creating part uri using mmsId.
        final Uri partUri = Telephony.Mms.Part.getPartUriForMessage(String.valueOf(mmsId));

        //Inserting data to MmsPart table with mapping with mms uri.
        Uri insertPartUri = insertIntoMmsPartTable(mmsUri, partUri, mmsId, MMS_BODY, SRC_NAME);
        assertThatMmsPartInsertSucceeded(insertPartUri, SRC_NAME, MMS_BODY);

        final ContentValues updateValues = new ContentValues();
        updateValues.put(Telephony.Mms.Part.TEXT, MMS_BODY_UPDATE);

        // Updating part table.
        int cursorUpdate = mContentResolver.update(partUri, updateValues, null, null);
        assertThat(cursorUpdate).isEqualTo(1);
        assertThatMmsPartInsertSucceeded(insertPartUri, SRC_NAME, MMS_BODY_UPDATE);

    }

    @Test
    public void testMmsPartDelete_canDeleteById() {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE);
        assertThat(mmsUri).isNotNull();
        final long mmsId = ContentUris.parseId(mmsUri);
        final Uri partUri = Telephony.Mms.Part.getPartUriForMessage(String.valueOf(mmsId));

        Uri insertPartUri = insertIntoMmsPartTable(mmsUri, partUri, mmsId, MMS_BODY, SRC_NAME);
        assertThat(insertPartUri).isNotNull();

        int deletedRows = mContentResolver.delete(partUri, null, null);

        assertThat(deletedRows).isEqualTo(1);

    }

    private long insertAndVerifyMmsPartReturningId(String name) {
        Uri mmsUri = insertIntoMmsTable(MMS_SUBJECT_ONE);
        assertThat(mmsUri).isNotNull();
        final long mmsId = ContentUris.parseId(mmsUri);

        //Creating part uri using mmsId.
        final Uri partUri = Telephony.Mms.Part.getPartUriForMessage(String.valueOf(mmsId));
        Uri insertPartUri = insertIntoMmsPartTable(mmsUri, partUri, mmsId, MMS_BODY, name);

        assertThatMmsPartInsertSucceeded(insertPartUri, name, MMS_BODY);
        return Long.parseLong(insertPartUri.getLastPathSegment());
    }

    private void deletePartById(long partId) {
        Uri uri = Uri.withAppendedPath(Telephony.Mms.Part.CONTENT_URI, Long.toString(partId));
        int deletedRows = mContentResolver.delete(uri, null, null);
        assertThat(deletedRows).isEqualTo(1);
    }

    private Uri insertTestMmsPartWithValues(ContentValues values) {
        Uri insertUri = Telephony.Mms.Part.getPartUriForMessage(TEST_MESSAGE_ID);

        Uri uri = mContentResolver.insert(insertUri, values);
        return uri;
    }

    private void assertThatMmsPartInsertSucceeded(@Nullable Uri uriReturnedFromInsert,
            String nameOfAttemptedInsert, String textBody) {
        assertThat(uriReturnedFromInsert).isNotNull();

        Cursor cursor = mContentResolver.query(uriReturnedFromInsert, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(1);

        cursor.moveToNext();
        String actualName = cursor.getString(cursor.getColumnIndex(Telephony.Mms.Part.NAME));
        assertThat(actualName).isEqualTo(nameOfAttemptedInsert);
        assertThat(cursor.getString(cursor.getColumnIndex(Telephony.Mms.Part.TEXT))).isEqualTo(
                textBody);
    }

    private Uri insertIntoMmsTable(String subject) {
        final ContentValues mmsValues = new ContentValues();
        mmsValues.put(Telephony.Mms.TEXT_ONLY, 1);
        mmsValues.put(Telephony.Mms.MESSAGE_TYPE, 128);
        mmsValues.put(Telephony.Mms.SUBJECT, subject);
        final Uri mmsUri = mContentResolver.insert(Telephony.Mms.CONTENT_URI, mmsValues);
        return mmsUri;
    }

    private Uri insertIntoMmsPartTable(Uri mmsUri, Uri partUri, long mmsId, String body,
            String name) {
        // Insert body part.
        final ContentValues values = new ContentValues();
        values.put(Telephony.Mms.Part.MSG_ID, mmsId);
        values.put(Telephony.Mms.Part.NAME, name);
        values.put(Telephony.Mms.Part.SEQ, 0);
        values.put(Telephony.Mms.Part.CONTENT_TYPE, TEXT_PLAIN);
        values.put(Telephony.Mms.Part.CONTENT_ID, "<" + SRC_NAME + ">");
        values.put(Telephony.Mms.Part.CONTENT_LOCATION, SRC_NAME);
        values.put(Telephony.Mms.Part.CHARSET, 111);
        values.put(Telephony.Mms.Part.TEXT, body);
        Uri insertPartUri = mContentResolver.insert(partUri, values);
        return insertPartUri;
    }
}
