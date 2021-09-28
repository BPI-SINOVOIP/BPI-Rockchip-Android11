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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;

import android.net.ipsec.ike.SaProposal;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.crypto.IkeCipher;
import com.android.internal.net.ipsec.ike.crypto.IkeMacIntegrity;
import com.android.internal.net.ipsec.ike.crypto.IkeNormalModeCipher;
import com.android.internal.net.ipsec.ike.exceptions.InvalidSyntaxException;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;
import java.util.Arrays;

public final class IkeSkfPayloadTest {
    private static final String IKE_FRAG_MSG_HEX_STRING =
            "bb3d5237aa558779db24aeb6c9ea183e352023200000000100000134"
                    + "0000011800020002c1471fc7714a3b77a2bfd78dd2df4c048dfed913"
                    + "c5de76cb1f4463ef541df2442b43c65308a47b873502268cc1195f99"
                    + "4f6f1a945f56cb342969936af97d79c560c8e0f8bb1a0874ebfb5d0e"
                    + "610b0fcff96d4197c06e7aef07a3a9ae487555bec95c78b87fe6483c"
                    + "be07e3d132f8594c34dba5b5b463b871d0272af6a1ee701fc6b7b70a"
                    + "22a1b8f63eed50ce6b2253ee63fe2cf0289a5eb715e56b389f72b5ba"
                    + "ecfb7340f4abf9253a8c973d281ed62f3516d130fcaaf2c2145b3047"
                    + "f3a243e60beb2fc28bf05183839caf46bfbfc4f28c9a2224e7d49686"
                    + "52a236a403ecb203a1de1e2144a6f5ce28acc2f93989608af17381fc"
                    + "f965cabe1a448274264b22167abfa047dc88e4bfdc5a492847d36d8b";

    private static final String IKE_FRAG_MSG_DECRYPTED_BODY_HEX_STRING =
            "dc89d82f4fccff8d3f0c4f4848571e57205f7dbdce954203983d2147"
                    + "3a9e10ba36876b860d33afbdfe6ebf000240e31f2039f4213e882d1f"
                    + "6f0a24887aed0584f4b50a016d989990fd58297757c7b842cd72b57c"
                    + "2f68cba8a5f06d899ce3fcfbd0419402a1d59f1c5b5b23bd0a4ed525"
                    + "27ed6cef9fd238552fcf6e4cd9f794d2b01ba61438fd21714fbc3e3f"
                    + "443a816751e55d46009ae7fb9f52db0977e453a2d28b0453a9393778"
                    + "3a0b625c27d186c052a7169807537d97e731a3543fc10dca605ca86d"
                    + "1496882e1d009a9216d07d0000001801850014120a00000f02000200"
                    + "0100000d010000";

    private static final String IKE_FRAG_MSG_CHECKSUM = "7abfa047dc88e4bfdc5a492847d36d8b";
    private static final String IKE_FRAG_MSG_PADDING = "05d925c1b3804aee08";

    private static final int FRAGMENT_NUM = 2;
    private static final int TOTAL_FRAGMENTS = 2;

    private static final int FRAGMENT_NUM_OFFSET =
            IkeHeader.IKE_HEADER_LENGTH + IkePayload.GENERIC_HEADER_LENGTH;
    private static final int TOTAL_FRAGMENTS_OFFET = FRAGMENT_NUM_OFFSET + 2;

    private byte[] mDecryptedData;

    private IkeMacIntegrity mSpyHmacSha256IntegrityMac;
    private IkeNormalModeCipher mSpyAesCbcCipher;

    @Before
    public void setUp() throws Exception {
        mDecryptedData = TestUtils.hexStringToByteArray(IKE_FRAG_MSG_DECRYPTED_BODY_HEX_STRING);

        // Set up integrity algorithm
        mSpyHmacSha256IntegrityMac =
                spy(
                        IkeMacIntegrity.create(
                                new IntegrityTransform(
                                        SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA2_256_128)));
        byte[] expectedChecksum = TestUtils.hexStringToByteArray(IKE_FRAG_MSG_CHECKSUM);
        doReturn(expectedChecksum).when(mSpyHmacSha256IntegrityMac).generateChecksum(any(), any());

        // Set up encryption algorithm
        mSpyAesCbcCipher =
                spy(
                        (IkeNormalModeCipher)
                                IkeCipher.create(
                                        new EncryptionTransform(
                                                SaProposal.ENCRYPTION_ALGORITHM_AES_CBC,
                                                SaProposal.KEY_LEN_AES_128)));
        byte[] expectedDecryptedPaddedData =
                TestUtils.hexStringToByteArray(
                        IKE_FRAG_MSG_DECRYPTED_BODY_HEX_STRING + IKE_FRAG_MSG_PADDING);
        doReturn(expectedDecryptedPaddedData).when(mSpyAesCbcCipher).decrypt(any(), any(), any());
    }

