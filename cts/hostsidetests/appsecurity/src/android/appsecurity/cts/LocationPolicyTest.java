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


import android.platform.test.annotations.SecurityTest;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Tests to verify app location access */
@RunWith(DeviceJUnit4ClassRunner.class)
public class LocationPolicyTest extends BaseAppSecurityTest {

    private static final String TEST_PKG = "android.appsecurity.cts.locationpolicy";
    private static final String TEST_APK = "CtsLocationPolicyApp.apk";

    @Before
    public void setUp() throws Exception {
        getDevice().uninstallPackage(TEST_PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(TEST_PKG);
    }

    @Test
    @SecurityTest
    public void testLocationPolicyPermissions() throws Exception {
        new InstallMultiple(true).addFile(TEST_APK).run();
        Utils.runDeviceTests(
            getDevice(), TEST_PKG, ".LocationPolicyTest", "testLocationPolicyPermissions");
    }
}
