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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import com.android.tradefed.build.IFolderBuildInfo;
import com.google.api.client.http.HttpHeaders;
import com.google.api.client.http.HttpRequest;
import com.google.api.client.http.HttpRequestFactory;
import com.google.api.client.http.HttpResponse;
import com.google.api.client.http.HttpTransport;
import com.google.api.client.http.LowLevelHttpRequest;
import com.google.api.client.http.LowLevelHttpResponse;
import com.google.api.client.json.Json;
import com.google.api.client.json.jackson2.JacksonFactory;
import com.google.api.client.testing.http.HttpTesting;
import com.google.api.client.testing.http.MockHttpTransport;
import com.google.api.client.testing.http.MockLowLevelHttpRequest;
import com.google.api.client.testing.http.MockLowLevelHttpResponse;
import com.google.api.client.util.Key;
import com.google.common.base.Preconditions;
import java.io.File;
import java.io.IOException;
import java.time.Duration;
import java.util.HashMap;
import java.util.Map;
import org.easymock.EasyMock;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for {@link VtsDashboardApiTransportTest}.
 */
@RunWith(JUnit4.class)
public class VtsDashboardApiTransportTest {
    private static final String SAMPLE = "123\u05D9\u05e0\u05D9\u05D1";

    private HttpTransport mockHttpTransport = null;
    private VtsDashboardApiTransport vtsDashboardApiTransport = null;

    public static class RspResult {
        @Key public int a;
        @Key public int b;
        @Key public String c;
    }

    @Before
    public void setUp() throws Exception {
        this.mockHttpTransport = this.createMockHttpTransport(Json.MEDIA_TYPE, SAMPLE);
        this.vtsDashboardApiTransport =
                new VtsDashboardApiTransport(this.mockHttpTransport, HttpTesting.SIMPLE_URL);
    }

    /**
     * Terminate the mockHttpTransport, join threads and close IO streams.
     */
    @After
    public void tearDown() throws IOException {
        if (this.mockHttpTransport != null) {
            this.mockHttpTransport.shutdown();
        }
    }

    /**
     * Create a mock HttpTransport with returns the expected results.
     */
    private HttpTransport createMockHttpTransport(String mediaType, String content) {
        HttpTransport transport = new MockHttpTransport() {
            @Override
            public LowLevelHttpRequest buildRequest(String method, String url) throws IOException {
                return new MockLowLevelHttpRequest() {
                    @Override
                    public LowLevelHttpResponse execute() throws IOException {
                        MockLowLevelHttpResponse result = new MockLowLevelHttpResponse();
                        result.setContentType(mediaType);
                        result.setContent(content);
                        return result;
                    }
                };
            }
        };
        return transport;
    }

    @Test
    public void testBasicRequest() throws Exception {
        HttpRequest request = mockHttpTransport.createRequestFactory().buildGetRequest(
                HttpTesting.SIMPLE_GENERIC_URL);
        HttpResponse response = request.execute();
        assertEquals(SAMPLE, response.parseAsString());
    }

    @Test
    public void testGetMethod() throws Exception {
        String jsonContent = "{\"a\": 1234, \"b\": 5678, \"c\": \"getData\"}";
        this.mockHttpTransport = this.createMockHttpTransport(Json.MEDIA_TYPE, jsonContent);
        this.vtsDashboardApiTransport =
                new VtsDashboardApiTransport(this.mockHttpTransport, HttpTesting.SIMPLE_URL);

        RspResult rspResult = this.vtsDashboardApiTransport.get("", RspResult.class);
        assertEquals(rspResult.a, 1234);
        assertEquals(rspResult.b, 5678);
        assertEquals(rspResult.c, "getData");
    }

    @Test
    public void testPostMethod() throws Exception {
        String jsonContent = "{\"a\": 4321, \"b\": 8765, \"c\": \"postData\"}";
        this.mockHttpTransport = this.createMockHttpTransport(Json.MEDIA_TYPE, jsonContent);
        this.vtsDashboardApiTransport =
                new VtsDashboardApiTransport(this.mockHttpTransport, HttpTesting.SIMPLE_URL);

        Map<String, Object> postData = new HashMap<>();
        postData.put("a", "aData");
        postData.put("z", 2343);
        RspResult rspResult = this.vtsDashboardApiTransport.post("", postData, RspResult.class);
        assertEquals(rspResult.a, 4321);
        assertEquals(rspResult.b, 8765);
        assertEquals(rspResult.c, "postData");
    }

    @Test
    public void testPostFile() throws Exception {
        String jsonContent = "{\"resultCode\": 1234, \"resultMsg\": \"success!\"}";
        this.mockHttpTransport = this.createMockHttpTransport(Json.MEDIA_TYPE, jsonContent);
        this.vtsDashboardApiTransport =
                new VtsDashboardApiTransport(this.mockHttpTransport, HttpTesting.SIMPLE_URL);

        File temp = File.createTempFile("temp-post-file", ".tmp");
        String rsp = this.vtsDashboardApiTransport.postFile(
                "", "application/octet-stream", temp.getAbsolutePath());
        assertEquals(rsp, jsonContent);
    }

    @Test
    public void testPostMultiFile() throws Exception {
        String jsonContent = "{\"resultCode\": 1234, \"resultMsg\": \"success!\"}";
        this.mockHttpTransport = this.createMockHttpTransport(Json.MEDIA_TYPE, jsonContent);
        this.vtsDashboardApiTransport =
                new VtsDashboardApiTransport(this.mockHttpTransport, HttpTesting.SIMPLE_URL);

        Map<String, String> postFiles = new HashMap<>();
        postFiles.put("application/octet-stream",
                File.createTempFile("temp-post-file1", ".tmp").getAbsolutePath());
        postFiles.put("application/json",
                File.createTempFile("temp-post-file2", ".tmp").getAbsolutePath());
        String rsp = this.vtsDashboardApiTransport.postMultiFiles("", postFiles);
        assertEquals(rsp, jsonContent);
    }
}