    private IkeSkfPayload decodeAndDecryptFragMsg(byte[] message) throws Exception {
        IkeSkfPayload payload =
                (IkeSkfPayload)
                        IkePayloadFactory.getIkeSkPayload(
                                        true /*isSkf*/,
                                        message,
                                        mSpyHmacSha256IntegrityMac,
                                        mSpyAesCbcCipher,
                                        new byte[0] /*integrityKey*/,
                                        new byte[0] /*decryptionKey*/)
                                .first;
        return payload;
    }

    @Test
    public void testDecode() throws Exception {
        byte[] message = TestUtils.hexStringToByteArray(IKE_FRAG_MSG_HEX_STRING);

        IkeSkfPayload payload = decodeAndDecryptFragMsg(message);

        assertEquals(IkePayload.PAYLOAD_TYPE_SKF, payload.payloadType);
        assertEquals(FRAGMENT_NUM, payload.fragmentNum);
        assertEquals(TOTAL_FRAGMENTS, payload.totalFragments);
        assertArrayEquals(mDecryptedData, payload.getUnencryptedData());
    }

    @Test
    public void testDecodeThrowsForZeroFragNum() throws Exception {
        byte[] message = TestUtils.hexStringToByteArray(IKE_FRAG_MSG_HEX_STRING);

        // Set Fragment Number to zero
        message[FRAGMENT_NUM_OFFSET] = 0;
        message[FRAGMENT_NUM_OFFSET + 1] = 0;

        try {
            decodeAndDecryptFragMsg(message);
            fail("Expected to fail because Fragment Number is zero.");
        } catch (InvalidSyntaxException expected) {
        }
    }

    @Test
    public void testDecodeThrowsForZeroTotalFragments() throws Exception {
        byte[] message = TestUtils.hexStringToByteArray(IKE_FRAG_MSG_HEX_STRING);

        // Set Total Fragments Number to zero
        message[TOTAL_FRAGMENTS_OFFET] = 0;
        message[TOTAL_FRAGMENTS_OFFET + 1] = 0;

        try {
            decodeAndDecryptFragMsg(message);
            fail("Expected to fail because Total Fragments Number is zero.");
        } catch (InvalidSyntaxException expected) {
        }
    }

    @Test
    public void testDecodeThrowsWhenFragNumIsLargerThanTotalFragments() throws Exception {
        byte[] message = TestUtils.hexStringToByteArray(IKE_FRAG_MSG_HEX_STRING);

        // Set Fragment Number to 5
        message[FRAGMENT_NUM_OFFSET] = 0;
        message[FRAGMENT_NUM_OFFSET + 1] = 5;

        // Set Total Fragments Number to 2
        message[TOTAL_FRAGMENTS_OFFET] = 0;
        message[TOTAL_FRAGMENTS_OFFET + 1] = 2;

        try {
            decodeAndDecryptFragMsg(message);
            fail(
                    "Expected to fail because Fragment Number is larger than"
                            + " Total Fragments Number");
        } catch (InvalidSyntaxException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        byte[] message = TestUtils.hexStringToByteArray(IKE_FRAG_MSG_HEX_STRING);
        IkeSkfPayload payload = decodeAndDecryptFragMsg(message);

        int payloadLength = payload.getPayloadLength();
        ByteBuffer buffer = ByteBuffer.allocate(payloadLength);
        payload.encodeToByteBuffer(IkePayload.PAYLOAD_TYPE_NO_NEXT, buffer);

        byte[] expectedPayloadBytes =
                Arrays.copyOfRange(message, IkeHeader.IKE_HEADER_LENGTH, message.length);
        assertArrayEquals(expectedPayloadBytes, buffer.array());
    }
}
