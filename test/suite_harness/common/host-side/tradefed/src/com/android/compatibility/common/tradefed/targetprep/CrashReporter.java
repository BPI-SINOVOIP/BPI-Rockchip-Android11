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
package com.android.compatibility.common.tradefed.targetprep;

import com.android.compatibility.common.util.CrashUtils;
import com.android.ddmlib.Log.LogLevel;
import com.android.ddmlib.MultiLineReceiver;
import com.android.tradefed.device.BackgroundDeviceAction;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.BaseTargetPreparer;

import org.json.JSONArray;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.PosixFilePermissions;
import java.util.regex.Matcher;

/**
 * Starts and kills the crash reporter thread. This thread uploads crash results to devices as they
 * occurring allowing for device side crash analysis.
 */
public class CrashReporter extends BaseTargetPreparer {

    private BackgroundDeviceAction mBackgroundThread;

    /** Uploads the current buffer of Crashes to the phone under the current test name. */
    private static void upload(ITestDevice device, String testname, JSONArray crashes) {
        if (crashes == null) {
            crashes = new JSONArray();
        }
        try {
            if (testname == null) {
                CLog.logAndDisplay(LogLevel.ERROR, "Attempted upload with no test name");
                return;
            }
            device.executeShellCommand(
                    String.format("rm -f %s%s", CrashUtils.DEVICE_PATH, CrashUtils.LOCK_FILENAME));
            Path tmpPath = Files.createTempFile(testname, ".txt");
            try {
                Files.setPosixFilePermissions(
                        tmpPath, PosixFilePermissions.fromString("rw-r--r--"));
                File reportFile = tmpPath.toFile();
                try (BufferedWriter writer = new BufferedWriter(new FileWriter(reportFile))) {
                    writer.write(crashes.toString());
                }
                device.pushFile(reportFile, CrashUtils.DEVICE_PATH + testname);
            } finally {
                Files.deleteIfExists(tmpPath);
            }
            device.executeShellCommand(
                    String.format("touch %s%s", CrashUtils.DEVICE_PATH, CrashUtils.LOCK_FILENAME));
        } catch (IOException | RuntimeException | DeviceNotAvailableException e) {
            CLog.logAndDisplay(LogLevel.ERROR, "Upload to device failed");
            CLog.logAndDisplay(LogLevel.ERROR, e.getMessage());
        }
    }

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo) {
        ITestDevice device = testInfo.getDevice();
        try {
            device.executeShellCommand("logcat -c");
        } catch (DeviceNotAvailableException e) {
            CLog.logAndDisplay(LogLevel.ERROR, "CrashReporterThread failed to clear logcat");
            CLog.logAndDisplay(LogLevel.ERROR, e.getMessage());
        }

        try {
            device.executeShellCommand("rm -rf " + CrashUtils.DEVICE_PATH);
            device.executeShellCommand("mkdir " + CrashUtils.DEVICE_PATH);
        } catch (DeviceNotAvailableException e) {
            CLog.logAndDisplay(
                    LogLevel.ERROR,
                    "CrashReporterThread failed to setup storage directory on device");
            CLog.logAndDisplay(LogLevel.ERROR, e.getMessage());
            return;
        }

        CrashReporterReceiver receiver = new CrashReporterReceiver(device);
        mBackgroundThread =
                new BackgroundDeviceAction(
                        "logcat", "CrashReporter logcat thread", device, receiver, 0);
        mBackgroundThread.start();

        try {
            // If the test starts before the reporter receiver can read the test start message then
            // the crash could only be read halfway. We wait until the receiver starts getting
            // messages.
            synchronized (receiver) {
                receiver.wait(10_000); // wait for 10 seconds max
            }
        } catch (InterruptedException e) {
            // continue as normal
        }
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) {
        if (mBackgroundThread != null) {
            mBackgroundThread.cancel();
        }
    }

    /**
     * Scans through lines received, parses out crashes, and stores them into a buffer. When a new
     * test started signal is received the buffered is cleared. When an upload signal is received
     * uploads the current buffer to the phone.
     */
    private class CrashReporterReceiver extends MultiLineReceiver {

        private String mTestName;
        private JSONArray mCrashes;
        private StringBuilder mLogcatChunk = new StringBuilder();
        private ITestDevice mDevice;

        public CrashReporterReceiver(ITestDevice device) {
            mDevice = device;
        }

        private void processLogLine(String line) {
            mLogcatChunk.append(line);
            mLogcatChunk.append('\n');
            Matcher m;
            if ((m = CrashUtils.sNewTestPattern.matcher(line)).find()) {
                mTestName = m.group(1);
                mCrashes = new JSONArray();
                mLogcatChunk.setLength(0);
            } else if (CrashUtils.sEndofCrashPattern.matcher(line).find()) {
                if (mCrashes == null) {
                    // In case the crash happens before any test is started, it's not related to the
                    // current test and shouldn't be reported.
                    return;
                }
                CrashUtils.addAllCrashes(mLogcatChunk.toString(), mCrashes);
                mLogcatChunk.setLength(0);
            } else if (CrashUtils.sUploadRequestPattern.matcher(line).find()) {
                upload(mDevice, mTestName, mCrashes);
            }
        }

        @Override
        public synchronized void processNewLines(String[] lines) {
            this.notifyAll(); // alert the main thread that we are active.
            if (!isCancelled()) {
                for (String line : lines) {
                    processLogLine(line);
                }
            }
        }

        @Override
        public boolean isCancelled() {
            return mBackgroundThread == null || mBackgroundThread.isCancelled();
        }
    }
}
