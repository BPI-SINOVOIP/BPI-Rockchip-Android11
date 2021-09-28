/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.tradefed.testtype.suite;

import com.android.annotations.VisibleForTesting;
import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.Option.Importance;
import com.android.tradefed.config.OptionCopier;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceProperties;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.NullDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.device.cloud.NestedRemoteDevice;
import com.android.tradefed.device.metric.CollectorHelper;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.device.metric.IMetricCollectorReceiver;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.logger.InvocationMetricLogger;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.invoker.shard.token.ITokenRequest;
import com.android.tradefed.invoker.shard.token.TokenProperty;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.ITestLoggerReceiver;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.retry.RetryStrategy;
import com.android.tradefed.suite.checker.ISystemStatusChecker;
import com.android.tradefed.suite.checker.ISystemStatusCheckerReceiver;
import com.android.tradefed.suite.checker.StatusCheckerResult;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.testtype.Abi;
import com.android.tradefed.testtype.IAbi;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IReportNotExecuted;
import com.android.tradefed.testtype.IRuntimeHintProvider;
import com.android.tradefed.testtype.IShardableTest;
import com.android.tradefed.testtype.ITestCollector;
import com.android.tradefed.util.AbiFormatter;
import com.android.tradefed.util.AbiUtils;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.TimeUtil;

import com.google.inject.Inject;
import com.google.inject.Injector;

