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
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.DeviceStateMonitor;
import com.android.tradefed.device.IDeviceManager;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.util.CommandResult;
import com.android.tradefed.util.CommandStatus;
import com.android.tradefed.util.RunUtil;

/**
 * Device state monitor that executes extra checks on nested device to accommodate the specifics of
 * the virtualized environment.
 */
public class NestedDeviceStateMonitor extends DeviceStateMonitor {
    // Error from dumpsys if the framework is not quite ready yet
    private static final String DUMPSYS_ERROR = "Error dumping service info";

    private ITestDevice mDevice;

    public NestedDeviceStateMonitor(IDeviceManager mgr, IDevice device, boolean fastbootEnabled) {
        super(mgr, device, fastbootEnabled);
    }

    /** {@inheritDoc} */
    @Override
    protected boolean postOnlineCheck(final long waitTime) {
        long startTime = System.currentTimeMillis();
        if (!super.postOnlineCheck(waitTime)) {
            return false;
        }
        long elapsedTime = System.currentTimeMillis() - startTime;
        // Check that the device is actually usable and services have registered.
        // VM devices tend to show back very quickly in adb but sometimes are not quite ready.
        return nestedWaitForDeviceOnline(waitTime - elapsedTime);
    }

    private boolean nestedWaitForDeviceOnline(long maxWaitTime) {
        long maxTime = System.currentTimeMillis() + maxWaitTime;
        CommandResult res = null;
        while (maxTime > System.currentTimeMillis()) {
            try {
                // TODO: Use IDevice directly
                // Ensure that framework is ready
                res = mDevice.executeShellV2Command("dumpsys package");
                if (CommandStatus.SUCCESS.equals(res.getStatus())
                        && !res.getStdout().contains(DUMPSYS_ERROR)) {
                    return true;
                }
            } catch (DeviceNotAvailableException e) {
                CLog.e(e);
            }
            RunUtil.getDefault().sleep(200L);
        }
        if (res != null) {
            CLog.e("Error checking device ready: %s", res.getStderr());
        }
        return false;
    }

    /** Set the ITestDevice to call an adb command. */
    final void setDevice(ITestDevice device) {
        mDevice = device;
    }
}
