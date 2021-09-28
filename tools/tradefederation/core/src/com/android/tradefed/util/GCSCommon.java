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
package com.android.tradefed.util;

import com.android.tradefed.host.HostOptions;

import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.googleapis.javanet.GoogleNetHttpTransport;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.services.storage.Storage;

import java.io.File;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Collection;

/**
 * Base class for Gcs operation like download and upload. {@link GCSFileDownloader} and {@link
 * GCSFileUploader}.
 */
public abstract class GCSCommon {
    /** This is the key for {@link HostOptions}'s service-account-json-key-file option. */
    private static final String GCS_JSON_KEY = "gcs-json-key";

    protected static final int DEFAULT_TIMEOUT = 10 * 60 * 1000; // 10minutes

    private File mJsonKeyFile = null;
    private Storage mStorage;

    public GCSCommon(File jsonKeyFile) {
        mJsonKeyFile = jsonKeyFile;
    }

    public GCSCommon() {}

    protected Storage getStorage(Collection<String> scopes) throws IOException {
        GoogleCredential credential = null;
        try {
            if (mStorage == null) {
                credential =
                        GoogleApiClientUtil.createCredential(scopes, mJsonKeyFile, GCS_JSON_KEY);
                mStorage =
                        new Storage.Builder(
                                        GoogleNetHttpTransport.newTrustedTransport(),
                                        JacksonFactory.getDefaultInstance(),
                                        GoogleApiClientUtil.configureRetryStrategy(
                                                GoogleApiClientUtil.setHttpTimeout(
                                                        credential,
                                                        DEFAULT_TIMEOUT,
                                                        DEFAULT_TIMEOUT)))
                                .setApplicationName(GoogleApiClientUtil.APP_NAME)
                                .build();
            }
            return mStorage;
        } catch (GeneralSecurityException e) {
            throw new IOException(e);
        }
    }
}
