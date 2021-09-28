/*
 * Copyright (C) 2010 The Android Open Source Project
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

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.command.ICommandOptions;
import com.android.tradefed.device.IDeviceRecovery;
import com.android.tradefed.device.IDeviceSelection;
import com.android.tradefed.device.TestDeviceOptions;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.postprocessor.IPostProcessor;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.suite.checker.ISystemStatusChecker;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.multi.IMultiTargetPreparer;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.coverage.CoverageOptions;
import com.android.tradefed.util.keystore.IKeyStoreClient;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.util.Collection;
import java.util.List;
import java.util.Set;

/**
 * Configuration information for a TradeFederation invocation.
 *
 * Each TradeFederation invocation has a single {@link IConfiguration}. An {@link IConfiguration}
 * stores all the delegate objects that should be used during the invocation, and their associated
 * {@link Option}'s
 */
public interface IConfiguration {

    /** Returns the name of the configuration. */
    public String getName();

    /**
     * Gets the {@link IBuildProvider} from the configuration.
     *
     * @return the {@link IBuildProvider} provided in the configuration
     */
    public IBuildProvider getBuildProvider();

    /**
     * Gets the {@link ITargetPreparer}s from the configuration.
     *
     * @return the {@link ITargetPreparer}s provided in order in the configuration
     */
    public List<ITargetPreparer> getTargetPreparers();

    /**
     * Gets the {@link IRemoteTest}s to run from the configuration.
     *
     * @return the tests provided in the configuration
     */
    public List<IRemoteTest> getTests();

    /**
     * Gets the {@link ITestInvocationListener}s to use from the configuration.
     *
     * @return the {@link ITestInvocationListener}s provided in the configuration.
     */
    public List<ITestInvocationListener> getTestInvocationListeners();

    /**
     * Gets the {@link IDeviceRecovery} to use from the configuration.
     *
     * @return the {@link IDeviceRecovery} provided in the configuration.
     */
    public IDeviceRecovery getDeviceRecovery();

    /**
     * Gets the {@link TestDeviceOptions} to use from the configuration.
     *
     * @return the {@link TestDeviceOptions} provided in the configuration.
     */
    public TestDeviceOptions getDeviceOptions();

    /**
     * Gets the {@link ILeveledLogOutput} to use from the configuration.
     *
     * @return the {@link ILeveledLogOutput} provided in the configuration.
     */
    public ILeveledLogOutput getLogOutput();

    /**
     * Gets the {@link ILogSaver} to use from the configuration.
     *
     * @return the {@link ILogSaver} provided in the configuration.
     */
    public ILogSaver getLogSaver();

    /** Returns the {@link IRetryDecision} used for the invocation. */
    public IRetryDecision getRetryDecision();

    /**
     * Gets the {@link IMultiTargetPreparer}s from the configuration.
     *
     * @return the {@link IMultiTargetPreparer}s provided in order in the configuration
     */
    public List<IMultiTargetPreparer> getMultiTargetPreparers();

    /**
     * Gets the {@link IMultiTargetPreparer}s from the configuration that should be executed before
     * any of the devices target_preparers.
     *
     * @return the {@link IMultiTargetPreparer}s provided in order in the configuration
     */
    public List<IMultiTargetPreparer> getMultiPreTargetPreparers();

    /**
     * Gets the {@link ISystemStatusChecker}s from the configuration.
     *
     * @return the {@link ISystemStatusChecker}s provided in order in the configuration
     */
    public List<ISystemStatusChecker> getSystemStatusCheckers();

    /** Gets the {@link IMetricCollector}s from the configuration. */
    public List<IMetricCollector> getMetricCollectors();

    /** Gets the {@link IPostProcessor}s from the configuration. */
    public List<IPostProcessor> getPostProcessors();

    /**
     * Gets the {@link ICommandOptions} to use from the configuration.
     *
     * @return the {@link ICommandOptions} provided in the configuration.
     */
    public ICommandOptions getCommandOptions();

    /** Returns the {@link ConfigurationDescriptor} provided in the configuration. */
    public ConfigurationDescriptor getConfigurationDescription();

    /**
     * Gets the {@link IDeviceSelection} to use from the configuration.
     *
     * @return the {@link IDeviceSelection} provided in the configuration.
     */
    public IDeviceSelection getDeviceRequirements();

    /**
     * Gets the {@link CoverageOptions} to use from the configuration.
     *
     * @return the {@link CoverageOptions} provided in the configuration.
     */
    public CoverageOptions getCoverageOptions();

