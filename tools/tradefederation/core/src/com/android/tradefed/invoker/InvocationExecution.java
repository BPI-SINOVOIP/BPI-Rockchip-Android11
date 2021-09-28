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
package com.android.tradefed.invoker;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildInfo;
import com.android.tradefed.build.BuildInfoKey.BuildInfoFileKey;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.build.IBuildInfo.BuildInfoProperties;
import com.android.tradefed.build.IBuildProvider;
import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.build.IDeviceBuildProvider;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IDeviceConfiguration;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.device.metric.AutoLogCollector;
import com.android.tradefed.device.metric.CollectorHelper;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.device.metric.IMetricCollectorReceiver;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.TestInvocation.Stage;
import com.android.tradefed.invoker.logger.InvocationMetricLogger;
import com.android.tradefed.invoker.logger.InvocationMetricLogger.InvocationMetricKey;
import com.android.tradefed.invoker.logger.TfObjectTracker;
import com.android.tradefed.invoker.shard.IShardHelper;
import com.android.tradefed.invoker.shard.TestsPoolPoller;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.ITestLoggerReceiver;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.retry.RetryLogSaverResultForwarder;
import com.android.tradefed.retry.RetryStatistics;
import com.android.tradefed.retry.RetryStrategy;
import com.android.tradefed.suite.checker.ISystemStatusCheckerReceiver;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.IHostCleaner;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.multi.IMultiTargetPreparer;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.testtype.retry.IAutoRetriableTest;
import com.android.tradefed.testtype.suite.ITestSuite;
import com.android.tradefed.testtype.suite.ModuleListener;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.PrettyPrintDelimiter;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.SystemUtil;
import com.android.tradefed.util.SystemUtil.EnvVariable;
import com.android.tradefed.util.TimeUtil;
import com.android.tradefed.util.executor.ParallelDeviceExecutor;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Strings;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.TimeUnit;

/**
 * Class that describes all the invocation steps: build download, target_prep, run tests, clean up.
 * Can be extended to override the default behavior of some steps. Order of the steps is driven by
 * {@link TestInvocation}.
 */
public class InvocationExecution implements IInvocationExecution {

    public static final String ADB_VERSION_KEY = "adb_version";
    public static final String JAVA_VERSION_KEY = "java_version";
    public static final String JAVA_CLASSPATH_KEY = "java_classpath";
    // Track which preparer ran in setup to ensure we don't trigger tearDown if setup was skipped.
    private Set<IMultiTargetPreparer> mTrackMultiPreparers = null;
    private Map<String, Set<ITargetPreparer>> mTrackTargetPreparers = null;

    @Override
    public boolean fetchBuild(
            TestInformation testInfo,
            IConfiguration config,
            IRescheduler rescheduler,
            ITestInvocationListener listener)
            throws DeviceNotAvailableException, BuildRetrievalError {
        String currentDeviceName = null;
        IBuildInfo buildReplicat = null;
        try {
            // TODO: evaluate fetching build in parallel
            for (int i = 0; i < testInfo.getContext().getDeviceConfigNames().size(); i++) {
                currentDeviceName = testInfo.getContext().getDeviceConfigNames().get(i);
                if (buildReplicat != null) {
                    // TODO: evaluate if cloning the build is needed
                    testInfo.getContext().addDeviceBuildInfo(currentDeviceName, buildReplicat);
                    continue;
                }
                IBuildInfo info = null;
                ITestDevice device = testInfo.getContext().getDevice(currentDeviceName);
                IDeviceConfiguration deviceConfig = config.getDeviceConfigByName(currentDeviceName);
                IBuildProvider provider = deviceConfig.getBuildProvider();
                TfObjectTracker.countWithParents(provider.getClass());
                // Inject the context to the provider if it can receive it
                if (provider instanceof IInvocationContextReceiver) {
                    ((IInvocationContextReceiver) provider)
                            .setInvocationContext(testInfo.getContext());
                }
                // Get the build
                if (provider instanceof IDeviceBuildProvider) {
                    // Download a device build if the provider can handle it.
                    info = ((IDeviceBuildProvider) provider).getBuild(device);
                } else {
                    info = provider.getBuild();
                }
                if (info != null) {
                    info.setDeviceSerial(device.getSerialNumber());
                    testInfo.getContext().addDeviceBuildInfo(currentDeviceName, info);
                    device.setRecovery(deviceConfig.getDeviceRecovery());
                } else {
                    CLog.logAndDisplay(
                            LogLevel.WARN,
                            "No build found to test for device: %s",
                            device.getSerialNumber());
                    IBuildInfo notFoundStub = new BuildInfo();
                    updateBuild(notFoundStub, config);
                    testInfo.getContext().addDeviceBuildInfo(currentDeviceName, notFoundStub);
                    return false;
                }
                // TODO: remove build update when reporting is done on context
                updateBuild(info, config);
                linkExternalDirs(info, testInfo);

                if (config.getCommandOptions().shouldUseReplicateSetup()) {
                    buildReplicat = info;
                }
            }
        } catch (BuildRetrievalError e) {
            CLog.e(e);
            if (currentDeviceName != null) {
                IBuildInfo errorBuild = e.getBuildInfo();
                updateBuild(errorBuild, config);
                testInfo.getContext().addDeviceBuildInfo(currentDeviceName, errorBuild);
            }
            throw e;
        } catch (RuntimeException re) {
            if (currentDeviceName != null) {
                IBuildInfo errorBuild = new BuildInfo();
                updateBuild(errorBuild, config);
                testInfo.getContext().addDeviceBuildInfo(currentDeviceName, errorBuild);
            }
            throw re;
        }
        setBinariesVersion(testInfo.getContext());
        return true;
    }

