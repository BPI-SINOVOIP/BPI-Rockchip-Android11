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

import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInvocation.Stage;
import com.android.tradefed.invoker.shard.IShardHelper;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;

/**
 * Interface describing the actions that will be done as part of an invocation. The invocation
 * {@link TestInvocation} itself ensure the order of the calls.
 */
public interface IInvocationExecution {

    /**
     * Execute the build_provider step of the invocation.
     *
     * @param testInfo the {@link TestInformation} of the invocation.
     * @param config the {@link IConfiguration} of this test run.
     * @param rescheduler the {@link IRescheduler}, for rescheduling portions of the invocation for
     *     execution on another resource(s)
     * @param listener the {@link ITestInvocation} to report build download failures.
     * @return True if we successfully downloaded the build, false otherwise.
     * @throws BuildRetrievalError
     * @throws DeviceNotAvailableException
     */
    public default boolean fetchBuild(
            TestInformation testInfo,
            IConfiguration config,
            IRescheduler rescheduler,
            ITestInvocationListener listener)
            throws BuildRetrievalError, DeviceNotAvailableException {
        return false;
    }

    /**
     * Execute the build_provider clean up step. Associated with the build fetching.
     *
     * @param context the {@link IInvocationContext} of the invocation.
     * @param config the {@link IConfiguration} of this test run.
     */
    public default void cleanUpBuilds(IInvocationContext context, IConfiguration config) {}

    /**
     * Execute the target_preparer and multi_target_preparer setUp step. Does all the devices setup
     * required for the test to run.
     *
     * @param testInfo the {@link TestInformation} of the invocation.
     * @param config the {@link IConfiguration} of this test run.
     * @param logger the {@link ITestLogger} to report setup failures logs.
     * @throws TargetSetupError
     * @throws BuildError
     * @throws DeviceNotAvailableException
     */
    public default void doSetup(
            TestInformation testInfo, IConfiguration config, final ITestLogger logger)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {}

    /**
     * Invoke the {@link ITestDevice#preInvocationSetup(IBuildInfo)} for each device part of the
     * invocation.
     *
     * @param context the {@link IInvocationContext} of the invocation.
     * @param config the {@link IConfiguration} of this test run.
     * @param logger the {@link ITestLogger} to report logs.
     * @throws DeviceNotAvailableException
     * @throws TargetSetupError
     */
    public default void runDevicePreInvocationSetup(
            IInvocationContext context, IConfiguration config, ITestLogger logger)
            throws DeviceNotAvailableException, TargetSetupError {}

    /**
     * Invoke the {@link ITestDevice#postInvocationTearDown(Throwable)} for each device part of the
     * invocation.
     *
     * @param context the {@link IInvocationContext} of the invocation.
     * @param config the {@link IConfiguration} of this test run.
     * @param exception the original exception thrown by the test running if any.
     */
    public default void runDevicePostInvocationTearDown(
            IInvocationContext context, IConfiguration config, Throwable exception) {}

    /**
     * Execute the target_preparer and multi_target_preparer teardown step. Does the devices tear
     * down associated with the setup.
     *
     * @param testInfo the {@link TestInformation} of the invocation.
     * @param config the {@link IConfiguration} of this test run.
     * @param logger the {@link ITestLogger} to report logs.
     * @param exception the original exception thrown by the test running.
     * @throws Throwable
     */
    public default void doTeardown(
            TestInformation testInfo,
            IConfiguration config,
            ITestLogger logger,
            Throwable exception)
            throws Throwable {}

    /**
     * Execute the target_preparer and multi_target_preparer cleanUp step. Does the devices clean
     * up.
     *
     * @param context the {@link IInvocationContext} of the invocation.
     * @param config the {@link IConfiguration} of this test run.
     * @param exception the original exception thrown by the test running.
     */
    public default void doCleanUp(
            IInvocationContext context, IConfiguration config, Throwable exception) {}

    /**
     * Attempt to shard the configuration into sub-configurations, to be re-scheduled to run on
     * multiple resources in parallel.
     *
     * <p>If a shard count is greater than 1, it will simply create configs for each shard by
     * setting shard indices and reschedule them. If a shard count is not set,it would fallback to
     * {@link IShardHelper#shardConfig}.
     *
     * @param config the current {@link IConfiguration}.
     * @param testInfo the {@link TestInformation} holding the info of the tests.
     * @param rescheduler the {@link IRescheduler}.
     * @param logger {@link ITestLogger} used to log file during sharding.
     * @return true if test was sharded. Otherwise return <code>false</code>
     */
    public default boolean shardConfig(
            IConfiguration config,
            TestInformation testInfo,
            IRescheduler rescheduler,
            ITestLogger logger) {
        return false;
    }

    /**
     * Runs the test.
     *
     * @param info the {@link TestInformation} to run tests with.
     * @param config the {@link IConfiguration} to run
     * @param listener the {@link ITestInvocationListener} of test results
     * @throws Throwable
     */
    public default void runTests(
            TestInformation info, IConfiguration config, ITestInvocationListener listener)
            throws Throwable {}

    /**
     * Report some device logs at different stage of the invocation. For example: logcat.
     *
     * @param device The device to report logs from.
     * @param logger The logger for the logs.
     * @param stage The stage of the invocation we are at.
     */
    public void reportLogs(ITestDevice device, ITestLogger logger, Stage stage);
}
