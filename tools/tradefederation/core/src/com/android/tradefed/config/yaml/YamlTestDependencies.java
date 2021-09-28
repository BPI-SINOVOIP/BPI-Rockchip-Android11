/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tradefed.config.yaml;

import com.android.tradefed.config.ConfigurationException;

import com.google.common.collect.ImmutableSet;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Helper to parse dependencies file from the YAML Tradefed Configuration. */
class YamlTestDependencies {
    private static final String APKS_KEY = "apks";
    private static final String FILES_KEY = "files";
    private static final String DEVICE_FILES_KEY = "device_files";
    private static final Set<String> DEPENDENCY_KEYS =
            ImmutableSet.of(APKS_KEY, FILES_KEY, DEVICE_FILES_KEY);

    private List<String> mApkDependencies = new ArrayList<>();
    private List<String> mFileDependencies = new ArrayList<>();
    private Map<String, String> mDeviceFileDependencies = new HashMap<>();

    public YamlTestDependencies(List<Map<String, Object>> dependencies)
            throws ConfigurationException {
        if (dependencies == null) {
            return;
        }
        for (Map<String, Object> dependencyEntry : dependencies) {
            if (dependencyEntry.containsKey(APKS_KEY)) {
                for (String apk : (List<String>) dependencyEntry.get(APKS_KEY)) {
                    mApkDependencies.add(apk.trim());
                }
            }
            if (dependencyEntry.containsKey(FILES_KEY)) {
                for (String apk : (List<String>) dependencyEntry.get(FILES_KEY)) {
                    mFileDependencies.add(apk.trim());
                }
            }
            if (dependencyEntry.containsKey(DEVICE_FILES_KEY)) {
                mDeviceFileDependencies =
                        (Map<String, String>) dependencyEntry.get(DEVICE_FILES_KEY);
            }
            if (!DEPENDENCY_KEYS.containsAll(dependencyEntry.keySet())) {
                Set<String> unexpectedKeys = new HashSet<>(dependencyEntry.keySet());
                unexpectedKeys.removeAll(unexpectedKeys);
                throw new ConfigurationException(
                        String.format(
                                "keys '%s' in '%s' are unexpected",
                                unexpectedKeys, ConfigurationYamlParser.DEPENDENCIES_KEY));
            }
        }
    }

    /**
     * Returns the APKs dependencies. Apk dependencies are automatically installed on Android
     * devices.
     */
    public List<String> apks() {
        return new ArrayList<>(mApkDependencies);
    }

    /** Returns the files that will be needed for the run. */
    public List<String> files() {
        return new ArrayList<>(mFileDependencies);
    }

    /** Returns the files that will be needed and need to be set on the device under test. */
    public Map<String, String> deviceFiles() {
        return new HashMap<>(mDeviceFileDependencies);
    }
}
