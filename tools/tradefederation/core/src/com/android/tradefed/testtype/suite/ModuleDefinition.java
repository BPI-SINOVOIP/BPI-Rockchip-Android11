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

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DeviceConfigurationHolder;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.ITestDevice.RecoveryMode;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.device.metric.LogcatOnFailureCollector;
import com.android.tradefed.device.metric.ScreenshotOnFailureCollector;
import com.android.tradefed.error.IHarnessException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.logger.CurrentInvocation;
import com.android.tradefed.invoker.logger.InvocationMetricLogger;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.invoker.logger.TfObjectTracker;
import com.android.tradefed.invoker.shard.token.TokenProperty;
import com.android.tradefed.log.ILogRegistry.EventType;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogRegistry;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.FailureDescription;
import com.android.tradefed.result.ILogSaver;
import com.android.tradefed.result.ILogSaverListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.ITestLoggerReceiver;
import com.android.tradefed.result.LogFile;
import com.android.tradefed.result.MultiFailureDescription;
import com.android.tradefed.result.ResultForwarder;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.result.error.ErrorIdentifier;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.retry.RetryStatistics;
import com.android.tradefed.suite.checker.ISystemStatusCheckerReceiver;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.multi.IMultiTargetPreparer;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IRuntimeHintProvider;
import com.android.tradefed.testtype.ITestCollector;
import com.android.tradefed.testtype.suite.module.BaseModuleController;
import com.android.tradefed.testtype.suite.module.IModuleController.RunStrategy;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import com.google.common.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

/**
 * Container for the test run configuration. This class is an helper to prepare and run the tests.
 */
public class ModuleDefinition implements Comparable<ModuleDefinition>, ITestCollector {

    /** key names used for saving module info into {@link IInvocationContext} */
    /**
     * Module name is the base name associated with the module, usually coming from the Xml TF
     * config file the module was loaded from.
     */
    public static final String MODULE_NAME = "module-name";
    public static final String MODULE_ABI = "module-abi";
    public static final String MODULE_PARAMETERIZATION = "module-param";
    /**
     * Module ID the name that will be used to identify uniquely the module during testRunStart. It
     * will usually be a combination of MODULE_ABI + MODULE_NAME.
     */
    public static final String MODULE_ID = "module-id";

    public static final String MODULE_CONTROLLER = "module_controller";

    public static final String PREPARATION_TIME = "PREP_TIME";
    public static final String TEAR_DOWN_TIME = "TEARDOWN_TIME";
    public static final String TEST_TIME = "TEST_TIME";
    public static final String RETRY_TIME = "MODULE_RETRY_TIME";
    public static final String RETRY_SUCCESS_COUNT = "MODULE_RETRY_SUCCESS";
    public static final String RETRY_FAIL_COUNT = "MODULE_RETRY_FAILED";

    private final IInvocationContext mModuleInvocationContext;
    private final IConfiguration mModuleConfiguration;
    private IConfiguration mInternalTestConfiguration;
    private IConfiguration mInternalTargetPreparerConfiguration;
    private ILogSaver mLogSaver;

    private final String mId;
    private Collection<IRemoteTest> mTests = null;
    private Map<String, List<ITargetPreparer>> mPreparersPerDevice = null;

    private List<IMultiTargetPreparer> mMultiPreparers = new ArrayList<>();
    private IBuildInfo mBuild;
    private ITestDevice mDevice;
    private List<IMetricCollector> mRunMetricCollectors = new ArrayList<>();
    private boolean mCollectTestsOnly = false;

    private List<TestRunResult> mTestsResults = new ArrayList<>();
    private List<ModuleListener> mRunListenersResults = new ArrayList<>();
    private int mExpectedTests = 0;
    private boolean mIsFailedModule = false;

    // Tracking of preparers performance
    private long mElapsedPreparation = 0l;
    private long mElapsedTearDown = 0l;

    private long mStartTestTime = 0l;
    private Long mStartModuleRunDate = null;

    // Tracking of retry performance
    private List<RetryStatistics> mRetryStats = new ArrayList<>();
    private boolean mDisableAutoRetryTimeReporting = false;

    private boolean mMergeAttempts = true;
    private IRetryDecision mRetryDecision;

    // Token during sharding
    private Set<TokenProperty> mRequiredTokens = new HashSet<>();

    private boolean mEnableDynamicDownload = false;

