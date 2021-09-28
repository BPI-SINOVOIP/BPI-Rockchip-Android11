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

package com.android.internal.net.ipsec.ike.message;

import static com.android.internal.net.ipsec.ike.message.IkeCertPayload.CERTIFICATE_ENCODING_X509_CERT_SIGNATURE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import android.util.Pair;

import com.android.internal.net.TestUtils;

import org.junit.Test;

import java.nio.ByteBuffer;

public class IkeCertReqPayloadTest {
    private static final int CERT_ENCODING_TYPE = CERTIFICATE_ENCODING_X509_CERT_SIGNATURE;
    private static final int NEXT_PAYLOAD_TYPE = IkePayload.PAYLOAD_TYPE_AUTH;
    private static final byte[] CERT_REQ_PAYLOAD =
            TestUtils.hexStringToByteArray("27000019040d0a12bb1f98996563f15b10db95c67eea7990fa");
    private static final byte[] CA_SUBJECT_PUBLIC_KEY_INFO_HASH =
            TestUtils.hexStringToByteArray("0d0a12bb1f98996563f15b10db95c67eea7990fa");

    @Test
    public void testDecode() throws Exception {
        Pair<IkePayload, Integer> pair =
                IkePayloadFactory.getIkePayload(
                        IkePayload.PAYLOAD_TYPE_CERT_REQUEST,
                        false /*isResp*/,
                        ByteBuffer.wrap(CERT_REQ_PAYLOAD));

        IkeCertReqPayload certPayload = (IkeCertReqPayload) pair.first;
        assertEquals(CERT_ENCODING_TYPE, certPayload.certEncodingType);
        assertArrayEquals(
                CA_SUBJECT_PUBLIC_KEY_INFO_HASH, certPayload.caSubjectPublicKeyInforHashes);

        assertEquals(NEXT_PAYLOAD_TYPE, (int) pair.second);
    }

    @Test
    public void testEncode() throws Exception {
        Pair<IkePayload, Integer> pair =
                IkePayloadFactory.getIkePayload(
                        IkePayload.PAYLOAD_TYPE_CERT_REQUEST,
                        false /*isResp*/,
                        ByteBuffer.wrap(CERT_REQ_PAYLOAD));
        IkeCertReqPayload certPayload = (IkeCertReqPayload) pair.first;

        ByteBuffer byteBuffer = ByteBuffer.allocate(CERT_REQ_PAYLOAD.length);
        certPayload.encodeToByteBuffer(NEXT_PAYLOAD_TYPE, byteBuffer);
        assertArrayEquals(CERT_REQ_PAYLOAD, byteBuffer.array());

        assertEquals(CERT_REQ_PAYLOAD.length, certPayload.getPayloadLength());
    }
}
