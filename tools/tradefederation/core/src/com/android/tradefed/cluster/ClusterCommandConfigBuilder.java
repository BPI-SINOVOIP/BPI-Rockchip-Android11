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

import com.android.tradefed.config.ArgsOptionParser;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DeviceConfigurationHolder;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.log.SimpleFileLogger;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.StringUtil;
import com.android.tradefed.util.UniqueMultiMap;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.reflect.InvocationTargetException;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/** A class to build a configuration file for a cluster command. */
public class ClusterCommandConfigBuilder {

    private static final String TEST_TAG = "cluster_command_launcher";

    private ClusterCommand mCommand;
    private TestEnvironment mTestEnvironment;
    private List<TestResource> mTestResources;
    private TestContext mTestContext;
    private File mWorkDir;

    /**
     * Set a {@link ClusterCommand} object.
     *
     * @param command a {@link ClusterCommand} object.
     * @return {@link ClusterCommandConfigBuilder} for chaining.
     */
    public ClusterCommandConfigBuilder setClusterCommand(ClusterCommand command) {
        mCommand = command;
        return this;
    }

    /**
     * Set a {@link TestEnvironment} object.
     *
     * @param testEnvironment a {@link TestEnvironment} object.
     * @return {@link ClusterCommandConfigBuilder} for chaining.
     */
    public ClusterCommandConfigBuilder setTestEnvironment(TestEnvironment testEnvironment) {
        mTestEnvironment = testEnvironment;
        return this;
    }

    /**
     * Set a list of {@link TestResource} object.
     *
     * @param testResources a list of {@link TestResource} objects.
     * @return {@link ClusterCommandConfigBuilder} for chaining.
     */
    public ClusterCommandConfigBuilder setTestResources(List<TestResource> testResources) {
        mTestResources = testResources;
        return this;
    }

    /**
     * Set a {@link TestContext} object.
     *
     * @param testContext a {@link TestContext} object.
     * @return {@link ClusterCommandConfigBuilder} for chaining.
     */
    public ClusterCommandConfigBuilder setTestContext(TestContext testContext) {
        mTestContext = testContext;
        return this;
    }

    /**
     * Set a work directory for a command.
     *
     * @param workDir a work directory.
     * @return {@link ClusterCommandConfigBuilder} for chaining.
     */
    public ClusterCommandConfigBuilder setWorkDir(File workDir) {
        mWorkDir = workDir;
        return this;
    }

    /** Get a {@link IConfiguration} object type name for {@link TradefedConfigObject.Type}. */
    private String getConfigObjectTypeName(TradefedConfigObject.Type type) {
        switch (type) {
            case TARGET_PREPARER:
                return Configuration.TARGET_PREPARER_TYPE_NAME;
            case RESULT_REPORTER:
                return Configuration.RESULT_REPORTER_TYPE_NAME;
            default:
                throw new UnsupportedOperationException(String.format("%s is not supported", type));
        }
    }

    /** Create a {@link IConfiguration} object for a {@link TradefedConfigObject}. */
    private Object createConfigObject(
            TradefedConfigObject configObjDef, Map<String, String> envVars)
            throws ConfigurationException {
        Object configObj = null;
        try {
            configObj =
                    Class.forName(configObjDef.getClassName())
                            .getDeclaredConstructor()
                            .newInstance();
        } catch (InstantiationException
                | IllegalAccessException
                | ClassNotFoundException
                | InvocationTargetException
                | NoSuchMethodException e) {
            throw new ConfigurationException(
                    String.format(
                            "Failed to add a config object '%s'", configObjDef.getClassName()),
                    e);
        }
        MultiMap<String, String> optionValues = configObjDef.getOptionValues();
        List<String> optionArgs = new ArrayList<>();
        for (String name : optionValues.keySet()) {
            List<String> values = optionValues.get(name);
            for (String value : values) {
                optionArgs.add(String.format("--%s", name));
                if (value != null) {
                    // value can be null for valueless options.
                    optionArgs.add(StringUtil.expand(value, envVars));
                }
            }
        }
        ArgsOptionParser parser = new ArgsOptionParser(configObj);
        parser.parse(optionArgs);
        return configObj;
    }

    @VisibleForTesting
    IConfiguration initConfiguration() {
        return new Configuration("Cluster Command " + mCommand.getCommandId(), "");
    }

