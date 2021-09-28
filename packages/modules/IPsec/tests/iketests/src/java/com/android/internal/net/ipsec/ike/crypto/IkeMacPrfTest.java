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

package com.android.internal.net.ipsec.ike.crypto;

import static com.android.internal.net.TestUtils.hexStringToByteArray;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;

import android.net.ipsec.ike.SaProposal;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.PrfTransform;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Arrays;

@RunWith(JUnit4.class)
public final class IkeMacPrfTest {
    // Test vectors for PRF_HMAC_SHA1
    private static final String PRF_KEY_HEX_STRING = "094787780EE466E2CB049FA327B43908BC57E485";
    private static final String DATA_TO_SIGN_HEX_STRING = "010000000a50500d";
    private static final String CALCULATED_MAC_HEX_STRING =
            "D83B20CC6A0932B2A7CEF26E4020ABAAB64F0C6A";

    private static final String IKE_INIT_SPI = "5F54BF6D8B48E6E1";
    private static final String IKE_RESP_SPI = "909232B3D1EDCB5C";

    private static final String IKE_NONCE_INIT_HEX_STRING =
            "C39B7F368F4681B89FA9B7BE6465ABD7C5F68B6ED5D3B4C72CB4240EB5C46412";
    private static final String IKE_NONCE_RESP_HEX_STRING =
            "9756112CA539F5C25ABACC7EE92B73091942A9C06950F98848F1AF1694C4DDFF";

    private static final String IKE_SHARED_DH_KEY_HEX_STRING =
            "C14155DEA40056BD9C76FB4819687B7A397582F4CD5AFF4B"
                    + "8F441C56E0C08C84234147A0BA249A555835A048E3CA2980"
                    + "7D057A61DD26EEFAD9AF9C01497005E52858E29FB42EB849"
                    + "6731DF96A11CCE1F51137A9A1B900FA81AEE7898E373D4E4"
                    + "8B899BBECA091314ECD4B6E412EF4B0FEF798F54735F3180"
                    + "7424A318287F20E8";

    private static final String IKE_SKEYSEED_HEX_STRING =
            "8C42F3B1F5F81C7BAAC5F33E9A4F01987B2F9657";
    private static final String IKE_SK_D_HEX_STRING = "C86B56EFCF684DCC2877578AEF3137167FE0EBF6";
    private static final String IKE_SK_AUTH_INIT_HEX_STRING =
            "554FBF5A05B7F511E05A30CE23D874DB9EF55E51";
    private static final String IKE_SK_AUTH_RESP_HEX_STRING =
            "36D83420788337CA32ECAA46892C48808DCD58B1";
    private static final String IKE_SK_ENCR_INIT_HEX_STRING = "5CBFD33F75796C0188C4A3A546AEC4A1";
    private static final String IKE_SK_ENCR_RESP_HEX_STRING = "C33B35FCF29514CD9D8B4A695E1A816E";
    private static final String IKE_SK_PRF_INIT_HEX_STRING =
            "094787780EE466E2CB049FA327B43908BC57E485";
    private static final String IKE_SK_PRF_RESP_HEX_STRING =
            "A30E6B08BE56C0E6BFF4744143C75219299E1BEB";
    private static final String IKE_KEY_MAT =
            IKE_SK_D_HEX_STRING
                    + IKE_SK_AUTH_INIT_HEX_STRING
                    + IKE_SK_AUTH_RESP_HEX_STRING
                    + IKE_SK_ENCR_INIT_HEX_STRING
                    + IKE_SK_ENCR_RESP_HEX_STRING
                    + IKE_SK_PRF_INIT_HEX_STRING
                    + IKE_SK_PRF_RESP_HEX_STRING;

    private static final int IKE_AUTH_ALGO_KEY_LEN = 20;
    private static final int IKE_ENCR_ALGO_KEY_LEN = 16;
    private static final int IKE_PRF_KEY_LEN = 20;
    private static final int IKE_SK_D_KEY_LEN = IKE_PRF_KEY_LEN;

    private static final String FIRST_CHILD_ENCR_INIT_HEX_STRING =
            "1B865CEA6E2C23973E8C5452ADC5CD7D";
    private static final String FIRST_CHILD_ENCR_RESP_HEX_STRING =
            "5E82FEDACC6DCB0756DDD7553907EBD1";
    private static final String FIRST_CHILD_AUTH_INIT_HEX_STRING =
            "A7A5A44F7EF4409657206C7DC52B7E692593B51E";
    private static final String FIRST_CHILD_AUTH_RESP_HEX_STRING =
            "CDE612189FD46DE870FAEC04F92B40B0BFDBD9E1";
    private static final String FIRST_CHILD_KEY_MAT =
            FIRST_CHILD_ENCR_INIT_HEX_STRING
                    + FIRST_CHILD_AUTH_INIT_HEX_STRING
                    + FIRST_CHILD_ENCR_RESP_HEX_STRING
                    + FIRST_CHILD_AUTH_RESP_HEX_STRING;

