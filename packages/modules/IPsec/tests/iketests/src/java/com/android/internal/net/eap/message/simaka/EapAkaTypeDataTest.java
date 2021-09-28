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
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_CHALLENGE_RESPONSE_MAC_BYTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_CHALLENGE_RESPONSE_TYPE_DATA;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_AKA_IDENTITY_REQUEST;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.INVALID_SUBTYPE;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_IDENTITY;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_ANY_ID_REQ;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_AUTN;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_CHECKCODE;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_MAC;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_RAND;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_RES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.RES_BYTES;

import static junit.framework.TestCase.fail;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.internal.net.eap.message.simaka.EapAkaTypeData.EapAkaTypeDataDecoder;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAnyIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAutn;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtMac;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRandAka;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRes;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EapSimAkaUnsupportedAttribute;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map.Entry;

public class EapAkaTypeDataTest {
    private static final int UNABLE_TO_PROCESS_CODE = 0;
    private static final int INVALID_SUBTYPE_INT = -1;

    private static final int EAP_AT_TRUST_IND = 139;
    private static final String RAND = "7A1FCDC0034BA1227E7B9FCEAFD47D53";
    private static final byte[] RAND_BYTES = hexStringToByteArray(RAND);
    private static final String AUTN = "000102030405060708090A0B0C0D0E0F";
    private static final byte[] AUTN_BYTES = hexStringToByteArray(AUTN);
    private static final String MAC = "95FEB9E70427F34B4FAC8F2C7A65A302";
    private static final byte[] MAC_BYTES = hexStringToByteArray(MAC);
    private static final byte[] EAP_AKA_REQUEST =
            hexStringToByteArray(
                    "010000" // Challenge | 2B padding
                            + "01050000" + RAND // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "8B010002" // AT_RESULT_IND attribute (TS 124 302#8.2.3.1)
                            + "0B050000" + MAC // AT_MAC attribute
                            + "86010000"); // AT_CHECKCODE attribute

    private EapAkaTypeDataDecoder mEapAkaTypeDataDecoder;

    @Before
    public void setUp() {
        mEapAkaTypeDataDecoder = EapAkaTypeData.getEapAkaTypeDataDecoder();
    }

    @Test
    public void testDecode() {
        DecodeResult<EapAkaTypeData> result =
                mEapAkaTypeDataDecoder.decode(EAP_AKA_CHALLENGE_RESPONSE_TYPE_DATA);

        assertTrue(result.isSuccessfulDecode());
        EapAkaTypeData eapAkaTypeData = result.eapTypeData;
        assertEquals(EAP_AKA_CHALLENGE, eapAkaTypeData.eapSubtype);

        // also check Map entries (needs to match input order)
        Iterator<Entry<Integer, EapSimAkaAttribute>> itr =
                eapAkaTypeData.attributeMap.entrySet().iterator();
        Entry<Integer, EapSimAkaAttribute> entry = itr.next();
        assertEquals(EAP_AT_RES, (int) entry.getKey());
        assertArrayEquals(RES_BYTES, ((AtRes) entry.getValue()).res);

        entry = itr.next();
        assertEquals(EAP_AT_MAC, (int) entry.getKey());
        assertArrayEquals(EAP_AKA_CHALLENGE_RESPONSE_MAC_BYTES, ((AtMac) entry.getValue()).mac);

        assertFalse(itr.hasNext());
    }

    @Test
    public void testDecodeWithOptionalAttributes() {
        DecodeResult<EapAkaTypeData> result = mEapAkaTypeDataDecoder.decode(EAP_AKA_REQUEST);

        assertTrue(result.isSuccessfulDecode());
        EapAkaTypeData eapAkaTypeData = result.eapTypeData;
        assertEquals(EAP_AKA_CHALLENGE, eapAkaTypeData.eapSubtype);

        // also check Map entries (needs to match input order)
        Iterator<Entry<Integer, EapSimAkaAttribute>> itr =
                eapAkaTypeData.attributeMap.entrySet().iterator();
        Entry<Integer, EapSimAkaAttribute> entry = itr.next();
        assertEquals(EAP_AT_RAND, (int) entry.getKey());
        assertArrayEquals(RAND_BYTES, ((AtRandAka) entry.getValue()).rand);

        entry = itr.next();
        assertEquals(EAP_AT_AUTN, (int) entry.getKey());
        assertArrayEquals(AUTN_BYTES, ((AtAutn) entry.getValue()).autn);

        entry = itr.next();
        assertEquals(EAP_AT_TRUST_IND, (int) entry.getKey());
        assertTrue(entry.getValue() instanceof EapSimAkaUnsupportedAttribute);

        entry = itr.next();
        assertEquals(EAP_AT_MAC, (int) entry.getKey());
        assertArrayEquals(MAC_BYTES, ((AtMac) entry.getValue()).mac);

        entry = itr.next();
        assertEquals(EAP_AT_CHECKCODE, (int) entry.getKey());
        assertTrue(entry.getValue() instanceof EapSimAkaUnsupportedAttribute);

        assertFalse(itr.hasNext());
    }

    @Test
    public void testDecodeInvalidSubtype() {
        DecodeResult<EapAkaTypeData> result = mEapAkaTypeDataDecoder.decode(INVALID_SUBTYPE);
        assertFalse(result.isSuccessfulDecode());
        assertEquals(UNABLE_TO_PROCESS_CODE, result.atClientErrorCode.errorCode);
    }

    @Test
    public void testEncode() throws Exception {
        LinkedHashMap<Integer, EapSimAkaAttribute> attributes = new LinkedHashMap<>();
        attributes.put(EAP_AT_ANY_ID_REQ, new AtAnyIdReq());
        EapAkaTypeData eapAkaTypeData = new EapAkaTypeData(EAP_AKA_IDENTITY, attributes);

        byte[] result = eapAkaTypeData.encode();
        assertArrayEquals(EAP_AKA_IDENTITY_REQUEST, result);
    }

    @Test
    public void testConstructorInvalidSubtype() throws Exception {
        try {
            new EapAkaTypeData(INVALID_SUBTYPE_INT, Arrays.asList(new AtAnyIdReq()));
            fail("Expected IllegalArgumentException for invalid subtype");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testConstructorDuplicateAttributes() throws Exception {
        try {
            new EapAkaTypeData(EAP_AKA_IDENTITY, Arrays.asList(new AtAnyIdReq(), new AtAnyIdReq()));
            fail("Expected IllegalArgumentException for duplicate attributes");
        } catch (IllegalArgumentException expected) {
        }
    }
}
