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

package com.android.internal.net.ipsec.ike.message;

import static com.android.internal.net.ipsec.ike.message.IkeAuthDigitalSignPayload.SIGNATURE_ALGO_RSA_SHA2_256;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.ipsec.ike.SaProposal;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.crypto.IkeMacPrf;
import com.android.internal.net.ipsec.ike.exceptions.AuthenticationFailedException;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.PrfTransform;
import com.android.internal.net.ipsec.ike.testutils.CertUtils;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;

public final class IkeAuthDigitalSignPayloadTest {
    // TODO: Build a RSA_SHA1 signature and add tests for it.

    // AuthMethod 14 (Generic DS) with RSA_SHA2_256
    private static final String AUTH_PAYLOAD_BODY_GENERIC_DIGITAL_SIGN_HEX_STRING =
            "0e0000000f300d06092a864886f70d01010b05006f76af4150d653c5d4136b9f"
                    + "69d905849bf075c563e6d14ccda42361ec3e7d12c72e2dece5711ea1d952f7b8"
                    + "e12c5d982aa4efdaeac36a02b222aa96242cc424";

    private static final String GENERIC_PAYLOAD_HEADER = "00000058"; // 88B length

    private static final String SIGNATURE =
            "6f76af4150d653c5d4136b9f69d905849bf075c563e6d14ccda42361ec3e7d12"
                    + "c72e2dece5711ea1d952f7b8e12c5d982aa4efdaeac36a02b222aa96242cc424";

    private static final String IKE_INIT_RESP_HEX_STRING =
            "02458497587b09d488d5b76480bce53d2120222000000000000001cc2200002c"
                    + "00000028010100040300000801000003030000080300000203000008020000020"
                    + "00000080400000e28000108000e000013d60e51c40922cb121e395bacbd627cdd"
                    + "d3240baa4fcefd29f65f8dd37329d68d4fb4854f8b8f07cfb60900e276d99a396"
                    + "1112ee866b5456cf588dc1092fd3bc19668fb8fa42872f51c0ee748bdb665dcbe"
                    + "15ac454f6ed966149954dac5187638d1ab61869d97a4873c4733c48cbe3acc8a6"
                    + "5cfea3ce83fd09fba174bf0ec56d73a0585859399e61c2c38e695841f8df8a511"
                    + "aadd438f56634165ad9b88e858c1585f1bee646943b8a96f5397721079a127b87"
                    + "fd286e8f869ae021ce82adf91fa360217ac32268b39b698bf06a4e89b8d0267af"
                    + "1c5b979b6493adb10a0e14aa707309e914b8d377903e75cb13cffbfde9c26842f"
                    + "b49a07a4497c9907d39515b290000244b8aed6297c09a5a0dda06c873f5573b34"
                    + "886dd779e90c19beca3fc54ab3cae02900001c00004004d8e7cb9d1e689ae8c84"
                    + "c5078355436f3347376ff2900001c0000400545bc3f2113770de91c769094f1bd"
                    + "614534e765ea290000080000402e290000100000402f000100020003000400000"
                    + "00800004014";

    private static final int NEXT_PAYLOAD_TYPE = IkePayload.PAYLOAD_TYPE_NO_NEXT;
    private static final String NONCE_INIT_HEX_STRING =
            "a5dded450b5ffd2670f37954367fce28279a085c830a03358b10b0872c0578f9";
    private static final String ID_RESP_PAYLOAD_BODY_HEX_STRING = "01000000c0a82b8a";
    private static final String SKP_RESP_HEX_STRING = "8FE8EC3153EDE924C23D6630D3C992A494E2F256";

    private static final byte[] IKE_INIT_RESP_REQUEST =
            TestUtils.hexStringToByteArray(IKE_INIT_RESP_HEX_STRING);
    private static final byte[] NONCE_INIT_RESP =
            TestUtils.hexStringToByteArray(NONCE_INIT_HEX_STRING);
    private static final byte[] ID_RESP_PAYLOAD_BODY =
            TestUtils.hexStringToByteArray(ID_RESP_PAYLOAD_BODY_HEX_STRING);
    private static final byte[] PRF_RESP_KEY = TestUtils.hexStringToByteArray(SKP_RESP_HEX_STRING);

    private IkeMacPrf mIkeHmacSha1Prf;