import java.io.File;
import java.io.FileNotFoundException;
import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Random;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Abstract class used to run Test Suite. This class provide the base of how the Suite will be run.
 * Each implementation can define the list of tests via the {@link #loadTests()} method.
 */
public abstract class ITestSuite
        implements IRemoteTest,
                IDeviceTest,
                IBuildReceiver,
                ISystemStatusCheckerReceiver,
                IShardableTest,
                ITestCollector,
                IInvocationContextReceiver,
                IRuntimeHintProvider,
                IMetricCollectorReceiver,
                IConfigurationReceiver,
                IReportNotExecuted,
                ITokenRequest,
                ITestLoggerReceiver {

    public static final String SKIP_SYSTEM_STATUS_CHECKER = "skip-system-status-check";
    public static final String RUNNER_WHITELIST = "runner-whitelist";
    public static final String PREPARER_WHITELIST = "preparer-whitelist";
    public static final String MODULE_CHECKER_PRE = "PreModuleChecker";
    public static final String MODULE_CHECKER_POST = "PostModuleChecker";
    public static final String ABI_OPTION = "abi";
    public static final String SKIP_HOST_ARCH_CHECK = "skip-host-arch-check";
    public static final String PRIMARY_ABI_RUN = "primary-abi-only";
    public static final String PARAMETER_KEY = "parameter";
    public static final String MAINLINE_PARAMETER_KEY = "mainline-param";
    public static final String ACTIVE_MAINLINE_PARAMETER_KEY = "active-mainline-parameter";
    public static final String TOKEN_KEY = "token";
    public static final String MODULE_METADATA_INCLUDE_FILTER = "module-metadata-include-filter";
    public static final String MODULE_METADATA_EXCLUDE_FILTER = "module-metadata-exclude-filter";
    public static final String RANDOM_SEED = "random-seed";
    public static final String REBOOT_BEFORE_TEST = "reboot-before-test";

    private static final String PRODUCT_CPU_ABI_KEY = "ro.product.cpu.abi";

    // Options for test failure case
    @Option(
        name = "bugreport-on-failure",
        description =
                "Take a bugreport on every test failure. Warning: This may require a lot"
                        + "of storage space of the machine running the tests."
    )
    private boolean mBugReportOnFailure = false;

    @Deprecated
    @Option(
        name = "logcat-on-failure",
        description = "Take a logcat snapshot on every test failure."
    )
    private boolean mLogcatOnFailure = false;

    @Deprecated
    @Option(
        name = "logcat-on-failure-size",
        description =
                "The max number of logcat data in bytes to capture when "
                        + "--logcat-on-failure is on. Should be an amount that can comfortably fit in memory."
    )
    private int mMaxLogcatBytes = 500 * 1024; // 500K

    @Deprecated
    @Option(
        name = "screenshot-on-failure",
        description = "Take a screenshot on every test failure."
    )
    private boolean mScreenshotOnFailure = false;

    @Option(name = "reboot-on-failure",
            description = "Reboot the device after every test failure.")
    private boolean mRebootOnFailure = false;

    // Options for suite runner behavior
    @Option(name = "reboot-per-module", description = "Reboot the device before every module run.")
    private boolean mRebootPerModule = false;

    @Option(
        name = REBOOT_BEFORE_TEST,
        description = "Reboot the device before the test suite starts."
    )
    private boolean mRebootBeforeTest = false;

    @Option(name = "skip-all-system-status-check",
            description = "Whether all system status check between modules should be skipped")
    private boolean mSkipAllSystemStatusCheck = false;

    @Option(
        name = SKIP_SYSTEM_STATUS_CHECKER,
        description =
                "Disable specific system status checkers."
                        + "Specify zero or more SystemStatusChecker as canonical class names. e.g. "
                        + "\"com.android.tradefed.suite.checker.KeyguardStatusChecker\" If not "
                        + "specified, all configured or whitelisted system status checkers will "
                        + "run."
    )
    private Set<String> mSystemStatusCheckBlacklist = new HashSet<>();

    @Option(
        name = "report-system-checkers",
        description = "Whether reporting system checkers as test or not."
    )
    private boolean mReportSystemChecker = false;

    @Option(
        name = "random-order",
        description = "Whether randomizing the order of the modules to be ran or not."
    )
    private boolean mRandomOrder = false;

    @Option(
        name = RANDOM_SEED,
        description = "Seed to randomize the order of the modules."
    )
    private long mRandomSeed = -1;

    @Option(
        name = "collect-tests-only",
        description =
                "Only invoke the suite to collect list of applicable test cases. All "
                        + "test run callbacks will be triggered, but test execution will not be "
                        + "actually carried out."
    )
    private boolean mCollectTestsOnly = false;

    // Abi related options
    @Option(
        name = ABI_OPTION,
        shortName = 'a',
        description = "the abi to test. For example: 'arm64-v8a'.",
        importance = Importance.IF_UNSET
    )
    private String mAbiName = null;

    @Option(
        name = SKIP_HOST_ARCH_CHECK,
        description = "Whether host architecture check should be skipped."
    )
    private boolean mSkipHostArchCheck = false;

    @Option(
        name = PRIMARY_ABI_RUN,
        description =
                "Whether to run tests with only the device primary abi. "
                        + "This is overriden by the --abi option."
    )
    private boolean mPrimaryAbiRun = false;

    @Option(
        name = MODULE_METADATA_INCLUDE_FILTER,
        description =
                "Include modules for execution based on matching of metadata fields: for any of "
                        + "the specified filter name and value, if a module has a metadata field "
                        + "with the same name and value, it will be included. When both module "
                        + "inclusion and exclusion rules are applied, inclusion rules will be "
                        + "evaluated first. Using this together with test filter inclusion rules "
                        + "may result in no tests to execute if the rules don't overlap."
    )
    private MultiMap<String, String> mModuleMetadataIncludeFilter = new MultiMap<>();

    @Option(
        name = MODULE_METADATA_EXCLUDE_FILTER,
        description =
                "Exclude modules for execution based on matching of metadata fields: for any of "
                        + "the specified filter name and value, if a module has a metadata field "
                        + "with the same name and value, it will be excluded. When both module "
                        + "inclusion and exclusion rules are applied, inclusion rules will be "
                        + "evaluated first."
    )
    private MultiMap<String, String> mModuleMetadataExcludeFilter = new MultiMap<>();

    @Option(name = RUNNER_WHITELIST, description = "Runner class(es) that are allowed to run.")
    private Set<String> mAllowedRunners = new HashSet<>();

    @Option(
        name = PREPARER_WHITELIST,
        description =
                "Preparer class(es) that are allowed to run. This mostly usefeul for dry-runs."
    )
    private Set<String> mAllowedPreparers = new HashSet<>();

    @Option(
        name = "enable-module-dynamic-download",
        description =
                "Whether or not to allow the downloading of dynamic @option files at module level."
    )
    private boolean mEnableDynamicDownload = false;

    @Option(
        name = "intra-module-sharding",
        description = "Whether or not to allow intra-module sharding."
    )
    private boolean mIntraModuleSharding = true;

    @Option(
        name = "isolated-module",
        description = "Whether or not to attempt the module isolation between modules"
    )
    private boolean mIsolatedModule = false;

    /** @deprecated to be deleted when next version is deployed */
    @Deprecated
    @Option(
        name = "max-testcase-run-count",
        description =
                "If the IRemoteTest can have its testcases run multiple times, "
                        + "the max number of runs for each testcase."
    )
    private int mMaxRunLimit = 1;

    /** @deprecated to be deleted when next version is deployed */
    @Deprecated
    @Option(
        name = "retry-strategy",
        description =
                "The retry strategy to be used when re-running some tests with "
                        + "--max-testcase-run-count"
    )
    private RetryStrategy mRetryStrategy = RetryStrategy.NO_RETRY;

    // [Options relate to module retry and intra-module retry][
    @Option(
        name = "merge-attempts",
        description = "Whether or not to use the merge the results of the different attempts."
    )
    private boolean mMergeAttempts = true;
    // end [Options relate to module retry and intra-module retry]

    private ITestDevice mDevice;
    private IBuildInfo mBuildInfo;
    private List<ISystemStatusChecker> mSystemStatusCheckers;
    private IInvocationContext mContext;
    private List<IMetricCollector> mMetricCollectors;
    private IConfiguration mMainConfiguration;

    // Sharding attributes
    private boolean mIsSharded = false;
    private ModuleDefinition mDirectModule = null;
    private boolean mShouldMakeDynamicModule = true;

    // Guice object
    private Injector mInjector;

    // Current modules to run, null if not started to run yet.
    private List<ModuleDefinition> mRunModules = null;
    private ModuleDefinition mModuleInProgress = null;
    // Logger to be used to files.
    private ITestLogger mCurrentLogger = null;
    // Whether or not we are currently in split
    private boolean mIsSplitting = false;

    private boolean mDisableAutoRetryTimeReporting = false;

    private DynamicRemoteFileResolver mDynamicResolver = new DynamicRemoteFileResolver();

    @VisibleForTesting
    void setDynamicResolver(DynamicRemoteFileResolver resolver) {
        mDynamicResolver = resolver;
    }

    /**
     * Get the current Guice {@link Injector} from the invocation. It should allow us to continue
     * the object injection of modules.
     */
    @Inject
    public void setInvocationInjector(Injector injector) {
        mInjector = injector;
    }

    /** Forward our invocation scope guice objects to whoever needs them in modules. */
    private void applyGuiceInjection(LinkedHashMap<String, IConfiguration> runConfig) {
        if (mInjector == null) {
            // TODO: Convert to a strong failure
            CLog.d("No injector received by the suite.");
            return;
        }
        for (IConfiguration config : runConfig.values()) {
            for (IRemoteTest test : config.getTests()) {
                mInjector.injectMembers(test);
            }
        }
    }

    /**
     * Abstract method to load the tests configuration that will be run. Each tests is defined by a
     * {@link IConfiguration} and a unique name under which it will report results.
     */
    public abstract LinkedHashMap<String, IConfiguration> loadTests();

    /**
     * Return an instance of the class implementing {@link ITestSuite}.
     */
    private ITestSuite createInstance() {
        try {
            return this.getClass().getDeclaredConstructor().newInstance();
        } catch (InstantiationException
                | IllegalAccessException
                | InvocationTargetException
                | NoSuchMethodException e) {
            throw new RuntimeException(e);
        }
    }

    public File getTestsDir() throws FileNotFoundException {
        IBuildInfo build = getBuildInfo();
        if (build instanceof IDeviceBuildInfo) {
            return ((IDeviceBuildInfo) build).getTestsDir();
        }
        // TODO: handle multi build?
        throw new FileNotFoundException("Could not found a tests dir folder.");
    }

    private LinkedHashMap<String, IConfiguration> loadAndFilter() {
        LinkedHashMap<String, IConfiguration> runConfig = loadTests();
        if (runConfig.isEmpty()) {
            CLog.i("No config were loaded. Nothing to run.");
            return runConfig;
        }
        // Apply our guice scope to all modules objects
        applyGuiceInjection(runConfig);

        Set<String> moduleNames = new HashSet<>();
        LinkedHashMap<String, IConfiguration> filteredConfig = new LinkedHashMap<>();
        for (Entry<String, IConfiguration> config : runConfig.entrySet()) {
            if (!mModuleMetadataIncludeFilter.isEmpty()
                    || !mModuleMetadataExcludeFilter.isEmpty()) {
                if (!filterByConfigMetadata(
                        config.getValue(),
                        mModuleMetadataIncludeFilter,
                        mModuleMetadataExcludeFilter)) {
                    // if the module config did not pass the metadata filters, it's excluded
                    // from execution.
                    continue;
                }
            }
            if (!filterByRunnerType(config.getValue(), mAllowedRunners)) {
                // if the module config did not pass the runner type filter, it's excluded from
                // execution.
                continue;
            }
            filterPreparers(config.getValue(), mAllowedPreparers);

            // Copy the CoverageOptions from the main configuration to the module configuration.
            if (mMainConfiguration != null) {
                config.getValue().setCoverageOptions(mMainConfiguration.getCoverageOptions());
            }

            filteredConfig.put(config.getKey(), config.getValue());
            moduleNames.add(config.getValue().getConfigurationDescription().getModuleName());
        }

        if (mBuildInfo != null
                && mBuildInfo.getRemoteFiles() != null
                && mBuildInfo.getRemoteFiles().size() > 0) {
            stageTestArtifacts(mDevice, moduleNames);
        }

        runConfig.clear();
        return filteredConfig;
    }

    /** Helper to download all artifacts for the given modules. */
    private void stageTestArtifacts(ITestDevice device, Set<String> modules) {
        CLog.i(String.format("Start to stage test artifacts for %d modules.", modules.size()));
        long startTime = System.currentTimeMillis();
        // Include the file if its path contains a folder name matching any of the module.
        String moduleRegex =
                modules.stream()
                        .map(m -> String.format("/%s/", m))
                        .collect(Collectors.joining("|"));
        List<String> includeFilters = Arrays.asList(moduleRegex);
        // Ignore config file as it's part of config zip artifact that's staged already.
        List<String> excludeFilters = Arrays.asList("[.]config$");
        mDynamicResolver.setDevice(device);
        mDynamicResolver.addExtraArgs(
                mMainConfiguration.getCommandOptions().getDynamicDownloadArgs());
        for (File remoteFile : mBuildInfo.getRemoteFiles()) {
            try {
                mDynamicResolver.resolvePartialDownloadZip(
                        getTestsDir(), remoteFile.toString(), includeFilters, excludeFilters);
            } catch (BuildRetrievalError | FileNotFoundException e) {
                CLog.e(
                        String.format(
                                "Failed to download partial zip from %s for modules: %s",
                                remoteFile, String.join(", ", modules)));
                CLog.e(e);
                throw new RuntimeException(e);
            }
        }
        long elapsedTime = System.currentTimeMillis() - startTime;
        InvocationMetricLogger.addInvocationMetrics(
                InvocationMetricKey.STAGE_TESTS_TIME, elapsedTime);
        CLog.i(
                String.format(
                        "Staging test artifacts for %d modules finished in %s.",
                        modules.size(), TimeUtil.formatElapsedTime(elapsedTime)));
    }

    /** Helper that creates and returns the list of {@link ModuleDefinition} to be executed. */
    private List<ModuleDefinition> createExecutionList() {
        List<ModuleDefinition> runModules = new ArrayList<>();
        if (mDirectModule != null) {
            // If we are sharded and already know what to run then we just do it.
            runModules.add(mDirectModule);
            mDirectModule.setDevice(mDevice);
            mDirectModule.setBuild(mBuildInfo);
            return runModules;
        }

        LinkedHashMap<String, IConfiguration> runConfig = loadAndFilter();
        if (runConfig.isEmpty()) {
            CLog.i("No config were loaded. Nothing to run.");
            return runModules;
        }

        for (Entry<String, IConfiguration> config : runConfig.entrySet()) {
            // Validate the configuration, it will throw if not valid.
            ValidateSuiteConfigHelper.validateConfig(config.getValue());
            Map<String, List<ITargetPreparer>> preparersPerDevice =
                    getPreparerPerDevice(config.getValue());
            ModuleDefinition module =
                    new ModuleDefinition(
                            config.getKey(),
                            config.getValue().getTests(),
                            preparersPerDevice,
                            config.getValue().getMultiTargetPreparers(),
                            config.getValue());
            if (mDisableAutoRetryTimeReporting) {
                module.disableAutoRetryReportingTime();
            }
            module.setDevice(mDevice);
            module.setBuild(mBuildInfo);
            runModules.add(module);
        }

        /** Randomize all the modules to be ran if random-order is set and no sharding.*/
        if (mRandomOrder) {
            randomizeTestModules(runModules, mRandomSeed);
        }

        CLog.logAndDisplay(LogLevel.DEBUG, "[Total Unique Modules = %s]", runModules.size());
        // Free the map once we are done with it.
        runConfig = null;
        return runModules;
    }

    /**
     * Helper method that handle randomizing the order of the modules.
     *
     * @param runModules The {@code List<ModuleDefinition>} of the test modules to be ran.
     * @param randomSeed The {@code long} seed used to randomize the order of test modules, use the
     *     current time as seed if no specified seed provided.
     */
    @VisibleForTesting
    void randomizeTestModules(List<ModuleDefinition> runModules, long randomSeed) {
        // Use current time as seed if no specified seed provided.
        if (randomSeed == -1) {
            randomSeed = System.currentTimeMillis();
        }
        CLog.i("Randomizing all the modules with seed: %s", randomSeed);
        Collections.shuffle(runModules, new Random(randomSeed));
        mBuildInfo.addBuildAttribute(RANDOM_SEED, String.valueOf(randomSeed));
    }

    private void checkClassLoad(Set<String> classes, String type) {
        for (String c : classes) {
            try {
                Class.forName(c);
            } catch (ClassNotFoundException e) {
                ConfigurationException ex =
                        new ConfigurationException(
                                String.format(
                                        "--%s must contains valid class, %s was not found",
                                        type, c),
                                e);
                throw new RuntimeException(ex);
            }
        }
    }

    /** Create the mapping of device to its target_preparer. */
    private Map<String, List<ITargetPreparer>> getPreparerPerDevice(IConfiguration config) {
        Map<String, List<ITargetPreparer>> res = new LinkedHashMap<>();
        for (IDeviceConfiguration holder : config.getDeviceConfig()) {
            List<ITargetPreparer> preparers = new ArrayList<>();
            res.put(holder.getDeviceName(), preparers);
            preparers.addAll(holder.getTargetPreparers());
        }
        return res;
    }

    /**
     * Opportunity to clean up all the things that were needed during the suites setup but are not
     * required to run the tests.
     */
    void cleanUpSuiteSetup() {
        // Empty by default.
    }

    /** Generic run method for all test loaded from {@link #loadTests()}. */
    @Override
    public final void run(TestInformation testInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        mCurrentLogger = listener;
        // Load and check the module checkers, runners and preparers in black and whitelist
        checkClassLoad(mSystemStatusCheckBlacklist, SKIP_SYSTEM_STATUS_CHECKER);
        checkClassLoad(mAllowedRunners, RUNNER_WHITELIST);
        checkClassLoad(mAllowedPreparers, PREPARER_WHITELIST);

        mRunModules = createExecutionList();
        // Check if we have something to run.
        if (mRunModules.isEmpty()) {
            CLog.i("No tests to be run.");
            return;
        }

        // Allow checkers to log files for easier debugging.
        for (ISystemStatusChecker checker : mSystemStatusCheckers) {
            if (checker instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) checker).setTestLogger(listener);
            }
        }

        // If requested reboot each device before the testing starts.
        if (mRebootBeforeTest) {
            for (ITestDevice device : mContext.getDevices()) {
                if (!(device.getIDevice() instanceof StubDevice)) {
                    CLog.d(
                            "Rebooting device '%s' before test starts as requested.",
                            device.getSerialNumber());
                    mDevice.reboot();
                }
            }
        }

        /** Setup a special listener to take actions on test failures. */
        TestFailureListener failureListener =
                new TestFailureListener(
                        mContext.getDevices(), mBugReportOnFailure, mRebootOnFailure);
        /** Create the list of listeners applicable at the module level. */
        List<ITestInvocationListener> moduleListeners = createModuleListeners();

        // Only print the running log if we are going to run something.
        if (mRunModules.get(0).hasTests()) {
            CLog.logAndDisplay(
                    LogLevel.INFO,
                    "%s running %s modules: %s",
                    mDevice.getSerialNumber(),
                    mRunModules.size(),
                    mRunModules);
        }

        /** Run all the module, make sure to reduce the list to release resources as we go. */
        try {
            while (!mRunModules.isEmpty()) {
                ModuleDefinition module = mRunModules.remove(0);
                // Before running the module we ensure it has tests at this point or skip completely
                // to avoid running SystemCheckers and preparation for nothing.
                if (module.hasTests()) {
                    continue;
                }

                // Populate the module context with devices and builds
                for (String deviceName : mContext.getDeviceConfigNames()) {
                    module.getModuleInvocationContext()
                            .addAllocatedDevice(deviceName, mContext.getDevice(deviceName));
                    module.getModuleInvocationContext()
                            .addDeviceBuildInfo(deviceName, mContext.getBuildInfo(deviceName));
                }
                listener.testModuleStarted(module.getModuleInvocationContext());
                mModuleInProgress = module;
                // Trigger module start on module level listener too
                new ResultForwarder(moduleListeners)
                        .testModuleStarted(module.getModuleInvocationContext());
                TestInformation moduleInfo =
                        TestInformation.createModuleTestInfo(
                                testInfo, module.getModuleInvocationContext());
                try {
                    runSingleModule(module, moduleInfo, listener, moduleListeners, failureListener);
                } finally {
                    // Trigger module end on module level listener too
                    new ResultForwarder(moduleListeners).testModuleEnded();
                    // clear out module invocation context since we are now done with module
                    // execution
                    listener.testModuleEnded();
                    mModuleInProgress = null;
                }
                // Module isolation routine
                moduleIsolation(mContext, listener);
            }
        } catch (DeviceNotAvailableException e) {
            CLog.e(
                    "A DeviceNotAvailableException occurred, following modules did not run: %s",
                    mRunModules);
            reportNotExecuted(listener, "Module did not run due to device not available.");
            throw e;
        }
    }

    /**
     * Returns the list of {@link ITestInvocationListener} applicable to the {@link ModuleListener}
     * level. These listeners will be re-used for each module, they will not be re-instantiated so
     * they should not assume an internal state.
     */
    protected List<ITestInvocationListener> createModuleListeners() {
        return new ArrayList<>();
    }

    /**
     * Routine that attempt to reset a device between modules in order to provide isolation.
     *
     * @param context The invocation context.
     * @param logger A logger where extra logs can be saved.
     * @throws DeviceNotAvailableException
     */
    private void moduleIsolation(IInvocationContext context, ITestLogger logger)
            throws DeviceNotAvailableException {
        // TODO: we can probably make it smarter: Did any test ran for example?
        ITestDevice device = context.getDevices().get(0);
        if (mIsolatedModule && (device instanceof NestedRemoteDevice)) {
            boolean res =
                    ((NestedRemoteDevice) device)
                            .resetVirtualDevice(
                                    logger,
                                    context.getBuildInfos().get(0),
                                    /* Do not collect the logs */ false);
            if (!res) {
                String serial = device.getSerialNumber();
                throw new DeviceNotAvailableException(
                        String.format(
                                "Failed to reset the AVD '%s' during module isolation.", serial),
                        serial);
            }
        }
    }

    /**
     * Helper method that handle running a single module logic.
     *
     * @param module The {@link ModuleDefinition} to be ran.
     * @param moduleInfo The {@link TestInformation} for the module.
     * @param listener The {@link ITestInvocationListener} where to report results
     * @param moduleListeners The {@link ITestInvocationListener}s that runs at the module level.
     * @param failureListener special listener that we add to collect information on failures.
     * @throws DeviceNotAvailableException
     */
    private void runSingleModule(
            ModuleDefinition module,
            TestInformation moduleInfo,
            ITestInvocationListener listener,
            List<ITestInvocationListener> moduleListeners,
            TestFailureListener failureListener)
            throws DeviceNotAvailableException {
        if (mRebootPerModule) {
            if ("user".equals(mDevice.getProperty(DeviceProperties.BUILD_TYPE))) {
                CLog.e(
                        "reboot-per-module should only be used during development, "
                                + "this is a\" user\" build device");
            } else {
                CLog.d("Rebooting device before starting next module");
                mDevice.reboot();
            }
        }

        if (!mSkipAllSystemStatusCheck) {
            runPreModuleCheck(module.getId(), mSystemStatusCheckers, mDevice, listener);
        }
        if (mCollectTestsOnly) {
            module.setCollectTestsOnly(mCollectTestsOnly);
        }
        // Pass the run defined collectors to be used.
        module.setMetricCollectors(CollectorHelper.cloneCollectors(mMetricCollectors));
        // Pass the main invocation logSaver
        module.setLogSaver(mMainConfiguration.getLogSaver());

        IRetryDecision decision = mMainConfiguration.getRetryDecision();
        // Pass whether we should merge the attempts of not
        if (mMergeAttempts
                && decision.getMaxRetryCount() > 1
                && !RetryStrategy.NO_RETRY.equals(decision.getRetryStrategy())) {
            CLog.d("Overriding '--merge-attempts' to false for auto-retry.");
            mMergeAttempts = false;
        }
        module.setMergeAttemps(mMergeAttempts);
        // Pass the retry decision to be used.
        module.setRetryDecision(decision);

        module.setEnableDynamicDownload(mEnableDynamicDownload);
        module.addDynamicDownloadArgs(
                mMainConfiguration.getCommandOptions().getDynamicDownloadArgs());
        // Actually run the module
        module.run(
                moduleInfo,
                listener,
                moduleListeners,
                failureListener,
                getConfiguration().getRetryDecision().getMaxRetryCount());

        if (!mSkipAllSystemStatusCheck) {
            runPostModuleCheck(module.getId(), mSystemStatusCheckers, mDevice, listener);
        }
    }

    /**
     * Helper to run the System Status checkers preExecutionChecks defined for the test and log
     * their failures.
     */
    private void runPreModuleCheck(
            String moduleName,
            List<ISystemStatusChecker> checkers,
            ITestDevice device,
            ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        long startTime = System.currentTimeMillis();
        CLog.i("Running system status checker before module execution: %s", moduleName);
        Map<String, String> failures = new LinkedHashMap<>();
        boolean bugreportNeeded = false;
        for (ISystemStatusChecker checker : checkers) {
            // Check if the status checker should be skipped.
            if (mSystemStatusCheckBlacklist.contains(checker.getClass().getName())) {
                CLog.d(
                        "%s was skipped via %s",
                        checker.getClass().getName(), SKIP_SYSTEM_STATUS_CHECKER);
                continue;
            }

            StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
            try {
                result = checker.preExecutionCheck(device);
            } catch (RuntimeException e) {
                // Catch RuntimeException to avoid leaking throws that go to the invocation.
                result.setErrorMessage(e.getMessage());
                result.setBugreportNeeded(true);
            }
            if (!CheckStatus.SUCCESS.equals(result.getStatus())) {
                String errorMessage =
                        (result.getErrorMessage() == null) ? "" : result.getErrorMessage();
                failures.put(checker.getClass().getCanonicalName(), errorMessage);
                bugreportNeeded = bugreportNeeded | result.isBugreportNeeded();
                CLog.w("System status checker [%s] failed.", checker.getClass().getCanonicalName());
            }
        }
        if (!failures.isEmpty()) {
            CLog.w("There are failed system status checkers: %s", failures.toString());
            if (bugreportNeeded && !(device.getIDevice() instanceof StubDevice)) {
                device.logBugreport(
                        String.format("bugreport-checker-pre-module-%s", moduleName), listener);
            }
        }

        // We report System checkers like tests.
        reportModuleCheckerResult(MODULE_CHECKER_PRE, moduleName, failures, startTime, listener);
    }

    /**
     * Helper to run the System Status checkers postExecutionCheck defined for the test and log
     * their failures.
     */
    private void runPostModuleCheck(
            String moduleName,
            List<ISystemStatusChecker> checkers,
            ITestDevice device,
            ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        long startTime = System.currentTimeMillis();
        CLog.i("Running system status checker after module execution: %s", moduleName);
        Map<String, String> failures = new LinkedHashMap<>();
        boolean bugreportNeeded = false;
        for (ISystemStatusChecker checker : checkers) {
            // Check if the status checker should be skipped.
            if (mSystemStatusCheckBlacklist.contains(checker.getClass().getName())) {
                continue;
            }

            StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
            try {
                result = checker.postExecutionCheck(device);
            } catch (RuntimeException e) {
                // Catch RuntimeException to avoid leaking throws that go to the invocation.
                result.setErrorMessage(e.getMessage());
                result.setBugreportNeeded(true);
            }
            if (!CheckStatus.SUCCESS.equals(result.getStatus())) {
                String errorMessage =
                        (result.getErrorMessage() == null) ? "" : result.getErrorMessage();
                failures.put(checker.getClass().getCanonicalName(), errorMessage);
                bugreportNeeded = bugreportNeeded | result.isBugreportNeeded();
                CLog.w("System status checker [%s] failed", checker.getClass().getCanonicalName());
            }
        }
        if (!failures.isEmpty()) {
            CLog.w("There are failed system status checkers: %s", failures.toString());
            if (bugreportNeeded && !(device.getIDevice() instanceof StubDevice)) {
                device.logBugreport(
                        String.format("bugreport-checker-post-module-%s", moduleName), listener);
            }
        }

        // We report System checkers like tests.
        reportModuleCheckerResult(MODULE_CHECKER_POST, moduleName, failures, startTime, listener);
    }

    /** Helper to report status checker results as test results. */
    private void reportModuleCheckerResult(
            String identifier,
            String moduleName,
            Map<String, String> failures,
            long startTime,
            ITestInvocationListener listener) {
        if (!mReportSystemChecker) {
            // do not log here, otherwise it could be very verbose.
            return;
        }
        // Avoid messing with the final test count by making them empty runs.
        listener.testRunStarted(identifier + "_" + moduleName, 0, 0, System.currentTimeMillis());
        if (!failures.isEmpty()) {
            listener.testRunFailed(String.format("%s failed '%s' checkers", moduleName, failures));
        }
        listener.testRunEnded(
                System.currentTimeMillis() - startTime, new HashMap<String, Metric>());
    }

    /** Returns true if we are currently in {@link #split(int)}. */
    public boolean isSplitting() {
        return mIsSplitting;
    }

    /** {@inheritDoc} */
    @Override
    public Collection<IRemoteTest> split(Integer shardCountHint, TestInformation testInfo) {
        if (shardCountHint == null || shardCountHint <= 1 || mIsSharded) {
            // cannot shard or already sharded
            return null;
        }
        // TODO: Replace by relying on testInfo directly
        setBuild(testInfo.getBuildInfo());
        setDevice(testInfo.getDevice());
        setInvocationContext(testInfo.getContext());

        mIsSplitting = true;
        try {
            LinkedHashMap<String, IConfiguration> runConfig = loadAndFilter();
            if (runConfig.isEmpty()) {
                CLog.i("No config were loaded. Nothing to run.");
                return null;
            }
            injectInfo(runConfig, testInfo);

            // We split individual tests on double the shardCountHint to provide better average.
            // The test pool mechanism prevent this from creating too much overhead.
            List<ModuleDefinition> splitModules =
                    ModuleSplitter.splitConfiguration(
                            testInfo,
                            runConfig,
                            shardCountHint,
                            mShouldMakeDynamicModule,
                            mIntraModuleSharding);
            runConfig.clear();
            runConfig = null;

            // Clean up the parent that will get sharded: It is fine to clean up before copying the
            // options, because the sharded module is already created/populated so there is no need
            // to carry these extra data.
            cleanUpSuiteSetup();

            // create an association of one ITestSuite <=> one ModuleDefinition as the smallest
            // execution unit supported.
            List<IRemoteTest> splitTests = new ArrayList<>();
            for (ModuleDefinition m : splitModules) {
                ITestSuite suite = createInstance();
                OptionCopier.copyOptionsNoThrow(this, suite);
                suite.mIsSharded = true;
                suite.mDirectModule = m;
                splitTests.add(suite);
            }
            // return the list of ITestSuite with their ModuleDefinition assigned
            return splitTests;
        } finally {
            // Done splitting at that point
            mIsSplitting = false;
        }
    }

    /**
     * Inject {@link ITestDevice} and {@link IBuildInfo} to the {@link IRemoteTest}s in the config
     * before sharding since they may be needed.
     */
    private void injectInfo(
            LinkedHashMap<String, IConfiguration> runConfig, TestInformation testInfo) {
        for (IConfiguration config : runConfig.values()) {
            for (IRemoteTest test : config.getTests()) {
                if (test instanceof IBuildReceiver) {
                    ((IBuildReceiver) test).setBuild(testInfo.getBuildInfo());
                }
                if (test instanceof IDeviceTest) {
                    ((IDeviceTest) test).setDevice(testInfo.getDevice());
                }
                if (test instanceof IInvocationContextReceiver) {
                    ((IInvocationContextReceiver) test).setInvocationContext(testInfo.getContext());
                }
                if (test instanceof ITestCollector) {
                    ((ITestCollector) test).setCollectTestsOnly(mCollectTestsOnly);
                }
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    /** Set the value of mAbiName */
    public void setAbiName(String abiName) {
        mAbiName = abiName;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuildInfo = buildInfo;
    }

    /**
     * Implementation of {@link ITestSuite} may require the build info to load the tests.
     */
    public IBuildInfo getBuildInfo() {
        return mBuildInfo;
    }

    /** Set the value of mPrimaryAbiRun */
    public void setPrimaryAbiRun(boolean primaryAbiRun) {
        mPrimaryAbiRun = primaryAbiRun;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setSystemStatusChecker(List<ISystemStatusChecker> systemCheckers) {
        mSystemStatusCheckers = systemCheckers;
    }

    /**
     * Run the test suite in collector only mode, this requires all the sub-tests to implements this
     * interface too.
     */
    @Override
    public void setCollectTestsOnly(boolean shouldCollectTest) {
        mCollectTestsOnly = shouldCollectTest;
    }

    /** {@inheritDoc} */
    @Override
    public void setMetricCollectors(List<IMetricCollector> collectors) {
        mMetricCollectors = collectors;
    }

    /**
     * When doing distributed sharding, we cannot have ModuleDefinition that shares tests in a pool
     * otherwise intra-module sharding will not work, so we allow to disable it.
     */
    public void setShouldMakeDynamicModule(boolean dynamicModule) {
        mShouldMakeDynamicModule = dynamicModule;
    }

    /** {@inheritDoc} */
    @Override
    public void setInvocationContext(IInvocationContext invocationContext) {
        mContext = invocationContext;
    }

    /**
     * Returns the invocation context.
     */
    public IInvocationContext getInvocationContext() {
        return mContext;
    }

    /** {@inheritDoc} */
    @Override
    public void setTestLogger(ITestLogger testLogger) {
        mCurrentLogger = testLogger;
    }

    public ITestLogger getCurrentTestLogger() {
        return mCurrentLogger;
    }

    /** {@inheritDoc} */
    @Override
    public long getRuntimeHint() {
        if (mDirectModule != null) {
            CLog.d(
                    "    %s: %s",
                    mDirectModule.getId(),
                    TimeUtil.formatElapsedTime(mDirectModule.getRuntimeHint()));
            return mDirectModule.getRuntimeHint();
        }
        return 0l;
    }

    /** {@inheritDoc} */
    @Override
    public void setConfiguration(IConfiguration configuration) {
        mMainConfiguration = configuration;
    }

    /** Returns the invocation {@link IConfiguration}. */
    public final IConfiguration getConfiguration() {
        return mMainConfiguration;
    }

    /** {@inheritDoc} */
    @Override
    public void reportNotExecuted(ITestInvocationListener listener) {
        reportNotExecuted(listener, IReportNotExecuted.NOT_EXECUTED_FAILURE);
    }

    /** {@inheritDoc} */
    @Override
    public void reportNotExecuted(ITestInvocationListener listener, String message) {
        // If the runner is already in progress, report the remaining tests as not executed.
        List<ModuleDefinition> runModules = null;
        if (mRunModules != null) {
            runModules = new ArrayList<>(mRunModules);
        }
        if (runModules == null) {
            runModules = createExecutionList();
        }

        if (mModuleInProgress != null) {
            // TODO: Ensure in-progress data make sense
            String inProgressMessage =
                    String.format(
                            "Module %s was interrupted after starting. Results might not be "
                                    + "accurate or complete.",
                            mModuleInProgress.getId());
            mModuleInProgress.reportNotExecuted(listener, inProgressMessage);
        }

        while (!runModules.isEmpty()) {
            ModuleDefinition module = runModules.remove(0);
            module.reportNotExecuted(listener, message);
        }
    }

    public void addModuleMetadataIncludeFilters(MultiMap<String, String> filters) {
        mModuleMetadataIncludeFilter.putAll(filters);
    }

    public void addModuleMetadataExcludeFilters(MultiMap<String, String> filters) {
        mModuleMetadataExcludeFilter.putAll(filters);
    }

    /**
     * Returns the {@link ModuleDefinition} to be executed directly, or null if none yet (when the
     * ITestSuite has not been sharded yet).
     */
    public ModuleDefinition getDirectModule() {
        return mDirectModule;
    }

    @Override
    public Set<TokenProperty> getRequiredTokens() {
        if (mDirectModule == null) {
            return null;
        }
        return mDirectModule.getRequiredTokens();
    }

    /**
     * Gets the set of ABIs supported by both Compatibility testing {@link
     * AbiUtils#getAbisSupportedByCompatibility()} and the device under test.
     *
     * @return The set of ABIs to run the tests on
     * @throws DeviceNotAvailableException
     */
    public Set<IAbi> getAbis(ITestDevice device) throws DeviceNotAvailableException {
        Set<IAbi> abis = new LinkedHashSet<>();
        Set<String> archAbis = getAbisForBuildTargetArch();
        // Handle null-device: use abi in common with host and suite build
        if (mPrimaryAbiRun) {
            if (mAbiName == null) {
                // Get the primary from the device and make it the --abi to run.
                mAbiName = getPrimaryAbi(device);
            } else {
                CLog.d(
                        "Option --%s supersedes the option --%s, using abi: %s",
                        ABI_OPTION, PRIMARY_ABI_RUN, mAbiName);
            }
        }
        if (mAbiName != null) {
            // A particular abi was requested, it still needs to be supported by the build.
            if ((!mSkipHostArchCheck && !archAbis.contains(mAbiName))
                    || !AbiUtils.isAbiSupportedByCompatibility(mAbiName)) {
                throw new IllegalArgumentException(
                        String.format(
                                "Your tests suite hasn't been built with "
                                        + "abi '%s' support, this suite currently supports '%s'.",
                                mAbiName, archAbis));
            } else {
                abis.add(new Abi(mAbiName, AbiUtils.getBitness(mAbiName)));
                return abis;
            }
        } else {
            // Run on all abi in common between the device and suite builds.
            List<String> deviceAbis = getDeviceAbis(device);
            if (deviceAbis.isEmpty()) {
                throw new IllegalArgumentException(
                        String.format(
                                "Couldn't determinate the abi of the device '%s'.",
                                device.getSerialNumber()));
            }
            for (String abi : deviceAbis) {
                if ((mSkipHostArchCheck || archAbis.contains(abi))
                        && AbiUtils.isAbiSupportedByCompatibility(abi)) {
                    abis.add(new Abi(abi, AbiUtils.getBitness(abi)));
                } else {
                    CLog.d(
                            "abi '%s' is supported by device but not by this suite build (%s), "
                                    + "tests will not run against it.",
                            abi, archAbis);
                }
            }
            if (abis.isEmpty()) {
                throw new IllegalArgumentException(
                        String.format(
                                "None of the abi supported by this tests suite build ('%s') are "
                                        + "supported by the device ('%s').",
                                archAbis, deviceAbis));
            }
            return abis;
        }
    }

    /** Returns the primary abi of the device or host if it's a null device. */
    private String getPrimaryAbi(ITestDevice device) throws DeviceNotAvailableException {
        if (device.getIDevice() instanceof NullDevice) {
            Set<String> hostAbis = getHostAbis();
            return hostAbis.iterator().next();
        }
        String property = device.getProperty(PRODUCT_CPU_ABI_KEY);
        if (property == null) {
            String serial = device.getSerialNumber();
            throw new DeviceNotAvailableException(
                    String.format(
                            "Device '%s' was not online to query %s", serial, PRODUCT_CPU_ABI_KEY),
                    serial);
        }
        return property.trim();
    }

    /** Returns the list of abis supported by the device or host if it's a null device. */
    private List<String> getDeviceAbis(ITestDevice device) throws DeviceNotAvailableException {
        if (device.getIDevice() instanceof NullDevice) {
            return new ArrayList<>(getHostAbis());
        }
        // Make it an arrayList to be able to modify the content.
        return new ArrayList<>(Arrays.asList(AbiFormatter.getSupportedAbis(device, "")));
    }

    /** Return the abis supported by the Host build target architecture. Exposed for testing. */
    @VisibleForTesting
    protected Set<String> getAbisForBuildTargetArch() {
        // If TestSuiteInfo does not exists, the stub arch will be replaced by all possible abis.
        Set<String> abis = new LinkedHashSet<>();
        for (String arch : TestSuiteInfo.getInstance().getTargetArchs()) {
            abis.addAll(AbiUtils.getAbisForArch(arch));
        }
        return abis;
    }

    /** Returns the host machine abis. */
    @VisibleForTesting
    protected Set<String> getHostAbis() {
        return AbiUtils.getHostAbi();
    }

    /** Returns the abi requested with the option -a or --abi. */
    public final String getRequestedAbi() {
        return mAbiName;
    }

    /** Getter used to validate the proper Guice injection. */
    @VisibleForTesting
    final Injector getInjector() {
        return mInjector;
    }

    /** Sets reboot-before-test to true. */
    public final void enableRebootBeforeTest() {
        mRebootBeforeTest = true;
    }

    /**
     * Apply the metadata filter to the config and see if the config should run.
     *
     * @param config The {@link IConfiguration} being evaluated.
     * @param include the metadata include filter
     * @param exclude the metadata exclude filter
     * @return True if the module should run, false otherwise.
     */
    @VisibleForTesting
    protected boolean filterByConfigMetadata(
            IConfiguration config,
            MultiMap<String, String> include,
            MultiMap<String, String> exclude) {
        MultiMap<String, String> metadata = config.getConfigurationDescription().getAllMetaData();
        boolean shouldInclude = false;
        for (String key : include.keySet()) {
            Set<String> filters = new HashSet<>(include.get(key));
            if (metadata.containsKey(key)) {
                filters.retainAll(metadata.get(key));
                if (!filters.isEmpty()) {
                    // inclusion filter is not empty and there's at least one matching inclusion
                    // rule so there's no need to match other inclusion rules
                    shouldInclude = true;
                    break;
                }
            }
        }
        if (!include.isEmpty() && !shouldInclude) {
            // if inclusion filter is not empty and we didn't find a match, the module will not be
            // included
            return false;
        }
        // Now evaluate exclusion rules, this ordering also means that exclusion rules may override
        // inclusion rules: a config already matched for inclusion may still be excluded if matching
        // rules exist
        for (String key : exclude.keySet()) {
            Set<String> filters = new HashSet<>(exclude.get(key));
            if (metadata.containsKey(key)) {
                filters.retainAll(metadata.get(key));
                if (!filters.isEmpty()) {
                    // we found at least one matching exclusion rules, so we are excluding this
                    // this module
                    return false;
                }
            }
        }
        // we've matched at least one inclusion rule (if there's any) AND we didn't match any of the
        // exclusion rules (if there's any)
        return true;
    }

    /**
     * Filter out the preparers that were not whitelisted. This is useful for collect-tests-only
     * where some preparers are not needed to dry run through the invocation.
     *
     * @param config the {@link IConfiguration} considered for filtering.
     * @param preparerWhiteList the current preparer whitelist.
     */
    @VisibleForTesting
    void filterPreparers(IConfiguration config, Set<String> preparerWhiteList) {
        // If no filters was provided, skip the filtering.
        if (preparerWhiteList.isEmpty()) {
            return;
        }
        for (IDeviceConfiguration deviceConfig : config.getDeviceConfig()) {
            List<ITargetPreparer> preparers = new ArrayList<>(deviceConfig.getTargetPreparers());
            for (ITargetPreparer prep : preparers) {
                if (!preparerWhiteList.contains(prep.getClass().getName())) {
                    deviceConfig.getTargetPreparers().remove(prep);
                }
            }
        }
    }

    /**
     * Apply the Runner whitelist filtering, removing any runner that was not whitelisted. If a
     * configuration has several runners, some might be removed and the config will still run.
     *
     * @param config The {@link IConfiguration} being evaluated.
     * @param allowedRunners The current runner whitelist.
     * @return True if the configuration module is allowed to run, false otherwise.
     */
    @VisibleForTesting
    protected boolean filterByRunnerType(IConfiguration config, Set<String> allowedRunners) {
        // If no filters are provided, simply run everything.
        if (allowedRunners.isEmpty()) {
            return true;
        }
        Iterator<IRemoteTest> iterator = config.getTests().iterator();
        while (iterator.hasNext()) {
            IRemoteTest test = iterator.next();
            if (!allowedRunners.contains(test.getClass().getName())) {
                CLog.d(
                        "Runner '%s' in module '%s' was skipped by the runner whitelist: '%s'.",
                        test.getClass().getName(), config.getName(), allowedRunners);
                iterator.remove();
            }
        }

        if (config.getTests().isEmpty()) {
            CLog.d("Module %s does not have any more tests, skipping it.", config.getName());
            return false;
        }
        return true;
    }

    void disableAutoRetryTimeReporting() {
        mDisableAutoRetryTimeReporting = true;
    }

    @VisibleForTesting
    void setModuleInProgress(ModuleDefinition moduleInProgress) {
        mModuleInProgress = moduleInProgress;
    }
}
