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

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** A class to encapsulate cluster command events to be uploaded. */
public class ClusterCommandEvent implements IClusterEvent {

    public static final String DATA_KEY_ERROR = "error";
    public static final String DATA_KEY_SUMMARY = "summary";
    public static final String DATA_KEY_SETUP_TIME_MILLIS = "setup_time_millis";
    public static final String DATA_KEY_FETCH_BUILD_TIME_MILLIS = "fetch_build_time_millis";
    public static final String DATA_KEY_TOTAL_TEST_COUNT = "total_test_count";
    public static final String DATA_KEY_FAILED_TEST_COUNT = "failed_test_count";
    public static final String DATA_KEY_PASSED_TEST_COUNT = "passed_test_count";
    public static final String DATA_KEY_FAILED_TEST_RUN_COUNT = "failed_test_run_count";
    public static final String DATA_KEY_LOST_DEVICE_DETECTED = "device_lost_detected";

    // Maximum size of an individual data string value.
    public static final int MAX_DATA_STRING_SIZE = 4095;

    public enum Type {
        AllocationFailed,
        ConfigurationError,
        FetchFailed,
        ExecuteFailed,
        InvocationInitiated,
        InvocationStarted,
        InvocationFailed,
        InvocationEnded,
        InvocationCompleted,
        TestRunInProgress,
        TestEnded
    }

    private long mTimestamp;
    private Type mType;
    private String mCommandTaskId;
    private String mAttemptId;
    private String mHostName;
    private InvocationStatus mInvocationStatus;
    private Map<String, Object> mData = new HashMap<>();
    private Set<String> mDeviceSerials;

    private ClusterCommandEvent() {}

    public String getHostName() {
        return mHostName;
    }

    public long getTimestamp() {
        return mTimestamp;
    }

    public Type getType() {
        return mType;
    }

    public String getCommandTaskId() {
        return mCommandTaskId;
    }

    public String getAttemptId() {
        return mAttemptId;
    }

    public InvocationStatus getInvocationStatus() {
        return mInvocationStatus;
    }

    public Map<String, Object> getData() {
        return mData;
    }

    public Set<String> getDeviceSerials() {
        return mDeviceSerials;
    }

    public static class Builder {

        private long mTimestamp = System.currentTimeMillis();
        private Type mType;
        private String mCommandTaskId;
        private String mAttemptId;
        private String mHostName;
        private InvocationStatus mInvocationStatus;
        private Map<String, Object> mData = new HashMap<>();
        private Set<String> mDeviceSerials = new HashSet<>();

        public Builder() {}

        public Builder setTimestamp(final long timestamp) {
            mTimestamp = timestamp;
            return this;
        }

        public Builder setType(final Type type) {
            mType = type;
            return this;
        }

        public Builder setCommandTaskId(final String commandTaskId) {
            mCommandTaskId = commandTaskId;
            return this;
        }

        public Builder setAttemptId(final String attemptId) {
            mAttemptId = attemptId;
            return this;
        }

        public Builder setHostName(final String hostName) {
            mHostName = hostName;
            return this;
        }

        public Builder setInvocationStatus(final InvocationStatus invocationStatus) {
            mInvocationStatus = invocationStatus;
            return this;
        }

        public Builder setData(final String name, final Object value) {
            if (value instanceof String && ((String) value).length() > MAX_DATA_STRING_SIZE) {
                CLog.w(
                        String.format(
                                "Data for '%s' exceeds %d characters, and has been truncated.",
                                name, MAX_DATA_STRING_SIZE));
                mData.put(name, ((String) value).substring(0, MAX_DATA_STRING_SIZE));
            } else {
                mData.put(name, value);
            }
            return this;
        }

        public Builder setDeviceSerials(final Set<String> deviceSerials) {
            mDeviceSerials = deviceSerials;
            return this;
        }

        public Builder addDeviceSerial(final String deviceSerial) {
            mDeviceSerials.add(deviceSerial);
            return this;
        }

        public ClusterCommandEvent build() {
            final ClusterCommandEvent obj = new ClusterCommandEvent();
            obj.mTimestamp = mTimestamp;
            obj.mType = mType;
            obj.mCommandTaskId = mCommandTaskId;
            obj.mAttemptId = mAttemptId;
            obj.mHostName = mHostName;
            obj.mInvocationStatus = mInvocationStatus;
            obj.mData = new HashMap<>(mData);
            obj.mDeviceSerials = mDeviceSerials;
            return obj;
        }
    }

    /**
     * Creates a base {@link Builder}.
     *
     * @return a {@link Builder}.
     */
    public static Builder createEventBuilder() {
        return createEventBuilder(null);
    }

    /**
     * Creates a base {@link Builder} for the given {@link ClusterCommand}.
     *
     * @return a {@link Builder}.
     */
    public static Builder createEventBuilder(final ClusterCommand command) {
        final ClusterCommandEvent.Builder builder = new ClusterCommandEvent.Builder();
        if (command != null) {
            builder.setCommandTaskId(command.getTaskId());
            builder.setAttemptId(command.getAttemptId());
        }
        return builder;
    }

    /** {@inheritDoc} */
    @Override
    public JSONObject toJSON() throws JSONException {
        final JSONObject json = new JSONObject();
        json.put("type", this.getType().toString());
        // event time should be in POSIX timestamp.
        json.put("time", this.getTimestamp() / 1000);
        json.put("task_id", this.getCommandTaskId());
        json.put("attempt_id", this.getAttemptId());
        json.put("hostname", this.getHostName());
        // TODO(b/79583735): deprecated.
        if (!this.getDeviceSerials().isEmpty()) {
            json.put("device_serial", this.getDeviceSerials().iterator().next());
        }
        json.put("device_serials", new JSONArray(this.getDeviceSerials()));
        if (mInvocationStatus != null) {
            json.put("invocation_status", mInvocationStatus.toJSON());
        }
        json.put("data", new JSONObject(this.getData()));
        return json;
    }

    @Override
    public String toString() {
        String str = null;
        try {
            str = toJSON().toString();
        } catch (final JSONException e) {
            // ignore
        }
        return str;
    }
}
