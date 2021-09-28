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

import com.google.api.client.googleapis.auth.oauth2.GoogleCredential;
import com.google.api.client.googleapis.javanet.GoogleNetHttpTransport;
import com.google.api.client.http.ByteArrayContent;
import com.google.api.client.http.GenericUrl;
import com.google.api.client.http.HttpContent;
import com.google.api.client.http.HttpRequest;
import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.http.HttpResponse;
import com.google.api.client.http.HttpTransport;
import com.google.api.client.json.JsonFactory;
import com.google.api.client.json.JsonObjectParser;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.common.annotations.VisibleForTesting;

import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.Collection;
import java.util.Map;

/** A helper class for performing REST API calls. */
public class RestApiHelper implements IRestApiHelper {

    protected static final JsonFactory JSON_FACTORY = JacksonFactory.getDefaultInstance();
    protected static final String JSON_MIME = "application/json";

    private HttpRequestFactory mRequestFactory;
    private String mBaseUri;

    /**
     * Creates an API helper instance with the given information.
     *
     * @param baseUri the base URI of API
     * @param requestFactory the factory to use when creating {@link HttpRequest}s.
     */
    public RestApiHelper(HttpRequestFactory requestFactory, String baseUri) {
        mRequestFactory = requestFactory;
        // Make sure the uri ends with a slash to avoid GenericUrl weirdness later
        mBaseUri = baseUri.endsWith("/") ? baseUri : baseUri + "/";
    }

    /**
     * Creates an API helper instance which uses a {@link GoogleCredential} for authentication.
     *
     * @param baseUri the base URI of the API
     * @param serviceAccount the name of the service account to use
     * @param keyFile the service account key file
     * @param scopes the collection of OAuth scopes to use with the service account
     * @throws GeneralSecurityException
     * @throws IOException
     */
    @Deprecated
    public static RestApiHelper newInstanceWithGoogleCredential(
            String baseUri, String serviceAccount, File keyFile, Collection<String> scopes)
            throws GeneralSecurityException, IOException {

        HttpTransport transport = GoogleNetHttpTransport.newTrustedTransport();
        GoogleCredential credential =
                GoogleApiClientUtil.createCredentialFromP12File(serviceAccount, keyFile, scopes);
        HttpRequestFactory requestFactory = transport.createRequestFactory(credential);

        return new RestApiHelper(requestFactory, baseUri);
    }

    /**
     * Creates an API helper instance which uses a {@link GoogleCredential} for authentication.
     *
     * @param baseUri the base URI of the API
     * @param jsonKeyFile the service account json key file
     * @param scopes the collection of OAuth scopes to use with the service account
     * @throws GeneralSecurityException
     * @throws IOException
     */
    public static RestApiHelper newInstanceWithGoogleCredential(
            String baseUri, File jsonKeyFile, Collection<String> scopes)
            throws GeneralSecurityException, IOException {
        HttpTransport transport = GoogleNetHttpTransport.newTrustedTransport();
        GoogleCredential credential =
                GoogleApiClientUtil.createCredentialFromJsonKeyFile(jsonKeyFile, scopes);
        HttpRequestFactory requestFactory = transport.createRequestFactory(credential);
        return new RestApiHelper(requestFactory, baseUri);
    }

    /** {@inheritDoc} */
    @Override
    public HttpResponse execute(
            String method, String[] uriParts, Map<String, Object> options, JSONObject data)
            throws IOException {
        HttpContent content = null;
        if (data != null) {
            content = new ByteArrayContent(JSON_MIME, data.toString().getBytes());
        }
        final GenericUrl uri = buildQueryUri(uriParts, options);
        final HttpRequest request = getRequestFactory().buildRequest(method, uri, content);
        request.setParser(new JsonObjectParser(JSON_FACTORY));
        return request.execute();
    }

    /**
     * Returns the HttpRequestFactory.
     *
     * <p>Exposed for testing.
     */
    @VisibleForTesting
    public HttpRequestFactory getRequestFactory() {
        return mRequestFactory;
    }

    /**
     * Construct a URI for a API call with given URI parts and options. uriParts should be
     * URL-encoded already, while options should be unencoded Strings.
     */
    public GenericUrl buildQueryUri(String[] uriParts, Map<String, Object> options) {
        final GenericUrl uri = new GenericUrl(mBaseUri);
        for (int i = 0; i < uriParts.length; ++i) {
            uri.appendRawPath(uriParts[i]);
            // Don't add a trailing slash
            if (i + 1 < uriParts.length) {
                uri.appendRawPath("/");
            }
        }

        if (options != null) {
            uri.putAll(options);
        }

        return uri;
    }
}
