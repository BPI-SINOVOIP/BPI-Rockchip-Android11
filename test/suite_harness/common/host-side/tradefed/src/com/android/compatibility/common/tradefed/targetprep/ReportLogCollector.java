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
import com.android.compatibility.common.tradefed.util.CollectorUtil;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.StubDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.FileUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

/** An {@link ITargetPreparer} that prepares and pulls report logs. */
public class ReportLogCollector extends BaseTargetPreparer {

    @Option(name= "src-dir", description = "The directory to copy to the results dir")
    private String mSrcDir;

    @Option(name = "dest-dir", description = "The directory under the result to store the files")
    private String mDestDir;

    @Option(name = "temp-dir", description = "The temp directory containing host-side report logs")
    private String mTempReportFolder;

    @Option(name = "device-dir", description = "Create unique directory for each device")
    private boolean mDeviceDir;

    public ReportLogCollector() {
    }

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        prepareReportLogContainers(testInfo.getBuildInfo());
    }

    private void prepareReportLogContainers(IBuildInfo buildInfo) {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(buildInfo);
        try {
            File resultDir = buildHelper.getResultDir();
            if (mDestDir != null) {
                resultDir = new File(resultDir, mDestDir);
            }
            resultDir.mkdirs();
            if (!resultDir.isDirectory()) {
                CLog.e("%s is not a directory", resultDir.getAbsolutePath());
                return;
            }
        } catch (FileNotFoundException fnfe) {
            CLog.e(fnfe);
        }
    }

    @Override
    public void tearDown(TestInformation testInfo, Throwable e) {
        if (e instanceof DeviceNotAvailableException) {
            CLog.e("Invocation finished with DeviceNotAvailable, skipping collecting logs.");
            return;
        }
        ITestDevice device = testInfo.getDevice();
        IBuildInfo buildInfo = testInfo.getBuildInfo();
        if (device.getIDevice() instanceof StubDevice) {
            CLog.d("Skipping ReportLogCollector, it requires a device.");
            return;
        }
        // Pull report log files from device.
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(buildInfo);
        try {
            File resultDir = buildHelper.getResultDir();
            if (mDestDir != null) {
                resultDir = new File(resultDir, mDestDir);
            }
            resultDir.mkdirs();
            if (!resultDir.isDirectory()) {
                CLog.e("%s is not a directory", resultDir.getAbsolutePath());
                return;
            }
            String tmpDirName = mTempReportFolder;
            if (mDeviceDir) {
                tmpDirName = tmpDirName.replaceAll("/$", "");
                tmpDirName += "-" + device.getSerialNumber();
            }
            final File hostReportDir = FileUtil.createNamedTempDir(tmpDirName);
            if (!hostReportDir.isDirectory()) {
                CLog.e("%s is not a directory", hostReportDir.getAbsolutePath());
                return;
            }
            device.pullDir(mSrcDir, resultDir);
            CollectorUtil.pullFromHost(hostReportDir, resultDir);
            CollectorUtil.reformatRepeatedStreams(resultDir);
        } catch (DeviceNotAvailableException | IOException exception) {
            CLog.e(exception);
        }
    }
}
