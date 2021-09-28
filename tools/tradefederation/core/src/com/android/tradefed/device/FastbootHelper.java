/*
 * Copyright (C) 2014 The Android Open Source Project
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
package com.android.tradefed.device;

import com.android.ddmlib.Log.LogLevel;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.IRunUtil;

import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A helper class for fastboot operations.
 */
public class FastbootHelper {

    /** max wait time in ms for fastboot devices command to complete */
    private static final long FASTBOOT_CMD_TIMEOUT = 1 * 60 * 1000;

    private IRunUtil mRunUtil;
    private String mFastbootPath = "fastboot";

    /**
     * Constructor.
     *
     * @param runUtil a {@link IRunUtil}.
     */
    public FastbootHelper(final IRunUtil runUtil, final String fastbootPath) {
        if (runUtil == null) {
            throw new IllegalArgumentException("runUtil cannot be null");
        }
        if (fastbootPath == null || fastbootPath.isEmpty()) {
            throw new IllegalArgumentException("fastboot cannot be null or empty");
        }
        mRunUtil = runUtil;
        mFastbootPath = fastbootPath;
    }

    /**
     * Determine if fastboot is available for use.
     */
    public boolean isFastbootAvailable() {
        // Run "fastboot help" to check the existence and the version of fastboot
        // (Old versions do not support "help" command).
        CommandResult fastbootResult = mRunUtil.runTimedCmdSilently(15000, mFastbootPath, "help");
        if (CommandStatus.SUCCESS.equals(fastbootResult.getStatus())) {
            return true;
        }
        if (fastbootResult.getStderr() != null &&
            fastbootResult.getStderr().indexOf("usage: fastboot") >= 0) {
            CLog.logAndDisplay(LogLevel.WARN,
                    "You are running an older version of fastboot, please update it.");
            return true;
        }
        CLog.d("fastboot not available. stdout: %s, stderr: %s",
                fastbootResult.getStdout(), fastbootResult.getStderr());
        return false;
    }

    /**
     * Returns a set of device serials in fastboot mode or an empty set if no fastboot devices.
     *
     * @return a set of device serials.
     */
    public Set<String> getDevices() {
        CommandResult fastbootResult =
                mRunUtil.runTimedCmdSilently(FASTBOOT_CMD_TIMEOUT, mFastbootPath, "devices");
        if (fastbootResult.getStatus().equals(CommandStatus.SUCCESS)) {
            CLog.v("fastboot devices returned\n %s",
                    fastbootResult.getStdout());
            return parseDevices(fastbootResult.getStdout(), false);
        } else {
            CLog.w("'fastboot devices' failed. Result: %s, stderr: %s", fastbootResult.getStatus(),
                    fastbootResult.getStderr());
        }
        return new HashSet<String>();
    }

    /**
     * Returns a map of device serials and whether they are in fastbootd mode or not.
     *
     * @return a Map of serial in bootloader or fastbootd, the boolean is true if in fastbootd
     */
    public Map<String, Boolean> getBootloaderAndFastbootdDevices() {
        CommandResult fastbootResult =
                mRunUtil.runTimedCmdSilently(FASTBOOT_CMD_TIMEOUT, mFastbootPath, "devices");
        if (fastbootResult.getStatus().equals(CommandStatus.SUCCESS)) {
            CLog.v("fastboot devices returned\n %s", fastbootResult.getStdout());
            Set<String> fastboot = parseDevices(fastbootResult.getStdout(), false);
            Set<String> fastbootd = parseDevices(fastbootResult.getStdout(), true);
            HashMap<String, Boolean> devices = new LinkedHashMap<>();
            for (String f : fastboot) {
                devices.put(f, false);
            }
            for (String f : fastbootd) {
                devices.put(f, true);
            }
            return devices;
        } else {
            CLog.w(
                    "'fastboot devices' failed. Result: %s, stderr: %s",
                    fastbootResult.getStatus(), fastbootResult.getStderr());
        }
        return new HashMap<String, Boolean>();
    }

    /**
     * Returns a map of device serials and whether they are in fastbootd mode or not.
     *
     * @param serials a map of devices serial number and fastboot mode serial number.
     * @return a Map of serial in bootloader or fastbootd, the boolean is true if in fastbootd
     */
    public Map<String, Boolean> getBootloaderAndFastbootdTcpDevices(Map<String, String> serials) {
        HashMap<String, Boolean> devices = new LinkedHashMap<>();
        long TIMEOUT = 1500;

        for (Entry<String, String> entry : serials.entrySet()) {
            CLog.v("Run 'fastboot -s %s getvar is-userspace' command", entry.getValue());
            CommandResult fastbootResult =
                    mRunUtil.runTimedCmdSilently(
                            TIMEOUT,
                            mFastbootPath,
                            "-s",
                            entry.getValue(),
                            "getvar",
                            "is-userspace");
            if (fastbootResult.getStatus().equals(CommandStatus.SUCCESS)) {
                if (fastbootResult.getStderr().contains("yes")) {
                    devices.put(entry.getKey(), true);
                } else {
                    devices.put(entry.getKey(), false);
                }
            }
        }

        return devices;
    }

    /**
     * Parses the output of "fastboot devices" command. Exposed for unit testing.
     *
     * @param fastbootOutput the output of fastboot command.
     * @param fastbootd whether or not we parse fastbootd or fastboot output
     * @return a set of device serials.
     */
    Set<String> parseDevices(String fastbootOutput, boolean fastbootd) {
        Set<String> serials = new HashSet<String>();
        Pattern fastbootPattern = null;
        if (fastbootd) {
            fastbootPattern = Pattern.compile("([\\w\\d-]+)\\s+fastbootd\\s*");
        } else {
            fastbootPattern = Pattern.compile("([\\w\\d-]+)\\s+fastboot\\s*");
        }
        Matcher fastbootMatcher = fastbootPattern.matcher(fastbootOutput);
        while (fastbootMatcher.find()) {
            serials.add(fastbootMatcher.group(1));
        }
        return serials;
    }

    /**
     * Executes a fastboot command on a device and return the output.
     *
     * @param serial a device serial.
     * @param command a fastboot command to run.
     * @return the output of the fastboot command. null if the command failed.
     */
    public String executeCommand(String serial, String command) {
        final CommandResult fastbootResult = mRunUtil.runTimedCmd(FASTBOOT_CMD_TIMEOUT,
                mFastbootPath, "-s", serial, command);
        if (fastbootResult.getStatus() != CommandStatus.SUCCESS) {
            CLog.w("'fastboot -s %s %s' failed. Result: %s, stderr: %s", serial, command,
                    fastbootResult.getStatus(), fastbootResult.getStderr());
            return null;
        }
        return fastbootResult.getStdout();
    }

    /** Returns whether or not a device is in Fastbootd instead of Bootloader. */
    public boolean isFastbootd(String serial) {
        final CommandResult fastbootResult =
                mRunUtil.runTimedCmd(
                        FASTBOOT_CMD_TIMEOUT,
                        mFastbootPath,
                        "-s",
                        serial,
                        "getvar",
                        "is-userspace");
        if (fastbootResult.getStderr().contains("is-userspace: yes")) {
            return true;
        }
        return false;
    }
}
