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
 * limitations under the License
 */

package com.android.cts.numberblocking.hostside;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.ddmlib.Log;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;

import java.io.File;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Multi-user tests for number blocking.
 */
// To run the tests in this file w/o running all the cts tests:
// make cts
// cts-tradefed
// run cts -m CtsHostsideNumberBlockingTestCases
public class NumberBlockingTest extends DeviceTestCase implements IBuildReceiver {
    private static final String BLOCKED_NUMBER = "556";
    private static final String PHONE_ACCOUNT_ID = "test_call_provider_id";
    private static final String TEST_APK = "CtsHostsideNumberBlockingAppTest.apk";
    private static final String NUMBER_BLOCKING_TESTS_PKG =
            NumberBlockingTest.class.getPackage().getName();
    private static final String CALL_BLOCKING_TEST_CLASS_NAME = "CallBlockingTest";
    private static final String NUMBER_BLOCKING_APP_TEST_CLASS_NAME = "NumberBlockingAppTest";
    private static final String TEST_APP_CONNECTION_SERVICE_NAME = "DummyConnectionService";
    private static final String SECONDARY_USER_NAME = "NumberBlockingTest SecondaryUser";
    private static final String FEATURE_TELEPHONY = "android.hardware.telephony";
    private static final String FEATURE_CONNECTION_SERVICE = "android.software.connectionservice";

    private int mSecondaryUserId;
    private int mPrimaryUserSerialNumber;
    private int mSecondaryUserSerialNumber;

    private IBuildInfo mCtsBuild;
    private boolean mHasFeature;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mHasFeature = getDevice().isMultiUserSupported()
                && getDevice().hasFeature(FEATURE_TELEPHONY)
                && getDevice().hasFeature(FEATURE_CONNECTION_SERVICE);

        if (!mHasFeature) {
            return;
        }

        installTestAppForUser(getDevice().getPrimaryUserId());
        createSecondaryUser();
        installTestAppForUser(mSecondaryUserId);

