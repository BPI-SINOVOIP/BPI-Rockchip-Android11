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

import android.net.ipsec.ike.SaProposal;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.crypto.IkeCipher;
import com.android.internal.net.ipsec.ike.crypto.IkeCombinedModeCipher;
import com.android.internal.net.ipsec.ike.crypto.IkeMacIntegrity;
import com.android.internal.net.ipsec.ike.crypto.IkeNormalModeCipher;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;

import org.junit.Before;
import org.junit.Test;

import java.security.GeneralSecurityException;
import java.util.Arrays;

public final class IkeEncryptedPayloadBodyTest {
    private static final String IKE_AUTH_INIT_REQUEST_HEADER =
            "5f54bf6d8b48e6e1909232b3d1edcb5c2e20230800000001000000ec";
    private static final String IKE_AUTH_INIT_REQUEST_SK_HEADER = "230000d0";
    private static final String IKE_AUTH_INIT_REQUEST_IV = "b9132b7bb9f658dfdc648e5017a6322a";
    private static final String IKE_AUTH_INIT_REQUEST_ENCRYPT_PADDED_DATA =
            "030c316ce55f365760d46426ce5cfc78bd1ed9abff63eb9594c1bd58"
                    + "46de333ecd3ea2b705d18293b130395300ba92a351041345"
                    + "0a10525cea51b2753b4e92b081fd78d995659a98f742278f"
                    + "f9b8fd3e21554865c15c79a5134d66b2744966089e416c60"
                    + "a274e44a9a3f084eb02f3bdce1e7de9de8d9a62773ab563b"
                    + "9a69ba1db03c752acb6136452b8a86c41addb4210d68c423"
                    + "efed80e26edca5fa3fe5d0a5ca9375ce332c474b93fb1fa3"
                    + "59eb4e81";
    private static final String IKE_AUTH_INIT_REQUEST_CHECKSUM = "ae6e0f22abdad69ba8007d50";

    private static final String IKE_AUTH_INIT_REQUEST_UNENCRYPTED_DATA =
            "2400000c010000000a50500d2700000c010000000a505050"
                    + "2100001c02000000df7c038aefaaa32d3f44b228b52a3327"
                    + "44dfb2c12c00002c00000028010304032ad4c0a20300000c"
                    + "0100000c800e008003000008030000020000000805000000"
                    + "2d00001801000000070000100000ffff00000000ffffffff"
                    + "2900001801000000070000100000ffff00000000ffffffff"
                    + "29000008000040000000000c0000400100000001";
    private static final String IKE_AUTH_INIT_REQUEST_PADDING = "0000000000000000000000";
    private static final int HMAC_SHA1_CHECKSUM_LEN = 12;

    private static final String ENCR_KEY_FROM_INIT_TO_RESP = "5cbfd33f75796c0188c4a3a546aec4a1";
    private static final String INTE_KEY_FROM_INIT_TO_RESP =
            "554fbf5a05b7f511e05a30ce23d874db9ef55e51";

    private static final String ENCR_ALGO_AES_CBC = "AES/CBC/NoPadding";

    // Test vectors for IKE message protected by HmacSha1 and 3DES
    private static final String HMAC_SHA1_3DES_MSG_HEX_STRING =
            "5837b1bd28ec424f85ddd0c609c8dbfe2e20232000000002"
                    + "00000064300000488beaf41d88544baabd95eac60269f19a"
                    + "5986295fe318ce02f65368cd957985f36b183794c4c78d35"
                    + "437762297a131a773d7f7806aaa0c590f48b9d71001f4d65"
                    + "70a44533";

    private static final String HMAC_SHA1_3DES_DECRYPTED_BODY_HEX_STRING =
            "00000028013c00241a013c001f10dac4f8b759138776091dd0f00033c5b07374726f6e675377616e";

    private static final String HMAC_SHA1_3DES_MSG_ENCR_KEY =
            "ee0fdd6d35bbdbe9eeef2f24495b6632e5047bdd8e413c87";
    private static final String HMAC_SHA1_3DES_MSG_INTE_KEY =
            "867a0bd019108db856cf6984fc9fb62d70c0de74";

