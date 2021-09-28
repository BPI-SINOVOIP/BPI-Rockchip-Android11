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

import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.EAP_MSCHAP_V2_CHALLENGE_RESPONSE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.ID_INT;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.NT_RESPONSE_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.PEER_CHALLENGE_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.PEER_NAME_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SHORT_CHALLENGE_BYTES;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2PacketDefinitions.SHORT_NT_RESPONSE;
import static com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EAP_MSCHAP_V2_RESPONSE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.mschapv2.EapMsChapV2ParsingException;
import com.android.internal.net.eap.message.mschapv2.EapMsChapV2TypeData.EapMsChapV2ChallengeResponse;

import org.junit.Test;

public class EapMsChapV2ChallengeResponseTest {
    private static final int FLAGS = 0;
    private static final int INVALID_FLAGS = 0xFF;

    @Test
    public void testConstructor() throws Exception {
        EapMsChapV2ChallengeResponse challengeResponse =
                new EapMsChapV2ChallengeResponse(
                        ID_INT, PEER_CHALLENGE_BYTES, NT_RESPONSE_BYTES, FLAGS, PEER_NAME_BYTES);
        assertEquals(EAP_MSCHAP_V2_RESPONSE, challengeResponse.opCode);
        assertEquals(ID_INT, challengeResponse.msChapV2Id);
        assertEquals(EAP_MSCHAP_V2_CHALLENGE_RESPONSE.length, challengeResponse.msLength);
        assertArrayEquals(PEER_CHALLENGE_BYTES, challengeResponse.peerChallenge);
        assertArrayEquals(NT_RESPONSE_BYTES, challengeResponse.ntResponse);
        assertEquals(FLAGS, challengeResponse.flags);
        assertArrayEquals(PEER_NAME_BYTES, challengeResponse.name);
    }

    @Test
    public void testConstructorInvalidChallenge() {
        try {
            EapMsChapV2ChallengeResponse challengeResponse =
                    new EapMsChapV2ChallengeResponse(
                            ID_INT,
                            SHORT_CHALLENGE_BYTES,
                            NT_RESPONSE_BYTES,
                            FLAGS,
                            PEER_NAME_BYTES);
            fail("Expected EapMsChapV2ParsingException for invalid Peer Challenge length");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testConstructorInvalidNtResponse() {
        try {
            EapMsChapV2ChallengeResponse challengeResponse =
                    new EapMsChapV2ChallengeResponse(
                            ID_INT,
                            PEER_CHALLENGE_BYTES,
                            SHORT_NT_RESPONSE,
                            FLAGS,
                            PEER_NAME_BYTES);
            fail("Expected EapMsChapV2ParsingException for invalid NT-Response length");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testConstructorInvalidFlags() {
        try {
            EapMsChapV2ChallengeResponse challengeResponse =
                    new EapMsChapV2ChallengeResponse(
                            ID_INT,
                            PEER_CHALLENGE_BYTES,
                            NT_RESPONSE_BYTES,
                            INVALID_FLAGS,
                            PEER_NAME_BYTES);
            fail("Expected EapMsChapV2ParsingException for non-zero Flags value");
        } catch (EapMsChapV2ParsingException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        EapMsChapV2ChallengeResponse challengeResponse =
                new EapMsChapV2ChallengeResponse(
                        ID_INT, PEER_CHALLENGE_BYTES, NT_RESPONSE_BYTES, FLAGS, PEER_NAME_BYTES);
        byte[] encodedChallengeResponse = challengeResponse.encode();

        assertArrayEquals(EAP_MSCHAP_V2_CHALLENGE_RESPONSE, encodedChallengeResponse);
    }
}
