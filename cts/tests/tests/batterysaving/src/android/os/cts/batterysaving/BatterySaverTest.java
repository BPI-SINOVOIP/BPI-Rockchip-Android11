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
 * limitations under the License.
 */
package android.os.cts.batterysaving;

import static com.android.compatibility.common.util.BatteryUtils.enableBatterySaver;
import static com.android.compatibility.common.util.BatteryUtils.runDumpsysBatteryUnplug;
import static com.android.compatibility.common.util.SystemUtil.runShellCommand;
import static com.android.compatibility.common.util.TestUtils.waitUntil;

import static junit.framework.Assert.fail;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.UiModeManager;
import android.content.res.Configuration;
import android.os.PowerManager;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.BatteryUtils;
import com.android.compatibility.common.util.SettingsUtils;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests related to battery saver:
 *
 * atest $ANDROID_BUILD_TOP/cts/tests/tests/batterysaving/src/android/os/cts/batterysaving/BatterySaverTest.java
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class BatterySaverTest extends BatterySavingTestBase {
    /**
     * Enable battery saver and make sure the relevant components get notifed.
     * @throws Exception
     */
    @Test
    public void testActivateBatterySaver() throws Exception {
        assertFalse(getPowerManager().isPowerSaveMode());
        assertEquals(PowerManager.LOCATION_MODE_NO_CHANGE,
                getPowerManager().getLocationPowerSaveMode());

        // Unplug the charger.
        runDumpsysBatteryUnplug();

        // Activate battery saver.
        enableBatterySaver(true);

        // Make sure the job scheduler and the alarm manager are informed.
        waitUntilAlarmForceAppStandby(true);
        waitUntilJobForceAppStandby(true);
        waitUntilForceBackgroundCheck(true);

        // Deactivate.
        // To avoid too much churn, let's sleep a little bit before deactivating.
        Thread.sleep(1000);

        enableBatterySaver(false);

        // Make sure the job scheduler and the alarm manager are informed.
        waitUntilAlarmForceAppStandby(false);
        waitUntilJobForceAppStandby(false);
        waitUntilForceBackgroundCheck(false);
    }

    @Test
    public void testSetBatterySaver_powerManager() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            PowerManager manager = BatteryUtils.getPowerManager();
            assertFalse(manager.isPowerSaveMode());

            // Unplug the charger.
            runDumpsysBatteryUnplug();
            Thread.sleep(1000);
            // Verify battery saver gets toggled.
            manager.setPowerSaveModeEnabled(true);
            assertTrue(manager.isPowerSaveMode());

            manager.setPowerSaveModeEnabled(false);
            assertFalse(manager.isPowerSaveMode());
        });
    }

    /** Tests that Battery Saver exemptions activate when car mode is active. */
    @Test
    public void testCarModeExceptions() throws Exception {
        final String nightModeText = runShellCommand("cmd uimode night");
        final String[] nightModeSplit = nightModeText.split(":");
        if (nightModeSplit.length != 2) {
            fail("Failed to get initial night mode value from " + nightModeText);
        }
        final String initialNightMode = nightModeSplit[1].trim();
        runShellCommand("cmd uimode night no");
        UiModeManager uiModeManager = getContext().getSystemService(UiModeManager.class);
        uiModeManager.disableCarMode(0);

        final PowerManager powerManager = BatteryUtils.getPowerManager();

        try {
            runDumpsysBatteryUnplug();

            SettingsUtils.set(SettingsUtils.NAMESPACE_GLOBAL, "battery_saver_constants",
                    "gps_mode=" + PowerManager.LOCATION_MODE_ALL_DISABLED_WHEN_SCREEN_OFF
                    + ",enable_night_mode=true");

            enableBatterySaver(true);

            assertTrue(powerManager.isPowerSaveMode());
            assertEquals(PowerManager.LOCATION_MODE_ALL_DISABLED_WHEN_SCREEN_OFF,
                    powerManager.getLocationPowerSaveMode());
            // UI change can take a while to propagate, so need to wait for this check.
            waitUntil("UI mode didn't change to " + Configuration.UI_MODE_NIGHT_YES,
                    () -> Configuration.UI_MODE_NIGHT_YES ==
                            (getContext().getResources().getConfiguration().uiMode
                                    & Configuration.UI_MODE_NIGHT_MASK));

            uiModeManager.enableCarMode(0);

            // Wait for UI change first before checking location mode since we can then be
            // confident that the broadcast has been processed.
            waitUntil("UI mode didn't change to " + Configuration.UI_MODE_NIGHT_NO,
                    () -> Configuration.UI_MODE_NIGHT_NO ==
                            (getContext().getResources().getConfiguration().uiMode
                                    & Configuration.UI_MODE_NIGHT_MASK));
            final int locationPowerSaveMode = powerManager.getLocationPowerSaveMode();
            assertTrue("Location power save mode didn't change from " + locationPowerSaveMode,
                    locationPowerSaveMode == PowerManager.LOCATION_MODE_FOREGROUND_ONLY
                            || locationPowerSaveMode == PowerManager.LOCATION_MODE_NO_CHANGE);

            uiModeManager.disableCarMode(0);

            // Wait for UI change first before checking location mode since we can then be
            // confident that the broadcast has been processed.
            waitUntil("UI mode didn't change to " + Configuration.UI_MODE_NIGHT_YES,
                    () -> Configuration.UI_MODE_NIGHT_YES ==
                            (getContext().getResources().getConfiguration().uiMode
                                    & Configuration.UI_MODE_NIGHT_MASK));
            assertEquals(PowerManager.LOCATION_MODE_ALL_DISABLED_WHEN_SCREEN_OFF,
                powerManager.getLocationPowerSaveMode());
        } finally {
            uiModeManager.disableCarMode(0);
            runShellCommand("cmd uimode night " + initialNightMode);
            SettingsUtils.delete(SettingsUtils.NAMESPACE_GLOBAL, "battery_saver_constants");
        }
    }
}
