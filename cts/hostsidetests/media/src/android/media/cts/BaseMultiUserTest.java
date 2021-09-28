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

package android.media.cts;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
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
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nonnull;
import javax.annotation.Nullable;

/**
 * Base class for host-side tests for multi-user aware media APIs.
 */
public class BaseMultiUserTest extends BaseMediaHostSideTest {
    private static final String SETTINGS_PACKAGE_VERIFIER_NAMESPACE = "global";
    private static final String SETTINGS_PACKAGE_VERIFIER_NAME = "package_verifier_enable";

    /**
     * User ID for all users.
     * The value is from the UserHandle class.
     */
    protected static final int USER_ALL = -1;

    /**
     * User ID for the system user.
     * The value is from the UserHandle class.
     */
    protected static final int USER_SYSTEM = 0;

    private String mPackageVerifier;

    private Set<String> mExistingPackages;
    private List<Integer> mExistingUsers;
    private HashSet<String> mAvailableFeatures;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // Ensure that build has been set before test is run.
        assertNotNull(mCtsBuild);
        mExistingPackages = getDevice().getInstalledPackageNames();

        // Disable the package verifier to avoid the dialog when installing an app
        mPackageVerifier =
                getSettings(
                        SETTINGS_PACKAGE_VERIFIER_NAMESPACE,
                        SETTINGS_PACKAGE_VERIFIER_NAME,
                        USER_ALL);
        putSettings(
                SETTINGS_PACKAGE_VERIFIER_NAMESPACE,
                SETTINGS_PACKAGE_VERIFIER_NAME,
                "0",
                USER_ALL);

        mExistingUsers = new ArrayList<>();
        int primaryUserId = getDevice().getPrimaryUserId();
        mExistingUsers.add(primaryUserId);
        mExistingUsers.add(USER_SYSTEM);

