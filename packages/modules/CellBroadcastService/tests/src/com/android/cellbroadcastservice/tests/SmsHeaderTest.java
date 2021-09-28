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

package com.android.cellbroadcastservice.tests;

import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper;

import com.android.cellbroadcastservice.SmsHeader;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

@RunWith(AndroidTestingRunner.class)
@TestableLooper.RunWithLooper
public class SmsHeaderTest extends CellBroadcastServiceTestBase {

    private SmsHeader mSmsHeader;

    @Before
    public void setUp() throws Exception {
        super.setUp();
        mSmsHeader = new SmsHeader();

        // construct port addresses
        SmsHeader.PortAddrs portAddresses = new SmsHeader.PortAddrs();
        portAddresses.areEightBits = true;
        portAddresses.destPort = 80;
        portAddresses.origPort = 80;

        // construct concatenated reference
        SmsHeader.ConcatRef concatRef = new SmsHeader.ConcatRef();
        concatRef.isEightBits = true;
        concatRef.msgCount = 0x03;
        concatRef.refNumber = 0x02;
        concatRef.seqNumber = 0x01;

        // construct special SMS message
        SmsHeader.SpecialSmsMsg specialSmsMessage1 = new SmsHeader.SpecialSmsMsg();
        specialSmsMessage1.msgIndType = 0x01;
        specialSmsMessage1.msgCount = 0x02;

        // construct misc elt
        SmsHeader.MiscElt miscElement1 = new SmsHeader.MiscElt();
        miscElement1.id = 0x26; // must be != to a defined elt id
        miscElement1.data = new byte[]{0x00, 0x01, 0x02, 0x03, 0x04};

        mSmsHeader.portAddrs = portAddresses;
        mSmsHeader.concatRef = concatRef;
        mSmsHeader.specialSmsMsgList = new ArrayList<>();
        mSmsHeader.specialSmsMsgList.add(specialSmsMessage1);
        mSmsHeader.miscEltList = new ArrayList<>();
        mSmsHeader.miscEltList.add(miscElement1);
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    private void assertEquals(SmsHeader a, SmsHeader b) {
        assertTrue(a.equals(b));
    }

    @Test
    public void testConvertToByteArray() {
        assertNotNull(mSmsHeader);

        // convert to byte array and back
        SmsHeader convertedSmsHeader = SmsHeader.fromByteArray(SmsHeader.toByteArray(mSmsHeader));
        assertEquals(mSmsHeader, convertedSmsHeader);
    }

    /**
     * Convert to byte array and back when ConcatRef and PortAddrs are 16 bit
     */
    @Test
    public void testConvertToByteArray16bits() {
        mSmsHeader.concatRef.isEightBits = false;
        mSmsHeader.portAddrs.areEightBits = false;
        assertNotNull(mSmsHeader);

        // convert to byte array and back
        SmsHeader convertedSmsHeader = SmsHeader.fromByteArray(SmsHeader.toByteArray(mSmsHeader));
        assertEquals(mSmsHeader, convertedSmsHeader);
    }

    /**
     * Convert to byte array and back with language table != 0
     */
    @Test
    public void testConvertToByteArrayLanguageTable() {
        mSmsHeader.languageShiftTable = 0x24;
        mSmsHeader.languageTable = 0x25;
        assertNotNull(mSmsHeader);

        // convert to byte array and back
        SmsHeader convertedSmsHeader = SmsHeader.fromByteArray(SmsHeader.toByteArray(mSmsHeader));
        assertEquals(mSmsHeader, convertedSmsHeader);
    }
}
