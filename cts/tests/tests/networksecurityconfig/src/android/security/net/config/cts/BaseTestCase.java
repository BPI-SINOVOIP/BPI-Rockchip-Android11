/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.security.net.config.cts;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.test.AndroidTestCase;

import com.android.compatibility.common.util.SystemUtil;

/**
 * Base test case for all tests under {@link android.security.net.config.cts}.
 */
public class BaseTestCase extends AndroidTestCase {
    @Override
    public void setUp() throws Exception {
        // Instant Apps cannot access WifiManager, skip wifi check.
        if (getContext().getPackageManager().isInstantApp()) {
            return;
        }
        WifiManager wifiManager = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
        if (!wifiManager.isWifiEnabled()) {
            SystemUtil.runShellCommand("svc wifi enable"); // toggle wifi on.
            Thread.sleep(5000); // sleep 5 second for wifi to connect to a network.
        }
    }
}
