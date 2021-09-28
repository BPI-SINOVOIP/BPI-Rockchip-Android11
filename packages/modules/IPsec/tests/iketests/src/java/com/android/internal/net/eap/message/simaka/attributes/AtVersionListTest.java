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
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_VERSION_LIST;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_VERSION_LIST;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_VERSION_LIST_INVALID_LENGTH;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.VERSION;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.internal.net.eap.exceptions.simaka.EapSimAkaInvalidAttributeException;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtVersionList;
import com.android.internal.net.eap.message.simaka.EapSimAttributeFactory;

import org.junit.Before;
import org.junit.Test;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;

public class AtVersionListTest {
    private static final int EXPECTED_LENGTH = 8;
    private static final List<Integer> EXPECTED_VERSIONS = Arrays.asList(1);

    private EapSimAttributeFactory mEapSimAttributeFactory;

    @Before
    public void setUp() {
        mEapSimAttributeFactory = EapSimAttributeFactory.getInstance();
    }

    @Test
    public void testDecode() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_VERSION_LIST);
        EapSimAkaAttribute result = mEapSimAttributeFactory.getAttribute(input);

        assertFalse(input.hasRemaining());
        assertTrue(result instanceof AtVersionList);
        AtVersionList atVersionList = (AtVersionList) result;
        assertEquals(EAP_AT_VERSION_LIST, atVersionList.attributeType);
        assertEquals(EXPECTED_LENGTH, atVersionList.lengthInBytes);
        assertEquals(EXPECTED_VERSIONS, atVersionList.versions);
    }

    @Test
    public void testDecodeInvalidActualLength() throws Exception {
        ByteBuffer input = ByteBuffer.wrap(AT_VERSION_LIST_INVALID_LENGTH);
        try {
            mEapSimAttributeFactory.getAttribute(input);
            fail("Expected EapSimAkaInvalidAttributeException for invalid actual list length");
        } catch (EapSimAkaInvalidAttributeException expected) {
        }
    }

    @Test
    public void testEncode() throws Exception {
        AtVersionList atVersionList = new AtVersionList(EXPECTED_LENGTH, hexStringToInt(VERSION));
        ByteBuffer result = ByteBuffer.allocate(EXPECTED_LENGTH);

        atVersionList.encode(result);
        assertArrayEquals(AT_VERSION_LIST, result.array());
    }
}
