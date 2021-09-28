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
package android.os.cts.deviceidle;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.os.PowerManager;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class DeviceIdleTest {
    @Test
    public void testDeviceIdleManager() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        assertNotNull(context.getSystemService(Context.DEVICE_IDLE_CONTROLLER));
    }

    @Test
    public void testPowerManagerIgnoringBatteryOptimizations() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();

        assertTrue(context.getSystemService(PowerManager.class)
                .isIgnoringBatteryOptimizations("com.android.shell"));
        assertFalse(context.getSystemService(PowerManager.class)
                .isIgnoringBatteryOptimizations("no.such.package.!!!"));
    }

}
