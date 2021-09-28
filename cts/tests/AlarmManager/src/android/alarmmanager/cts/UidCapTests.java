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
 *
 */

package android.alarmmanager.cts;

import static org.junit.Assert.fail;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;

@AppModeFull
@RunWith(AndroidJUnit4.class)
public class UidCapTests {
    private static final String TAG = UidCapTests.class.getSimpleName();
    private static final String ACTION_PREFIX = "android.alarmmanager.cts.action.ALARM_";

    private static final int SUFFICIENT_NUM_ALARMS = 450;
    private static final int[] ALARM_TYPES = {
            AlarmManager.RTC_WAKEUP,
            AlarmManager.RTC,
            AlarmManager.ELAPSED_REALTIME_WAKEUP,
            AlarmManager.ELAPSED_REALTIME
    };

    private AlarmManager mAlarmManager;
    private Context mContext;
    private ArrayList<PendingIntent> mAlarmsSet = new ArrayList<>();

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        mAlarmManager = mContext.getSystemService(AlarmManager.class);
    }

    @Test
    public void sufficientAlarmsAllowedByDefault() {
        deleteAlarmManagerConstants();
        for (int i = 1; i <= SUFFICIENT_NUM_ALARMS; i++) {
            try {
                final PendingIntent pi = PendingIntent.getBroadcast(mContext, 0,
                        new Intent(ACTION_PREFIX + i), 0);
                mAlarmManager.set(ALARM_TYPES[i % ALARM_TYPES.length], Long.MAX_VALUE, pi);
                mAlarmsSet.add(pi);
            } catch (Exception e) {
                Log.e(TAG, "Could not set the " + i + "th alarm", e);
                fail("Caught exception [" + e.getMessage() + "] while setting " + i + "th alarm");
            }
        }
    }

    @Test
    public void alarmsCannotExceedLimit() {
        final int limit = 598;
        setMaxAlarmsPerUid(limit);
        for (int i = 0; i < limit; i++) {
            final PendingIntent pi = PendingIntent.getBroadcast(mContext, 0,
                    new Intent(ACTION_PREFIX + i), 0);
            mAlarmManager.set(ALARM_TYPES[i % ALARM_TYPES.length], Long.MAX_VALUE, pi);
            mAlarmsSet.add(pi);
        }

        final PendingIntent lastPi = PendingIntent.getBroadcast(mContext, 0,
                new Intent(ACTION_PREFIX + limit), 0);
        for (int type : ALARM_TYPES) {
            try {
                mAlarmManager.set(type, Long.MAX_VALUE, lastPi);
                mAlarmsSet.add(lastPi);
                fail("Able to set an alarm of type " + type + " after reaching the limit of "
                        + limit);
            } catch (IllegalStateException e) {
            }
        }
    }

    private void setMaxAlarmsPerUid(int maxAlarmsPerUid) {
        SystemUtil.runShellCommand("settings put global alarm_manager_constants max_alarms_per_uid="
                + maxAlarmsPerUid);
    }

    @After
    public void cancelAlarms() {
        for (PendingIntent pi : mAlarmsSet) {
            mAlarmManager.cancel(pi);
        }
        mAlarmsSet.clear();
    }

    @After
    public void deleteAlarmManagerConstants() {
        SystemUtil.runShellCommand("settings delete global alarm_manager_constants");
    }
}
