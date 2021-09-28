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

import com.android.tradefed.command.CommandScheduler;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** A class to encapsulate cluster host events to be uploaded. */
public class ClusterHostEvent implements IClusterEvent {
    private long mTimestamp;
    private String mHostName;
    private String mTfVersion;
    private String mClusterId;
    private List<String> mNextClusterIds;
    private List<ClusterDeviceInfo> mDeviceInfos = new ArrayList<>();
    private Map<String, String> mData = new HashMap<>();
    private String mLabName;
    public static final String EVENT_QUEUE = "host-event-queue";

    /** Enums of the different types of host events. */
    public enum HostEventType {
        DeviceSnapshot("DeviceSnapshot", "DEVICE_SNAPSHOT"),
        HostStateChanged("HostStateChanged", "HOST_STATE_CHANGED");
        private final String mName;
        // The AndroidEngProdAPIName should map to
        // https://cs.corp.google.com/piper///depot/google3/google/internal/android/engprod/proto/test/v1/androidtest.proto?rcl=221372995&l=80
        private final String mAndroidEngProdAPIName;

        private HostEventType(final String name, final String androidEngProdAPIName) {
            mName = name;
            mAndroidEngProdAPIName = androidEngProdAPIName;
        }

        @Override
        public String toString() {
            return mName;
        }

        public String getAndroidEngProdAPIName() {
            return mAndroidEngProdAPIName;
        }
    }

    private HostEventType mType;
    private CommandScheduler.HostState mHostState = CommandScheduler.HostState.UNKNOWN;

    private ClusterHostEvent() {}

    public long getTimestamp() {
        return mTimestamp;
    }

    public String getHostName() {
        return mHostName;
    }

    public String getTfVersion() {
        return mTfVersion;
    }

    public String getClusterId() {
        return mClusterId;
    }

    public CommandScheduler.HostState getHostState() {
        return mHostState;
    }

    public long getTfStartTime() {
        return ClusterHostUtil.getTfStartTimeMillis();
    }

    public List<ClusterDeviceInfo> getDeviceInfos() {
        return mDeviceInfos;
    }

    public Map<String, String> getData() {
        return mData;
    }

    public HostEventType getType() {
        return mType;
    }

    public List<String> getNextClusterIds() {
        return mNextClusterIds;
    }

    public String getLabName() {
        return mLabName;
    }

    public static class Builder {
        private HostEventType mType;
        private long mTimestamp = System.currentTimeMillis();
        private String mHostName;
        private String mTfVersion;
        private String mClusterId;
        private List<String> mNextClusterIds;
        private List<ClusterDeviceInfo> mDeviceInfos = new ArrayList<ClusterDeviceInfo>();
        private Map<String, String> mData = new HashMap<>();
        private CommandScheduler.HostState mHostState = CommandScheduler.HostState.UNKNOWN;
        private String mLabName;

        public Builder() {}

        public Builder setHostEventType(final HostEventType type) {
            mType = type;
            return this;
        }

        public Builder setTimestamp(final long timestamp) {
            mTimestamp = timestamp;
            return this;
        }

        public Builder setClusterId(final String clusterId) {
            mClusterId = clusterId;
            return this;
        }

        public Builder setHostName(final String hostname) {
            mHostName = hostname;
            return this;
        }

        public Builder setTfVersion(final String tfVersion) {
            mTfVersion = tfVersion;
            return this;
        }

        public Builder addDeviceInfo(final ClusterDeviceInfo deviceInfo) {
            mDeviceInfos.add(deviceInfo);
            return this;
        }

        public Builder addDeviceInfos(List<ClusterDeviceInfo> deviceInfos) {
            mDeviceInfos.addAll(deviceInfos);
            return this;
        }

        public Builder setData(final String name, final String value) {
            mData.put(name, value);
            return this;
        }

        public Builder setData(Map<String, String> data) {
            mData.putAll(data);
            return this;
        }

        public Builder setNextClusterIds(List<String> nexClusterIds) {
            mNextClusterIds = nexClusterIds;
            return this;
        }

        public Builder setHostState(CommandScheduler.HostState state) {
            mHostState = state;
            return this;
        }

        public Builder setLabName(String labName) {
            mLabName = labName;
            return this;
        }

        public ClusterHostEvent build() {
            final ClusterHostEvent event = new ClusterHostEvent();
            event.mType = mType;
            event.mTimestamp = mTimestamp;
            event.mHostName = mHostName;
            event.mTfVersion = mTfVersion;
            event.mClusterId = mClusterId;
            event.mDeviceInfos = new ArrayList<>(mDeviceInfos);
            event.mData = new HashMap<>(mData);
            event.mNextClusterIds = mNextClusterIds;
            event.mHostState = mHostState;
            event.mLabName = mLabName;
            return event;
        }
    }

    /** {@inheritDoc} */
    @Override
    public JSONObject toJSON() throws JSONException {
        final JSONObject json = new JSONObject();
        // event time should be in POSIX timestamp.
        json.put("time", this.getTimestamp() / 1000);
        if (this.getType() != null) json.put("type", this.getType().toString());
        json.put("hostname", this.getHostName());
        json.put("tf_version", this.getTfVersion());
        json.put("cluster", this.getClusterId());
        JSONArray deviceInfos = new JSONArray();
        for (ClusterDeviceInfo d : this.getDeviceInfos()) {
            deviceInfos.put(d.toJSON());
        }
        json.put("device_infos", deviceInfos);
        json.put("data", new JSONObject(this.getData()));
        if (this.getNextClusterIds() != null) {
            json.put("next_cluster_ids", new JSONArray(this.getNextClusterIds()));
        }
        if (this.getLabName() != null) json.put("lab_name", this.getLabName());
        json.put("state", this.getHostState().toString());
        json.put("tf_start_time_seconds", this.getTfStartTime() / 1000);
        return json;
    }
}
