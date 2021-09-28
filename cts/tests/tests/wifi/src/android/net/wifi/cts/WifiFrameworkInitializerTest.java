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

package android.net.wifi.cts;

import android.net.wifi.WifiFrameworkInitializer;
import android.test.AndroidTestCase;

public class WifiFrameworkInitializerTest extends WifiJUnit3TestBase {
    /**
     * WifiFrameworkInitializer.registerServiceWrappers() should only be called by
     * SystemServiceRegistry during boot up when Wifi is first initialized. Calling this API at
     * any other time should throw an exception.
     */
    public void testRegisterServiceWrappers_failsWhenCalledOutsideOfSystemServiceRegistry() {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        try {
            WifiFrameworkInitializer.registerServiceWrappers();
            fail("Expected exception when calling "
                    + "WifiFrameworkInitializer.registerServiceWrappers() outside of "
                    + "SystemServiceRegistry!");
        } catch (IllegalStateException expected) {}
    }
}
