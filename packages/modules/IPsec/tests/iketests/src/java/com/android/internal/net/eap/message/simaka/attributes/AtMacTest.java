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
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_MAC;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_MAC;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_MAC_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.MAC;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtMac;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttributeFactory;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtMacTest {
    private static final int EXPECTED_LENGTH = 20;
    private static final int MAC_LENGTH = 16;
    private static final byte[] MAC_BYTES = hexStringToByteArray(MAC);
    private static final byte[] INVALID_MAC = {(byte) 1, (byte) 2, (byte) 3};

    private EapSimAkaAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = new EapSimAkaAttributeFactory() {};
    }

    @Test
    public void testConstructor() throws Exception {
        AtMac atMac = new AtMac();
        assertEquals(EAP_AT_MAC, atMac.attributeType);
        assertEquals(EXPECTED_LENGTH, atMac.lengthInBytes);
        assertArrayEquals(new byte[MAC_LENGTH], atMac.mac);
    }

    @Test
    public void testParameterizedConstructor() throws Exception {
        AtMac atMac = new AtMac(MAC_BYTES);
        assertEquals(EAP_AT_MAC, atMac.attributeType);
        assertEquals(EXPECTED_LENGTH, atMac.lengthInBytes);
        assertArrayEquals(MAC_BYTES, atMac.mac);
    }

    @Test
    public void testParameterizedConstructorInvalidMac() {
        try {
            AtMac atMac = new AtMac(INVALID_MAC);
            fail("Expected EapSimAkaInvalidAttributeException for invalid MAC length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_MAC);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        assertTrue(result instanceof AtMac);
        AtMac atMac = (AtMac) result;
        assertEquals(EAP_AT_MAC, atMac.attributeType);
        assertEquals(EXPECTED_LENGTH, atMac.lengthInBytes);
        assertArrayEquals(MAC_BYTES, atMac.mac);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_MAC_INVALID_LENGTH);
        try {
            mAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtMac atMac = new AtMac(MAC_BYTES);

        ByteBuffer result = ByteBuffer.allocate(EXPECTED_LENGTH);
        atMac.encode(result);
        assertArrayEquals(AT_MAC, result.array());
    }
}
