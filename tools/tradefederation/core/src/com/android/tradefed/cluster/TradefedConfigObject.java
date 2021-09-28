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

import com.android.tradefed.config.Configuration;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.util.MultiMap;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * A class to model a TradefedConfigObject message of TFC API.
 *
 * <p>A TradefedConfigObject message is a part of a TestEnvironment message and used to pass down
 * extra {@link ITargetPreparer} or {@link ITestInvocationListener} to be added to a cluster command
 * launcher config.
 */
public class TradefedConfigObject {

    /**
     * A list of configuration object types which can be injected to a cluster command config.
     *
     * <p>This must be a subset of configuration object types defined in {@link Configuration}.
     */
    public static enum Type {
        UNKNOWN,
        TARGET_PREPARER,
        RESULT_REPORTER
    }

    private final Type mType;
    private final String mClassName;
    private final MultiMap<String, String> mOptionValues;

    TradefedConfigObject(Type type, String className, MultiMap<String, String> optionValues) {
        mType = type;
        mClassName = className;
        mOptionValues = optionValues;
    }

    public Type getType() {
        return mType;
    }

    public String getClassName() {
        return mClassName;
    }

    public MultiMap<String, String> getOptionValues() {
        return mOptionValues;
    }

    public static TradefedConfigObject fromJson(JSONObject json) throws JSONException {
        MultiMap<String, String> optionValues = new MultiMap<>();
        JSONArray arr = json.optJSONArray("option_values");
        if (arr != null) {
            for (int i = 0; i < arr.length(); i++) {
                JSONObject pair = arr.getJSONObject(i);
                String key = pair.getString("key");
                JSONArray valueArr = pair.optJSONArray("values");
                if (valueArr == null) {
                    optionValues.put(key, null);
                } else {
                    for (int j = 0; j < valueArr.length(); j++) {
                        optionValues.put(key, valueArr.getString(j));
                    }
                }
            }
        }
        Type type = Type.valueOf(json.optString("type", Type.UNKNOWN.name()));
        return new TradefedConfigObject(type, json.getString("class_name"), optionValues);
    }

    public static List<TradefedConfigObject> fromJsonArray(JSONArray arr) throws JSONException {
        List<TradefedConfigObject> objs = new ArrayList<>();
        for (int i = 0; i < arr.length(); i++) {
            objs.add(fromJson(arr.getJSONObject(i)));
        }
        return objs;
    }
}
