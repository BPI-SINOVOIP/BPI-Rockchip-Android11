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

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_KDF_INPUT;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_KDF_INPUT;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_KDF_INPUT_EMPTY_NETWORK_NAME;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.NETWORK_NAME_BYTES;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import com.android.internal.net.eap.message.simaka.EapAkaPrimeAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtKdfInput;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtKdfInputTest {
    private EapAkaPrimeAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = EapAkaPrimeAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_KDF_INPUT);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtKdfInput atKdfInput = (AtKdfInput) result;
        assertEquals(EAP_AT_KDF_INPUT, atKdfInput.attributeType);
        assertEquals(AT_KDF_INPUT.length, atKdfInput.lengthInBytes);
        assertArrayEquals(NETWORK_NAME_BYTES, atKdfInput.networkName);
    }

    @Test
    public void testDecodeEmptyNetworkName() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_KDF_INPUT_EMPTY_NETWORK_NAME);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtKdfInput atKdfInput = (AtKdfInput) result;
        assertEquals(EAP_AT_KDF_INPUT, atKdfInput.attributeType);
        assertEquals(AT_KDF_INPUT_EMPTY_NETWORK_NAME.length, atKdfInput.lengthInBytes);
        assertArrayEquals(new byte[0], atKdfInput.networkName);
    }

    @Test
    public void testEncode() throws Exception {
        AtKdfInput atKdfInput = new AtKdfInput(AT_KDF_INPUT.length, NETWORK_NAME_BYTES);
        ByteBuffer result = ByteBuffer.allocate(AT_KDF_INPUT.length);

        atKdfInput.encode(result);
        assertArrayEquals(AT_KDF_INPUT, result.array());
        assertFalse(result.hasRemaining());
    }
}
