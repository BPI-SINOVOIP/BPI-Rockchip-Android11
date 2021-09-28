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

package com.android.internal.net.utils;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class LogTest {
    private static final String TAG = "IkeLogTest";
    private static final String PII = "123456789ABCDEF"; // "IMSI"
    private static final String HEX_STRING = "00112233445566778899AABBCCDDEEFF";
    private static final byte[] HEX_BYTES = new byte[] {
            (byte) 0x00, (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x44, (byte) 0x55,
            (byte) 0x66, (byte) 0x77, (byte) 0x88, (byte) 0x99, (byte) 0xAA, (byte) 0xBB,
            (byte) 0xCC, (byte) 0xDD, (byte) 0xEE, (byte) 0xFF
    };

    @Test
    public void testPii() {
        // Log(String tag, boolean isEngBuild, boolean logSensitive);
        String result = new Log(TAG, false, false).pii(PII);
        assertEquals(Integer.toString(PII.hashCode()), result);

        result = new Log(TAG,  true, false).pii(PII);
        assertEquals(Integer.toString(PII.hashCode()), result);

        result = new Log(TAG,  false, true).pii(PII);
        assertEquals(Integer.toString(PII.hashCode()), result);

        result = new Log(TAG,  true, true).pii(PII);
        assertEquals(PII, result);

        result = new Log(TAG, true, true).pii(HEX_BYTES);
        assertEquals(HEX_STRING, result);
    }

    @Test
    public void testByteArrayToHexString() {
        assertEquals("", Log.byteArrayToHexString(null));

        assertEquals("", Log.byteArrayToHexString(new byte[0]));

        assertEquals(HEX_STRING, Log.byteArrayToHexString(HEX_BYTES));
    }
}
