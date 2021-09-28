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
package com.android.wallpaper.testing;

import android.app.PendingIntent;

import com.android.wallpaper.module.AlarmManagerWrapper;

/**
 * Mock of {@link AlarmManagerWrapper}.
 */
public class TestAlarmManagerWrapper implements AlarmManagerWrapper {

    private int mExactAlarmSetCount;
    private int mInexactAlarmSetCount;
    private int mAlarmCanceledCount;

    private long mLastInexactTriggerAtMillis;

    @Override
    public void set(int type, long triggerAtMillis, PendingIntent operation) {
        mExactAlarmSetCount++;
    }

    @Override
    public void setWindow(int type, long windowStartMillis, long windowLengthMillis,
            PendingIntent operation) {
        mExactAlarmSetCount++;
    }

    @Override
    public void setInexactRepeating(int type, long triggerAtMillis, long intervalMillis,
            PendingIntent operation) {
        mInexactAlarmSetCount++;
        mLastInexactTriggerAtMillis = triggerAtMillis;
    }

    @Override
    public void cancel(PendingIntent operation) {
        mAlarmCanceledCount++;
    }

    public int getExactAlarmSetCount() {
        return mExactAlarmSetCount;
    }

    public int getInexactAlarmSetCount() {
        return mInexactAlarmSetCount;
    }

    public int getAlarmCanceledCount() {
        return mAlarmCanceledCount;
    }

    public long getLastInexactTriggerAtMillis() {
        return mLastInexactTriggerAtMillis;
    }
}
