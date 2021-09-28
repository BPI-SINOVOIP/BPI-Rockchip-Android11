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
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests for checking sync adapter settings value backup and restore works correctly.
 * The app uses AccountSyncSettingsBackupHelper to do sync adapter settings backup of those values.
 * The tests use "bmgr backupnow android" for backup
 *
 * Invokes device side tests provided by
 * {@link android.cts.backup.syncadaptersettingsapp.SyncAdapterSettingsTest}.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class SyncAdapterSettingsHostSideTest extends BaseBackupHostSideTest {

    /** The name of the APK that a sync adapter settings will be run for */
    private static final String APP_APK = "CtsBackupSyncAdapterSettingsApp.apk";

    /** The name of the package of the app under test */
    private static final String APP_PACKAGE = "android.cts.backup.syncadaptersettingsapp";

    /** The name of the device side test class */
    private static final String TEST_CLASS = APP_PACKAGE + ".SyncAdapterSettingsTest";

    /** The name of the package for backup */
    private static final String ANDROID_PACKAGE = "android";

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
        // BackupManagerService won't let us wipe the data if package is uninstalled
        clearBackupDataInLocalTransport(ANDROID_PACKAGE);
        assertNull(uninstallPackage(APP_PACKAGE));
    }

    /**
     * Test backup and restore of MasterSyncAutomatically=true.
     */
    @Test
    public void testMasterSyncAutomatically_whenOn_isRestored() throws Exception {
        if (!mIsBackupSupported) {
            CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testMasterSyncAutomatically_whenOn_isRestored");
    }

    /**
     * Test backup and restore of MasterSyncAutomatically=false.
     */
    @Test
    public void testMasterSyncAutomatically_whenOff_isRestored() throws Exception {
        if (!mIsBackupSupported) {
            CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testMasterSyncAutomatically_whenOff_isRestored");
    }

    /**
     * Test that if syncEnabled=true and isSyncable=1 in restore data, syncEnabled will be true
     * and isSyncable will be left as the current value in the device.
     */
    @Test
    public void testIsSyncableChanged_ifTurnOnSyncEnabled() throws Exception {
        if (!mIsBackupSupported) {
            CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testIsSyncableChanged_ifTurnOnSyncEnabled");
    }

    /**
     * Test that if syncEnabled=false and isSyncable=0 in restore data, syncEnabled will be false
     * and isSyncable will be set to 0.
     */
    @Test
    public void testIsSyncableIsZero_ifTurnOffSyncEnabled() throws Exception {
        if (!mIsBackupSupported) {
            CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testIsSyncableIsZero_ifTurnOffSyncEnabled");
    }

    /**
     * Test that if syncEnabled=false and isSyncable=1 in restore data, syncEnabled will be false
     * and isSyncable will be set to 1.
     */
    @Test
    public void testIsSyncableIsOne_ifTurnOffSyncEnabled() throws Exception {
        if (!mIsBackupSupported) {
            CLog.i("android.software.backup feature is not supported on this device");
            return;
        }

        checkDeviceTest("testIsSyncableIsOne_ifTurnOffSyncEnabled");
    }

    private void checkDeviceTest(String methodName) throws DeviceNotAvailableException {
        checkDeviceTest(APP_PACKAGE, TEST_CLASS, methodName);
    }
}
