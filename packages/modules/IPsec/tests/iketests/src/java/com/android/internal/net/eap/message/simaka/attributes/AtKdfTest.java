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

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_KDF;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_KDF;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_KDF_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.KDF_VERSION;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapAkaPrimeAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtKdf;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtKdfTest {
    private EapAkaPrimeAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = EapAkaPrimeAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_KDF);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtKdf atKdf = (AtKdf) result;
        assertEquals(EAP_AT_KDF, atKdf.attributeType);
        assertEquals(AT_KDF.length, atKdf.lengthInBytes);
        assertEquals(KDF_VERSION, atKdf.kdf);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_KDF_INVALID_LENGTH);
        try {
            mAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtKdf atKdf = new AtKdf(KDF_VERSION);
        ByteBuffer result = ByteBuffer.allocate(AT_KDF.length);

        atKdf.encode(result);
        assertArrayEquals(AT_KDF, result.array());
        assertFalse(result.hasRemaining());
    }
}