    /**
     * Generic interface to get the configuration object with the given type name.
     *
     * @param typeName the unique type of the configuration object
     *
     * @return the configuration object or <code>null</code> if the object type with given name
     * does not exist.
     */
    public Object getConfigurationObject(String typeName);

    /**
     * Generic interface to get all the object of one given type name across devices.
     *
     * @param typeName the unique type of the configuration object
     * @return The list of configuration objects of the given type.
     */
    public Collection<Object> getAllConfigurationObjectsOfType(String typeName);

    /**
     * Similar to {@link #getConfigurationObject(String)}, but for configuration
     * object types that support multiple objects.
     *
     * @param typeName the unique type name of the configuration object
     *
     * @return the list of configuration objects or <code>null</code> if the object type with
     * given name does not exist.
     */
    public List<?> getConfigurationObjectList(String typeName);

    /**
     * Gets the {@link IDeviceConfiguration}s from the configuration.
     *
     * @return the {@link IDeviceConfiguration}s provided in order in the configuration
     */
    public List<IDeviceConfiguration> getDeviceConfig();

    /**
     * Return the {@link IDeviceConfiguration} associated to the name provided, null if not found.
     */
    public IDeviceConfiguration getDeviceConfigByName(String nameDevice);

    /**
     * Inject a option value into the set of configuration objects.
     * <p/>
     * Useful to provide values for options that are generated dynamically.
     *
     * @param optionName the option name
     * @param optionValue the option value
     * @throws ConfigurationException if failed to set the option's value
     */
    public void injectOptionValue(String optionName, String optionValue)
            throws ConfigurationException;

    /**
     * Inject a option value into the set of configuration objects.
     * <p/>
     * Useful to provide values for options that are generated dynamically.
     *
     * @param optionName the option name
     * @param optionKey the optional key for map options, or null
     * @param optionValue the map option value
     * @throws ConfigurationException if failed to set the option's value
     */
    public void injectOptionValue(String optionName, String optionKey, String optionValue)
            throws ConfigurationException;

    /**
     * Inject a option value into the set of configuration objects.
     * <p/>
     * Useful to provide values for options that are generated dynamically.
     *
     * @param optionName the option name
     * @param optionKey the optional key for map options, or null
     * @param optionValue the map option value
     * @param optionSource the source config that provided this option value
     * @throws ConfigurationException if failed to set the option's value
     */
    public void injectOptionValueWithSource(String optionName, String optionKey, String optionValue,
            String optionSource) throws ConfigurationException;

    /**
     * Inject multiple option values into the set of configuration objects.
     * <p/>
     * Useful to inject many option values at once after creating a new object.
     *
     * @param optionDefs a list of option defs to inject
     * @throws ConfigurationException if failed to set option values
     */
    public void injectOptionValues(List<OptionDef> optionDefs) throws ConfigurationException;

    /**
     * Inject multiple option values into the set of configuration objects without throwing if one
     * of the option cannot be applied.
     *
     * <p>Useful to inject many option values at once after creating a new object.
     *
     * @param optionDefs a list of option defs to inject
     * @throws ConfigurationException if failed to create the {@link OptionSetter}
     */
    public void safeInjectOptionValues(List<OptionDef> optionDefs) throws ConfigurationException;

    /**
     * Create a shallow copy of this object.
     *
     * @return a {link IConfiguration} copy
     */
    public IConfiguration clone();

    /**
     * Create a base clone from {@link #clone()} then deep clone the list of given config object.
     *
     * @param objectToDeepClone The list of configuration object to deep clone.
     * @param client The keystore client.
     * @return The partially deep cloned config.
     * @throws ConfigurationException
     */
    public IConfiguration partialDeepClone(List<String> objectToDeepClone, IKeyStoreClient client)
            throws ConfigurationException;

    /**
     * Replace the current {@link IBuildProvider} in the configuration.
     *
     * @param provider the new {@link IBuildProvider}
     */
    public void setBuildProvider(IBuildProvider provider);

    /**
     * Set the {@link ILeveledLogOutput}, replacing any existing value.
     *
     * @param logger
     */
    public void setLogOutput(ILeveledLogOutput logger);

    /**
     * Set the {@link IRetryDecision}, replacing any existing value.
     *
     * @param decisionRetry
     */
    public void setRetryDecision(IRetryDecision decisionRetry);

    /**
     * Set the {@link ILogSaver}, replacing any existing value.
     *
     * @param logSaver
     */
    public void setLogSaver(ILogSaver logSaver);

    /**
     * Set the {@link IDeviceRecovery}, replacing any existing value.
     *
     * @param recovery
     */
    public void setDeviceRecovery(IDeviceRecovery recovery);