    @Override
    public void cleanUpBuilds(IInvocationContext context, IConfiguration config) {
        // Ensure build infos are always cleaned up at the end of invocation.
        for (String cleanUpDevice : context.getDeviceConfigNames()) {
            if (context.getBuildInfo(cleanUpDevice) != null) {
                try {
                    config.getDeviceConfigByName(cleanUpDevice)
                            .getBuildProvider()
                            .cleanUp(context.getBuildInfo(cleanUpDevice));
                } catch (RuntimeException e) {
                    // We catch an simply log exception in cleanUp to avoid missing any final
                    // step of the invocation.
                    CLog.e(e);
                }
            }
        }
    }

    @Override
    public boolean shardConfig(
            IConfiguration config,
            TestInformation testInfo,
            IRescheduler rescheduler,
            ITestLogger logger) {
        return createShardHelper().shardConfig(config, testInfo, rescheduler, logger);
    }

    /** Create an return the {@link IShardHelper} to be used. */
    @VisibleForTesting
    protected IShardHelper createShardHelper() {
        return GlobalConfiguration.getInstance().getShardingStrategy();
    }

    @Override
    public void doSetup(TestInformation testInfo, IConfiguration config, final ITestLogger listener)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        long start = System.currentTimeMillis();
        try {
            // Before all the individual setup, make the multi-pre-target-preparer devices setup
            runMultiTargetPreparers(
                    config.getMultiPreTargetPreparers(),
                    listener,
                    testInfo,
                    "multi pre target preparer setup");

            mTrackTargetPreparers = new ConcurrentHashMap<>();
            int index = 0;
            if (config.getCommandOptions().shouldUseReplicateSetup()
                    && config.getDeviceConfig().size() > 1) {
                CLog.d("Using parallel setup due to replicated setup enabled.");
                ParallelDeviceExecutor<Boolean> executor =
                        new ParallelDeviceExecutor<>(testInfo.getContext().getDevices());
                List<Callable<Boolean>> callableTasks = new ArrayList<>();
                for (String deviceName : testInfo.getContext().getDeviceConfigNames()) {
                    mTrackTargetPreparers.put(deviceName, new HashSet<>());
                    final int deviceIndex = index;
                    // Replicate TestInfo
                    TestInformation replicated =
                            TestInformation.createModuleTestInfo(testInfo, testInfo.getContext());
                    Callable<Boolean> callableTask =
                            () -> {
                                runPreparationOnDevice(
                                        replicated, deviceName, deviceIndex, config, listener);
                                return true;
                            };
                    callableTasks.add(callableTask);
                    index++;
                }
                // Run setup with 30 minutes right now.
                executor.invokeAll(callableTasks, 30, TimeUnit.MINUTES);
                if (executor.hasErrors()) {
                    List<Throwable> errors = executor.getErrors();
                    // TODO: Handle throwing multi-exceptions, right now throw the first one.
                    for (Throwable error : errors) {
                        if (error instanceof TargetSetupError) {
                            throw (TargetSetupError) error;
                        }
                        if (error instanceof BuildError) {
                            throw (BuildError) error;
                        }
                        if (error instanceof DeviceNotAvailableException) {
                            throw (DeviceNotAvailableException) error;
                        }
                        throw new RuntimeException(error);
                    }
                }
            } else {
                for (String deviceName : testInfo.getContext().getDeviceConfigNames()) {
                    mTrackTargetPreparers.put(deviceName, new HashSet<>());
                    runPreparationOnDevice(testInfo, deviceName, index, config, listener);
                    index++;
                }
            }
            // After all the individual setup, make the multi-devices setup
            runMultiTargetPreparers(
                    config.getMultiTargetPreparers(),
                    listener,
                    testInfo,
                    "multi target preparer setup");
        } finally {
            // Note: These metrics are handled in a try in case of a kernel reset or device issue.
            // Setup timing metric. It does not include flashing time on boot tests.
            long setupDuration = System.currentTimeMillis() - start;
            InvocationMetricLogger.addInvocationMetrics(InvocationMetricKey.SETUP, setupDuration);
            CLog.d("Setup duration: %s'", TimeUtil.formatElapsedTime(setupDuration));
            // Upload the setup logcat after setup is complete.
            for (String deviceName : testInfo.getContext().getDeviceConfigNames()) {
                reportLogs(testInfo.getContext().getDevice(deviceName), listener, Stage.SETUP);
            }
        }
    }

    private void runPreparationOnDevice(
            TestInformation testInfo,
            String deviceName,
            int index,
            IConfiguration config,
            ITestLogger logger)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getContext().getDevice(deviceName);
        CLog.d("Starting setup for device: '%s'", device.getSerialNumber());
        if (device instanceof ITestLoggerReceiver) {
            ((ITestLoggerReceiver) testInfo.getContext().getDevice(deviceName))
                    .setTestLogger(logger);
        }
        for (ITargetPreparer preparer :
                config.getDeviceConfigByName(deviceName).getTargetPreparers()) {
            // do not call the preparer if it was disabled
            if (preparer.isDisabled()) {
                CLog.d("%s has been disabled. skipping.", preparer);
                continue;
            }
            TfObjectTracker.countWithParents(preparer.getClass());
            if (preparer instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) preparer).setTestLogger(logger);
            }
            CLog.d("starting preparer '%s' on device: '%s'", preparer, device.getSerialNumber());
            try {
                testInfo.setActiveDeviceIndex(index);
                preparer.setUp(testInfo);
            } finally {
                testInfo.setActiveDeviceIndex(0);
            }
            mTrackTargetPreparers.get(deviceName).add(preparer);
            CLog.d("done with preparer '%s' on device: '%s'", preparer, device.getSerialNumber());
        }
        CLog.d("Done with setup of device: '%s'", device.getSerialNumber());
    }

    /** {@inheritDoc} */
    @Override
    public final void runDevicePreInvocationSetup(
            IInvocationContext context, IConfiguration config, ITestLogger logger)
            throws DeviceNotAvailableException, TargetSetupError {
        customizeDevicePreInvocation(config, context);
        for (String deviceName : context.getDeviceConfigNames()) {
            ITestDevice device = context.getDevice(deviceName);

            CLog.d("Starting device pre invocation setup for : '%s'", device.getSerialNumber());
            if (device instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) context.getDevice(deviceName)).setTestLogger(logger);
            }
            device.preInvocationSetup(context.getBuildInfo(deviceName));
        }
    }

    /**
     * Give a chance to customize some of the device before preInvocationSetup.
     *
     * @param config The config of the invocation.
     * @param context The current invocation context.
     */
    protected void customizeDevicePreInvocation(IConfiguration config, IInvocationContext context) {
        // Empty by default
    }

    /** {@inheritDoc} */
    @Override
    public final void runDevicePostInvocationTearDown(
            IInvocationContext context, IConfiguration config, Throwable exception) {
        // Extra tear down step for the device
        for (String deviceName : context.getDeviceConfigNames()) {
            ITestDevice device = context.getDevice(deviceName);
            device.postInvocationTearDown(exception);
        }
    }

    /** Runs the {@link IMultiTargetPreparer} specified. */
    private void runMultiTargetPreparers(
            List<IMultiTargetPreparer> multiPreparers,
            ITestLogger logger,
            TestInformation testInfo,
            String description)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        if (mTrackMultiPreparers == null) {
            mTrackMultiPreparers = new HashSet<>();
        }
        for (IMultiTargetPreparer multiPreparer : multiPreparers) {
            // do not call the preparer if it was disabled
            if (multiPreparer.isDisabled()) {
                CLog.d("%s has been disabled. skipping.", multiPreparer);
                continue;
            }
            TfObjectTracker.countWithParents(multiPreparer.getClass());
            if (multiPreparer instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) multiPreparer).setTestLogger(logger);
            }
            CLog.d("Starting %s '%s'", description, multiPreparer);
            multiPreparer.setUp(testInfo);
            mTrackMultiPreparers.add(multiPreparer);
            CLog.d("done with %s '%s'", description, multiPreparer);
        }
    }

    /** Runs the {@link IMultiTargetPreparer} specified tearDown. */
    private Throwable runMultiTargetPreparersTearDown(
            List<IMultiTargetPreparer> multiPreparers,
            TestInformation testInfo,
            ITestLogger logger,
            Throwable throwable,
            String description)
            throws Throwable {
        ListIterator<IMultiTargetPreparer> iterator =
                multiPreparers.listIterator(multiPreparers.size());
        Throwable deferredThrowable = null;

        while (iterator.hasPrevious()) {
            IMultiTargetPreparer multipreparer = iterator.previous();
            if (multipreparer.isDisabled() || multipreparer.isTearDownDisabled()) {
                CLog.d("%s has been disabled. skipping.", multipreparer);
                continue;
            }
            if (mTrackMultiPreparers == null || !mTrackMultiPreparers.contains(multipreparer)) {
                CLog.d("%s didn't run setUp, skipping tearDown.", multipreparer);
                continue;
            }
            if (multipreparer instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) multipreparer).setTestLogger(logger);
            }
            CLog.d("Starting %s '%s'", description, multipreparer);
            try {
                multipreparer.tearDown(testInfo, throwable);
            } catch (Throwable t) {
                // We catch it and rethrow later to allow each multi_targetprep to be attempted.
                // Only the first one will be thrown but all should be logged.
                CLog.e("Deferring throw for:");
                CLog.e(t);
                if (deferredThrowable == null) {
                    deferredThrowable = t;
                }
            }
            CLog.d("Done with %s '%s'", description, multipreparer);
        }

        return deferredThrowable;
    }

    @Override
    public void doTeardown(
            TestInformation testInfo,
            IConfiguration config,
            ITestLogger logger,
            Throwable exception)
            throws Throwable {
        IInvocationContext context = testInfo.getContext();
        Throwable deferredThrowable = null;

        List<IMultiTargetPreparer> multiPreparers = config.getMultiTargetPreparers();
        deferredThrowable =
                runMultiTargetPreparersTearDown(
                        multiPreparers,
                        testInfo,
                        logger,
                        exception,
                        "multi target preparer teardown");

        // Clear wifi settings, to prevent wifi errors from interfering with teardown process.
        int deviceIndex = 0;
        for (String deviceName : context.getDeviceConfigNames()) {
            ITestDevice device = context.getDevice(deviceName);
            device.clearLastConnectedWifiNetwork();
            List<ITargetPreparer> preparers =
                    config.getDeviceConfigByName(deviceName).getTargetPreparers();
            ListIterator<ITargetPreparer> itr = preparers.listIterator(preparers.size());
            while (itr.hasPrevious()) {
                ITargetPreparer preparer = itr.previous();
                // do not call the cleaner if it was disabled
                if (preparer.isDisabled() || preparer.isTearDownDisabled()) {
                    CLog.d("%s has been disabled. skipping.", preparer);
                    continue;
                }
                if (mTrackTargetPreparers == null
                        || !mTrackTargetPreparers.containsKey(deviceName)
                        || !mTrackTargetPreparers.get(deviceName).contains(preparer)) {
                    CLog.d("%s didn't run setUp, skipping tearDown.", preparer);
                    continue;
                }
                // If setup hit a targetSetupError, the setUp() and setTestLogger might not have
                // run, ensure we still have the logger.
                if (preparer instanceof ITestLoggerReceiver) {
                    ((ITestLoggerReceiver) preparer).setTestLogger(logger);
                }
                try {
                    CLog.d(
                            "starting tearDown '%s' on device: '%s'",
                            preparer, device.getSerialNumber());
                    testInfo.setActiveDeviceIndex(deviceIndex);
                    preparer.tearDown(testInfo, exception);
                    CLog.d(
                            "done with tearDown '%s' on device: '%s'",
                            preparer, device.getSerialNumber());
                } catch (Throwable e) {
                    // We catch it and rethrow later to allow each targetprep to be attempted.
                    // Only the first one will be thrown but all should be logged.
                    CLog.e("Deferring throw for:");
                    CLog.e(e);
                    if (deferredThrowable == null) {
                        deferredThrowable = e;
                    }
                } finally {
                    testInfo.setActiveDeviceIndex(0);
                }
            }
            deviceIndex++;
        }

        // Extra tear down step for the device
        runDevicePostInvocationTearDown(context, config, exception);

        // After all, run the multi_pre_target_preparer tearDown.
        List<IMultiTargetPreparer> multiPrePreparers = config.getMultiPreTargetPreparers();
        Throwable preTargetTearDownException =
                runMultiTargetPreparersTearDown(
                        multiPrePreparers,
                        testInfo,
                        logger,
                        exception,
                        "multi pre target preparer teardown");
        if (deferredThrowable == null) {
            deferredThrowable = preTargetTearDownException;
        }

        // Collect adb logs.
        logHostAdb(logger);

        if (deferredThrowable != null) {
            throw deferredThrowable;
        }
    }

    @Override
    public void doCleanUp(IInvocationContext context, IConfiguration config, Throwable exception) {
        for (String deviceName : context.getDeviceConfigNames()) {
            List<ITargetPreparer> preparers =
                    config.getDeviceConfigByName(deviceName).getTargetPreparers();
            ListIterator<ITargetPreparer> itr = preparers.listIterator(preparers.size());
            while (itr.hasPrevious()) {
                ITargetPreparer preparer = itr.previous();
                if (preparer instanceof IHostCleaner) {
                    IHostCleaner cleaner = (IHostCleaner) preparer;
                    if (preparer.isDisabled() || preparer.isTearDownDisabled()) {
                        CLog.d("%s has been disabled. skipping.", cleaner);
                        continue;
                    }
                    cleaner.cleanUp(context.getBuildInfo(deviceName), exception);
                }
            }
        }
    }

    @Override
    public void runTests(
            TestInformation info, IConfiguration config, ITestInvocationListener listener)
            throws Throwable {
        List<IRemoteTest> remainingTests = new ArrayList<>(config.getTests());
        UnexecutedTestReporterThread reporterThread =
                new UnexecutedTestReporterThread(listener, remainingTests);
        Runtime.getRuntime().addShutdownHook(reporterThread);
        TestInvocation.printStageDelimiter(Stage.TEST, false);
        try {
            for (IRemoteTest test : config.getTests()) {
                TfObjectTracker.countWithParents(test.getClass());
                // For compatibility of those receivers, they are assumed to be single device alloc.
                if (test instanceof IDeviceTest) {
                    ((IDeviceTest) test).setDevice(info.getDevice());
                }
                if (test instanceof IBuildReceiver) {
                    ((IBuildReceiver) test).setBuild(info.getBuildInfo());
                }
                if (test instanceof ISystemStatusCheckerReceiver) {
                    ((ISystemStatusCheckerReceiver) test)
                            .setSystemStatusChecker(config.getSystemStatusCheckers());
                }
                if (test instanceof IInvocationContextReceiver) {
                    ((IInvocationContextReceiver) test).setInvocationContext(info.getContext());
                }

                updateAutoCollectors(config);

                IRetryDecision decision = config.getRetryDecision();
                // Handle the no-retry use case
                if (!decision.isAutoRetryEnabled()
                        || RetryStrategy.NO_RETRY.equals(decision.getRetryStrategy())
                        || test instanceof ITestSuite
                        // TODO: Handle auto-retry in local-sharding for non-suite
                        || test instanceof TestsPoolPoller
                        // If test doesn't support auto-retry
                        || (!(test instanceof ITestFilterReceiver)
                                && !(test instanceof IAutoRetriableTest))) {
                    runTest(config, info, listener, test);
                    remainingTests.remove(test);
                    continue;
                }
                CLog.d("Using RetryLogSaverResultForwarder to forward results.");
                ModuleListener mainGranularRunListener = new ModuleListener(null);
                RetryLogSaverResultForwarder runListener =
                        initializeListeners(config, listener, mainGranularRunListener);
                runTest(config, info, runListener, test);
                remainingTests.remove(test);
                runListener.incrementAttempt();

                // Avoid entering the loop if no retry to be done.
                if (!decision.shouldRetry(
                        test, 0, mainGranularRunListener.getTestRunForAttempts(0))) {
                    continue;
                }

                long startTime = System.currentTimeMillis();
                try {
                    PrettyPrintDelimiter.printStageDelimiter("Starting auto-retry");
                    for (int attemptNumber = 1;
                            attemptNumber < decision.getMaxRetryCount();
                            attemptNumber++) {
                        boolean retry =
                                decision.shouldRetry(
                                        test,
                                        attemptNumber - 1,
                                        mainGranularRunListener.getTestRunForAttempts(
                                                attemptNumber - 1));
                        if (!retry) {
                            continue;
                        }
                        CLog.d("auto-retry attempt number '%s'", attemptNumber);
                        // Run the tests again
                        runTest(config, info, runListener, test);
                        runListener.incrementAttempt();
                    }
                    // Feed the last attempt if we reached here.
                    decision.addLastAttempt(
                            mainGranularRunListener.getTestRunForAttempts(
                                    decision.getMaxRetryCount() - 1));
                } finally {
                    RetryStatistics retryStats = decision.getRetryStatistics();
                    // Track how long we spend in retry
                    retryStats.mRetryTime = System.currentTimeMillis() - startTime;
                    addRetryTime(retryStats.mRetryTime);
                }
            }
        } finally {
            TestInvocation.printStageDelimiter(Stage.TEST, true);
            // TODO: Look if this can be improved to DeviceNotAvailableException too.
            Runtime.getRuntime().removeShutdownHook(reporterThread);
        }

    }

    @Override
    public void reportLogs(ITestDevice device, ITestLogger listener, Stage stage) {
        if (device == null) {
            return;
        }
        IDevice idevice = device.getIDevice();
        // non stub device
        if (!(idevice instanceof StubDevice)) {
            try (InputStreamSource logcatSource = device.getLogcat()) {
                device.clearLogcat();
                String name =
                        String.format(
                                "%s_%s",
                                TestInvocation.getDeviceLogName(stage), device.getSerialNumber());
                listener.testLog(name, LogDataType.LOGCAT, logcatSource);
            }
        }
        // emulator logs
        if (idevice != null && idevice.isEmulator()) {
            try (InputStreamSource emulatorOutput = device.getEmulatorOutput()) {
                // TODO: Clear the emulator log
                String name = TestInvocation.getEmulatorLogName(stage);
                listener.testLog(name, LogDataType.TEXT, emulatorOutput);
            }
        }
    }

    /** Helper to create the test tag from the configuration. */
    private String getTestTag(IConfiguration config) {
        String testTag = config.getCommandOptions().getTestTag();
        if (config.getCommandOptions().getTestTagSuffix() != null) {
            testTag =
                    String.format("%s-%s", testTag, config.getCommandOptions().getTestTagSuffix());
        }
        return testTag;
    }

    /** Handle setting the test tag on the build info. */
    protected void setTestTag(IBuildInfo info, IConfiguration config) {
        // When CommandOption is set, it overrides any test-tag from build_providers
        if (!"stub".equals(config.getCommandOptions().getTestTag())) {
            info.setTestTag(getTestTag(config));
        } else if (Strings.isNullOrEmpty(info.getTestTag())) {
            // We ensure that that a default test-tag is always available.
            info.setTestTag("stub");
        }
    }

    /**
     * Update the {@link IBuildInfo} with additional info from the {@link IConfiguration}.
     *
     * @param info the {@link IBuildInfo}
     * @param config the {@link IConfiguration}
     */
    void updateBuild(IBuildInfo info, IConfiguration config) {
        if (config.getCommandLine() != null) {
            // TODO: obfuscate the password if any.
            info.addBuildAttribute(TestInvocation.COMMAND_ARGS_KEY, config.getCommandLine());
        }
        if (config.getCommandOptions().getShardCount() != null) {
            info.addBuildAttribute(
                    "shard_count", config.getCommandOptions().getShardCount().toString());
        }
        if (config.getCommandOptions().getShardIndex() != null) {
            info.addBuildAttribute(
                    "shard_index", config.getCommandOptions().getShardIndex().toString());
        }
        setTestTag(info, config);
    }

    private void runTest(
            IConfiguration config,
            TestInformation info,
            ITestInvocationListener listener,
            IRemoteTest test)
            throws DeviceNotAvailableException {
        // We clone the collectors for each IRemoteTest to ensure no state conflicts.
        List<IMetricCollector> clonedCollectors = new ArrayList<>();
        // Add automated collectors
        for (AutoLogCollector auto : config.getCommandOptions().getAutoLogCollectors()) {
            clonedCollectors.add(auto.getInstanceForValue());
        }
        // Add the collector from the configuration
        clonedCollectors.addAll(CollectorHelper.cloneCollectors(config.getMetricCollectors()));
        if (test instanceof IMetricCollectorReceiver) {
            ((IMetricCollectorReceiver) test).setMetricCollectors(clonedCollectors);
            // If test can receive collectors then let it handle the how to set them up
            test.run(info, listener);
        } else {
            // Wrap collectors in each other and collection will be sequential, do this in the
            // loop to ensure they are always initialized against the right context.
            ITestInvocationListener listenerWithCollectors = listener;
            for (IMetricCollector collector : clonedCollectors) {
                if (collector.isDisabled()) {
                    CLog.d("%s has been disabled. Skipping.", collector);
                } else {
                    listenerWithCollectors =
                            collector.init(info.getContext(), listenerWithCollectors);
                    TfObjectTracker.countWithParents(collector.getClass());
                }
            }
            test.run(info, listenerWithCollectors);
        }
    }

    private RetryLogSaverResultForwarder initializeListeners(
            IConfiguration config,
            ITestInvocationListener mainListener,
            ITestInvocationListener mainGranularLevelListener) {
        List<ITestInvocationListener> currentTestListeners = new ArrayList<>();
        currentTestListeners.add(mainGranularLevelListener);
        currentTestListeners.add(mainListener);
        return new RetryLogSaverResultForwarder(config.getLogSaver(), currentTestListeners) {
            @Override
            public void testLog(
                    String dataName, LogDataType dataType, InputStreamSource dataStream) {
                // We know for sure that the sub-listeners are LogSaverResultForwarder
                // so we delegate to them to save and generate the logAssociation.
                testLogForward(dataName, dataType, dataStream);
            }
        };
    }

    private void addRetryTime(long retryTimeMs) {
        // InvocationMetricLogger automatically adds the auto retry time.
        InvocationMetricLogger.addInvocationMetrics(
                InvocationMetricKey.AUTO_RETRY_TIME, retryTimeMs);
    }

    private void linkExternalDirs(IBuildInfo info, TestInformation testInfo) {
        if (info.getProperties().contains(BuildInfoProperties.DO_NOT_LINK_TESTS_DIR)) {
            CLog.d("Skip linking external directory as FileProperty was set.");
            return;
        }
        // Load environment tests dir.
        if (info instanceof IDeviceBuildInfo) {
            // TODO: Use tests directory from TestInformation instead.
            File testsDir = ((IDeviceBuildInfo) info).getTestsDir();
            if (testsDir != null && testsDir.exists()) {
                if (testInfo.executionFiles().get(FilesKey.TARGET_TESTS_DIRECTORY) == null) {
                    File targetTestCases =
                            handleLinkingExternalDirs(
                                    (IDeviceBuildInfo) info,
                                    testsDir,
                                    EnvVariable.ANDROID_TARGET_OUT_TESTCASES,
                                    BuildInfoFileKey.TARGET_LINKED_DIR.getFileKey());
                    if (targetTestCases != null) {
                        testInfo.executionFiles()
                                .put(FilesKey.TARGET_TESTS_DIRECTORY, targetTestCases, true);
                    }
                }
                if (testInfo.executionFiles().get(FilesKey.HOST_TESTS_DIRECTORY) == null) {
                    File hostTestCases =
                            handleLinkingExternalDirs(
                                    (IDeviceBuildInfo) info,
                                    testsDir,
                                    EnvVariable.ANDROID_HOST_OUT_TESTCASES,
                                    BuildInfoFileKey.HOST_LINKED_DIR.getFileKey());
                    if (hostTestCases != null) {
                        testInfo.executionFiles()
                                .put(FilesKey.HOST_TESTS_DIRECTORY, hostTestCases, true);
                    }
                }
            }
        }
    }

    private File handleLinkingExternalDirs(
            IDeviceBuildInfo info, File testsDir, EnvVariable var, String baseName) {
        File externalDir = getExternalTestCasesDirs(var);
        if (externalDir == null) {
            String path = SystemUtil.ENV_VARIABLE_PATHS_IN_TESTS_DIR.get(var);
            File varDir = FileUtil.getFileForPath(testsDir, path);
            if (varDir.exists()) {
                // If we found a dir already in the tests dir we keep track of it
                info.setFile(
                        baseName,
                        varDir,
                        /** version */
                        "v1");
                return varDir;
            }
            return null;
        }
        try {
            // Avoid conflict by creating a randomized name for the arriving symlink file.
            File subDir = FileUtil.createTempDir(baseName, testsDir);
            subDir.delete();
            FileUtil.symlinkFile(externalDir, subDir);
            // Tag the dir in the build info to be possibly cleaned.
            info.setFile(
                    baseName,
                    subDir,
                    /** version */
                    "v1");
            // Ensure we always delete the linking, no matter how the JVM exits.
            subDir.deleteOnExit();
            return subDir;
        } catch (IOException e) {
            CLog.e("Failed to load external test dir %s. Ignoring it.", externalDir);
            CLog.e(e);
        }
        return null;
    }

    private void setBinariesVersion(IInvocationContext context) {
        String version = getAdbVersion();
        if (version != null) {
            context.addInvocationAttribute(ADB_VERSION_KEY, version);
        }
        String javaVersion = System.getProperty("java.version");
        if (javaVersion != null && !javaVersion.isEmpty()) {
            context.addInvocationAttribute(JAVA_VERSION_KEY, javaVersion);
        }
        String javaClasspath = System.getProperty("java.class.path");
        if (javaClasspath != null && !javaClasspath.isEmpty()) {
            context.addInvocationAttribute(JAVA_CLASSPATH_KEY, javaClasspath);
        }
    }

    /** Convert the legacy *-on-failure options to the new auto-collect. */
    private void updateAutoCollectors(IConfiguration config) {
        if (config.getCommandOptions().captureScreenshotOnFailure()) {
            config.getCommandOptions()
                    .getAutoLogCollectors()
                    .add(AutoLogCollector.SCREENSHOT_ON_FAILURE);
        }
        if (config.getCommandOptions().captureLogcatOnFailure()) {
            config.getCommandOptions()
                    .getAutoLogCollectors()
                    .add(AutoLogCollector.LOGCAT_ON_FAILURE);
        }
    }

    /** Collect the logs from $TMPDIR/adb.$UID.log. */
    @VisibleForTesting
    void logHostAdb(ITestLogger logger) {
        String tmpDir = "/tmp";
        if (System.getenv("TMPDIR") != null) {
            tmpDir = System.getenv("TMPDIR");
        }
        CommandResult uidRes =
                RunUtil.getDefault()
                        .runTimedCmd(60000, "id", "-u", System.getProperty("user.name"));
        if (!CommandStatus.SUCCESS.equals(uidRes.getStatus())) {
            CLog.e("Failed to collect UID for adb logs: %s", uidRes.getStderr());
            return;
        }
        String uid = uidRes.getStdout().trim();
        File adbLog = new File(tmpDir, String.format("adb.%s.log", uid));
        if (!adbLog.exists()) {
            CLog.i("Did not find adb log file: %s, upload skipped.", adbLog);
            return;
        }
        CommandResult truncAdb =
                RunUtil.getDefault()
                        .runTimedCmd(60000, "tail", "--bytes=10MB", adbLog.getAbsolutePath());
        if (!CommandStatus.SUCCESS.equals(truncAdb.getStatus())) {
            CLog.e("Fail to truncate the adb log: %s\n%s", adbLog, truncAdb.getStderr());
            return;
        }
        try (InputStreamSource source =
                new ByteArrayInputStreamSource(truncAdb.getStdout().getBytes())) {
            logger.testLog("host_adb_log", LogDataType.TEXT, source);
        }
    }

    /** Returns the external directory coming from the environment. */
    @VisibleForTesting
    File getExternalTestCasesDirs(EnvVariable envVar) {
        return SystemUtil.getExternalTestCasesDir(envVar);
    }

    /** Returns the adb version in use for the invocation. */
    protected String getAdbVersion() {
        return GlobalConfiguration.getDeviceManagerInstance().getAdbVersion();
    }
}
