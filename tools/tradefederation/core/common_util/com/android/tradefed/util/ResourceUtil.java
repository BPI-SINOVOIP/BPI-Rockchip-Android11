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

package com.android.tradefed.util;

import com.android.tradefed.log.LogUtil.CLog;

import com.google.common.base.Strings;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;

/** Utility for reading configuration resources. */
public class ResourceUtil {

    /**
     * Read a property configuration from the resources. Configuration must follow a "key=value" or
     * "key:value" format. Method is safe and will return an empty map in case of error.
     *
     * @param resource The path of the resource to read.
     * @return a {@link Map} of the loaded resources.
     * @see Properties
     */
    public static Map<String, String> readConfigurationFromResource(String resource) {
        try (InputStream resStream = ResourceUtil.class.getResourceAsStream(resource)) {
            return readConfigurationFromStream(resStream);
        } catch (IOException e) {
            CLog.e(e);
        }
        return new HashMap<>();
    }

    /**
     * Read a property configuration from a file. Configuration must follow a "key=value" or
     * "key:value" format. Method is safe and will return an empty map in case of error.
     *
     * @param propertyFile The path of the resource to read.
     * @return a {@link Map} of the loaded resources.
     * @see Properties
     */
    public static Map<String, String> readConfigurationFromFile(File propertyFile) {
        try (InputStream resStream = new FileInputStream(propertyFile)) {
            return readConfigurationFromStream(resStream);
        } catch (IOException e) {
            CLog.e(e);
        }
        return new HashMap<>();
    }

    /**
     * Read a property configuration from a stream. Configuration must follow a "key=value" or
     * "key:value" format. Method is safe and will return an empty map in case of error.
     *
     * @param propertyStream The path of the resource to read.
     * @return a {@link Map} of the loaded resources.
     * @see Properties
     */
    public static Map<String, String> readConfigurationFromStream(InputStream propertyStream) {
        Map<String, String> resources = new HashMap<>();
        try {
            if (propertyStream == null) {
                CLog.w("No configuration stream");
                return resources;
            }
            Properties properties = new Properties();
            properties.load(propertyStream);
            for (Object key : properties.keySet()) {
                String keyString = (String) key;
                String valueString = (String) properties.get(key);
                if (Strings.isNullOrEmpty(valueString)) {
                    continue;
                }
                resources.put(keyString, valueString);
            }
        } catch (IOException e) {
            CLog.e(e);
        }
        return resources;
    }
}