    // TODO: b/142753861 Test IKE fragment protected by AES_CBC instead of 3DES

    // Test vectors for IKE fragment protected by HmacSha1 and 3DES
    private static final String HMAC_SHA1_3DES_FRAG_HEX_STRING =
            "939ae1251d18eb9077a99551b15c6e933520232000000001"
                    + "000000c0000000a400050005fd7c7931705af184b7be76bb"
                    + "d45a8ecbb3ffd58b9438b93f67e9fe86b06229f80e9b52d2"
                    + "ff6afde3f2c13ae93ce55a801f62e1a818c9003880a36bbe"
                    + "986fe6979ba233b9f4f0ddc992d06dbad5a2b998be18fae9"
                    + "47e5ccfb37775d069344e711fbf499bb289cf4cca245bd45"
                    + "0ad89d18689207759507ba18d47247e920b9e000a25a7596"
                    + "e4130929e5cdc37d5c1b0d90bbaae946c260f4d3cf815f6d";
    private static final String HMAC_SHA1_3DES_FRAG_DECRYPTED_BODY_HEX_STRING =
            "54ebd95c572100002002000000000100040a0a0a01000300"
                    + "040808080800030004080804042c00002c00000028020304"
                    + "03cc86090a0300000c0100000c800e010003000008030000"
                    + "0200000008050000002d00001801000000070000100000ff"
                    + "ff0a0a0a010a0a0a010000001801000000070000100000ff"
                    + "ff00000000ffffffff";
    private static final String HMAC_SHA1_3DES_FRAG_IV = "fd7c7931705af184";
    private static final String HMAC_SHA1_3DES_FRAG_PADDING = "dcf0fa5e2b64";

    private static final int HMAC_SHA1_3DES_FRAGMENT_NUM = 5;
    private static final int HMAC_SHA1_3DES_TOTAL_FRAGMENTS = 5;

    private static final String HMAC_SHA1_3DES_FRAG_ENCR_KEY =
            "6BBF6CB3526D6492F4DA0AF45E9B9FD3E1FF534280352073";
    private static final String HMAC_SHA1_3DES_FRAG_INTE_KEY =
            "293449E8E518060780B9C06F15838A06EEF57814";

    // Test vectors for IKE message protected by AES_GCM_16
    private static final String AES_GCM_MSG_HEX_STRING =
            "77c708b4523e39a471dc683c1d4f21362e20230800000005"
                    + "0000006127000045fbd69d9ee2dafc5e7c03a0106761065b2"
                    + "8fa8d11aed6046f7f8af117e44da7635be6e0dfafcb0a387c"
                    + "53fb46ba5d6fa9509161915929de97b7fbe23dc65723b0fe";
    private static final String AES_GCM_MSG_DECRYPTED_BODY_HEX_STRING =
            "000000280200000033233837e909ec805d56151bef5b1fa9b8e25b32419c9b3fc96ee699ec29d501";
    private static final String AES_GCM_MSG_IV = "fbd69d9ee2dafc5e";
    private static final String AES_GCM_MSG_ENCR_KEY =
            "7C04513660DEC572D896105254EF92608054F8E6EE19E79CE52AB8697B2B5F2C2AA90C29";

