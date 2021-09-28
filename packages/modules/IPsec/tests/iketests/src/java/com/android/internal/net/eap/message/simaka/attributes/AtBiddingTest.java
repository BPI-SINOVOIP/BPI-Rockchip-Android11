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

import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_BIDDING;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_BIDDING_DOES_NOT_SUPPORT_AKA_PRIME;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_BIDDING_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_BIDDING_SUPPORTS_AKA_PRIME;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapAkaAttributeFactory;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtBidding;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtBiddingTest {
    private EapAkaAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = EapAkaAttributeFactory.getInstance();
    }

    @Test
    public void testDecodeServerSupportsAkaPrime() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_BIDDING_SUPPORTS_AKA_PRIME);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtBidding atBidding = (AtBidding) result;
        assertEquals(EAP_AT_BIDDING, atBidding.attributeType);
        assertEquals(AT_BIDDING_SUPPORTS_AKA_PRIME.length, atBidding.lengthInBytes);
        assertTrue(atBidding.doesServerSupportEapAkaPrime);
    }

    @Test
    public void testDecodeDoesNotSupportAkaPrime() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_BIDDING_DOES_NOT_SUPPORT_AKA_PRIME);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        AtBidding atBidding = (AtBidding) result;
        assertEquals(EAP_AT_BIDDING, atBidding.attributeType);
        assertEquals(AT_BIDDING_DOES_NOT_SUPPORT_AKA_PRIME.length, atBidding.lengthInBytes);
        assertFalse(atBidding.doesServerSupportEapAkaPrime);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_BIDDING_INVALID_LENGTH);
        try {
            mAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncodeServerSupportsAkaPrime() throws Exception {
        AtBidding atBidding = new AtBidding(true);

        ByteBuffer result = ByteBuffer.allocate(AT_BIDDING_SUPPORTS_AKA_PRIME.length);
        atBidding.encode(result);
        assertArrayEquals(AT_BIDDING_SUPPORTS_AKA_PRIME, result.array());
    }

    @Test
    public void testEncodeDoesNotSupportAkaPrime() throws Exception {
        AtBidding atBidding = new AtBidding(false);

        ByteBuffer result = ByteBuffer.allocate(AT_BIDDING_DOES_NOT_SUPPORT_AKA_PRIME.length);
        atBidding.encode(result);
        assertArrayEquals(AT_BIDDING_DOES_NOT_SUPPORT_AKA_PRIME, result.array());
    }
}
