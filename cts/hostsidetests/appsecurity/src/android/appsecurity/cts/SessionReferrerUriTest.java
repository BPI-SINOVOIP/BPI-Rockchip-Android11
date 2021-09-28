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

package android.appsecurity.cts;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

@RunWith(DeviceJUnit4ClassRunner.class)
public class SessionReferrerUriTest extends BaseAppSecurityTest {

    private static final String SESSION_INSPECTOR_A_APK = "CtsSessionInspectorAppA.apk";
    private static final String SESSION_INSPECTOR_B_APK = "CtsSessionInspectorAppB.apk";
    private static final String SESSION_INSPECTOR_PKG_A = "com.android.cts.sessioninspector.a";
    private static final String SESSION_INSPECTOR_PKG_B = "com.android.cts.sessioninspector.b";

    @Before
    public void setup() throws Exception {
        new InstallMultiple().addFile(SESSION_INSPECTOR_A_APK).run();
        new InstallMultiple().addFile(SESSION_INSPECTOR_B_APK).run();
    }

    @After
    public void teardown() throws Exception {
        getDevice().uninstallPackage(SESSION_INSPECTOR_PKG_A);
        getDevice().uninstallPackage(SESSION_INSPECTOR_PKG_B);
    }

    @Test
    @AppModeFull(reason = "Only full apps may install")
    public void testSessionReferrerUriVisibleToOwner() throws DeviceNotAvailableException {
        Utils.runDeviceTests(getDevice(), SESSION_INSPECTOR_PKG_A,
                "com.android.cts.sessioninspector.SessionInspectorTest", "testOnlyOwnerCanSee");
        Utils.runDeviceTests(getDevice(), SESSION_INSPECTOR_PKG_B,
                "com.android.cts.sessioninspector.SessionInspectorTest", "testOnlyOwnerCanSee");
    }
}
