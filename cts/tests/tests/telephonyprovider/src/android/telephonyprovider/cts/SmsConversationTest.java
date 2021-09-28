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
import android.provider.Telephony.Sms.Conversations;

import androidx.test.filters.SmallTest;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

@SmallTest
public class SmsConversationTest {

    private static final String TEST_ADDRESS = "+19998880001";
    private static final String TEST_SMS_BODY = "TEST_SMS_BODY";
    private Context mContext;
    private ContentResolver mContentResolver;

    @BeforeClass
    public static void ensureDefaultSmsApp() {
        DefaultSmsAppHelper.ensureDefaultSmsApp();
    }

    @AfterClass
    public static void cleanup() {
        ContentResolver contentResolver = getInstrumentation().getContext().getContentResolver();
        contentResolver.delete(Telephony.Sms.CONTENT_URI, null, null);
    }

    @Before
    public void setupTestEnvironment() {
        assumeTelephony();
        cleanup();
        mContentResolver = getInstrumentation().getContext().getContentResolver();
    }

    /**
     * The purpose of this test is to check most recent insert sms body equals to the Conversation
     * Snippet
     */
    @Test
    public void testQueryConversation_snippetEqualsMostRecentMessageBody() {
        String testSmsMostRecent = "TEST_SMS_MOST_RECENT";

        saveToTelephony(TEST_SMS_BODY, TEST_ADDRESS);
        saveToTelephony(testSmsMostRecent, TEST_ADDRESS);

        Cursor cursor = mContentResolver
                .query(Telephony.Sms.CONTENT_URI, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(2);

        Cursor conversationCursor = mContentResolver
                .query(Conversations.CONTENT_URI, null, null, null);
        assertThat(conversationCursor.getCount()).isEqualTo(1);
        conversationCursor.moveToNext();

        assertThat(
            conversationCursor.getString(conversationCursor.getColumnIndex(Conversations.SNIPPET)))
            .isEqualTo(testSmsMostRecent);
    }

    /**
     * The purpose of this test is to check Conversation message count is equal to the number of sms
     * inserted.
     */
    @Test
    public void testQueryConversation_returnsCorrectMessageCount() {
        String testSecondSmsBody = "TEST_SECOND_SMS_BODY";

        saveToTelephony(TEST_SMS_BODY, TEST_ADDRESS);
        saveToTelephony(testSecondSmsBody, TEST_ADDRESS);

        Cursor cursor = mContentResolver
                .query(Telephony.Sms.CONTENT_URI, null, null, null);
        assertThat(cursor.getCount()).isEqualTo(2);

        Cursor conversationCursor = mContentResolver
                .query(Conversations.CONTENT_URI, null, null, null);
        assertThat(conversationCursor.getCount()).isEqualTo(1);
        conversationCursor.moveToNext();

        assertThat(conversationCursor
            .getInt(conversationCursor.getColumnIndex(Conversations.MESSAGE_COUNT)))
            .isEqualTo(2);
    }

    public void saveToTelephony(String messageBody, String address) {
        ContentValues values = new ContentValues();
        values.put(Telephony.Sms.BODY, messageBody);
        values.put(Telephony.Sms.ADDRESS, address);
        Uri uri1 = mContentResolver.insert(Telephony.Sms.CONTENT_URI, values);
    }
}

