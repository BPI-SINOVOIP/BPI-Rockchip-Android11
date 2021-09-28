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
 * limitations under the License
 */

package android.cts.backup;

import static org.junit.Assert.assertNull;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for checking other sounds settings value backup and restore works correctly.
 * The tests use "bmgr backupnow com.android.providers.settings" for backup
 *
 * Invokes device side tests provided by
 * {@link android.cts.backup.othersoundssettingsapp.OtherSoundsSettingsTest}.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class OtherSoundsSettingsHostSideTest extends BaseBackupHostSideTest {

    /** The name of the APK that a other sounds settings test will be run for */
    private static final String APP_APK = "CtsBackupOtherSoundsSettingsApp.apk";

    /** The name of the package of the app under test */
    private static final String APP_PACKAGE = "android.cts.backup.othersoundssettingsapp";

    /** The name of the device side test class */
    private static final String TEST_CLASS = APP_PACKAGE + ".OtherSoundsSettingsTest";

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        if (!mIsBackupSupported) {
            return;
        }

        installPackage(APP_APK);
    }

    @After
    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        if (!mIsBackupSupported) {
            return;
        }

        // Clear backup data and uninstall the package (in that order!)
        clearBackupDataInLocalTransport(APP_PACKAGE);
        assertNull(uninstallPackage(APP_PACKAGE));
    }

    /**
     * Test backup and restore of Dial pad tones
     */
    @Test
    public void testOtherSoundsSettings_dialPadTones() throws Exception {
        if (!mIsBackupSupported) {
            LogUtil.CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testOtherSoundsSettings_dialPadTones");
    }

    /**
     * Test backup and restore of Touch sounds
     */
    @Test
    public void testOtherSoundsSettings_touchSounds() throws Exception {
        if (!mIsBackupSupported) {
            LogUtil.CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testOtherSoundsSettings_touchSounds");
    }

    /**
     * Test backup and restore of Touch vibration
     */
    @Test
    public void testOtherSoundsSettings_touchVibration() throws Exception {
        if (!mIsBackupSupported) {
            LogUtil.CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testOtherSoundsSettings_touchVibration");
    }

    private void checkDeviceTest(String methodName) throws DeviceNotAvailableException {
        checkDeviceTest(APP_PACKAGE, TEST_CLASS, methodName);
    }
}
