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
package com.android.tradefed.testtype.suite.retry;

import static org.junit.Assert.assertNull;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.ConfigurationFactory;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationFactory;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IDeviceSelection;
import com.android.tradefed.invoker.IRescheduler;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.FileLogger;
import com.android.tradefed.log.ILeveledLogOutput;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.result.TextResultReporter;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.suite.BaseTestSuite;
import com.android.tradefed.testtype.suite.ITestSuite;
import com.android.tradefed.testtype.suite.SuiteTestFilter;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.QuotationAwareTokenizer;

import com.google.inject.Inject;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * A special runner that allows to reschedule a previous run tests that failed or where not
 * executed.
 */
public final class RetryRescheduler implements IRemoteTest, IConfigurationReceiver {

    /** The types of the tests that can be retried. */
    public enum RetryType {
        FAILED,
        NOT_EXECUTED,
    }

    @Option(
            name = "retry-type",
            description =
                    "used to retry tests of a certain status. Possible values include \"failed\" "
                            + "and \"not_executed\".")
    private RetryType mRetryType = null;

    @Option(
        name = BaseTestSuite.MODULE_OPTION,
        shortName = BaseTestSuite.MODULE_OPTION_SHORT_NAME,
        description = "the test module to run. Only works for configuration in the tests dir."
    )
    private String mModuleName = null;

    /**
     * It's possible to add extra exclusion from the rerun. But these tests will not change their
     * state.
     */
    @Option(
        name = BaseTestSuite.EXCLUDE_FILTER_OPTION,
        description = "the exclude module filters to apply.",
        importance = Importance.ALWAYS
    )
    private Set<String> mExcludeFilters = new HashSet<>();

    // Carry some options from suites that are convenient and don't impact the tests selection.
    @Option(
        name = ITestSuite.REBOOT_BEFORE_TEST,
        description = "Reboot the device before the test suite starts."
    )
    private boolean mRebootBeforeTest = false;

    public static final String PREVIOUS_LOADER_NAME = "previous_loader";

    private IConfiguration mConfiguration;
    private IRescheduler mRescheduler;

    private IConfigurationFactory mFactory;

    private IConfiguration mRescheduledConfiguration;

    @Override
    public void run(
            TestInformation testInfo /* do not use - should be null */,
            ITestInvocationListener listener /* do not use - should be null */)
            throws DeviceNotAvailableException {
        assertNull(testInfo);
        assertNull(listener);

        // Get the re-loader for previous results
        Object loader = mConfiguration.getConfigurationObject(PREVIOUS_LOADER_NAME);
        if (loader == null) {
            throw new RuntimeException(
                    String.format(
                            "An <object> of type %s was expected in the retry.",
                            PREVIOUS_LOADER_NAME));
        }
        if (!(loader instanceof ITestSuiteResultLoader)) {
            throw new RuntimeException(
                    String.format(
                            "%s should be implementing %s",
                            loader.getClass().getCanonicalName(),
                            ITestSuiteResultLoader.class.getCanonicalName()));
        }

        ITestSuiteResultLoader previousLoader = (ITestSuiteResultLoader) loader;
        // First init the reloader.
        previousLoader.init();
        // Then get the command line of the previous run
        String commandLine = previousLoader.getCommandLine();
        IConfiguration originalConfig;
        try {
            originalConfig =
                    getFactory()
                            .createConfigurationFromArgs(
                                    QuotationAwareTokenizer.tokenizeLine(commandLine));
            // Transfer the sharding options from the original command.
            originalConfig
                    .getCommandOptions()
                    .setShardCount(mConfiguration.getCommandOptions().getShardCount());
            originalConfig
                    .getCommandOptions()
                    .setShardIndex(mConfiguration.getCommandOptions().getShardIndex());
            IDeviceSelection requirements = mConfiguration.getDeviceRequirements();
            // It should be safe to use the current requirements against the old config because
            // There will be more checks like fingerprint if it was supposed to run.
            originalConfig.setDeviceRequirements(requirements);

            // Transfer log level from retry to subconfig
            ILeveledLogOutput originalLogger = originalConfig.getLogOutput();
            originalLogger.setLogLevel(mConfiguration.getLogOutput().getLogLevel());
            if (originalLogger instanceof FileLogger) {
                ((FileLogger) originalLogger)
                        .setLogLevelDisplay(mConfiguration.getLogOutput().getLogLevel());
            }

            handleExtraResultReporter(originalConfig, mConfiguration);
        } catch (ConfigurationException e) {
            throw new RuntimeException(e);
        }
        // Get previous results
        CollectingTestListener collectedTests = previousLoader.loadPreviousResults();
        previousLoader.cleanUp();

        // Appropriately update the configuration
        IRemoteTest test = originalConfig.getTests().get(0);
        if (!(test instanceof BaseTestSuite)) {
            throw new RuntimeException(
                    "RetryScheduler only works for BaseTestSuite implementations");
        }
        BaseTestSuite suite = (BaseTestSuite) test;
        ResultsPlayer replayer = new ResultsPlayer();
        updateRunner(suite, collectedTests, replayer);
        collectedTests = null;
        updateConfiguration(originalConfig, replayer);
        // Do the customization of the configuration for specialized use cases.
        customizeConfig(previousLoader, originalConfig);

        if (mRebootBeforeTest) {
            suite.enableRebootBeforeTest();
        }

        mRescheduledConfiguration = originalConfig;

        if (mRescheduler != null) {
            // At the end, reschedule if requested
            boolean res = mRescheduler.scheduleConfig(originalConfig);
            if (!res) {
                CLog.e("Something went wrong, failed to kick off the retry run.");
            }
        }
    }

