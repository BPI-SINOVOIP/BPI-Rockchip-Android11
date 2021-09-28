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

package com.android.internal.net.eap.crypto;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.fail;

import com.android.internal.net.TestUtils;

import org.junit.Before;
import org.junit.Test;

/**
 * The test vectors in this file come directly from NIST:
 *
 * <p>The original link to the test vectors was here:
 * http://csrc.nist.gov/publications/fips/fips186-2/fips186-2-change1.pdf
 *
 * <p>But has since been removed. A cached copy was found here:
 * https://web.archive.org/web/20041031202951/http://www.csrc.nist.gov/CryptoToolkit/dss/Examples-
 * 1024bit.pdf
 */
public final class Fips186_2PrfTest {
    private static final String SEED = "bd029bbe7f51960bcf9edb2b61f06f0feb5a38b6";
    private static final String EXPECTED_RESULT =
            "2070b3223dba372fde1c0ffc7b2e3b498b2606143c6c18bacb0f6c55babb13788e20d737a3275116";

    private Fips186_2Prf mFipsPrf;

    @Before
    public void setUp() {
        mFipsPrf = new Fips186_2Prf();
    }

    @Test
    public void testFips186_2Prf_Invalid_Seed() throws Exception {
        try {
            mFipsPrf.getRandom(new byte[0], 40);
            fail("Expected exception for invalid length seed");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testFips186_2Prf() throws Exception {
        byte[] seed = TestUtils.hexStringToByteArray(SEED);
        byte[] actual = mFipsPrf.getRandom(seed, 40);

        assertArrayEquals(TestUtils.hexStringToByteArray(EXPECTED_RESULT), actual);
    }

    // TODO: (b/136177143) Add more test vectors
}
