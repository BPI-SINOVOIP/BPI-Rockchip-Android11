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

package com.android.internal.net.eap.message;

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapData.EAP_IDENTITY;
import static com.android.internal.net.eap.message.EapData.EAP_NAK;
import static com.android.internal.net.eap.message.EapData.EAP_TYPE_SIM;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_REQUEST;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_RESPONSE;
import static com.android.internal.net.eap.message.EapMessage.EAP_CODE_SUCCESS;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_IDENTITY_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_IDENTITY_PACKET_TOO_LONG;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_SIM_START_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_REQUEST_SIM_TYPE_DATA;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NAK_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_RESPONSE_NOTIFICATION_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SUCCESS_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.ID_INT;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.INCOMPLETE_HEADER_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.INVALID_CODE_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.LONG_SUCCESS_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.REQUEST_MISSING_TYPE_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.REQUEST_UNSUPPORTED_TYPE_PACKET;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SHORT_PACKET;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.EapResult;
import com.android.internal.net.eap.EapResult.EapResponse;
import com.android.internal.net.eap.exceptions.EapInvalidPacketLengthException;
import com.android.internal.net.eap.exceptions.InvalidEapCodeException;
import com.android.internal.net.eap.exceptions.UnsupportedEapTypeException;

import org.junit.Test;

import java.util.Arrays;

public class EapMessageTest {
    @Test
    public void testConstructorRequestWithoutType() throws Exception {
        try {
            new EapMessage(EAP_CODE_REQUEST, ID_INT, null);
            fail("Expected EapInvalidPacketLengthException for an EAP-Request without Type value");
        } catch (EapInvalidPacketLengthException expected) {
        }
    }

    @Test
    public void testDecode() throws Exception {
        EapMessage result = EapMessage.decode(EAP_SUCCESS_PACKET);
        assertEquals(EAP_CODE_SUCCESS, result.eapCode);
        assertEquals(ID_INT, result.eapIdentifier);
        assertEquals(EAP_SUCCESS_PACKET.length, result.eapLength);
        assertNull(result.eapData);

        EapData expectedEapData = new EapData(EAP_TYPE_SIM,
                hexStringToByteArray(EAP_REQUEST_SIM_TYPE_DATA));
        result = EapMessage.decode(EAP_REQUEST_SIM_START_PACKET);
        assertEquals(EAP_CODE_REQUEST, result.eapCode);
        assertEquals(ID_INT, result.eapIdentifier);
        assertEquals(EAP_REQUEST_SIM_START_PACKET.length, result.eapLength);
        assertEquals(expectedEapData, result.eapData);
    }

    @Test
    public void testDecodeInvalidCode() throws Exception {
        try {
            EapMessage.decode(INVALID_CODE_PACKET);
            fail("Expected InvalidEapCodeException");
        } catch (InvalidEapCodeException expected) {
        }
    }

    @Test
    public void testDecodeIncompleteHeader() throws Exception {
        try {
            EapMessage.decode(INCOMPLETE_HEADER_PACKET);
            fail("Expected EapInvalidPacketLengthException");
        } catch (EapInvalidPacketLengthException expected) {
        }
    }

    @Test
    public void testDecodeShortPacket() throws Exception {
        try {
            EapMessage.decode(SHORT_PACKET);
            fail("Expected EapInvalidPacketLengthException");
        } catch (EapInvalidPacketLengthException expected) {
        }
    }

    @Test
    public void testDecodeSuccessIncorrectLength() throws Exception {
        try {
            EapMessage.decode(LONG_SUCCESS_PACKET);
            fail("Expected EapInvalidPacketLengthException");
        } catch (EapInvalidPacketLengthException expected) {
        }
    }

    @Test
    public void testDecodeMissingTypeData() throws Exception {
        try {
            EapMessage.decode(REQUEST_MISSING_TYPE_PACKET);
            fail("Expected EapInvalidPacketLengthException");
        } catch (EapInvalidPacketLengthException expected) {
        }
    }

    @Test
    public void testDecodeUnsupportedEapType() throws Exception {
        try {
            EapMessage.decode(REQUEST_UNSUPPORTED_TYPE_PACKET);
            fail("Expected UnsupportedEapDataTypeException");
        } catch (UnsupportedEapTypeException expected) {
            assertEquals(ID_INT, expected.eapIdentifier);
        }
    }

    @Test
    public void testEncode() throws Exception {
        EapMessage eapMessage = new EapMessage(EAP_CODE_SUCCESS, ID_INT, null);
        byte[] actualPacket = eapMessage.encode();
        assertArrayEquals(EAP_SUCCESS_PACKET, actualPacket);

        EapData nakData = new EapData(EAP_NAK, new byte[] {EAP_TYPE_SIM});
        eapMessage = new EapMessage(EAP_CODE_RESPONSE, ID_INT, nakData);
        actualPacket = eapMessage.encode();
        assertArrayEquals(EAP_RESPONSE_NAK_PACKET, actualPacket);
    }

    @Test
    public void testEncodeDecode() throws Exception {
        EapMessage eapMessage = new EapMessage(EAP_CODE_SUCCESS, ID_INT, null);
        EapMessage result = EapMessage.decode(eapMessage.encode());

        assertEquals(eapMessage.eapCode, result.eapCode);
        assertEquals(eapMessage.eapIdentifier, result.eapIdentifier);
        assertEquals(eapMessage.eapLength, result.eapLength);
        assertEquals(eapMessage.eapData, result.eapData);
    }

    @Test
    public void testDecodeEncode() throws Exception {
        byte[] result = EapMessage.decode(EAP_REQUEST_SIM_START_PACKET).encode();
        assertArrayEquals(EAP_REQUEST_SIM_START_PACKET, result);
    }

    @Test
    public void testGetNakResponse() {
        EapResult nakResponse = EapMessage.getNakResponse(ID_INT, Arrays.asList(EAP_TYPE_SIM));

        assertTrue(nakResponse instanceof EapResponse);
        EapResponse eapResponse = (EapResponse) nakResponse;
        assertArrayEquals(EAP_RESPONSE_NAK_PACKET, eapResponse.packet);
    }

    @Test
    public void testGetNotificationResponse() {
        EapResult notificationResponse = EapMessage.getNotificationResponse(ID_INT);

        assertTrue(notificationResponse instanceof EapResponse);
        EapResponse eapResponse = (EapResponse) notificationResponse;
        assertArrayEquals(EAP_RESPONSE_NOTIFICATION_PACKET, eapResponse.packet);
    }

    @Test
    public void testDecodewithExtraBytes() throws Exception {
        EapMessage result = EapMessage.decode(EAP_REQUEST_IDENTITY_PACKET_TOO_LONG);
        EapData eapData = new EapData(EAP_IDENTITY, new byte[0]);

        assertEquals(EAP_CODE_REQUEST, result.eapCode);
        assertEquals(ID_INT, result.eapIdentifier);
        assertEquals(EAP_REQUEST_IDENTITY_PACKET.length, result.eapLength);
        assertEquals(eapData, result.eapData);
    }
}
