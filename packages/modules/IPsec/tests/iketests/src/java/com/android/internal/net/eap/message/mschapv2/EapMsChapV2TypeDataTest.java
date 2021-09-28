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

package com.android.internal.net.eap.message.mschapv2;

import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.AUTH_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.AUTH_STRING;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EXTRA_M_MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.MESSAGE_MISSING_TEXT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_DUPLICATE_KEY;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_EXTRA_M;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_INVALID_FORMAT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_MISSING_M;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_CHALLENGE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.exceptions.mschapv2.EapMsChapV2ParsingException;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder.DecodeResult;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2VariableTypeData;

import org.junit.Test;

import java.util.HashMap;
import java.util.Map;

public class EapMsChapV2TypeDataTest {
    private static final int INVALID_OPCODE = -1;
    private static final int MSCHAP_V2_ID = 1;
    private static final int MS_LENGTH = 32;
    private static final String HEX_STRING_INVALID_LENGTH = "00112";
    private static final String HEX_STRING_INVALID_CHARS = "001122z-+x";

    @Test
    public void testEapMsChapV2TypeDataConstructor() throws Exception {
        EapMsChapV2TypeData typeData = new EapMsChapV2TypeData(EAP_MSCHAP_V2_CHALLENGE) {};
        assertEquals(EAP_MSCHAP_V2_CHALLENGE, typeData.opCode);

        try {
            new EapMsChapV2TypeData(INVALID_OPCODE) {};
            fail("ExpectedEapMsChapV2ParsingException for invalid OpCode");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testEapMsChapV2VariableTypeDataConstructor() throws Exception {
        EapMsChapV2VariableTypeData typeData =
                new EapMsChapV2VariableTypeData(
                        EAP_MSCHAP_V2_CHALLENGE, MSCHAP_V2_ID, MS_LENGTH) {};
        assertEquals(EAP_MSCHAP_V2_CHALLENGE, typeData.opCode);
        assertEquals(MSCHAP_V2_ID, typeData.msChapV2Id);
        assertEquals(MS_LENGTH, typeData.msLength);

        try {
            new EapMsChapV2VariableTypeData(INVALID_OPCODE, MSCHAP_V2_ID, MS_LENGTH) {};
            fail("ExpectedEapMsChapV2ParsingException for invalid OpCode");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testDecodeResultIsSuccessfulDecode() throws Exception {
        DecodeResult<EapMsChapV2TypeData> result =
                new DecodeResult(new EapMsChapV2TypeData(EAP_MSCHAP_V2_CHALLENGE) {});
        assertTrue(result.isSuccessfulDecode());

        result = new DecodeResult(new EapError(new Exception()));
        assertFalse(result.isSuccessfulDecode());
    }

    @Test
    public void testGetMessageMappings() throws Exception {
        Map<String, String> expectedMappings = new HashMap<>();
        expectedMappings.put("S", AUTH_STRING);
        expectedMappings.put("M", MESSAGE);
        assertEquals(expectedMappings, EapMsChapV2TypeData.getMessageMappings(SUCCESS_REQUEST));

        expectedMappings = new HashMap<>();
        expectedMappings.put("S", AUTH_STRING);
        expectedMappings.put("M", EXTRA_M_MESSAGE);
        assertEquals(
                expectedMappings, EapMsChapV2TypeData.getMessageMappings(SUCCESS_REQUEST_EXTRA_M));

        expectedMappings = new HashMap<>();
        expectedMappings.put("S", AUTH_STRING);
        expectedMappings.put("M", MESSAGE_MISSING_TEXT);
        assertEquals(
                expectedMappings,
                EapMsChapV2TypeData.getMessageMappings(SUCCESS_REQUEST_MISSING_M));
    }

    @Test
    public void testGetMessageMappingsInvalidFormat() {
        try {
            EapMsChapV2TypeData.getMessageMappings(SUCCESS_REQUEST_INVALID_FORMAT);
            fail("Expected EapMsChapV2ParsingException for extra '='s in message");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testGetMessageMappingDuplicateKey() {
        try {
            EapMsChapV2TypeData.getMessageMappings(SUCCESS_REQUEST_DUPLICATE_KEY);
            fail("Expected EapMsChapV2ParsingException for duplicate key in message");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testHexStringToByteArray() throws Exception {
        byte[] result = EapMsChapV2TypeData.hexStringToByteArray(AUTH_STRING);
        assertArrayEquals(AUTH_BYTES, result);
    }

    @Test
    public void testHexStringToByteArrayInvalidLength() {
        try {
            EapMsChapV2TypeData.hexStringToByteArray(HEX_STRING_INVALID_LENGTH);
            fail("Expected EapMsChapV2ParsingException for invalid hex string length");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testHexStringToByteArrayInvalidChars() throws Exception {
        try {
            EapMsChapV2TypeData.hexStringToByteArray(HEX_STRING_INVALID_CHARS);
            fail("Expected NumberFormatException for invalid hex chars");
        } catch (NumberFormatException expected) {
        }
    }
}
