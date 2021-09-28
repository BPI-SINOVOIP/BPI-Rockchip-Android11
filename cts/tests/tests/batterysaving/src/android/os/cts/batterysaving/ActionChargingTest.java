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
package android.os.cts.batterysaving;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.SystemClock;
import android.util.Log;

import com.android.compatibility.common.util.AmUtils;
import com.android.compatibility.common.util.BatteryUtils;
import com.android.compatibility.common.util.SettingsUtils;
import com.android.compatibility.common.util.ShellIdentityUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class ActionChargingTest extends BatterySavingTestBase {
    private static final String TAG = "ActionChargingTest";

    private static final long DELIVER_DELAY_ALLOWANCE_MILLIS = 5000;
    private static final long CHARGING_DELAY_MILLIS = 5000;

    private BatteryManager mBatteryManager;

    private class ActionChargingListener extends BroadcastReceiver {
        private final CountDownLatch mLatch = new CountDownLatch(1);
        private volatile long mReceiveUptime;

        @Override
        public void onReceive(Context context, Intent intent) {
            if (BatteryManager.ACTION_CHARGING.equals(intent.getAction())) {
                Log.i(TAG, "ACTION_CHARGING received");
                mReceiveUptime = SystemClock.uptimeMillis();
                mLatch.countDown();
            }
        }

        public void startMonitoring() {
            final IntentFilter filter = new IntentFilter(BatteryManager.ACTION_CHARGING);
            getContext().registerReceiver(this, filter);
        }

        public void stopMonitoring() {
            getContext().unregisterReceiver(this);
        }

        public void await(long timeoutMillis) throws InterruptedException {
            assertTrue("Didn't receive ACTION_CHARGING within timeout",
                    mLatch.await(timeoutMillis, TimeUnit.MILLISECONDS));
        }

        public boolean received() {
            return mReceiveUptime != 0;
        }

        public long getReceivedUptime() {
            return mReceiveUptime;
        }
    }

    private void setChargingDelay(long millis) {
        SettingsUtils.set(SettingsUtils.NAMESPACE_GLOBAL,
                "battery_stats_constants",
                "battery_charged_delay_ms=" + millis);
    }

    private void resetSetting() {
        SettingsUtils.delete(SettingsUtils.NAMESPACE_GLOBAL, "battery_stats_constants");
        ShellIdentityUtils.invokeMethodWithShellPermissions(mBatteryManager,
                (bm) -> bm.setChargingStateUpdateDelayMillis(-1));
    }

    @Before
    public void setUp() {
        mBatteryManager = getContext().getSystemService(BatteryManager.class);
    }

    @After
    public void tearDown() {
        resetSetting();
    }

    @Test
    public void testActionChargingDeferred_withGlobalSetting() throws Exception {
        // Change the default
        setChargingDelay(CHARGING_DELAY_MILLIS);
        checkActionChargingDeferred();
    }

    @Test
    public void testActionChargingDeferred_withApi() throws Exception {
        ShellIdentityUtils.invokeMethodWithShellPermissions(mBatteryManager,
                (bm) -> bm.setChargingStateUpdateDelayMillis((int) CHARGING_DELAY_MILLIS));
        checkActionChargingDeferred();
    }

    @Test
    public void testActionChargingDeferred_withApiThenReset() throws Exception {
        setChargingDelay(CHARGING_DELAY_MILLIS);

        // Set a longer value via the API, and then set -1, which should reset the override value.
        ShellIdentityUtils.invokeMethodWithShellPermissions(mBatteryManager,
                (bm) -> bm.setChargingStateUpdateDelayMillis((int) CHARGING_DELAY_MILLIS * 100));
        ShellIdentityUtils.invokeMethodWithShellPermissions(mBatteryManager,
                (bm) -> bm.setChargingStateUpdateDelayMillis(-1));

        // So now CHARGING_DELAY_MILLIS should be in effect.
        checkActionChargingDeferred();
    }

    public void checkActionChargingDeferred() throws Exception {
        BatteryUtils.runDumpsysBatteryUnplug();
        BatteryUtils.runDumpsysBatterySetLevel(50);

        AmUtils.waitForBroadcastIdle();

        final ActionChargingListener listener = new ActionChargingListener();
        try {
            listener.startMonitoring();

            // Plug in the charger.
            BatteryUtils.runDumpsysBatterySetPluggedIn(true);

            // Wait for CHARGING_DELAY_MILLIS -- broadcast shouldn't be sent yet, because
            // the battery level hasn't increased.
            Thread.sleep(CHARGING_DELAY_MILLIS + DELIVER_DELAY_ALLOWANCE_MILLIS);
            AmUtils.waitForBroadcastIdle();

            assertFalse(
                    "CHARGING shouldn't be sent (battery level not increased)",
                    listener.received());

            // Increase the battery level, now the broadcast should be sent.
            final long increasedUptime = SystemClock.uptimeMillis();
            BatteryUtils.runDumpsysBatterySetLevel(51);

            listener.await(CHARGING_DELAY_MILLIS + DELIVER_DELAY_ALLOWANCE_MILLIS);

            final long actualDelay = listener.getReceivedUptime() - increasedUptime;

            assertThat(actualDelay).isAtLeast(CHARGING_DELAY_MILLIS);

        } finally {
            listener.stopMonitoring();
        }
    }

    @Test
    public void testSetChargingStateUpdateDelayMillis_noPermission() {
        try {
            mBatteryManager.setChargingStateUpdateDelayMillis(1);
        } catch (SecurityException expected) {
            return;
        }
        fail("Didn't throw SecurityException");
    }
}
