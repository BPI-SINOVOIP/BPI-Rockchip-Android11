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

package android.app.cts;

import static android.service.notification.NotificationStats.DISMISSAL_PEEK;
import static android.service.notification.NotificationStats.DISMISS_SENTIMENT_NEGATIVE;

import android.os.Parcel;
import android.service.notification.NotificationStats;
import android.test.AndroidTestCase;


/** Test the public NotificationStats api. */
public class NotificationStatsTest extends AndroidTestCase {

    public void testGetDismissalSentiment() {
        NotificationStats notificationStats = new NotificationStats();

        // Starts at unknown. Cycle through all of the options and back.
        assertEquals(NotificationStats.DISMISS_SENTIMENT_UNKNOWN,
                notificationStats.getDismissalSentiment());
        notificationStats.setDismissalSentiment(NotificationStats.DISMISS_SENTIMENT_NEGATIVE);
        assertEquals(NotificationStats.DISMISS_SENTIMENT_NEGATIVE,
                notificationStats.getDismissalSentiment());
        notificationStats.setDismissalSentiment(NotificationStats.DISMISS_SENTIMENT_NEUTRAL);
        assertEquals(NotificationStats.DISMISS_SENTIMENT_NEUTRAL,
                notificationStats.getDismissalSentiment());
        notificationStats.setDismissalSentiment(NotificationStats.DISMISS_SENTIMENT_POSITIVE);
        assertEquals(NotificationStats.DISMISS_SENTIMENT_POSITIVE,
                notificationStats.getDismissalSentiment());
        notificationStats.setDismissalSentiment(NotificationStats.DISMISS_SENTIMENT_UNKNOWN);
        assertEquals(NotificationStats.DISMISS_SENTIMENT_UNKNOWN,
                notificationStats.getDismissalSentiment());
    }

    public void testConstructor() {
        NotificationStats stats = new NotificationStats();

        assertFalse(stats.hasSeen());
        assertFalse(stats.hasDirectReplied());
        assertFalse(stats.hasExpanded());
        assertFalse(stats.hasInteracted());
        assertFalse(stats.hasViewedSettings());
        assertFalse(stats.hasSnoozed());
        assertEquals(NotificationStats.DISMISSAL_NOT_DISMISSED, stats.getDismissalSurface());
        assertEquals(NotificationStats.DISMISS_SENTIMENT_UNKNOWN, stats.getDismissalSentiment());
    }

    public void testSeen() {
        NotificationStats stats = new NotificationStats();
        stats.setSeen();
        assertTrue(stats.hasSeen());
        assertFalse(stats.hasInteracted());
    }

    public void testDirectReplied() {
        NotificationStats stats = new NotificationStats();
        stats.setDirectReplied();
        assertTrue(stats.hasDirectReplied());
        assertTrue(stats.hasInteracted());
    }

    public void testExpanded() {
        NotificationStats stats = new NotificationStats();
        stats.setExpanded();
        assertTrue(stats.hasExpanded());
        assertTrue(stats.hasInteracted());
    }

    public void testSnoozed() {
        NotificationStats stats = new NotificationStats();
        stats.setSnoozed();
        assertTrue(stats.hasSnoozed());
        assertTrue(stats.hasInteracted());
    }

    public void testViewedSettings() {
        NotificationStats stats = new NotificationStats();
        stats.setViewedSettings();
        assertTrue(stats.hasViewedSettings());
        assertTrue(stats.hasInteracted());
    }

    public void testDismissalSurface() {
        NotificationStats stats = new NotificationStats();
        stats.setDismissalSurface(DISMISSAL_PEEK);
        assertEquals(DISMISSAL_PEEK, stats.getDismissalSurface());
        assertFalse(stats.hasInteracted());
    }

    public void testDismissalSentiment() {
        NotificationStats stats = new NotificationStats();
        stats.setDismissalSentiment(DISMISS_SENTIMENT_NEGATIVE);
        assertEquals(DISMISS_SENTIMENT_NEGATIVE, stats.getDismissalSentiment());
        assertFalse(stats.hasInteracted());
    }

    public void testWriteToParcel() {
        NotificationStats stats = new NotificationStats();
        stats.setViewedSettings();
        stats.setDismissalSurface(NotificationStats.DISMISSAL_AOD);
        stats.setDismissalSentiment(NotificationStats.DISMISS_SENTIMENT_POSITIVE);
        Parcel parcel = Parcel.obtain();
        stats.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        NotificationStats stats1 = NotificationStats.CREATOR.createFromParcel(parcel);
        assertEquals(stats, stats1);
    }
}
