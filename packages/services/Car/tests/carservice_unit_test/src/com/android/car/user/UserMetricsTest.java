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

package com.android.car.user;

import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STARTING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STOPPED;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_STOPPING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_SWITCHING;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED;
import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKING;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;


import android.annotation.UserIdInt;
import android.os.SystemClock;
import android.util.SparseArray;

import com.android.car.user.UserMetrics.UserStartingMetric;
import com.android.car.user.UserMetrics.UserStoppingMetric;

import org.junit.Test;

public final class UserMetricsTest {

    private final UserMetrics mUserMetrics = new UserMetrics();
    @UserIdInt private final int mFromUserId = 10;
    @UserIdInt private final int mUserId = 11;

    @Test
    public void testStartingEvent_success() {
        long timestamp = sendStartingEvent(mUserId);

        assertStartTime(timestamp, mUserId);
    }

    @Test
    public void testStartingEvent_multipleCallsDifferentUser() {
        long timestamp1 = sendStartingEvent(mUserId);
        int userId = 12;
        long timestamp2 = sendStartingEvent(userId);

        assertStartTime(timestamp1, mUserId);
        assertStartTime(timestamp2, userId);
    }

    @Test
    public void testStartingEvent_multipleCallsSameUser() {
        long timestamp1 = sendStartingEvent(mUserId);
        assertStartTime(timestamp1, mUserId);
        long timestamp2 = sendStartingEvent(mUserId);

        assertStartTime(timestamp2, mUserId);
    }

    @Test
    public void testSwitchingEvent_failure() {
        sendSwitchingEvent(mFromUserId, mUserId);

        assertNoStartingMetric(mUserId);
    }

    @Test
    public void testSwitchingEvent_success() {
        sendStartingEvent(mUserId);
        long timestamp = sendSwitchingEvent(mFromUserId, mUserId);

        assertSwitchTime(timestamp, mFromUserId, mUserId);
    }

    @Test
    public void testUnlockingEvent_failure() {
        sendUnlockingEvent(mUserId);

        assertNoStartingMetric(mUserId);
    }

    @Test
    public void testUnlockingEvent_success() {
        sendStartingEvent(mUserId);
        long timestamp = sendUnlockingEvent(mUserId);

        assertUnlockingTime(timestamp, mUserId);
    }

    @Test
    public void testUnlockedEvent_failure() {
        sendUnlockedEvent(mUserId);

        assertNoStartingMetric(mUserId);
    }

    @Test
    public void testUnlockedEvent_success() {
        long timestamp = sendStartingEvent(mUserId);
        assertStartTime(timestamp, mUserId);
        sendUnlockedEvent(mUserId);

        // a successful unlocked event would have removed the metric
        assertNoStartingMetric(mUserId);
    }

    @Test
    public void testStopingEvent_success() {
        long timestamp = sendStopingEvent(mUserId);

        assertStopingTime(timestamp, mUserId);
    }

    @Test
    public void testStopingEvent_multipleCallsDifferentUser() {
        long timestamp1 = sendStopingEvent(mUserId);
        int userId = 12;
        long timestamp2 = sendStopingEvent(userId);

        assertStopingTime(timestamp1, mUserId);
        assertStopingTime(timestamp2, userId);
    }

    @Test
    public void testStopingEvent_multipleCallsSameUser() {
        long timestamp1 = sendStopingEvent(mUserId);
        assertStopingTime(timestamp1, mUserId);
        long timestamp2 = sendStopingEvent(mUserId);

        assertStopingTime(timestamp2, mUserId);
    }

    @Test
    public void testStoppedEvent_failure() {
        sendStoppedEvent(mUserId);

        assertNoStoppingMetric(mUserId);
    }

    @Test
    public void testStoppedEvent_success() {
        long timestamp = sendStopingEvent(mUserId);
        assertStopingTime(timestamp, mUserId);
        sendStoppedEvent(mUserId);

        // a successful stopped event would have removed the metric
        assertNoStoppingMetric(mUserId);
    }

    private long sendStartingEvent(@UserIdInt int userId) {
        long timestampMs = SystemClock.elapsedRealtimeNanos();
        mUserMetrics.onEvent(USER_LIFECYCLE_EVENT_TYPE_STARTING, timestampMs, /* fromUserId */ -1,
                userId);
        return timestampMs;
    }

    private long sendSwitchingEvent(@UserIdInt int fromUserId, @UserIdInt int userId) {
        long timestampMs = SystemClock.elapsedRealtimeNanos();
        mUserMetrics.onEvent(USER_LIFECYCLE_EVENT_TYPE_SWITCHING, timestampMs, fromUserId, userId);
        return timestampMs;
    }

    private long sendUnlockingEvent(@UserIdInt int userId) {
        long timestampMs = SystemClock.elapsedRealtimeNanos();
        mUserMetrics.onEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKING, timestampMs, /* fromUserId */ -1,
                userId);
        return timestampMs;
    }

    private long sendUnlockedEvent(@UserIdInt int userId) {
        long timestampMs = SystemClock.elapsedRealtimeNanos();
        mUserMetrics.onEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED, timestampMs, /* fromUserId */ -1,
                userId);
        return timestampMs;
    }

    private long sendStopingEvent(@UserIdInt int userId) {
        long timestampMs = SystemClock.elapsedRealtimeNanos();
        mUserMetrics.onEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPING, timestampMs, /* fromUserId */ -1,
                userId);
        return timestampMs;
    }

    private long sendStoppedEvent(@UserIdInt int userId) {
        long timestampMs = SystemClock.elapsedRealtimeNanos();
        mUserMetrics.onEvent(USER_LIFECYCLE_EVENT_TYPE_STOPPED, timestampMs, /* fromUserId */ -1,
                userId);
        return timestampMs;
    }

    private void assertStartTime(long timestamp, @UserIdInt int userId) {
        SparseArray<UserStartingMetric> startArray = mUserMetrics.getUserStartMetrics();
        UserStartingMetric metric = startArray.get(userId);
        assertThat(metric.startTime).isEqualTo(timestamp);
    }

    private void assertSwitchTime(long timestamp, @UserIdInt int fromUserId,
            @UserIdInt int userId) {
        SparseArray<UserStartingMetric> startArray = mUserMetrics.getUserStartMetrics();
        UserStartingMetric metric = startArray.get(userId);
        assertThat(metric.switchFromUserId).isEqualTo(fromUserId);
        assertThat(metric.switchTime).isEqualTo(timestamp);
    }

    private void assertUnlockingTime(long timestamp, int userId) {
        SparseArray<UserStartingMetric> startArray = mUserMetrics.getUserStartMetrics();
        UserStartingMetric metric = startArray.get(userId);
        assertThat(metric.unlockingTime).isEqualTo(timestamp);
    }

    private void assertNoStartingMetric(@UserIdInt int userId) {
        SparseArray<UserStartingMetric> startArray = mUserMetrics.getUserStartMetrics();
        assertThrows(NullPointerException.class, () -> startArray.get(userId));
    }

    private void assertStopingTime(long timestamp, @UserIdInt int userId) {
        SparseArray<UserStoppingMetric> stopArray = mUserMetrics.getUserStopMetrics();
        UserStoppingMetric metric = stopArray.get(userId);
        assertThat(metric.stopTime).isEqualTo(timestamp);
    }

    private void assertNoStoppingMetric(@UserIdInt int userId) {
        SparseArray<UserStoppingMetric> stopArray = mUserMetrics.getUserStopMetrics();
        assertThrows(NullPointerException.class, () -> stopArray.get(userId));
    }
}
