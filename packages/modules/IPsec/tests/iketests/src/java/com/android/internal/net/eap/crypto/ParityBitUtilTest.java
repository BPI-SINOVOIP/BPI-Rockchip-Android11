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

package com.android.internal.net.eap.crypto;

import static com.android.internal.net.TestUtils.hexStringToByteArray;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import org.junit.Test;

/**
 * Inputs taken from RFC 2759 Section 9.3.
 *
 * @see <a href="https://tools.ietf.org/html/rfc2759#section-9.3">RFC 2759 Section 9.3, Examples of
 *     DES Key Generation</a>
 */
public class ParityBitUtilTest {
    private static final byte[] INPUT_BYTES = {(byte) 0xFC, (byte) 0x0E, (byte) 0x7E, (byte) 0x37};
    private static final byte[] OUTPUT_BYTES = {(byte) 0xFD, (byte) 0x0E, (byte) 0x7F, (byte) 0x37};

    private static final String[] RAW_KEYS = {"FC156AF7EDCD6C", "0EDDE3337D427F"};
    private static final long[] RAW_KEY_LONGS = {0xFC156AF7EDCD6CL, 0xEDDE3337D427FL};
    private static final String[] PARITY_CORRECTED_KEYS = {"FD0B5B5E7F6E34D9", "0E6E796737EA08FE"};

    @Test
    public void testGetByteWithParityBit() {
        for (int i = 0; i < INPUT_BYTES.length; i++) {
            assertEquals(OUTPUT_BYTES[i], ParityBitUtil.getByteWithParityBit(INPUT_BYTES[i]));
        }
    }

    @Test
    public void testByteArrayToLong() {
        for (int i = 0; i < RAW_KEYS.length; i++) {
            byte[] rawKey = hexStringToByteArray(RAW_KEYS[i]);

            assertEquals(RAW_KEY_LONGS[i], ParityBitUtil.byteArrayToLong(rawKey));
        }
    }

    @Test
    public void testAddParityBits() {
        for (int i = 0; i < RAW_KEYS.length; i++) {
            byte[] rawKey = hexStringToByteArray(RAW_KEYS[i]);
            byte[] parityCorrectedKey = hexStringToByteArray(PARITY_CORRECTED_KEYS[i]);

            assertArrayEquals(parityCorrectedKey, ParityBitUtil.addParityBits(rawKey));
        }
    }
}
