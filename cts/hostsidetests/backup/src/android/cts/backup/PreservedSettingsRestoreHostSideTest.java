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

package android.cts.backup;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Verifies that modified settings will not be overridden during settings restore unless they are
 * explicitly marked as overrideable.
 */
// TODO(b/135920714): Move to device-side when it's possible to use System APIs there.
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class PreservedSettingsRestoreHostSideTest extends BaseBackupHostSideTest {
    private static final String SETTINGS_PACKAGE = "com.android.providers.settings";
    private static final String TEST_APP_PACKAGE = "android.cts.backup.preservedsettingsapp";
    private static final String TEST_APP_CLASS = TEST_APP_PACKAGE + ".PreservedSettingsRestoreTest";
    private static final String TEST_APP_APK = "CtsPreservedSettingsApp.apk";

    @Before
    public void setUp() throws Exception {
        installPackage(TEST_APP_APK);
        runDeviceSideProcedure("setupDevice");
    }

    @After
    public void tearDown() throws Exception {
        runDeviceSideProcedure("cleanupDevice");
        uninstallPackage(TEST_APP_PACKAGE);
    }

    /**
     * <ol>
     *     <li>Save the current values for settings used in the test</li>
     *     <li>Run settings backup</li>
     *     <li>Modify settings: with and without making them as overrideable by restore</li>
     *     <li>Run settings restore</li>
     *     <li>Verify that for the settings not marked as overrideable the current values were kept,
     *         while the overrideable ones were restored from the backup data</li>
     * </ol>
     */
    @Test
    public void testModifiedSettingsArePreserved() throws Exception {
        getBackupUtils().backupNowAndAssertSuccess(SETTINGS_PACKAGE);

        runDeviceSideProcedure("modifySettings");
        getBackupUtils().restoreAndAssertSuccess("1", SETTINGS_PACKAGE);

        runDeviceSideProcedure("testOnlyOverrideableSettingsAreOverridden");
    }

    private void runDeviceSideProcedure(String procedure) throws Exception {
        runDeviceTests(TEST_APP_PACKAGE, TEST_APP_CLASS, procedure);
    }
}
