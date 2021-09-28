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

import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.CHALLENGE_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_FAILURE_REQUEST;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE_WITH_SPACE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.ERROR_CODE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.FAILURE_REQUEST_EXTRA_ATTRIBUTE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.FAILURE_REQUEST_INVALID_CHALLENGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.FAILURE_REQUEST_INVALID_ERROR_CODE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.FAILURE_REQUEST_INVALID_PASSWORD_CHANGE_PROTOCOL;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.FAILURE_REQUEST_SHORT_CHALLENGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.ID_INT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.MESSAGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.MESSAGE_MISSING_TEXT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.PASSWORD_CHANGE_PROTOCOL;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.RETRY_BIT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_FAILURE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.mschapv2.EapMsChapV2ParsingException;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2FailureRequest;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder.DecodeResult;

import org.junit.Before;
import org.junit.Test;

public class EapMsChapV2FailureRequestTest {
    private static final String TAG = EapMsChapV2FailureRequestTest.class.getSimpleName();

    private EapMsChapV2TypeDataDecoder mTypeDataDecoder;

    @Before
    public void setUp() {
        mTypeDataDecoder = new EapMsChapV2TypeDataDecoder();
    }

    @Test
    public void testDecodeFailureRequest() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(TAG, EAP_MSCHAP_V2_FAILURE_REQUEST);
        assertTrue(result.isSuccessfulDecode());

        EapMsChapV2FailureRequest failureRequest = result.eapTypeData;
        assertEquals(EAP_MSCHAP_V2_FAILURE, failureRequest.opCode);
        assertEquals(ID_INT, failureRequest.msChapV2Id);
        assertEquals(EAP_MSCHAP_V2_FAILURE_REQUEST.length, failureRequest.msLength);
        assertEquals(ERROR_CODE, failureRequest.errorCode);
        assertEquals(RETRY_BIT, failureRequest.isRetryable);
        assertArrayEquals(CHALLENGE_BYTES, failureRequest.challenge);
        assertEquals(PASSWORD_CHANGE_PROTOCOL, failureRequest.passwordChangeProtocol);
        assertEquals(MESSAGE, failureRequest.message);
    }

    @Test
    public void testDecodeFailureRequestMissingMessage() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(
                        TAG, EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE);
        assertTrue(result.isSuccessfulDecode());

        EapMsChapV2FailureRequest failureRequest = result.eapTypeData;
        assertEquals(EAP_MSCHAP_V2_FAILURE, failureRequest.opCode);
        assertEquals(ID_INT, failureRequest.msChapV2Id);
        assertEquals(EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE.length, failureRequest.msLength);
        assertEquals(ERROR_CODE, failureRequest.errorCode);
        assertEquals(RETRY_BIT, failureRequest.isRetryable);
        assertArrayEquals(CHALLENGE_BYTES, failureRequest.challenge);
        assertEquals(PASSWORD_CHANGE_PROTOCOL, failureRequest.passwordChangeProtocol);
        assertEquals(MESSAGE_MISSING_TEXT, failureRequest.message);
    }

    @Test
    public void testDecodeFailureRequestMissingMessageWithSpace() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(
                        TAG, EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE_WITH_SPACE);
        assertTrue(result.isSuccessfulDecode());

        EapMsChapV2FailureRequest failureRequest = result.eapTypeData;
        assertEquals(EAP_MSCHAP_V2_FAILURE, failureRequest.opCode);
        assertEquals(ID_INT, failureRequest.msChapV2Id);
        assertEquals(
                EAP_MSCHAP_V2_FAILURE_REQUEST_MISSING_MESSAGE_WITH_SPACE.length,
                failureRequest.msLength);
        assertEquals(ERROR_CODE, failureRequest.errorCode);
        assertEquals(RETRY_BIT, failureRequest.isRetryable);
        assertArrayEquals(CHALLENGE_BYTES, failureRequest.challenge);
        assertEquals(PASSWORD_CHANGE_PROTOCOL, failureRequest.passwordChangeProtocol);
        assertEquals(MESSAGE_MISSING_TEXT, failureRequest.message);
    }

    @Test
    public void testDecodeFailureRequestInvalidErrorCode() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(TAG, FAILURE_REQUEST_INVALID_ERROR_CODE);
        assertTrue(!result.isSuccessfulDecode());
        assertTrue(result.eapError.cause instanceof NumberFormatException);
    }

    @Test
    public void testDecodeFailureRequestInvalidChallenge() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(TAG, FAILURE_REQUEST_INVALID_CHALLENGE);
        assertTrue(!result.isSuccessfulDecode());
        assertTrue(result.eapError.cause instanceof NumberFormatException);
    }

    @Test
    public void testDecodeFailureRequestShortChallenge() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(TAG, FAILURE_REQUEST_SHORT_CHALLENGE);
        assertTrue(!result.isSuccessfulDecode());
        assertTrue(result.eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testDecodeFailureRequestInvalidPasswordChangeProtocol() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(
                        TAG, FAILURE_REQUEST_INVALID_PASSWORD_CHANGE_PROTOCOL);
        assertTrue(!result.isSuccessfulDecode());
        assertTrue(result.eapError.cause instanceof NumberFormatException);
    }

    @Test
    public void testDecodeFailureExtraAttribute() {
        DecodeResult<EapMsChapV2FailureRequest> result =
                mTypeDataDecoder.decodeFailureRequest(TAG, FAILURE_REQUEST_EXTRA_ATTRIBUTE);
        assertTrue(!result.isSuccessfulDecode());
        assertTrue(result.eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testEncodeFails() throws Exception {
        EapMsChapV2FailureRequest failureRequest =
                new EapMsChapV2FailureRequest(
                        ID_INT,
                        EAP_MSCHAP_V2_FAILURE_REQUEST.length,
                        ERROR_CODE,
                        RETRY_BIT,
                        CHALLENGE_BYTES,
                        PASSWORD_CHANGE_PROTOCOL,
                        MESSAGE);
        try {
            failureRequest.encode();
            fail("Expected UnsupportedOperationException for encoding a request");
        } catch (UnsupportedOperationException expected) {
        }
    }
}
