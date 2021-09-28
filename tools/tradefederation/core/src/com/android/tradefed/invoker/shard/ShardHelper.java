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
package com.android.tradefed.invoker.shard;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationDescriptor;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IGlobalConfiguration;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.IRescheduler;
import com.android.tradefed.invoker.ShardListener;
import com.android.tradefed.invoker.ShardMainResultForwarder;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.shard.token.ITokenRequest;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.IShardableListener;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.ITestLoggerReceiver;
import com.android.tradefed.retry.IRetryDecision;
import com.android.tradefed.suite.checker.ISystemStatusChecker;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IShardableTest;
import com.android.tradefed.util.keystore.IKeyStoreClient;
import com.android.tradefed.util.keystore.KeyStoreException;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.CountDownLatch;

/** Helper class that handles creating the shards and scheduling them for an invocation. */
public class ShardHelper implements IShardHelper {

    public static final String LAST_SHARD_DETECTOR = "last_shard_detector";
    public static final String SHARED_TEST_INFORMATION = "shared_test_information";

    /**
     * List of the list configuration obj that should be clone to each shard in order to avoid state
     * issues.
     */
    private static final List<String> CONFIG_OBJ_TO_CLONE = new ArrayList<>();

    static {
        CONFIG_OBJ_TO_CLONE.add(Configuration.SYSTEM_STATUS_CHECKER_TYPE_NAME);
        CONFIG_OBJ_TO_CLONE.add(Configuration.DEVICE_METRICS_COLLECTOR_TYPE_NAME);
        // Copy all the objects under the <device> tag from
        // {@link Configuration#getMultiDeviceSupportedTag()} except DEVICE_REQUIREMENTS_TYPE_NAME
        // which should be shared since all shards should have the same requirements.
        CONFIG_OBJ_TO_CLONE.add(Configuration.BUILD_PROVIDER_TYPE_NAME);
        CONFIG_OBJ_TO_CLONE.add(Configuration.TARGET_PREPARER_TYPE_NAME);
        CONFIG_OBJ_TO_CLONE.add(Configuration.DEVICE_RECOVERY_TYPE_NAME);
        CONFIG_OBJ_TO_CLONE.add(Configuration.DEVICE_OPTIONS_TYPE_NAME);

        CONFIG_OBJ_TO_CLONE.add(Configuration.MULTI_PREPARER_TYPE_NAME);
        CONFIG_OBJ_TO_CLONE.add(Configuration.CMD_OPTIONS_TYPE_NAME);
        CONFIG_OBJ_TO_CLONE.add(Configuration.LOGGER_TYPE_NAME);
        // Deep clone of log_saver to ensure each shard manages its own logs
        CONFIG_OBJ_TO_CLONE.add(Configuration.LOG_SAVER_TYPE_NAME);
        // Deep clone RetryDecision to ensure each shard retry independently
        CONFIG_OBJ_TO_CLONE.add(Configuration.RETRY_DECISION_TYPE_NAME);
    }