    @Inject
    public void setRescheduler(IRescheduler rescheduler) {
        mRescheduler = rescheduler;
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    private IConfigurationFactory getFactory() {
        if (mFactory != null) {
            return mFactory;
        }
        return ConfigurationFactory.getInstance();
    }

    @VisibleForTesting
    void setConfigurationFactory(IConfigurationFactory factory) {
        mFactory = factory;
    }

    /** Returns the {@link IConfiguration} that should be retried. */
    public final IConfiguration getRetryConfiguration() {
        return mRescheduledConfiguration;
    }

    /**
     * Update the configuration to be ready for re-run.
     *
     * @param suite The {@link BaseTestSuite} that will be re-run.
     * @param results The results of the previous run.
     * @param replayer The {@link ResultsPlayer} that will replay the non-retried use cases.
     */
    private void updateRunner(
            BaseTestSuite suite, CollectingTestListener results, ResultsPlayer replayer) {
        List<RetryType> types = new ArrayList<>();
        if (mRetryType == null) {
            types.add(RetryType.FAILED);
            types.add(RetryType.NOT_EXECUTED);
        } else {
            types.add(mRetryType);
        }

        // Expand the --module option in case no abi is specified.
        Set<String> expandedModuleOption = new HashSet<>();
        if (mModuleName != null) {
            SuiteTestFilter moduleFilter = SuiteTestFilter.createFrom(mModuleName);
            expandedModuleOption.add(mModuleName);
            if (moduleFilter.getAbi() == null) {
                Set<String> abis = AbiUtils.getAbisSupportedByCompatibility();
                for (String abi : abis) {
                    SuiteTestFilter namingFilter =
                            new SuiteTestFilter(
                                    abi, moduleFilter.getName(), moduleFilter.getTest());
                    expandedModuleOption.add(namingFilter.toString());
                }
            }
        }

        // Expand the exclude-filter in case no abi is specified.
        Set<String> extendedExcludeRetryFilters = new HashSet<>();
        for (String excludeFilter : mExcludeFilters) {
            SuiteTestFilter suiteFilter = SuiteTestFilter.createFrom(excludeFilter);
            // Keep the current exclude-filter
            extendedExcludeRetryFilters.add(excludeFilter);
            if (suiteFilter.getAbi() == null) {
                // If no abi is specified, exclude them all.
                Set<String> abis = AbiUtils.getAbisSupportedByCompatibility();
                for (String abi : abis) {
                    SuiteTestFilter namingFilter =
                            new SuiteTestFilter(abi, suiteFilter.getName(), suiteFilter.getTest());
                    extendedExcludeRetryFilters.add(namingFilter.toString());
                }
            }
        }

        // Prepare exclusion filters
        for (TestRunResult moduleResult : results.getMergedTestRunResults()) {
            // If the module is explicitly excluded from retries, preserve the original results.
            if (!extendedExcludeRetryFilters.contains(moduleResult.getName())
                    && (expandedModuleOption.isEmpty()
                            || expandedModuleOption.contains(moduleResult.getName()))
                    && RetryResultHelper.shouldRunModule(moduleResult, types)) {
                if (types.contains(RetryType.NOT_EXECUTED)) {
                    // Clear the run failure since we are attempting to rerun all non-executed
                    moduleResult.resetRunFailure();
                }

                Map<TestDescription, TestResult> parameterizedMethods = new LinkedHashMap<>();

                for (Entry<TestDescription, TestResult> result :
                        moduleResult.getTestResults().entrySet()) {
                    // Put aside all parameterized methods
                    if (isParameterized(result.getKey())) {
                        parameterizedMethods.put(result.getKey(), result.getValue());
                        continue;
                    }
                    if (!RetryResultHelper.shouldRunTest(result.getValue(), types)) {
                        addExcludeToConfig(suite, moduleResult, result.getKey().toString());
                        replayer.addToReplay(
                                results.getModuleContextForRunResult(moduleResult.getName()),
                                moduleResult,
                                result);
                    }
                }

                // Handle parameterized methods
                for (Entry<String, Map<TestDescription, TestResult>> subMap :
                        sortMethodToClass(parameterizedMethods).entrySet()) {
                    boolean shouldNotrerunAnything =
                            subMap.getValue()
                                    .entrySet()
                                    .stream()
                                    .noneMatch(
                                            (v) ->
                                                    RetryResultHelper.shouldRunTest(
                                                                    v.getValue(), types)
                                                            == true);
                    // If None of the base method need to be rerun exclude it
                    if (shouldNotrerunAnything) {
                        // Exclude the base method
                        addExcludeToConfig(suite, moduleResult, subMap.getKey());
                        // Replay all test cases
                        for (Entry<TestDescription, TestResult> result :
                                subMap.getValue().entrySet()) {
                            replayer.addToReplay(
                                    results.getModuleContextForRunResult(moduleResult.getName()),
                                    moduleResult,
                                    result);
                        }
                    }
                }
            } else {
                // Exclude the module completely - it will keep its current status
                addExcludeToConfig(suite, moduleResult, null);
                replayer.addToReplay(
                        results.getModuleContextForRunResult(moduleResult.getName()),
                        moduleResult,
                        null);
            }
        }
    }

    /** Update the configuration to put the replayer before all the actual real tests. */
    private void updateConfiguration(IConfiguration config, ResultsPlayer replayer) {
        List<IRemoteTest> tests = config.getTests();
        List<IRemoteTest> newList = new ArrayList<>();
        // Add the replayer first to replay all the tests cases first.
        newList.add(replayer);
        newList.addAll(tests);
        config.setTests(newList);
    }

    /** Allow the specialized loader to customize the config before re-running it. */
    private void customizeConfig(ITestSuiteResultLoader loader, IConfiguration originalConfig) {
        loader.customizeConfiguration(originalConfig);
    }

    /** Add the filter to the suite. */
    private void addExcludeToConfig(
            BaseTestSuite suite, TestRunResult moduleResult, String testDescription) {
        String filter = moduleResult.getName();
        if (testDescription != null) {
            filter = String.format("%s %s", filter, testDescription);
        }
        SuiteTestFilter testFilter = SuiteTestFilter.createFrom(filter);
        Set<String> excludeFilter = new LinkedHashSet<>();
        excludeFilter.add(testFilter.toString());
        suite.setExcludeFilter(excludeFilter);
    }

    /** Returns True if a test case is a parameterized one. */
    private boolean isParameterized(TestDescription description) {
        return !description.getTestName().equals(description.getTestNameWithoutParams());
    }

    private Map<String, Map<TestDescription, TestResult>> sortMethodToClass(
            Map<TestDescription, TestResult> paramMethods) {
        Map<String, Map<TestDescription, TestResult>> returnMap = new LinkedHashMap<>();
        for (Entry<TestDescription, TestResult> entry : paramMethods.entrySet()) {
            String noParamName =
                    String.format(
                            "%s#%s",
                            entry.getKey().getClassName(),
                            entry.getKey().getTestNameWithoutParams());
            Map<TestDescription, TestResult> forClass = returnMap.get(noParamName);
            if (forClass == null) {
                forClass = new LinkedHashMap<>();
                returnMap.put(noParamName, forClass);
            }
            forClass.put(entry.getKey(), entry.getValue());
        }
        return returnMap;
    }

    /**
     * Fetch additional result_reporter from the retry configuration and add them to the original
     * command. This is the only allowed modification of the original command: add more result
     * end-points.
     */
    private void handleExtraResultReporter(
            IConfiguration originalConfig, IConfiguration retryConfig) {
        // Since we always have 1 default reporter, avoid carrying it for no reason. Only carry
        // reporters if some actual ones were specified.
        if (retryConfig.getTestInvocationListeners().size() == 1
                && (mConfiguration.getTestInvocationListeners().get(0)
                        instanceof TextResultReporter)) {
            return;
        }
        List<ITestInvocationListener> listeners = originalConfig.getTestInvocationListeners();
        listeners.addAll(retryConfig.getTestInvocationListeners());
        originalConfig.setTestInvocationListeners(listeners);
    }
}
