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

package android.net.wifi.cts;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.MulticastLock;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;

@AppModeFull(reason = "Cannot get WifiManager in instant app mode")
public class MulticastLockTest extends WifiJUnit3TestBase {

    private static final String WIFI_TAG = "MulticastLockTest";

    /**
     * Verify acquire and release of Multicast locks
     */
    public void testMulticastLock() {
        if (!WifiFeature.isWifiSupported(getContext())) {
            // skip the test if WiFi is not supported
            return;
        }
        WifiManager wm = (WifiManager) getContext().getSystemService(Context.WIFI_SERVICE);
        MulticastLock mcl = wm.createMulticastLock(WIFI_TAG);

        mcl.setReferenceCounted(true);
        assertFalse(mcl.isHeld());
        mcl.acquire();
        assertTrue(mcl.isHeld());
        mcl.release();
        assertFalse(mcl.isHeld());
        mcl.acquire();
        mcl.acquire();
        assertTrue(mcl.isHeld());
        mcl.release();
        assertTrue(mcl.isHeld());
        mcl.release();
        assertFalse(mcl.isHeld());
        assertNotNull(mcl.toString());
        try {
            mcl.release();
            fail("should throw out exception because release is called"
                    +" a greater number of times than acquire");
        } catch (RuntimeException e) {
            // expected
        }

        mcl = wm.createMulticastLock(WIFI_TAG);
        mcl.setReferenceCounted(false);
        assertFalse(mcl.isHeld());
        mcl.acquire();
        assertTrue(mcl.isHeld());
        mcl.release();
        assertFalse(mcl.isHeld());
        mcl.acquire();
        mcl.acquire();
        assertTrue(mcl.isHeld());
        mcl.release();
        assertFalse(mcl.isHeld());
        assertNotNull(mcl.toString());
        // releasing again after release: but ignored for non-referenced locks
        mcl.release();
    }
}
