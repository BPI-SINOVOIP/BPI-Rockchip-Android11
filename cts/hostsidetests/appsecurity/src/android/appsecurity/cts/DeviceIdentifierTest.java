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

package android.appsecurity.cts;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;

/**
 * Hostside test to verify an app with the READ_DEVICE_IDENTIFIERS app op set can read device
 * identifiers.
 */
public class DeviceIdentifierTest extends DeviceTestCase implements IBuildReceiver {
    private static final String DEVICE_IDENTIFIER_APK = "CtsAccessDeviceIdentifiers.apk";
    private static final String DEVICE_IDENTIFIER_PKG = "android.appsecurity.cts.deviceids";
    private static final String DEVICE_IDENTIFIER_CLASS =
            DEVICE_IDENTIFIER_PKG + ".DeviceIdentifierAppOpTest";
    private static final String DEVICE_IDENTIFIER_TEST_METHOD =
            "testAccessToDeviceIdentifiersWithAppOp";

    private CompatibilityBuildHelper mBuildHelper;

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuildHelper = new CompatibilityBuildHelper(buildInfo);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        assertNotNull(mBuildHelper);
        assertNull(
                getDevice().installPackage(mBuildHelper.getTestFile(DEVICE_IDENTIFIER_APK), false,
                        true));
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        getDevice().uninstallPackage(DEVICE_IDENTIFIER_PKG);
    }

    public void testDeviceIdentifierAccessWithAppOpGranted() throws Exception {
        setDeviceIdentifierAccessAppOp(DEVICE_IDENTIFIER_PKG, true);
        Utils.runDeviceTestsAsCurrentUser(getDevice(), DEVICE_IDENTIFIER_PKG,
                DEVICE_IDENTIFIER_CLASS,
                DEVICE_IDENTIFIER_TEST_METHOD);
    }

    private void setDeviceIdentifierAccessAppOp(String packageName, boolean allowed)
            throws DeviceNotAvailableException {
        String command =
                "appops set --user " + getDevice().getCurrentUser() + " " + packageName + " "
                        + "READ_DEVICE_IDENTIFIERS " + (allowed ? "allow" : "deny");
        LogUtil.CLog.d(
                "Output for command " + command + ": " + getDevice().executeShellCommand(command));
    }
}
