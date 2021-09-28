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

import com.android.tradefed.command.remote.DeviceDescriptor;

import org.json.JSONException;
import org.json.JSONObject;

/** A class to encapsulate cluster device info to be uploaded. */
public class ClusterDeviceInfo {
    private String mRunTarget;
    private String mGroupName;
    private DeviceDescriptor mDeviceDescriptor;

    private ClusterDeviceInfo(
            DeviceDescriptor deviceDescriptor, String runTarget, String groupName) {
        mDeviceDescriptor = deviceDescriptor;
        mRunTarget = runTarget;
        mGroupName = groupName;
    }

    public String getRunTarget() {
        return mRunTarget;
    }

    public String getGroupName() {
        return mGroupName;
    }

    public DeviceDescriptor getDeviceDescriptor() {
        return mDeviceDescriptor;
    }

    public static class Builder {
        private DeviceDescriptor mDeviceDescriptor;
        private String mRunTarget;
        private String mGroupName;

        public Builder() {}

        public Builder setRunTarget(final String runTarget) {
            mRunTarget = runTarget;
            return this;
        }

        public Builder setGroupName(final String groupName) {
            mGroupName = groupName;
            return this;
        }

        public Builder setDeviceDescriptor(final DeviceDescriptor deviceDescriptor) {
            mDeviceDescriptor = deviceDescriptor;
            return this;
        }

        public ClusterDeviceInfo build() {
            final ClusterDeviceInfo deviceInfo =
                    new ClusterDeviceInfo(mDeviceDescriptor, mRunTarget, mGroupName);
            return deviceInfo;
        }
    }

    /**
     * Generates the JSON Object for this device info.
     *
     * @return JSONObject equivalent of this device info.
     * @throws JSONException
     */
    public JSONObject toJSON() throws JSONException {
        final JSONObject json = new JSONObject();
        json.put("device_serial", mDeviceDescriptor.getSerial());
        json.put("run_target", mRunTarget);
        json.put("build_id", mDeviceDescriptor.getBuildId());
        json.put("product", mDeviceDescriptor.getProduct());
        json.put("product_variant", mDeviceDescriptor.getProductVariant());
        json.put("sdk_version", mDeviceDescriptor.getSdkVersion());
        json.put("battery_level", mDeviceDescriptor.getBatteryLevel());
        json.put("mac_address", mDeviceDescriptor.getMacAddress());
        json.put("sim_state", mDeviceDescriptor.getSimState());
        json.put("sim_operator", mDeviceDescriptor.getSimOperator());
        json.put("state", mDeviceDescriptor.getState());
        json.put("group_name", mGroupName);
        return json;
    }
}
