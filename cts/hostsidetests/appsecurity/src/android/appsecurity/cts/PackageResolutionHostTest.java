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
 * limitations under the License
 */

package android.appsecurity.cts;

import android.platform.test.annotations.AppModeFull;
import android.platform.test.annotations.AppModeInstant;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for package resolution.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class PackageResolutionHostTest extends BaseAppSecurityTest {

    private static final String TINY_APK = "CtsOrderedActivityApp.apk";
    private static final String TINY_PKG = "android.appsecurity.cts.orderedactivity";

    private String mOldVerifierValue;

    @Before
    public void setUp() throws Exception {
        getDevice().uninstallPackage(TINY_PKG);
    }

    @After
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(TINY_PKG);
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testResolveOrderedActivity_full() throws Exception {
        testResolveOrderedActivity(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testResolveOrderedActivity_instant() throws Exception {
        testResolveOrderedActivity(true);
    }
    private void testResolveOrderedActivity(boolean instant) throws Exception {
        new InstallMultiple(instant)
                .addFile(TINY_APK)
                .run();
        Utils.runDeviceTests(getDevice(), TINY_PKG,
                ".PackageResolutionTest", "queryActivityOrdered");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testResolveOrderedService_full() throws Exception {
        testResolveOrderedService(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testResolveOrderedService_instant() throws Exception {
        testResolveOrderedService(true);
    }
    private void testResolveOrderedService(boolean instant) throws Exception {
        new InstallMultiple(instant)
                .addFile(TINY_APK)
                .run();
        Utils.runDeviceTests(getDevice(), TINY_PKG,
                ".PackageResolutionTest", "queryServiceOrdered");
    }

    @Test
    @AppModeFull(reason = "'full' portion of the hostside test")
    public void testResolveOrderedReceiver_full() throws Exception {
        testResolveOrderedReceiver(false);
    }
    @Test
    @AppModeInstant(reason = "'instant' portion of the hostside test")
    public void testResolveOrderedReceiver_instant() throws Exception {
        testResolveOrderedReceiver(true);
    }
    private void testResolveOrderedReceiver(boolean instant) throws Exception {
        new InstallMultiple(instant)
                .addFile(TINY_APK)
                .run();
        Utils.runDeviceTests(getDevice(), TINY_PKG,
                ".PackageResolutionTest", "queryReceiverOrdered");
    }
}
