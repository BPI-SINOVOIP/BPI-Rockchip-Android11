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


package com.android.tradefed.targetprep.companion;

import com.android.tradefed.build.IDeviceBuildInfo;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.targetprep.TestAppInstallSetup;

/**
 * A {@link ITargetPreparer} that installs one or more apps from a
 * {@link IDeviceBuildInfo#getTestsDir()} folder onto an allocated companion device.
 * <p>
 * Note that this implies the test artifact in question in built into primary device build but also
 * suitable for companion device
 */
@OptionClass(alias = "companion-tests-zip-app")
public class CompanionTestAppInstallSetup extends TestAppInstallSetup {

    @Override
    public ITestDevice getDevice() throws TargetSetupError {
        ITestDevice device = super.getDevice();
        ITestDevice companion = CompanionDeviceTracker.getInstance().getCompanionDevice(device);
        if (companion == null) {
            throw new TargetSetupError(String.format("no companion device allocated for %s",
                    device.getSerialNumber()), device.getDeviceDescriptor());
        }
        return companion;
    }
}
