/*
 * Copyright (C) 2017 The Android Open Source Project
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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.fail;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.util.FileUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;

/** Unit tests for {@link JSONFileKeyStoreFactory}. */
@RunWith(JUnit4.class)
public class JSONFileKeyStoreFactoryTest {

    private final String mJsonData =
            new String("{\"key1\":\"value 1\",\"key2\":\"foo\",\"key3\":\"bar\"}");
    private final String mJsonData2 =
            new String("{\"hostname.?\":{\"key1\":\"value 2\",\"key2\":\"foo 2\"}}");
    private final String mJsonData3 = new String("{\"hostname.?\":{\"key1\":\"value 3\"}}");
    private File mJsonFile;
    private JSONFileKeyStoreFactory mFactory;
    private File mJsonFile2;
    private File mJsonFile3;

    @Before
    public void setUp() throws Exception {
        mFactory = new JSONFileKeyStoreFactory();
        mFactory.setHostName("hostname");
        mJsonFile = FileUtil.createTempFile("json-keystore", ".json");
        FileUtil.writeToFile(mJsonData, mJsonFile);
        mJsonFile2 = FileUtil.createTempFile("json-keystore-2", ".json");
        FileUtil.writeToFile(mJsonData2, mJsonFile2);
        mJsonFile3 = FileUtil.createTempFile("json-keystore-3", ".json");
        FileUtil.writeToFile(mJsonData3, mJsonFile3);
    }

    @After
    public void tearDown() {
        FileUtil.recursiveDelete(mJsonFile);
        FileUtil.recursiveDelete(mJsonFile2);
        FileUtil.recursiveDelete(mJsonFile3);
    }

    /**
     * Test that if the underlying JSON keystore has not changed, the client returned is the same.
     */
    @Test
    public void testLoadKeyStore_same() throws Exception {
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        IKeyStoreClient client2 = mFactory.createKeyStoreClient();
        assertSame(client, client2);
    }

    /** Test that if the last modified flag has changed, we reload a new client. */
    @Test
    public void testLoadKeyStore_modified() throws Exception {
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        mJsonFile.setLastModified(mJsonFile.lastModified() + 5000l);
        IKeyStoreClient client2 = mFactory.createKeyStoreClient();
        assertNotNull(client2);
        assertNotSame(client, client2);
    }

    /**
     * Test that if the underlying file is not accessible anymore for any reason, we rely on the
     * cache.
     */
    @Test
    public void testLoadKeyStore_null() throws Exception {
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        FileUtil.deleteFile(mJsonFile);
        IKeyStoreClient client2 = mFactory.createKeyStoreClient();
        assertNotNull(client2);
        assertSame(client, client2);
    }

    /**
     * Test that if the last modified flag of the primary key store file has changed, we reload a
     * new client.
     */
    @Test
    public void testLoadKeyStore_primaryFileModified() throws Exception {
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile2.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        mJsonFile.setLastModified(mJsonFile.lastModified() + 5000l);
        IKeyStoreClient client2 = mFactory.createKeyStoreClient();
        assertNotNull(client2);
        assertNotSame(client, client2);
    }

    /**
     * Test that if the last modified flag of a host-based key store file has changed, we reload a
     * new client.
     */
    @Test
    public void testLoadKeyStore_hostBasedFileModified() throws Exception {
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile2.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        mJsonFile2.setLastModified(mJsonFile2.lastModified() + 5000l);
        IKeyStoreClient client2 = mFactory.createKeyStoreClient();
        assertNotNull(client2);
        assertNotSame(client, client2);
    }

    /**
     * Test that if the primary key store file is not accessible anymore for any reason, we rely on
     * the cache.
     */
    @Test
    public void testLoadKeyStore_primaryFileNotAccessible() throws Exception {
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile2.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        FileUtil.deleteFile(mJsonFile);
        IKeyStoreClient client2 = mFactory.createKeyStoreClient();
        assertNotNull(client2);
        assertSame(client, client2);
    }

