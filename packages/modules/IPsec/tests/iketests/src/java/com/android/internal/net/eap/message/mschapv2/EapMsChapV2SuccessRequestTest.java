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
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_SUCCESS_REQUEST;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_SUCCESS_REQUEST_EMPTY_MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE_WITH_SPACE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.ID_INT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.MESSAGE_MISSING_TEXT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_EXTRA_ATTRIBUTE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_INVALID_AUTH_STRING;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_SHORT_AUTH_STRING;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_WRONG_OP_CODE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SUCCESS_REQUEST_WRONG_PREFIX;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_SUCCESS;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.exceptions.mschapv2.EapMsChapV2ParsingException;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2SuccessRequest;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder.DecodeResult;

import org.junit.Before;
import org.junit.Test;

import java.nio.BufferUnderflowException;

public class EapMsChapV2SuccessRequestTest {
    private static final String TAG = EapMsChapV2SuccessRequestTest.class.getSimpleName();

    private EapMsChapV2TypeDataDecoder mTypeDataDecoder;

    @Before
    public void setUp() {
        mTypeDataDecoder = new EapMsChapV2TypeDataDecoder();
    }

    @Test
    public void testDecodeSuccessRequest() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(TAG, EAP_MSCHAP_V2_SUCCESS_REQUEST);
        assertTrue(result.isSuccessfulDecode());

        EapMsChapV2SuccessRequest successRequest = result.eapTypeData;
        assertEquals(EAP_MSCHAP_V2_SUCCESS, successRequest.opCode);
        assertEquals(ID_INT, successRequest.msChapV2Id);
        assertEquals(EAP_MSCHAP_V2_SUCCESS_REQUEST.length, successRequest.msLength);
        assertArrayEquals(AUTH_BYTES, successRequest.authBytes);
        assertEquals(MESSAGE, successRequest.message);
    }

    @Test
    public void testDecodeSuccessRequestEmptyMessage() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(
                        TAG, EAP_MSCHAP_V2_SUCCESS_REQUEST_EMPTY_MESSAGE);
        assertTrue(result.isSuccessfulDecode());

        EapMsChapV2SuccessRequest successRequest = result.eapTypeData;
        assertEquals(EAP_MSCHAP_V2_SUCCESS, successRequest.opCode);
        assertEquals(ID_INT, successRequest.msChapV2Id);
        assertEquals(EAP_MSCHAP_V2_SUCCESS_REQUEST_EMPTY_MESSAGE.length, successRequest.msLength);
        assertArrayEquals(AUTH_BYTES, successRequest.authBytes);
        assertTrue(successRequest.message.isEmpty());
    }

    @Test
    public void testDecodeSuccessRequestMissingMessage() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(
                        TAG, EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE);
        assertTrue(result.isSuccessfulDecode());

        EapMsChapV2SuccessRequest successRequest = result.eapTypeData;
        assertEquals(EAP_MSCHAP_V2_SUCCESS, successRequest.opCode);
        assertEquals(ID_INT, successRequest.msChapV2Id);
        assertEquals(EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE.length, successRequest.msLength);
        assertArrayEquals(AUTH_BYTES, successRequest.authBytes);
        assertEquals(MESSAGE_MISSING_TEXT, successRequest.message);
    }

    @Test
    public void testDecodeSuccessRequestMissingMessageWithSpace() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(
                        TAG, EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE_WITH_SPACE);
        assertTrue(result.isSuccessfulDecode());

        EapMsChapV2SuccessRequest successRequest = result.eapTypeData;
        assertEquals(EAP_MSCHAP_V2_SUCCESS, successRequest.opCode);
        assertEquals(ID_INT, successRequest.msChapV2Id);
        assertEquals(
                EAP_MSCHAP_V2_SUCCESS_REQUEST_MISSING_MESSAGE_WITH_SPACE.length,
                successRequest.msLength);
        assertArrayEquals(AUTH_BYTES, successRequest.authBytes);
        assertEquals(MESSAGE_MISSING_TEXT, successRequest.message);
    }

    @Test
    public void testDecodeSuccessRequestWrongOpCode() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(TAG, SUCCESS_REQUEST_WRONG_OP_CODE);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testDecodeSuccessRequestShortMessage() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(TAG, new byte[0]);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof BufferUnderflowException);
    }

    @Test
    public void testDecodeSuccessRequestInvalidPrefix() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(TAG, SUCCESS_REQUEST_WRONG_PREFIX);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testDecodeSuccessRequestShortAuthString() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(TAG, SUCCESS_REQUEST_SHORT_AUTH_STRING);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testDecodeSuccessRequestInvalidAuthString() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(TAG, SUCCESS_REQUEST_INVALID_AUTH_STRING);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof NumberFormatException);
    }

    @Test
    public void testDecodeSuccessRequestExtraAttribute() {
        DecodeResult<EapMsChapV2SuccessRequest> result =
                mTypeDataDecoder.decodeSuccessRequest(TAG, SUCCESS_REQUEST_EXTRA_ATTRIBUTE);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testEncodeFails() throws Exception {
        EapMsChapV2SuccessRequest successRequest =
                new EapMsChapV2SuccessRequest(
                        ID_INT, EAP_MSCHAP_V2_SUCCESS_REQUEST.length, AUTH_BYTES, MESSAGE);
        try {
            successRequest.encode();
            fail("Expected UnsupportedOperationException for encoding a request");
        } catch (UnsupportedOperationException expected) {
        }
    }
}