    /**
     * Constructor
     *
     * @param name unique name of the test configuration.
     * @param tests list of {@link IRemoteTest} that needs to run.
     * @param preparersPerDevice list of {@link ITargetPreparer} to be used to setup the device.
     * @param moduleConfig the {@link IConfiguration} of the underlying module config.
     */
    public ModuleDefinition(
            String name,
            Collection<IRemoteTest> tests,
            Map<String, List<ITargetPreparer>> preparersPerDevice,
            List<IMultiTargetPreparer> multiPreparers,
            IConfiguration moduleConfig) {
        mId = name;
        mTests = tests;
        mModuleConfiguration = moduleConfig;
        ConfigurationDescriptor configDescriptor = moduleConfig.getConfigurationDescription();
        mModuleInvocationContext = new InvocationContext();
        mModuleInvocationContext.setConfigurationDescriptor(configDescriptor.clone());

        // If available in the suite, add the abi name
        if (configDescriptor.getAbi() != null) {
            mModuleInvocationContext.addInvocationAttribute(
                    MODULE_ABI, configDescriptor.getAbi().getName());
        }
        if (configDescriptor.getModuleName() != null) {
            mModuleInvocationContext.addInvocationAttribute(
                    MODULE_NAME, configDescriptor.getModuleName());
        }
        String parameterization =
                configDescriptor
                        .getAllMetaData()
                        .getUniqueMap()
                        .get(ConfigurationDescriptor.ACTIVE_PARAMETER_KEY);
        if (parameterization != null) {
            mModuleInvocationContext.addInvocationAttribute(
                    MODULE_PARAMETERIZATION, parameterization);
        }
        // If there is no specific abi, module-id should be module-name
        mModuleInvocationContext.addInvocationAttribute(MODULE_ID, mId);

        mMultiPreparers.addAll(multiPreparers);
        mPreparersPerDevice = preparersPerDevice;

        // Get the tokens of the module
        List<String> tokens = configDescriptor.getMetaData(ITestSuite.TOKEN_KEY);
        if (tokens != null) {
            for (String token : tokens) {
                mRequiredTokens.add(TokenProperty.valueOf(token.toUpperCase()));
            }
        }
    }

    /**
     * Returns the next {@link IRemoteTest} from the list of tests. The list of tests of a module
     * may be shared with another one in case of sharding.
     */
    IRemoteTest poll() {
        synchronized (mTests) {
            if (mTests.isEmpty()) {
                return null;
            }
            IRemoteTest test = mTests.iterator().next();
            mTests.remove(test);
            return test;
        }
    }

    /**
     * Add some {@link IRemoteTest} to be executed as part of the module. Used when merging two
     * modules.
     */
    void addTests(List<IRemoteTest> test) {
        synchronized (mTests) {
            mTests.addAll(test);
        }
    }

    /** Returns the current number of {@link IRemoteTest} waiting to be executed. */
    public int numTests() {
        synchronized (mTests) {
            return mTests.size();
        }
    }

    /**
     * Return True if the Module still has {@link IRemoteTest} to run in its pool. False otherwise.
     */
    protected boolean hasTests() {
        synchronized (mTests) {
            return mTests.isEmpty();
        }
    }

