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

import com.google.common.collect.LinkedListMultimap;
import com.google.common.collect.Multimap;

import java.util.List;
import java.util.Map;
import java.util.Map.Entry;

/** Helper to parse test runner information from the YAML Tradefed Configuration. */
public class YamlTestRunners {

    private static final String TEST_KEY = "test";
    private static final String TEST_NAME_KEY = "name";
    private static final String OPTIONS_KEY = "options";

    private String mRunner;
    private Multimap<String, String> mOptions = LinkedListMultimap.create();

    public YamlTestRunners(List<Map<String, Object>> tests) throws ConfigurationException {
        if (tests.size() > 1) {
            throw new ConfigurationException("Currently only support one runner at a time.");
        }
        for (Map<String, Object> runnerEntry : tests) {
            if (runnerEntry.containsKey(TEST_KEY)) {
                for (Entry<String, Object> entry :
                        ((Map<String, Object>) runnerEntry.get(TEST_KEY)).entrySet()) {
                    if (TEST_NAME_KEY.equals(entry.getKey())) {
                        mRunner = (String) entry.getValue();
                    }
                    if (OPTIONS_KEY.equals(entry.getKey())) {
                        for (Map<String, Object> optionMap :
                                (List<Map<String, Object>>) entry.getValue()) {
                            for (Entry<String, Object> optionVal : optionMap.entrySet()) {
                                // TODO: Support map option
                                mOptions.put(optionVal.getKey(), optionVal.getValue().toString());
                            }
                        }
                    }
                }
            } else {
                throw new ConfigurationException(
                        String.format(
                                "'%s' key is mandatory in '%s'",
                                TEST_KEY, ConfigurationYamlParser.TESTS_KEY));
            }
        }
    }

    /** Returns the test runner to be used. */
    public String getRunner() {
        return mRunner;
    }

    /** Returns the options for the test runner */
    public Multimap<String, String> getOptions() {
        return mOptions;
    }
}
