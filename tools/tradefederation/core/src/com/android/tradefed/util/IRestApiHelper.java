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

import com.google.api.client.http.HttpResponse;

import org.json.JSONObject;

import java.io.IOException;
import java.util.Map;

/** A helper interface for performing REST API calls. */
public interface IRestApiHelper {

    /**
     * Executes an API request.
     *
     * @param method a HTTP method of the request
     * @param uriParts URL encoded URI parts to be used to construct the request URI.
     * @param options unencoded parameter names and values used to construct the query string
     * @param data data to be sent with the request
     * @return a HttpResponse object
     * @throws IOException
     */
    public HttpResponse execute(
            String method, String[] uriParts, Map<String, Object> options, JSONObject data)
            throws IOException;
}
