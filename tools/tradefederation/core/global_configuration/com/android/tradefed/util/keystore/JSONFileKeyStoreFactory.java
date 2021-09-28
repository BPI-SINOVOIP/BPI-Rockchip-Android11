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

import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.FileUtil;

import com.google.common.annotations.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.regex.Pattern;

/**
 * Implementation of a JSON KeyStore Factory, which provides a {@link JSONFileKeyStoreClient} for
 * accessing a JSON Key Store File.
 */
@OptionClass(alias = "json-keystore")
public class JSONFileKeyStoreFactory implements IKeyStoreFactory {
    @Option(
            name = "json-key-store-file",
            description = "The JSON file from where to read the key store",
            importance = Importance.IF_UNSET)
    private File mJsonFile = null;

    @Option(
            name = "host-based-key-store-file",
            description = "The JSON file from where to read the host-based key store",
            importance = Importance.IF_UNSET)
    private List<File> mHostBasedJsonFiles = new ArrayList<File>();

    /** Keystore factory is a global object and may be accessed concurrently so we need a lock */
    private static final Object mLock = new Object();

    private JSONFileKeyStoreClient mCachedClient = null;
    private long mLastLoadedTime = 0l;
    private String mHostName = null;

    /** {@inheritDoc} */
    @Override
    public IKeyStoreClient createKeyStoreClient() throws KeyStoreException {
        synchronized (mLock) {
            mHostName = getHostName();

            List<String> invalidFiles = findInvalidJsonKeyStoreFiles();
            if (mCachedClient == null) {
                if (!invalidFiles.isEmpty()) {
                    throw new KeyStoreException(
                            String.format(
                                    "These keystore files are missing: %s",
                                    String.join("\n", invalidFiles)));
                }

                createKeyStoreInternal();
                CLog.d(
                        "Keystore initialized with %s and %d host-based keystore files.",
                        mJsonFile.getAbsolutePath(), mHostBasedJsonFiles.size());
            }
            if (!invalidFiles.isEmpty()) {
                CLog.w(
                        String.format(
                                "These keystore files are missing: %s",
                                String.join("\n", invalidFiles)));
            } else {
                List<String> changedFiles = findChangedJsonKeyStoreFiles();
                if (!changedFiles.isEmpty()) {
                    createKeyStoreInternal();
                    CLog.d(
                            "Reloading the keystore as these keystore files have changed: %s",
                            String.join("\n", changedFiles));
                }
            }

            return mCachedClient;
        }
    }

    private String getHostName() throws KeyStoreException {
        if (mHostName == null) {
            try {
                mHostName = InetAddress.getLocalHost().getHostName();
            } catch (UnknownHostException e) {
                throw new KeyStoreException(String.format("Failed to get hostname"), e);
            }
        }

        return mHostName;
    }

    private List<String> findInvalidJsonKeyStoreFiles() {
        List<String> invalidFiles = new ArrayList<String>(); // not exist or not canRead().
        if (!mJsonFile.exists() || !mJsonFile.canRead()) {
            invalidFiles.add(mJsonFile.getAbsolutePath());
        }
        for (File file : mHostBasedJsonFiles) {
            if (!file.exists() || !file.canRead()) {
                invalidFiles.add(file.getAbsolutePath());
            }
        }
        return invalidFiles;
    }

    private List<String> findChangedJsonKeyStoreFiles() {
        List<String> changedFiles = new ArrayList<String>();
        if (mLastLoadedTime < mJsonFile.lastModified()) {
            changedFiles.add(mJsonFile.getAbsolutePath());
        }
        for (File file : mHostBasedJsonFiles) {
            if (mLastLoadedTime < file.lastModified()) {
                changedFiles.add(file.getAbsolutePath());
            }
        }
        return changedFiles;
    }

    private void createKeyStoreInternal() throws KeyStoreException {
        mLastLoadedTime = System.currentTimeMillis();
        mCachedClient = new JSONFileKeyStoreClient(mJsonFile);
        for (File file : mHostBasedJsonFiles) {
            overrideClientWithHostKeyStoreFromFile(file);
        }
    }

    private void overrideClientWithHostKeyStoreFromFile(File file) throws KeyStoreException {
        JSONObject hostKeyStore = getHostKeyStoreFromFile(file);
        if (hostKeyStore == null) {
            // This is not necessarily an error.
            CLog.d(
                    "Host-based keystore for %s not found in Keystore file %s",
                    mHostName, mJsonFile.getAbsolutePath());
            return;
        }
        for (Iterator<String> keys = hostKeyStore.keys(); keys.hasNext(); ) {
            String key = keys.next();
            try {
                mCachedClient.setKey(key, hostKeyStore.getString(key));
                CLog.d(
                        "Hostname: %s, %s gets value from file:%s",
                        mHostName, key, mJsonFile.getAbsolutePath());
            } catch (JSONException e) {
                throw new KeyStoreException(
                        String.format(
                                "Failed to update keystore with host-based keystore from file"
                                        + " %s",
                                file.toString()),
                        e);
            }
        }
    }

    private JSONObject getHostKeyStoreFromFile(File file) throws KeyStoreException {
        JSONObject hostKeyStore = null;
        JSONObject jsonKeyStore = loadKeyStoreFromFile(file);
        int matchingPatternCount = 0;
        for (Iterator<String> patternStrings = jsonKeyStore.keys(); patternStrings.hasNext(); ) {
            String patternString = patternStrings.next();
            Pattern pattern = Pattern.compile(patternString);
            if (pattern.matcher(mHostName).matches()) {
                CLog.d(
                        "Hostname %s matches pattern string %s in file %s.",
                        mHostName, patternString, file.getAbsolutePath());

                if (++matchingPatternCount > 1) {
                    throw new KeyStoreException(
                            String.format(
                                    "Hostname %s matches multiple pattern strings in file %s.",
                                    mHostName, file.toString()));
                }

                try {
                    hostKeyStore = jsonKeyStore.getJSONObject(patternString);
                } catch (JSONException e) {
                    throw new KeyStoreException(
                            String.format(
                                    "Failed to parse JSON data from file %s", file.toString()),
                            e);
                }
            }
        }
        return hostKeyStore;
    }

    private JSONObject loadKeyStoreFromFile(File jsonFile) throws KeyStoreException {
        JSONObject keyStore = null;
        try {
            String data = FileUtil.readStringFromFile(jsonFile);
            keyStore = new JSONObject(data);
        } catch (IOException e) {
            throw new KeyStoreException(
                    String.format("Failed to read JSON key file %s: %s", jsonFile.toString(), e));
        } catch (JSONException e) {
            throw new KeyStoreException(
                    String.format("Failed to parse JSON data from file %s", jsonFile.toString()),
                    e);
        }
        return keyStore;
    }

    /**
     * Helper method used to set host name. Used for testing.
     *
     * @param hostName to use as host name.
     */
    @VisibleForTesting
    public void setHostName(String hostName) {
        mHostName = hostName;
    }
}