    /**
     * Builds a configuration file.
     *
     * @return a {@link File} object for a generated configuration file.
     */
    public File build() throws ConfigurationException, IOException {
        assert mCommand != null;
        assert mTestEnvironment != null;
        assert mTestResources != null;
        assert mWorkDir != null;

        IConfiguration config = initConfiguration();
        config.getCommandOptions().setTestTag(TEST_TAG);
        List<IDeviceConfiguration> deviceConfigs = new ArrayList<>();
        int index = 0;
        assert 0 < mCommand.getTargetDeviceSerials().size();

        // Split config defs into device/non-device ones.
        List<TradefedConfigObject> deviceConfigObjDefs = new ArrayList<>();
        List<TradefedConfigObject> nonDeviceConfigObjDefs = new ArrayList<>();
        for (TradefedConfigObject configObjDef : mTestEnvironment.getTradefedConfigObjects()) {
            if (TradefedConfigObject.Type.TARGET_PREPARER.equals(configObjDef.getType())) {
                deviceConfigObjDefs.add(configObjDef);
            } else {
                nonDeviceConfigObjDefs.add(configObjDef);
            }
        }

        Map<String, String> envVars = new TreeMap<>();
        envVars.put("TF_WORK_DIR", mWorkDir.getAbsolutePath());
        envVars.putAll(mTestEnvironment.getEnvVars());
        envVars.putAll(mTestContext.getEnvVars());
        for (String serial : mCommand.getTargetDeviceSerials()) {
            IDeviceConfiguration device =
                    new DeviceConfigurationHolder(String.format("TF_DEVICE_%d", index++));
            device.getDeviceRequirements().setSerial(serial);
            for (TradefedConfigObject configObjDef : deviceConfigObjDefs) {
                device.addSpecificConfig(createConfigObject(configObjDef, envVars));
            }
            deviceConfigs.add(device);
        }
        deviceConfigs.get(0).addSpecificConfig(new ClusterBuildProvider());
        config.setDeviceConfigList(deviceConfigs);
        config.setTest(new ClusterCommandLauncher());
        config.setLogSaver(new ClusterLogSaver());
        // TODO(b/135636270): return log path to TFC instead of relying on a specific filename
        config.setLogOutput(new SimpleFileLogger());
        config.injectOptionValue(
                "simple-file:path",
                Paths.get(mWorkDir.getAbsolutePath(), "logs", "host_log.txt").toString());
        config.setTestInvocationListeners(Collections.<ITestInvocationListener>emptyList());
        for (TradefedConfigObject configObjDef : nonDeviceConfigObjDefs) {
            String typeName = getConfigObjectTypeName(configObjDef.getType());
            @SuppressWarnings("unchecked")
            List<Object> configObjs = (List<Object>) config.getConfigurationObjectList(typeName);
            configObjs.add(createConfigObject(configObjDef, envVars));
            config.setConfigurationObjectList(typeName, configObjs);
        }

        config.injectOptionValue("cluster:request-id", mCommand.getRequestId());
        config.injectOptionValue("cluster:command-id", mCommand.getCommandId());
        config.injectOptionValue("cluster:attempt-id", mCommand.getAttemptId());
        // FIXME: Make this configurable.
        config.injectOptionValue("enable-root", "false");

        String commandLine = mTestContext.getCommandLine();
        if (commandLine == null || commandLine.isEmpty()) {
            commandLine = mCommand.getCommandLine();
        }
        config.injectOptionValue("cluster:command-line", commandLine);
        config.injectOptionValue("cluster:original-command-line", mCommand.getCommandLine());
        config.injectOptionValue("cluster:root-dir", mWorkDir.getAbsolutePath());

        for (final Map.Entry<String, String> entry : envVars.entrySet()) {
            config.injectOptionValue("cluster:env-var", entry.getKey(), entry.getValue());
        }
        for (final String script : mTestEnvironment.getSetupScripts()) {
            config.injectOptionValue("cluster:setup-script", script);
        }
        if (mTestEnvironment.useSubprocessReporting()) {
            config.injectOptionValue("cluster:use-subprocess-reporting", "true");
        }
        config.injectOptionValue(
                "cluster:output-idle-timeout",
                String.valueOf(mTestEnvironment.getOutputIdleTimeout()));
        for (String option : mTestEnvironment.getJvmOptions()) {
            config.injectOptionValue("cluster:jvm-option", option);
        }
        for (final Map.Entry<String, String> entry :
                mTestEnvironment.getJavaProperties().entrySet()) {
            config.injectOptionValue("cluster:java-property", entry.getKey(), entry.getValue());
        }
        if (mTestEnvironment.getOutputFileUploadUrl() != null) {
            String baseUrl = mTestEnvironment.getOutputFileUploadUrl();
            if (!baseUrl.endsWith("/")) {
                baseUrl += "/";
            }
            final String url =
                    String.format(
                            "%s%s/%s/", baseUrl, mCommand.getCommandId(), mCommand.getAttemptId());
            config.injectOptionValue("cluster:output-file-upload-url", url);
        }
        for (final String pattern : mTestEnvironment.getOutputFilePatterns()) {
            config.injectOptionValue("cluster:output-file-pattern", pattern);
        }
        if (mTestEnvironment.getContextFilePattern() != null) {
            config.injectOptionValue(
                    "cluster:context-file-pattern", mTestEnvironment.getContextFilePattern());
        }
        for (String file : mTestEnvironment.getExtraContextFiles()) {
            config.injectOptionValue("cluster:extra-context-file", file);
        }
        if (mTestEnvironment.getRetryCommandLine() != null) {
            config.injectOptionValue(
                    "cluster:retry-command-line", mTestEnvironment.getRetryCommandLine());
        }
        if (mTestEnvironment.getLogLevel() != null) {
            config.injectOptionValue("log-level", mTestEnvironment.getLogLevel());
        }

        List<TestResource> testResources = new ArrayList<>();
        testResources.addAll(mTestResources);
        testResources.addAll(mTestContext.getTestResources());
        for (final TestResource resource : testResources) {
            config.injectOptionValue(
                    "cluster:test-resource", resource.getName(), resource.getUrl());
        }

        // Inject any extra options into the configuration
        UniqueMultiMap<String, String> extraOptions = mCommand.getExtraOptions();
        for (String key : extraOptions.keySet()) {
            for (String value : extraOptions.get(key)) {
                config.injectOptionValue(key, value);
            }
        }

        File f = new File(mWorkDir, "command.xml");
        PrintWriter writer = new PrintWriter(f);
        config.dumpXml(writer);
        writer.close();
        return f;
    }
}