    // Test vectors for IKE fragment protected by AES_GCM_16
    private static final String AES_GCM_FRAG_HEX_STRING =
            "77c708b4523e39a471dc683c1d4f213635202320000000010"
                    + "0000113000000f7000200026faf9e5c04c67571871681d443"
                    + "01489f99fd78d318b0517a5a99bf6a3e1770f43d7d997c9e0"
                    + "d186038d16df3fd525eda821f80b3a40fc6bce397ac67539e"
                    + "40042919a5e9af38c70881d092a8571f0e131f594c0e8d6b8"
                    + "4ea116f0c95619439b0a267b35bc47dac72bbfb3d3776feb3"
                    + "86d7d4f819b0248f52f60bf4371ab6384e37819a9685c27d8"
                    + "e41abe30cd6f60905dd5c05c351ec0a1fcf9b99360161d2f3"
                    + "4dcf6401829df9392121d88e2201d279200e25d750678af6a"
                    + "7f4892a5c8d4a7358ec50cdf12cfa7652488f756ba6d07441"
                    + "e9a27aad3976ac8a705ff796857cb2df9ce360c3992e0285b"
                    + "34834255b06";
    private static final String AES_GCM_FRAG_DECRYPTED_BODY_HEX_STRING =
            "0fce6e996f4936ec8db8097339c371c686be75f4bed3f259c"
                    + "14d39c3ad90cb864085c6375f75b724d9f9daa8e7b22a106a"
                    + "488bc48c081997b7416fd33b146882e51ff6a640edf760212"
                    + "7f2454d502e92262ba3dd07cff52ee1bb1ea85f582db41a68"
                    + "aaf6dace362e5d8b10cfeb65eebc7572690e2c415a11cae57"
                    + "020810cf7aa56d9f2d5c2be3a633f8e4c6af5483a2b1f05bd"
                    + "4340ab551ddf7f51def57eaf5a37793ff6aa1e1ec288a2adf"
                    + "a647c369f15efa61a619966a320f24e1765c0e00c5ed394aa"
                    + "ef14512032b005827c000000090100000501";
    private static final String AES_GCM_FRAG_IV = "6faf9e5c04c67571";

    private static final int AES_GCM_FRAGMENT_NUM = 2;
    private static final int AES_GCM_TOTAL_FRAGMENTS = 2;

    private static final String AES_GCM_FRAG_ENCR_KEY =
            "955ED949D6F18857220E97B17D9285C830A39F8D4DC46AB43943668093C62A3D66664F8C";

    private static final int ENCRYPTED_BODY_SK_OFFSET =
            IkeHeader.IKE_HEADER_LENGTH + IkePayload.GENERIC_HEADER_LENGTH;
    private static final int ENCRYPTED_BODY_SKF_OFFSET =
            ENCRYPTED_BODY_SK_OFFSET + IkeSkfPayload.SKF_HEADER_LEN;

    private IkeNormalModeCipher mAesCbcCipher;
    private byte[] mAesCbcKey;

    private IkeMacIntegrity mHmacSha1IntegrityMac;
    private byte[] mHmacSha1IntegrityKey;

    private byte[] mDataToPadAndEncrypt;
    private byte[] mDataToAuthenticate;
    private byte[] mEncryptedPaddedData;
    private byte[] mIkeMessage;

    private byte[] mChecksum;
    private byte[] mIv;
    private byte[] mPadding;

    private IkeNormalModeCipher m3DesCipher;

    private IkeCombinedModeCipher mAesGcm16Cipher;

    private byte[] mAesGcmMsgKey;
    private byte[] mAesGcmMsg;
    private byte[] mAesGcmUnencryptedData;

    private byte[] mAesGcmFragKey;
    private byte[] mAesGcmFragMsg;
    private byte[] mAesGcmFragUnencryptedData;

