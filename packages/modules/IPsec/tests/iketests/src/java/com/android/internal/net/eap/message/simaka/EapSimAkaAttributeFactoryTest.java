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

package com.android.internal.net.eap.message.simaka;

import static com.android.internal.net.TestUtils.hexStringToByteArray;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SKIPPABLE_DATA;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SKIPPABLE_DATA_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SKIPPABLE_INVALID_ATTRIBUTE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaUnsupportedAttributeException;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EapSimAkaUnsupportedAttribute;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class EapSimAkaAttributeFactoryTest {
    private static final int SKIPPABLE_ATTRIBUTE_TYPE = 0xFF;
    private static final int SKIPPABLE_EXPECTED_LENGTH = 8;

    private static final int NON_SKIPPABLE_ATTRIBUTE_TYPE = 0x7F;
    private static final int NON_SKIPPABLE_ATTRIBUTE_LENGTH = 4;

    private EapSimAkaAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = new EapSimAkaAttributeFactory() {};
    }

    @Test
    public void testDecodeInvalidSkippable() throws Exception {
        ByteBuffer byteBuffer = ByteBuffer.wrap(SKIPPABLE_DATA_BYTES);

        EapSimAkaAttribute result = mAttributeFactory.getAttribute(
                SKIPPABLE_ATTRIBUTE_TYPE,
                SKIPPABLE_EXPECTED_LENGTH,
                byteBuffer);
        assertTrue(result instanceof EapSimAkaUnsupportedAttribute);
        EapSimAkaUnsupportedAttribute unsupportedAttribute = (EapSimAkaUnsupportedAttribute) result;
        assertEquals(SKIPPABLE_ATTRIBUTE_TYPE, unsupportedAttribute.attributeType);
        assertEquals(SKIPPABLE_EXPECTED_LENGTH, unsupportedAttribute.lengthInBytes);
        assertArrayEquals(hexStringToByteArray(SKIPPABLE_DATA), unsupportedAttribute.data);
    }

    @Test
    public void testEncodeInvalidSkippable() throws Exception {
        EapSimAkaUnsupportedAttribute unsupportedAttribute = new EapSimAkaUnsupportedAttribute(
                SKIPPABLE_ATTRIBUTE_TYPE,
                SKIPPABLE_EXPECTED_LENGTH,
                hexStringToByteArray(SKIPPABLE_DATA));

        ByteBuffer result = ByteBuffer.allocate(SKIPPABLE_EXPECTED_LENGTH);
        unsupportedAttribute.encode(result);
        assertArrayEquals(SKIPPABLE_INVALID_ATTRIBUTE, result.array());
    }

    @Test
    public void testDecodeInvalidNonSkippable() throws Exception {
        // Unskippable type + length + byte[] represent shortest legitimate attribute: "7F040000"
        ByteBuffer byteBuffer = ByteBuffer.wrap(new byte[2]);

        try {
            mAttributeFactory.getAttribute(
                    NON_SKIPPABLE_ATTRIBUTE_TYPE,
                    NON_SKIPPABLE_ATTRIBUTE_LENGTH,
                    byteBuffer);
            fail("Expected EapSimAkaUnsupportedAttributeException for decoding invalid"
                    + " non-skippable Attribute");
        } catch (EapSimAkaUnsupportedAttributeException expected) {
        }
    }

}
