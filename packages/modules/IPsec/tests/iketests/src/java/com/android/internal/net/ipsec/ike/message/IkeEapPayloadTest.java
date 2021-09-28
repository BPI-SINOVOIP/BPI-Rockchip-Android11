/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.internal.net.ipsec.ike.message;

import static com.android.internal.net.TestUtils.hexStringToByteArray;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.nio.ByteBuffer;

public class IkeEapPayloadTest {
    private static final String EAP_SUCCESS_STRING = "03010004";
    private static final byte[] EAP_SUCCESS_PACKET = hexStringToByteArray(EAP_SUCCESS_STRING);

    private static final byte[] IKE_EAP_PAYLOAD =
            hexStringToByteArray(
                    "00000008" + EAP_SUCCESS_STRING);

    @Test
    public void testDecodeIkeEapPayload() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(IKE_EAP_PAYLOAD);
        IkePayload result = IkePayloadFactory
                .getIkePayload(IkePayload.PAYLOAD_TYPE_EAP, true, input).first;

        assertTrue(result instanceof IkeEapPayload);
        IkeEapPayload ikeEapPayload = (IkeEapPayload) result;
        assertArrayEquals(EAP_SUCCESS_PACKET, ikeEapPayload.eapMessage);
    }

    @Test
    public void testEncodeToByteBuffer() {
        IkeEapPayload ikeEapPayload = new IkeEapPayload(EAP_SUCCESS_PACKET);
        ByteBuffer result = ByteBuffer.allocate(IKE_EAP_PAYLOAD.length);

        ikeEapPayload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_NO_NEXT, result);
        assertArrayEquals(IKE_EAP_PAYLOAD, result.array());
    }

    @Test
    public void testOutboundConstructorForIke() {
        IkeEapPayload ikeEapPayload = new IkeEapPayload(EAP_SUCCESS_PACKET);
        assertArrayEquals(EAP_SUCCESS_PACKET, ikeEapPayload.eapMessage);
    }

    @Test
    public void testGetPayloadLength() {
        IkeEapPayload ikeEapPayload = new IkeEapPayload(EAP_SUCCESS_PACKET);
        assertEquals(IKE_EAP_PAYLOAD.length, ikeEapPayload.getPayloadLength());
    }
}