    /**
     * Attempt to shard the configuration into sub-configurations, to be re-scheduled to run on
     * multiple resources in parallel.
     *
     * <p>A successful shard action renders the current config empty, and invocation should not
     * proceed.
     *
     * @see IShardableTest
     * @see IRescheduler
     * @param config the current {@link IConfiguration}.
     * @param testInfo the {@link TestInformation} holding the tests information.
     * @param rescheduler the {@link IRescheduler}
     * @return true if test was sharded. Otherwise return <code>false</code>
     */
    @Override
    public boolean shardConfig(
            IConfiguration config,
            TestInformation testInfo,
            IRescheduler rescheduler,
            ITestLogger logger) {
        IInvocationContext context = testInfo.getContext();
        List<IRemoteTest> shardableTests = new ArrayList<IRemoteTest>();
        boolean isSharded = false;
        Integer shardCount = config.getCommandOptions().getShardCount();
        for (IRemoteTest test : config.getTests()) {
            isSharded |= shardTest(shardableTests, test, shardCount, testInfo, logger);
        }
        if (!isSharded) {
            return false;
        }
        // shard this invocation!
        // create the TestInvocationListener that will collect results from all the shards,
        // and forward them to the original set of listeners (minus any ISharddableListeners)
        // once all shards complete
        int expectedShard = shardableTests.size();
        if (shardCount != null) {
            expectedShard = Math.min(shardCount, shardableTests.size());
        }
        // Add a tracker so we know in invocation if the last shard is done running.
        LastShardDetector lastShard = new LastShardDetector();
        ShardMainResultForwarder resultCollector =
                new ShardMainResultForwarder(
                        buildMainShardListeners(config, lastShard), expectedShard);

        config.getLogSaver().invocationStarted(context);
        resultCollector.invocationStarted(context);
        synchronized (shardableTests) {
            // When shardCount is available only create 1 poller per shard
            // TODO: consider aggregating both case by picking a predefined shardCount if not
            // available (like 4) for autosharding.
            if (shardCount != null) {
                // We shuffle the tests for best results: avoid having the same module sub-tests
                // contiguously in the list.
                Collections.shuffle(shardableTests);
                int maxShard = Math.min(shardCount, shardableTests.size());
                CountDownLatch tracker = new CountDownLatch(maxShard);
                Collection<ITokenRequest> tokenPool = null;
                if (config.getCommandOptions().shouldUseTokenSharding()) {
                    tokenPool = extractTokenTests(shardableTests);
                }
                for (int i = 0; i < maxShard; i++) {
                    IConfiguration shardConfig = cloneConfigObject(config);
                    try {
                        shardConfig.setConfigurationObject(LAST_SHARD_DETECTOR, lastShard);
                    } catch (ConfigurationException e) {
                        throw new RuntimeException(e);
                    }
                    TestsPoolPoller poller =
                            new TestsPoolPoller(shardableTests, tokenPool, tracker);
                    shardConfig.setTest(poller);
                    rescheduleConfig(
                            shardConfig, config, testInfo, rescheduler, resultCollector, i);
                }
            } else {
                CountDownLatch tracker = new CountDownLatch(shardableTests.size());
                Collection<ITokenRequest> tokenPool = null;
                if (config.getCommandOptions().shouldUseTokenSharding()) {
                    tokenPool = extractTokenTests(shardableTests);
                }
                int i = 0;
                for (IRemoteTest testShard : shardableTests) {
                    CLog.d("Rescheduling sharded config...");
                    IConfiguration shardConfig = cloneConfigObject(config);
                    try {
                        shardConfig.setConfigurationObject(LAST_SHARD_DETECTOR, lastShard);
                    } catch (ConfigurationException e) {
                        throw new RuntimeException(e);
                    }
                    if (config.getCommandOptions().shouldUseDynamicSharding()) {
                        TestsPoolPoller poller =
                                new TestsPoolPoller(shardableTests, tokenPool, tracker);
                        shardConfig.setTest(poller);
                    } else {
                        shardConfig.setTest(testShard);
                    }
                    rescheduleConfig(
                            shardConfig, config, testInfo, rescheduler, resultCollector, i);
                    i++;
                }
            }
        }
        // clean up original builds
        for (String deviceName : context.getDeviceConfigNames()) {
            config.getDeviceConfigByName(deviceName)
                    .getBuildProvider()
                    .cleanUp(context.getBuildInfo(deviceName));
        }
        return true;
    }

    private void rescheduleConfig(
            IConfiguration shardConfig,
            IConfiguration config,
            TestInformation testInfo,
            IRescheduler rescheduler,
            ShardMainResultForwarder resultCollector,
            int index) {
        validateOptions(testInfo, shardConfig);
        ShardBuildCloner.cloneBuildInfos(config, shardConfig, testInfo);

        shardConfig.setTestInvocationListeners(
                buildShardListeners(resultCollector, config, config.getTestInvocationListeners()));

        // Set the host_log suffix to avoid similar names
        String suffix = String.format("_shard_index_%s", index);
        if (shardConfig.getCommandOptions().getHostLogSuffix() != null) {
            suffix = shardConfig.getCommandOptions().getHostLogSuffix() + suffix;
        }
        shardConfig.getCommandOptions().setHostLogSuffix(suffix);

        // Use the same {@link ITargetPreparer}, {@link IDeviceRecovery} etc as original config
        // Make sure we don't run as sandboxed in shards, only parent invocation needs to
        // run as sandboxed
        shardConfig.getConfigurationDescription().setSandboxed(false);
        rescheduler.scheduleConfig(shardConfig);
    }

    /** Returns the current global configuration. */
    @VisibleForTesting
    protected IGlobalConfiguration getGlobalConfiguration() {
        return GlobalConfiguration.getInstance();
    }

