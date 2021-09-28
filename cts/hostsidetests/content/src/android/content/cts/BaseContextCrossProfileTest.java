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
package android.content.cts;

import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.IDeviceTest;

import org.junit.After;
import org.junit.Before;

import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Base class for context cross profile tests.
 */
public class BaseContextCrossProfileTest implements IDeviceTest {

    private static final int USER_SYSTEM = 0; // From the UserHandle class.

    /** Whether multi-user is supported. */
    protected boolean mSupportsMultiUser;
    protected boolean mIsSplitSystemUser;
    protected int mInitialUserId;
    protected int mPrimaryUserId;

    /** Users we shouldn't delete in the tests. */
    private ArrayList<Integer> mFixedUsers;

    private ITestDevice mDevice;

    @Before
    public void setUp() throws Exception {
        mSupportsMultiUser = getDevice().getMaxNumberOfUsersSupported() > 1;
        mIsSplitSystemUser = checkIfSplitSystemUser();
        mPrimaryUserId = getDevice().getPrimaryUserId();
        setFixedUsers();
        removeTestUsers();
    }

    /**
     * Sets the list of users that should not be removed during setup.
     */
    private void setFixedUsers() throws Exception{
        mFixedUsers = new ArrayList<>();
        mFixedUsers.add(mPrimaryUserId);

        // Set the value of initial user ID calls in {@link #setUp}.
        if (mSupportsMultiUser) {
            mInitialUserId = getDevice().getCurrentUser();
        }

        if (mPrimaryUserId != USER_SYSTEM) {
            mFixedUsers.add(USER_SYSTEM);
        }

        if (getDevice().getCurrentUser() != mPrimaryUserId) {
            mFixedUsers.add(getDevice().getCurrentUser());
        }
    }

    @After
    public void tearDown() throws Exception {
        if (getDevice().getCurrentUser() != mInitialUserId) {
            CLog.w("User changed during test. Switching back to " + mInitialUserId);
            getDevice().switchUser(mInitialUserId);
        }
        // Remove the users created during this test.
        removeTestUsers();
    }

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    /**
     * @param userId the userId of the parent for this profile
     * @return the userId of the created user
     */
    protected int createProfile(int userId)
            throws DeviceNotAvailableException, IllegalStateException {
        return createUser("--profileOf " + userId + " --managed");
    }

    /**
     * @return the userid of the created user
     */
    protected int createUser()
            throws DeviceNotAvailableException, IllegalStateException {
        return createUser("");
    }

    private int createUser(String extraParam)
            throws DeviceNotAvailableException, IllegalStateException {
        final String command =
                "pm create-user " + extraParam + " TestUser_" + System.currentTimeMillis();
        CLog.d("Starting command: " + command);
        final String output = getDevice().executeShellCommand(command);
        CLog.d("Output for command " + command + ": " + output);

        if (output.startsWith("Success")) {
            try {
                return Integer.parseInt(output.substring(output.lastIndexOf(" ")).trim());
            } catch (NumberFormatException e) {
                CLog.e("Failed to parse result: %s", output);
            }
        } else {
            CLog.e("Failed to create user: %s", output);
        }
        throw new IllegalStateException();
    }

    protected static void runDeviceTests(ITestDevice device, String packageName,
            String testClassName,
            String testMethodName, int userId, Map<String, String> testArgs, long timeout,
            TimeUnit unit)
            throws DeviceNotAvailableException {
        if (testClassName != null && testClassName.startsWith(".")) {
            testClassName = packageName + testClassName;
        }
        RemoteAndroidTestRunner testRunner = new RemoteAndroidTestRunner(packageName,
                "androidx.test.runner.AndroidJUnitRunner", device.getIDevice());
        // timeout_msec is the timeout per test for instrumentation
        testRunner.addInstrumentationArg("timeout_msec", Long.toString(unit.toMillis(timeout)));
        if (testClassName != null && testMethodName != null) {
            testRunner.setMethodName(testClassName, testMethodName);
        } else if (testClassName != null) {
            testRunner.setClassName(testClassName);
        }

        if (testArgs != null && testArgs.size() > 0) {
            for (String name : testArgs.keySet()) {
                final String value = testArgs.get(name);
                testRunner.addInstrumentationArg(name, value);
            }
        }
        final CollectingTestListener listener = new CollectingTestListener();
        device.runInstrumentationTestsAsUser(testRunner, userId, listener);

        final TestRunResult result = listener.getCurrentRunResults();
        if (result.isRunFailure()) {
            throw new AssertionError("Failed to successfully run device tests for "
                    + result.getName() + ": " + result.getRunFailureMessage());
        }
        if (result.getNumTests() == 0) {
            throw new AssertionError("No tests were run on the device");
        }
        if (result.hasFailedTests()) {
            // build a meaningful error message
            StringBuilder errorBuilder = new StringBuilder("on-device tests failed:\n");
            for (Map.Entry<TestDescription, TestResult> resultEntry :
                    result.getTestResults().entrySet()) {
                if (!resultEntry.getValue().getStatus().equals(
                        com.android.ddmlib.testrunner.TestResult.TestStatus.PASSED)) {
                    errorBuilder.append(resultEntry.getKey().toString());
                    errorBuilder.append(":\n");
                    errorBuilder.append(resultEntry.getValue().getStackTrace());
                }
            }
            throw new AssertionError(errorBuilder.toString());
        }
    }

    private void removeTestUsers() throws Exception {
        for (int userId : getDevice().listUsers()) {
            if (!mFixedUsers.contains(userId)) {
                getDevice().removeUser(userId);
            }
        }
    }

    private boolean checkIfSplitSystemUser() throws DeviceNotAvailableException {
        final String commandOuput = getDevice().executeShellCommand(
                "getprop ro.fw.system_user_split");
        return "y".equals(commandOuput) || "yes".equals(commandOuput)
                || "1".equals(commandOuput) || "true".equals(commandOuput)
                || "on".equals(commandOuput);
    }
}
