/*
 * Copyright (C) 2018 The Android Open Source Project
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

import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.host.HostOptions;
import com.android.tradefed.log.LogUtil.CLog;

import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.googleapis.javanet.GoogleNetHttpTransport;
import com.google.api.client.http.HttpBackOffUnsuccessfulResponseHandler;
import com.google.api.client.http.HttpRequest;
import com.google.api.client.http.HttpRequestInitializer;
import com.google.api.client.http.HttpResponse;
import com.google.api.client.http.HttpUnsuccessfulResponseHandler;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.client.util.ExponentialBackOff;
import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/** Utils for create Google API client. */
public class GoogleApiClientUtil {

    public static final String APP_NAME = "tradefed";
    private static GoogleApiClientUtil sInstance = null;

    private static GoogleApiClientUtil getInstance() {
        if (sInstance == null) {
            sInstance = new GoogleApiClientUtil();
        }
        return sInstance;
    }

    /**
     * Create credential from json key file.
     *
     * @param file is the p12 key file
     * @param scopes is the API's scope.
     * @return a {@link GoogleCredential}.
     * @throws FileNotFoundException
     * @throws IOException
     * @throws GeneralSecurityException
     */
    public static GoogleCredential createCredentialFromJsonKeyFile(
            File file, Collection<String> scopes) throws IOException, GeneralSecurityException {
        return getInstance().doCreateCredentialFromJsonKeyFile(file, scopes);
    }

    @VisibleForTesting
    GoogleCredential doCreateCredentialFromJsonKeyFile(File file, Collection<String> scopes)
            throws IOException, GeneralSecurityException {
        return GoogleCredential.fromStream(
                        new FileInputStream(file),
                        GoogleNetHttpTransport.newTrustedTransport(),
                        JacksonFactory.getDefaultInstance())
                .createScoped(scopes);
    }

    /**
     * Try to create credential with different key files or from local host.
     *
     * <p>1. If primaryKeyFile is set, try to use it to create credential. 2. Try to get
     * corresponding key files from {@link HostOptions}. 3. Try to use backup key files. 4. Use
     * local default credential.
     *
     * @param scopes scopes for the credential.
     * @param primaryKeyFile the primary json key file; it can be null.
     * @param hostOptionKeyFileName {@link HostOptions}'service-account-json-key-file option's key;
     *     it can be null.
     * @param backupKeyFiles backup key files.
     * @return a {@link GoogleCredential}
     * @throws IOException
     * @throws GeneralSecurityException
     */
    public static GoogleCredential createCredential(
            Collection<String> scopes,
            File primaryKeyFile,
            String hostOptionKeyFileName,
            File... backupKeyFiles)
            throws IOException, GeneralSecurityException {
        return getInstance()
                .doCreateCredential(scopes, primaryKeyFile, hostOptionKeyFileName, backupKeyFiles);
    }

    @VisibleForTesting
    GoogleCredential doCreateCredential(
            Collection<String> scopes,
            File primaryKeyFile,
            String hostOptionKeyFileName,
            File... backupKeyFiles)
            throws IOException, GeneralSecurityException {
        List<File> keyFiles = new ArrayList<File>();
        if (primaryKeyFile != null) {
            keyFiles.add(primaryKeyFile);
        }
        File hostOptionKeyFile = null;
        if (hostOptionKeyFileName != null) {
            try {
                hostOptionKeyFile =
                        GlobalConfiguration.getInstance()
                                .getHostOptions()
                                .getServiceAccountJsonKeyFiles()
                                .get(hostOptionKeyFileName);
                if (hostOptionKeyFile != null) {
                    keyFiles.add(hostOptionKeyFile);
                }
            } catch (IllegalStateException e) {
                CLog.d("Global configuration haven't been initialized.");
            }
        }
        keyFiles.addAll(Arrays.asList(backupKeyFiles));
        for (File keyFile : keyFiles) {
            if (keyFile != null) {
                if (keyFile.exists() && keyFile.canRead()) {
                    CLog.d("Using %s.", keyFile.getAbsolutePath());
                    return doCreateCredentialFromJsonKeyFile(keyFile, scopes);
                } else {
                    CLog.i("No access to %s.", keyFile.getAbsolutePath());
                }
            }
        }
        return doCreateDefaultCredential(scopes);
    }

