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

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_IDENTITY;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_IDENTITY;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.IDENTITY;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtIdentity;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAttributeFactory;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtIdentityTest {
    private EapSimAkaAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = new EapSimAkaAttributeFactory() {};
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_IDENTITY);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        assertTrue(result instanceof AtIdentity);
        AtIdentity atIdentity = (AtIdentity) result;
        assertEquals(EAP_AT_IDENTITY, atIdentity.attributeType);
        assertEquals(AT_IDENTITY.length, atIdentity.lengthInBytes);
        assertArrayEquals(IDENTITY, atIdentity.identity);
    }

    @Test
    public void testEncode() throws Exception {
        AtIdentity atIdentity = new AtIdentity(AT_IDENTITY.length, IDENTITY);
        ByteBuffer result = ByteBuffer.allocate(AT_IDENTITY.length);
        atIdentity.encode(result);

        assertArrayEquals(AT_IDENTITY, result.array());
    }

    @Test
    public void testGetAtIdentity() throws Exception {
        AtIdentity atIdentity = AtIdentity.getAtIdentity(IDENTITY);

        assertArrayEquals(IDENTITY, atIdentity.identity);

        ByteBuffer buffer = ByteBuffer.allocate(atIdentity.lengthInBytes);
        atIdentity.encode(buffer);
        buffer.rewind();

        EapSimAkaAttribute eapSimAkaAttribute =
                EapSimAttributeFactory.getInstance().getAttribute(buffer);
        assertTrue(eapSimAkaAttribute instanceof AtIdentity);
        AtIdentity newAtIdentity = (AtIdentity) eapSimAkaAttribute;
        assertEquals(atIdentity.lengthInBytes, newAtIdentity.lengthInBytes);
        assertArrayEquals(atIdentity.identity, newAtIdentity.identity);
    }
}
