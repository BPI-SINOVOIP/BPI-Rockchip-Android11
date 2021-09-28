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
package android.permission.cts;

import android.os.PowerManager;
import android.test.AndroidTestCase;

public class PowerManagerServicePermissionTest extends AndroidTestCase {

    public void testSetBatterySaver_requiresPermissions() {
        PowerManager manager = getContext().getSystemService(PowerManager.class);
        boolean batterySaverOn = manager.isPowerSaveMode();

        try {
            manager.setPowerSaveModeEnabled(!batterySaverOn);
            fail("Toggling battery saver requires POWER_SAVER or DEVICE_POWER permission");
        } catch (SecurityException e) {
            // Expected Exception
        }
    }

    public void testGetPowerSaverMode_requiresPermissions() {
        try {
            PowerManager manager = getContext().getSystemService(PowerManager.class);
            manager.getPowerSaveModeTrigger();
            fail("Getting the current power saver mode requires the POWER_SAVER permission");
        } catch (SecurityException e) {
            // Expected Exception
        }
    }

    public void testsetDynamicPowerSavings_requiresPermissions() {
        try {
            PowerManager manager = getContext().getSystemService(PowerManager.class);
            manager.setDynamicPowerSaveHint(true, 0);
            fail("Updating the dynamic power savings state requires the POWER_SAVER permission");
        } catch (SecurityException e) {
            // Expected Exception
        }
    }
}
