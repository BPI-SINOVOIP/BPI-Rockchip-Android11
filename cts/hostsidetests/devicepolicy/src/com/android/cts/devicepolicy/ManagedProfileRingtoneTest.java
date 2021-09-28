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

package com.android.cts.devicepolicy;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil.CLog;

import org.junit.Test;

public class ManagedProfileRingtoneTest extends BaseManagedProfileTest {
    @Test
    public void testRingtoneSync() throws Exception {
        if (!mHasFeature) {
            return;
        }
        givePackageWriteSettingsPermission(mProfileUserId);
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".RingtoneSyncTest",
                "testRingtoneSync", mProfileUserId);
    }

    // Test if setting RINGTONE disables sync
    @Test
    public void testRingtoneSyncAutoDisableRingtone() throws Exception {
        if (!mHasFeature) {
            return;
        }
        givePackageWriteSettingsPermission(mProfileUserId);
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".RingtoneSyncTest",
                "testRingtoneDisableSync", mProfileUserId);
    }

    // Test if setting NOTIFICATION disables sync
    @Test
    public void testRingtoneSyncAutoDisableNotification() throws Exception {
        if (!mHasFeature) {
            return;
        }
        givePackageWriteSettingsPermission(mProfileUserId);
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".RingtoneSyncTest",
                "testNotificationDisableSync", mProfileUserId);
    }

    // Test if setting ALARM disables sync
    @Test
    public void testRingtoneSyncAutoDisableAlarm() throws Exception {
        if (!mHasFeature) {
            return;
        }
        givePackageWriteSettingsPermission(mProfileUserId);
        runDeviceTestsAsUser(MANAGED_PROFILE_PKG, ".RingtoneSyncTest",
                "testAlarmDisableSync", mProfileUserId);
    }

    private void givePackageWriteSettingsPermission(int userId) throws DeviceNotAvailableException {
        // Allow app to write to settings (for RingtoneManager.setActualDefaultUri to work)
        String command = "appops set --user " + userId + " " + MANAGED_PROFILE_PKG
                + " android:write_settings allow";
        CLog.d("Output for command " + command + ": " + getDevice().executeShellCommand(command));
    }
}
