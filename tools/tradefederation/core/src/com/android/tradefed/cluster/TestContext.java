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
package com.android.tradefed.cluster;

import com.android.tradefed.log.LogUtil.CLog;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * A class to model a TestContext message of TFC API.
 *
 * <p>A TestContext message is used to store and retrieve contextual information being passed across
 * multiple attempts of same test command.
 */
public class TestContext {

    String mCommandLine = null;
    final Map<String, String> mEnvVars = new TreeMap<>();
    final List<TestResource> mTestResources = new ArrayList<>();

    public String getCommandLine() {
        return mCommandLine;
    }

    public void setCommandLine(String commandLine) {
        mCommandLine = commandLine;
    }

    public Map<String, String> getEnvVars() {
        return Collections.unmodifiableMap(mEnvVars);
    }

    public void addEnvVars(Map<String, String> envVars) {
        mEnvVars.putAll(envVars);
    }

    public List<TestResource> getTestResources() {
        return Collections.unmodifiableList(mTestResources);
    }

    public void addTestResource(TestResource testResource) {
        mTestResources.add(testResource);
    }

    public static TestContext fromJson(JSONObject json) throws JSONException {
        final TestContext obj = new TestContext();
        obj.mCommandLine = json.optString("command_line");
        final JSONArray envVars = json.optJSONArray("env_vars");
        if (envVars != null) {
            for (int i = 0; i < envVars.length(); i++) {
                final JSONObject pair = envVars.getJSONObject(i);
                obj.mEnvVars.put(pair.getString("key"), pair.getString("value"));
            }
        }
        final JSONArray testResources = json.optJSONArray("test_resources");
        if (testResources != null) {
            obj.mTestResources.addAll(TestResource.fromJsonArray(testResources));
        }
        return obj;
    }

    public JSONObject toJson() throws JSONException {
        final JSONObject json = new JSONObject();
        json.put("command_line", mCommandLine);
        final JSONArray envVars = new JSONArray();
        for (Map.Entry<String, String> entry : mEnvVars.entrySet()) {
            final JSONObject pair = new JSONObject();
            pair.put("key", entry.getKey());
            pair.put("value", entry.getValue());
            envVars.put(pair);
        }
        json.put("env_vars", envVars);
        final JSONArray testResources = new JSONArray();
        for (TestResource obj : mTestResources) {
            testResources.put(obj.toJson());
        }
        json.put("test_resources", testResources);
        return json;
    }

    @Override
    public String toString() {
        try {
            return toJson().toString();
        } catch (JSONException e) {
            CLog.w("Failed to convert to JSON: %s", e);
        }
        return null;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof TestContext)) {
            return false;
        }

        return toString().equals(o.toString());
    }
}
