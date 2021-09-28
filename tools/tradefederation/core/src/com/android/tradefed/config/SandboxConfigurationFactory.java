/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.tradefed.config;

import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.sandbox.ISandbox;
import com.android.tradefed.sandbox.SandboxConfigDump;
import com.android.tradefed.sandbox.SandboxConfigDump.DumpCmd;
import com.android.tradefed.sandbox.SandboxConfigUtil;
import com.android.tradefed.sandbox.SandboxConfigurationException;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.keystore.IKeyStoreClient;
import com.android.tradefed.util.keystore.IKeyStoreFactory;
import com.android.tradefed.util.keystore.KeyStoreException;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Special Configuration factory to handle creation of configurations for Sandboxing purpose.
 *
 * <p>TODO: Split the configuration dump part to another class
 */
public class SandboxConfigurationFactory extends ConfigurationFactory {

    private static SandboxConfigurationFactory sInstance = null;
    public static final Set<String> OPTION_IGNORED_ELEMENTS = new HashSet<>();

    static {
        OPTION_IGNORED_ELEMENTS.addAll(SandboxConfigDump.VERSIONED_ELEMENTS);
        OPTION_IGNORED_ELEMENTS.add(Configuration.DEVICE_REQUIREMENTS_TYPE_NAME);
        OPTION_IGNORED_ELEMENTS.add(Configuration.DEVICE_OPTIONS_TYPE_NAME);
        OPTION_IGNORED_ELEMENTS.add(Configuration.DEVICE_RECOVERY_TYPE_NAME);
        OPTION_IGNORED_ELEMENTS.add(Configuration.CMD_OPTIONS_TYPE_NAME);
    }

    /** Get the singleton {@link IConfigurationFactory} instance. */
    public static SandboxConfigurationFactory getInstance() {
        if (sInstance == null) {
            sInstance = new SandboxConfigurationFactory();
        }
        return sInstance;
    }

    /** {@inheritDoc} */
    @Override
    protected ConfigurationDef getConfigurationDef(
            String name, boolean isGlobal, Map<String, String> templateMap)
            throws ConfigurationException {
        // TODO: Extend ConfigurationDef to possibly create a different IConfiguration type and
        // handle more elegantly the parent/subprocess incompatibilities.
        ConfigurationDef def = createConfigurationDef(name);
        new ConfigLoader(isGlobal).loadConfiguration(name, def, null, templateMap, null);
        return def;
    }

    /** Internal method to create {@link ConfigurationDef} */
    protected ConfigurationDef createConfigurationDef(String name) {
        return new ConfigurationDef(name);
    }

    /**
     * When running the dump for a command. Create a config with specific expectations.
     *
     * @param arrayArgs the command line for the run.
     * @param command The dump command in progress
     * @return a {@link IConfiguration} valid for the VERSIONED Sandbox.
     * @throws ConfigurationException
     */
    public IConfiguration createConfigurationFromArgs(String[] arrayArgs, DumpCmd command)
            throws ConfigurationException {
        // Create on a new object to avoid state on the factory.
        SandboxConfigurationFactory loader = new RunSandboxConfigurationFactory(command);
        return loader.createConfigurationFromArgs(arrayArgs, null, getKeyStoreClient());
    }

