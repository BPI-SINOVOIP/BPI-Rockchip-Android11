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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.net.IpSecAlgorithm;
import android.net.ipsec.ike.SaProposal;

import com.android.internal.net.TestUtils;
import com.android.internal.net.ipsec.ike.message.IkeSaPayload.IntegrityTransform;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.util.Arrays;

@RunWith(JUnit4.class)
public final class IkeMacIntegrityTest {
    private static final String DATA_TO_AUTH_HEX_STRING =
            "5f54bf6d8b48e6e1909232b3d1edcb5c2e20230800000001000000ec"
                    + "230000d0b9132b7bb9f658dfdc648e5017a6322a030c316c"
                    + "e55f365760d46426ce5cfc78bd1ed9abff63eb9594c1bd58"
                    + "46de333ecd3ea2b705d18293b130395300ba92a351041345"
                    + "0a10525cea51b2753b4e92b081fd78d995659a98f742278f"
                    + "f9b8fd3e21554865c15c79a5134d66b2744966089e416c60"
                    + "a274e44a9a3f084eb02f3bdce1e7de9de8d9a62773ab563b"
                    + "9a69ba1db03c752acb6136452b8a86c41addb4210d68c423"
                    + "efed80e26edca5fa3fe5d0a5ca9375ce332c474b93fb1fa3"
                    + "59eb4e81";
    private static final String INTEGRITY_KEY_HEX_STRING =
            "554fbf5a05b7f511e05a30ce23d874db9ef55e51";
    private static final String CHECKSUM_HEX_STRING = "ae6e0f22abdad69ba8007d50";

    private IkeMacIntegrity mHmacSha1IntegrityMac;
    private byte[] mHmacSha1IntegrityKey;

    private byte[] mDataToAuthenticate;

    private IkeMacIntegrity mAes128XCbcIntgerityMac;
    private static final String AUTH_AES128XCBC_KEY_HEX_STRING = "000102030405060708090a0b0c0d0e0f";
    private static final String AUTH_AES128XCBC_DATA_TO_SIGN_HEX_STRING =
            "000102030405060708090a0b0c0d0e0f10111213";
    private static final String AUTH_AES128XCBC_CALCULATED_MAC_HEX_STRING =
            "47f51b4564966215b8985c63";

    private static final String AUTH_AES128XCBC_KEY_HEX_STRING1 =
            "000102030405060708090a0b0c0d0e0f";
    private static final String AUTH_AES128XCBC_DATA_TO_SIGN_HEX_STRING1 =
            "000102030405060708090a0b0c0d0e0f";
    private static final String AUTH_AES128XCBC_CALCULATED_MAC_HEX_STRING1 =
            "d2a246fa349b68a79998a439";

    @Before
    public void setUp() throws Exception {
        mHmacSha1IntegrityMac =
                IkeMacIntegrity.create(
                        new IntegrityTransform(SaProposal.INTEGRITY_ALGORITHM_HMAC_SHA1_96));
        mHmacSha1IntegrityKey = TestUtils.hexStringToByteArray(INTEGRITY_KEY_HEX_STRING);

        mDataToAuthenticate = TestUtils.hexStringToByteArray(DATA_TO_AUTH_HEX_STRING);
        mAes128XCbcIntgerityMac =
                IkeMacIntegrity.create(
                        new IntegrityTransform(SaProposal.INTEGRITY_ALGORITHM_AES_XCBC_96));
    }

    @Test
    public void testGenerateChecksum() throws Exception {
        byte[] calculatedChecksum =
                mHmacSha1IntegrityMac.generateChecksum(mHmacSha1IntegrityKey, mDataToAuthenticate);

        byte[] expectedChecksum = TestUtils.hexStringToByteArray(CHECKSUM_HEX_STRING);
        assertArrayEquals(expectedChecksum, calculatedChecksum);
    }

    @Test
    public void testGenerateChecksumWithDifferentKey() throws Exception {
        byte[] integrityKey = mHmacSha1IntegrityKey.clone();
        integrityKey[0]++;

        byte[] calculatedChecksum =
                mHmacSha1IntegrityMac.generateChecksum(integrityKey, mDataToAuthenticate);

        byte[] expectedChecksum = TestUtils.hexStringToByteArray(CHECKSUM_HEX_STRING);
        assertFalse(Arrays.equals(expectedChecksum, calculatedChecksum));
    }

    @Test
    public void testGenerateChecksumWithInvalidKey() throws Exception {
        byte[] integrityKey = TestUtils.hexStringToByteArray(INTEGRITY_KEY_HEX_STRING + "0000");

        try {
            byte[] calculatedChecksum =
                    mHmacSha1IntegrityMac.generateChecksum(integrityKey, mDataToAuthenticate);
            fail("Expected to fail due to invalid authentication key.");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testBuildIpSecAlgorithm() throws Exception {
        IpSecAlgorithm ipsecAlgorithm =
                mHmacSha1IntegrityMac.buildIpSecAlgorithmWithKey(mHmacSha1IntegrityKey);

        IpSecAlgorithm expectedIpSecAlgorithm =
                new IpSecAlgorithm(IpSecAlgorithm.AUTH_HMAC_SHA1, mHmacSha1IntegrityKey, 96);

        assertTrue(IpSecAlgorithm.equals(expectedIpSecAlgorithm, ipsecAlgorithm));
    }

    @Test
    public void buildIpSecAlgorithmWithInvalidKey() throws Exception {
        byte[] encryptionKey = TestUtils.hexStringToByteArray(INTEGRITY_KEY_HEX_STRING + "00");

        try {
            mHmacSha1IntegrityMac.buildIpSecAlgorithmWithKey(encryptionKey);

            fail("Expected to fail due to integrity key with wrong length.");
        } catch (IllegalArgumentException expected) {

        }
    }

    @Test
    public void testSignBytesAuthAes128XCbc() throws Exception {
        byte[] skpBytes = TestUtils.hexStringToByteArray(AUTH_AES128XCBC_KEY_HEX_STRING);
        byte[] dataBytes = TestUtils.hexStringToByteArray(AUTH_AES128XCBC_DATA_TO_SIGN_HEX_STRING);

        byte[] calculatedBytes = mAes128XCbcIntgerityMac.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(AUTH_AES128XCBC_CALCULATED_MAC_HEX_STRING);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }

    @Test
    public void testSignBytesAuthAes128XCbcWith16ByteInput() throws Exception {
        // 16-byte is a multiple of aes block size. Hence key2 will be used instead of key3
        byte[] skpBytes = TestUtils.hexStringToByteArray(AUTH_AES128XCBC_KEY_HEX_STRING1);
        byte[] dataBytes = TestUtils.hexStringToByteArray(AUTH_AES128XCBC_DATA_TO_SIGN_HEX_STRING1);

        byte[] calculatedBytes = mAes128XCbcIntgerityMac.signBytes(skpBytes, dataBytes);

        byte[] expectedBytes =
                TestUtils.hexStringToByteArray(AUTH_AES128XCBC_CALCULATED_MAC_HEX_STRING1);
        assertArrayEquals(expectedBytes, calculatedBytes);
    }
}
