/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.tradefed.result;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.regex.Pattern;

/** encapsulates compatibility run results for a single app package tested */
public class CompatibilityTestResult {

    public static final String KEY_PACKAGE = "app_package";
    // Keep app_version for backwards compatibility
    public static final String KEY_VERSION_CODE = "app_version_code";
    public static final String KEY_VERSION_STRING = "app_version_string";
    public static final String KEY_NAME = "app_name";
    public static final String KEY_RANK = "app_rank";
    public static final String KEY_STATUS = "status";
    public static final String KEY_MESSAGE = "message";

    public static final String SEPARATOR = "=@ppcomp@t=";
    public static final Pattern REGEX =
            Pattern.compile(String.format("^%s(.*?)%s", SEPARATOR, SEPARATOR));

    public static final String STATUS_SUCCESS = "success";
    public static final String STATUS_ERROR = "error"; // installation errors etc
    public static final String STATUS_FAILURE = "failure"; // app launch failures

    public String packageName = null;
    public String versionString = null;
    public String versionCode = null;
    public String name = null;
    public Integer rank = null;
    public String status = null;
    public String message = null;

    /**
     * Return the Serialized fields into JSON string
     *
     * @throws JSONException
     */
    public String toJsonString() throws JSONException {
        JSONObject o = new JSONObject();
        o.put(KEY_PACKAGE, packageName);
        o.put(KEY_VERSION_STRING, versionString);
        o.put(KEY_VERSION_CODE, versionString);
        o.put(KEY_NAME, name);
        o.put(KEY_RANK, rank);
        o.put(KEY_STATUS, status);
        o.put(KEY_MESSAGE, message);
        return o.toString();
    }

    /**
     * Reconstructs an instance from a JSON string
     *
     * @param json
     * @return the {@link CompatibilityTestResult} instance from the JSON serialized string.
     * @throws JSONException
     */
    public static CompatibilityTestResult fromJsonString(String json) throws JSONException {
        JSONObject o = new JSONObject(json);
        CompatibilityTestResult result = new CompatibilityTestResult();
        result.packageName = o.getString(KEY_PACKAGE);
        if (o.has(KEY_VERSION_STRING)) {
            result.versionString = o.getString(KEY_VERSION_STRING);
        }
        if (o.has(KEY_VERSION_CODE)) {
            result.versionString = o.getString(KEY_VERSION_CODE);
        }
        if (o.has(KEY_NAME)) {
            result.name = o.getString(KEY_NAME);
        }
        if (o.has(KEY_RANK)) {
            result.rank = o.getInt(KEY_RANK);
        }
        result.status = o.getString(KEY_STATUS);
        if (o.has(KEY_MESSAGE)) {
            result.message = o.getString(KEY_MESSAGE);
        }
        return result;
    }
}
