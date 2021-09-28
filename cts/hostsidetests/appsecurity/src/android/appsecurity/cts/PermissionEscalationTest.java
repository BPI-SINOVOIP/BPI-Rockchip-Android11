/*
 * Copyright (C) 2016 The Android Open Source Project
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

import android.platform.test.annotations.SecurityTest;
import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceTestCase;
import com.android.tradefed.testtype.IBuildReceiver;

public class PermissionEscalationTest extends DeviceTestCase implements IBuildReceiver {
    private static final String ESCALATE_PERMISSION_PKG = "com.android.cts.escalate.permission";

    private static final String APK_DECLARE_NON_RUNTIME_PERMISSIONS =
            "CtsDeclareNonRuntimePermissions.apk";
    private static final String APK_ESCLATE_TO_RUNTIME_PERMISSIONS =
            "CtsEscalateToRuntimePermissions.apk";

    private CompatibilityBuildHelper mBuildHelper;

    @Override
    public void setBuild(IBuildInfo buildInfo) {
        mBuildHelper = new CompatibilityBuildHelper(buildInfo);
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        Utils.prepareSingleUser(getDevice());
        assertNotNull(mBuildHelper);

        getDevice().uninstallPackage(ESCALATE_PERMISSION_PKG);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        getDevice().uninstallPackage(ESCALATE_PERMISSION_PKG);
    }

    public void testNoPermissionEscalation() throws Exception {
        assertNull(getDevice().installPackage(mBuildHelper.getTestFile(
                APK_DECLARE_NON_RUNTIME_PERMISSIONS), false, false));
        assertNull(getDevice().installPackage(mBuildHelper.getTestFile(
                APK_ESCLATE_TO_RUNTIME_PERMISSIONS), true, false));
        runDeviceTests(ESCALATE_PERMISSION_PKG,
                "com.android.cts.escalatepermission.PermissionEscalationTest",
                "testCannotEscalateNonRuntimePermissionsToRuntime");
    }

    @SecurityTest
    public void testNoPermissionEscalationAfterReboot() throws Exception {
        assertNull(getDevice().installPackage(mBuildHelper.getTestFile(
                APK_DECLARE_NON_RUNTIME_PERMISSIONS), false, false));
        assertNull(getDevice().installPackage(mBuildHelper.getTestFile(
                APK_ESCLATE_TO_RUNTIME_PERMISSIONS), true, false));
        getDevice().reboot();
        runDeviceTests(ESCALATE_PERMISSION_PKG,
                "com.android.cts.escalatepermission.PermissionEscalationTest",
                "testRuntimePermissionsAreNotGranted");
    }

    private void runDeviceTests(String packageName, String testClassName, String testMethodName)
            throws DeviceNotAvailableException {
        Utils.runDeviceTestsAsCurrentUser(getDevice(), packageName, testClassName, testMethodName);
    }
}
