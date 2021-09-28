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
package android.host.multiuser;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.IDeviceTest;

import org.junit.After;
import org.junit.Before;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Scanner;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

/**
 * Base class for multi user tests.
 */
public class BaseMultiUserTest implements IDeviceTest {

    /** Guest flag value from android/content/pm/UserInfo.java */
    private static final int FLAG_GUEST = 0x00000004;

    /**
     * Feature flag for automotive devices
     * https://source.android.com/compatibility/android-cdd#2_5_automotive_requirements
     */
    private static final String FEATURE_AUTOMOTIVE = "feature:android.hardware.type.automotive";


    protected static final long LOGCAT_POLL_INTERVAL_MS = 1000;
    protected static final long USER_SWITCH_COMPLETE_TIMEOUT_MS = 360_000;

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

        mInitialUserId = getDevice().getCurrentUser();
        mPrimaryUserId = getDevice().getPrimaryUserId();

        // Test should not modify / remove any of the existing users.
        mFixedUsers = getDevice().listUsers();
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

    protected int createRestrictedProfile(int userId)
            throws DeviceNotAvailableException, IllegalStateException{
        final String command = "pm create-user --profileOf " + userId + " --restricted "
                + "TestUser_" + System.currentTimeMillis();
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
            CLog.e("Failed to create restricted profile: %s", output);
        }
        throw new IllegalStateException();
    }

    /**
     * @return the userid of the created user
     */
    protected int createUser()
            throws DeviceNotAvailableException, IllegalStateException {
        final String command = "pm create-user "
                + "TestUser_" + System.currentTimeMillis();
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

    protected int createGuestUser() throws Exception {
        return mDevice.createUser(
                "TestUser_" + System.currentTimeMillis() /* name */,
                true /* guest */,
                false /* ephemeral */);
    }

    protected int getGuestUser() throws Exception {
        for (int userId : mDevice.listUsers()) {
            if ((mDevice.getUserFlags(userId) & FLAG_GUEST) != 0) {
                return userId;
            }
        }
        return -1;
    }

    protected boolean isAutomotiveDevice() throws Exception {
        return getDevice().hasFeature(FEATURE_AUTOMOTIVE);
    }

    protected void assertSwitchToNewUser(int toUserId) throws Exception {
        final String exitString = "Finished processing BOOT_COMPLETED for u" + toUserId;
        final Set<String> appErrors = new LinkedHashSet<>();
        getDevice().executeAdbCommand("logcat", "-b", "all", "-c"); // Reset log
        assertTrue("Couldn't switch to user " + toUserId, getDevice().switchUser(toUserId));
        final boolean result = waitForUserSwitchComplete(appErrors, toUserId, exitString);
        assertTrue("Didn't receive BOOT_COMPLETED delivered notification. appErrors="
                + appErrors, result);
        if (!appErrors.isEmpty()) {
            throw new AppCrashOnBootError(appErrors);
        }
    }

    protected void assertSwitchToUser(int fromUserId, int toUserId) throws Exception {
        final String exitString = "uc_continue_user_switch: [" + fromUserId + "," + toUserId + "]";
        final Set<String> appErrors = new LinkedHashSet<>();
        getDevice().executeAdbCommand("logcat", "-b", "all", "-c"); // Reset log
        assertTrue("Couldn't switch to user " + toUserId, getDevice().switchUser(toUserId));
        final boolean result = waitForUserSwitchComplete(appErrors, toUserId, exitString);
        assertTrue("Didn't reach \"Continue user switch\" stage. appErrors=" + appErrors, result);
        if (!appErrors.isEmpty()) {
            throw new AppCrashOnBootError(appErrors);
        }
    }

    protected void assertUserNotPresent(int userId) throws Exception {
        assertFalse("User ID " + userId + " should not be present",
                getDevice().listUsers().contains(userId));
    }

    /*
     * Waits for userId to removed or at removing state.
     * Returns true if user is removed or at removing state.
     * False if user is not removed by USER_SWITCH_COMPLETE_TIMEOUT_MS.
     */
    protected boolean waitForUserRemove(int userId)
            throws DeviceNotAvailableException, InterruptedException {
        // Example output from dumpsys when user is flagged for removal:
        // UserInfo{11:Driver:154} serialNo=50 <removing>  <partial>
        final String userSerialPatter = "(.*\\{)(\\d+)(.*\\})(.*=)(\\d+)(.*)";
        final Pattern pattern = Pattern.compile(userSerialPatter);
        long ti = System.currentTimeMillis();
        while (System.currentTimeMillis() - ti < USER_SWITCH_COMPLETE_TIMEOUT_MS) {
            if (!getDevice().listUsers().contains(userId)) {
                return true;
            }
            String commandOutput = getDevice().executeShellCommand("dumpsys user");
            Matcher matcher = pattern.matcher(commandOutput);
            while(matcher.find()) {
                if (Integer.parseInt(matcher.group(2)) == userId
                        && matcher.group(6).contains("removing")) {
                    return true;
                }
            }
            Thread.sleep(LOGCAT_POLL_INTERVAL_MS);
        }
        return false;
    }

    private boolean waitForUserSwitchComplete(Set<String> appErrors, int targetUserId,
            String exitString) throws DeviceNotAvailableException, InterruptedException {
        boolean mExitFound = false;
        long ti = System.currentTimeMillis();
        while (System.currentTimeMillis() - ti < USER_SWITCH_COMPLETE_TIMEOUT_MS) {
            String logs = getDevice().executeAdbCommand("logcat", "-b", "all", "-d",
                    "ActivityManager:D", "AndroidRuntime:E", "*:I");
            Scanner in = new Scanner(logs);
            while (in.hasNextLine()) {
                String line = in.nextLine();
                if (line.contains("Showing crash dialog for package")) {
                    appErrors.add(line);
                } else if (line.contains(exitString)) {
                    // Parse all logs in case crashes occur as a result of onUserChange callbacks
                    mExitFound = true;
                } else if (line.contains("FATAL EXCEPTION IN SYSTEM PROCESS")) {
                    throw new IllegalStateException("System process crashed - " + line);
                }
            }
            in.close();
            if (mExitFound) {
                if (!appErrors.isEmpty()) {
                    CLog.w("App crash dialogs found: " + appErrors);
                }
                return true;
            }
            Thread.sleep(LOGCAT_POLL_INTERVAL_MS);
        }
        return false;
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

    static class AppCrashOnBootError extends AssertionError {
        private static final Pattern PACKAGE_NAME_PATTERN = Pattern.compile("package ([^\\s]+)");
        private Set<String> errorPackages;

        AppCrashOnBootError(Set<String> errorLogs) {
            super("App error dialog(s) are present: " + errorLogs);
            this.errorPackages = errorLogsToPackageNames(errorLogs);
        }

        private static Set<String> errorLogsToPackageNames(Set<String> errorLogs) {
            Set<String> result = new HashSet<>();
            for (String line : errorLogs) {
                Matcher matcher = PACKAGE_NAME_PATTERN.matcher(line);
                if (matcher.find()) {
                    result.add(matcher.group(1));
                } else {
                    throw new IllegalStateException("Unrecognized line " + line);
                }
            }
            return result;
        }
    }

    /**
     * Rule that retries the test if it failed due to {@link AppCrashOnBootError}
     */
    public static class AppCrashRetryRule implements TestRule {

        @Override
        public Statement apply(Statement base, Description description) {
            return new Statement() {
                @Override
                public void evaluate() throws Throwable {
                    Set<String> errors = evaluateAndReturnAppCrashes(base);
                    if (errors.isEmpty()) {
                        return;
                    }
                    CLog.e("Retrying due to app crashes: " + errors);
                    // Fail only if same apps are crashing in both runs
                    errors.retainAll(evaluateAndReturnAppCrashes(base));
                    assertTrue("App error dialog(s) are present after 2 attempts: " + errors,
                            errors.isEmpty());
                }
            };
        }

        private static Set<String> evaluateAndReturnAppCrashes(Statement base) throws Throwable {
            try {
                base.evaluate();
            } catch (AppCrashOnBootError e) {
                return e.errorPackages;
            }
            return new HashSet<>();
        }
    }
}
