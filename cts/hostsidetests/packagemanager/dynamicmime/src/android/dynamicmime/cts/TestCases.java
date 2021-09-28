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

package android.dynamicmime.cts;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Invokes device-side tests as is, no need for any host-side setup
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class TestCases extends BaseHostJUnit4Test {
    private static final String PACKAGE_TEST_APP = "android.dynamicmime.testapp";

    @Test
    public void testDynamicMimeWithSingleApp() throws DeviceNotAvailableException {
        runDeviceTests("SingleAppTest");
    }

    @Test
    public void testDynamicMimeWithMultipleApps() throws DeviceNotAvailableException {
        runDeviceTests("MultipleAppsTest");
    }

    @Test
    public void testDynamicMimeWithMultipleGroupsPerFilter() throws DeviceNotAvailableException {
        runDeviceTests("ComplexFilterTest");
    }

    @Test
    public void testDynamicMimeWithAppUpdate() throws DeviceNotAvailableException {
        runDeviceTests("update.SameGroupsTest");
    }

    @Test
    public void testDynamicMimeWithChangedGroupsAppUpdate() throws DeviceNotAvailableException {
        runDeviceTests("update.ChangedGroupsTest");
    }

    private void runDeviceTests(String className)
            throws DeviceNotAvailableException {
        runDeviceTests(PACKAGE_TEST_APP, PACKAGE_TEST_APP + "." + className);
    }
}
