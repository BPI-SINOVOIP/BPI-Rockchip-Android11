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
import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.config.Configuration;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.DynamicRemoteFileResolver;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.cloud.NestedRemoteDevice;
import com.android.tradefed.device.metric.IMetricCollector;
import com.android.tradefed.device.metric.IMetricCollectorReceiver;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.invoker.shard.token.ITokenProvider;
import com.android.tradefed.invoker.shard.token.ITokenRequest;
import com.android.tradefed.invoker.shard.token.TokenProperty;
import com.android.tradefed.invoker.shard.token.TokenProviderHelper;
import com.android.tradefed.log.ILogRegistry;
import com.android.tradefed.log.ILogRegistry.EventType;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogRegistry;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.suite.checker.ISystemStatusChecker;
import com.android.tradefed.suite.checker.ISystemStatusCheckerReceiver;
import com.android.tradefed.testtype.IBuildReceiver;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IInvocationContextReceiver;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.IReportNotExecuted;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.TimeUtil;

import com.google.common.collect.Sets;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;

/**
 * Tests wrapper that allow to execute all the tests of a pool of tests. Tests can be shared by
 * another {@link TestsPoolPoller} so synchronization is required.
 *
 * <p>TODO: Add handling for token module/tests.
 */
public final class TestsPoolPoller
        implements IRemoteTest,
                IConfigurationReceiver,
                ISystemStatusCheckerReceiver,
                IMetricCollectorReceiver {

    private static final long WAIT_RECOVERY_TIME = 15 * 60 * 1000;

    private Collection<IRemoteTest> mGenericPool;
    private Collection<ITokenRequest> mTokenPool;
    private CountDownLatch mTracker;
    private Set<ITokenRequest> mRejectedToken;

    private TestInformation mTestInfo;
    private IConfiguration mConfig;
    private List<ISystemStatusChecker> mSystemStatusCheckers;
    private List<IMetricCollector> mCollectors;

    private ILogRegistry mRegistry = null;

    /**
     * Ctor where the pool of {@link IRemoteTest} is provided.
     *
     * @param tests {@link IRemoteTest}s pool of all tests.
     * @param tracker a {@link CountDownLatch} shared to get the number of running poller.
     */
    public TestsPoolPoller(Collection<IRemoteTest> tests, CountDownLatch tracker) {
        mGenericPool = tests;
        mTracker = tracker;
    }

    public TestsPoolPoller(
            Collection<IRemoteTest> tests,
            Collection<ITokenRequest> tokenTests,
            CountDownLatch tracker) {
        this(tests, tracker);
        mTokenPool = tokenTests;
        mRejectedToken = Sets.newConcurrentHashSet();
    }

    /** Returns the first {@link IRemoteTest} from the pool or null if none remaining. */
    IRemoteTest poll() {
        return poll(false);
    }

    /** Returns the first {@link IRemoteTest} from the pool or null if none remaining. */
    private IRemoteTest poll(boolean reportNotExecuted) {
        if (mTokenPool != null) {
            synchronized (mTokenPool) {
                if (!mTokenPool.isEmpty()) {
                    Iterator<ITokenRequest> itr = mTokenPool.iterator();
                    while (itr.hasNext()) {
                        ITokenRequest test = itr.next();
                        if (reportNotExecuted) {
                            // Return to report not executed tests, regardless of if they can
                            // actually execute or not.
                            mRejectedToken.remove(test);
                            mTokenPool.remove(test);
                            return test;
                        }
                        if (mRejectedToken.contains(test)) {
                            // If the poller already rejected the tests once, do not re-evaluate.
                            continue;
                        }
                        Set<TokenProperty> tokens = test.getRequiredTokens();
                        if (tokens == null || tokens.isEmpty() || isSupported(tokens)) {
                            // No Token can run anywhere, or supported can run
                            mTokenPool.remove(test);
                            mRejectedToken.remove(test);
                            return test;
                        }

                        // Track as rejected
                        mRejectedToken.add(test);
                    }
                }
            }
        }
        synchronized (mGenericPool) {
            if (mGenericPool.isEmpty()) {
                return null;
            }
            IRemoteTest test = mGenericPool.iterator().next();
            mGenericPool.remove(test);
            return test;
        }
    }

    private ITokenRequest pollRejectedTokenModule() {
        if (mTokenPool == null) {
            return null;
        }
        synchronized (mTokenPool) {
            if (mRejectedToken.isEmpty()) {
                return null;
            }
            ITokenRequest test = mRejectedToken.iterator().next();
            mRejectedToken.remove(test);
            mTokenPool.remove(test);
            return test;
        }
    }

    /** {@inheritDoc} */
    @Override
    public void run(TestInformation info, ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        mTestInfo = info;
        try {
            ITestInvocationListener listenerWithCollectors = listener;
            for (IMetricCollector collector : mCollectors) {
                listenerWithCollectors = collector.init(info.getContext(), listenerWithCollectors);
            }
            while (true) {
                IRemoteTest test = poll();
                if (test == null) {
                    return;
                }
                if (test instanceof IBuildReceiver) {
                    ((IBuildReceiver) test).setBuild(info.getBuildInfo());
                }
                if (test instanceof IDeviceTest) {
                    ((IDeviceTest) test).setDevice(info.getDevice());
                }
                if (test instanceof IInvocationContextReceiver) {
                    ((IInvocationContextReceiver) test).setInvocationContext(info.getContext());
                }
                if (test instanceof ISystemStatusCheckerReceiver) {
                    ((ISystemStatusCheckerReceiver) test)
                            .setSystemStatusChecker(mSystemStatusCheckers);
                }
                IConfiguration validationConfig = new Configuration("validation", "validation");
                try {
                    // At this point only the <test> object needs to be validated for options, this
                    // ensures that the object is fine before running it.
                    validationConfig.setTest(test);
                    validationConfig.validateOptions();
                    DynamicRemoteFileResolver resolver = new DynamicRemoteFileResolver();
                    resolver.addExtraArgs(
                            validationConfig.getCommandOptions().getDynamicDownloadArgs());
                    resolver.setDevice(info.getDevice());
                    validationConfig.resolveDynamicOptions(resolver);
                    // Set the configuration after the validation, otherwise we override the config
                    // available to the test.
                    if (test instanceof IConfigurationReceiver) {
                        ((IConfigurationReceiver) test).setConfiguration(mConfig);
                    }
                    // Run the test itself and prevent random exception from stopping the poller.
                    if (test instanceof IMetricCollectorReceiver) {
                        ((IMetricCollectorReceiver) test).setMetricCollectors(mCollectors);
                        // If test can receive collectors then let it handle the how to set them up
                        test.run(info, listener);
                    } else {
                        test.run(info, listenerWithCollectors);
                    }
                } catch (RuntimeException e) {
                    CLog.e(
                            "Caught an Exception in a test: %s. Proceeding to next test.",
                            test.getClass());
                    CLog.e(e);
                } catch (DeviceUnresponsiveException due) {
                    // being able to catch a DeviceUnresponsiveException here implies that recovery
                    // was successful, and test execution should proceed to next test.
                    CLog.w(
                            "Ignored DeviceUnresponsiveException because recovery was "
                                    + "successful, proceeding with next test. Stack trace:");
                    CLog.w(due);
                    CLog.w("Proceeding to the next test.");
                } catch (DeviceNotAvailableException dnae) {
                    HandleDeviceNotAvailable(listener, dnae, test);
                } catch (ConfigurationException | BuildRetrievalError e) {
                    CLog.w(
                            "Failed to validate the @options of test: %s. Proceeding to next test.",
                            test.getClass());
                    CLog.w(e);
                } finally {
                    validationConfig.cleanConfigurationData();
                }
            }
        } finally {
            mTracker.countDown();
            if (mTracker.getCount() == 0) {
                // If the last poller is also disconnected we want to know about the tests that
                // did not execute.
                reportNotExecuted(listener);
            }
        }
    }

    /**
     * Helper to wait for the device to maybe come back online, in that case we reboot it to refresh
     * the state and proceed with execution.
     */
    void HandleDeviceNotAvailable(
            ITestLogger logger, DeviceNotAvailableException originalException, IRemoteTest test)
            throws DeviceNotAvailableException {
        ITestDevice device = mTestInfo.getDevice();
        try {
            if (device instanceof NestedRemoteDevice) {
                // If it's not the last device, reset it.
                // TODO: Attempt reset when fixed
            } else if (mTracker.getCount() > 1) {
                CLog.d(
                        "Wait %s for device to maybe come back online.",
                        TimeUtil.formatElapsedTime(WAIT_RECOVERY_TIME));
                device.waitForDeviceAvailable(WAIT_RECOVERY_TIME);
                device.reboot();
                CLog.d("TestPoller was recovered after %s went offline", device.getSerialNumber());
                return;
            }
        } catch (DeviceNotAvailableException e) {
            // ignore this exception
        }
        // We catch and rethrow in order to log that the poller associated with the device
        // that went offline is terminating.
        CLog.e(
                "Test %s threw DeviceNotAvailableException. Test poller associated with "
                        + "device %s is terminating.",
                test.getClass(), device.getSerialNumber());
        // Log an event to track more easily the failure
        logDeviceEvent(
                EventType.SHARD_POLLER_EARLY_TERMINATION,
                device.getSerialNumber(),
                originalException);
        throw originalException;
    }

    /** Go through the remaining IRemoteTest and report them as not executed. */
    private void reportNotExecuted(ITestInvocationListener listener) {
        // Report non-executed token test first
        ITokenRequest tokenTest = pollRejectedTokenModule();
        while (tokenTest != null) {
            if (tokenTest instanceof IReportNotExecuted) {
                String message =
                        String.format(
                                "Test did not run. No token '%s' matching it on any device.",
                                tokenTest.getRequiredTokens());
                ((IReportNotExecuted) tokenTest).reportNotExecuted(listener, message);
            } else {
                CLog.e(
                        "Could not report not executed tests from %s.",
                        tokenTest.getClass().getCanonicalName());
            }
            tokenTest = pollRejectedTokenModule();
        }
        // Report all remaining test
        IRemoteTest test = poll(true);
        while (test != null) {
            if (test instanceof IReportNotExecuted) {
                ((IReportNotExecuted) test).reportNotExecuted(listener);
            } else {
                CLog.e(
                        "Could not report not executed tests from %s.",
                        test.getClass().getCanonicalName());
            }
            test = poll(true);
        }
    }

    /** Helper to log the device events. */
    private void logDeviceEvent(EventType event, String serial, Throwable t) {
        Map<String, String> args = new HashMap<>();
        args.put("serial", serial);
        args.put("trace", StreamUtil.getStackTrace(t));
        getLogRegistry().logEvent(LogLevel.DEBUG, event, args);
    }

    private ILogRegistry getLogRegistry() {
        if (mRegistry != null) {
            return mRegistry;
        }
        return LogRegistry.getLogRegistry();
    }

    private boolean isSupported(Set<TokenProperty> requiredTokens) {
        for (TokenProperty prop : requiredTokens) {
            ITokenProvider provider = TokenProviderHelper.getTokenProvider(prop);
            if (provider == null) {
                CLog.e("No provider for token %s", prop);
                return false;
            }
            if (!provider.hasToken(mTestInfo.getDevice(), prop)) {
                return false;
            }
        }
        return true;
    }

    @VisibleForTesting
    public void setLogRegistry(ILogRegistry registry) {
        mRegistry = registry;
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfig = configuration;
    }

    @Override
    public void setSystemStatusChecker(List<ISystemStatusChecker> systemCheckers) {
        mSystemStatusCheckers = systemCheckers;
    }

    @Override
    public void setMetricCollectors(List<IMetricCollector> collectors) {
        mCollectors = collectors;
    }

    /** Get a copy of the pool of token tests. For testing only. */
    @VisibleForTesting
    List<ITokenRequest> getTokenPool() {
        if (mTokenPool == null) {
            return null;
        }
        synchronized (mTokenPool) {
            return new ArrayList<>(mTokenPool);
        }
    }
}