    @Before
    public void setUp() throws Exception {
        mDataToPadAndEncrypt =
                TestUtils.hexStringToByteArray(IKE_AUTH_INIT_REQUEST_UNENCRYPTED_DATA);
        String hexStringToAuthenticate =
                IKE_AUTH_INIT_REQUEST_HEADER
                        + IKE_AUTH_INIT_REQUEST_SK_HEADER
                        + IKE_AUTH_INIT_REQUEST_IV
                        + IKE_AUTH_INIT_REQUEST_ENCRYPT_PADDED_DATA;
        mDataToAuthenticate = TestUtils.hexStringToByteArray(hexStringToAuthenticate);
        mEncryptedPaddedData =
                TestUtils.hexStringToByteArray(IKE_AUTH_INIT_REQUEST_ENCRYPT_PADDED_DATA);
        mIkeMessage =
                TestUtils.hexStringToByteArray(
                        IKE_AUTH_INIT_REQUEST_HEADER
                                + IKE_AUTH_INIT_REQUEST_SK_HEADER
                                + IKE_AUTH_INIT_REQUEST_IV
                                + IKE_AUTH_INIT_REQUEST_ENCRYPT_PADDED_DATA
                                + IKE_AUTH_INIT_REQUEST_CHECKSUM);

        mChecksum = TestUtils.hexStringToByteArray(IKE_AUTH_INIT_REQUEST_CHECKSUM);
        mIv = TestUtils.hexStringToByteArray(IKE_AUTH_INIT_REQUEST_IV);
        mPadding = TestUtils.hexStringToByteArray(IKE_AUTH_INIT_REQUEST_PADDING);

        m3DesCipher =
                (IkeNormalModeCipher)
                        IkeCipher.create(
                                new EncryptionTransform(SaProposal.ENCRYPTION_ALGORITHM_3DES));

        mAesCbcCipher =
                (IkeNormalModeCipher)
                        IkeCipher.create(
                                new EncryptionTransform(
                                        SaProposal.ENCRYPTION_ALGORITHM_AES_CBC,
                                        SaProposal.KEY_LEN_AES_128));
        mAesCbcKey = TestUtils.hexStringToByteArray(ENCR_KEY_FROM_INIT_TO_RESP);

        mHmacSha1IntegrityMac =
                IkeMacIntegrity.create(
                        new IntegrityTransform(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96));
        mHmacSha1IntegrityKey = TestUtils.hexStringToByteArray(INTE_KEY_FROM_INIT_TO_RESP);

        mAesGcm16Cipher =
                (IkeCombinedModeCipher)
                        IkeCipher.create(
                                new EncryptionTransform(
                                        SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_16,
                                        SaProposal.KEY_LEN_AES_256));

        mAesGcmMsgKey = TestUtils.hexStringToByteArray(AES_GCM_MSG_ENCR_KEY);
        mAesGcmMsg = TestUtils.hexStringToByteArray(AES_GCM_MSG_HEX_STRING);
        mAesGcmUnencryptedData =
                TestUtils.hexStringToByteArray(AES_GCM_MSG_DECRYPTED_BODY_HEX_STRING);

        mAesGcmFragKey = TestUtils.hexStringToByteArray(AES_GCM_FRAG_ENCR_KEY);
        mAesGcmFragMsg = TestUtils.hexStringToByteArray(AES_GCM_FRAG_HEX_STRING);
        mAesGcmFragUnencryptedData =
                TestUtils.hexStringToByteArray(AES_GCM_FRAG_DECRYPTED_BODY_HEX_STRING);
    }

    @Test
    public void testValidateChecksum() throws Exception {
        IkeEncryptedPayloadBody.validateInboundChecksumOrThrow(
                mDataToAuthenticate, mHmacSha1IntegrityMac, mHmacSha1IntegrityKey, mChecksum);
    }

    @Test
    public void testThrowForInvalidChecksum() throws Exception {
        byte[] dataToAuthenticate = Arrays.copyOf(mDataToAuthenticate, mDataToAuthenticate.length);
        dataToAuthenticate[0]++;

        try {
            IkeEncryptedPayloadBody.validateInboundChecksumOrThrow(
                    dataToAuthenticate, mHmacSha1IntegrityMac, mHmacSha1IntegrityKey, mChecksum);
            fail("Expected GeneralSecurityException due to mismatched checksum.");
        } catch (GeneralSecurityException expected) {
        }
    }

    @Test
    public void testCalculatePaddingPlaintextShorterThanBlockSize() throws Exception {
        int blockSize = 16;
        int plainTextLength = 15;
        int expectedPadLength = 0;

        byte[] calculatedPadding =
                IkeEncryptedPayloadBody.calculatePadding(plainTextLength, blockSize);
        assertEquals(expectedPadLength, calculatedPadding.length);
    }

    @Test
    public void testCalculatePaddingPlaintextInBlockSize() throws Exception {
        int blockSize = 16;
        int plainTextLength = 16;
        int expectedPadLength = 15;

        byte[] calculatedPadding =
                IkeEncryptedPayloadBody.calculatePadding(plainTextLength, blockSize);
        assertEquals(expectedPadLength, calculatedPadding.length);
    }

