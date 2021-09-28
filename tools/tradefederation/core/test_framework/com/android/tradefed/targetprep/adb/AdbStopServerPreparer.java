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
package com.android.tradefed.targetprep.adb;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.invoker.ExecutionFiles.FilesKey;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.SemaphoreTokenTargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.IRunUtil;
import com.android.tradefed.util.RunUtil;

import java.io.File;
import java.io.IOException;

/**
 * Target preparer to stop adb server on the host before and after running adb tests.
 *
 * <p>This preparer should be used with care as it stops and restart adb on the hosts. It should
 * usually be tight with {@link SemaphoreTokenTargetPreparer} to avoid other tests from running at
 * the same time.
 */
@OptionClass(alias = "adb-stop-server-preparer")
public class AdbStopServerPreparer extends BaseTargetPreparer {

    public static final String ADB_BINARY_KEY = "adb_path";

    @Option(
        name = "restart-new-adb-version",
        description = "Whether or not to restart adb with the new version after stopping it."
    )
    private boolean mRestartNewVersion = true;

    private static final long CMD_TIMEOUT = 60000L;
    private static final String ANDROID_HOST_OUT = "ANDROID_HOST_OUT";

    private IRunUtil mRunUtil;
    private File mTmpDir;

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        IBuildInfo buildInfo = testInfo.getBuildInfo();
        getDeviceManager().stopAdbBridge();

        // Kill the default adb server
        getRunUtil().runTimedCmd(CMD_TIMEOUT, "adb", "kill-server");
        // Let the adb process finish
        getRunUtil().sleep(2000);

        if (!mRestartNewVersion) {
            CLog.d("Skipping restarting of new adb version.");
            return;
        }

        File adb = null;
        if (getEnvironment(ANDROID_HOST_OUT) != null) {
            String hostOut = getEnvironment(ANDROID_HOST_OUT);
            adb = new File(hostOut, "bin/adb");
            if (adb.exists()) {
                adb.setExecutable(true);
            } else {
                adb = null;
            }
        }

        if (adb == null && buildInfo.getFile("adb") != null) {
            adb = buildInfo.getFile("adb");
            adb = renameAdbBinary(adb);
            // Track the updated adb file.
            testInfo.executionFiles().put(FilesKey.ADB_BINARY, adb);
        }

        if (adb != null) {
            CLog.d("Restarting adb from %s", adb.getAbsolutePath());
            IRunUtil restartAdb = createRunUtil();
            CommandResult result =
                    restartAdb.runTimedCmd(CMD_TIMEOUT, adb.getAbsolutePath(), "start-server");
            if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
                throw new TargetSetupError(
                        String.format(
                                "Failed to restart adb with the build info one. stdout: %s.\n"
                                        + "sterr: %s",
                                result.getStdout(), result.getStderr()),
                        testInfo.getDevice().getDeviceDescriptor());
            }
        } else {
            getRunUtil().runTimedCmd(CMD_TIMEOUT, "adb", "start-server");
            throw new TargetSetupError(
                    "Could not find a new version of adb to tests.",
                    testInfo.getDevice().getDeviceDescriptor());
        }
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        FileUtil.recursiveDelete(mTmpDir);
        // Kill the test adb server
        getRunUtil().runTimedCmd(CMD_TIMEOUT, "adb", "kill-server");
        // Restart the one from the parent PATH (original one)
        CommandResult restart = getRunUtil().runTimedCmd(CMD_TIMEOUT, "adb", "start-server");
        CLog.d("Restart adb -  stdout: %s\nstderr: %s", restart.getStdout(), restart.getStderr());
        // Restart device manager monitor
        getDeviceManager().restartAdbBridge();
    }

    @VisibleForTesting
    IDeviceManager getDeviceManager() {
        return GlobalConfiguration.getDeviceManagerInstance();
    }

    @VisibleForTesting
    IRunUtil createRunUtil() {
        return new RunUtil();
    }

    @VisibleForTesting
    String getEnvironment(String key) {
        return System.getenv(key);
    }

    private IRunUtil getRunUtil() {
        if (mRunUtil == null) {
            mRunUtil = createRunUtil();
        }
        return mRunUtil;
    }

    private File renameAdbBinary(File originalAdb) {
        try {
            mTmpDir = FileUtil.createTempDir("adb");
        } catch (IOException e) {
            CLog.e("Cannot create temp directory");
            FileUtil.recursiveDelete(mTmpDir);
            return null;
        }
        File renamedAdbBinary = new File(mTmpDir, "adb");
        if (!originalAdb.renameTo(renamedAdbBinary)) {
            CLog.e("Cannot rename adb binary");
            return null;
        }
        if (!renamedAdbBinary.setExecutable(true)) {
            CLog.e("Cannot set adb binary executable");
            return null;
        }
        return renamedAdbBinary;
    }
}
