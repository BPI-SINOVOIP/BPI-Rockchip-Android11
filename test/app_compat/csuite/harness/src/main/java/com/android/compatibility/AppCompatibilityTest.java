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

package com.android.compatibility;

import static com.google.common.base.Preconditions.checkArgument;
import static com.google.common.base.Preconditions.checkNotNull;

import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionCopier;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceUnresponsiveException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.LogcatReceiver;
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
import com.android.tradefed.testtype.IShardableTest;
import com.android.tradefed.testtype.ITestFilterReceiver;
import com.android.tradefed.testtype.InstrumentationTest;
import com.android.tradefed.util.AaptParser;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.PublicApkUtil;
import com.android.tradefed.util.PublicApkUtil.ApkInfo;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.StreamUtil;

import com.google.common.base.Strings;

import org.json.JSONException;
import org.junit.Assert;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Test that determines application compatibility. The test iterates through the apks in a given
 * directory. The test installs, launches, and uninstalls each apk.
 */
public abstract class AppCompatibilityTest
        implements IDeviceTest,
                IRemoteTest,
                IShardableTest,
                IConfigurationReceiver,
                ITestFilterReceiver {

    @Option(
            name = "product",
            description = "The product, corresponding to the borgcron job product arg.")
    private String mProduct;

    @Option(
            name = "base-dir",
            description = "The directory of the results excluding the date.",
            importance = Option.Importance.ALWAYS)
    // TODO(b/36786754): Add `mandatory = true` when cmdfiles are moved over
    private File mBaseDir;

    @Option(
            name = "date",
            description =
                    "The date to run, in the form YYYYMMDD. If not set, then the latest "
                            + "results will be used.")
    private String mDate;

    @Option(name = "test-label", description = "Unique test identifier label.")
    private String mTestLabel = "AppCompatibility";

    @Option(
            name = "reboot-after-apks",
            description = "Reboot the device after a centain number of apks. 0 means no reboot.")
    private int mRebootNumber = 100;

    @Option(
            name = "fallback-to-apk-scan",
            description =
                    "Fallback to scanning for apks in base directory if ranking information "
                            + "is missing.")
    private boolean mFallbackToApkScan = false;

    @Option(
            name = "retry-count",
            description = "Number of times to retry a failed test case. 0 means no retry.")
    private int mRetryCount = 5;

    @Option(name = "include-filter", description = "The include filter of the test names to run.")
    protected Set<String> mIncludeFilters = new HashSet<>();

    @Option(name = "exclude-filter", description = "The exclude filter of the test names to run.")
    protected Set<String> mExcludeFilters = new HashSet<>();

    private static final long DOWNLOAD_TIMEOUT_MS = 60 * 1000;
    private static final int DOWNLOAD_RETRIES = 3;
    private static final long JOIN_TIMEOUT_MS = 5 * 60 * 1000;
    private static final int LOGCAT_SIZE_BYTES = 20 * 1024 * 1024;

    private ITestDevice mDevice;
    private LogcatReceiver mLogcat;
    private IConfiguration mConfiguration;

    // The number of tests run so far
    private int mTestCount = 0;

    // indicates the current sharding setup
    private int mShardCount = 1;
    private int mShardIndex = 0;

    protected final String mLauncherPackage;
    protected final String mRunnerClass;
    protected final String mPackageBeingTestedKey;

    protected AppCompatibilityTest(
            String launcherPackage, String runnerClass, String packageBeingTestedKey) {
        this.mLauncherPackage = launcherPackage;
        this.mRunnerClass = runnerClass;
        this.mPackageBeingTestedKey = packageBeingTestedKey;
    }

    /**
     * Creates and sets up an instrumentation test with information about the test runner as well as
     * the package being tested (provided as a parameter).
     */
    protected abstract InstrumentationTest createInstrumentationTest(String packageBeingTested);

    /** Sets up some default aspects of the instrumentation test. */
    protected final InstrumentationTest createDefaultInstrumentationTest(
            String packageBeingTested) {
        InstrumentationTest instrTest = new InstrumentationTest();
        instrTest.setPackageName(mLauncherPackage);
        instrTest.setConfiguration(mConfiguration);
        instrTest.addInstrumentationArg(mPackageBeingTestedKey, packageBeingTested);
        instrTest.setRunnerName(mRunnerClass);
        return instrTest;
    }

    /*
     * {@inheritDoc}
     */
    @Override
    public final void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        CLog.d("Start of launch test run method. base-dir: %s", mBaseDir);
        Assert.assertNotNull("Base dir cannot be null", mBaseDir);
        Assert.assertTrue("Base dir should be a directory", mBaseDir.isDirectory());

        if (mProduct == null) {
            mProduct = mDevice.getProductType();
            CLog.i("\"--product\" not specified, using property from device instead: %s", mProduct);
        }
        Assert.assertTrue(
                String.format(
                        "Shard index out of range: expected [0, %d), got %d",
                        mShardCount, mShardIndex),
                mShardIndex >= 0 && mShardIndex < mShardCount);

        File apkDir = null;
        try {
            apkDir = PublicApkUtil.constructApkDir(mBaseDir.getPath(), mDate);
        } catch (IOException e) {
            CLog.e(e);
            throw new RuntimeException(e);
        }
        CLog.d("apkDir: %s.", apkDir);
        Assert.assertNotNull("Could not find the output dir", apkDir);
        List<ApkInfo> apkList = null;
        try {
            apkList = shardApkList(PublicApkUtil.getApkList(mProduct, apkDir, mFallbackToApkScan));
        } catch (IOException e) {
            CLog.e(e);
            throw new RuntimeException(e);
        }
        CLog.d("Completed sharding apkList. Number of items: %s", apkList.size());
        Assert.assertNotNull("Could not download apk list", apkList);

        apkList = filterApk(apkList);
        CLog.d("Completed filtering apkList. Number of items: %s", apkList.size());

        long start = System.currentTimeMillis();
        listener.testRunStarted(mTestLabel, apkList.size());
        mLogcat = new LogcatReceiver(getDevice(), LOGCAT_SIZE_BYTES, 0);
        mLogcat.start();

        try {
            downloadAndTestApks(listener, apkDir, apkList);
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
     * Downloads and tests all the APKs in the apk list.
     *
     * @param listener The {@link ITestInvocationListener}.
     * @param kharonDir The {@link File} of the CNS dir containing the APKs.
     * @param apkList The sharded list of {@link ApkInfo} objects.
     * @throws DeviceNotAvailableException
     * @throws InterruptedException if a download thread was interrupted.
     */
    private void downloadAndTestApks(
            ITestInvocationListener listener, File kharonDir, List<ApkInfo> apkList)
            throws DeviceNotAvailableException, InterruptedException {
        CLog.d("Started downloading and testing apks.");
        ApkInfo testingApk = null;
        File testingFile = null;
        for (ApkInfo downloadingApk : apkList) {
            ApkDownloadRunnable downloader = new ApkDownloadRunnable(kharonDir, downloadingApk);
            Thread downloadThread = new Thread(downloader);
            downloadThread.start();

            testApk(listener, testingApk, testingFile);

            try {
                downloadThread.join(JOIN_TIMEOUT_MS);
            } catch (InterruptedException e) {
                FileUtil.deleteFile(downloader.getDownloadedFile());
                throw e;
            }
            testingApk = downloadingApk;
            testingFile = downloader.getDownloadedFile();
        }
        // One more time since the first time through the loop we don't test
        testApk(listener, testingApk, testingFile);
        CLog.d("Completed downloading and testing apks.");
    }

    /**
     * Attempts to install and launch an APK and reports the results.
     *
     * @param listener The {@link ITestInvocationListener}.
     * @param apkInfo The {@link ApkInfo} to run the test against.
     * @param apkFile The downloaded {@link File}.
     * @throws DeviceNotAvailableException
     */
    private void testApk(ITestInvocationListener listener, ApkInfo apkInfo, File apkFile)
            throws DeviceNotAvailableException {
        if (apkInfo == null || apkFile == null) {
            CLog.d("apkInfo or apkFile is null.");
            FileUtil.deleteFile(apkFile);
            return;
        }
        CLog.d(
                "Started testing package: %s, apk file: %s.",
                apkInfo.packageName, apkFile.getAbsolutePath());

        mTestCount++;
        if (mRebootNumber != 0 && mTestCount % mRebootNumber == 0) {
            mDevice.reboot();
        }
        mLogcat.clear();

        TestDescription testId = createTestDescription(apkInfo.packageName);
        listener.testStarted(testId, System.currentTimeMillis());

        CompatibilityTestResult result = new CompatibilityTestResult();
        result.rank = apkInfo.rank;
        // Default to package name since name is a required field. This will be replaced by
        // AaptParser in installApk()
        result.name = apkInfo.packageName;
        result.packageName = apkInfo.packageName;
        result.versionString = apkInfo.versionString;
        result.versionCode = apkInfo.versionCode;

        try {
            // Install the app, and also skip aapt check if we've fell back to apk scan
            installApk(result, apkFile, mFallbackToApkScan);
            boolean installationSuccess = result.status == null;

            for (int i = 0; i <= mRetryCount; i++) {
                if (installationSuccess) {
                    // Clear test result between retries
                    result.status = null;
                    result.message = null;
                    launchApk(result);
                    mDevice.executeShellCommand(
                            String.format("am force-stop %s", apkInfo.packageName));
                }
                if (result.status == null) {
                    result.status = CompatibilityTestResult.STATUS_SUCCESS;
                    break;
                }
            }

            if (installationSuccess) {
                mDevice.uninstallPackage(result.packageName);
            }
        } finally {
            reportResult(listener, testId, result);
            try {
                postLogcat(result, listener);
            } catch (JSONException e) {
                CLog.w("Posting failed: %s.", e.getMessage());
            }
            listener.testEnded(
                    testId, System.currentTimeMillis(), Collections.<String, String>emptyMap());
            FileUtil.deleteFile(apkFile);
            CLog.d("Completed testing package: %s.", apkInfo.packageName);
        }
    }

    /**
     * Checks that the file is correct and attempts to install it.
     *
     * <p>Will set the result status to error if the APK could not be installed or if it contains
     * conflicting information.
     *
     * @param result the {@link CompatibilityTestResult} containing the APK info.
     * @param apkFile the APK file to install.
     * @throws DeviceNotAvailableException
     */
    private void installApk(CompatibilityTestResult result, File apkFile, boolean skipAaptCheck)
            throws DeviceNotAvailableException {
        if (!skipAaptCheck) {
            CLog.d("Parsing apk file: %s.", apkFile.getAbsolutePath());
            AaptParser parser = AaptParser.parse(apkFile);
            if (parser == null) {
                CLog.d(
                        "Failed to parse apk file: %s, package: %s, error: %s.",
                        apkFile.getAbsolutePath(), result.packageName, result.message);
                result.status = CompatibilityTestResult.STATUS_ERROR;
                result.message = "aapt fail";
                return;
            }

            result.name = parser.getLabel();

            if (!equalsOrNull(result.packageName, parser.getPackageName())
                    || !equalsOrNull(result.versionString, parser.getVersionName())
                    || !equalsOrNull(result.versionCode, parser.getVersionCode())) {
                CLog.d(
                        "Package info mismatch: want %s v%s (%s), got %s v%s (%s)",
                        result.packageName,
                        result.versionCode,
                        result.versionString,
                        parser.getPackageName(),
                        parser.getVersionCode(),
                        parser.getVersionName());
                result.status = CompatibilityTestResult.STATUS_ERROR;
                result.message = "package info mismatch";
                return;
            }
            CLog.d("Completed parsing apk file: %s.", apkFile.getAbsolutePath());
        }

        try {
            String error = mDevice.installPackage(apkFile, true);
            if (error != null) {
                result.status = CompatibilityTestResult.STATUS_ERROR;
                result.message = error;
                CLog.d(
                        "Failed to install apk file: %s, package: %s, error: %s.",
                        apkFile.getAbsolutePath(), result.packageName, result.message);
                return;
            }
        } catch (DeviceUnresponsiveException e) {
            result.status = CompatibilityTestResult.STATUS_ERROR;
            result.message = "install timeout";
            CLog.d(
                    "Installing apk file %s timed out, package: %s, error: %s.",
                    apkFile.getAbsolutePath(), result.packageName, result.message);
            return;
        }
        CLog.d("Completed installing apk file %s.", apkFile.getAbsolutePath());
    }

    /**
     * Method which attempts to launch an APK.
     *
     * <p>Will set the result status to failure if the APK could not be launched.
     *
     * @param result the {@link CompatibilityTestResult} containing the APK info.
     * @throws DeviceNotAvailableException
     */
    private void launchApk(CompatibilityTestResult result) throws DeviceNotAvailableException {
        CLog.d("Lauching package: %s.", result.packageName);
        InstrumentationTest instrTest = createInstrumentationTest(result.packageName);
        instrTest.setDevice(mDevice);

        FailureCollectingListener failureListener = new FailureCollectingListener();
        instrTest.run(failureListener);

        if (failureListener.getStackTrace() != null) {
            CLog.w("Failed to launch package: %s.", result.packageName);
            result.status = CompatibilityTestResult.STATUS_FAILURE;
            result.message = failureListener.getStackTrace();
        }

        CLog.d("Completed launching package: %s", result.packageName);
    }

    /** Helper method which reports a test failed if the status is either a failure or an error. */
    private void reportResult(
            ITestInvocationListener listener, TestDescription id, CompatibilityTestResult result) {
        String message = result.message != null ? result.message : "unknown";
        if (CompatibilityTestResult.STATUS_ERROR.equals(result.status)) {
            listener.testFailed(id, "ERROR:" + message);
        } else if (CompatibilityTestResult.STATUS_FAILURE.equals(result.status)) {
            listener.testFailed(id, "FAILURE:" + message);
        }
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
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try (InputStreamSource logcatData = mLogcat.getLogcatData()) {
            try {
                baos.write(header.getBytes());
                StreamUtil.copyStreams(logcatData.createInputStream(), baos);
                stream = new ByteArrayInputStreamSource(baos.toByteArray());
                baos.flush();
                baos.close();
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

    /** Helper method which takes a list of {@link ApkInfo} objects and returns the sharded list. */
    private List<ApkInfo> shardApkList(List<ApkInfo> apkList) {
        List<ApkInfo> shardedList = new ArrayList<>(apkList.size() / mShardCount + 1);
        for (int i = mShardIndex; i < apkList.size(); i += mShardCount) {
            shardedList.add(apkList.get(i));
        }
        return shardedList;
    }

    /**
     * Helper method which takes a list of {@link ApkInfo} objects and returns the filtered list.
     */
    protected List<ApkInfo> filterApk(List<ApkInfo> apkList) {
        List<ApkInfo> filteredList = new ArrayList<>();

        for (ApkInfo apk : apkList) {
            if (filterTest(apk.packageName)) {
                filteredList.add(apk);
            }
        }

        return filteredList;
    }

    /**
     * Return true if a test matches one or more of the include filters AND does not match any of
     * the exclude filters. If no include filters are given all tests should return true as long as
     * they do not match any of the exclude filters.
     */
    protected boolean filterTest(String testName) {
        if (mExcludeFilters.contains(testName)) {
            return false;
        }
        if (mIncludeFilters.size() == 0 || mIncludeFilters.contains(testName)) {
            return true;
        }
        return false;
    }

    /** Returns true if either object is null or if both objects are equal. */
    private static boolean equalsOrNull(Object a, Object b) {
        return a == null || b == null || a.equals(b);
    }

    /** Helper {@link Runnable} which downloads a file, and can be used in another thread. */
    private class ApkDownloadRunnable implements Runnable {
        private final File mKharonDir;
        private final ApkInfo mApkInfo;

        private File mDownloadedFile = null;

        ApkDownloadRunnable(File kharonDir, ApkInfo apkInfo) {
            mKharonDir = kharonDir;
            mApkInfo = apkInfo;
        }

        @Override
        public void run() {
            // No-op if mApkInfo is null
            if (mApkInfo == null) {
                CLog.d("ApkInfo is null.");
                return;
            }

            File sourceFile = new File(mKharonDir, mApkInfo.fileName);
            try {
                mDownloadedFile =
                        PublicApkUtil.downloadFile(
                                sourceFile, DOWNLOAD_TIMEOUT_MS, DOWNLOAD_RETRIES);
            } catch (IOException e) {
                // Log and ignore
                CLog.e("Could not download apk from %s.", sourceFile);
                CLog.e(e);
            }
            CLog.d("Completed downloading apk file: %s.", mDownloadedFile.getAbsolutePath());
        }

        public File getDownloadedFile() {
            return mDownloadedFile;
        }
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

    /** Return a {@link IRunUtil} instance to execute commands with. */
    IRunUtil getRunUtil() {
        return RunUtil.getDefault();
    }

    private IRemoteTest getTestShard(int shardCount, int shardIndex) {
        AppCompatibilityTest shard;
        try {
            shard = getClass().newInstance();
        } catch (InstantiationException | IllegalAccessException e) {
            throw new IllegalStateException(
                    "The class "
                            + getClass().getName()
                            + " has no public constructor with no arguments, but all subclasses of "
                            + AppCompatibilityTest.class.getName()
                            + " should",
                    e);
        }
        try {
            OptionCopier.copyOptions(this, shard);
        } catch (ConfigurationException e) {
            CLog.e("Failed to copy test options: %s.", e.getMessage());
        }
        shard.mShardIndex = shardIndex;
        shard.mShardCount = shardCount;
        return shard;
    }

    /** {@inheritDoc} */
    @Override
    public Collection<IRemoteTest> split(int shardCountHint) {
        if (shardCountHint <= 1) {
            // cannot shard or already sharded
            return null;
        }
        Collection<IRemoteTest> shards = new ArrayList<>(shardCountHint);
        for (int index = 0; index < shardCountHint; index++) {
            shards.add(getTestShard(shardCountHint, index));
        }
        return shards;
    }

    /**
     * Get a test description for use in logging. For compatibility with logs, this should be
     * TestDescription(launcher package, package being run).
     */
    private TestDescription createTestDescription(String packageBeingTested) {
        return new TestDescription(mLauncherPackage, packageBeingTested);
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
