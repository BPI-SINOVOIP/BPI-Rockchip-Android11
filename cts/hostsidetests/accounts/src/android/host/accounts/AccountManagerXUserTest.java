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
 * limitations under the License
 */

package android.host.accounts;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.SystemUserOnly;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.ddmlib.testrunner.RemoteAndroidTestRunner;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.IBuildReceiver;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Tests adding accounts for user profile
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@SystemUserOnly
@AppModeFull(reason = "instant applications cannot see any other application")
public class AccountManagerXUserTest extends BaseMultiUserTest implements IBuildReceiver {

    private static final String TEST_WITH_PERMISSION_APK =
            "CtsAccountManagerCrossUserApp.apk";
    private static final String TEST_WITH_PERMISSION_PKG =
            "com.android.cts.accountmanager";

    public static final int USER_SYSTEM = 0;

    private String mOldVerifierValue;
    private IBuildInfo mCtsBuild;

    private int mParentUserId;
    private int mProfileUserId;
    private final Map<String, String> mTestArgs = new HashMap<>();

    @Before
    public void setUp() throws Exception {
        super.setUp();

        assumeTrue(mSupportsMultiUser && mSupportsManagedUsers);

        mOldVerifierValue =
                getDevice().executeShellCommand("settings get global package_verifier_enable");
        getDevice().executeShellCommand("settings put global package_verifier_enable 0");

        mParentUserId = getDevice().getCurrentUser();

        assertThat(mParentUserId).isEqualTo(USER_SYSTEM);

        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        File apkFile = buildHelper.getTestFile(TEST_WITH_PERMISSION_APK);
        getDevice().installPackageForUser(apkFile, true, true, mParentUserId, "-t");

        mProfileUserId = createProfile(mParentUserId);
        getDevice().startUser(mProfileUserId, true);
        getDevice().installPackageForUser(apkFile, true, true, mProfileUserId, "-t");

        mTestArgs.put("testUser", Integer.toString(mProfileUserId));
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(TEST_WITH_PERMISSION_PKG);
        getDevice().executeShellCommand("settings put global package_verifier_enable "
                + mOldVerifierValue);

        mTestArgs.clear();

        super.tearDown();
    }

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    @Test
    public void testAddMockAccountForUserProfile() throws Exception {
        // add account from parentUserId for profileUserId
        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".AccountManagerCrossUserTest",
                "testAccountManager_addMockAccount",
                mParentUserId,
                mTestArgs,
                60L,
                TimeUnit.SECONDS);

        // get accounts as parentUserId (should fail) => 0 accounts
        mTestArgs.put("numAccountsExpected", Integer.toString(0));
        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".AccountManagerCrossUserTest",
                "testAccountManager_getAccountsForCurrentUser",
                mParentUserId,
                mTestArgs,
                60L,
                TimeUnit.SECONDS);

        // get accounts as profileUserId (should succeed) => 1 account
        mTestArgs.put("numAccountsExpected", Integer.toString(1));
        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".AccountManagerCrossUserTest",
                "testAccountManager_getAccountsForCurrentUser",
                mProfileUserId,
                mTestArgs,
                60L,
                TimeUnit.SECONDS);

    }

    @Test
    public void testGetAccountsForUserProfile() throws Exception {
        // need to run addAccountExplicitly from profile user
        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".AccountManagerCrossUserTest",
                "testAccountManager_addAccountExplicitlyForCurrentUser",
                mProfileUserId,
                mTestArgs,
                60L,
                TimeUnit.SECONDS);

        // run getAccounts from initial user
        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".AccountManagerCrossUserTest",
                "testAccountManager_getAccountsForTestUser",
                mParentUserId,
                mTestArgs,
                60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testCreateContextAsUser_revokePermissions() throws Exception {
        // should throw SecurityException when INTERACT_ACROSS_USERS is revoked
        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".AccountManagerCrossUserTest",
                "testAccountManager_failCreateContextAsUserRevokePermissions",
                mParentUserId,
                mTestArgs,
                60L,
                TimeUnit.SECONDS);
    }

    @Test
    public void testCreateContextAsUserForSameUser_revokePermissions() throws Exception {
        // should create context even if INTERACT_ACROSS_USERS is revoked (same user)
        runDeviceTests(
                getDevice(),
                TEST_WITH_PERMISSION_PKG,
                ".AccountManagerCrossUserTest",
                "testAccountManager_createContextAsUserForCurrentUserRevokePermissions",
                mProfileUserId,
                mTestArgs,
                60L,
                TimeUnit.SECONDS);
    }

    public static void runDeviceTests(ITestDevice device, String packageName, String testClassName,
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
                if (!resultEntry.getValue().getStatus().equals(TestStatus.PASSED)) {
                    errorBuilder.append(resultEntry.getKey().toString());
                    errorBuilder.append(":\n");
                    errorBuilder.append(resultEntry.getValue().getStackTrace());
                }
            }
            throw new AssertionError(errorBuilder.toString());
        }
    }

}