    /**
     * Create a {@link IConfiguration} based on the command line and sandbox provided.
     *
     * @param args the command line for the run.
     * @param keyStoreClient the {@link IKeyStoreClient} where to load the key from.
     * @param sandbox the {@link ISandbox} used for the run.
     * @param runUtil the {@link IRunUtil} to run commands.
     * @return a {@link IConfiguration} valid for the sandbox.
     * @throws ConfigurationException
     */
    public IConfiguration createConfigurationFromArgs(
            String[] args, IKeyStoreClient keyStoreClient, ISandbox sandbox, IRunUtil runUtil)
            throws ConfigurationException {
        IConfiguration config = null;
        File xmlConfig = null;
        File globalConfig = null;
        try {
            runUtil.unsetEnvVariable(GlobalConfiguration.GLOBAL_CONFIG_VARIABLE);
            // Dump the NON_VERSIONED part of the configuration against the current TF and not the
            // sandboxed environment.
            globalConfig = SandboxConfigUtil.dumpFilteredGlobalConfig(new HashSet<>());
            xmlConfig =
                    SandboxConfigUtil.dumpConfigForVersion(
                            createClasspath(),
                            runUtil,
                            args,
                            DumpCmd.NON_VERSIONED_CONFIG,
                            globalConfig);
            // Get the non version part of the configuration in order to do proper allocation
            // of devices and such.
            config =
                    super.createConfigurationFromArgs(
                            new String[] {xmlConfig.getAbsolutePath()}, null, keyStoreClient);
            // Reset the command line to the original one.
            config.setCommandLine(args);
            config.setConfigurationObject(Configuration.SANDBOX_TYPE_NAME, sandbox);
        } catch (SandboxConfigurationException e) {
            // Handle the thin launcher mode: Configuration does not exists in parent version yet.
            config = sandbox.createThinLauncherConfig(args, keyStoreClient, runUtil, globalConfig);
            if (config == null) {
                // Rethrow the original exception.
                CLog.e(e);
                throw e;
            }
        } catch (IOException e) {
            CLog.e(e);
            throw new ConfigurationException("Failed to dump global config.", e);
        } catch (ConfigurationException e) {
            CLog.e(e);
            throw e;
        } finally {
            if (config == null) {
                // In case of error, tear down the sandbox.
                sandbox.tearDown();
            }
            FileUtil.deleteFile(globalConfig);
            FileUtil.deleteFile(xmlConfig);
        }
        return config;
    }

    private IKeyStoreClient getKeyStoreClient() {
        try {
            IKeyStoreFactory f = GlobalConfiguration.getInstance().getKeyStoreFactory();
            if (f != null) {
                try {
                    return f.createKeyStoreClient();
                } catch (KeyStoreException e) {
                    CLog.e("Failed to create key store client");
                    CLog.e(e);
                }
            }
        } catch (IllegalStateException e) {
            CLog.w("Global configuration has not been created, failed to get keystore");
            CLog.e(e);
        }
        return null;
    }

    /** Returns the classpath of the current running Tradefed. */
    private String createClasspath() throws ConfigurationException {
        // Get the classpath property.
        String classpathStr = System.getProperty("java.class.path");
        if (classpathStr == null) {
            throw new ConfigurationException(
                    "Could not find the classpath property: java.class.path");
        }
        return classpathStr;
    }

    private class RunSandboxConfigurationFactory extends SandboxConfigurationFactory {

        private DumpCmd mCommand;

        RunSandboxConfigurationFactory(DumpCmd command) {
            mCommand = command;
        }

        @Override
        protected ConfigurationDef createConfigurationDef(String name) {
            return new ConfigurationDef(name) {
                @Override
                protected void checkRejectedObjects(
                        Map<String, String> rejectedObjects, Throwable cause)
                        throws ClassNotFoundConfigurationException {
                    if (mCommand.equals(DumpCmd.RUN_CONFIG) || mCommand.equals(DumpCmd.TEST_MODE)) {
                        Map<String, String> copyRejected = new HashMap<>();
                        for (Entry<String, String> item : rejectedObjects.entrySet()) {
                            if (SandboxConfigDump.VERSIONED_ELEMENTS.contains(item.getValue())) {
                                copyRejected.put(item.getKey(), item.getValue());
                            }
                        }
                        super.checkRejectedObjects(copyRejected, cause);
                    } else {
                        super.checkRejectedObjects(rejectedObjects, cause);
                    }
                }

                @Override
                protected void injectOptions(IConfiguration config, List<OptionDef> optionList)
                        throws ConfigurationException {
                    List<OptionDef> individualAttempt = new ArrayList<>();
                    if (mCommand.equals(DumpCmd.RUN_CONFIG) || mCommand.equals(DumpCmd.TEST_MODE)) {
                        individualAttempt =
                                optionList
                                        .stream()
                                        .filter(
                                                o ->
                                                        (o.applicableObjectType != null
                                                                && !OPTION_IGNORED_ELEMENTS
                                                                        .contains(
                                                                                o.applicableObjectType)))
                                        .collect(Collectors.toList());
                        optionList.removeAll(individualAttempt);
                    }
                    super.injectOptions(config, optionList);
                    // Try individually each "filtered-option", they will not stop the run if they
                    // cannot be set since they are not part of the versioned process.
                    for (OptionDef item : individualAttempt) {
                        List<OptionDef> tmpList = Arrays.asList(item);
                        try {
                            super.injectOptions(config, tmpList);
                        } catch (ConfigurationException e) {
                            // Ignore
                        }
                    }
                }
            };
        }
    }
}
