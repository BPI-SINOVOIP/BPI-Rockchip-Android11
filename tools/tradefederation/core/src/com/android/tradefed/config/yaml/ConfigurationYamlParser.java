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

import com.android.tradefed.command.CommandOptions;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDef;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.config.yaml.IDefaultObjectLoader.LoaderConfiguration;

import com.google.common.collect.ImmutableList;

import org.yaml.snakeyaml.Yaml;
import org.yaml.snakeyaml.error.YAMLException;

import java.io.InputStream;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.ServiceLoader;
import java.util.Set;

/** Parser for YAML style Tradefed configurations */
public final class ConfigurationYamlParser {

    private static final String DESCRIPTION_KEY = "description";
    public static final String DEPENDENCIES_KEY = "dependencies";
    public static final String TESTS_KEY = "tests";

    private static final List<String> REQUIRED_KEYS =
            ImmutableList.of(DESCRIPTION_KEY, DEPENDENCIES_KEY, TESTS_KEY);
    private Set<String> mSeenKeys = new HashSet<>();
    private boolean mCreatedAsModule = false;

    /**
     * Main entry point of the parser to parse a given YAML file into Trade Federation objects.
     *
     * @param configDef
     * @param source
     * @param yamlInput
     * @param createdAsModule
     */
    public void parse(
            ConfigurationDef configDef,
            String source,
            InputStream yamlInput,
            boolean createdAsModule)
            throws ConfigurationException {
        mCreatedAsModule = createdAsModule;
        // We don't support multi-device in YAML
        configDef.setMultiDeviceMode(false);
        Yaml yaml = new Yaml();
        try {
            configDef.addOptionDef(
                    CommandOptions.TEST_TAG_OPTION,
                    null,
                    source,
                    source,
                    Configuration.CMD_OPTIONS_TYPE_NAME);
            Map<String, Object> yamlObjects = (Map<String, Object>) yaml.load(yamlInput);
            translateYamlInTradefed(configDef, yamlObjects);
        } catch (YAMLException e) {
            throw new ConfigurationException(
                    String.format("Failed to parse yaml file: '%s'.", source), e);
        }
    }

    private void translateYamlInTradefed(
            ConfigurationDef configDef, Map<String, Object> yamlObjects)
            throws ConfigurationException {
        if (yamlObjects.containsKey(DESCRIPTION_KEY)) {
            configDef.setDescription((String) yamlObjects.get(DESCRIPTION_KEY));
            mSeenKeys.add(DESCRIPTION_KEY);
        }
        Set<String> dependencyFiles = new LinkedHashSet<>();
        if (yamlObjects.containsKey(DEPENDENCIES_KEY)) {
            YamlTestDependencies testDeps =
                    new YamlTestDependencies(
                            (List<Map<String, Object>>) yamlObjects.get(DEPENDENCIES_KEY));
            dependencyFiles = convertDependenciesToObjects(configDef, testDeps);
            mSeenKeys.add(DEPENDENCIES_KEY);
        }
        if (yamlObjects.containsKey(TESTS_KEY)) {
            YamlTestRunners runnerInfo =
                    new YamlTestRunners((List<Map<String, Object>>) yamlObjects.get(TESTS_KEY));
            mSeenKeys.add(TESTS_KEY);
            convertTestsToObjects(configDef, runnerInfo);
        }

        if (!mSeenKeys.containsAll(REQUIRED_KEYS)) {
            Set<String> missingKeys = new HashSet<>(REQUIRED_KEYS);
            missingKeys.removeAll(mSeenKeys);
            throw new ConfigurationException(
                    String.format("'%s' keys are required and were not found.", missingKeys));
        }

        // Add default configured objects
        LoaderConfiguration loadConfiguration = new LoaderConfiguration();
        loadConfiguration
                .setConfigurationDef(configDef)
                .addDependencies(dependencyFiles)
                .setCreatedAsModule(mCreatedAsModule);
        ServiceLoader<IDefaultObjectLoader> serviceLoader =
                ServiceLoader.load(IDefaultObjectLoader.class);
        for (IDefaultObjectLoader loader : serviceLoader) {
            loader.addDefaultObjects(loadConfiguration);
        }
    }

    /**
     * Converts the test dependencies into target_preparer objects.
     *
     * <p>TODO: Figure out a more robust way to map to target_preparers options.
     *
     * @return returns a list of all the dependency files.
     */
    private Set<String> convertDependenciesToObjects(
            ConfigurationDef def, YamlTestDependencies testDeps) {
        Set<String> dependencies = new LinkedHashSet<>();
        List<String> apks = testDeps.apks();
        if (!apks.isEmpty()) {
            String className = "com.android.tradefed.targetprep.suite.SuiteApkInstaller";
            int classCount =
                    def.addConfigObjectDef(Configuration.TARGET_PREPARER_TYPE_NAME, className);
            String optionName =
                    String.format(
                            "%s%c%d%c%s",
                            className,
                            OptionSetter.NAMESPACE_SEPARATOR,
                            classCount,
                            OptionSetter.NAMESPACE_SEPARATOR,
                            "test-file-name");
            for (String apk : apks) {
                def.addOptionDef(
                        optionName,
                        null,
                        apk,
                        def.getName(),
                        Configuration.TARGET_PREPARER_TYPE_NAME);
            }
            dependencies.addAll(apks);
        }

        Map<String, String> deviceFiles = testDeps.deviceFiles();
        if (!deviceFiles.isEmpty()) {
            String className = "com.android.tradefed.targetprep.PushFilePreparer";
            int classCount =
                    def.addConfigObjectDef(Configuration.TARGET_PREPARER_TYPE_NAME, className);
            String optionName =
                    String.format(
                            "%s%c%d%c%s",
                            className,
                            OptionSetter.NAMESPACE_SEPARATOR,
                            classCount,
                            OptionSetter.NAMESPACE_SEPARATOR,
                            "push-file");
            for (Entry<String, String> toPush : deviceFiles.entrySet()) {
                def.addOptionDef(
                        optionName,
                        toPush.getKey(),
                        toPush.getValue(),
                        def.getName(),
                        Configuration.TARGET_PREPARER_TYPE_NAME);
                dependencies.add(toPush.getKey());
            }
        }
        // Add the non-apk and non-device files
        dependencies.addAll(testDeps.files());
        return dependencies;
    }

    private void convertTestsToObjects(ConfigurationDef def, YamlTestRunners tests) {
        if (tests.getRunner() == null) {
            return;
        }
        String className = tests.getRunner();
        int classCount = def.addConfigObjectDef(Configuration.TEST_TYPE_NAME, className);
        for (Entry<String, String> options : tests.getOptions().entries()) {
            String optionName =
                    String.format(
                            "%s%c%d%c%s",
                            className,
                            OptionSetter.NAMESPACE_SEPARATOR,
                            classCount,
                            OptionSetter.NAMESPACE_SEPARATOR,
                            options.getKey());
            def.addOptionDef(
                    optionName,
                    null,
                    options.getValue(),
                    def.getName(),
                    Configuration.TEST_TYPE_NAME);
        }
    }
}
