/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.performance.tests;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ITestInvocationListener;
import com.android.tradefed.testtype.IDeviceTest;
import com.android.tradefed.testtype.IRemoteTest;
import com.android.tradefed.util.AaptParser;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.proto.TfMetricProtoUtil;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@OptionClass(alias = "app-incremental-install-perf")
// Test framework that measures the install time for all apk files located under a given directory.
// The test needs aapt to be in its path in order to determine the package name of the apk. The
// package name is needed to clean up after the test is done.
public class AppIncrementalInstallTest implements IDeviceTest, IRemoteTest {

    @Option(
            name = "test-apk-dir",
            description = "Directory that contains the test apks.",
            mandatory = true)
    private File mTestApkPath;

    @Option(name = "test-label", description = "Unique test identifier label.")
    private String mTestLabel = "AppIncrementalInstallPerformance";

    @Option(
            name = "test-start-delay",
            description = "Delay in ms to wait for before starting the install test.")
    private long mTestStartDelay = 60000;

    @Option(
            name = "test-delay-between-installs",
            description = "Delay in ms to wait for before starting the install test.")
    private long mTestDelayBetweenInstalls = 5000;

    @Option(name = "test-uninstall-after", description = "If the apk should be uninstalled after.")
    private boolean mUninstallAfter = true;

    @Option(
            name = "package-list",
            description =
                    "If given, filters the apk files in the test dir based on the list of "
                            + "packages. It checks that the apk name is packageName-version.apk")
    private List<String> mPackages = new ArrayList<>();

    @Option(
            name = "test-apk-remote-dir",
            description = "Directory on the device to push artefacts.")
    private String mTestApkRemoteDir = "/data/local/tmp/";

    private ITestDevice mDevice;

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

    /*
     * {@inheritDoc}
     */
    @Override
    public void run(ITestInvocationListener listener) throws DeviceNotAvailableException {
        // Delay test start time to give the background processes to finish.
        if (mTestStartDelay > 0) {
            RunUtil.getDefault().sleep(mTestStartDelay);
        }

        assert mTestApkPath.isDirectory();

        // Find all apks in directory.
        String[] files = mTestApkPath.list();
        Map<String, String> metrics = new HashMap<>();
        try {
            for (String fileName : files) {
                if (!fileName.endsWith(".apk")) {
                    CLog.d("Skipping non-apk %s", fileName);
                    continue;
                } else if (!matchesPackagesForInstall(fileName)) {
                    CLog.d("Skipping apk %s", fileName);
                    continue;
                }
                File file = new File(mTestApkPath, fileName);
                // Install app and measure time.
                long installTime = installAndTime(file);
                if (installTime > 0) {
                    metrics.put(fileName, Long.toString(installTime));
                }
                RunUtil.getDefault().sleep(mTestDelayBetweenInstalls);
            }
        } finally {
            reportMetrics(listener, mTestLabel, metrics);
        }
    }

    /**
     * Install file and time its install time. Cleans up after itself.
     *
     * @param packageFile apk file to install
     * @return install time in msecs.
     * @throws DeviceNotAvailableException
     */
    long installAndTime(File packageFile) throws DeviceNotAvailableException {
        AaptParser parser = AaptParser.parse(packageFile);
        if (parser == null) {
            CLog.e("Failed to parse %s", packageFile);
            return -1;
        }

        String packageName = parser.getPackageName();
        if (packageName == null || packageName.length() == 0) {
            CLog.e("Failed to obtain package name %s", packageFile);
            return -1;
        }

        String remotePath = mTestApkRemoteDir + packageFile.getName();
        if (!mDevice.pushFile(packageFile, remotePath)) {
            CLog.e("Failed to push %s", packageFile);
            return -1;
        }

        File signatureFile = getSignatureFile(packageFile);
        String idsigRemotePath = mTestApkRemoteDir + signatureFile.getName();
        if (!mDevice.pushFile(signatureFile, idsigRemotePath)) {
            CLog.e("Failed to push %s", signatureFile);
            return -1;
        }

        long start = System.currentTimeMillis();

        // Install using incremental.
        String output =
                mDevice.executeShellCommand("pm install-incremental -r -d -g " + remotePath);
        if (!checkSuccess(output, packageFile, "install-incremental")) {
            return -1;
        }

        long end = System.currentTimeMillis();
        if (!checkSuccess(output, packageFile, "install-commit")) {
            return -1;
        }

        // Remove the temp files.
        mDevice.executeShellCommand(String.format("rm \"%s\"", remotePath));
        mDevice.executeShellCommand(String.format("rm \"%s\"", idsigRemotePath));

        // Uninstall the package if needed.
        if (mUninstallAfter) {
            CLog.d("Uninstalling: %s", packageName);
            mDevice.uninstallPackage(packageName);
        }
        return end - start;
    }

    /**
     * Report run metrics by creating an empty test run to stick them in
     *
     * @param listener the {@link ITestInvocationListener} of test results
     * @param runName the test name
     * @param metrics the {@link Map} that contains metrics for the given test
     */
    void reportMetrics(
            ITestInvocationListener listener, String runName, Map<String, String> metrics) {
        // Create an empty testRun to report the parsed runMetrics
        CLog.d("About to report metrics: %s", metrics);
        listener.testRunStarted(runName, 0);
        listener.testRunEnded(0, TfMetricProtoUtil.upgradeConvert(metrics));
    }

    /** Verifies that the output contains the "Success" mark. */
    private boolean checkSuccess(String output, File packageFile, String stepForErrorLog) {
        if (output == null || output.indexOf("Success") == -1) {
            CLog.e(
                    "Failed to execute [%s] for package %s with error %s",
                    stepForErrorLog, packageFile, output);
            return false;
        }
        return true;
    }

    private File getSignatureFile(File packageFile) {
        return new File(packageFile.getAbsolutePath().concat(".idsig"));
    }

    private boolean matchesPackagesForInstall(String fileName) {
        if (mPackages.isEmpty()) {
            return true;
        }

        for (String pkg : mPackages) {
            // "-" is the version delimiter and ensures we don't match for example
            // com.google.android.apps.docs for com.google.android.apps.docs.slides.
            if (fileName.contains(pkg + "-")) {
                return true;
            }
        }
        return false;
    }
}
