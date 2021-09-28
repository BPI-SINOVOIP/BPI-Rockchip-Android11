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

package android.alarmmanager.cts;

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.app.AlarmManager;
import android.content.Context;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeInstant;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests that alarm manager works as expected with instant apps
 */
@AppModeInstant
@RunWith(AndroidJUnit4.class)
public class InstantAppsTests {
    private static final String TAG = "AlarmManagerInstantTests";

    private AlarmManager mAlarmManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mAlarmManager = mContext.getSystemService(AlarmManager.class);
        assumeTrue(mContext.getPackageManager().isInstantApp());
        updateAlarmManagerSettings();
    }

    @Test
    public void elapsedRealtimeAlarm() throws Exception {
        final long futurity = 2500;
        final long triggerElapsed = SystemClock.elapsedRealtime() + futurity;
        final CountDownLatch latch = new CountDownLatch(1);
        mAlarmManager.setExact(AlarmManager.ELAPSED_REALTIME, triggerElapsed, TAG,
                () -> latch.countDown(), null);
        Thread.sleep(futurity);
        assertTrue("Alarm did not fire as expected", latch.await(500, TimeUnit.MILLISECONDS));
    }

    @Test
    public void rtcAlarm() throws Exception {
        final long futurity = 2500;
        final long triggerRtc = System.currentTimeMillis() + futurity;
        final CountDownLatch latch = new CountDownLatch(1);
        mAlarmManager.setExact(AlarmManager.RTC, triggerRtc, TAG, () -> latch.countDown(), null);
        Thread.sleep(futurity);
        assertTrue("Alarm did not fire as expected", latch.await(500, TimeUnit.MILLISECONDS));
    }

    @After
    public void deleteAlarmManagerSettings() {
        SystemUtil.runShellCommand("settings delete global alarm_manager_constants");
    }

    private void updateAlarmManagerSettings() {
        final StringBuffer cmd = new StringBuffer("settings put global alarm_manager_constants ");
        cmd.append("min_futurity=0");
        SystemUtil.runShellCommand(cmd.toString());
    }
}