    private static final int FIRST_CHILD_AUTH_ALGO_KEY_LEN = 20;
    private static final int FIRST_CHILD_ENCR_ALGO_KEY_LEN = 16;

    // Test vectors for PRF_HMAC_SHA256
    private static final String PRF_HMAC256_KEY_HEX_STRING =
            "E6075DF8C7DE1695C3641C190920E8C0655DE695A429FEAC9AA8F932871F1EDD";
    private static final String PRF_HMAC256_DATA_TO_SIGN_HEX_STRING = "01000000C0A82B67";
    private static final String PRF_HMAC256_CALCULATED_MAC_HEX_STRING =
            "CFEE9454BA2FFFAD01D0B49EAEA854B5FEC9839F8747600F85197FF0054AE716";

    private static final String PRF_HMAC256_IKE_NONCE_INIT_HEX_STRING =
            "25de54767208d840fbe6881c699cbc8841582eae6f8724ac17c0ad8680e57b3e";
    private static final String PRF_HMAC256_IKE_NONCE_RESP_HEX_STRING =
            "7550340a140ce00f523b0e4bddec7671336bbca8c3dd2e31312d36c904950ca4";
    private static final String PRF_HMAC256_IKE_SHARED_DH_KEY_HEX_STRING =
            "01FB25276B0D1D9979DAC170B53815988C31A50AD0DF4EEB89B8407EFAE3A676"
                    + "281903E48AD020050C9E1522F4DDA031C57A7A3D0CA9D075F044B5212E2A88B92E71";
    private static final String PRF_HMAC256_IKE_SKEYSEED_HEX_STRING =
            "9A3E05D4DBE797DC45DA7660DB60C9CCCEEBBC32CA2B0EDA5B5A8793EF192E4E";

    private static final String PRF_AES128XCBC_KEY_HEX_STRING = "000102030405060708090a0b0c0d0e0f";
    private static final String PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING =
            "000102030405060708090a0b0c0d0e0f10111213";
    private static final String PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING =
            "47f51b4564966215b8985c63055ed308";

    private static final String PRF_AES128XCBC_KEY_HEX_STRING1 = "000102030405060708090a0b0c0d0e0f";
    private static final String PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING1 =
            "000102030405060708090a0b0c0d0e0f";
    private static final String PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING1 =
            "d2a246fa349b68a79998a4394ff7a263";
    private static final String PRF_AES128_IKE_NONCE_INIT_HEX_STRING =
            "00010203040506079B7BE6465ABD7C5F68B6ED5D3B4C72CB4240EB5C464123";
    private static final String PRF_AES128_IKE_NONCE_RESP_HEX_STRING =
            "08090a0B0C0D0E0FEE92B73091942A9C06950F98848F1AF1694C4DDFF1";
    private static final String PRF_AES128XCBC_KEY_HEX_STRING2 = "00010203040506070809";
    private static final String PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING2 =
            "000102030405060708090a0b0c0d0e0f10111213";
    private static final String PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING2 =
            "0fa087af7d866e7653434e602fdde835";
    private static final String PRF_AES128XCBC_KEY_HEX_STRING3 =
            "000102030405060708090a0b0c0d0e0fedcb";
    private static final String PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING3 =
            "000102030405060708090a0b0c0d0e0f10111213";
    private static final String PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING3 =
            "8cd3c93ae598a9803006ffb67c40e9e4";

    private IkeMacPrf mIkeHmacSha1Prf;
    private IkeMacPrf mIkeHmacSha256Prf;
    private IkeMacPrf mIkeAes128XCbcPrf;

