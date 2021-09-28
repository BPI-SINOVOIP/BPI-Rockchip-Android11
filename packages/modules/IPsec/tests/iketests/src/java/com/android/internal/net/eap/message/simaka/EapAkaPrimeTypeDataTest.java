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
import static com.android.internal.net.eap.message.simaka.EapAkaTypeData.EAP_AKA_CHALLENGE;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_AUTN;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_KDF;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_KDF_INPUT;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_MAC;
import static com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.EAP_AT_RAND;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.AT_KDF_INPUT;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.KDF_VERSION;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.NETWORK_NAME_BYTES;
import static com.android.internal.net.eap.message.simaka.attributes.EapTestAttributeDefinitions.NETWORK_NAME_HEX;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.android.internal.net.eap.message.simaka.EapAkaPrimeTypeData.EapAkaPrimeTypeDataDecoder;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtAutn;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtClientErrorCode;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtKdf;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtKdfInput;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtMac;
import com.android.internal.net.eap.message.simaka.EapSimAkaAttribute.AtRandAka;
import com.android.internal.net.eap.message.simaka.EapSimAkaTypeData.DecodeResult;

import org.junit.Before;
import org.junit.Test;

import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map.Entry;

public class EapAkaPrimeTypeDataTest {
    private static final String RAND = "7A1FCDC0034BA1227E7B9FCEAFD47D53";
    private static final byte[] RAND_BYTES = hexStringToByteArray(RAND);
    private static final String AUTN = "000102030405060708090A0B0C0D0E0F";
    private static final byte[] AUTN_BYTES = hexStringToByteArray(AUTN);
    private static final String MAC = "95FEB9E70427F34B4FAC8F2C7A65A302";
    private static final byte[] MAC_BYTES = hexStringToByteArray(MAC);
    private static final byte[] EAP_AKA_PRIME_CHALLENGE_REQUEST =
            hexStringToByteArray(
                    "010000" // Challenge | 2B padding
                            + "01050000" + RAND // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "1704000B" + NETWORK_NAME_HEX + "00" // AT_KDF_INPUT
                            + "18010001" // AT_KDF
                            + "0B050000" + MAC); // AT_MAC attribute
    private static final byte[] EAP_AKA_PRIME_MULTIPLE_AT_KDF =
            hexStringToByteArray(
                    "010000" // Challenge | 2B padding
                            + "01050000" + RAND // AT_RAND attribute
                            + "02050000" + AUTN // AT_AUTN attribute
                            + "1704000B" + NETWORK_NAME_HEX + "00" // AT_KDF_INPUT
                            + "18010001" // AT_KDF
                            + "18010002" // AT_KDF
                            + "0B050000" + MAC); // AT_MAC attribute

    private EapAkaPrimeTypeDataDecoder mTypeDataDecoder;

    @Before
    public void setUp() {
        mTypeDataDecoder = EapAkaPrimeTypeData.getEapAkaPrimeTypeDataDecoder();
    }

    @Test
    public void testDecode() {
        DecodeResult<EapAkaTypeData> result =
                mTypeDataDecoder.decode(EAP_AKA_PRIME_CHALLENGE_REQUEST);

        assertTrue(result.isSuccessfulDecode());
        EapAkaPrimeTypeData eapAkaPrimeTypeData = (EapAkaPrimeTypeData) result.eapTypeData;
        assertEquals(EAP_AKA_CHALLENGE, eapAkaPrimeTypeData.eapSubtype);

        // also check Map entries (needs to match input order)
        Iterator<Entry<Integer, EapSimAkaAttribute>> itr =
                eapAkaPrimeTypeData.attributeMap.entrySet().iterator();
        Entry<Integer, EapSimAkaAttribute> entry = itr.next();
        assertEquals(EAP_AT_RAND, (int) entry.getKey());
        assertArrayEquals(RAND_BYTES, ((AtRandAka) entry.getValue()).rand);

        entry = itr.next();
        assertEquals(EAP_AT_AUTN, (int) entry.getKey());
        assertArrayEquals(AUTN_BYTES, ((AtAutn) entry.getValue()).autn);

        entry = itr.next();
        assertEquals(EAP_AT_KDF_INPUT, (int) entry.getKey());
        assertArrayEquals(NETWORK_NAME_BYTES, ((AtKdfInput) entry.getValue()).networkName);

        entry = itr.next();
        assertEquals(EAP_AT_KDF, (int) entry.getKey());
        assertEquals(KDF_VERSION, ((AtKdf) entry.getValue()).kdf);

        entry = itr.next();
        assertEquals(EAP_AT_MAC, (int) entry.getKey());
        assertArrayEquals(MAC_BYTES, ((AtMac) entry.getValue()).mac);

        assertFalse(itr.hasNext());
    }

    @Test
    public void testDecodeMultipleAtKdfAttributes() {
        DecodeResult<EapAkaTypeData> result =
                mTypeDataDecoder.decode(EAP_AKA_PRIME_MULTIPLE_AT_KDF);

        assertFalse(result.isSuccessfulDecode());
        assertEquals(AtClientErrorCode.UNABLE_TO_PROCESS, result.atClientErrorCode);
    }

    @Test
    public void testEncode() throws Exception {
        LinkedHashMap<Integer, EapSimAkaAttribute> attributes = new LinkedHashMap<>();
        attributes.put(EAP_AT_RAND, new AtRandAka(RAND_BYTES));
        attributes.put(EAP_AT_AUTN, new AtAutn(AUTN_BYTES));
        attributes.put(EAP_AT_KDF_INPUT, new AtKdfInput(AT_KDF_INPUT.length, NETWORK_NAME_BYTES));
        attributes.put(EAP_AT_KDF, new AtKdf(KDF_VERSION));
        attributes.put(EAP_AT_MAC, new AtMac(MAC_BYTES));
        EapAkaPrimeTypeData eapAkaPrimeTypeData =
                new EapAkaPrimeTypeData(EAP_AKA_CHALLENGE, attributes);

        byte[] result = eapAkaPrimeTypeData.encode();
        assertArrayEquals(EAP_AKA_PRIME_CHALLENGE_REQUEST, result);
    }
}