    @Test
    public void testCalculatePaddingPlaintextLongerThanBlockSize() throws Exception {
        int blockSize = 16;
        int plainTextLength = 17;
        int expectedPadLength = 14;

        byte[] calculatedPadding =
                IkeEncryptedPayloadBody.calculatePadding(plainTextLength, blockSize);
        assertEquals(expectedPadLength, calculatedPadding.length);
    }

    @Test
    public void testEncrypt() throws Exception {
        byte[] calculatedData =
                IkeEncryptedPayloadBody.normalModeEncrypt(
                        mDataToPadAndEncrypt, mAesCbcCipher, mAesCbcKey, mIv, mPadding);

        assertArrayEquals(mEncryptedPaddedData, calculatedData);
    }

    @Test
    public void testDecrypt() throws Exception {
        byte[] calculatedPlainText =
                IkeEncryptedPayloadBody.normalModeDecrypt(
                        mEncryptedPaddedData, mAesCbcCipher, mAesCbcKey, mIv);

        assertArrayEquals(mDataToPadAndEncrypt, calculatedPlainText);
    }

    @Test
    public void testBuildAndEncodeOutboundIkeEncryptedPayloadBody() throws Exception {
        IkeHeader ikeHeader = new IkeHeader(mIkeMessage);

        IkeEncryptedPayloadBody payloadBody =
                new IkeEncryptedPayloadBody(
                        ikeHeader,
                        IkePayload.PAYLOAD_TYPE_ID_INITIATOR,
                        new byte[0] /*skfHeader*/,
                        mDataToPadAndEncrypt,
                        mHmacSha1IntegrityMac,
                        mAesCbcCipher,
                        mHmacSha1IntegrityKey,
                        mAesCbcKey,
                        mIv,
                        mPadding);

        byte[] expectedEncodedData =
                TestUtils.hexStringToByteArray(
                        IKE_AUTH_INIT_REQUEST_IV
                                + IKE_AUTH_INIT_REQUEST_ENCRYPT_PADDED_DATA
                                + IKE_AUTH_INIT_REQUEST_CHECKSUM);
        assertArrayEquals(expectedEncodedData, payloadBody.encode());
    }

    @Test
    public void testAuthAndDecodeHmacSha1AesCbc() throws Exception {
        IkeEncryptedPayloadBody payloadBody =
                new IkeEncryptedPayloadBody(
                        mIkeMessage,
                        ENCRYPTED_BODY_SK_OFFSET,
                        mHmacSha1IntegrityMac,
                        mAesCbcCipher,
                        mHmacSha1IntegrityKey,
                        mAesCbcKey);

        assertArrayEquals(mDataToPadAndEncrypt, payloadBody.getUnencryptedData());
    }

