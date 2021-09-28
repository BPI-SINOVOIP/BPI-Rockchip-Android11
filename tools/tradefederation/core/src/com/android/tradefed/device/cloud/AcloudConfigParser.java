/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.tradefed.device.cloud;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/** Helper class that parse an Acloud config (used to start a Cloud device instance). */
public class AcloudConfigParser {

    /** Sets of key that can be searched from the configuration. */
    public enum AcloudKeys {
        SERVICE_ACCOUNT_NAME("service_account_name"),
        SERVICE_ACCOUNT_PRIVATE_KEY("service_account_private_key_path"),
        ZONE("zone"),
        PROJECT("project"),
        SERVICE_ACCOUNT_JSON_PRIVATE_KEY("service_account_json_private_key_path"),
        STABLE_HOST_IMAGE_NAME("stable_host_image_name"),
        STABLE_HOST_IMAGE_PROJECT("stable_host_image_project");

        private String mName;

        private AcloudKeys(String name) {
            mName = name;
        }

        @Override
        public String toString() {
            return mName;
        }
    }

    private Map<String, String> mConfigValues;

    private AcloudConfigParser() {
        mConfigValues = new HashMap<>();
    }

    /**
     * Parse an acloud configuration file and returns a populated {@link AcloudConfigParser} that
     * can be queried for values. Returns null in case of errors.
     */
    public static AcloudConfigParser parseConfig(File configFile) {
        if (configFile == null || !configFile.exists()) {
            CLog.e("Could not read acloud config file: %s.", configFile);
            return null;
        }
        AcloudConfigParser config = new AcloudConfigParser();
        try {
            String content = FileUtil.readStringFromFile(configFile);
            String[] lines = content.split("\n");
            for (String line : lines) {
                if (line.contains(": ")) {
                    String key = line.split(": ")[0];
                    String value = line.split(": ")[1].replace("\"", "");
                    config.mConfigValues.put(key, value);
                }
            }
        } catch (IOException e) {
            CLog.e(e);
            return null;
        }
        return config;
    }

    /** Returns the value associated with the Acloud key, null if no value. */
    public String getValueForKey(AcloudKeys key) {
        return mConfigValues.get(key.toString());
    }
}
