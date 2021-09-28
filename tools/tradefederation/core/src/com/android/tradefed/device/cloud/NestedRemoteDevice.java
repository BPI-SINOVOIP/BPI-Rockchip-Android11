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
package com.android.tradefed.device.cloud;

import com.android.ddmlib.IDevice;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.IDeviceMonitor;
import com.android.tradefed.device.IDeviceStateMonitor;
import com.android.tradefed.device.TestDevice;
import com.android.tradefed.device.cloud.CommonLogRemoteFileUtil.KnownLogFileEntry;
import com.android.tradefed.invoker.RemoteInvocationExecution;
import com.android.tradefed.log.ITestLogger;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.ByteArrayInputStreamSource;
import com.android.tradefed.result.FileInputStreamSource;
import com.android.tradefed.result.InputStreamSource;
import com.android.tradefed.result.LogDataType;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Representation of the device running inside a remote Cuttlefish VM. It will alter the local
 * device {@link TestDevice} behavior in some cases to take advantage of the setup.
 */
public class NestedRemoteDevice extends TestDevice {

    // TODO: Improve the way we associate nested device with their user
    private static final Map<String, String> IP_TO_USER = new HashMap<>();

    static {
        IP_TO_USER.put("127.0.0.1:6520", "vsoc-01");
        IP_TO_USER.put("127.0.0.1:6521", "vsoc-02");
        IP_TO_USER.put("127.0.0.1:6522", "vsoc-03");
        IP_TO_USER.put("127.0.0.1:6523", "vsoc-04");
        IP_TO_USER.put("127.0.0.1:6524", "vsoc-05");
        IP_TO_USER.put("127.0.0.1:6525", "vsoc-06");
        IP_TO_USER.put("127.0.0.1:6526", "vsoc-07");
        IP_TO_USER.put("127.0.0.1:6527", "vsoc-08");
        IP_TO_USER.put("127.0.0.1:6528", "vsoc-09");
        IP_TO_USER.put("127.0.0.1:6529", "vsoc-10");
        IP_TO_USER.put("127.0.0.1:6530", "vsoc-11");
        IP_TO_USER.put("127.0.0.1:6531", "vsoc-12");
        IP_TO_USER.put("127.0.0.1:6532", "vsoc-13");
        IP_TO_USER.put("127.0.0.1:6533", "vsoc-14");
        IP_TO_USER.put("127.0.0.1:6534", "vsoc-15");
    }

    /** When calling launch_cvd, the launcher.log is populated. */
    private static final String LAUNCHER_LOG_PATH = "/home/%s/cuttlefish_runtime/launcher.log";

    /**
     * Creates a {@link NestedRemoteDevice}.
     *
     * @param device the associated {@link IDevice}
     * @param stateMonitor the {@link IDeviceStateMonitor} mechanism to use
     * @param allocationMonitor the {@link IDeviceMonitor} to inform of allocation state changes.
     */
    public NestedRemoteDevice(
            IDevice device, IDeviceStateMonitor stateMonitor, IDeviceMonitor allocationMonitor) {
        super(device, stateMonitor, allocationMonitor);
        // TODO: Use IDevice directly
        if (stateMonitor instanceof NestedDeviceStateMonitor) {
            ((NestedDeviceStateMonitor) stateMonitor).setDevice(this);
        }
    }