        executeShellCommand("am switch-user " + primaryUserId);
        executeShellCommand("wm dismiss-keyguard");
    }

    @Override
    protected void tearDown() throws Exception {
        // Reset the package verifier setting to its original value.
        putSettings(
                SETTINGS_PACKAGE_VERIFIER_NAMESPACE,
                SETTINGS_PACKAGE_VERIFIER_NAME,
                mPackageVerifier,
                USER_ALL);

        // Remove users created during the test.
        for (int userId : getDevice().listUsers()) {
            if (!mExistingUsers.contains(userId)) {
                removeUser(userId);
            }
        }
        // Remove packages installed during the test.
        for (String packageName : getDevice().getUninstallablePackageNames()) {
            if (mExistingPackages.contains(packageName)) {
                continue;
            }
            CLog.d("Removing leftover package: " + packageName);
            getDevice().uninstallPackage(packageName);
        }
        super.tearDown();
    }

    /**
     * Installs the app as if the user of the ID {@param userId} has installed the app.
     *
     * @param appFileName file name of the app.
     * @param userId user ID to install the app against.
     */
    protected void installAppAsUser(String appFileName, int userId, boolean asInstantApp)
            throws FileNotFoundException, DeviceNotAvailableException {
        CLog.d("Installing app " + appFileName + " for user " + userId);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        String result = getDevice().installPackageForUser(
                buildHelper.getTestFile(appFileName),
                true,
                true,
                userId,
                "-t",
                asInstantApp ? "--instant" : "");
        assertNull("Failed to install " + appFileName + " for user " + userId + ": " + result,
                result);
    }

    private int createAndStartUser(String extraParam) throws Exception {
        String command = "pm create-user" + extraParam + " TestUser_" + System.currentTimeMillis();
        String commandOutput = executeShellCommand(command);

        String[] tokens = commandOutput.split("\\s+");
        assertTrue(tokens.length > 0);
        assertEquals("Success:", tokens[0]);
        int userId = Integer.parseInt(tokens[tokens.length-1]);

        // Start user for MediaSessionService to notice the created user.
        getDevice().startUser(userId);
        return userId;
    }

    /**
     * Creates and starts a new user.
     */
    protected int createAndStartUser() throws Exception {
        return createAndStartUser("");
    }

    /**
     * Creates and starts a restricted profile for the {@param parentUserId}.
     *
     * @param parentUserId parent user id.
     */
    protected int createAndStartRestrictedProfile(int parentUserId) throws Exception {
        return createAndStartUser(" --profileOf " + parentUserId + " --restricted");
    }

    /**
     * Creates and starts a managed profile for the {@param parentUserId}.
     *
     * @param parentUserId parent user id.
     */
    protected int createAndStartManagedProfile(int parentUserId) throws Exception {
        return createAndStartUser(" --profileOf " + parentUserId + " --managed");
    }

    /**
     * Removes the user that is created during the test.
     * <p>It will be no-op if the user cannot be removed or doesn't exist.
     *
     * @param userId user ID to remove.
     */
    protected void removeUser(int userId) throws Exception  {
        if (getDevice().listUsers().contains(userId) && userId != USER_SYSTEM
                && !mExistingUsers.contains(userId)) {
            getDevice().executeShellCommand("am wait-for-broadcast-idle");
            // Don't log output, as tests sometimes set no debug user restriction, which
            // causes this to fail, we should still continue and remove the user.
            String stopUserCommand = "am stop-user -w -f " + userId;
            CLog.d("Stopping and removing user " + userId);
            getDevice().executeShellCommand(stopUserCommand);
            assertTrue("Couldn't remove user", getDevice().removeUser(userId));
        }
    }

    /**
     * Runs tests on the device as if it's {@param userId}.
     *
     * @param pkgName test package file name that contains the {@link AndroidTestCase}
     * @param testClassName Class name to test within the test package. Can be {@code null} if you
     *    want to run all test classes in the package.
     * @param testMethodName Method name to test within the test class. Can be {@code null} if you
     *    want to run all test methods in the class. Will be ignored if {@param testClassName} is
     *    {@code null}.
     * @param userId user ID to run the tests as.
     */
    protected void runDeviceTests(
            String pkgName, @Nullable String testClassName,
            @Nullable String testMethodName, int userId) throws DeviceNotAvailableException {
        RemoteAndroidTestRunner testRunner = getTestRunner(pkgName, testClassName, testMethodName);
        CollectingTestListener listener = new CollectingTestListener();
        assertTrue(getDevice().runInstrumentationTestsAsUser(testRunner, userId, listener));

        final TestRunResult result = listener.getCurrentRunResults();
        assertTestsPassed(result);
    }

    /**
     * Checks whether it is possible to create the desired number of users.
     */
    protected boolean canCreateAdditionalUsers(int numberOfUsers)
            throws DeviceNotAvailableException {
        return getDevice().listUsers().size() + numberOfUsers <=
                getDevice().getMaxNumberOfUsersSupported();
    }

    /**
     * Gets the system setting as a string from the system settings provider for the user.
     *
     * @param namespace namespace of the setting.
     * @param name name of the setting.
     * @param userId user ID to query the setting. Can be {@link #USER_ALL}.
     * @return value of the system setting provider with the given namespace and name.
     *    {@code null}, empty string, or "null" will be returned to the empty string ("") instead.
     */
    protected @Nonnull String getSettings(@Nonnull String namespace, @Nonnull String name,
            int userId) throws Exception {
        String userFlag = (userId == USER_ALL) ? "" : " --user " + userId;
        String commandOutput = executeShellCommand(
                "settings" + userFlag + " get " + namespace + " " + name);
        if (commandOutput == null || commandOutput.isEmpty() || commandOutput.equals("null")) {
            commandOutput = "";
        }
        return commandOutput;
    }

    /**
     * Puts the string to the system settings provider for the user.
     * <p>This deletes the setting for an empty {@param value} as 'settings put' doesn't allow
     * putting empty value.
     *
     * @param namespace namespace of the setting.
     * @param name name of the setting.
     * @param value value of the system setting provider with the given namespace and name.
     * @param userId user ID to set the setting. Can be {@link #USER_ALL}.
     */
    protected void putSettings(@Nonnull String namespace, @Nonnull String name,
            @Nullable String value, int userId) throws Exception {
        if (value == null || value.isEmpty()) {
            // Delete the setting if the value is null or empty as 'settings put' doesn't accept
            // them.
            // Ignore userId here because 'settings delete' doesn't support it.
            executeShellCommand("settings delete " + namespace + " " + name);
        } else {
            String userFlag = (userId == USER_ALL) ? "" : " --user " + userId;
            executeShellCommand("settings" + userFlag + " put " + namespace + " " + name
                    + " " + value);
        }
    }

    protected boolean hasDeviceFeature(String requiredFeature) throws DeviceNotAvailableException {
        if (mAvailableFeatures == null) {
            // TODO: Move this logic to ITestDevice.
            String command = "pm list features";
            String commandOutput = getDevice().executeShellCommand(command);
            CLog.i("Output for command " + command + ": " + commandOutput);

            // Extract the id of the new user.
            mAvailableFeatures = new HashSet<>();
            for (String feature : commandOutput.split("\\s+")) {
                // Each line in the output of the command has the format "feature:{FEATURE_VALUE}".
                String[] tokens = feature.split(":");
                assertTrue(
                        "\"" + feature + "\" expected to have format feature:{FEATURE_VALUE}",
                        tokens.length > 1);
                assertEquals(feature, "feature", tokens[0]);
                mAvailableFeatures.add(tokens[1]);
            }
        }
        boolean result = mAvailableFeatures.contains(requiredFeature);
        return result;
    }
}
