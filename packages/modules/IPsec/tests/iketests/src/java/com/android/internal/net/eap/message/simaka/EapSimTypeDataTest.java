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

import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SIM_START_DUPLICATE_ATTRIBUTES;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.EAP_SIM_START_SUBTYPE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.INVALID_SUBTYPE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.SHORT_TYPE_DATA;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.TYPE_DATA_INVALID_ATTRIBUTE;
import static com.android.internal.net.eap.message.EapTestMessageDefinitions.TYPE_DATA_INVALID_AT_RAND;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_PERMANENT_ID_REQ;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_VERSION_LIST;

import static junit.framework.TestCase.fail;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtPermanentIdReq;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtVersionList;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;
import com.android.internal.net.eap.message.simaka.EapSimTypeData.EapSimTypeDataDecoder;

import org.junit.Before;
import org.junit.Test;

import java.util.Arrays;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map.Entry;

public class EapSimTypeDataTest {
    private static final int UNABLE_TO_PROCESS_CODE = 0;
    private static final int INSUFFICIENT_CHALLENGES_CODE = 2;
    private static final int EAP_SIM_START = 10;
    private static final int INVALID_SUBTYPE_INT = -1;

    private EapSimTypeDataDecoder mEapSimTypeDataDecoder;

    @Before
    public void setUp() {
        mEapSimTypeDataDecoder = EapSimTypeData.getEapSimTypeDataDecoder();
    }

    @Test
    public void testConstructor() throws Exception {
        List<EapSimAkaAttribute> attributes = Arrays.asList(
                new AtVersionList(8, 1), new AtPermanentIdReq());

        EapSimTypeData eapSimTypeData = new EapSimTypeData(EAP_SIM_START, attributes);
        assertEquals(EAP_SIM_START, eapSimTypeData.eapSubtype);

        // check order of entries in EapSimTypeData.attributeMap
        Iterator<Entry<Integer, EapSimAkaAttribute>> itr =
                eapSimTypeData.attributeMap.entrySet().iterator();
        Entry<Integer, EapSimAkaAttribute> pair = itr.next();
        assertEquals(EAP_AT_VERSION_LIST, (int) pair.getKey());
        assertEquals(Arrays.asList(1), ((AtVersionList) pair.getValue()).versions);

        pair = itr.next();
        assertEquals(EAP_AT_PERMANENT_ID_REQ, (int) pair.getKey());
        assertTrue(pair.getValue() instanceof AtPermanentIdReq);
    }

    @Test
    public void testDecode() {
        DecodeResult<EapSimTypeData> result = mEapSimTypeDataDecoder.decode(EAP_SIM_START_SUBTYPE);

        assertTrue(result.isSuccessfulDecode());
        EapSimTypeData eapSimTypeData = result.eapTypeData;
        assertEquals(EAP_SIM_START, eapSimTypeData.eapSubtype);
        assertTrue(eapSimTypeData.attributeMap.containsKey(EAP_AT_VERSION_LIST));
        AtVersionList atVersionList = (AtVersionList)
                eapSimTypeData.attributeMap.get(EAP_AT_VERSION_LIST);
        assertEquals(Arrays.asList(1), atVersionList.versions);
        assertTrue(eapSimTypeData.attributeMap.containsKey(EAP_AT_PERMANENT_ID_REQ));

        // also check order of Map entries (needs to match input order)
        Iterator<Integer> itr = eapSimTypeData.attributeMap.keySet().iterator();
        assertEquals(EAP_AT_VERSION_LIST, (int) itr.next());
        assertEquals(EAP_AT_PERMANENT_ID_REQ, (int) itr.next());
        assertFalse(itr.hasNext());
    }

    @Test
    public void testDecodeNullTypeData() {
        DecodeResult<EapSimTypeData> result = mEapSimTypeDataDecoder.decode(null);
        assertFalse(result.isSuccessfulDecode());
        assertEquals(UNABLE_TO_PROCESS_CODE, result.atClientErrorCode.errorCode);
    }

    @Test
    public void testDecodeInvalidSubtype() {
        DecodeResult<EapSimTypeData> result = mEapSimTypeDataDecoder.decode(INVALID_SUBTYPE);
        assertFalse(result.isSuccessfulDecode());
        assertEquals(UNABLE_TO_PROCESS_CODE, result.atClientErrorCode.errorCode);
    }

    @Test
    public void testDecodeInvalidAtRand() {
        DecodeResult<EapSimTypeData> result =
                mEapSimTypeDataDecoder.decode(TYPE_DATA_INVALID_AT_RAND);
        assertFalse(result.isSuccessfulDecode());
        assertEquals(INSUFFICIENT_CHALLENGES_CODE, result.atClientErrorCode.errorCode);
    }

    @Test
    public void testDecodeShortPacket() {
        DecodeResult<EapSimTypeData> result = mEapSimTypeDataDecoder.decode(SHORT_TYPE_DATA);
        assertFalse(result.isSuccessfulDecode());
        assertEquals(UNABLE_TO_PROCESS_CODE, result.atClientErrorCode.errorCode);
    }

    @Test
    public void testDecodeInvalidEapAttribute() {
        DecodeResult<EapSimTypeData> result =
                mEapSimTypeDataDecoder.decode(TYPE_DATA_INVALID_ATTRIBUTE);
        assertFalse(result.isSuccessfulDecode());
        assertEquals(UNABLE_TO_PROCESS_CODE, result.atClientErrorCode.errorCode);
    }

    @Test
    public void testEncode() throws Exception {
        LinkedHashMap<Integer, EapSimAkaAttribute> attributes = new LinkedHashMap<>();
        attributes.put(EAP_AT_VERSION_LIST, new AtVersionList(8, 1));
        attributes.put(EAP_AT_PERMANENT_ID_REQ, new AtPermanentIdReq());
        EapSimTypeData eapSimTypeData = new EapSimTypeData(EAP_SIM_START, attributes);

        byte[] result = eapSimTypeData.encode();
        assertArrayEquals(EAP_SIM_START_SUBTYPE, result);
    }

    @Test
    public void testDecodeDuplicateAttributes() {
        DecodeResult<EapSimTypeData> result =
                mEapSimTypeDataDecoder.decode(EAP_SIM_START_DUPLICATE_ATTRIBUTES);
        assertFalse(result.isSuccessfulDecode());
        assertEquals(UNABLE_TO_PROCESS_CODE, result.atClientErrorCode.errorCode);
    }

    @Test
    public void testConstructorInvalidSubtype() throws Exception {
        try {
            new EapSimTypeData(INVALID_SUBTYPE_INT, Arrays.asList(new AtPermanentIdReq()));
            fail("Expected IllegalArgumentException for invalid subtype");
        } catch (IllegalArgumentException expected) {
        }
    }

    @Test
    public void testConstructorDuplicateAttributes() throws Exception {
        try {
            new EapSimTypeData(
                    EAP_SIM_START, Arrays.asList(new AtPermanentIdReq(), new AtPermanentIdReq()));
            fail("Expected IllegalArgumentException for duplicate attributes");
        } catch (IllegalArgumentException expected) {
        }
    }
}
