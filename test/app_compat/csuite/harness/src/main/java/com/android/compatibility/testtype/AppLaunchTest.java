/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.android.compatibility.testtype;

import static com.google.common.base.Preconditions.checkArgument;
import static com.google.common.base.Preconditions.checkNotNull;

import com.android.compatibility.FailureCollectingListener;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.LogcatReceiver;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.metrics.proto.MetricMeasurement.Metric;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.CompatibilityTestResult;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.testtype.InstrumentationTest;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.StreamUtil;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Strings;

import org.json.JSONException;
import org.junit.Assert;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/** A test that verifies that a single app can be successfully launched. */
public class AppLaunchTest
        implements IDeviceTest, IRemoteTest, IConfigurationReceiver, ITestFilterReceiver {

    @Option(name = "package-name", description = "Package name of testing app.")
    private String mPackageName;

    @Option(name = "test-label", description = "Unique test identifier label.")
    private String mTestLabel = "AppCompatibility";

    /** @deprecated */
    @Deprecated
    @Option(
            name = "retry-count",
            description = "Number of times to retry a failed test case. 0 means no retry.")
    private int mRetryCount = 0;

    @Option(name = "include-filter", description = "The include filter of the test type.")
    protected Set<String> mIncludeFilters = new HashSet<>();

    @Option(name = "exclude-filter", description = "The exclude filter of the test type.")
    protected Set<String> mExcludeFilters = new HashSet<>();

    @Option(
            name = "app-launch-timeout-ms",
            description = "Time to wait for app to launch in msecs.")
    private int mAppLaunchTimeoutMs = 15000;

    private static final String LAUNCH_TEST_RUNNER =
            "com.android.compatibilitytest.AppCompatibilityRunner";
    private static final String LAUNCH_TEST_PACKAGE = "com.android.compatibilitytest";
    private static final String PACKAGE_TO_LAUNCH = "package_to_launch";
    private static final String APP_LAUNCH_TIMEOUT_LABEL = "app_launch_timeout_ms";
    private static final int LOGCAT_SIZE_BYTES = 20 * 1024 * 1024;

    private ITestDevice mDevice;
    private LogcatReceiver mLogcat;
    private IConfiguration mConfiguration;

    public AppLaunchTest() {}

    @VisibleForTesting
    public AppLaunchTest(String packageName) {
        mPackageName = packageName;
    }

    @VisibleForTesting
    public AppLaunchTest(String packageName, int retryCount) {
        mPackageName = packageName;
        mRetryCount = retryCount;
    }

    /**
     * Creates and sets up an instrumentation test with information about the test runner as well as
     * the package being tested (provided as a parameter).
     */
    protected InstrumentationTest createInstrumentationTest(String packageBeingTested) {
        InstrumentationTest instrTest = new InstrumentationTest();

        instrTest.setPackageName(LAUNCH_TEST_PACKAGE);
        instrTest.setConfiguration(mConfiguration);
        instrTest.addInstrumentationArg(PACKAGE_TO_LAUNCH, packageBeingTested);
        instrTest.setRunnerName(LAUNCH_TEST_RUNNER);
        instrTest.setDevice(mDevice);
        instrTest.addInstrumentationArg(
                APP_LAUNCH_TIMEOUT_LABEL, Integer.toString(mAppLaunchTimeoutMs));

        return instrTest;
    }

    /*
     * {@inheritDoc}
     */
    @Override
    public void run(final TestInformation testInfo, final ITestInvocationListener listener)
            throws DeviceNotAvailableException {
        CLog.d("Start of run method.");
        CLog.d("Include filters: %s", mIncludeFilters);
        CLog.d("Exclude filters: %s", mExcludeFilters);

        Assert.assertNotNull("Package name cannot be null", mPackageName);

        TestDescription testDescription = createTestDescription();

        if (!inFilter(testDescription.toString())) {
            CLog.d("Test case %s doesn't match any filter", testDescription);
            return;
        }
        CLog.d("Complete filtering test case: %s", testDescription);

        long start = System.currentTimeMillis();
        listener.testRunStarted(mTestLabel, 1);
        mLogcat = new LogcatReceiver(getDevice(), LOGCAT_SIZE_BYTES, 0);
        mLogcat.start();

        try {
            testPackage(testInfo, testDescription, listener);
        } catch (InterruptedException e) {
            CLog.e(e);
            throw new RuntimeException(e);
        } finally {
            mLogcat.stop();
            listener.testRunEnded(
                    System.currentTimeMillis() - start, new HashMap<String, Metric>());
        }
    }

    /**
     * Attempts to test a package and reports the results.
     *
     * @param listener The {@link ITestInvocationListener}.
     * @throws DeviceNotAvailableException
     */
    private void testPackage(
            final TestInformation testInfo,
            TestDescription testDescription,
            ITestInvocationListener listener)
            throws DeviceNotAvailableException, InterruptedException {
        CLog.d("Started testing package: %s.", mPackageName);

        listener.testStarted(testDescription, System.currentTimeMillis());

        CompatibilityTestResult result = createCompatibilityTestResult();
        result.packageName = mPackageName;

        try {
            for (int i = 0; i <= mRetryCount; i++) {
                result.status = null;
                result.message = null;
                // Clear test result between retries.
                launchPackage(testInfo, result);
                if (result.status == CompatibilityTestResult.STATUS_SUCCESS) {
                    return;
                }
            }
        } finally {
            reportResult(listener, testDescription, result);
            stopPackage();
            try {
                postLogcat(result, listener);
            } catch (JSONException e) {
                CLog.w("Posting failed: %s.", e.getMessage());
            }
            listener.testEnded(
                    testDescription,
                    System.currentTimeMillis(),
                    Collections.<String, String>emptyMap());

            CLog.d("Completed testing package: %s.", mPackageName);
        }
    }

    /**
     * Method which attempts to launch a package.
     *
     * <p>Will set the result status to success if the package could be launched. Otherwise the
     * result status will be set to failure.
     *
     * @param result the {@link CompatibilityTestResult} containing the package info.
     * @throws DeviceNotAvailableException
     */
    private void launchPackage(final TestInformation testInfo, CompatibilityTestResult result)
            throws DeviceNotAvailableException {
        CLog.d("Launching package: %s.", result.packageName);

        CommandResult resetResult = resetPackage();
        if (resetResult.getStatus() != CommandStatus.SUCCESS) {
            result.status = CompatibilityTestResult.STATUS_ERROR;
            result.message = resetResult.getStatus() + resetResult.getStderr();
            return;
        }

        InstrumentationTest instrTest = createInstrumentationTest(result.packageName);

        FailureCollectingListener failureListener = createFailureListener();
        instrTest.run(testInfo, failureListener);
        CLog.d("Stack Trace: %s", failureListener.getStackTrace());

        if (failureListener.getStackTrace() != null) {
            CLog.w("Failed to launch package: %s.", result.packageName);
            result.status = CompatibilityTestResult.STATUS_FAILURE;
            result.message = failureListener.getStackTrace();
        } else {
            result.status = CompatibilityTestResult.STATUS_SUCCESS;
        }

        CLog.d("Completed launching package: %s", result.packageName);
    }

    /** Helper method which reports a test failed if the status is either a failure or an error. */
    private void reportResult(
            ITestInvocationListener listener, TestDescription id, CompatibilityTestResult result) {
        String message = result.message != null ? result.message : "unknown";
        String tag = errorStatusToTag(result.status);
        if (tag != null) {
            listener.testFailed(id, result.status + ":" + message);
        }
    }

    private String errorStatusToTag(String status) {
        if (status.equals(CompatibilityTestResult.STATUS_ERROR)) {
            return "ERROR";
        }
        if (status.equals(CompatibilityTestResult.STATUS_FAILURE)) {
            return "FAILURE";
        }
        return null;
    }

    /** Helper method which posts the logcat. */
    private void postLogcat(CompatibilityTestResult result, ITestInvocationListener listener)
            throws JSONException {
        InputStreamSource stream = null;
        String header =
                String.format(
                        "%s%s%s\n",
                        CompatibilityTestResult.SEPARATOR,
                        result.toJsonString(),
                        CompatibilityTestResult.SEPARATOR);

        try (InputStreamSource logcatData = mLogcat.getLogcatData()) {
            try (ByteArrayOutputStream baos = new ByteArrayOutputStream()) {
                baos.write(header.getBytes());
                StreamUtil.copyStreams(logcatData.createInputStream(), baos);
                stream = new ByteArrayInputStreamSource(baos.toByteArray());
            } catch (IOException e) {
                CLog.e("error inserting compatibility test result into logcat");
                CLog.e(e);
                // fallback to logcat data
                stream = logcatData;
            }
            listener.testLog("logcat_" + result.packageName, LogDataType.LOGCAT, stream);
        } finally {
            StreamUtil.cancel(stream);
        }
    }

    /**
     * Return true if a test matches one or more of the include filters AND does not match any of
     * the exclude filters. If no include filters are given all tests should return true as long as
     * they do not match any of the exclude filters.
     */
    protected boolean inFilter(String testName) {
        if (mExcludeFilters.contains(testName)) {
            return false;
        }
        if (mIncludeFilters.size() == 0 || mIncludeFilters.contains(testName)) {
            return true;
        }
        return false;
    }

    protected CommandResult resetPackage() throws DeviceNotAvailableException {
        return mDevice.executeShellV2Command(String.format("pm clear %s", mPackageName));
    }

    private void stopPackage() throws DeviceNotAvailableException {
        mDevice.executeShellCommand(String.format("am force-stop %s", mPackageName));
    }

    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    /*
     * {@inheritDoc}
     */
    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /*
     * {@inheritDoc}
     */
    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    public int getmRetryCount() {
        return mRetryCount;
    }

    /**
     * Get a test description for use in logging. For compatibility with logs, this should be
     * TestDescription(test class name, test type).
     */
    private TestDescription createTestDescription() {
        return new TestDescription(getClass().getSimpleName(), mPackageName);
    }

    /** Get a FailureCollectingListener for failure listening. */
    private FailureCollectingListener createFailureListener() {
        return new FailureCollectingListener();
    }

    /**
     * Get a CompatibilityTestResult for encapsulating compatibility run results for a single app
     * package tested.
     */
    private CompatibilityTestResult createCompatibilityTestResult() {
        return new CompatibilityTestResult();
    }

    /** {@inheritDoc} */
    @Override
    public void addIncludeFilter(String filter) {
        checkArgument(!Strings.isNullOrEmpty(filter), "Include filter cannot be null or empty.");
        mIncludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllIncludeFilters(Set<String> filters) {
        checkNotNull(filters, "Include filters cannot be null.");
        mIncludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public void clearIncludeFilters() {
        mIncludeFilters.clear();
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getIncludeFilters() {
        return Collections.unmodifiableSet(mIncludeFilters);
    }

    /** {@inheritDoc} */
    @Override
    public void addExcludeFilter(String filter) {
        checkArgument(!Strings.isNullOrEmpty(filter), "Exclude filter cannot be null or empty.");
        mExcludeFilters.add(filter);
    }

    /** {@inheritDoc} */
    @Override
    public void addAllExcludeFilters(Set<String> filters) {
        checkNotNull(filters, "Exclude filters cannot be null.");
        mExcludeFilters.addAll(filters);
    }

    /** {@inheritDoc} */
    @Override
    public void clearExcludeFilters() {
        mExcludeFilters.clear();
    }

    /** {@inheritDoc} */
    @Override
    public Set<String> getExcludeFilters() {
        return Collections.unmodifiableSet(mExcludeFilters);
    }
}
