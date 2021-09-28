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

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Preferred Activities test cases
 *
 * Verifies that preferred activity is removed after any change to any MIME group
 * declared by preferred activity package
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class PreferredActivitiesTestCases extends BaseHostJUnit4Test {
    private static final String PACKAGE_TEST_APP = "android.dynamicmime.testapp";

    @Before
    public void setUp() throws DeviceNotAvailableException {
        // wake up and unlock device
        getDevice().executeShellCommand("input keyevent KEYCODE_WAKEUP");
        getDevice().disableKeyguard();
    }

    @Test
    public void testRemoveFromGroup() throws DeviceNotAvailableException {
        runDeviceTest("testRemoveFromGroup");
    }

    @Test
    public void testAddToGroup() throws DeviceNotAvailableException {
        runDeviceTest("testAddToGroup");
    }

    @Test
    public void testClearGroup() throws DeviceNotAvailableException {
        runDeviceTest("testClearGroup");
    }

    @Test
    public void testModifyGroupWithoutActualGroupChanges() throws DeviceNotAvailableException {
        runDeviceTest("testModifyGroupWithoutActualGroupChanges");
    }

    @Test
    public void testModifyGroupWithoutActualIntentFilterChanges()
            throws DeviceNotAvailableException {
        runDeviceTest("testModifyGroupWithoutActualIntentFilterChanges");
    }

    private void runDeviceTest(String testMethodName) throws DeviceNotAvailableException {
        runDeviceTests(PACKAGE_TEST_APP, PACKAGE_TEST_APP + ".preferred.PreferredActivitiesTest",
                testMethodName);
    }
}
