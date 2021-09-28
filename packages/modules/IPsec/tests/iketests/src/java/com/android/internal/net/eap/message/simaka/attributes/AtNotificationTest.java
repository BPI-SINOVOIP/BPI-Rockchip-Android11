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

import static com.android.internal.net.TestUtils.hexStringToInt;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNotification.GENERAL_FAILURE_POST_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_NOTIFICATION;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_NOTIFICATION;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_NOTIFICATION_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_NOTIFICATION_INVALID_STATE;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.NOTIFICATION_CODE;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtNotification;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttributeFactory;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;

public class AtNotificationTest {
    private static final int EXPECTED_LENGTH = 4;
    private static final int UNKNOWN_CODE = 0xA0FF;

    private EapSimAkaAttributeFactory mAttributeFactory;

    @Before
    public void setUp() {
        mAttributeFactory = new EapSimAkaAttributeFactory() {};
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_NOTIFICATION);
        EapSimAkaAttribute result = mAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        assertTrue(result instanceof AtNotification);
        AtNotification atNotification = (AtNotification) result;
        assertEquals(EAP_AT_NOTIFICATION, atNotification.attributeType);
        assertEquals(EXPECTED_LENGTH, atNotification.lengthInBytes);
        assertTrue(atNotification.isSuccessCode);
        assertFalse(atNotification.isPreSuccessfulChallenge);
        assertEquals(hexStringToInt(NOTIFICATION_CODE), atNotification.notificationCode);
    }

    @Test
    public void testDecodeInvalidLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_NOTIFICATION_INVALID_LENGTH);
        try {
            mAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid attribute length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testDecodeInvalidState() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_NOTIFICATION_INVALID_STATE);
        try {
            mAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid state");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtNotification atNotification = new AtNotification(hexStringToInt(NOTIFICATION_CODE));
        ByteBuffer result = ByteBuffer.allocate(EXPECTED_LENGTH);

        atNotification.encode(result);
        assertArrayEquals(AT_NOTIFICATION, result.array());
    }

    @Test
    public void testToString() throws Exception {
        AtNotification knownCode = new AtNotification(GENERAL_FAILURE_POST_CHALLENGE);
        AtNotification unknownCode = new AtNotification(UNKNOWN_CODE);

        assertNotNull(knownCode.toString());
        assertNotNull(unknownCode.toString());
        assertNotEquals(knownCode.toString(), unknownCode.toString());
    }
}
