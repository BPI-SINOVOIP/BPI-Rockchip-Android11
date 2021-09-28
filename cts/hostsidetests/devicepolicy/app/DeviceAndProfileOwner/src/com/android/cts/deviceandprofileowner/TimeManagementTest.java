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

package com.android.cts.deviceandprofileowner;

import static com.google.common.truth.Truth.assertThat;

import java.util.concurrent.TimeUnit;

public class TimeManagementTest extends BaseDeviceAdminTest {

    // Real world time to restore after the test.
    private long mStartTimeWallClockMillis;
    // Elapsed time to measure time taken by the test.
    private long mStartTimeElapsedNanos;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        saveTime();
    }

    @Override
    protected void tearDown() throws Exception {
        restoreTime();
        super.tearDown();
    }

    public void testSetAutoTimeZoneEnabled() {
        mDevicePolicyManager.setAutoTimeZoneEnabled(ADMIN_RECEIVER_COMPONENT, true);

        assertThat(mDevicePolicyManager.getAutoTimeZoneEnabled(ADMIN_RECEIVER_COMPONENT)).isTrue();

        mDevicePolicyManager.setAutoTimeZoneEnabled(ADMIN_RECEIVER_COMPONENT, false);

        assertThat(mDevicePolicyManager.getAutoTimeZoneEnabled(ADMIN_RECEIVER_COMPONENT)).isFalse();
    }

    public void testSetTime() {
        mDevicePolicyManager.setAutoTimeEnabled(ADMIN_RECEIVER_COMPONENT, false);

        final long estimatedNow = mStartTimeWallClockMillis +
                TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - mStartTimeElapsedNanos);
        assertThat(mDevicePolicyManager.setTime(ADMIN_RECEIVER_COMPONENT,
                estimatedNow - TimeUnit.HOURS.toMillis(1))).isTrue();
    }

    public void testSetTime_failWhenAutoTimeEnabled() {
        mDevicePolicyManager.setAutoTimeEnabled(ADMIN_RECEIVER_COMPONENT, true);

        assertThat(mDevicePolicyManager.setTime(ADMIN_RECEIVER_COMPONENT, 0)).isFalse();
    }

    public void testSetTimeZone() {
        mDevicePolicyManager.setAutoTimeZoneEnabled(ADMIN_RECEIVER_COMPONENT, false);

        assertThat(
                mDevicePolicyManager.setTimeZone(ADMIN_RECEIVER_COMPONENT, "Singapore")).isTrue();
    }

    public void testSetTimeZone_failIfAutoTimeZoneEnabled() {
        mDevicePolicyManager.setAutoTimeZoneEnabled(ADMIN_RECEIVER_COMPONENT, true);

        assertThat(
                mDevicePolicyManager.setTimeZone(ADMIN_RECEIVER_COMPONENT, "Singapore")).isFalse();
    }

    private void saveTime() {
        mStartTimeWallClockMillis = System.currentTimeMillis();
        mStartTimeElapsedNanos = System.nanoTime();
    }

    private void restoreTime() {
        mDevicePolicyManager.setAutoTimeEnabled(ADMIN_RECEIVER_COMPONENT, false);

        final long estimatedNow = mStartTimeWallClockMillis +
                TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - mStartTimeElapsedNanos);
        mDevicePolicyManager.setTime(ADMIN_RECEIVER_COMPONENT, estimatedNow);

        mDevicePolicyManager.setAutoTimeEnabled(ADMIN_RECEIVER_COMPONENT, true);
    }
}
