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

package com.android.tradefed.targetprep;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.BackgroundDeviceAction;
import com.android.tradefed.device.CollectingOutputReceiver;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.error.DeviceErrorIdentifier;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.RunUtil;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

@OptionClass(alias = "run-command")
public class RunCommandTargetPreparer extends BaseTargetPreparer {

    @Option(name = "run-command", description = "adb shell command to run")
    private List<String> mCommands = new ArrayList<String>();

    @Option(name = "run-bg-command", description = "Command to run repeatedly in the"
            + " device background. Can be repeated to run multiple commands"
            + " in the background.")
    private List<String> mBgCommands = new ArrayList<String>();

    @Option(name = "hide-bg-output", description = "if true, don't log background command output")
    private boolean mHideBgOutput = false;

    @Option(name = "teardown-command", description = "adb shell command to run at teardown time")
    private List<String> mTeardownCommands = new ArrayList<String>();

    @Option(name = "delay-after-commands",
            description = "Time to delay after running commands, in msecs")
    private long mDelayMsecs = 0;

    @Option(name = "run-command-timeout",
            description = "Timeout for execute shell command",
            isTimeVal = true)
    private long mRunCmdTimeout = 0;

    @Option(name = "throw-if-cmd-fail", description = "Whether or not to throw if a command fails")
    private boolean mThrowIfFailed = false;

    private Map<BackgroundDeviceAction, CollectingOutputReceiver> mBgDeviceActionsMap =
            new HashMap<>();

    /** {@inheritDoc} */
    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, DeviceNotAvailableException {
        ITestDevice device = getDevice(testInfo);
        for (String bgCmd : mBgCommands) {
            CollectingOutputReceiver receiver = new CollectingOutputReceiver();
            BackgroundDeviceAction mBgDeviceAction =
                    new BackgroundDeviceAction(bgCmd, bgCmd, device, receiver, 0);
            mBgDeviceAction.start();
            mBgDeviceActionsMap.put(mBgDeviceAction, receiver);
        }

        for (String cmd : mCommands) {
            CommandResult result;
            // Shell v2 with command status checks
            if (mRunCmdTimeout > 0) {
                result =
                        device.executeShellV2Command(cmd, mRunCmdTimeout, TimeUnit.MILLISECONDS, 0);
            } else {
                result = device.executeShellV2Command(cmd);
            }
            // Ensure the command ran successfully.
            if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
                if (mThrowIfFailed) {
                    throw new TargetSetupError(
                            String.format(
                                    "Failed to run '%s' without error. stdout: '%s'\nstderr: '%s'",
                                    cmd, result.getStdout(), result.getStderr()),
                            device.getDeviceDescriptor(),
                            DeviceErrorIdentifier.SHELL_COMMAND_ERROR);
                } else {
                    CLog.d(
                            "cmd: '%s' failed, returned:\nstdout:%s\nstderr:%s",
                            cmd, result.getStdout(), result.getStderr());
                }
            }
        }

        if (mDelayMsecs > 0) {
            CLog.d("Sleeping %d msecs on device %s", mDelayMsecs, device.getSerialNumber());
            RunUtil.getDefault().sleep(mDelayMsecs);
        }
    }

    /** {@inheritDoc} */
    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        for (Map.Entry<BackgroundDeviceAction, CollectingOutputReceiver> bgAction :
                mBgDeviceActionsMap.entrySet()) {
            if (!mHideBgOutput) {
                CLog.d("Background command output : %s", bgAction.getValue().getOutput());
            }
            bgAction.getKey().cancel();
        }
        if (e instanceof DeviceNotAvailableException) {
            CLog.e("Skipping command teardown since exception was DeviceNotAvailable");
            return;
        }
        for (String cmd : mTeardownCommands) {
            try {
                CommandResult result = getDevice(testInfo).executeShellV2Command(cmd);
                if (!CommandStatus.SUCCESS.equals(result.getStatus())) {
                    CLog.d(
                            "tearDown cmd: '%s' failed, returned:\nstdout:%s\nstderr:%s",
                            cmd, result.getStdout(), result.getStderr());
                }
            } catch (TargetSetupError tse) {
                CLog.e(tse);
            }
        }
    }

    /** Add a command that will be run by the preparer. */
    public final void addRunCommand(String cmd) {
        mCommands.add(cmd);
    }

    /**
     * Returns the device to apply the preparer on.
     *
     * @param testInfo
     * @return The device to apply the preparer on.
     * @throws TargetSetupError
     */
    protected ITestDevice getDevice(TestInformation testInfo) throws TargetSetupError {
        return testInfo.getDevice();
    }
}

