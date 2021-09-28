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
 * limitations under the License
 */

package com.android.compatibility.common.util;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.INativeDevice;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.targetprep.TargetSetupError;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/** Host-side specific utilities for backup and restore tests. */
public class BackupHostSideUtils {
    private static final int USER_SYSTEM = 0;
    // This is Settings.Secure.USER_SETUP_COMPLETE
    private static final String USER_SETUP_COMPLETE = "user_setup_complete";

    /** Create a new {@link BackupUtils} instance. */
    public static BackupUtils createBackupUtils(INativeDevice device) {
        return new BackupUtils() {
            @Override
            protected InputStream executeShellCommand(String command) throws IOException {
                try {
                    String result = device.executeShellCommand(command);
                    return new ByteArrayInputStream(result.getBytes(StandardCharsets.UTF_8));
                } catch (DeviceNotAvailableException e) {
                    throw new IOException(e);
                }
            }
        };
    }

    public static void checkSetupComplete(ITestDevice device)
            throws TargetSetupError, DeviceNotAvailableException {
        if (!isSetupCompleteSettingForSystemUser(device))
            throw new TargetSetupError(
                    "Backup tests cannot be run:Setup not completed for system user",
                    device.getDeviceDescriptor());
    }

    private static boolean isSetupCompleteSettingForSystemUser(ITestDevice device)
            throws DeviceNotAvailableException {
        return device.getSetting(USER_SYSTEM, "secure", USER_SETUP_COMPLETE).equals("1");
    }
}
