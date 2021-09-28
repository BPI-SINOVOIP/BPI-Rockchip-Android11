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

package com.android.bluetooth.mapclient;

import static org.mockito.Mockito.*;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class BmessageTest {
    private static final String TAG = BmessageTest.class.getSimpleName();
    private static final String SIMPLE_MMS_MESSAGE =
            "BEGIN:BMSG\r\nVERSION:1.0\r\nSTATUS:READ\r\nTYPE:MMS\r\nFOLDER:null\r\nBEGIN:BENV\r\n"
            + "BEGIN:VCARD\r\nVERSION:2.1\r\nN:null;;;;\r\nTEL:555-5555\r\nEND:VCARD\r\n"
            + "BEGIN:BBODY\r\nLENGTH:39\r\nBEGIN:MSG\r\nThis is a new msg\r\nEND:MSG\r\n"
            + "END:BBODY\r\nEND:BENV\r\nEND:BMSG\r\n";

    private static final String NO_END_MESSAGE =
            "BEGIN:BMSG\r\nVERSION:1.0\r\nSTATUS:READ\r\nTYPE:MMS\r\nFOLDER:null\r\nBEGIN:BENV\r\n"
            + "BEGIN:VCARD\r\nVERSION:2.1\r\nN:null;;;;\r\nTEL:555-5555\r\nEND:VCARD\r\n"
            + "BEGIN:BBODY\r\nLENGTH:39\r\nBEGIN:MSG\r\nThis is a new msg\r\n";

    private static final String WRONG_LENGTH_MESSAGE =
            "BEGIN:BMSG\r\nVERSION:1.0\r\nSTATUS:READ\r\nTYPE:MMS\r\nFOLDER:null\r\nBEGIN:BENV\r\n"
            + "BEGIN:VCARD\r\nVERSION:2.1\r\nN:null;;;;\r\nTEL:555-5555\r\nEND:VCARD\r\n"
            + "BEGIN:BBODY\r\nLENGTH:200\r\nBEGIN:MSG\r\nThis is a new msg\r\nEND:MSG\r\n"
            + "END:BBODY\r\nEND:BENV\r\nEND:BMSG\r\n";

    private static final String NO_BODY_MESSAGE =
            "BEGIN:BMSG\r\nVERSION:1.0\r\nSTATUS:READ\r\nTYPE:MMS\r\nFOLDER:null\r\nBEGIN:BENV\r\n"
            + "BEGIN:VCARD\r\nVERSION:2.1\r\nN:null;;;;\r\nTEL:555-5555\r\nEND:VCARD\r\n"
            + "BEGIN:BBODY\r\nLENGTH:\r\n";

    private static final String NEGATIVE_LENGTH_MESSAGE =
            "BEGIN:BMSG\r\nVERSION:1.0\r\nSTATUS:READ\r\nTYPE:MMS\r\nFOLDER:null\r\nBEGIN:BENV\r\n"
            + "BEGIN:VCARD\r\nVERSION:2.1\r\nN:null;;;;\r\nTEL:555-5555\r\nEND:VCARD\r\n"
            + "BEGIN:BBODY\r\nLENGTH:-1\r\nBEGIN:MSG\r\nThis is a new msg\r\nEND:MSG\r\n"
            + "END:BBODY\r\nEND:BENV\r\nEND:BMSG\r\n";

    @Test
    public void testNormalMessages() {
        Bmessage message = BmessageParser.createBmessage(SIMPLE_MMS_MESSAGE);
        Assert.assertNotNull(message);
    }

    @Test
    public void testParseWrongLengthMessage() {
        Bmessage message = BmessageParser.createBmessage(WRONG_LENGTH_MESSAGE);
        Assert.assertNull(message);
    }

    @Test
    public void testParseNoEndMessage() {
        Bmessage message = BmessageParser.createBmessage(NO_END_MESSAGE);
        Assert.assertNull(message);
    }

    @Test
    public void testParseReallyLongMessage() {
        String testMessage = new String(new char[68048]).replace('\0', 'A');
        Bmessage message = BmessageParser.createBmessage(testMessage);
        Assert.assertNull(message);
    }

    @Test
    public void testNoBodyMessage() {
        Bmessage message = BmessageParser.createBmessage(NO_BODY_MESSAGE);
        Assert.assertNull(message);
    }

    @Test
    public void testNegativeLengthMessage() {
        Bmessage message = BmessageParser.createBmessage(NEGATIVE_LENGTH_MESSAGE);
        Assert.assertNull(message);
    }
}
