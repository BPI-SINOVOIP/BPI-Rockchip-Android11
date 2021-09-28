/*
 * Copyright (C) 2018 The Android Open Source Project
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
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;

public class AccessSerialNumberTest extends DeviceTestCase implements IBuildReceiver {
    private static final String APK_ACCESS_SERIAL_LEGACY = "CtsAccessSerialLegacy.apk";
    private static final String APK_ACCESS_SERIAL_MODERN = "CtsAccessSerialModern.apk";
    private static final String ACCESS_SERIAL_PKG = "android.os.cts";

    private CompatibilityBuildHelper mBuildHelper;

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuildHelper = new CompatibilityBuildHelper(buildInfo);
    }

    private void runDeviceTests(String packageName, String testClassName, String testMethodName)
            throws DeviceNotAvailableException {
        Utils.runDeviceTests(getDevice(), packageName, testClassName, testMethodName);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        Utils.prepareSingleUser(getDevice());
        assertNotNull(mBuildHelper);

        getDevice().uninstallPackage(ACCESS_SERIAL_PKG);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        getDevice().uninstallPackage(ACCESS_SERIAL_PKG);
    }

    public void testSerialAccessPolicy() throws Exception {
        // Verify legacy app behavior
        assertNull(getDevice().installPackage(mBuildHelper.getTestFile(
                APK_ACCESS_SERIAL_LEGACY), false, false));
        runDeviceTests(ACCESS_SERIAL_PKG,
                "android.os.cts.AccessSerialLegacyTest",
                "testAccessSerialNoPermissionNeeded");

        // Verify modern app behavior
        assertNull(getDevice().installPackage(mBuildHelper.getTestFile(
                APK_ACCESS_SERIAL_MODERN), true, false));
        runDeviceTests(ACCESS_SERIAL_PKG,
                "android.os.cts.AccessSerialModernTest",
                "testAccessSerialPermissionNeeded");
    }
}
