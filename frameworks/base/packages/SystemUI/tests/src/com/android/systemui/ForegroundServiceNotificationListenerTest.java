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

package com.android.systemui;

import static com.android.systemui.ForegroundServiceLifetimeExtender.MIN_FGS_TIME_MS;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Notification;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.systemui.statusbar.NotificationInteractionTracker;
import com.android.systemui.statusbar.notification.collection.NotificationEntry;
import com.android.systemui.statusbar.notification.collection.NotificationEntryBuilder;
import com.android.systemui.util.time.FakeSystemClock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class ForegroundServiceNotificationListenerTest extends SysuiTestCase {
    private ForegroundServiceLifetimeExtender mExtender;
    private NotificationEntry mEntry;
    private Notification mNotif;
    private final FakeSystemClock mClock = new FakeSystemClock();

    @Mock
    private NotificationInteractionTracker mInteractionTracker;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mExtender = new ForegroundServiceLifetimeExtender(mInteractionTracker, mClock);

        mNotif = new Notification.Builder(mContext, "")
                .setSmallIcon(R.drawable.ic_person)
                .setContentTitle("Title")
                .setContentText("Text")
                .build();

        mEntry = new NotificationEntryBuilder()
                .setCreationTime(mClock.uptimeMillis())
                .setNotification(mNotif)
                .build();
    }

    /**
     * ForegroundServiceLifetimeExtenderTest
     */
    @Test
    public void testShouldExtendLifetime_should_foreground() {
        // Extend the lifetime of a FGS notification iff it has not been visible
        // for the minimum time
        mNotif.flags |= Notification.FLAG_FOREGROUND_SERVICE;

        // No time has elapsed, keep showing
        assertTrue(mExtender.shouldExtendLifetime(mEntry));
    }

    @Test
    public void testShouldExtendLifetime_shouldNot_foreground() {
        mNotif.flags |= Notification.FLAG_FOREGROUND_SERVICE;

        // Entry was created at mClock.uptimeMillis(), advance it MIN_FGS_TIME_MS + 1
        mClock.advanceTime(MIN_FGS_TIME_MS + 1);
        assertFalse(mExtender.shouldExtendLifetime(mEntry));
    }

    @Test
    public void testShouldExtendLifetime_shouldNot_notForeground() {
        mNotif.flags = 0;

        // Entry was created at mClock.uptimeMillis(), advance it MIN_FGS_TIME_MS + 1
        mClock.advanceTime(MIN_FGS_TIME_MS + 1);
        assertFalse(mExtender.shouldExtendLifetime(mEntry));
    }

    @Test
    public void testShouldExtendLifetime_shouldNot_interruped() {
        // GIVEN a notification that would trigger lifetime extension
        mNotif.flags |= Notification.FLAG_FOREGROUND_SERVICE;

        // GIVEN the notification has alerted
        mEntry.setInterruption();

        // THEN the notification does not need to have its lifetime extended by this extender
        assertFalse(mExtender.shouldExtendLifetime(mEntry));
    }
}
