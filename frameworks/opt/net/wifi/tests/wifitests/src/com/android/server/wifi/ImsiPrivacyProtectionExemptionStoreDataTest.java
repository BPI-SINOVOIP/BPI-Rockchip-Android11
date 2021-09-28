/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.server.wifi;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Xml;

import com.android.internal.util.FastXmlSerializer;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlSerializer;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

public class ImsiPrivacyProtectionExemptionStoreDataTest {
    private static final int TEST_CARRIER_ID = 1911;

    private @Mock ImsiPrivacyProtectionExemptionStoreData.DataSource mDataSource;
    private ImsiPrivacyProtectionExemptionStoreData mImsiPrivacyProtectionExemptionStoreData;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mImsiPrivacyProtectionExemptionStoreData =
                new ImsiPrivacyProtectionExemptionStoreData(mDataSource);
    }

    /**
     * Helper function for serializing configuration data to a XML block.
     */
    private byte[] serializeData() throws Exception {
        final XmlSerializer out = new FastXmlSerializer();
        final ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
        out.setOutput(outputStream, StandardCharsets.UTF_8.name());
        mImsiPrivacyProtectionExemptionStoreData.serializeData(out, null);
        out.flush();
        return outputStream.toByteArray();
    }

    /**
     * Helper function for parsing configuration data from a XML block.
     */
    private void deserializeData(byte[] data) throws Exception {
        final XmlPullParser in = Xml.newPullParser();
        final ByteArrayInputStream inputStream = new ByteArrayInputStream(data);
        in.setInput(inputStream, StandardCharsets.UTF_8.name());
        mImsiPrivacyProtectionExemptionStoreData.deserializeData(in, in.getDepth(),
                WifiConfigStore.ENCRYPT_CREDENTIALS_CONFIG_STORE_DATA_VERSION, null);
    }

    /**
     * Verify store file Id.
     */
    @Test
    public void verifyStoreFileId() throws Exception {
        assertEquals(WifiConfigStore.STORE_FILE_USER_GENERAL,
                mImsiPrivacyProtectionExemptionStoreData.getStoreFileId());
    }

    /**
     * Verify serialize and deserialize Protection exemption map.
     */
    @Test
    public void testSerializeDeserialize() throws Exception {
        Map<Integer, Boolean> imsiPrivacyProtectionExemptionMap = new HashMap<>();
        imsiPrivacyProtectionExemptionMap.put(TEST_CARRIER_ID, true);
        assertSerializeDeserialize(imsiPrivacyProtectionExemptionMap);
    }

    /**
     * Verify serialize and deserialize empty protection exemption map.
     */
    @Test
    public void testSerializeDeserializeEmpty() throws Exception {
        Map<Integer, Boolean> imsiPrivacyProtectionExemptionMap = new HashMap<>();
        assertSerializeDeserialize(imsiPrivacyProtectionExemptionMap);
    }

    @Test
    public void testDeserializeOnNewDeviceOrNewUser() throws Exception {
        ArgumentCaptor<Map> deserializedNetworkSuggestionsMap =
                ArgumentCaptor.forClass(Map.class);
        mImsiPrivacyProtectionExemptionStoreData.deserializeData(null, 0,
                WifiConfigStore.ENCRYPT_CREDENTIALS_CONFIG_STORE_DATA_VERSION, null);
        verify(mDataSource).fromDeserialized(deserializedNetworkSuggestionsMap.capture());
        assertTrue(deserializedNetworkSuggestionsMap.getValue().isEmpty());
    }


    private Map<Integer, Boolean> assertSerializeDeserialize(
            Map<Integer, Boolean> mImsiPrivacyProtectionExemptionMap) throws Exception {
        // Setup the data to serialize.
        when(mDataSource.toSerialize()).thenReturn(mImsiPrivacyProtectionExemptionMap);

        // Serialize/deserialize data.
        deserializeData(serializeData());

        // Verify the deserialized data.
        ArgumentCaptor<HashMap> deserializedNetworkSuggestionsMap =
                ArgumentCaptor.forClass(HashMap.class);
        verify(mDataSource).fromDeserialized(deserializedNetworkSuggestionsMap.capture());
        assertEquals(mImsiPrivacyProtectionExemptionMap,
                deserializedNetworkSuggestionsMap.getValue());
        return deserializedNetworkSuggestionsMap.getValue();
    }
}
