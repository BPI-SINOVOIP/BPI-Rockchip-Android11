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

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_AUTN;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_AUTN;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_AUTN_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AUTN_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapAkaAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAutn;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtAutnTest {
    private EapAkaAttributeFactory mEapAkaAttributeFactory;

    @Before
    public void setUp() {
        mEapAkaAttributeFactory = EapAkaAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_AUTN);
        EapSimAkaAttribute result = mEapAkaAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtAutn atAutn = (AtAutn) result;
        assertEquals(EAP_AT_AUTN, atAutn.attributeType);
        assertEquals(AT_AUTN.length, atAutn.lengthInBytes);
        assertArrayEquals(AUTN_BYTES, atAutn.autn);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_AUTN_INVALID_LENGTH);
        try {
            mEapAkaAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtAutn atAutn = new AtAutn(AUTN_BYTES);

        ByteBuffer result = ByteBuffer.allocate(AT_AUTN.length);
        atAutn.encode(result);
        assertArrayEquals(AT_AUTN, result.array());
    }
}
