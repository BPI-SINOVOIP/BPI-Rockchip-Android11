/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.cts.net;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.targetprep.ITargetPreparer;

public class NetworkPolicyTestsPreparer implements ITargetPreparer {
    private ITestDevice mDevice;
    private boolean mOriginalAirplaneModeEnabled;
    private String mOriginalAppStandbyEnabled;
    private String mOriginalBatteryStatsConstants;
    private final static String KEY_STABLE_CHARGING_DELAY_MS = "battery_charged_delay_ms";
    private final static int DESIRED_STABLE_CHARGING_DELAY_MS = 0;

    @Override
    public void setUp(TestInformation testInformation) throws DeviceNotAvailableException {
        mDevice = testInformation.getDevice();
        mOriginalAppStandbyEnabled = getAppStandbyEnabled();
        setAppStandbyEnabled("1");
        LogUtil.CLog.d("Original app_standby_enabled: " + mOriginalAppStandbyEnabled);

        mOriginalBatteryStatsConstants = getBatteryStatsConstants();
        setBatteryStatsConstants(
                KEY_STABLE_CHARGING_DELAY_MS + "=" + DESIRED_STABLE_CHARGING_DELAY_MS);
        LogUtil.CLog.d("Original battery_saver_constants: " + mOriginalBatteryStatsConstants);

        mOriginalAirplaneModeEnabled = getAirplaneModeEnabled();
        // Turn off airplane mode in case another test left the device in that state.
        setAirplaneModeEnabled(false);
        LogUtil.CLog.d("Original airplane mode state: " + mOriginalAirplaneModeEnabled);
    }

    @Override
    public void tearDown(TestInformation testInformation, Throwable e)
            throws DeviceNotAvailableException {
        setAirplaneModeEnabled(mOriginalAirplaneModeEnabled);
        setAppStandbyEnabled(mOriginalAppStandbyEnabled);
        setBatteryStatsConstants(mOriginalBatteryStatsConstants);
    }

    private void setAirplaneModeEnabled(boolean enable) throws DeviceNotAvailableException {
        executeCmd("cmd connectivity airplane-mode " + (enable ? "enable" : "disable"));
    }

    private boolean getAirplaneModeEnabled() throws DeviceNotAvailableException {
        return "enabled".equals(executeCmd("cmd connectivity airplane-mode").trim());
    }

    private void setAppStandbyEnabled(String appStandbyEnabled) throws DeviceNotAvailableException {
        if ("null".equals(appStandbyEnabled)) {
            executeCmd("settings delete global app_standby_enabled");
        } else {
            executeCmd("settings put global app_standby_enabled " + appStandbyEnabled);
        }
    }

    private String getAppStandbyEnabled() throws DeviceNotAvailableException {
        return executeCmd("settings get global app_standby_enabled").trim();
    }

    private void setBatteryStatsConstants(String batteryStatsConstants)
            throws DeviceNotAvailableException {
        executeCmd("settings put global battery_stats_constants \"" + batteryStatsConstants + "\"");
    }

    private String getBatteryStatsConstants() throws DeviceNotAvailableException {
        return executeCmd("settings get global battery_stats_constants");
    }

    private String executeCmd(String cmd) throws DeviceNotAvailableException {
        final String output = mDevice.executeShellCommand(cmd).trim();
        LogUtil.CLog.d("Output for '%s': %s", cmd, output);
        return output;
    }
}