    /** Teardown and restore the virtual device so testing can proceed. */
    public final boolean resetVirtualDevice(
            ITestLogger logger, IBuildInfo info, boolean resetDueToFailure)
            throws DeviceNotAvailableException {
        String username = IP_TO_USER.get(getSerialNumber());
        // stop_cvd
        String stopCvdCommand = String.format("sudo runuser -l %s -c 'stop_cvd'", username);
        CommandResult stopCvdRes = getRunUtil().runTimedCmd(60000L, stopCvdCommand.split(" "));
        if (!CommandStatus.SUCCESS.equals(stopCvdRes.getStatus())) {
            CLog.e("%s", stopCvdRes.getStderr());
            // Log 'adb devices' to confirm device is gone
            CommandResult printAdbDevices = getRunUtil().runTimedCmd(60000L, "adb", "devices");
            CLog.e("%s\n%s", printAdbDevices.getStdout(), printAdbDevices.getStderr());
            // Proceed here, device could have been already gone.
        }
        // Synchronize this so multiple reset do not occur at the same time inside one VM.
        synchronized (NestedRemoteDevice.class) {
            if (resetDueToFailure) {
                // Log the common files before restarting otherwise they are lost
                logDebugFiles(logger, username);
            }
            // Restart the device without re-creating the data partitions.
            List<String> createCommand =
                    LaunchCvdHelper.createSimpleDeviceCommand(username, true, false, false);
            CommandResult createRes =
                    getRunUtil()
                            .runTimedCmd(
                                    RemoteInvocationExecution.LAUNCH_EXTRA_DEVICE,
                                    "sh",
                                    "-c",
                                    String.join(" ", createCommand));
            if (!CommandStatus.SUCCESS.equals(createRes.getStatus())) {
                CLog.e("%s", createRes.getStderr());
                captureLauncherLog(username, logger);
                return false;
            }
            // Wait for the device to start for real.
            getRunUtil().sleep(5000);
            waitForDeviceAvailable();
            // Re-init the freshly started device.
            return reInitDevice(info);
        }
    }

    /**
     * Log the runtime files of the virtual device before resetting it since they will be deleted.
     */
    private void logDebugFiles(ITestLogger logger, String username) {
        List<KnownLogFileEntry> toFetch =
                CommonLogRemoteFileUtil.KNOWN_FILES_TO_FETCH.get(getOptions().getInstanceType());
        if (toFetch != null) {
            SimpleDateFormat formatter = new SimpleDateFormat("HH:mm:ss");
            for (KnownLogFileEntry entry : toFetch) {
                File toLog = new File(String.format(entry.path, username));
                if (!toLog.exists()) {
                    continue;
                }
                try (FileInputStreamSource source = new FileInputStreamSource(toLog)) {
                    logger.testLog(
                            String.format(
                                    "before_reset_%s_%s_%s",
                                    toLog.getName(), username, formatter.format(new Date())),
                            entry.type,
                            source);
                }
            }
        }
        logBugreport(String.format("before_reset_%s_bugreport", username), logger);
    }

    /** TODO: Re-run the target_preparation. */
    private boolean reInitDevice(IBuildInfo info) throws DeviceNotAvailableException {
        // Reset recovery since it's a new device
        setRecoveryMode(RecoveryMode.AVAILABLE);
        try {
            preInvocationSetup(info);
        } catch (TargetSetupError e) {
            CLog.e("Failed to re-init the device %s", getSerialNumber());
            CLog.e(e);
            return false;
        }
        // Success
        return true;
    }

    /** Capture and log the launcher.log to debug why the device didn't start properly. */
    private void captureLauncherLog(String username, ITestLogger logger) {
        String logName = String.format("launcher_log_failure_%s", username);
        File launcherLog = new File(String.format(LAUNCHER_LOG_PATH, username));
        if (!launcherLog.exists()) {
            CLog.e("%s doesn't exists, skip logging it.", launcherLog.getAbsolutePath());
            return;
        }
        // TF runs as the primary user and may get a permission denied to read the launcher.log of
        // other users. So use the shell to cat the file content.
        CommandResult readLauncherLogRes =
                getRunUtil()
                        .runTimedCmd(
                                60000L,
                                "sudo",
                                "runuser",
                                "-l",
                                username,
                                "-c",
                                String.format("'cat %s'", launcherLog.getAbsolutePath()));
        if (!CommandStatus.SUCCESS.equals(readLauncherLogRes.getStatus())) {
            CLog.e(
                    "Failed to read Launcher.log content due to: %s",
                    readLauncherLogRes.getStderr());
            return;
        }
        String content = readLauncherLogRes.getStdout();
        try (InputStreamSource source = new ByteArrayInputStreamSource(content.getBytes())) {
            logger.testLog(logName, LogDataType.TEXT, source);
        }
        CLog.d("See %s for the launcher.log that failed to start.", logName);
    }
}
