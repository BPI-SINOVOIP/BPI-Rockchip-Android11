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
package com.android.cts.deviceowner;

import android.app.admin.DevicePolicyManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.location.LocationManager;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Test {@link DevicePolicyManager#setLocationEnabled}.
 */
public class SetLocationEnabledTest extends BaseDeviceOwnerTest {
    private static final long TIMEOUT_MS = 5000;

    public void testSetLocationEnabled() throws Exception {
        LocationManager locationManager = mContext.getSystemService(LocationManager.class);
        boolean enabled = locationManager.isLocationEnabled();

        setLocationEnabledAndWait(!enabled);
        assertEquals(!enabled, locationManager.isLocationEnabled());
        setLocationEnabledAndWait(enabled);
        assertEquals(enabled, locationManager.isLocationEnabled());
    }

    private void setLocationEnabledAndWait(boolean enabled) throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        final BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                latch.countDown();
            }
        };
        mContext.registerReceiver(receiver, new IntentFilter(LocationManager.MODE_CHANGED_ACTION));

        try {
            mDevicePolicyManager.setLocationEnabled(getWho(), enabled);
            assertTrue("timed out waiting for location mode change broadcast",
                    latch.await(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } finally {
            mContext.unregisterReceiver(receiver);
        }
    }
}
