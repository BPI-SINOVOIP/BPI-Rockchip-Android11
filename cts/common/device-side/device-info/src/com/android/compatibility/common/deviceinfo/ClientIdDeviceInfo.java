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
package com.android.compatibility.common.deviceinfo;

import android.util.Log;

import com.android.compatibility.common.util.DeviceInfoStore;
import com.android.compatibility.common.util.PropertyUtil;

import java.io.IOException;
import java.util.Map;

/**
 * Client ID info collector.
 */
public final class ClientIdDeviceInfo extends DeviceInfo {

    private static final String LOG_TAG = "ClientIdDeviceInfo";

    @Override
    protected void collectDeviceInfo(DeviceInfoStore store) throws Exception {
        try {
            collectClientIds(store);
        } catch (IOException e) {
            Log.w(LOG_TAG, "Failed to collect client IDs", e);
        }
    }

    private void collectClientIds(DeviceInfoStore store) throws IOException {
        store.startArray("client_id");
        Map<String, String> clientIds = PropertyUtil.getClientIds();
        for (String name : clientIds.keySet()) {
            store.startGroup();
            store.addResult("name", name);
            store.addResult("value", clientIds.get(name));
            store.endGroup();
        }
        store.endArray();
    }
}
