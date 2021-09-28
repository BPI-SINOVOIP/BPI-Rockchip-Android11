/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tradefed.targetprep;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.ITestLoggerReceiver;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.StreamUtil;
import com.android.tradefed.util.ZipUtil;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * A {@link ITargetPreparer} that pulls directories off device, compresses and saves it into logging
 * backend.
 */
public final class FolderSaver extends BaseTargetPreparer implements ITestLoggerReceiver {

    @Option(name = "device-path", description = "Location of directory on device to be pulled and "
            + "logged, may be repeated.")
    private List<String> mDevicePaths = new ArrayList<>();

    @Option(name = "include-empty", description = "Upload empty folders if set; don't if not.")
    private Boolean mIncludeEmpty = false;

    private ITestLogger mTestLogger;

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        // no-op
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void setTestLogger(ITestLogger testLogger) {
        mTestLogger = testLogger;
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        if (e instanceof DeviceNotAvailableException) {
            CLog.i("Device %s not available, skipping.", testInfo.getDevice().getSerialNumber());
            return;
        }
        if (mDevicePaths.isEmpty()) {
            CLog.i("No device path provided, skipping.");
            return;
        }
        ITestDevice device = testInfo.getDevice();
        for (String path : mDevicePaths) {
            // Don't try to pull a directory if it doesn't exist.
            if (!device.doesFileExist(path)) {
                CLog.w("Directory, %s, does not exist.", path);
                continue;
            }

            // Don't try to pull a file that isn't a directory.
            if (!device.isDirectory(path)) {
                CLog.w("File, %s, is not a directory.", path);
                continue;
            }

            // Don't pull empty directories if it's specified not to.
            if (!mIncludeEmpty && device.getFileEntry(path).getChildren(false).isEmpty()) {
                CLog.w("Skipping empty directory, %s.", path);
                continue;
            }

            File tempDir = null;
            try {
                tempDir = FileUtil.createTempDir("tf-pulled-dir");
                if (!device.pullDir(path, tempDir)) {
                    CLog.w("Failed to pull directory %s from device %s",
                            path, device.getSerialNumber());
                } else {
                    File zip = null;
                    try {
                        zip = ZipUtil.createZip(tempDir);
                        InputStreamSource s = null;
                        try {
                            s = new FileInputStreamSource(zip);
                            mTestLogger.testLog(path, LogDataType.ZIP, s);
                        } finally {
                            StreamUtil.cancel(s);
                        }
                    } finally {
                        FileUtil.deleteFile(zip);
                    }
                }
            } catch (IOException ioe) {
                throw new RuntimeException("exception while saving device directory", ioe);
            } finally {
                FileUtil.recursiveDelete(tempDir);
            }
        }
    }
}
