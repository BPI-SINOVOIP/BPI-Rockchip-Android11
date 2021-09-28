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
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.CHALLENGE_REQUEST_LONG_MS_LENGTH;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.CHALLENGE_REQUEST_SHORT_CHALLENGE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.CHALLENGE_REQUEST_SHORT_MS_LENGTH;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.CHALLENGE_REQUEST_WRONG_OP_CODE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_CHALLENGE_REQUEST;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.ID_INT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SERVER_NAME_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_CHALLENGE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.EapResult.EapError;
import com.android.internal.net.eap.exceptions.mschapv2.EapMsChapV2ParsingException;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2ChallengeRequest;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2TypeDataDecoder.DecodeResult;

import org.junit.Before;
import org.junit.Test;

import java.nio.BufferUnderflowException;

public class EapMsChapV2ChallengeRequestTest {
    private static final String TAG = EapMsChapV2ChallengeRequestTest.class.getSimpleName();

    private EapMsChapV2TypeDataDecoder mTypeDataDecoder;

    @Before
    public void setUp() {
        mTypeDataDecoder = new EapMsChapV2TypeDataDecoder();
    }

    @Test
    public void testDecodeChallengeRequest() {
        DecodeResult<EapMsChapV2ChallengeRequest> result =
                mTypeDataDecoder.decodeChallengeRequest(TAG, EAP_MSCHAP_V2_CHALLENGE_REQUEST);
        assertTrue(result.isSuccessfulDecode());
        EapMsChapV2ChallengeRequest challengeRequest = result.eapTypeData;

        assertEquals(EAP_MSCHAP_V2_CHALLENGE, challengeRequest.opCode);
        assertEquals(ID_INT, challengeRequest.msChapV2Id);
        assertEquals(EAP_MSCHAP_V2_CHALLENGE_REQUEST.length, challengeRequest.msLength);
        assertArrayEquals(CHALLENGE_BYTES, challengeRequest.challenge);
        assertArrayEquals(SERVER_NAME_BYTES, challengeRequest.name);
    }

    @Test
    public void testDecodeChallengeRequestWrongOpCode() {
        DecodeResult<EapMsChapV2ChallengeRequest> result =
                mTypeDataDecoder.decodeChallengeRequest(TAG, CHALLENGE_REQUEST_WRONG_OP_CODE);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testDecodeChallengeRequestShortChallenge() {
        DecodeResult<EapMsChapV2ChallengeRequest> result =
                mTypeDataDecoder.decodeChallengeRequest(TAG, CHALLENGE_REQUEST_SHORT_CHALLENGE);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testDecodeChallengeRequestShortMsLength() {
        DecodeResult<EapMsChapV2ChallengeRequest> result =
                mTypeDataDecoder.decodeChallengeRequest(TAG, CHALLENGE_REQUEST_SHORT_MS_LENGTH);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof EapMsChapV2ParsingException);
    }

    @Test
    public void testDecodeChallengeRequestLongMsLength() {
        DecodeResult<EapMsChapV2ChallengeRequest> result =
                mTypeDataDecoder.decodeChallengeRequest(TAG, CHALLENGE_REQUEST_LONG_MS_LENGTH);
        assertFalse(result.isSuccessfulDecode());
        EapError eapError = result.eapError;
        assertTrue(eapError.cause instanceof BufferUnderflowException);
    }

    @Test
    public void testEncodeChallengeRequestFails() throws Exception {
        EapMsChapV2ChallengeRequest challengeRequest =
                new EapMsChapV2ChallengeRequest(
                        ID_INT,
                        EAP_MSCHAP_V2_CHALLENGE_REQUEST.length,
                        CHALLENGE_BYTES,
                        SERVER_NAME_BYTES);
        try {
            challengeRequest.encode();
            fail("Expected UnsupportedOperationException for encoding a Challenge Request");
        } catch (UnsupportedOperationException expected) {
        }
    }
}