    @Before
    public void setUp() throws Exception {
        mIkeHmacSha1Prf =
                IkeMacPrf.create(new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1));
    }

    @Test
    public void testDecodeGenericDigitalSignPayload() throws Exception {
        byte[] inputPacket =
                TestUtils.hexStringToByteArray(AUTH_PAYLOAD_BODY_GENERIC_DIGITAL_SIGN_HEX_STRING);
        IkeAuthPayload payload = IkeAuthPayload.getIkeAuthPayload(false, inputPacket);

        assertTrue(payload instanceof IkeAuthDigitalSignPayload);
        IkeAuthDigitalSignPayload dsPayload = (IkeAuthDigitalSignPayload) payload;
        assertEquals(SIGNATURE_ALGO_RSA_SHA2_256, dsPayload.signatureAndHashAlgos);
        assertArrayEquals(dsPayload.signature, TestUtils.hexStringToByteArray(SIGNATURE));
    }

    @Test
    public void testSignAndEncode() throws Exception {
        PrivateKey key = CertUtils.createRsaPrivateKeyFromKeyFile("end-cert-key-a.key");

        IkeAuthDigitalSignPayload authPayload =
                new IkeAuthDigitalSignPayload(
                        SIGNATURE_ALGO_RSA_SHA2_256,
                        key,
                        IKE_INIT_RESP_REQUEST,
                        NONCE_INIT_RESP,
                        ID_RESP_PAYLOAD_BODY,
                        mIkeHmacSha1Prf,
                        PRF_RESP_KEY);

        ByteBuffer buffer = ByteBuffer.allocate(authPayload.getPayloadLength());
        authPayload.encodeToByteBuffer(NEXT_PAYLOAD_TYPE, buffer);

        byte[] expected =
                TestUtils.hexStringToByteArray(
                        GENERIC_PAYLOAD_HEADER + AUTH_PAYLOAD_BODY_GENERIC_DIGITAL_SIGN_HEX_STRING);
        assertArrayEquals(expected, buffer.array());
    }

    @Test
    public void testVerifyInboundSignature() throws Exception {
        byte[] inputPacket =
                TestUtils.hexStringToByteArray(AUTH_PAYLOAD_BODY_GENERIC_DIGITAL_SIGN_HEX_STRING);
        IkeAuthDigitalSignPayload payload =
                (IkeAuthDigitalSignPayload) IkeAuthPayload.getIkeAuthPayload(false, inputPacket);

        X509Certificate cert = CertUtils.createCertFromPemFile("end-cert-small.pem");

        payload.verifyInboundSignature(
                cert,
                IKE_INIT_RESP_REQUEST,
                NONCE_INIT_RESP,
                ID_RESP_PAYLOAD_BODY,
                mIkeHmacSha1Prf,
                PRF_RESP_KEY);
    }

    @Test
    public void testVerifyInboundSignatureFail() throws Exception {
        byte[] inputPacket =
                TestUtils.hexStringToByteArray(AUTH_PAYLOAD_BODY_GENERIC_DIGITAL_SIGN_HEX_STRING);
        IkeAuthDigitalSignPayload payload =
                (IkeAuthDigitalSignPayload) IkeAuthPayload.getIkeAuthPayload(false, inputPacket);

        assertArrayEquals(payload.signature, TestUtils.hexStringToByteArray(SIGNATURE));
        X509Certificate cert = CertUtils.createCertFromPemFile("end-cert-a.pem");

        try {
            payload.verifyInboundSignature(
                    cert,
                    IKE_INIT_RESP_REQUEST,
                    NONCE_INIT_RESP,
                    ID_RESP_PAYLOAD_BODY,
                    mIkeHmacSha1Prf,
                    PRF_RESP_KEY);
            fail("Expected to fail due to wrong certificate.");
        } catch (AuthenticationFailedException expected) {
        }
    }

    @Test
    public void testGenerateSignature() throws Exception {
        PrivateKey key = CertUtils.createRsaPrivateKeyFromKeyFile("end-cert-key-a.key");

        IkeAuthDigitalSignPayload authPayload =
                new IkeAuthDigitalSignPayload(
                        SIGNATURE_ALGO_RSA_SHA2_256,
                        key,
                        IKE_INIT_RESP_REQUEST,
                        NONCE_INIT_RESP,
                        ID_RESP_PAYLOAD_BODY,
                        mIkeHmacSha1Prf,
                        PRF_RESP_KEY);

        assertEquals(SIGNATURE_ALGO_RSA_SHA2_256, authPayload.signatureAndHashAlgos);
        assertArrayEquals(authPayload.signature, TestUtils.hexStringToByteArray(SIGNATURE));
    }
}
