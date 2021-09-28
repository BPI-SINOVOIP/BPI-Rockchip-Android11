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

package com.android.internal.net.eap.crypto;

import static com.android.internal.net.TestUtils.hexStringToByteArray;

import static org.junit.Assert.assertArrayEquals;

import org.junit.Before;
import org.junit.Test;

/**
 * HmacSha256ByteSignerTest tests that {@link HmacSha256ByteSigner} correctly signs data using the
 * HMAC-SHA-256 algorithm.
 *
 * <p>These test vectors are defined in RFC 4231#4.
 *
 * @see <a href="https://tools.ietf.org/html/rfc4231#section-4">Test Vectors</a>
 */
public class HmacSha256ByteSignerTest {
    private static final String[] KEYS = {
        "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
        "4a656665",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "0102030405060708090a0b0c0d0e0f10111213141516171819",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaa",
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaa"
    };
    private static final String[] DATA = {
        "4869205468657265",
        "7768617420646f2079612077616e7420666f72206e6f7468696e673f",
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"
                + "dddddddddddddddddddddddddddddddddddd",
        "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
                + "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd",
        "54657374205573696e67204c6172676572205468616e20426c6f636b2d53697a"
                + "65204b6579202d2048617368204b6579204669727374",
        "5468697320697320612074657374207573696e672061206c6172676572207468"
                + "616e20626c6f636b2d73697a65206b657920616e642061206c61726765722074"
                + "68616e20626c6f636b2d73697a6520646174612e20546865206b6579206e6565"
                + "647320746f20626520686173686564206265666f7265206265696e6720757365"
                + "642062792074686520484d414320616c676f726974686d2e"
    };
    private static final String[] MACS = {
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe",
        "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
        "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"
    };

    private HmacSha256ByteSigner mMacByteSigner;

    @Before
    public void setUp() {
        mMacByteSigner = HmacSha256ByteSigner.getInstance();
    }

    @Test
    public void testSignBytes() {
        for (int i = 0; i < KEYS.length; i++) {
            byte[] key = hexStringToByteArray(KEYS[i]);
            byte[] data = hexStringToByteArray(DATA[i]);

            byte[] expected = hexStringToByteArray(MACS[i]);

            assertArrayEquals(expected, mMacByteSigner.signBytes(key, data));
        }
    }
}
