/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.atrace.cts;

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

import trebuchet.io.BufferProducer;
import trebuchet.io.DataSlice;
import trebuchet.model.Model;
import trebuchet.task.ImportTask;
import trebuchet.util.PrintlnImportFeedback;

import java.io.FileNotFoundException;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.concurrent.TimeUnit;

public class AtraceHostTestBase extends DeviceTestCase implements IBuildReceiver {
    private static final String TEST_RUNNER = "androidx.test.runner.AndroidJUnitRunner";
    private static final String TEST_APK = "CtsAtraceTestApp.apk";
    // TODO: Make private
    protected static final String TEST_PKG = "com.android.cts.atracetestapp";
    private static final String TEST_CLASS = "com.android.cts.atracetestapp.AtraceDeviceTests";

    private static final String START_TRACE_CMD = "atrace --async_start -a \\* -c -b 16000";
    private static final String START_TRACE_NO_APP_CMD = "atrace --async_start -c -b 16000";
    private static final String STOP_TRACE_CMD = "atrace --async_stop";

    private IBuildInfo mCtsBuild;
    private boolean mIsInstalled = false;

    /**
     * {@inheritDoc}
     */
    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mCtsBuild = buildInfo;
    }

    /**
     * Install a device side test package.
     *
     * @param appFileName Apk file name, such as "CtsNetStatsApp.apk".
     * @param grantPermissions whether to give runtime permissions.
     */
    private void installPackage(String appFileName, boolean grantPermissions)
            throws FileNotFoundException, DeviceNotAvailableException {
        LogUtil.CLog.d("Installing app " + appFileName);
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(mCtsBuild);
        final String result = getDevice().installPackage(
                buildHelper.getTestFile(appFileName), true, grantPermissions);
        assertNull("Failed to install " + appFileName + ": " + result, result);
    }

    private final void requireApk() {
        if (mIsInstalled) return;
        try {
            System.out.println("Installing APK");
            installPackage(TEST_APK, true);
            mIsInstalled = true;
        } catch (FileNotFoundException | DeviceNotAvailableException e) {
            e.printStackTrace();
            throw new RuntimeException(e);
        }
    }

    protected final void turnScreenOn() {
        shell("input keyevent KEYCODE_WAKEUP");
        shell("wm dismiss-keyguard");
    }

    @Override
    public void run(junit.framework.TestResult result) {
        try {
            super.run(result);
        } finally {
            try {
                // We don't have the equivalent of @AfterClass, but this basically does that
                System.out.println("Uninstalling APK");
                getDevice().uninstallPackage(TEST_PKG);
                mIsInstalled = false;
            } catch (DeviceNotAvailableException e) {}
        }
    }

    @Override
    protected void setUp() throws Exception {
        try {
            shell("atrace --async_stop");
        } finally {
            super.setUp();
        }
    }

    @Override
    protected void tearDown() throws Exception {
        try {
            shell("atrace --async_stop");
        } finally {
            super.tearDown();
        }
    }

    /**
     * Run a device side test.
     *
     * @param pkgName Test package name, such as "com.android.server.cts.netstats".
     * @param testClassName Test class name; either a fully qualified name, or "." + a class name.
     * @param testMethodName Test method name.
     * @throws DeviceNotAvailableException
     */
    private PidTidPair runDeviceTests(String pkgName,
            String testClassName,  String testMethodName)
            throws DeviceNotAvailableException {
        if (testClassName != null && testClassName.startsWith(".")) {
            testClassName = pkgName + testClassName;
        }

        RemoteAndroidTestRunner testRunner = new RemoteAndroidTestRunner(
                pkgName, TEST_RUNNER, getDevice().getIDevice());
        testRunner.setMaxTimeout(60, TimeUnit.SECONDS);
        if (testClassName != null && testMethodName != null) {
            testRunner.setMethodName(testClassName, testMethodName);
        } else if (testClassName != null) {
            testRunner.setClassName(testClassName);
        }

        CollectingTestListener listener = new CollectingTestListener();
        assertTrue(getDevice().runInstrumentationTests(testRunner, listener));

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
            StringBuilder errorBuilder = new StringBuilder("On-device tests failed:\n");
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
        return new PidTidPair(result);
    }

    private final PidTidPair runAppTest(String testname) {
        requireApk();
        try {
            return runDeviceTests(TEST_PKG, TEST_CLASS, testname);
        } catch (DeviceNotAvailableException e) {
            throw new RuntimeException(e);
        }
    }

    protected final String shell(String command, String... args) {
        if (args != null && args.length > 0) {
            command += " " + String.join(" ", args);
        }
        try {
            return getDevice().executeShellCommand(command);
        } catch (DeviceNotAvailableException ex) {
            throw new RuntimeException(ex);
        }
    }

    private static class StringAdapter implements BufferProducer {
        private byte[] data;
        private boolean hasRead = false;

        StringAdapter(String str) {
            this.data = str.getBytes(StandardCharsets.UTF_8);
        }

        @Override
        public DataSlice next() {
            if (!hasRead) {
                hasRead = true;
                return new DataSlice(data);
            }
            return null;
        }

        @Override
        public void close() {
            hasRead = true;
        }
    }

    private Model parse(String traceOutput) {
        ImportTask importTask = new ImportTask(new PrintlnImportFeedback());
        return importTask.importTrace(new StringAdapter(traceOutput));
    }

    protected final PidTidPair runSingleAppTest(AtraceDeviceTestList test) {
        return runAppTest(test.toString());
    }

    protected final TraceResult traceSingleTest(AtraceDeviceTestList test, boolean withAppTracing,
            String... categories) {
        requireApk();
        shell(withAppTracing ? START_TRACE_CMD : START_TRACE_NO_APP_CMD, categories);
        PidTidPair pidTid = runSingleAppTest(test);
        String traceOutput = shell("atrace --async_stop", categories);
        assertNotNull("unable to capture atrace output", traceOutput);
        return new TraceResult(pidTid, parse(traceOutput));
    }

    protected final TraceResult traceSingleTest(AtraceDeviceTestList test, String... categories) {
        return traceSingleTest(test, true, categories);
    }
}
