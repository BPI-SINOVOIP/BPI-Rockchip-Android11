/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.google.android.cts.settings;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.ddmlib.Log.LogLevel;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;
import java.io.FileNotFoundException;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;

/** Set of tests for Device Owner use cases. */
public class PrivacyDeviceOwnerTest extends DeviceTestCase implements IBuildReceiver {
    private static final String RUNNER = "androidx.test.runner.AndroidJUnitRunner";

    private static final String DEVICE_OWNER_APK = "CtsSettingsDeviceOwnerApp.apk";
    private static final String DEVICE_OWNER_PKG = "com.google.android.cts.deviceowner";

    private static final String ADMIN_RECEIVER_TEST_CLASS = ".DeviceOwnerTest$BasicAdminReceiver";
    private static final String CLEAR_DEVICE_OWNER_TEST_CLASS = ".ClearDeviceOwnerTest";

    /**
     * The defined timeout (in milliseconds) is used as a maximum waiting time when expecting the
     * command output from the device. At any time, if the shell command does not output anything
     * for a period longer than defined timeout the Tradefed run terminates.
     */
    private static final long DEFAULT_SHELL_TIMEOUT_MILLIS = TimeUnit.MINUTES.toMillis(20);

    /** instrumentation test runner argument key used for individual test timeout */
    protected static final String TEST_TIMEOUT_INST_ARGS_KEY = "timeout_msec";

    /**
     * Sets timeout (in milliseconds) that will be applied to each test. In the event of a test
     * timeout it will log the results and proceed with executing the next test.
     */
    private static final long DEFAULT_TEST_TIMEOUT_MILLIS = TimeUnit.MINUTES.toMillis(10);

    protected boolean mHasFeature;
    protected IBuildInfo mCtsBuild;

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mHasFeature = hasDeviceFeature("android.software.device_admin");
        if (mHasFeature) {
            installPackage(DEVICE_OWNER_APK);
        }
    }

    @Override
    protected void tearDown() throws Exception {
        if (mHasFeature) {
            assertTrue(
                    "Failed to remove device owner.",
                    runDeviceTests(
                            DEVICE_OWNER_PKG,
                            DEVICE_OWNER_PKG + CLEAR_DEVICE_OWNER_TEST_CLASS,
                            null));
            getDevice().uninstallPackage(DEVICE_OWNER_PKG);
        }

        super.tearDown();
    }

    /** The case: app is the device owner, has work policy info. */
    public void testDeviceOwnerWithInfo() throws Exception {
        if (!mHasFeature) {
            return;
        }
        setDeviceOwner();
        executeDeviceOwnerTest("testDeviceOwnerWithInfo");
    }

    /** The case: app is NOT the device owner, has work policy info. */
    public void testNonDeviceOwnerWithInfo() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("testNonDeviceOwnerWithInfo");
    }

    /** The case: app is the device owner, doesn't have work policy info. */
    public void testDeviceOwnerWithoutInfo() throws Exception {
        if (!mHasFeature) {
            return;
        }
        setDeviceOwner();
        executeDeviceOwnerTest("testDeviceOwnerWithoutInfo");
    }

    /** The case: app is NOT the device owner, doesn't have work policy info. */
    public void testNonDeviceOwnerWithoutInfo() throws Exception {
        if (!mHasFeature) {
            return;
        }
        executeDeviceOwnerTest("testNonDeviceOwnerWithoutInfo");
    }

    private void executeDeviceOwnerTest(String testMethodName) throws Exception {
        String testClass = DEVICE_OWNER_PKG + ".DeviceOwnerTest";
        assertTrue(
                testClass + " failed.",
                runDeviceTests(DEVICE_OWNER_PKG, testClass, testMethodName));
    }

    protected void installPackage(String appFileName)
            throws FileNotFoundException, DeviceNotAvailableException {
        CLog.d("Installing app " + appFileName);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        List<String> extraArgs = new LinkedList<>();
        extraArgs.add("-t");
        String result =
                getDevice()
                        .installPackage(
                                buildHelper.getTestFile(appFileName),
                                true,
                                true,
                                extraArgs.toArray(new String[extraArgs.size()]));
        assertNull("Failed to install " + appFileName + ": " + result, result);
    }

    protected boolean runDeviceTests(
            String pkgName, @Nullable String testClassName, @Nullable String testMethodName)
            throws DeviceNotAvailableException {
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

        CollectingTestListener listener = new CollectingTestListener();
        boolean runResult = getDevice().runInstrumentationTests(testRunner, listener);

        final TestRunResult result = listener.getCurrentRunResults();
        if (result.isRunFailure()) {
            throw new AssertionError(
                    "Failed to successfully run device tests for "
                            + result.getName()
                            + ": "
                            + result.getRunFailureMessage());
        }
        if (result.getNumTests() == 0) {
            throw new AssertionError("No tests were run on the device");
        }

        if (result.hasFailedTests()) {
            // build a meaningful error message
            StringBuilder errorBuilder = new StringBuilder("On-device tests failed:\n");
            for (Map.Entry<TestDescription, TestResult> resultEntry :
                    result.getTestResults().entrySet()) {
                if (!resultEntry.getValue().getStatus().equals(TestStatus.PASSED)) {
                    errorBuilder.append(resultEntry.getKey().toString());
                    errorBuilder.append(":\n");
                    errorBuilder.append(resultEntry.getValue().getStackTrace());
                }
            }
            throw new AssertionError(errorBuilder.toString());
        }

        return runResult;
    }

    private void setDeviceOwner() throws DeviceNotAvailableException {
        String componentName = DEVICE_OWNER_PKG + "/" + ADMIN_RECEIVER_TEST_CLASS;
        String command = "dpm set-device-owner '" + componentName + "'";
        String commandOutput = getDevice().executeShellCommand(command);
        CLog.logAndDisplay(LogLevel.INFO, "Output for command " + command + ": " + commandOutput);
        assertTrue(
                commandOutput + " expected to start with \"Success:\" " + commandOutput,
                commandOutput.startsWith("Success:"));
    }

    protected boolean hasDeviceFeature(String requiredFeature) throws DeviceNotAvailableException {
        String command = "pm list features";
        String commandOutput = getDevice().executeShellCommand(command);
        CLog.i("Output for command " + command + ": " + commandOutput);

        Set<String> availableFeatures = new HashSet<>();
        for (String feature : commandOutput.split("\\s+")) {
            // Each line in the output of the command has the format "feature:{FEATURE_VALUE}".
            String[] tokens = feature.split(":");
            assertTrue(
                    "\"" + feature + "\" expected to have format feature:{FEATURE_VALUE}",
                    tokens.length > 1);
            assertEquals(feature, "feature", tokens[0]);
            availableFeatures.add(tokens[1]);
        }
        boolean result = availableFeatures.contains(requiredFeature);
        if (!result) {
            CLog.d("Device doesn't have required feature " + requiredFeature + ". Test won't run.");
        }
        return result;
    }
}
