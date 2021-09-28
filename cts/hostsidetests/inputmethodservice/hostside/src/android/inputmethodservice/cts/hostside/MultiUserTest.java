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

package android.inputmethodservice.cts.hostside;

import static android.inputmethodservice.cts.common.BusyWaitUtils.pollingCheck;

import static org.junit.Assert.assertEquals;
import static org.junit.Assume.assumeTrue;

import android.inputmethodservice.cts.common.Ime1Constants;
import android.inputmethodservice.cts.common.Ime2Constants;
import android.inputmethodservice.cts.common.test.DeviceTestConstants;
import android.inputmethodservice.cts.common.test.ShellCommandUtils;
import android.inputmethodservice.cts.common.test.TestInfo;
import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.DeviceTestRunOptions;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test IME APIs for multi-user environment.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class MultiUserTest extends BaseHostJUnit4Test {
    private static final long USER_SWITCH_TIMEOUT = TimeUnit.SECONDS.toMillis(60);
    private static final long USER_SWITCH_POLLING_INTERVAL = TimeUnit.MILLISECONDS.toMillis(100);
    private static final long IME_COMMAND_TIMEOUT = TimeUnit.SECONDS.toMillis(7);

    /**
     * Because of Bug 132082599, processes can be asynchronously killed due to delayed tasks in
     * ActivityManagerService after APK installation. To work around this, we check if a no-op test
     * can stay alive for 3 seconds.  This is the retry count of this precondition check.
     */
    private static final int NO_OP_TEST_RETRY_COUNT_AFTER_APK_INSTALL = 3;

    /**
     * A sleep time after calling {@link com.android.tradefed.device.ITestDevice#switchUser(int)}
     * to see if the flakiness comes from race condition in UserManagerService#removeUser() or not.
     *
     * <p>TODO(Bug 122609784): Remove this once we figure out what is the root cause of flakiness.
     * </p>
     */
    private static final long WAIT_AFTER_USER_SWITCH = TimeUnit.SECONDS.toMillis(10);

    private boolean mNeedsTearDown = false;

    /**
     * {@code true} if {@link #tearDown()} needs to be fully executed.
     *
     * <p>When {@link #setUp()} is interrupted by {@link org.junit.AssumptionViolatedException}
     * before the actual setup tasks are executed, all the corresponding cleanup tasks should also
     * be skipped.</p>
     *
     * <p>Once JUnit 5 becomes available in Android, we can remove this by moving the assumption
     * checks into a non-static {@link org.junit.BeforeClass} method.</p>
     */
    private ArrayList<Integer> mOriginalUsers;

    /**
     * Set up the test case
     */
    @Before
    public void setUp() throws Exception {
        // Skip whole tests when DUT has no android.software.input_methods feature.
        assumeTrue(hasDeviceFeature(ShellCommandUtils.FEATURE_INPUT_METHODS));
        assumeTrue(getDevice().isMultiUserSupported());
        mNeedsTearDown = true;

        mOriginalUsers = new ArrayList<>(getDevice().listUsers());
        mOriginalUsers.forEach(
                userId -> shell(ShellCommandUtils.uninstallPackage(Ime1Constants.PACKAGE, userId)));
    }

    /**
     * Tear down the test case.
     */
    @After
    public void tearDown() throws Exception {
        if (!mNeedsTearDown) {
            return;
        }

        getDevice().switchUser(getDevice().getPrimaryUserId());
        // We suspect that the optimization made for Bug 38143512 was a bit unstable.  Let's see
        // if adding a sleep improves the stability or not.
        Thread.sleep(WAIT_AFTER_USER_SWITCH);

        final ArrayList<Integer> newUsers = getDevice().listUsers();
        for (int userId : newUsers) {
            if (!mOriginalUsers.contains(userId)) {
                getDevice().removeUser(userId);
            }
        }

        shell(ShellCommandUtils.resetImesForAllUsers());

        shell(ShellCommandUtils.wakeUp());
        shell(ShellCommandUtils.dismissKeyguard());
        shell(ShellCommandUtils.closeSystemDialog());
    }

    /**
     * Make sure that InputMethodManagerService automatically updates its internal IME list upon
     * IME APK installation for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testSecondaryUserFull() throws Exception {
        testSecondaryUser(false);
    }

    /**
     * Make sure that InputMethodManagerService automatically updates its internal IME list upon
     * IME APK installation for instant apps.
     */
    @AppModeInstant
    @Test
    public void testSecondaryUserInstant() throws Exception {
        testSecondaryUser(true);
    }

    private void testSecondaryUser(boolean instant) throws Exception {
        final int primaryUserId = getDevice().getPrimaryUserId();
        final int secondaryUserId = getDevice().createUser(
                "InputMethodMultiUserTest_secondaryUser" + System.currentTimeMillis());

        getDevice().startUser(secondaryUserId, true /* waitFlag */);

        installPossibleInstantPackage(DeviceTestConstants.APK, primaryUserId, instant);
        installPossibleInstantPackage(DeviceTestConstants.APK, secondaryUserId, instant);

        // Work around b/31009094.
        assertTestApkIsReadyAfterInstallation(primaryUserId);

        assertIme1NotExistInApiResult(secondaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(primaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(secondaryUserId);

        installPackageAsUser(Ime1Constants.APK, true, secondaryUserId, "-r");

        assertIme1NotExistInApiResult(primaryUserId);
        assertIme1ExistsInApiResult(secondaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(primaryUserId);
        assertIme1ImplicitlyEnabledSubtypeExists(secondaryUserId);

        switchUser(secondaryUserId);

        assertIme1NotExistInApiResult(primaryUserId);
        assertIme1ExistsInApiResult(secondaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(primaryUserId);
        assertIme1ImplicitlyEnabledSubtypeExists(secondaryUserId);

        switchUser(primaryUserId);

        assertIme1NotExistInApiResult(primaryUserId);
        assertIme1ExistsInApiResult(secondaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(primaryUserId);
        assertIme1ImplicitlyEnabledSubtypeExists(secondaryUserId);
    }

    /**
     * Make sure that InputMethodManagerService automatically updates its internal IME list upon
     * IME APK installation for full (non-instant) apps.
     */
    @AppModeFull
    @Test
    public void testProfileUserFull() throws Exception {
        testProfileUser(false);
    }

    /**
     * Make sure that InputMethodManagerService automatically updates its internal IME list upon
     * IME APK installation for instant apps.
     */
    @AppModeInstant
    @Test
    public void testProfileUserInstant() throws Exception {
        testProfileUser(true);
    }

    private void testProfileUser(boolean instant) throws Exception {
        assumeTrue(getDevice().hasFeature("android.software.managed_users"));

        final int primaryUserId = getDevice().getPrimaryUserId();
        final int profileUserId = createProfile(primaryUserId);

        getDevice().startUser(profileUserId, true /* waitFlag */);

        installPossibleInstantPackage(DeviceTestConstants.APK, primaryUserId, instant);
        installPossibleInstantPackage(DeviceTestConstants.APK, profileUserId, instant);

        // Work around b/31009094.
        assertTestApkIsReadyAfterInstallation(profileUserId);

        assertIme1NotExistInApiResult(primaryUserId);
        assertIme1NotExistInApiResult(profileUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(primaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(profileUserId);

        // Install IME1 then enable/set it as the current IME for the primary user.
        installPackageAsUser(Ime1Constants.APK, true, primaryUserId, "-r");
        waitUntilImeIsInShellCommandResult(Ime1Constants.IME_ID, primaryUserId);
        shell(ShellCommandUtils.enableIme(Ime1Constants.IME_ID, primaryUserId));
        shell(ShellCommandUtils.setCurrentImeSync(Ime1Constants.IME_ID, primaryUserId));

        // Install IME2 then enable/set it as the current IME for the profile user.
        installPackageAsUser(Ime2Constants.APK, true, profileUserId, "-r");
        waitUntilImeIsInShellCommandResult(Ime2Constants.IME_ID, profileUserId);
        shell(ShellCommandUtils.enableIme(Ime2Constants.IME_ID, profileUserId));
        shell(ShellCommandUtils.setCurrentImeSync(Ime2Constants.IME_ID, profileUserId));

        // Primary User: IME1:enabled, IME2:N/A
        assertIme1ExistsInApiResult(primaryUserId);
        assertIme1EnabledInApiResult(primaryUserId);
        assertIme2NotExistInApiResult(primaryUserId);
        assertIme2NotEnabledInApiResult(primaryUserId);
        assertIme1Selected(primaryUserId);

        // Profile User: IME1:N/A, IME2:enabled
        assertIme1NotExistInApiResult(profileUserId);
        assertIme1NotEnabledInApiResult(profileUserId);
        assertIme2ExistsInApiResult(profileUserId);
        assertIme2EnabledInApiResult(profileUserId);
        assertIme2Selected(profileUserId);

        // Make sure that IME switches depending on the target user.
        runTestAsUser(DeviceTestConstants.TEST_CONNECTING_TO_THE_SAME_USER_IME, primaryUserId);
        runTestAsUser(DeviceTestConstants.TEST_CONNECTING_TO_THE_SAME_USER_IME, profileUserId);
        runTestAsUser(DeviceTestConstants.TEST_CONNECTING_TO_THE_SAME_USER_IME, primaryUserId);

        assertIme1ImplicitlyEnabledSubtypeExists(primaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(profileUserId);

        assertIme1ExistsInApiResult(primaryUserId);
        assertIme1NotExistInApiResult(profileUserId);
        assertIme1ImplicitlyEnabledSubtypeExists(primaryUserId);
        assertIme1ImplicitlyEnabledSubtypeNotExist(profileUserId);
    }

    private String shell(String command) {
        try {
            return getDevice().executeShellCommand(command).trim();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * A convenient wrapper for {@link com.android.tradefed.device.ITestDevice#switchUser(int)}
     * that also makes sure that InputMethodManagerService actually receives the new user ID.
     *
     * @param userId user ID to switch to.
     */
    private void switchUser(int userId) throws Exception {
        getDevice().switchUser(userId);
        final long initialTime = System.currentTimeMillis();
        while (true) {
            final CommandResult result = getDevice().executeShellV2Command(
                    ShellCommandUtils.getLastSwitchUserId(), USER_SWITCH_TIMEOUT,
                    TimeUnit.MILLISECONDS);
            if (result.getStatus() != CommandStatus.SUCCESS) {
                throw new IllegalStateException(
                        "Failed to get last SwitchUser ID from InputMethodManagerService."
                        + " result.getStatus()=" + result.getStatus());
            }
            final String[] lines = result.getStdout().split("\\r?\\n");
            if (lines.length < 1) {
                throw new IllegalStateException(
                        "Failed to get last SwitchUser ID from InputMethodManagerService."
                                + " result=" + result);
            }
            final int lastSwitchUserId = Integer.parseInt(lines[0], 10);
            if (userId == lastSwitchUserId) {
                // InputMethodManagerService.Lifecycle#onSwitchUser() gets called.  Ready to go.
                return;
            }
            if (System.currentTimeMillis() > initialTime + USER_SWITCH_TIMEOUT) {
                throw new TimeoutException(
                        "Failed to get last SwitchUser ID from InputMethodManagerService.");
            }
            // InputMethodManagerService did not receive onSwitchUser() yet.
            try {
                Thread.sleep(USER_SWITCH_POLLING_INTERVAL);
            } catch (InterruptedException e) {
                throw new IllegalStateException("Sleep interrupted while obtaining last SwitchUser"
                        + " ID from InputMethodManagerService.");
            }
        }
    }

    private void installPossibleInstantPackage(String apkFileName, int userId, boolean instant)
            throws Exception {
        if (instant) {
            installPackageAsUser(apkFileName, true, userId, "-r", "--instant");
        } else {
            installPackageAsUser(apkFileName, true, userId, "-r");
        }
    }

    private int createProfile(int parentUserId) throws Exception {
        final String command = ShellCommandUtils.createManagedProfileUser(parentUserId,
                "InputMethodMultiUserTest_testProfileUser" + System.currentTimeMillis());
        final String output = getDevice().executeShellCommand(command);

        if (output.startsWith("Success")) {
            try {
                return Integer.parseInt(output.substring(output.lastIndexOf(" ")).trim());
            } catch (NumberFormatException e) {
            }
        }
        throw new IllegalStateException();
    }

    private void waitUntilImeIsInShellCommandResult(String imeId, int userId) throws Exception {
        final String command = ShellCommandUtils.getAvailableImes(userId);
        pollingCheck(() -> Arrays.stream(shell(command).split("\n")).anyMatch(imeId::equals),
                IME_COMMAND_TIMEOUT, imeId + " is not found for user #" + userId
                        + " within timeout.");
    }

    private void assertTestApkIsReadyAfterInstallation(int userId) throws Exception {
        for (int i = 0; i < NO_OP_TEST_RETRY_COUNT_AFTER_APK_INSTALL; ++i) {
            try {
                // This test should never fail.  If this fails, it means that the system was not yet
                // ready to run tests in this APK.
                runTestAsUser(DeviceTestConstants.TEST_WAIT_15SEC, userId);
                return;
            } catch (AssertionError e) {
                // Ignoring because it can be because of Bug 132082599.
            }
        }
        runTestAsUser(DeviceTestConstants.TEST_WAIT_15SEC, userId);
    }


    private void assertIme1ExistsInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME1_IN_INPUT_METHOD_LIST, userId);
    }

    private void assertIme1NotExistInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME1_NOT_IN_INPUT_METHOD_LIST, userId);
    }

    private void assertIme1EnabledInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME1_IN_ENABLED_INPUT_METHOD_LIST, userId);
    }

    private void assertIme1NotEnabledInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME1_NOT_IN_ENABLED_INPUT_METHOD_LIST, userId);
    }

    private void assertIme1Selected(int userId)  {
        assertEquals(Ime1Constants.IME_ID, shell(ShellCommandUtils.getCurrentIme(userId)));
    }

    private void assertIme2ExistsInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME2_IN_INPUT_METHOD_LIST, userId);
    }

    private void assertIme2NotExistInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME2_NOT_IN_INPUT_METHOD_LIST, userId);
    }

    private void assertIme2EnabledInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME2_IN_ENABLED_INPUT_METHOD_LIST, userId);
    }

    private void assertIme2NotEnabledInApiResult(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME2_NOT_IN_ENABLED_INPUT_METHOD_LIST, userId);
    }

    private void assertIme2Selected(int userId)  {
        assertEquals(Ime2Constants.IME_ID, shell(ShellCommandUtils.getCurrentIme(userId)));
    }

    private void assertIme1ImplicitlyEnabledSubtypeExists(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME1_IMPLICITLY_ENABLED_SUBTYPE_EXISTS, userId);
    }

    private void assertIme1ImplicitlyEnabledSubtypeNotExist(int userId) throws Exception  {
        runTestAsUser(DeviceTestConstants.TEST_IME1_IMPLICITLY_ENABLED_SUBTYPE_NOT_EXIST, userId);
    }

    private void runTestAsUser(TestInfo testInfo, int userId) throws Exception {
        runDeviceTests(new DeviceTestRunOptions(testInfo.testPackage)
                .setDevice(getDevice())
                .setTestClassName(testInfo.testClass)
                .setTestMethodName(testInfo.testMethod)
                .setUserId(userId));
    }
}
