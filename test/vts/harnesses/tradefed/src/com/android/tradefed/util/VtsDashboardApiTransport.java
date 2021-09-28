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

import com.google.api.client.http.ByteArrayContent;
import com.google.api.client.http.FileContent;
import com.google.api.client.http.GenericUrl;
import com.google.api.client.http.HttpContent;
import com.google.api.client.http.HttpHeaders;
import com.google.api.client.http.HttpRequest;
import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.http.HttpResponse;
import com.google.api.client.http.HttpTransport;
import com.google.api.client.http.MultipartContent;
import com.google.api.client.http.MultipartContent.Part;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.client.util.Charsets;
import com.google.common.base.Preconditions;
import com.google.gson.Gson;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Type;
import java.net.URI;
import java.time.Duration;
import java.util.Map;

/**
 * Http Client Class Library which is customized for dashboard using google http java client
 * Currently, this class support GET, POST method and media file upload.
 */
public class VtsDashboardApiTransport {
    /**
     * The target API host url
     */
    private String API_URL = "";

    /**
     * The recommended concrete implementation HTTP transport library to use depends on what
     * environment you are running in
     */
    private final HttpTransport httpTransport;

    /**
     * VtsDashboardApiTransport class constructor function.
     */
    public VtsDashboardApiTransport(HttpTransport httpTransport, String apiUrl) {
        this.httpTransport = Preconditions.checkNotNull(httpTransport);
        this.API_URL = apiUrl;
    }

    /**
     * Upload file content through GET method.
     *
     * @param path The url path from API HOST.
     * @param responseType The response class type.
     * @return responseType Class will have mapped attributes from Json response.
     * @throws IOException when http request is broken.
     */
    @SuppressWarnings("unchecked")
    public <T> T get(String path, Type responseType) throws IOException {
        HttpRequestFactory requestFactory = getHttpRequestFactory();

        GenericUrl url = new GenericUrl(URI.create(API_URL + "/" + path));
        try {
            HttpRequest httpRequest = requestFactory.buildGetRequest(url);
            HttpResponse response = httpRequest.execute();
            return (T) response.parseAs(responseType);
        } catch (IOException e) {
            throw new IOException("Error running get API operation " + path, e);
        }
    }

    /**
     * Upload file content through POST method.
     *
     * @param path The url path from API HOST.
     * @param request The request post data object.
     * @param responseType The response class type.
     * @return responseType Class will have mapped attributes from Json response.
     * @throws IOException when http request is broken.
     */
    @SuppressWarnings("unchecked")
    public <T> T post(String path, Object request, Type responseType) throws IOException {
        HttpRequestFactory requestFactory = getHttpRequestFactory();

        GenericUrl url = new GenericUrl(URI.create(API_URL + "/" + path));

        HttpContent httpContent = new ByteArrayContent(
                "application/json", new Gson().toJson(request).getBytes(Charsets.UTF_8));
        try {
            HttpRequest httpRequest = requestFactory.buildPostRequest(url, httpContent);
            HttpResponse response = httpRequest.execute();
            return (T) response.parseAs(responseType);
        } catch (IOException e) {
            throw new IOException("Error running post API operation " + path, e);
        }
    }

    /**
     * Upload file content through POST method.
     *
     * @param path The url path from API HOST.
     * @param fileType The file type to upload.
     * @param testFilePath The file path to upload.
     * @return String Response body will be returned.
     * @throws IOException when http request is broken.
     */
    public String postFile(String path, String fileType, String testFilePath) throws IOException {
        HttpRequestFactory requestFactory = getHttpRequestFactory();

        FileContent fileContent = new FileContent(fileType, new File(testFilePath));

        GenericUrl url = new GenericUrl(URI.create(API_URL + "/" + path));
        HttpRequest request = requestFactory.buildPostRequest(url, fileContent);

        return request.execute().parseAsString();
    }

    /**
     * Upload multiple files content through POST method.
     *
     * @param path The url path from API HOST.
     * @param testFileMap The map for each file with file type.
     * @return String Response body will be returned.
     * @throws IOException when http request is broken.
     */
    public String postMultiFiles(String path, Map<String, String> testFileMap) throws IOException {
        HttpRequestFactory requestFactory = getHttpRequestFactory();

        MultipartContent multipartContent = new MultipartContent();
        for (Map.Entry<String, String> entry : testFileMap.entrySet()) {
            FileContent fileContent = new FileContent(entry.getKey(), new File(entry.getValue()));
            Part part = new Part(fileContent);
            part.setHeaders(new HttpHeaders().set("Content-Disposition",
                    String.format(
                            "form-data; name=\"content\"; filename=\"%s\"", entry.getValue())));
            multipartContent.addPart(part);
        }

        GenericUrl url = new GenericUrl(URI.create(API_URL + "/" + path));
        HttpRequest request = requestFactory.buildPostRequest(url, multipartContent);

        return request.execute().parseAsString();
    }

    /**
     * Get HttpRequestFactory instance with default options.
     *
     * @return HttpRequestFactory from google-http-java-client library with custom options.
     */
    protected HttpRequestFactory getHttpRequestFactory() {
        return httpTransport.createRequestFactory(request -> {
            request.setConnectTimeout((int) Duration.ofMinutes(1).toMillis());
            request.setReadTimeout((int) Duration.ofMinutes(1).toMillis());
            HttpHeaders httpHeaders = new HttpHeaders();
            request.setHeaders(httpHeaders);
            request.setParser(new JacksonFactory().createJsonObjectParser());
        });
    }
}
