/*
 * Copyright (C) 2020 The Android Open Source Project
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
package android.media.cts;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;

import java.io.FileNotFoundException;
import java.util.Map;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/** Base class for host-side tests for media APIs. */
public class BaseMediaHostSideTest extends DeviceTestCase implements IBuildReceiver {
    private static final String RUNNER = "androidx.test.runner.AndroidJUnitRunner";

    /**
     * The defined timeout (in milliseconds) is used as a maximum waiting time when expecting the
     * command output from the device. At any time, if the shell command does not output anything
     * for a period longer than the defined timeout the Tradefed run terminates.
     */
    private static final long DEFAULT_SHELL_TIMEOUT_MILLIS = TimeUnit.MINUTES.toMillis(5);

    /** Instrumentation test runner argument key used for individual test timeout. */
    protected static final String TEST_TIMEOUT_INST_ARGS_KEY = "timeout_msec";

    /**
     * Sets timeout (in milliseconds) that will be applied to each test. In the event of a test
     * timeout it will log the results and proceed with executing the next test.
     */
    private static final long DEFAULT_TEST_TIMEOUT_MILLIS = TimeUnit.MINUTES.toMillis(5);

    protected IBuildInfo mCtsBuild;

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    /**
     * Runs tests on the device.
     *
     * @param pkgName The test package file name that contains the test.
     * @param testClassName The class name to test within the test package. If {@code null}, runs
     *     all test classes in the package.
     * @param testMethodName Method name to test within the test class. Ignored if {@code
     *     testClassName} is {@code null}. If {@code null}, runs all test classes in the class.
     */
    protected void runDeviceTests(
            String pkgName, @Nullable String testClassName, @Nullable String testMethodName)
            throws DeviceNotAvailableException {
        RemoteAndroidTestRunner testRunner = getTestRunner(pkgName, testClassName, testMethodName);
        CollectingTestListener listener = new CollectingTestListener();
        assertTrue(getDevice().runInstrumentationTests(testRunner, listener));
        assertTestsPassed(listener.getCurrentRunResults());
    }

    /**
     * Excutes shell command and returns the result.
     *
     * @param command The command to run.
     * @return The result from the command. If the result was {@code null}, empty string ("") will
     *     be returned instead. Otherwise, trimmed result will be returned.
     */
    protected @Nonnull String executeShellCommand(String command) throws Exception {
        LogUtil.CLog.d("Starting command " + command);
        String commandOutput = getDevice().executeShellCommand(command);
        LogUtil.CLog.d("Output for command " + command + ": " + commandOutput);
        return commandOutput != null ? commandOutput.trim() : "";
    }

    /** Installs the app with the given {@code appFileName}. */
    protected void installApp(String appFileName)
            throws FileNotFoundException, DeviceNotAvailableException {
        LogUtil.CLog.d("Installing app " + appFileName);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        String result =
                getDevice()
                        .installPackage(
                                buildHelper.getTestFile(appFileName),
                                /* reinstall= */ true,
                                /* grantPermissions= */ true,
                                "-t"); // Signals that this is a test APK.
        assertNull("Failed to install " + appFileName + ": " + result, result);
    }

    /** Returns a {@link RemoteAndroidTestRunner} for the given test parameters. */
    protected RemoteAndroidTestRunner getTestRunner(
            String pkgName, String testClassName, String testMethodName) {
        if (testClassName != null && testClassName.startsWith(".")) {
            testClassName = pkgName + testClassName;
        }

        RemoteAndroidTestRunner testRunner =
                new RemoteAndroidTestRunner(pkgName, RUNNER, getDevice().getIDevice());
        testRunner.setMaxTimeToOutputResponse(DEFAULT_SHELL_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS);
        testRunner.addInstrumentationArg(
                TEST_TIMEOUT_INST_ARGS_KEY, Long.toString(DEFAULT_TEST_TIMEOUT_MILLIS));
        if (testClassName != null && testMethodName != null) {
            testRunner.setMethodName(testClassName, testMethodName);
        } else if (testClassName != null) {
            testRunner.setClassName(testClassName);
        }
        return testRunner;
    }

    /**
     * Asserts that {@code testRunResult} contains at least one test, and that all tests passed.
     *
     * <p>If the assertion fails, an {@link AssertionError} with a descriptive message is thrown.
     */
    protected void assertTestsPassed(TestRunResult testRunResult) {
        if (testRunResult.isRunFailure()) {
            throw new AssertionError(
                    "Failed to successfully run device tests for "
                            + testRunResult.getName()
                            + ": "
                            + testRunResult.getRunFailureMessage());
        }
        if (testRunResult.getNumTests() == 0) {
            throw new AssertionError("No tests were run on the device");
        }

        if (testRunResult.hasFailedTests()) {
            // Build a meaningful error message
            StringBuilder errorBuilder = new StringBuilder("On-device tests failed:\n");
            for (Map.Entry<TestDescription, TestResult> resultEntry :
                    testRunResult.getTestResults().entrySet()) {
                if (!resultEntry
                        .getValue()
                        .getStatus()
                        .equals(com.android.ddmlib.testrunner.TestResult.TestStatus.PASSED)) {
                    errorBuilder.append(resultEntry.getKey().toString());
                    errorBuilder.append(":\n");
                    errorBuilder.append(resultEntry.getValue().getStackTrace());
                }
            }
            throw new AssertionError(errorBuilder.toString());
        }
    }
}
