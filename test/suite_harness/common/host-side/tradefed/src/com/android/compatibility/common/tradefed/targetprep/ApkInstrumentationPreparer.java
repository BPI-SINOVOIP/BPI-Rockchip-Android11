/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.compatibility.common.tradefed.targetprep;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.ddmlib.testrunner.TestResult.TestStatus;
import com.android.tradefed.config.IConfiguration;
import com.android.tradefed.config.IConfigurationReceiver;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.CollectingTestListener;
import com.android.tradefed.result.TestDescription;
import com.android.tradefed.result.TestResult;
import com.android.tradefed.result.TestRunResult;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.testtype.AndroidJUnitTest;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Map.Entry;

/** Target preparer that instruments an APK. */
@OptionClass(alias = "apk-instrumentation-preparer")
public class ApkInstrumentationPreparer extends PreconditionPreparer
        implements IConfigurationReceiver {

    @Option(name = "apk", description = "Name of the apk to instrument", mandatory = true)
    protected String mApkFileName = null;

    @Option(name = "package", description = "Name of the package", mandatory = true)
    protected String mPackageName = null;

    public enum When {
        BEFORE, AFTER, BOTH;
    }

    @Option(name = "when", description = "When to instrument the apk", mandatory = true)
    protected When mWhen = null;

    @Option(name = "throw-error", description = "Whether to throw error for device test failure")
    protected boolean mThrowError = true;

    private IConfiguration mConfiguration = null;

    /** {@inheritDoc} */
    @Override
    public void setConfiguration(IConfiguration configuration) {
        mConfiguration = configuration;
    }

    /** {@inheritDoc} */
    @Override
    public void run(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        if (mWhen == When.AFTER) {
            return;
        }
        ITestDevice device = testInfo.getDevice();
        try {
            if (instrument(testInfo)) {
                CLog.d("Target preparation successful");
            } else if (mThrowError) {
                throw new TargetSetupError("Not all target preparation steps completed",
                        device.getDeviceDescriptor());
            }
        } catch (FileNotFoundException e) {
            throw new TargetSetupError("Couldn't find apk to instrument", e,
                    device.getDeviceDescriptor());
        }
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        if (e instanceof DeviceNotAvailableException) {
            return;
        }
        if (mWhen == When.BEFORE) {
            return;
        }
        try {
            instrument(testInfo);
        } catch (FileNotFoundException e1) {
            CLog.e("Couldn't find apk to instrument");
            CLog.e(e1);
        }
    }

    private boolean instrument(TestInformation testInfo)
            throws DeviceNotAvailableException, FileNotFoundException {
        CompatibilityBuildHelper buildHelper =
                new CompatibilityBuildHelper(testInfo.getBuildInfo());

        File apkFile = buildHelper.getTestFile(mApkFileName);
        if (!apkFile.exists()) {
            throw new FileNotFoundException(String.format("%s not found", mApkFileName));
        }

        ITestDevice device = testInfo.getDevice();
        if (device.getAppPackageInfo(mPackageName) != null) {
            CLog.i("Package %s already present on the device, uninstalling ...", mPackageName);
            device.uninstallPackage(mPackageName);
        }

        CLog.i("Instrumenting package: %s", mPackageName);
        CollectingTestListener listener = new CollectingTestListener();
        AndroidJUnitTest instrTest = new AndroidJUnitTest();
        instrTest.setConfiguration(mConfiguration);
        instrTest.setDevice(device);
        instrTest.setInstallFile(apkFile);
        instrTest.setPackageName(mPackageName);
        instrTest.setRerunMode(false);
        instrTest.setReRunUsingTestFile(false);
        // TODO: Make this configurable.
        instrTest.setIsolatedStorage(false);
        instrTest.run(testInfo, listener);
        TestRunResult result = listener.getCurrentRunResults();

        for (Entry<TestDescription, TestResult> results : result.getTestResults().entrySet()) {
            if (TestStatus.FAILURE.equals(results.getValue().getStatus())) {
                if (mThrowError) {
                    CLog.e(
                            "Target preparation step %s failed.\n%s",
                            results.getKey(), results.getValue().getStackTrace());
                } else {
                    CLog.w(
                            "Target preparation step %s failed.\n%s",
                            results.getKey(), results.getValue().getStackTrace());
                }
            }
        }
        // If any failure return false
        return !(result.isRunFailure() || result.hasFailedTests());
    }
}
