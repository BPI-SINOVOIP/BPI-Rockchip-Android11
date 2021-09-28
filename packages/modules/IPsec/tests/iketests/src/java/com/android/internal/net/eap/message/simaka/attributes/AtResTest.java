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

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_RES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RES_INVALID_RES_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RES_LONG_RES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_RES_SHORT_RES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RES_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapAkaAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRes;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtResTest {
    private EapAkaAttributeFactory mEapAkaAttributeFactory;

    @Before
    public void setUp() {
        mEapAkaAttributeFactory = EapAkaAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RES);
        EapSimAkaAttribute result = mEapAkaAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtRes atRes = (AtRes) result;
        assertEquals(EAP_AT_RES, atRes.attributeType);
        assertEquals(AT_RES.length, atRes.lengthInBytes);
        assertArrayEquals(RES_BYTES, atRes.res);
    }

    @Test
    public void testDecodeInvalidResLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RES_INVALID_RES_LENGTH);
        try {
            mEapAkaAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid RES length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testDecodeShortResLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RES_SHORT_RES);
        try {
            mEapAkaAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for too short RES");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testDecodeLongResLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_RES_LONG_RES);
        try {
            mEapAkaAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for too long RES");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtRes atRes = new AtRes(AT_RES.length, RES_BYTES);

        ByteBuffer result = ByteBuffer.allocate(AT_RES.length);
        atRes.encode(result);
        assertArrayEquals(AT_RES, result.array());
    }

    @Test
    public void testGetAtRes() throws Exception {
        AtRes atRes = AtRes.getAtRes(RES_BYTES);

        ByteBuffer result = ByteBuffer.allocate(AT_RES.length);
        atRes.encode(result);
        assertArrayEquals(AT_RES, result.array());
    }

    @Test
    public void testIsValidResLen() {
        // valid RES length: 4 <= RES length <= 16
        assertTrue(AtRes.isValidResLen(5));
        assertFalse(AtRes.isValidResLen(0));
        assertFalse(AtRes.isValidResLen(20));
    }
}
