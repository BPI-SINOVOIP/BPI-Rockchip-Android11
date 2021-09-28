/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tradefed.util.keystore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.util.FileUtil;

import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for JSON File Key Store Client test. */
@RunWith(JUnit4.class)
public class JSONFileKeyStoreClientTest {
    final String mJsonData = new String("{\"key1\":\"value 1\",\"key2 \":\"foo\"}");
    JSONFileKeyStoreClient mKeyStore = null;

    @Before
    public void setUp() throws Exception {
        mKeyStore = new JSONFileKeyStoreClient();
    }

    @Test
    public void testKeyStoreNullFile() throws Exception {
        try {
            new JSONFileKeyStoreClient(null);
            fail("Key store should not be available for null file");
        } catch (KeyStoreException e) {
            // Expected.
        }
    }

    @Test
    public void testKeyStoreFetchUnreadableFile() throws Exception {
        File test = FileUtil.createTempFile("keystore", "test");
        try {
            // Delete the file to make it non-readable. (do not use setReadable as a root tradefed
            // process would still be able to write it)
            test.delete();
            new JSONFileKeyStoreClient(test);
            fail("Should have thrown an exception");
        } catch(KeyStoreException expected) {
            assertEquals(String.format("Unable to read the JSON key store file %s", test),
                    expected.getMessage());
        } finally {
            FileUtil.deleteFile(test);
        }
    }

    @Test
    public void testKeyStoreFetchEmptyFile() throws Exception {
        File test = FileUtil.createTempFile("keystore", "test");
        try {
            new JSONFileKeyStoreClient(test);
            fail("Should have thrown an exception");
        } catch(KeyStoreException expected) {
            // expected
        } finally {
            FileUtil.deleteFile(test);
        }
    }

    @Test
    public void testKeyStoreFetchFile() throws Exception {
        File test = FileUtil.createTempFile("keystore", "test");
        try {
            FileUtil.writeToFile(mJsonData, test);
            JSONFileKeyStoreClient keystore = new JSONFileKeyStoreClient(test);
            assertTrue(keystore.isAvailable());
            assertEquals("value 1", keystore.fetchKey("key1"));
        } finally {
            FileUtil.deleteFile(test);
        }
    }

    @Test
    public void testContainsKeyinNullKeyStore() throws Exception {
        mKeyStore.setKeyStore(null);
        assertFalse("Key should not exist in null key store", mKeyStore.containsKey("test"));
    }

    @Test
    public void testDoesNotContainMissingKey() throws Exception {
        JSONObject data = new JSONObject(mJsonData);
        mKeyStore.setKeyStore(data);
        assertFalse("Missing key should not exist in key store",
                mKeyStore.containsKey("invalid key"));
    }

    @Test
    public void testContainsValidKey() throws Exception {
        JSONObject data = new JSONObject(mJsonData);
        mKeyStore.setKeyStore(data);
        assertTrue("Failed to fetch valid key in key store", mKeyStore.containsKey("key1"));
    }

    @Test
    public void testFetchMissingKey() throws Exception {
        JSONObject data = new JSONObject(mJsonData);
        mKeyStore.setKeyStore(data);
        assertNull("Missing key should not exist in key store",
                mKeyStore.fetchKey("invalid key"));
    }

    @Test
    public void testFetchNullKey() throws Exception {
        JSONObject data = new JSONObject(mJsonData);
        mKeyStore.setKeyStore(data);
        assertNull("Null key should not exist in key store",
                mKeyStore.fetchKey(null));
    }

    @Test
    public void testFetchValidKey() throws Exception {
        JSONObject data = new JSONObject(mJsonData);
        mKeyStore.setKeyStore(data);
        assertEquals("value 1", mKeyStore.fetchKey("key1"));
    }

    @Test
    public void testSetKey() throws Exception {
        JSONObject data = new JSONObject(mJsonData);
        mKeyStore.setKeyStore(data);
        mKeyStore.setKey("key1", "value 2");
        assertEquals("value 2", mKeyStore.fetchKey("key1"));
    }
}
