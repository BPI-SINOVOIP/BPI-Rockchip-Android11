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
 * limitations under the License
 */

package android.deviceconfig.cts;

import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.os.UserHandle;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.runner.RunWith;

import java.util.concurrent.Executor;

@RunWith(AndroidJUnit4.class)
abstract class AbstractDeviceConfigTestCase {

    static final Context CONTEXT = InstrumentationRegistry.getContext();
    static final Executor EXECUTOR = CONTEXT.getMainExecutor();

    static final String WRITE_DEVICE_CONFIG_PERMISSION = "android.permission.WRITE_DEVICE_CONFIG";
    static final String READ_DEVICE_CONFIG_PERMISSION = "android.permission.READ_DEVICE_CONFIG";

    // String used to skip tests if not support.
    // TODO: ideally it would be simpler to just use assumeTrue() in the @BeforeClass method, but
    // then the test would crash - it might be an issue on atest / AndroidJUnit4
    private static String sUnsupportedReason;

    /**
     * Get necessary permissions to access and modify properties through DeviceConfig API.
     */
    @BeforeClass
    public static void setUp() throws Exception {
        if (CONTEXT.getUserId() != UserHandle.USER_SYSTEM
                && CONTEXT.getPackageManager().isInstantApp()) {
            sUnsupportedReason = "cannot run test as instant app on secondary user "
                    + CONTEXT.getUserId();
        }
    }

    @Before
    public void assumeSupported() {
        assumeTrue(sUnsupportedReason, isSupported());
    }

    static boolean isSupported() {
        return sUnsupportedReason == null;
    }
}