    @Test
    public void testAuthAndDecodeHmacSha13Des() throws Exception {
        byte[] message = TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_MSG_HEX_STRING);
        byte[] expectedDecryptedData =
                TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_DECRYPTED_BODY_HEX_STRING);

        IkeEncryptedPayloadBody payloadBody =
                new IkeEncryptedPayloadBody(
                        message,
                        ENCRYPTED_BODY_SK_OFFSET,
                        mHmacSha1IntegrityMac,
                        m3DesCipher,
                        TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_MSG_INTE_KEY),
                        TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_MSG_ENCR_KEY));

        assertArrayEquals(expectedDecryptedData, payloadBody.getUnencryptedData());
    }

    @Test
    public void testBuildAndEncodeWithHmacSha13Des() throws Exception {
        byte[] message = TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_FRAG_HEX_STRING);
        IkeHeader ikeHeader = new IkeHeader(message);

        byte[] skfHeaderBytes =
                IkeSkfPayload.encodeSkfHeader(
                        HMAC_SHA1_3DES_FRAGMENT_NUM, HMAC_SHA1_3DES_TOTAL_FRAGMENTS);

        IkeEncryptedPayloadBody payloadBody =
                new IkeEncryptedPayloadBody(
                        ikeHeader,
                        IkePayload.PAYLOAD_TYPE_NO_NEXT,
                        skfHeaderBytes,
                        TestUtils.hexStringToByteArray(
                                HMAC_SHA1_3DES_FRAG_DECRYPTED_BODY_HEX_STRING),
                        mHmacSha1IntegrityMac,
                        m3DesCipher,
                        TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_FRAG_INTE_KEY),
                        TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_FRAG_ENCR_KEY),
                        TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_FRAG_IV),
                        TestUtils.hexStringToByteArray(HMAC_SHA1_3DES_FRAG_PADDING));

        byte[] expectedEncodedData =
                Arrays.copyOfRange(message, ENCRYPTED_BODY_SKF_OFFSET, message.length);

        assertArrayEquals(expectedEncodedData, payloadBody.encode());
    }

    @Test
    public void testAuthAndDecodeFullMsgWithAesGcm() throws Exception {
        IkeEncryptedPayloadBody encryptedBody =
                new IkeEncryptedPayloadBody(
                        mAesGcmMsg,
                        ENCRYPTED_BODY_SK_OFFSET,
                        null /*integrityMac*/,
                        mAesGcm16Cipher,
                        null /*integrityKey*/,
                        mAesGcmMsgKey);

        assertArrayEquals(mAesGcmUnencryptedData, encryptedBody.getUnencryptedData());
    }

    @Test
    public void testBuildAndEncodeMsgWithAesGcm() throws Exception {
        IkeHeader ikeHeader = new IkeHeader(mAesGcmMsg);

        IkeEncryptedPayloadBody payloadBody =
                new IkeEncryptedPayloadBody(
                        ikeHeader,
                        IkePayload.PAYLOAD_TYPE_AUTH,
                        new byte[0],
                        mAesGcmUnencryptedData,
                        null /*integrityMac*/,
                        mAesGcm16Cipher,
                        null /*integrityKey*/,
                        mAesGcmMsgKey,
                        TestUtils.hexStringToByteArray(AES_GCM_MSG_IV),
                        new byte[0] /*padding*/);

        byte[] expectedEncodedData =
                Arrays.copyOfRange(mAesGcmMsg, ENCRYPTED_BODY_SK_OFFSET, mAesGcmMsg.length);

        assertArrayEquals(expectedEncodedData, payloadBody.encode());
    }

    @Test
    public void testAuthAndDecodeFragMsgWithAesGcm() throws Exception {
        IkeEncryptedPayloadBody encryptedBody =
                new IkeEncryptedPayloadBody(
                        mAesGcmFragMsg,
                        ENCRYPTED_BODY_SKF_OFFSET,
                        null /*integrityMac*/,
                        mAesGcm16Cipher,
                        null /*integrityKey*/,
                        mAesGcmFragKey);

        assertArrayEquals(mAesGcmFragUnencryptedData, encryptedBody.getUnencryptedData());
    }

    @Test
    public void testBuildAndEncodeFragMsgWithAesGcm() throws Exception {
        IkeHeader ikeHeader = new IkeHeader(mAesGcmFragMsg);
        byte[] skfHeaderBytes =
                IkeSkfPayload.encodeSkfHeader(AES_GCM_FRAGMENT_NUM, AES_GCM_TOTAL_FRAGMENTS);

        IkeEncryptedPayloadBody payloadBody =
                new IkeEncryptedPayloadBody(
                        ikeHeader,
                        IkePayload.PAYLOAD_TYPE_NO_NEXT,
                        skfHeaderBytes,
                        mAesGcmFragUnencryptedData,
                        null /*integrityMac*/,
                        mAesGcm16Cipher,
                        null /*integrityKey*/,
                        mAesGcmFragKey,
                        TestUtils.hexStringToByteArray(AES_GCM_FRAG_IV),
                        new byte[0] /*padding*/);

        byte[] expectedEncodedData =
                Arrays.copyOfRange(
                        mAesGcmFragMsg, ENCRYPTED_BODY_SKF_OFFSET, mAesGcmFragMsg.length);

        assertArrayEquals(expectedEncodedData, payloadBody.encode());
    }
}
