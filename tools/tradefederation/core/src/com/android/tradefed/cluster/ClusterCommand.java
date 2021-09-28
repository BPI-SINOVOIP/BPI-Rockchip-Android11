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

import com.android.tradefed.util.UniqueMultiMap;

import com.google.common.base.Strings;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/** A class that represents a task fetched from TF Cluster. */
public class ClusterCommand {

    public static enum RequestType {
        /** An unmanaged request: the command line will run as is by the current TF process. */
        UNMANAGED,
        /** A managed request: the command line will run by a new TF process. */
        MANAGED;
    }

    /** Command's status in the TF cluster. */
    public static enum State {
        /** Initial state, or failed to determine state. */
        UNKNOWN,
        /** Inserted into the cluster's queue. */
        QUEUED,
        /** Currently being executed. */
        RUNNING,
        /** Canceled by user, or failed to allocate a device. */
        CANCELED,
        /** Completed successfully. */
        COMPLETED,
        /** Completed exceptionally. */
        ERROR,
        /** Non-retryable error, e.g. invalid configuration. */
        FATAL
    }

    private final String mTaskId;
    private final String mRequestId;
    private final String mCommandId;
    private final String mCommandLine;
    private final RequestType mRequestType;
    private final Integer mShardCount;
    private final Integer mShardIndex;
    private final String mAttemptId;
    // Devices try to match the current command.
    private List<String> mTargetDeviceSerials = new ArrayList<>();
    // Additional options to inject
    private UniqueMultiMap<String, String> mExtraOptions = new UniqueMultiMap<>();

    public ClusterCommand(String commandId, String taskId, String cmdLine) {
        this(null, commandId, taskId, cmdLine, null, RequestType.UNMANAGED, null, null);
    }

    /**
     * Constructor.
     *
     * @param requestId A request ID
     * @param commandId The ID of the command that issued this task
     * @param taskId The ID of this task
     * @param cmdLine The command line to run
     * @param requestType A request type
     * @param shardCount A shard count
     * @param shardIndex A shard index
     */
    public ClusterCommand(
            String requestId,
            String commandId,
            String taskId,
            String cmdLine,
            String attemptId,
            RequestType requestType,
            Integer shardCount,
            Integer shardIndex) {
        mTaskId = taskId;
        mRequestId = requestId;
        mCommandId = commandId;
        mCommandLine = cmdLine;
        mRequestType = requestType;
        mShardCount = shardCount;
        mShardIndex = shardIndex;
        if (!Strings.isNullOrEmpty(attemptId)) {
            mAttemptId = attemptId;
        } else {
            // TODO(b/123294120): Remove creating attemptId on TF side once
            // b/123294120 rolls out.
            mAttemptId = UUID.randomUUID().toString();
        }
    }

    /**
     * Returns the task ID.
     *
     * @return task ID.
     */
    public String getTaskId() {
        return mTaskId;
    }

    /**
     * Returns the request ID.
     *
     * @return the request ID
     */
    public String getRequestId() {
        return mRequestId;
    }

    /**
     * Returns the command ID.
     *
     * @return the command ID
     */
    public String getCommandId() {
        return mCommandId;
    }

    /**
     * Returns the attempt ID. The attempt is randomly generated GUID used to distinguish multiple
     * command runs.
     *
     * @return the attempt ID
     */
    public String getAttemptId() {
        return mAttemptId;
    }

    /**
     * Returns the command line string.
     *
     * @return the command line string.
     */
    public String getCommandLine() {
        return mCommandLine;
    }

    /**
     * Returns a request type
     *
     * @return a request type
     */
    public RequestType getRequestType() {
        return mRequestType;
    }

    /**
     * Returns the list of target device serials on which this command will attempt to run.
     *
     * @return the list of target device serials
     */
    public List<String> getTargetDeviceSerials() {
        return mTargetDeviceSerials;
    }

    /**
     * Sets the list of target device serials on which the command will try to run.
     *
     * @param targetDeviceSerials the list of device serials to set
     */
    public void setTargetDeviceSerials(List<String> targetDeviceSerials) {
        this.mTargetDeviceSerials = targetDeviceSerials;
    }

    /** @return multimap of additional options to inject */
    public UniqueMultiMap<String, String> getExtraOptions() {
        return mExtraOptions;
    }

    /**
     * Returns a shard count.
     *
     * @return a shard count.
     */
    public Integer getShardCount() {
        return mShardCount;
    }

    /**
     * Returns a shard index.
     *
     * @return a shard index.
     */
    public Integer getShardIndex() {
        return mShardIndex;
    }

    private static Integer optInteger(JSONObject json, String name) throws JSONException {
        if (json.isNull(name)) {
            return null;
        }
        return json.getInt(name);
    }

    public static ClusterCommand fromJson(JSONObject json) throws JSONException {
        ClusterCommand command =
                new ClusterCommand(
                        json.getString("request_id"),
                        json.getString("command_id"),
                        json.getString("task_id"),
                        json.getString("command_line"),
                        json.optString("attempt_id", null),
                        RequestType.valueOf(
                                json.optString("request_type", RequestType.UNMANAGED.name())),
                        optInteger(json, "shard_count"),
                        optInteger(json, "shard_index"));
        JSONArray jsonDeviceSerials = json.optJSONArray("device_serials");
        if (jsonDeviceSerials != null) {
            final List<String> deviceSerials = new ArrayList<>();
            for (int j = 0; j < jsonDeviceSerials.length(); j++) {
                deviceSerials.add(jsonDeviceSerials.getString(j));
            }
            command.setTargetDeviceSerials(deviceSerials);
        }
        JSONArray extraOptions = json.optJSONArray("extra_options");
        if (extraOptions != null) {
            for (int i = 0; i < extraOptions.length(); i++) {
                JSONObject entry = extraOptions.getJSONObject(i);
                String key = entry.getString("key");
                JSONArray values = entry.getJSONArray("values");
                for (int j = 0; j < values.length(); j++) {
                    command.mExtraOptions.put(key, values.getString(j));
                }
            }
        }
        return command;
    }
}