    @VisibleForTesting
    GoogleCredential doCreateDefaultCredential(Collection<String> scopes) throws IOException {
        try {
            CLog.d("Using local authentication.");
            return GoogleCredential.getApplicationDefault().createScoped(scopes);
        } catch (IOException e) {
            CLog.e(
                    "Try 'gcloud auth application-default login' to login for "
                            + "personal account; Or 'export "
                            + "GOOGLE_APPLICATION_CREDENTIALS=/path/to/key.json' "
                            + "for service account.");
            throw e;
        }
    }

    /**
     * Create credential from p12 file for service account.
     *
     * @deprecated It's better to use json key file, since p12 is deprecated by Google App Engine.
     *     And json key file have more information.
     * @param serviceAccount is the service account
     * @param keyFile is the p12 key file
     * @param scopes is the API's scope.
     * @return a {@link GoogleCredential}.
     * @throws GeneralSecurityException
     * @throws IOException
     */
    @Deprecated
    public static GoogleCredential createCredentialFromP12File(
            String serviceAccount, File keyFile, Collection<String> scopes)
            throws GeneralSecurityException, IOException {
        return new GoogleCredential.Builder()
                .setTransport(GoogleNetHttpTransport.newTrustedTransport())
                .setJsonFactory(JacksonFactory.getDefaultInstance())
                .setServiceAccountId(serviceAccount)
                .setServiceAccountScopes(scopes)
                .setServiceAccountPrivateKeyFromP12File(keyFile)
                .build();
    }

    /**
     * @param requestInitializer a {@link HttpRequestInitializer}, normally it's {@link
     *     GoogleCredential}.
     * @param connectTimeout connect timeout in milliseconds.
     * @param readTimeout read timeout in milliseconds.
     * @return a {@link HttpRequestInitializer} with timeout.
     */
    public static HttpRequestInitializer setHttpTimeout(
            final HttpRequestInitializer requestInitializer, int connectTimeout, int readTimeout) {
        return new HttpRequestInitializer() {
            @Override
            public void initialize(HttpRequest request) throws IOException {
                requestInitializer.initialize(request);
                request.setConnectTimeout(connectTimeout);
                request.setReadTimeout(readTimeout);
            }
        };
    }

    /**
     * Setup a retry strategy for the provided HttpRequestInitializer. In case of server errors
     * requests will be automatically retried with an exponential backoff.
     *
     * @param initializer - an initializer which will setup a retry strategy.
     * @return an initializer that will retry failed requests automatically.
     */
    public static HttpRequestInitializer configureRetryStrategy(
            HttpRequestInitializer initializer) {
        return new HttpRequestInitializer() {
            @Override
            public void initialize(HttpRequest request) throws IOException {
                initializer.initialize(request);
                request.setUnsuccessfulResponseHandler(new RetryResponseHandler());
            }
        };
    }

    private static class RetryResponseHandler implements HttpUnsuccessfulResponseHandler {
        // Initial interval to wait before retrying if a request fails.
        private static final int INITIAL_RETRY_INTERVAL = 1000;
        private static final int MAX_RETRY_INTERVAL = 3 * 60000; // Set max interval to 3 minutes.

        private final HttpUnsuccessfulResponseHandler backOffHandler;

        public RetryResponseHandler() {
            backOffHandler =
                    new HttpBackOffUnsuccessfulResponseHandler(
                            new ExponentialBackOff.Builder()
                                    .setInitialIntervalMillis(INITIAL_RETRY_INTERVAL)
                                    .setMaxIntervalMillis(MAX_RETRY_INTERVAL)
                                    .build());
        }

        @Override
        public boolean handleResponse(
                HttpRequest request, HttpResponse response, boolean supportsRetry)
                throws IOException {
            CLog.w(
                    "Request to %s failed: %d %s",
                    request.getUrl(), response.getStatusCode(), response.getStatusMessage());
            return backOffHandler.handleResponse(request, response, supportsRetry);
        }
    }
}