    /**
     * Set the {@link ITargetPreparer}, replacing any existing value.
     *
     * @param preparer
     */
    public void setTargetPreparer(ITargetPreparer preparer);

    /**
     * Set the list of {@link ITargetPreparer}s, replacing any existing value.
     *
     * @param preparers
     */
    public void setTargetPreparers(List<ITargetPreparer> preparers);

    /**
     * Set a {@link IDeviceConfiguration}, replacing any existing value.
     *
     * @param deviceConfig
     */
    public void setDeviceConfig(IDeviceConfiguration deviceConfig);

    /**
     * Set the {@link IDeviceConfiguration}s, replacing any existing value.
     *
     * @param deviceConfigs
     */
    public void setDeviceConfigList(List<IDeviceConfiguration> deviceConfigs);

    /**
     * Convenience method to set a single {@link IRemoteTest} in this configuration, replacing any
     * existing values
     *
     * @param test
     */
    public void setTest(IRemoteTest test);

    /**
     * Set the list of {@link IRemoteTest}s in this configuration, replacing any
     * existing values
     *
     * @param tests
     */
    public void setTests(List<IRemoteTest> tests);

    /**
     * Set the list of {@link IMultiTargetPreparer}s in this configuration, replacing any
     * existing values
     *
     * @param multiTargPreps
     */
    public void setMultiTargetPreparers(List<IMultiTargetPreparer> multiTargPreps);

    /**
     * Convenience method to set a single {@link IMultiTargetPreparer} in this configuration,
     * replacing any existing values
     *
     * @param multiTargPrep
     */
    public void setMultiTargetPreparer(IMultiTargetPreparer multiTargPrep);

    /**
     * Set the list of {@link IMultiTargetPreparer}s in this configuration that should be executed
     * before any of the devices target_preparers, replacing any existing values
     *
     * @param multiPreTargPreps
     */
    public void setMultiPreTargetPreparers(List<IMultiTargetPreparer> multiPreTargPreps);

    /**
     * Convenience method to set a single {@link IMultiTargetPreparer} in this configuration that
     * should be executed before any of the devices target_preparers, replacing any existing values
     *
     * @param multiPreTargPreps
     */
    public void setMultiPreTargetPreparer(IMultiTargetPreparer multiPreTargPreps);

    /**
     * Set the list of {@link ISystemStatusChecker}s in this configuration, replacing any
     * existing values
     *
     * @param systemCheckers
     */
    public void setSystemStatusCheckers(List<ISystemStatusChecker> systemCheckers);

    /**
     * Convenience method to set a single {@link ISystemStatusChecker} in this configuration,
     * replacing any existing values
     *
     * @param systemChecker
     */
    public void setSystemStatusChecker(ISystemStatusChecker systemChecker);

    /**
     * Set the list of {@link ITestInvocationListener}s, replacing any existing values
     *
     * @param listeners
     */
    public void setTestInvocationListeners(List<ITestInvocationListener> listeners);

    /**
     * Convenience method to set a single {@link ITestInvocationListener}
     *
     * @param listener
     */
    public void setTestInvocationListener(ITestInvocationListener listener);

    /** Set the list of {@link IMetricCollector}s, replacing any existing values. */
    public void setDeviceMetricCollectors(List<IMetricCollector> collectors);

    /** Set the list of {@link IPostProcessor}s, replacing any existing values. */
    public void setPostProcessors(List<IPostProcessor> processors);

    /**
     * Set the {@link ICommandOptions}, replacing any existing values
     *
     * @param cmdOptions
     */
    public void setCommandOptions(ICommandOptions cmdOptions);

    /**
     * Set the {@link IDeviceSelection}, replacing any existing values
     *
     * @param deviceSelection
     */
    public void setDeviceRequirements(IDeviceSelection deviceSelection);

    /**
     * Set the {@link TestDeviceOptions}, replacing any existing values
     */
    public void setDeviceOptions(TestDeviceOptions deviceOptions);

    /** Set the {@link CoverageOptions}, replacing any existing values. */
    public void setCoverageOptions(CoverageOptions coverageOptions);

    /**
     * Generic method to set the config object with the given name, replacing any existing value.
     *
     * @param name the unique name of the config object type.
     * @param configObject the config object
     * @throws ConfigurationException if the configObject was not the correct type
     */
    public void setConfigurationObject(String name, Object configObject)
            throws ConfigurationException;

    /**
     * Generic method to set the config object list for the given name, replacing any existing
     * value.
     *
     * @param name the unique name of the config object type.
     * @param configList the config object list
     * @throws ConfigurationException if any objects in the list are not the correct type
     */
    public void setConfigurationObjectList(String name, List<?> configList)
            throws ConfigurationException;

