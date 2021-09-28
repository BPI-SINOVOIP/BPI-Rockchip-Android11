/*
 * Copyright (C) 2015 The Android Open Source Project
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

import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;

/** Checks that location is enabled before running a compatibility test */
@OptionClass(alias = "location-check")
public class LocationCheck extends SettingsPreparer {

    private static final String LOCATION_SETTING = "location_mode";
    private static final String LOCATION_MODE_ON = "3";

    private static final String GPS_FEATURE = "android.hardware.location.gps";
    private static final String NETWORK_FEATURE = "android.hardware.location.network";


    private boolean hasLocationFeature(ITestDevice device) throws DeviceNotAvailableException {
        String adbFeatures = device.executeShellCommand("pm list features");
        return (adbFeatures.contains(GPS_FEATURE) || adbFeatures.contains(NETWORK_FEATURE));
    }

    @Override
    public void run(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {

        if (!hasLocationFeature(testInfo.getDevice())) {
            return; // skip this precondition if required location feature is not present
        }

        CLog.i("Verifying location setting");
        mSettingName = LOCATION_SETTING;
        mSettingType = SettingsPreparer.SettingType.SECURE;
        mExpectedSettingValues.add(LOCATION_MODE_ON);
        mFailureMessage =
                "Location services must be enabled in order to "
                        + "successfully run the test suite";
        super.run(testInfo);
    }
}
