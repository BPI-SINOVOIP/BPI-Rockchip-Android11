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

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A class to store invocation status.
 *
 * <p>This also includes statuses of test groups that run within the invocation.
 */
public class InvocationStatus {

    private List<TestGroupStatus> mTestGroupStatuses = new ArrayList<TestGroupStatus>();

    public void addTestGroupStatus(final TestGroupStatus progress) {
        mTestGroupStatuses.add(progress);
    }

    public List<TestGroupStatus> getTestGroupStatuses() {
        return Collections.unmodifiableList(mTestGroupStatuses);
    }

    public JSONObject toJSON() throws JSONException {
        final JSONObject json = new JSONObject();
        final JSONArray testModuleStatuses = new JSONArray();
        for (final TestGroupStatus o : mTestGroupStatuses) {
            testModuleStatuses.put(o.toJSON());
        }
        json.put("test_group_statuses", testModuleStatuses);
        return json;
    }
}
