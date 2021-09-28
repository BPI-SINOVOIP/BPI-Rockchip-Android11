/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.os.cts;

import android.content.ContentResolver;
import android.content.Context;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings.Global;
import android.test.AndroidTestCase;
import com.android.compatibility.common.util.BatteryUtils;
import com.android.compatibility.common.util.SystemUtil;
import org.junit.After;
import org.junit.Before;

@AppModeFull(reason = "Instant Apps don't have the WRITE_SECURE_SETTINGS permission "
        + "required in tearDown for Global#putInt")
public class PowerManagerTest extends AndroidTestCase {
    private static final String TAG = "PowerManagerTest";
    public static final long TIME = 3000;
    public static final int MORE_TIME = 300;

    private int mInitialPowerSaverMode;
    private int mInitialDynamicPowerSavingsEnabled;
    private int mInitialThreshold;

    /**
     * test points:
     * 1 Get a wake lock at the level of the flags parameter
     * 2 Force the device to go to sleep
     * 3 User activity happened
     */
    public void testPowerManager() throws InterruptedException {
        PowerManager pm = (PowerManager)getContext().getSystemService(Context.POWER_SERVICE);

        WakeLock wl = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, TAG);
        wl.acquire(TIME);
        assertTrue(wl.isHeld());
        Thread.sleep(TIME + MORE_TIME);
        assertFalse(wl.isHeld());

        try {
            pm.reboot("Testing");
            fail("reboot should throw SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    @Before
    public void setUp() {
        // store the current value so we can restore it
        ContentResolver resolver = getContext().getContentResolver();
        mInitialPowerSaverMode = Global.getInt(resolver, Global.AUTOMATIC_POWER_SAVE_MODE, 0);
        mInitialDynamicPowerSavingsEnabled =
                Global.getInt(resolver, Global.DYNAMIC_POWER_SAVINGS_ENABLED, 0);
        mInitialThreshold =
                Global.getInt(resolver, Global.DYNAMIC_POWER_SAVINGS_DISABLE_THRESHOLD, 0);

    }

    @After
    public void tearDown() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            ContentResolver resolver = getContext().getContentResolver();

            // Verify we can change it to dynamic.
            Global.putInt(resolver, Global.AUTOMATIC_POWER_SAVE_MODE, mInitialPowerSaverMode);
            Global.putInt(resolver,
                    Global.DYNAMIC_POWER_SAVINGS_ENABLED, mInitialDynamicPowerSavingsEnabled);
            Global.putInt(resolver,
                    Global.DYNAMIC_POWER_SAVINGS_DISABLE_THRESHOLD, mInitialThreshold);
        });
    }

    public void testPowerManager_getPowerSaveMode() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            PowerManager manager = BatteryUtils.getPowerManager();
            ContentResolver resolver = getContext().getContentResolver();

            // Verify we can change it to percentage.
            Global.putInt(resolver, Global.AUTOMATIC_POWER_SAVE_MODE, 0);
            assertEquals(
                    PowerManager.POWER_SAVE_MODE_TRIGGER_PERCENTAGE,
                    manager.getPowerSaveModeTrigger());

            // Verify we can change it to dynamic.
            Global.putInt(resolver, Global.AUTOMATIC_POWER_SAVE_MODE, 1);
            assertEquals(
                    PowerManager.POWER_SAVE_MODE_TRIGGER_DYNAMIC,
                    manager.getPowerSaveModeTrigger());
        });
    }

    public void testPowerManager_setDynamicPowerSavings() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            PowerManager manager = BatteryUtils.getPowerManager();
            ContentResolver resolver = getContext().getContentResolver();

            // Verify settings are actually updated.
            manager.setDynamicPowerSaveHint(true, 80);
            assertEquals(1, Global.getInt(resolver, Global.DYNAMIC_POWER_SAVINGS_ENABLED, 0));
            assertEquals(80, Global.getInt(resolver,
                    Global.DYNAMIC_POWER_SAVINGS_DISABLE_THRESHOLD, 0));

            manager.setDynamicPowerSaveHint(false, 20);
            assertEquals(0, Global.getInt(resolver, Global.DYNAMIC_POWER_SAVINGS_ENABLED, 1));
            assertEquals(20, Global.getInt(resolver,
                    Global.DYNAMIC_POWER_SAVINGS_DISABLE_THRESHOLD, 0));
        });
    }
}
