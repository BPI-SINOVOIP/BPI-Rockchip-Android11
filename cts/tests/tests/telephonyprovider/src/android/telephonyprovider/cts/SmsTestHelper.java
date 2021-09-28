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

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.google.common.truth.Truth.assertThat;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.provider.Telephony;

class SmsTestHelper {

    private ContentResolver mContentResolver;
    private ContentValues mContentValues;
    public static final String SMS_ADDRESS_BODY_1 = "sms CTS text 1",
            SMS_ADDRESS_BODY_2 = "sms CTS text 2";
    SmsTestHelper() {
        mContentResolver = getInstrumentation().getContext().getContentResolver();
        mContentValues = new ContentValues();
    }

    public Uri insertTestSms(String testAddress, String testSmsBody) {
        mContentValues.put(Telephony.Sms.ADDRESS, testAddress);
        mContentValues.put(Telephony.Sms.BODY, testSmsBody);
        return mContentResolver.insert(Telephony.Sms.CONTENT_URI, mContentValues);
    }

    public ContentValues createSmsValues(String messageBody) {
        ContentValues smsRow = new ContentValues();
        smsRow.put(Telephony.Sms.BODY, messageBody);
        return smsRow;
    }

    public Uri insertTestSmsWithThread(String testAddress, String testSmsBody, int threadId) {
        mContentValues.put(Telephony.Sms.ADDRESS, testAddress);
        mContentValues.put(Telephony.Sms.BODY, testSmsBody);
        mContentValues.put(Telephony.Sms.THREAD_ID, threadId);

        return mContentResolver.insert(Telephony.Sms.CONTENT_URI, mContentValues);
    }
    /**
     * Asserts the verify message details
     */
    public void assertSmsColumnEquals(String columnName, Uri uri, String expectedValue) {
        Cursor cursor = mContentResolver.query(uri, null, null, null);

        assertThat(cursor.getCount()).isEqualTo(1);
        assertThat(cursor.moveToNext()).isEqualTo(true);

        assertThat(cursor.getString(cursor.getColumnIndex(columnName))).isEqualTo(expectedValue);
    }

    public void assertBulkSmsContentEqual(int count, ContentValues[] smsContentValues) {
        Cursor cursor = mContentResolver.query(Telephony.Sms.CONTENT_URI, null, null, null);

        assertThat(count).isEqualTo(smsContentValues.length);
        assertThat(cursor.getCount()).isEqualTo(smsContentValues.length);

        assertThat(cursor.moveToNext()).isEqualTo(true);
        assertThat(cursor.getString(cursor.getColumnIndex(Telephony.Sms.BODY)))
                .isEqualTo(SMS_ADDRESS_BODY_2);

        assertThat(cursor.moveToNext()).isEqualTo(true);
        assertThat(cursor.getString(cursor.getColumnIndex(Telephony.Sms.BODY)))
                .isEqualTo(SMS_ADDRESS_BODY_1);

    }
    public void assertUpdateSmsStatus(int status, Uri uri) {
        ContentValues mContentValues = new ContentValues();
        mContentValues.put(Telephony.Sms.STATUS, status);

        int cursorUpdate = mContentResolver.update(uri, mContentValues, null, null);
        assertThat(cursorUpdate).isEqualTo(1);

        Cursor cursorStatus = mContentResolver.query(uri, null, null, null);
        assertThat(cursorStatus.getCount()).isEqualTo(1);
        cursorStatus.moveToNext();

        int statusResult = cursorStatus.getInt(cursorStatus.getColumnIndex(Telephony.Sms.STATUS));
        assertThat(statusResult).isAnyOf(0, 32, 64);
        assertThat(statusResult).isEqualTo(status);
    }

    public void assertUpdateSmsType(int type, Uri uri) {
        mContentValues.put(Telephony.Sms.TYPE, type);

        int cursorUpdate = mContentResolver.update(uri, mContentValues, null, null);
        assertThat(cursorUpdate).isEqualTo(1);

        Cursor cursorStatus = mContentResolver.query(uri, null, null, null);
        assertThat(cursorStatus.getCount()).isEqualTo(1);
        cursorStatus.moveToNext();

        int typeResult = cursorStatus.getInt(cursorStatus.getColumnIndex(Telephony.Sms.TYPE));
        assertThat(typeResult).isAnyOf(1, 2);
        assertThat(typeResult).isEqualTo(type);
    }
}

