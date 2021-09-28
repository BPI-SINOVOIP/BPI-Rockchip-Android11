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

import org.json.JSONException;
import org.json.JSONObject;

/**
 * A class to store status of a test group.
 *
 * <p>A test group corresponds to a group of test cases reported under a same test run name.
 */
public class TestGroupStatus {

    private String mName;
    private int mTotalTestCount;
    private int mCompletedTestCount;
    private int mFailedTestCount;
    private int mPassedTestCount;
    private boolean mIsComplete;
    private long mElapsedTime;

    public TestGroupStatus(
            final String name,
            final int totalTestCount,
            final int completedTestCount,
            final int failedTestCount,
            final int passedTestCount,
            final boolean isComplete,
            final long elapsedTime) {
        mName = name;
        mTotalTestCount = totalTestCount;
        mCompletedTestCount = completedTestCount;
        mFailedTestCount = failedTestCount;
        mPassedTestCount = passedTestCount;
        mIsComplete = isComplete;
        mElapsedTime = elapsedTime;
    }

    public String getName() {
        return mName;
    }

    public int getTotalTestCount() {
        return mTotalTestCount;
    }

    public int getCompletedTestCount() {
        return mCompletedTestCount;
    }

    public int getFailedTestCount() {
        return mFailedTestCount;
    }

    public int getPassedTestCount() {
        return mPassedTestCount;
    }

    public boolean isComplete() {
        return mIsComplete;
    }

    public long getElapsedTime() {
        return mElapsedTime;
    }

    public JSONObject toJSON() throws JSONException {
        final JSONObject json = new JSONObject();
        json.put("name", mName);
        json.put("total_test_count", mTotalTestCount);
        json.put("completed_test_count", mCompletedTestCount);
        json.put("failed_test_count", mFailedTestCount);
        json.put("passed_test_count", mPassedTestCount);
        json.put("is_complete", mIsComplete);
        json.put("elapsed_time", mElapsedTime);
        return json;
    }
}
