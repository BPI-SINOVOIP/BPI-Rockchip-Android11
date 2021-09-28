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

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_RAND;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RAND_SIM;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RAND_SIM_DUPLICATE_RANDS;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RAND_SIM_INVALID_NUM_RANDS;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_1;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RAND_2;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.exceptions.simaka.EapSimInvalidAtRandException;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRandSim;
import com.android.internal.net.eap.message.simaka.EapSimAttributeFactory;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtRandSimTest {
    private static final int EXPECTED_NUM_RANDS = 2;

    private EapSimAttributeFactory mEapSimAttributeFactory;

    @Before
    public void setUp() {
        mEapSimAttributeFactory = EapSimAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RAND_SIM);
        EapSimAkaAttribute result = mEapSimAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        assertTrue(result instanceof AtRandSim);
        AtRandSim atRandSim = (AtRandSim) result;
        assertEquals(EAP_AT_RAND, atRandSim.attributeType);
        assertEquals(AT_RAND_SIM.length, atRandSim.lengthInBytes);
        assertEquals(EXPECTED_NUM_RANDS, atRandSim.rands.size());
        assertArrayEquals(hexStringToByteArray(RAND_1), atRandSim.rands.get(0));
        assertArrayEquals(hexStringToByteArray(RAND_2), atRandSim.rands.get(1));
    }

    @Test
    public void testDecodeInvalidNumRands() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RAND_SIM_INVALID_NUM_RANDS);
        try {
            mEapSimAttributeFactory.getAttribute(input);
            fail("Expected EapSimInvalidAtRandException for invalid number of RANDs");
        } catch (EapSimInvalidAtRandException expected) {
        }
    }

    @Test
    public void testDecodeDuplicateRands() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RAND_SIM_DUPLICATE_RANDS);
        try {
            mEapSimAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for duplicate RANDs");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        byte[][] expectedRands = new byte[][] {
                hexStringToByteArray(RAND_1),
                hexStringToByteArray(RAND_2)
        };
        AtRandSim atRandSim = new AtRandSim(AT_RAND_SIM.length, expectedRands);

        ByteBuffer result = ByteBuffer.allocate(AT_RAND_SIM.length);
        atRandSim.encode(result);
        assertArrayEquals(AT_RAND_SIM, result.array());
    }
}