    /** Runs the {@link IConfiguration#validateOptions()} on the config. */
    @VisibleForTesting
    protected void validateOptions(TestInformation testInfo, IConfiguration config) {
        try {
            config.validateOptions();
            DynamicRemoteFileResolver resolver = new DynamicRemoteFileResolver();
            resolver.setDevice(testInfo.getDevice());
            resolver.addExtraArgs(config.getCommandOptions().getDynamicDownloadArgs());
            config.resolveDynamicOptions(resolver);
        } catch (ConfigurationException | BuildRetrievalError e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Helper to clone {@link ISystemStatusChecker}s from the original config to the clonedConfig.
     */
    private IConfiguration cloneConfigObject(IConfiguration origConfig) {
        IKeyStoreClient client = null;
        try {
            client = getGlobalConfiguration().getKeyStoreFactory().createKeyStoreClient();
        } catch (KeyStoreException e) {
            throw new RuntimeException(
                    String.format(
                            "failed to load keystore client when sharding: %s", e.getMessage()),
                    e);
        }

        try {
            IConfiguration deepCopy = origConfig.partialDeepClone(CONFIG_OBJ_TO_CLONE, client);
            // Sharding was done, no need for children to look into it.
            deepCopy.getCommandOptions().setShardCount(null);
            deepCopy.getConfigurationDescription()
                    .addMetadata(ConfigurationDescriptor.LOCAL_SHARDED_KEY, "true");
            return deepCopy;
        } catch (ConfigurationException e) {
            throw new RuntimeException(
                    String.format("failed to deep copy a configuration: %s", e.getMessage()), e);
        }
    }

    /**
     * Attempt to shard given {@link IRemoteTest}.
     *
     * @param shardableTests the list of {@link IRemoteTest}s to add to
     * @param test the {@link IRemoteTest} to shard
     * @param shardCount attempted number of shard, can be null.
     * @param testInfo the {@link TestInformation} of the current invocation.
     * @return <code>true</code> if test was sharded
     */
    private static boolean shardTest(
            List<IRemoteTest> shardableTests,
            IRemoteTest test,
            Integer shardCount,
            TestInformation testInfo,
            ITestLogger logger) {
        boolean isSharded = false;
        if (test instanceof IShardableTest) {
            // inject device and build since they might be required to shard.
            if (test instanceof IBuildReceiver) {
                ((IBuildReceiver) test).setBuild(testInfo.getBuildInfo());
            }
            if (test instanceof IDeviceTest) {
                ((IDeviceTest) test).setDevice(testInfo.getDevice());
            }
            if (test instanceof IInvocationContextReceiver) {
                ((IInvocationContextReceiver) test).setInvocationContext(testInfo.getContext());
            }
            if (test instanceof ITestLoggerReceiver) {
                ((ITestLoggerReceiver) test).setTestLogger(logger);
            }

            IShardableTest shardableTest = (IShardableTest) test;
            Collection<IRemoteTest> shards = null;
            // Give the shardCount hint to tests if they need it.
            shards = shardableTest.split(shardCount, testInfo);
            if (shards != null) {
                shardableTests.addAll(shards);
                isSharded = true;
            }
        }
        if (!isSharded) {
            shardableTests.add(test);
        }
        return isSharded;
    }

    /**
     * Builds the {@link ITestInvocationListener} listeners that will collect the results from all
     * shards. Currently excludes {@link IShardableListener}s.
     */
    private static List<ITestInvocationListener> buildMainShardListeners(
            IConfiguration config, LastShardDetector lastShardDetector) {
        List<ITestInvocationListener> newListeners = new ArrayList<ITestInvocationListener>();
        for (ITestInvocationListener l : config.getTestInvocationListeners()) {
            if (!(l instanceof IShardableListener)) {
                newListeners.add(l);
            }
        }
        newListeners.add(lastShardDetector);
        return newListeners;
    }

    /**
     * Builds the list of {@link ITestInvocationListener}s for each shard. Currently includes any
     * {@link IShardableListener}, plus a single listener that will forward results to the main
     * shard collector.
     */
    private static List<ITestInvocationListener> buildShardListeners(
            ITestInvocationListener resultCollector,
            IConfiguration config,
            List<ITestInvocationListener> origListeners) {
        List<ITestInvocationListener> shardListeners = new ArrayList<ITestInvocationListener>();
        for (ITestInvocationListener l : origListeners) {
            if (l instanceof IShardableListener) {
                shardListeners.add(((IShardableListener) l).clone());
            }
        }
        ShardListener origConfigListener = new ShardListener(resultCollector);
        origConfigListener.setSupportGranularResults(isAutoRetryEnabled(config));
        shardListeners.add(origConfigListener);
        return shardListeners;
    }

    private static boolean isAutoRetryEnabled(IConfiguration config) {
        IRetryDecision decision = config.getRetryDecision();
        if (decision.isAutoRetryEnabled() && decision.getMaxRetryCount() > 0) {
            return true;
        }
        return false;
    }

    private Collection<ITokenRequest> extractTokenTests(Collection<IRemoteTest> shardableTests) {
        List<ITokenRequest> tokenPool = new ArrayList<>();
        Iterator<IRemoteTest> itr = new ArrayList<>(shardableTests).iterator();

        while (itr.hasNext()) {
            IRemoteTest test = itr.next();
            if (test instanceof ITokenRequest) {
                tokenPool.add((ITokenRequest) test);
                shardableTests.remove(test);
            }
        }
        return tokenPool;
    }
}