    /**
     * Test that if a host-based key store file is not accessible anymore for any reason, we rely on
     * the cache.
     */
    @Test
    public void testLoadKeyStore_hostBasedFileNotAccessible() throws Exception {
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile2.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        FileUtil.deleteFile(mJsonFile2);
        IKeyStoreClient client2 = mFactory.createKeyStoreClient();
        assertNotNull(client2);
        assertSame(client, client2);
    }

    /** Test that the value for a key is properly overridden when the host name matches. */
    @Test
    public void testLoadKeyStore_valueOverriddenWhenHostNameMatches() throws Exception {
        // The hostname "hostname0" has matching entries in both host based files.
        mFactory.setHostName("hostname0");
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile2.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile3.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        assertEquals("value 3", client.fetchKey("key1")); // from file "json-keystore-3"
        assertEquals("foo 2", client.fetchKey("key2")); // from file "json-keystore-2"
        assertEquals("bar", client.fetchKey("key3")); // from file "json-keystore"
    }

    /** Test that the value for a key is unchanged when the host name has no matches. */
    @Test
    public void testLoadKeyStore_valueUnchangedWhenNoHostNameMatch() throws Exception {
        // The hostname "unmatched" has no matches in any of the host-based file.
        mFactory.setHostName("unmatched");
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile2.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", mJsonFile3.getAbsolutePath());
        IKeyStoreClient client = mFactory.createKeyStoreClient();
        assertNotNull(client);
        assertEquals("value 1", client.fetchKey("key1"));
        assertEquals("foo", client.fetchKey("key2"));
        assertEquals("bar", client.fetchKey("key3"));
    }

    /** Test that KeyStoreException is thrown for multiple hostname matches */
    @Test
    public void testLoadKeyStore_multipleHostNameMatches() throws Exception {
        // The hostname "hostname0" has multiple matches in jsonFile.
        String jsonData =
                new String(
                        "{\"hostname.?\":{\"key1\":\"value 2\"},"
                                + "\"hostname0\":{\"key1\":\"value 2\"}}");
        File jsonFile = FileUtil.createTempFile("json-keystore-4", ".json");
        FileUtil.writeToFile(jsonData, jsonFile);
        String hostname = "hostname0";
        mFactory.setHostName(hostname);
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", jsonFile.getAbsolutePath());
        try {
            mFactory.createKeyStoreClient();
            fail(
                    "Key store should not be available if hostname has multiple matches in"
                            + " host-based file.");
        } catch (KeyStoreException expected) {
            assertEquals(
                    String.format(
                            "Hostname %s matches multiple pattern strings in file %s.",
                            hostname, jsonFile.toString()),
                    expected.getMessage());
        } finally {
            FileUtil.recursiveDelete(jsonFile);
        }
    }

    /** Test that KeyStoreException is thrown for a bad-format host-based keystore file */
    @Test
    public void testLoadKeyStore_badKeyStoreFile() throws Exception {
        String jsonData = new String("{\"hostname.?\":{\"key1\"::\"value 2\"}}");
        File jsonFile = FileUtil.createTempFile("json-keystore-5", ".json");
        FileUtil.writeToFile(jsonData, jsonFile);
        String hostname = "hostname";
        mFactory.setHostName(hostname);
        OptionSetter setter = new OptionSetter(mFactory);
        setter.setOptionValue("json-key-store-file", mJsonFile.getAbsolutePath());
        setter.setOptionValue("host-based-key-store-file", jsonFile.getAbsolutePath());
        try {
            mFactory.createKeyStoreClient();
            fail("Key store should not be available for invalid keystore files.");
        } catch (KeyStoreException expected) {
            assertEquals(
                    String.format("Failed to parse JSON data from file %s", jsonFile.toString()),
                    expected.getMessage());
        } finally {
            FileUtil.recursiveDelete(jsonFile);
        }
    }
}
