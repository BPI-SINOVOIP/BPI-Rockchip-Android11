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

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_AUTS;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_AUTS;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_AUTS_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AUTS_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapAkaAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAuts;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtAutsTest {
    private EapAkaAttributeFactory mEapAkaAttributeFactory;

    @Before
    public void setUp() {
        mEapAkaAttributeFactory = EapAkaAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_AUTS);
        EapSimAkaAttribute result = mEapAkaAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtAuts atAuts = (AtAuts) result;
        assertEquals(EAP_AT_AUTS, atAuts.attributeType);
        assertEquals(AT_AUTS.length, atAuts.lengthInBytes);
        assertArrayEquals(AUTS_BYTES, atAuts.auts);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_AUTS_INVALID_LENGTH);
        try {
            mEapAkaAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtAuts atAuts = new AtAuts(AUTS_BYTES);

        ByteBuffer result = ByteBuffer.allocate(AT_AUTS.length);
        atAuts.encode(result);
        assertArrayEquals(AT_AUTS, result.array());
    }
}