        mPrimaryUserSerialNumber = getUserSerialNumber(getDevice().getPrimaryUserId());
        mSecondaryUserSerialNumber = getUserSerialNumber(mSecondaryUserId);
    }

    @Override
    protected void tearDown() throws Exception {
        if (mHasFeature) {
            getDevice().removeUser(mSecondaryUserId);
        }

        super.tearDown();
    }

    @Override
    public void setBuild(IBuildInfo iBuildInfo) {
        mCtsBuild = iBuildInfo;
    }

    public void testNumberBlocking() throws Exception {
        if (!mHasFeature) {
            LogUtil.CLog.logAndDisplay(Log.LogLevel.INFO,
                    "Skipping number blocking test as the feature is not supported.");
            return;
        }

        try {
            // First run tests for primary user.
            // Cleanup state prior to running tests.
            setTestAppAsDefaultSmsAppForUser(
                    true /* setToSmsApp */, getDevice().getPrimaryUserId());
            runTestAsPrimaryUser(NUMBER_BLOCKING_APP_TEST_CLASS_NAME,
                    "testCleanupBlockedNumberAsPrimaryUserSucceeds");

            // Block a number as a privileged app that can block numbers.
            runTestAsPrimaryUser(
                    NUMBER_BLOCKING_APP_TEST_CLASS_NAME, "testBlockNumberAsPrimaryUserSucceeds");
            setTestAppAsDefaultSmsAppForUser(
                    false /* setToSmsApp */, getDevice().getPrimaryUserId());

            // Ensure incoming call from blocked number is rejected, and unregister the phone
            // account.
            runTestAsPrimaryUser(CALL_BLOCKING_TEST_CLASS_NAME, "testRegisterPhoneAccount");
            enablePhoneAccountForUser(mPrimaryUserSerialNumber);
            runTestAsPrimaryUser(CALL_BLOCKING_TEST_CLASS_NAME,
                    "testIncomingCallFromBlockedNumberIsRejected");
            runTestAsPrimaryUser(CALL_BLOCKING_TEST_CLASS_NAME, "testUnregisterPhoneAccount");

            // Run tests as secondary user.
            startUserAndWait(mSecondaryUserId);

            // Ensure that a privileged app cannot block numbers when the current user is a
            // secondary user.
            setTestAppAsDefaultSmsAppForUser(true /* setToSmsApp */, mSecondaryUserId);
            runTestAsSecondaryUser(NUMBER_BLOCKING_APP_TEST_CLASS_NAME,
                    "testSecondaryUserCannotBlockNumbers");
            setTestAppAsDefaultSmsAppForUser(false /* setToSmsApp */, mSecondaryUserId);

            // Calls should be blocked by Telecom for secondary users as well.
            runTestAsSecondaryUser(CALL_BLOCKING_TEST_CLASS_NAME, "testRegisterPhoneAccount");
            enablePhoneAccountForUser(mSecondaryUserSerialNumber);
            runTestAsSecondaryUser(CALL_BLOCKING_TEST_CLASS_NAME,
                    "testIncomingCallFromBlockedNumberIsRejected");
        } finally {
            // Cleanup state by unblocking the blocked number.
            setTestAppAsDefaultSmsAppForUser(
                    true /* setToSmsApp */, getDevice().getPrimaryUserId());
            runTestAsPrimaryUser(
                    NUMBER_BLOCKING_APP_TEST_CLASS_NAME, "testUnblockNumberAsPrimaryUserSucceeds");
        }
    }

    /** Starts user {@code userId} and waits until it is in state RUNNING_UNLOCKED. */
    protected void startUserAndWait(int userId) throws Exception {
        getDevice().startUser(userId);

        final String desiredState = "RUNNING_UNLOCKED";
        final long USER_STATE_TIMEOUT_MS = 60_0000; // 1 minute
        final long timeout = System.currentTimeMillis() + USER_STATE_TIMEOUT_MS;
        final String command = String.format("am get-started-user-state %d", userId);
        String output = "";
        while (System.currentTimeMillis() <= timeout) {
            output = getDevice().executeShellCommand(command);
            if (output.contains(desiredState)) {
                return;
            }
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                // Do nothing.
            }
        }
        fail("User state of " + userId + " was '" + output + "' rather than " + desiredState);
    }

    private void createSecondaryUser() throws Exception {
        mSecondaryUserId = getDevice().createUser(SECONDARY_USER_NAME);
        getDevice().waitForDeviceAvailable();
    }

    private void installTestAppForUser(int userId) throws Exception {
        LogUtil.CLog.logAndDisplay(Log.LogLevel.INFO, "Installing test app for user: " + userId);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File testAppFile = buildHelper.getTestFile(TEST_APK);
        String installResult;
        try {
            installResult = getDevice().installPackageForUser(
                    testAppFile, true /*reinstall*/, userId);
        } catch (DeviceNotAvailableException dna) {
            fail("Device not available to install test app " + dna);
            return;
        }
        assertNull(String.format(
                "failed to install number blocking test app. Reason: %s", installResult),
                installResult);

        waitForTestAppInstallation(NUMBER_BLOCKING_TESTS_PKG);
    }

    private void runTestAsPrimaryUser(String className, String methodName) throws Exception {
        runTestAsUser(className, methodName, getDevice().getPrimaryUserId());
    }

    private void runTestAsSecondaryUser(String className, String methodName) throws Exception {
        runTestAsUser(className, methodName, mSecondaryUserId);
    }

    private void runTestAsUser(String className, String methodName, int userId) throws Exception {
        LogUtil.CLog.logAndDisplay(Log.LogLevel.INFO, "Running %s.%s for user: %d",
                className, methodName, userId);
        RemoteAndroidTestRunner testRunner = new RemoteAndroidTestRunner(
                NUMBER_BLOCKING_TESTS_PKG,
                "androidx.test.runner.AndroidJUnitRunner",
                getDevice().getIDevice());
        testRunner.addInstrumentationArg("blocked_number", BLOCKED_NUMBER);
        testRunner.addInstrumentationArg("phone_account_id", PHONE_ACCOUNT_ID);
        testRunner.setMethodName(NUMBER_BLOCKING_TESTS_PKG + "." + className, methodName);
        CollectingTestListener listener = new CollectingTestListener();
        getDevice().runInstrumentationTestsAsUser(testRunner, userId, listener);
        assertEquals(1, listener.getNumTotalTests());
        assertFalse(listener.getCurrentRunResults().getTestResults().keySet().toString(),
                listener.getCurrentRunResults().hasFailedTests());
    }

    private void enablePhoneAccountForUser(int userSerialNumber) throws Exception {
        String command = String.format(
                "telecom set-phone-account-enabled %s\\/%s.%s\\$%s %s %d",
                NUMBER_BLOCKING_TESTS_PKG,
                NUMBER_BLOCKING_TESTS_PKG,
                CALL_BLOCKING_TEST_CLASS_NAME,
                TEST_APP_CONNECTION_SERVICE_NAME,
                PHONE_ACCOUNT_ID,
                userSerialNumber);
        String commandResponse = getDevice().executeShellCommand(command);
        assertTrue(commandResponse, commandResponse.contains("Success"));
    }

    private void setTestAppAsDefaultSmsAppForUser(boolean setToSmsApp, int userId)
            throws Exception {
        String command = String.format("appops set --user %d %s WRITE_SMS %s", userId,
                NUMBER_BLOCKING_TESTS_PKG,
                setToSmsApp ? "allow" : "default");
        assertEquals("", getDevice().executeShellCommand(command));
    }

    // TODO: Replace this with API in ITestDevice once it is available.
    private int getUserSerialNumber(int userId) throws DeviceNotAvailableException {
        Pattern pattern = Pattern.compile("^.*UserInfo\\{" + userId + "\\:.*serialNo=(\\d+).*$");
        // dumpsys user return lines like "UserInfo{0:Owner:13} serialNo=0"
        String commandOutput = getDevice().executeShellCommand("dumpsys user");
        String[] tokens = commandOutput.split("\\n");
        for (String token : tokens) {
            token = token.trim();
            Matcher matcher = pattern.matcher(token);
            if (matcher.matches()) {
                int serialNumber = Integer.parseInt(matcher.group(1));
                LogUtil.CLog.logAndDisplay(
                        Log.LogLevel.INFO,
                        String.format("Serial number of user %d : %d", userId, serialNumber));
                return serialNumber;
            }
        }
        fail("Couldn't find user " + userId);
        return -1;
    }

    private void waitForTestAppInstallation(String packageName) {
        try {
            int retries = 0;
            while (!getDevice().getInstalledPackageNames().contains(packageName)
                    && retries < 10) {
                Thread.sleep(50);
                retries++;
            }

            assertTrue(getDevice().getInstalledPackageNames().contains(packageName));
        } catch (DeviceNotAvailableException dne) {
            fail("Device not available.");
        } catch (InterruptedException ie) {
            fail("Failed to wait for change.");
        }
    }
}
