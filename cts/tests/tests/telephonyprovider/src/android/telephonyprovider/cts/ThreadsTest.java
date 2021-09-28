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
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

@SmallTest
public class ThreadsTest {

    private static final String DESTINATION = "+19998880001";
    private static final String MESSAGE_BODY = "MESSAGE_BODY";
    private static final int THREAD_ID = 101;

    private Context mContext;
    private ContentResolver mContentResolver;

    @BeforeClass
    public static void ensureDefaultSmsApp() {
        DefaultSmsAppHelper.ensureDefaultSmsApp();
    }

    @AfterClass
    public static void cleanup() {
        ContentResolver contentResolver = getInstrumentation().getContext().getContentResolver();
        contentResolver.delete(Telephony.Threads.CONTENT_URI, null, null);
        contentResolver.delete(Telephony.Sms.CONTENT_URI, null, null);
    }

    @Before
    public void setupTestEnvironment() {
        assumeTelephony();
        cleanup();
        mContext = getInstrumentation().getContext();
        mContentResolver = mContext.getContentResolver();
    }

    @Test
    public void testThreadDeletion_doNotReuseThreadIdsFromEmptyThreads() {
        String destination2 = "+19998880002";

        long threadId1 = Telephony.Threads.getOrCreateThreadId(mContext, DESTINATION);

        int deletedCount =
                mContentResolver.delete(
                        saveToTelephony(threadId1, destination2, "testThreadDeletion body"),
                        null,
                        null);

        assertThat(deletedCount).isEqualTo(1);

        long threadId2 = Telephony.Threads.getOrCreateThreadId(mContext, destination2);

        assertThat(threadId2).isGreaterThan(threadId1);
    }

    // This purpose of this test case is to return latest date inserted as sms from thread
    @Test
    public void testMultipleSmsInsertDate_returnsLatestDateFromThread() {
        final long earlierTime = 1557382640;

        Uri smsUri = addMessageToTelephonyWithDate(earlierTime, MESSAGE_BODY, THREAD_ID);
        assertThat(smsUri).isNotNull();

        assertVerifyThreadDate(earlierTime);

        final long laterTime = 1557382650;

        Uri smsUri2 = addMessageToTelephonyWithDate(laterTime, MESSAGE_BODY,
                THREAD_ID);
        assertThat(smsUri2).isNotNull();

        assertVerifyThreadDate(laterTime);
    }

    private Uri addMessageToTelephonyWithDate(long date, String messageBody, long threadId) {
        ContentValues contentValues = new ContentValues();

        contentValues.put(Telephony.Sms.THREAD_ID, threadId);
        contentValues.put(Telephony.Sms.DATE, date);
        contentValues.put(Telephony.Sms.BODY, messageBody);

        return mContext.getContentResolver().insert(Telephony.Sms.CONTENT_URI, contentValues);
    }

    private void assertVerifyThreadDate(long timeStamp) {
        Cursor cursor = mContentResolver.query(Telephony.Threads.CONTENT_URI,
                null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);
        assertThat(cursor.moveToNext()).isEqualTo(true);

        assertThat(cursor.getLong(cursor.getColumnIndex(Telephony.Threads.DATE)))
            .isEqualTo(timeStamp);
    }

    private Uri saveToTelephony(long threadId, String address, String body) {
        ContentValues contentValues = new ContentValues();
        contentValues.put(Telephony.Sms.THREAD_ID, threadId);
        contentValues.put(Telephony.Sms.ADDRESS, address);
        contentValues.put(Telephony.Sms.BODY, body);

        return mContext.getContentResolver().insert(Telephony.Sms.Inbox.CONTENT_URI, contentValues);
    }
}
