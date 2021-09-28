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

package com.android.internal.net.eap.message.simaka.attributes;

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_RAND;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RAND_AKA;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RAND_AKA_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_1_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapAkaAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRandAka;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtRandAkaTest {
    private EapAkaAttributeFactory mEapAkaAttributeFactory;

    @Before
    public void setUp() {
        mEapAkaAttributeFactory = EapAkaAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RAND_AKA);
        EapSimAkaAttribute result = mEapAkaAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtRandAka atRandAka = (AtRandAka) result;
        assertEquals(EAP_AT_RAND, atRandAka.attributeType);
        assertEquals(AT_RAND_AKA.length, atRandAka.lengthInBytes);
        assertArrayEquals(RAND_1_BYTES, atRandAka.rand);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RAND_AKA_INVALID_LENGTH);
        try {
            mEapAkaAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtRandAka atRandAka = new AtRandAka(RAND_1_BYTES);

        ByteBuffer result = ByteBuffer.allocate(AT_RAND_AKA.length);
        atRandAka.encode(result);
        assertArrayEquals(AT_RAND_AKA, result.array());
    }
}