    @Before
    public void setUp() throws Exception {
        mIkeHmacSha1Prf =
                IkeMacPrf.create(new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_HMAC_SHA1));
        mIkeHmacSha256Prf =
                IkeMacPrf.create(new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_SHA2_256));
        mIkeAes128XCbcPrf =
                IkeMacPrf.create(new PrfTransform(SaProposal.PSEUDORANDOM_FUNCTION_AES128_XCBC));
    }

    @Test
    public void testSignBytesHmacSha1() throws Exception {
        byte[] skpBytes = TestUtils.hexStringToByteArray(PRF_KEY_HEX_STRING);
        byte[] dataBytes = TestUtils.hexStringToByteArray(DATA_TO_SIGN_HEX_STRING);

        byte[] calculatedBytes = mIkeHmacSha1Prf.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes = TestUtils.hexStringToByteArray(CALCULATED_MAC_HEX_STRING);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }

    @Test
    public void testSignBytesHmacSha256() throws Exception {
        byte[] skpBytes = TestUtils.hexStringToByteArray(PRF_HMAC256_KEY_HEX_STRING);
        byte[] dataBytes = TestUtils.hexStringToByteArray(PRF_HMAC256_DATA_TO_SIGN_HEX_STRING);

        byte[] calculatedBytes = mIkeHmacSha256Prf.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(PRF_HMAC256_CALCULATED_MAC_HEX_STRING);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }

    @Test
    public void testGenerateSKeySeedHmacSha1() throws Exception {
        byte[] nonceInit = TestUtils.hexStringToByteArray(IKE_NONCE_INIT_HEX_STRING);
        byte[] nonceResp = TestUtils.hexStringToByteArray(IKE_NONCE_RESP_HEX_STRING);
        byte[] sharedDhKey = TestUtils.hexStringToByteArray(IKE_SHARED_DH_KEY_HEX_STRING);

        byte[] calculatedSKeySeed =
                mIkeHmacSha1Prf.generateSKeySeed(nonceInit, nonceResp, sharedDhKey);

        byte[] expectedSKeySeed = TestUtils.hexStringToByteArray(IKE_SKEYSEED_HEX_STRING);
        assertArrayEquals(expectedSKeySeed, calculatedSKeySeed);
    }

    @Test
    public void testGenerateSKeySeedHmacSha256() throws Exception {
        byte[] nonceInit = TestUtils.hexStringToByteArray(PRF_HMAC256_IKE_NONCE_INIT_HEX_STRING);
        byte[] nonceResp = TestUtils.hexStringToByteArray(PRF_HMAC256_IKE_NONCE_RESP_HEX_STRING);
        byte[] sharedDhKey =
                TestUtils.hexStringToByteArray(PRF_HMAC256_IKE_SHARED_DH_KEY_HEX_STRING);

        byte[] calculatedSKeySeed =
                mIkeHmacSha256Prf.generateSKeySeed(nonceInit, nonceResp, sharedDhKey);

        byte[] expectedSKeySeed =
                TestUtils.hexStringToByteArray(PRF_HMAC256_IKE_SKEYSEED_HEX_STRING);
        assertArrayEquals(expectedSKeySeed, calculatedSKeySeed);
    }

    @Test
    public void testGenerateRekeyedSKeySeed() throws Exception {
        byte[] nonceInit = TestUtils.hexStringToByteArray(IKE_NONCE_INIT_HEX_STRING);
        byte[] nonceResp = TestUtils.hexStringToByteArray(IKE_NONCE_RESP_HEX_STRING);
        byte[] sharedDhKey = TestUtils.hexStringToByteArray(IKE_SHARED_DH_KEY_HEX_STRING);
        byte[] old_skd = TestUtils.hexStringToByteArray(IKE_SK_D_HEX_STRING);

        byte[] calculatedSKeySeed =
                mIkeHmacSha1Prf.generateRekeyedSKeySeed(old_skd, nonceInit, nonceResp, sharedDhKey);

        // Verify that the new sKeySeed is different.
        // TODO: Find actual test vectors to test positive case.
        byte[] oldSKeySeed = TestUtils.hexStringToByteArray(IKE_SKEYSEED_HEX_STRING);
        assertFalse(Arrays.equals(oldSKeySeed, calculatedSKeySeed));
    }

    @Test
    public void testGenerateKeyMatForIke() throws Exception {
        byte[] prfKey = TestUtils.hexStringToByteArray(IKE_SKEYSEED_HEX_STRING);
        byte[] prfData =
                TestUtils.hexStringToByteArray(
                        IKE_NONCE_INIT_HEX_STRING
                                + IKE_NONCE_RESP_HEX_STRING
                                + IKE_INIT_SPI
                                + IKE_RESP_SPI);
        int keyMaterialLen =
                IKE_SK_D_KEY_LEN
                        + IKE_AUTH_ALGO_KEY_LEN * 2
                        + IKE_ENCR_ALGO_KEY_LEN * 2
                        + IKE_PRF_KEY_LEN * 2;

        byte[] calculatedKeyMat = mIkeHmacSha1Prf.generateKeyMat(prfKey, prfData, keyMaterialLen);

        byte[] expectedKeyMat = TestUtils.hexStringToByteArray(IKE_KEY_MAT);
        assertArrayEquals(expectedKeyMat, calculatedKeyMat);
    }

    @Test
    public void testGenerateKeyMatForFirstChild() throws Exception {
        byte[] prfKey = TestUtils.hexStringToByteArray(IKE_SK_D_HEX_STRING);
        byte[] prfData =
                TestUtils.hexStringToByteArray(
                        IKE_NONCE_INIT_HEX_STRING + IKE_NONCE_RESP_HEX_STRING);
        int keyMaterialLen = FIRST_CHILD_AUTH_ALGO_KEY_LEN * 2 + FIRST_CHILD_ENCR_ALGO_KEY_LEN * 2;

        byte[] calculatedKeyMat = mIkeHmacSha1Prf.generateKeyMat(prfKey, prfData, keyMaterialLen);

        byte[] expectedKeyMat = TestUtils.hexStringToByteArray(FIRST_CHILD_KEY_MAT);
        assertArrayEquals(expectedKeyMat, calculatedKeyMat);
    }

    @Test
    public void testSignBytesPrfAes128XCbc() throws Exception {
        byte[] skpBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_KEY_HEX_STRING);
        byte[] dataBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING);

        byte[] calculatedBytes = mIkeAes128XCbcPrf.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }

    @Test
    public void testSignBytesPrfAes128XCbcWith16ByteInput() throws Exception {
        // 16-byte is a multiple of aes block size. Hence key2 will be used instead of key3
        byte[] skpBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_KEY_HEX_STRING1);
        byte[] dataBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING1);

        byte[] calculatedBytes = mIkeAes128XCbcPrf.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING1);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }

    @Test
    public void testSignBytesPrfAes128XCbcWithKeyShorterThan16Bytes() throws Exception {
        byte[] skpBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_KEY_HEX_STRING2);
        byte[] dataBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING2);

        byte[] calculatedBytes = mIkeAes128XCbcPrf.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING2);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }

    @Test
    public void testSignBytesPrfAes128XCbcWithKeyLongerThan16Bytes() throws Exception {
        byte[] skpBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_KEY_HEX_STRING3);
        byte[] dataBytes = TestUtils.hexStringToByteArray(PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING3);

        byte[] calculatedBytes = mIkeAes128XCbcPrf.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING3);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }

    @Test
    public void testGenerateSKeySeedAes128XCbc() throws Exception {
        // TODO: Test key generation with real IKE exchange packets
        byte[] nonceInit = TestUtils.hexStringToByteArray(PRF_AES128_IKE_NONCE_INIT_HEX_STRING);
        byte[] nonceResp = TestUtils.hexStringToByteArray(PRF_AES128_IKE_NONCE_RESP_HEX_STRING);
        byte[] sharedDhKey =
                TestUtils.hexStringToByteArray(PRF_AES128XCBC_DATA_TO_SIGN_HEX_STRING1);

        byte[] calculatedSKeySeed =
                mIkeAes128XCbcPrf.generateSKeySeed(nonceInit, nonceResp, sharedDhKey);

        byte[] expectedSKeySeed =
                TestUtils.hexStringToByteArray(PRF_AES128XCBC_CALCULATED_MAC_HEX_STRING1);
        assertArrayEquals(expectedSKeySeed, calculatedSKeySeed);
    }

    @Test
    public void testGenerateRekeySKeySeedAes128XCbc() throws Exception {
        final String nonceInitHex =
                "89BB8717030645EF008FE594C95F5E0950F9C84073678695FFD9B45AB4E2A3A2";
        final String nonceRespHex =
                "46B8ECA226DE2F7510A755867F370A891154591AF34342C5C2E87DD5D0E1037A";
        final String sharedDhKeyHex =
                "3A814834D7695D62AA523B7728315F16342DFDE1AB0A955C6E75DEB2269CB34E"
                        + "4588CA9ADED563E2037A1D88EDB6A6AF7D8444313395ED0DBD994D6CB2E3878F"
                        + "E54E4701C320793BAB8A74E4CD7EDE787EC600E2E375A5FC899936062724E2DC"
                        + "20B1B24283E607A314F3E7B3D07565A75525968E097F7BEEBB688E09256CFCBA";
        final String oldSkdHex = "291560590304CDC3BCF5CF0402E4F83E";
        final String expectedSKeySeedHex = "73DEE6B57953E561CD55306D64B726EF";

        byte[] nonceInit = hexStringToByteArray(nonceInitHex);
        byte[] nonceResp = hexStringToByteArray(nonceRespHex);
        byte[] sharedDhKey = hexStringToByteArray(sharedDhKeyHex);
        byte[] oldSkd = hexStringToByteArray(oldSkdHex);

        byte[] calculatedSKeySeed =
                mIkeAes128XCbcPrf.generateRekeyedSKeySeed(
                        hexStringToByteArray(oldSkdHex),
                        hexStringToByteArray(nonceInitHex),
                        hexStringToByteArray(nonceRespHex),
                        hexStringToByteArray(sharedDhKeyHex));
        assertArrayEquals(hexStringToByteArray(expectedSKeySeedHex), calculatedSKeySeed);
    }
}