    /** Returns whether or not a configured device is tagged isFake=true or not. */
    public boolean isDeviceConfiguredFake(String deviceName);

    /**
     * Set the config {@link Option} fields with given set of command line arguments
     * <p/>
     * {@link ArgsOptionParser} for expected format
     *
     * @param listArgs the command line arguments
     * @return the unconsumed arguments
     */
    public List<String> setOptionsFromCommandLineArgs(List<String> listArgs)
            throws ConfigurationException;

    /**
     * Set the config {@link Option} fields with given set of command line arguments
     * <p/>
     * See {@link ArgsOptionParser} for expected format
     *
     * @param listArgs the command line arguments
     * @param keyStoreClient {@link IKeyStoreClient} to use.
     * @return the unconsumed arguments
     */
    public List<String> setOptionsFromCommandLineArgs(List<String> listArgs,
            IKeyStoreClient keyStoreClient)
            throws ConfigurationException;

    /**
     * Set the config {@link Option} fields with given set of command line arguments using a best
     * effort approach.
     *
     * <p>See {@link ArgsOptionParser} for expected format
     *
     * @param listArgs the command line arguments
     * @param keyStoreClient {@link IKeyStoreClient} to use.
     * @return the unconsumed arguments
     */
    public List<String> setBestEffortOptionsFromCommandLineArgs(
            List<String> listArgs, IKeyStoreClient keyStoreClient) throws ConfigurationException;

    /**
     * Outputs a command line usage help text for this configuration to given printStream.
     *
     * @param importantOnly if <code>true</code> only print help for the important options
     * @param out the {@link PrintStream} to use.
     * @throws ConfigurationException
     */
    public void printCommandUsage(boolean importantOnly, PrintStream out)
            throws ConfigurationException;

    /**
     * Validate option values.
     * <p/>
     * Currently this will just validate that all mandatory options have been set
     *
     * @throws ConfigurationException if config is not valid
     */
    public void validateOptions() throws ConfigurationException;

    /**
     * Resolve options of {@link File} pointing to a remote location. This requires {@link
     * #cleanConfigurationData()} to be called to clean up the files.
     *
     * @param resolver the {@link DynamicRemoteFileResolver} to resolve the files
     * @throws BuildRetrievalError
     * @throws ConfigurationException
     */
    public void resolveDynamicOptions(DynamicRemoteFileResolver resolver)
            throws ConfigurationException, BuildRetrievalError;

    /** Delete any files that was downloaded to resolved Option fields of remote files. */
    public void cleanConfigurationData();

    /** Add files that must be cleaned during {@link #cleanConfigurationData()} */
    public void addFilesToClean(Set<File> toBeCleaned);

    /** Get the list of files that will be cleaned during {@link #cleanConfigurationData()} */
    public Set<File> getFilesToClean();

    /**
     * Sets the command line used to create this {@link IConfiguration}.
     * This stores the whole command line, including the configuration name,
     * unlike setOptionsFromCommandLineArgs.
     *
     * @param arrayArgs the command line
     */
    public void setCommandLine(String[] arrayArgs);

    /**
     * Gets the command line used to create this {@link IConfiguration}.
     *
     * @return the command line used to create this {@link IConfiguration}.
     */
    public String getCommandLine();

    /**
     * Gets the expanded XML file for the config with all options shown for this
     * {@link IConfiguration} as a {@link String}.
     *
     * @param output the writer to print the xml to.
     * @throws IOException
     */
    public void dumpXml(PrintWriter output) throws IOException;

    /**
     * Gets the expanded XML file for the config with all options shown for this {@link
     * IConfiguration} minus the objects filters by their key name.
     *
     * <p>Filter example: {@link Configuration#TARGET_PREPARER_TYPE_NAME}.
     *
     * @param output the writer to print the xml to.
     * @param excludeFilters the list of object type that should not be dumped.
     * @throws IOException
     */
    public void dumpXml(PrintWriter output, List<String> excludeFilters) throws IOException;

    /**
     * Gets the expanded XML file for the config with all options shown for this {@link
     * IConfiguration} minus the objects filters by their key name.
     *
     * <p>Filter example: {@link Configuration#TARGET_PREPARER_TYPE_NAME}.
     *
     * @param output the writer to print the xml to.
     * @param excludeFilters the list of object type that should not be dumped.
     * @param printDeprecatedOptions Whether or not to print options marked as deprecated
     * @throws IOException
     */
    public void dumpXml(
            PrintWriter output,
            List<String> excludeFilters,
            boolean printDeprecatedOptions,
            boolean printUnchangedOptions)
            throws IOException;
}
