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
package com.android.internal.net.ipsec.ike.testmode;

import static android.net.ipsec.ike.SaProposal.DH_GROUP_2048_BIT_MODP;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;

import com.android.internal.net.ipsec.ike.message.IkeKePayload;
import com.android.internal.net.ipsec.ike.utils.RandomnessFactory;

import org.junit.Test;

import java.util.Arrays;

import javax.crypto.spec.DHPrivateKeySpec;

public final class DeterministicSecureRandomTest {
    private static final int BYTE_ARRAY_LEN = 20;

    @Test
    public void testDeterministicSecureRandomNonRepeating() throws Exception {
        DeterministicSecureRandom secureRandom = new DeterministicSecureRandom();

        assertFalse(
                Arrays.equals(
                        secureRandom.generateSeed(BYTE_ARRAY_LEN),
                        secureRandom.generateSeed(BYTE_ARRAY_LEN)));
        assertNotEquals(secureRandom.nextLong(), secureRandom.nextLong());

        byte[] randomBytesOne = new byte[BYTE_ARRAY_LEN];
        byte[] randomBytesTwo = new byte[BYTE_ARRAY_LEN];
        secureRandom.nextBytes(randomBytesOne);
        secureRandom.nextBytes(randomBytesTwo);
        assertFalse(Arrays.equals(randomBytesOne, randomBytesTwo));
    }

    @Test
    public void testTwoDeterministicSecureRandomsAreDeterministic() throws Exception {
        DeterministicSecureRandom srOne = new DeterministicSecureRandom();
        DeterministicSecureRandom srTwo = new DeterministicSecureRandom();

        assertArrayEquals(srOne.generateSeed(BYTE_ARRAY_LEN), srTwo.generateSeed(BYTE_ARRAY_LEN));
        assertEquals(srOne.nextLong(), srTwo.nextLong());

        byte[] randomBytesOne = new byte[BYTE_ARRAY_LEN];
        byte[] randomBytesTwo = new byte[BYTE_ARRAY_LEN];
        srOne.nextBytes(randomBytesOne);
        srTwo.nextBytes(randomBytesTwo);
        assertArrayEquals(randomBytesOne, randomBytesTwo);
    }

    @Test
    public void testDeterministicSecureRandomInKePayload() throws Exception {
        RandomnessFactory rFactory = mock(RandomnessFactory.class);
        doAnswer(
            (invocation) -> {
                return new DeterministicSecureRandom();
            })
            .when(rFactory)
            .getRandom();

        IkeKePayload kePayloadOne = new IkeKePayload(DH_GROUP_2048_BIT_MODP, rFactory);
        IkeKePayload kePayloadTwo = new IkeKePayload(DH_GROUP_2048_BIT_MODP, rFactory);
        assertArrayEquals(kePayloadOne.keyExchangeData, kePayloadTwo.keyExchangeData);

        DHPrivateKeySpec localPrivateKeyOne = kePayloadOne.localPrivateKey;
        DHPrivateKeySpec localPrivateKeyTwo = kePayloadTwo.localPrivateKey;
        assertEquals(localPrivateKeyOne.getG(), localPrivateKeyTwo.getG());
        assertEquals(localPrivateKeyOne.getP(), localPrivateKeyTwo.getP());
        assertEquals(localPrivateKeyOne.getX(), localPrivateKeyTwo.getX());
    }
}
