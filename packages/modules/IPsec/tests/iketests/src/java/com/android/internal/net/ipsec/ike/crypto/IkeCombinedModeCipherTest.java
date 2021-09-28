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

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.IpSecAlgorithm;
import android.net.ipsec.ike.SaProposal;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.EncryptionTransform;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Arrays;
import java.util.Random;

import javax.crypto.AEADBadTagException;

@RunWith(JUnit4.class)
public final class IkeCombinedModeCipherTest {
    private static final String IV = "fbd69d9de2dafc5e";
    private static final String ENCRYPTED_PADDED_DATA_WITH_CHECKSUM =
            "f4109834e9f3559758c05edf119917521b885f67f0d14ced43";
    private static final String UNENCRYPTED_PADDED_DATA = "000000080000400f00";
    private static final String ADDITIONAL_AUTH_DATA =
            "77c708b4523e39a471dc683c1d4f21362e202508000000060000004129000025";
    private static final String KEY =
            "7C04513660DEC572D896105254EF92608054F8E6EE19E79CE52AB8697B2B5F2C2AA90C29";

    private static final int AES_GCM_IV_LEN = 8;
    private static final int AES_GCM_16_CHECKSUM_LEN = 128;

    private IkeCombinedModeCipher mAesGcm16Cipher;

    private byte[] mAesGcmKey;
    private byte[] mIv;
    private byte[] mEncryptedPaddedDataWithChecksum;
    private byte[] mUnencryptedPaddedData;
    private byte[] mAdditionalAuthData;

    @Before
    public void setUp() {
        mAesGcm16Cipher =
                (IkeCombinedModeCipher)
                        IkeCipher.create(
                                new EncryptionTransform(
                                        SaProposal.ENCRYPTION_ALGORITHM_AES_GCM_16,
                                        SaProposal.KEY_LEN_AES_256));

        mAesGcmKey = TestUtils.hexStringToByteArray(KEY);
        mIv = TestUtils.hexStringToByteArray(IV);
        mEncryptedPaddedDataWithChecksum =
                TestUtils.hexStringToByteArray(ENCRYPTED_PADDED_DATA_WITH_CHECKSUM);
        mUnencryptedPaddedData = TestUtils.hexStringToByteArray(UNENCRYPTED_PADDED_DATA);
        mAdditionalAuthData = TestUtils.hexStringToByteArray(ADDITIONAL_AUTH_DATA);
    }

    @Test
    public void testBuild() throws Exception {
        assertTrue(mAesGcm16Cipher.isAead());
        assertEquals(AES_GCM_IV_LEN, mAesGcm16Cipher.generateIv().length);
    }

    @Test
    public void testGenerateRandomIv() throws Exception {
        assertFalse(Arrays.equals(mAesGcm16Cipher.generateIv(), mAesGcm16Cipher.generateIv()));
    }

    @Test
    public void testEncrypt() throws Exception {
        byte[] calculatedData =
                mAesGcm16Cipher.encrypt(
                        mUnencryptedPaddedData, mAdditionalAuthData, mAesGcmKey, mIv);

        assertArrayEquals(mEncryptedPaddedDataWithChecksum, calculatedData);
    }

    @Test
    public void testDecrypt() throws Exception {
        byte[] calculatedData =
                mAesGcm16Cipher.decrypt(
                        mEncryptedPaddedDataWithChecksum, mAdditionalAuthData, mAesGcmKey, mIv);

        assertArrayEquals(mUnencryptedPaddedData, calculatedData);
    }

    @Test
    public void testEncryptWithWrongKeyLen() throws Exception {
        byte[] encryptionKey = TestUtils.hexStringToByteArray(KEY + "00");

        try {
            mAesGcm16Cipher.encrypt(
                    mUnencryptedPaddedData, mAdditionalAuthData, encryptionKey, mIv);
            fail("Expected to fail because encryption key has wrong length.");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testDecrypWithWrongKey() throws Exception {
        byte[] encryptionKey = new byte[mAesGcmKey.length];
        new Random().nextBytes(encryptionKey);

        try {
            mAesGcm16Cipher.decrypt(
                    mEncryptedPaddedDataWithChecksum, mAdditionalAuthData, encryptionKey, mIv);
            fail("Expected to fail because decryption key is wrong");
        } catch (AEADBadTagException expected) {

        }
    }

    @Test
    public void testBuildIpSecAlgorithm() throws Exception {
        IpSecAlgorithm ipsecAlgorithm = mAesGcm16Cipher.buildIpSecAlgorithmWithKey(mAesGcmKey);

        IpSecAlgorithm expectedIpSecAlgorithm =
                new IpSecAlgorithm(
                        IpSecAlgorithm.AUTH_CRYPT_AES_GCM, mAesGcmKey, AES_GCM_16_CHECKSUM_LEN);

        assertTrue(IpSecAlgorithm.equals(expectedIpSecAlgorithm, ipsecAlgorithm));
    }

    @Test
    public void buildIpSecAlgorithmWithInvalidKey() throws Exception {
        byte[] encryptionKey = TestUtils.hexStringToByteArray(KEY + "00");

        try {
            mAesGcm16Cipher.buildIpSecAlgorithmWithKey(encryptionKey);
            fail("Expected to fail because encryption key has wrong length.");
        } catch (IllegalArgumentException expected) {

        }
    }
}
