/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static com.google.common.truth.Truth.assertThat;

import android.platform.test.annotations.AppModeFull;

import com.android.compatibility.common.util.BackupUtils;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Test verifying that {@link BackupManager#setAutoRestore(boolean)} setting is respected. */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class AutoRestoreHostSideTest extends BaseBackupHostSideTest {
    private static final String PACKAGE = "android.cts.backup.autorestoreapp";
    private static final String CLASS = PACKAGE + ".AutoRestoreTest";
    private static final String APK = "CtsAutoRestoreApp.apk";

    private BackupUtils mBackupUtils;
    private Optional<Boolean> mWasAutoRestoreEnabled = Optional.empty();

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();

        mBackupUtils = getBackupUtils();
        mWasAutoRestoreEnabled = Optional.of(isAutoRestoreEnabled());

        installPackage(APK);
    }

    @After
    @Override
    public void tearDown() throws Exception {
        super.tearDown();

        if (mWasAutoRestoreEnabled.isPresent()) {
            mBackupUtils.executeShellCommandSync(
                    "bmgr autorestore " + (mWasAutoRestoreEnabled.get() ? "true" : "false"));
            uninstallPackage(PACKAGE);

            mWasAutoRestoreEnabled = Optional.empty();
        }
    }

    /**
     *
     *
     * <ol>
     *   <li>Enable auto restore
     *   <li>Write dummy values to shared preferences
     *   <li>Backup the package
     *   <li>Uninstall the package
     *   <li>Install the package again and verify shared prefs are restored
     * </ol>
     */
    @Test
    public void testSetAutoRestore_autoRestoresDataWhenEnabled() throws Exception {
        runDeviceProcedure("enableAutoRestore");

        populateSharedPrefs();

        backupAndReinstallPackage();

        runDeviceProcedure("testCheckSharedPrefsExist");
    }

    /**
     *
     *
     * <ol>
     *   <li>Disable auto restore
     *   <li>Write dummy values to shared preferences
     *   <li>Backup the package
     *   <li>Uninstall the package
     *   <li>Install the package again and verify shared prefs aren't restored
     * </ol>
     */
    @Test
    public void testSetAutoRestore_dontAutoRestoresDataWhenDisabled() throws Exception {
        runDeviceProcedure("disableAutoRestore");

        populateSharedPrefs();

        backupAndReinstallPackage();

        runDeviceProcedure("testCheckSharedPrefsDontExist");
    }

    private void populateSharedPrefs() throws Exception {
        runDeviceProcedure("saveValuesToSharedPrefs");
        runDeviceProcedure("testCheckSharedPrefsExist");
    }

    private void backupAndReinstallPackage() throws Exception {
        mBackupUtils.backupNowAndAssertSuccess(PACKAGE);
        uninstallPackage(PACKAGE);
        installPackage(APK);
    }

    private boolean isAutoRestoreEnabled() throws Exception {
        String output = mBackupUtils.executeShellCommandAndReturnOutput("dumpsys backup");
        Pattern pattern = Pattern.compile("Auto-restore is (enabled|disabled)");
        Matcher matcher = pattern.matcher(output.trim());

        assertThat(matcher.find()).isTrue();

        return "enabled".equals(matcher.group(1));
    }

    private void runDeviceProcedure(String testName) throws Exception {
        runDeviceTests(PACKAGE, CLASS, testName);
    }
}