    /** Return the unique module name. */
    public String getId() {
        return mId;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public int compareTo(ModuleDefinition moduleDef) {
        return getId().compareTo(moduleDef.getId());
    }

    /**
     * Inject the {@link IBuildInfo} to be used during the tests.
     */
    public void setBuild(IBuildInfo build) {
        mBuild = build;
    }

    /**
     * Inject the {@link ITestDevice} to be used during the tests.
     */
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /** Inject the List of {@link IMetricCollector} to be used by the module. */
    public void setMetricCollectors(List<IMetricCollector> collectors) {
        if (collectors == null) {
            return;
        }
        mRunMetricCollectors.addAll(collectors);
    }

    /** Pass the invocation log saver to the module so it can use it if necessary. */
    public void setLogSaver(ILogSaver logSaver) {
        mLogSaver = logSaver;
    }

    /**
     * Run all the {@link IRemoteTest} contained in the module and use all the preparers before and
     * after to setup and clean the device.
     *
     * @param listener the {@link ITestInvocationListener} where to report results.
     * @throws DeviceNotAvailableException in case of device going offline.
     */
    public final void run(TestInformation moduleInfo, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        run(moduleInfo, listener, null, null);
    }

    /**
     * Run all the {@link IRemoteTest} contained in the module and use all the preparers before and
     * after to setup and clean the device.
     *
     * @param listener the {@link ITestInvocationListener} where to report results.
     * @param moduleLevelListeners The list of listeners at the module level.
     * @param failureListener a particular listener to collect logs on testFail. Can be null.
     * @throws DeviceNotAvailableException in case of device going offline.
     */
    public final void run(
            TestInformation moduleInfo,
            ITestInvocationListener listener,
            List<ITestInvocationListener> moduleLevelListeners,
            TestFailureListener failureListener)
            throws DeviceNotAvailableException {
        run(moduleInfo, listener, moduleLevelListeners, failureListener, 1);
    }

    /**
     * Run all the {@link IRemoteTest} contained in the module and use all the preparers before and
     * after to setup and clean the device.
     *
     * @param moduleInfo the {@link TestInformation} for the module.
     * @param listener the {@link ITestInvocationListener} where to report results.
     * @param moduleLevelListeners The list of listeners at the module level.
     * @param failureListener a particular listener to collect logs on testFail. Can be null.
     * @param maxRunLimit the max number of runs for each testcase.
     * @throws DeviceNotAvailableException in case of device going offline.
     */
    public final void run(
            TestInformation moduleInfo,
            ITestInvocationListener listener,
            List<ITestInvocationListener> moduleLevelListeners,
            TestFailureListener failureListener,
            int maxRunLimit)
            throws DeviceNotAvailableException {
        mStartModuleRunDate = System.currentTimeMillis();
        // Load extra configuration for the module from module_controller
        // TODO: make module_controller a full TF object
        boolean skipTestCases = false;
        RunStrategy rs = applyConfigurationControl(failureListener);
        if (RunStrategy.FULL_MODULE_BYPASS.equals(rs)) {
            CLog.d("module_controller applied and module %s should not run.", getId());
            return;
        } else if (RunStrategy.SKIP_MODULE_TESTCASES.equals(rs)) {
            CLog.d("All tests cases for %s will be marked skipped.", getId());
            skipTestCases = true;
        }

        CLog.logAndDisplay(LogLevel.DEBUG, "Running module %s", getId());
        // Exception generated during setUp or run of the tests
        Throwable preparationException = null;
        DeviceNotAvailableException runException = null;
        // Resolve dynamic files except for the IRemoteTest ones
        preparationException = invokeRemoteDynamic(moduleInfo.getDevice(), mModuleConfiguration);

        if (preparationException == null) {
            mInternalTargetPreparerConfiguration =
                    new Configuration("tmp-download", "tmp-download");
            mInternalTargetPreparerConfiguration
                    .getCommandOptions()
                    .getDynamicDownloadArgs()
                    .putAll(mModuleConfiguration.getCommandOptions().getDynamicDownloadArgs());
            for (String device : mPreparersPerDevice.keySet()) {
                mInternalTargetPreparerConfiguration.setDeviceConfig(
                        new DeviceConfigurationHolder(device));
                for (ITargetPreparer preparer : mPreparersPerDevice.get(device)) {
                    try {
                        mInternalTargetPreparerConfiguration
                                .getDeviceConfigByName(device)
                                .addSpecificConfig(preparer);
                    } catch (ConfigurationException e) {
                        // Shouldn't happen;
                        throw new RuntimeException(e);
                    }
                }
            }
            mInternalTargetPreparerConfiguration.setMultiTargetPreparers(mMultiPreparers);
            preparationException =
                    invokeRemoteDynamic(
                            moduleInfo.getDevice(), mInternalTargetPreparerConfiguration);
        }
        // Setup
        long prepStartTime = getCurrentTime();
        if (preparationException == null) {
            preparationException = runTargetPreparation(moduleInfo, listener);
        }
        // Skip multi-preparation if preparation already failed.
        if (preparationException == null) {
            for (IMultiTargetPreparer multiPreparer : mMultiPreparers) {
                preparationException = runMultiPreparerSetup(multiPreparer, moduleInfo, listener);
                if (preparationException != null) {
                    mIsFailedModule = true;
                    CLog.e("Some preparation step failed. failing the module %s", getId());
                    break;
                }
            }
        }
        mElapsedPreparation = getCurrentTime() - prepStartTime;
        // Run the tests
        try {
            if (preparationException != null) {
                reportSetupFailure(preparationException, listener, moduleLevelListeners);
                return;
            }
            mStartTestTime = getCurrentTime();
            while (true) {
                IRemoteTest test = poll();
                if (test == null) {
                    return;
                }
                TfObjectTracker.countWithParents(test.getClass());
                if (test instanceof IBuildReceiver) {
                    ((IBuildReceiver) test).setBuild(mBuild);
                }
                if (test instanceof IDeviceTest) {
                    ((IDeviceTest) test).setDevice(mDevice);
                }
                if (test instanceof IInvocationContextReceiver) {
                    ((IInvocationContextReceiver) test)
                            .setInvocationContext(mModuleInvocationContext);
                }
                mInternalTestConfiguration = new Configuration("tmp-download", "tmp-download");
                mInternalTestConfiguration
                        .getCommandOptions()
                        .getDynamicDownloadArgs()
                        .putAll(mModuleConfiguration.getCommandOptions().getDynamicDownloadArgs());
                // We do it before the official set, otherwise the IConfiguration will not be the
                // right one.
                mInternalTestConfiguration.setTest(test);
                if (test instanceof IConfigurationReceiver) {
                    ((IConfigurationReceiver) test).setConfiguration(mModuleConfiguration);
                }
                if (test instanceof ISystemStatusCheckerReceiver) {
                    // We do not pass down Status checker because they are already running at the
                    // top level suite.
                    ((ISystemStatusCheckerReceiver) test).setSystemStatusChecker(new ArrayList<>());
                }
                if (test instanceof ITestCollector) {
                    if (skipTestCases) {
                        mCollectTestsOnly = true;
                    }
                    ((ITestCollector) test).setCollectTestsOnly(mCollectTestsOnly);
                }
                GranularRetriableTestWrapper retriableTest =
                        prepareGranularRetriableWrapper(
                                test,
                                listener,
                                failureListener,
                                moduleLevelListeners,
                                skipTestCases,
                                maxRunLimit);
                retriableTest.setCollectTestsOnly(mCollectTestsOnly);
                // Resolve the dynamic options for that one test.
                preparationException =
                        invokeRemoteDynamic(moduleInfo.getDevice(), mInternalTestConfiguration);
                if (preparationException != null) {
                    reportSetupFailure(preparationException, listener, moduleLevelListeners);
                    return;
                }
                try {
                    retriableTest.run(moduleInfo, listener);
                } catch (DeviceNotAvailableException dnae) {
                    runException = dnae;
                    // We do special logging of some information in Context of the module for easier
                    // debugging.
                    CLog.e(
                            "Module %s threw a DeviceNotAvailableException on device %s during "
                                    + "test %s",
                            getId(), mDevice.getSerialNumber(), test.getClass());
                    CLog.e(dnae);
                    // log an events
                    logDeviceEvent(
                            EventType.MODULE_DEVICE_NOT_AVAILABLE,
                            mDevice.getSerialNumber(),
                            dnae,
                            getId());
                    throw dnae;
                } finally {
                    mInternalTestConfiguration.cleanConfigurationData();
                    mInternalTestConfiguration = null;
                    if (mMergeAttempts) {
                        // A single module can generate several test runs
                        mTestsResults.addAll(retriableTest.getFinalTestRunResults());
                    } else {
                        // Keep track of each listener for attempts
                        mRunListenersResults.add(retriableTest.getResultListener());
                    }

                    mExpectedTests += retriableTest.getExpectedTestsCount();
                    // Get information about retry
                    if (mRetryDecision != null) {
                        RetryStatistics res = mRetryDecision.getRetryStatistics();
                        if (res != null) {
                            addRetryTime(res.mRetryTime);
                            mRetryStats.add(res);
                        }
                    }
                }
                // After the run, if the test failed (even after retry the final result passed) has
                // failed, capture a bugreport.
                if (retriableTest.getResultListener().hasLastAttemptFailed()) {
                    captureBugreport(
                            listener,
                            getId(),
                            retriableTest
                                    .getResultListener()
                                    .getCurrentRunResults()
                                    .getRunFailureDescription());
                }
            }
        } finally {
            // Clean target preparers dynamic files.
            if (mInternalTargetPreparerConfiguration != null) {
                mInternalTargetPreparerConfiguration.cleanConfigurationData();
                mInternalTargetPreparerConfiguration = null;
            }
            long cleanStartTime = getCurrentTime();
            RuntimeException tearDownException = null;
            try {
                Throwable exception = (runException != null) ? runException : preparationException;
                // Tear down
                runTearDown(moduleInfo, exception);
            } catch (DeviceNotAvailableException dnae) {
                CLog.e(
                        "Module %s failed during tearDown with: %s",
                        getId(), StreamUtil.getStackTrace(dnae));
                throw dnae;
            } catch (RuntimeException e) {
                CLog.e("Exception while running tearDown:");
                CLog.e(e);
                tearDownException = e;
            } finally {
                if (failureListener != null) {
                    failureListener.join();
                }
                mElapsedTearDown = getCurrentTime() - cleanStartTime;
                // finalize results
                if (preparationException == null) {
                    mModuleConfiguration.cleanConfigurationData();
                    if (mMergeAttempts) {
                        reportFinalResults(
                                listener, mExpectedTests, mTestsResults, null, tearDownException);
                    } else {
                        // Push the attempts one by one
                        for (int i = 0; i < maxRunLimit; i++) {
                            // Get all the results for the attempt
                            List<TestRunResult> runResultList = new ArrayList<TestRunResult>();
                            int expectedCount = 0;
                            for (ModuleListener attemptListener : mRunListenersResults) {
                                for (String runName : attemptListener.getTestRunNames()) {
                                    TestRunResult run =
                                            attemptListener.getTestRunAtAttempt(runName, i);
                                    if (run != null) {
                                        runResultList.add(run);
                                        expectedCount += run.getExpectedTestCount();
                                    }
                                }
                            }

                            if (!runResultList.isEmpty()) {
                                reportFinalResults(
                                        listener,
                                        expectedCount,
                                        runResultList,
                                        i,
                                        tearDownException);
                            } else {
                                CLog.d("No results to be forwarded for attempt %s.", i);
                            }
                        }
                    }
                }
            }
        }
    }

    /**
     * Create a wrapper class for the {@link IRemoteTest} which has built-in logic to schedule
     * multiple test runs for the same module, and have the ability to run testcases at a more
     * granular level (a subset of testcases in the module).
     *
     * @param test the {@link IRemoteTest} that is being wrapped.
     * @param failureListener a particular listener to collect logs on testFail. Can be null.
     * @param skipTestCases A run strategy when SKIP_MODULE_TESTCASES is defined.
     * @param maxRunLimit a rate-limiter on testcases retrying times.
     */
    @VisibleForTesting
    GranularRetriableTestWrapper prepareGranularRetriableWrapper(
            IRemoteTest test,
            ITestInvocationListener listener,
            TestFailureListener failureListener,
            List<ITestInvocationListener> moduleLevelListeners,
            boolean skipTestCases,
            int maxRunLimit) {
        GranularRetriableTestWrapper retriableTest =
                new GranularRetriableTestWrapper(
                        test, listener, failureListener, moduleLevelListeners, maxRunLimit);
        retriableTest.setModuleId(getId());
        retriableTest.setMarkTestsSkipped(skipTestCases);
        retriableTest.setMetricCollectors(mRunMetricCollectors);
        retriableTest.setModuleConfig(mModuleConfiguration);
        retriableTest.setInvocationContext(mModuleInvocationContext);
        retriableTest.setLogSaver(mLogSaver);
        retriableTest.setRetryDecision(mRetryDecision);
        return retriableTest;
    }

    private void captureBugreport(
            ITestLogger listener, String moduleId, FailureDescription failure) {
        FailureStatus status = failure.getFailureStatus();
        if (!FailureStatus.LOST_SYSTEM_UNDER_TEST.equals(status)
                && !FailureStatus.SYSTEM_UNDER_TEST_CRASHED.equals(status)) {
            return;
        }
        for (ITestDevice device : mModuleInvocationContext.getDevices()) {
            if (device.getIDevice() instanceof StubDevice) {
                continue;
            }
            device.logBugreport(
                    String.format(
                            "module-%s-failure-%s-bugreport", moduleId, device.getSerialNumber()),
                    listener);
        }
    }

    /** Helper to log the device events. */
    private void logDeviceEvent(EventType event, String serial, Throwable t, String moduleId) {
        Map<String, String> args = new HashMap<>();
        args.put("serial", serial);
        args.put("trace", StreamUtil.getStackTrace(t));
        args.put("module-id", moduleId);
        LogRegistry.getLogRegistry().logEvent(LogLevel.DEBUG, event, args);
    }

    /** Finalize results to report them all and count if there are missing tests. */
    private void reportFinalResults(
            ITestInvocationListener listener,
            int totalExpectedTests,
            List<TestRunResult> listResults,
            Integer attempt,
            RuntimeException tearDownException) {
        long elapsedTime = 0l;
        HashMap<String, Metric> metricsProto = new HashMap<>();
        if (attempt != null) {
            long startTime =
                    listResults.isEmpty() ? mStartTestTime : listResults.get(0).getStartTime();
            listener.testRunStarted(getId(), totalExpectedTests, attempt, startTime);
        } else {
            listener.testRunStarted(getId(), totalExpectedTests, 0, mStartTestTime);
        }
        int numResults = 0;
        MultiMap<String, LogFile> aggLogFiles = new MultiMap<>();
        List<FailureDescription> runFailureMessages = new ArrayList<>();
        for (TestRunResult runResult : listResults) {
            numResults += runResult.getTestResults().size();
            forwardTestResults(runResult.getTestResults(), listener);
            if (runResult.isRunFailure()) {
                runFailureMessages.add(runResult.getRunFailureDescription());
            }
            elapsedTime += runResult.getElapsedTime();
            // put metrics from the tests
            metricsProto.putAll(runResult.getRunProtoMetrics());
            aggLogFiles.putAll(runResult.getRunLoggedFiles());
        }
        // put metrics from the preparation
        metricsProto.put(
                PREPARATION_TIME,
                TfMetricProtoUtil.createSingleValue(mElapsedPreparation, "milliseconds"));
        metricsProto.put(
                TEAR_DOWN_TIME,
                TfMetricProtoUtil.createSingleValue(mElapsedTearDown, "milliseconds"));
        metricsProto.put(
                TEST_TIME, TfMetricProtoUtil.createSingleValue(elapsedTime, "milliseconds"));
        // Report all the retry informations
        if (!mRetryStats.isEmpty()) {
            RetryStatistics agg = RetryStatistics.aggregateStatistics(mRetryStats);
            metricsProto.put(
                    RETRY_TIME,
                    TfMetricProtoUtil.createSingleValue(agg.mRetryTime, "milliseconds"));
            metricsProto.put(
                    RETRY_SUCCESS_COUNT,
                    TfMetricProtoUtil.createSingleValue(agg.mRetrySuccess, ""));
            metricsProto.put(
                    RETRY_FAIL_COUNT, TfMetricProtoUtil.createSingleValue(agg.mRetryFailure, ""));
        }

        // Only report the mismatch if there were no error during the run.
        if (runFailureMessages.isEmpty() && totalExpectedTests != numResults) {
            String error =
                    String.format(
                            "Module %s only ran %d out of %d expected tests.",
                            getId(), numResults, totalExpectedTests);
            FailureDescription mismatch =
                    FailureDescription.create(error)
                            .setFailureStatus(FailureStatus.TEST_FAILURE)
                            .setErrorIdentifier(InfraErrorIdentifier.EXPECTED_TESTS_MISMATCH);
            runFailureMessages.add(mismatch);
            CLog.e(error);
        }

        if (tearDownException != null) {
            FailureDescription failure =
                    CurrentInvocation.createFailure(
                                    StreamUtil.getStackTrace(tearDownException), null)
                            .setCause(tearDownException);
            runFailureMessages.add(failure);
        }
        // If there is any errors report them all at once
        if (!runFailureMessages.isEmpty()) {
            if (runFailureMessages.size() == 1) {
                listener.testRunFailed(runFailureMessages.get(0));
            } else {
                listener.testRunFailed(new MultiFailureDescription(runFailureMessages));
            }
            mIsFailedModule = true;
        }

        // Provide a strong association of the run to its logs.
        for (String key : aggLogFiles.keySet()) {
            for (LogFile logFile : aggLogFiles.get(key)) {
                if (listener instanceof ILogSaverListener) {
                    ((ILogSaverListener) listener).logAssociation(key, logFile);
                }
            }
        }
        // Allow each attempt to have its own start/end time
        if (attempt != null) {
            listener.testRunEnded(elapsedTime, metricsProto);
        } else {
            listener.testRunEnded(getCurrentTime() - mStartTestTime, metricsProto);
        }
    }

    private void forwardTestResults(
            Map<TestDescription, TestResult> testResults, ITestInvocationListener listener) {
        for (Map.Entry<TestDescription, TestResult> testEntry : testResults.entrySet()) {
            listener.testStarted(testEntry.getKey(), testEntry.getValue().getStartTime());
            switch (testEntry.getValue().getStatus()) {
                case FAILURE:
                    listener.testFailed(testEntry.getKey(), testEntry.getValue().getFailure());
                    break;
                case ASSUMPTION_FAILURE:
                    listener.testAssumptionFailure(
                            testEntry.getKey(), testEntry.getValue().getStackTrace());
                    break;
                case IGNORED:
                    listener.testIgnored(testEntry.getKey());
                    break;
                case INCOMPLETE:
                    listener.testFailed(
                            testEntry.getKey(),
                            FailureDescription.create(
                                    "Test did not complete due to exception.",
                                    FailureStatus.TEST_FAILURE));
                    break;
                default:
                    break;
            }
            // Provide a strong association of the test to its logs.
            for (Entry<String, LogFile> logFile :
                    testEntry.getValue().getLoggedFiles().entrySet()) {
                if (listener instanceof ILogSaverListener) {
                    ((ILogSaverListener) listener)
                            .logAssociation(logFile.getKey(), logFile.getValue());
                }
            }
            listener.testEnded(
                    testEntry.getKey(),
                    testEntry.getValue().getEndTime(),
                    testEntry.getValue().getProtoMetrics());
        }
    }

    /** Run all the prepare steps. */
    private Throwable runPreparerSetup(
            TestInformation moduleInfo,
            ITargetPreparer preparer,
            ITestLogger logger,
            int deviceIndex) {
        if (preparer.isDisabled()) {
            // If disabled skip completely.
            return null;
        }
        TfObjectTracker.countWithParents(preparer.getClass());
        CLog.d("Running setup preparer: %s", preparer.getClass().getSimpleName());
        try {
            // set the logger in case they need it.
            if (preparer instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) preparer).setTestLogger(logger);
            }
            if (preparer instanceof IInvocationContextReceiver) {
                ((IInvocationContextReceiver) preparer)
                        .setInvocationContext(mModuleInvocationContext);
            }
            moduleInfo.setActiveDeviceIndex(deviceIndex);
            preparer.setUp(moduleInfo);
            return null;
        } catch (BuildError
                | TargetSetupError
                | DeviceNotAvailableException
                | RuntimeException
                | AssertionError e) {
            // We catch all the TargetPreparer possible exception + RuntimeException to avoid
            // specific issues + AssertionError since it's widely used in tests and doesn't notify
            // something very wrong with the harness.
            CLog.e("Unexpected Exception from preparer: %s", preparer.getClass().getName());
            CLog.e(e);
            return e;
        } finally {
            moduleInfo.setActiveDeviceIndex(0);
        }
    }

    /** Run all multi target preparer step. */
    private Throwable runMultiPreparerSetup(
            IMultiTargetPreparer preparer, TestInformation moduleInfo, ITestLogger logger) {
        if (preparer.isDisabled()) {
            // If disabled skip completely.
            return null;
        }
        TfObjectTracker.countWithParents(preparer.getClass());
        CLog.d("Running setup multi preparer: %s", preparer.getClass().getSimpleName());
        try {
            // set the logger in case they need it.
            if (preparer instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) preparer).setTestLogger(logger);
            }
            if (preparer instanceof IInvocationContextReceiver) {
                ((IInvocationContextReceiver) preparer)
                        .setInvocationContext(mModuleInvocationContext);
            }
            preparer.setUp(moduleInfo);
            return null;
        } catch (BuildError
                | TargetSetupError
                | DeviceNotAvailableException
                | RuntimeException
                | AssertionError e) {
            // We catch all the MultiTargetPreparer possible exception + RuntimeException to avoid
            // specific issues + AssertionError since it's widely used in tests and doesn't notify
            // something very wrong with the harness.
            CLog.e("Unexpected Exception from preparer: %s", preparer.getClass().getName());
            CLog.e(e);
            return e;
        }
    }

    /** Run all the tear down steps from preparers. */
    private void runTearDown(TestInformation moduleInfo, Throwable exception)
            throws DeviceNotAvailableException {
        // Tear down
        List<IMultiTargetPreparer> cleanerList = new ArrayList<>(mMultiPreparers);
        Collections.reverse(cleanerList);
        for (IMultiTargetPreparer multiCleaner : cleanerList) {
            if (multiCleaner.isDisabled() || multiCleaner.isTearDownDisabled()) {
                // If disabled skip completely.
                continue;
            }
            CLog.d("Running teardown multi cleaner: %s", multiCleaner.getClass().getSimpleName());
            multiCleaner.tearDown(moduleInfo, exception);
        }

        for (int i = 0; i < mModuleInvocationContext.getDeviceConfigNames().size(); i++) {
            String deviceName = mModuleInvocationContext.getDeviceConfigNames().get(i);
            ITestDevice device = mModuleInvocationContext.getDevice(deviceName);
            if (i >= mPreparersPerDevice.size()) {
                CLog.d(
                        "Main configuration has more devices than the module configuration. '%s' "
                                + "will not run any tear down.",
                        deviceName);
                continue;
            }
            List<ITargetPreparer> preparers = mPreparersPerDevice.get(deviceName);
            if (preparers == null) {
                CLog.w(
                        "Module configuration devices mismatch the main configuration "
                                + "(Missing device '%s'), resolving preparers by index.",
                        deviceName);
                String key = new ArrayList<>(mPreparersPerDevice.keySet()).get(i);
                preparers = mPreparersPerDevice.get(key);
            }
            ListIterator<ITargetPreparer> itr = preparers.listIterator(preparers.size());
            while (itr.hasPrevious()) {
                ITargetPreparer preparer = itr.previous();
                // do not call the cleaner if it was disabled
                if (preparer.isDisabled() || preparer.isTearDownDisabled()) {
                    CLog.d("%s has been disabled. skipping.", preparer);
                    continue;
                }

                RecoveryMode origMode = null;
                try {
                    // If an exception was generated in setup with a DNAE do not attempt any
                    // recovery again in case we hit the device not available again.
                    if (exception != null && exception instanceof DeviceNotAvailableException) {
                        origMode = device.getRecoveryMode();
                        device.setRecoveryMode(RecoveryMode.NONE);
                    }
                    moduleInfo.setActiveDeviceIndex(i);
                    preparer.tearDown(moduleInfo, exception);
                } finally {
                    moduleInfo.setActiveDeviceIndex(0);
                    if (origMode != null) {
                        device.setRecoveryMode(origMode);
                    }
                }
            }
        }
    }

    /** Returns the current time. */
    private long getCurrentTime() {
        return System.currentTimeMillis();
    }

    @Override
    public void setCollectTestsOnly(boolean collectTestsOnly) {
        mCollectTestsOnly = collectTestsOnly;
    }

    /** Sets whether or not we should merge results. */
    public final void setMergeAttemps(boolean mergeAttempts) {
        mMergeAttempts = mergeAttempts;
    }

    /** Sets the {@link IRetryDecision} to be used for intra-module retry. */
    public final void setRetryDecision(IRetryDecision decision) {
        mRetryDecision = decision;
        // Carry the retry decision to the module configuration
        mModuleConfiguration.setRetryDecision(decision);
    }

    /** Returns a list of tests that ran in this module. */
    List<TestRunResult> getTestsResults() {
        return mTestsResults;
    }

    /** Returns the number of tests that was expected to be run */
    int getNumExpectedTests() {
        return mExpectedTests;
    }

    /** Returns True if a testRunFailure has been called on the module * */
    public boolean hasModuleFailed() {
        return mIsFailedModule;
    }

    public Set<TokenProperty> getRequiredTokens() {
        return mRequiredTokens;
    }

    /** {@inheritDoc} */
    @Override
    public String toString() {
        return getId();
    }

    /** Returns the approximate time to run all the tests in the module. */
    public long getRuntimeHint() {
        long hint = 0l;
        for (IRemoteTest test : mTests) {
            if (test instanceof IRuntimeHintProvider) {
                hint += ((IRuntimeHintProvider) test).getRuntimeHint();
            } else {
                hint += 60000;
            }
        }
        return hint;
    }

    /** Returns the list of {@link IRemoteTest} defined for this module. */
    @VisibleForTesting
    List<IRemoteTest> getTests() {
        return new ArrayList<>(mTests);
    }

    /** Returns the list of {@link ITargetPreparer} associated with the given device name */
    @VisibleForTesting
    List<ITargetPreparer> getTargetPreparerForDevice(String deviceName) {
        return mPreparersPerDevice.get(deviceName);
    }

    /**
     * When running unit tests for ModuleDefinition we don't want to unnecessarily report some auto
     * retry times.
     */
    @VisibleForTesting
    void disableAutoRetryReportingTime() {
        mDisableAutoRetryTimeReporting = true;
    }

    /** Returns the {@link IInvocationContext} associated with the module. */
    public IInvocationContext getModuleInvocationContext() {
        return mModuleInvocationContext;
    }

    /** Report completely not executed modules. */
    public final void reportNotExecuted(ITestInvocationListener listener, String message) {
        if (mStartModuleRunDate == null) {
            listener.testModuleStarted(getModuleInvocationContext());
        }
        listener.testRunStarted(getId(), 0, 0, System.currentTimeMillis());
        FailureDescription description =
                FailureDescription.create(message).setFailureStatus(FailureStatus.NOT_EXECUTED);
        listener.testRunFailed(description);
        listener.testRunEnded(0, new HashMap<String, Metric>());
        listener.testModuleEnded();
    }

    /** Whether or not to enable dynamic download at module level. */
    public void setEnableDynamicDownload(boolean enableDynamicDownload) {
        mEnableDynamicDownload = enableDynamicDownload;
    }

    public void addDynamicDownloadArgs(Map<String, String> extraArgs) {
        mModuleConfiguration.getCommandOptions().getDynamicDownloadArgs().putAll(extraArgs);
    }

    /**
     * Allow to load a module_controller object to tune how should a particular module run.
     *
     * @param failureListener The {@link TestFailureListener} taking actions on tests failures.
     * @return The strategy to use to run the tests.
     */
    private RunStrategy applyConfigurationControl(TestFailureListener failureListener) {
        Object ctrlObject = mModuleConfiguration.getConfigurationObject(MODULE_CONTROLLER);
        if (ctrlObject != null && ctrlObject instanceof BaseModuleController) {
            BaseModuleController controller = (BaseModuleController) ctrlObject;
            // module_controller can also control the log collection for the one module
            if (failureListener != null) {
                failureListener.applyModuleConfiguration(controller.shouldCaptureBugreport());
            }
            if (!controller.shouldCaptureLogcat()) {
                mRunMetricCollectors.removeIf(c -> (c instanceof LogcatOnFailureCollector));
            }
            if (!controller.shouldCaptureScreenshot()) {
                mRunMetricCollectors.removeIf(c -> (c instanceof ScreenshotOnFailureCollector));
            }
            return controller.shouldRunModule(mModuleInvocationContext);
        }
        return RunStrategy.RUN;
    }

    private void addRetryTime(long retryTimeMs) {
        if (retryTimeMs <= 0 || mDisableAutoRetryTimeReporting) {
            return;
        }
        InvocationMetricLogger.addInvocationMetrics(
                InvocationMetricKey.AUTO_RETRY_TIME, retryTimeMs);
    }

    private Throwable runTargetPreparation(TestInformation moduleInfo, ITestLogger logger) {
        Throwable preparationException = null;
        for (int i = 0; i < mModuleInvocationContext.getDeviceConfigNames().size(); i++) {
            String deviceName = mModuleInvocationContext.getDeviceConfigNames().get(i);
            if (i >= mPreparersPerDevice.size()) {
                CLog.d(
                        "Main configuration has more devices than the module configuration. '%s' "
                                + "will not run any preparation.",
                        deviceName);
                continue;
            }
            List<ITargetPreparer> preparers = mPreparersPerDevice.get(deviceName);
            if (preparers == null) {
                CLog.w(
                        "Module configuration devices mismatch the main configuration "
                                + "(Missing device '%s'), resolving preparers by index.",
                        deviceName);
                String key = new ArrayList<>(mPreparersPerDevice.keySet()).get(i);
                preparers = mPreparersPerDevice.get(key);
            }
            for (ITargetPreparer preparer : preparers) {
                preparationException = runPreparerSetup(moduleInfo, preparer, logger, i);
                if (preparationException != null) {
                    mIsFailedModule = true;
                    CLog.e("Some preparation step failed. failing the module %s", getId());
                    // If one device errored out, we skip the remaining devices.
                    return preparationException;
                }
            }
        }
        return null;
    }

    /**
     * Handle calling the {@link IConfiguration#resolveDynamicOptions(DynamicRemoteFileResolver)}.
     */
    private Exception invokeRemoteDynamic(ITestDevice device, IConfiguration moduleConfiguration) {
        if (!mEnableDynamicDownload) {
            return null;
        }
        // TODO: Add elapsed time tracking
        try {
            CLog.d("Attempting to resolve dynamic files from %s", getId());
            DynamicRemoteFileResolver resolver = new DynamicRemoteFileResolver();
            resolver.setDevice(device);
            resolver.addExtraArgs(moduleConfiguration.getCommandOptions().getDynamicDownloadArgs());
            moduleConfiguration.resolveDynamicOptions(resolver);
            return null;
        } catch (RuntimeException | ConfigurationException | BuildRetrievalError e) {
            mIsFailedModule = true;
            return e;
        }
    }

    /** Report a setup exception as a run failure and notify all the listeners. */
    private void reportSetupFailure(
            Throwable setupException,
            ITestInvocationListener invocListener,
            List<ITestInvocationListener> moduleListeners)
            throws DeviceNotAvailableException {
        List<ITestInvocationListener> allListeners = new ArrayList<>();
        allListeners.add(invocListener);
        if (moduleListeners != null) {
            allListeners.addAll(moduleListeners);
        }
        // Report the early module failures to the moduleListeners too in order for them
        // to know about it.
        ITestInvocationListener forwarder = new ResultForwarder(allListeners);
        // For reporting purpose we create a failure placeholder with the error stack
        // similar to InitializationError of JUnit.
        forwarder.testRunStarted(getId(), 1, 0, System.currentTimeMillis());
        FailureDescription failureDescription =
                CurrentInvocation.createFailure(StreamUtil.getStackTrace(setupException), null);
        if (setupException instanceof IHarnessException
                && ((IHarnessException) setupException).getErrorId() != null) {
            ErrorIdentifier id = ((IHarnessException) setupException).getErrorId();
            failureDescription.setErrorIdentifier(id);
            failureDescription.setFailureStatus(id.status());
            failureDescription.setOrigin(((IHarnessException) setupException).getOrigin());
        } else {
            failureDescription.setFailureStatus(FailureStatus.UNSET);
        }
        failureDescription.setCause(setupException);
        forwarder.testRunFailed(failureDescription);
        HashMap<String, Metric> metricsProto = new HashMap<>();
        metricsProto.put(TEST_TIME, TfMetricProtoUtil.createSingleValue(0L, "milliseconds"));
        forwarder.testRunEnded(0, metricsProto);
        // If it was a not available exception rethrow it to signal the new device state.
        if (setupException instanceof DeviceNotAvailableException) {
            throw (DeviceNotAvailableException) setupException;
        }
    }
}
