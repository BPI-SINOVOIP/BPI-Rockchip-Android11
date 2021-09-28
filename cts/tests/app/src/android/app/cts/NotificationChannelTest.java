/*
 * Copyright (C) 2016 The Android Open Source Project
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

import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.app.NotificationManager.IMPORTANCE_HIGH;
import static android.app.NotificationManager.IMPORTANCE_UNSPECIFIED;

import android.app.Notification;
import android.app.NotificationChannel;
import android.graphics.Color;
import android.media.AudioAttributes;
import android.net.Uri;
import android.os.Parcel;
import android.provider.Settings;
import android.test.AndroidTestCase;

public class NotificationChannelTest extends AndroidTestCase {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
    }

    public void testDescribeContents() {
        final int expected = 0;
        NotificationChannel channel =
                new NotificationChannel("1", "1", IMPORTANCE_DEFAULT);
        assertEquals(expected, channel.describeContents());
    }

    public void testConstructor() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_HIGH);
        assertEquals("1", channel.getId());
        assertEquals("one", channel.getName());
        assertEquals(null, channel.getDescription());
        assertEquals(false, channel.canBypassDnd());
        assertEquals(false, channel.shouldShowLights());
        assertEquals(false, channel.shouldVibrate());
        assertEquals(null, channel.getVibrationPattern());
        assertEquals(IMPORTANCE_HIGH, channel.getImportance());
        assertEquals(Settings.System.DEFAULT_NOTIFICATION_URI, channel.getSound());
        assertTrue(channel.canShowBadge());
        assertEquals(Notification.AUDIO_ATTRIBUTES_DEFAULT, channel.getAudioAttributes());
        assertEquals(null, channel.getGroup());
        assertTrue(channel.getLightColor() == 0);
        assertFalse(channel.canBubble());
        assertFalse(channel.isImportanceLockedByOEM());
        assertEquals(IMPORTANCE_UNSPECIFIED, channel.getOriginalImportance());
        assertNull(channel.getConversationId());
        assertNull(channel.getParentChannelId());
        assertFalse(channel.isImportantConversation());
    }

    public void testWriteToParcel() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setBypassDnd(true);
        channel.setOriginalImportance(IMPORTANCE_HIGH);
        channel.setShowBadge(false);
        channel.setAllowBubbles(false);
        channel.setGroup("a thing");
        channel.setSound(Uri.fromParts("a", "b", "c"),
                new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_NOTIFICATION_EVENT)
                        .build());
        channel.setLightColor(Color.RED);
        channel.setDeleted(true);
        channel.setFgServiceShown(true);
        channel.setVibrationPattern(new long[] {299, 4562});
        channel.setBlockable(true);
        channel.setConversationId("parent_channel", "conversation 1");
        channel.setImportantConversation(true);
        Parcel parcel = Parcel.obtain();
        channel.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        NotificationChannel channel1 = NotificationChannel.CREATOR.createFromParcel(parcel);
        assertEquals(channel, channel1);
    }

    public void testName() {
        NotificationChannel channel = new NotificationChannel("a", "ab", IMPORTANCE_DEFAULT);
        channel.setName("new name");
        assertEquals("new name", channel.getName());
    }

    public void testDescription() {
        NotificationChannel channel = new NotificationChannel("a", "ab", IMPORTANCE_DEFAULT);
        channel.setDescription("success");
        assertEquals("success", channel.getDescription());
    }

    public void testLights() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.enableLights(true);
        assertTrue(channel.shouldShowLights());
        channel.enableLights(false);
        assertFalse(channel.shouldShowLights());
    }

    public void testLightColor() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setLightColor(Color.RED);
        assertFalse(channel.shouldShowLights());
        assertEquals(Color.RED, channel.getLightColor());
    }

    public void testVibration() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.enableVibration(true);
        assertTrue(channel.shouldVibrate());
        channel.enableVibration(false);
        assertFalse(channel.shouldVibrate());
    }

    public void testVibrationPattern() {
        final long[] pattern = new long[] {1, 7, 1, 7, 3};
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        assertNull(channel.getVibrationPattern());
        channel.setVibrationPattern(pattern);
        assertEquals(pattern, channel.getVibrationPattern());
        assertTrue(channel.shouldVibrate());

        channel.setVibrationPattern(new long[]{});
        assertEquals(false, channel.shouldVibrate());

        channel.setVibrationPattern(null);
        assertEquals(false, channel.shouldVibrate());
    }

    public void testSound() {
        Uri expected = new Uri.Builder().scheme("fruit").appendQueryParameter("favorite", "bananas")
                .build();
        AudioAttributes attributes = new AudioAttributes.Builder()
                .setContentType(AudioAttributes.CONTENT_TYPE_UNKNOWN)
                .setUsage(AudioAttributes.USAGE_NOTIFICATION_RINGTONE)
                .setFlags(AudioAttributes.FLAG_AUDIBILITY_ENFORCED)
                .build();
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setSound(expected, attributes);
        assertEquals(expected, channel.getSound());
        assertEquals(attributes, channel.getAudioAttributes());
    }

    public void testShowBadge() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setShowBadge(true);
        assertTrue(channel.canShowBadge());
    }

    public void testGroup() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setGroup("banana");
        assertEquals("banana", channel.getGroup());
    }

    public void testBubble() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setAllowBubbles(true);
        assertEquals(true, channel.canBubble());

        channel = new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setAllowBubbles(false);
        assertEquals(false, channel.canBubble());
    }

    public void testIsBlockableSystem() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setBlockable(true);
        assertTrue(channel.isBlockable());
    }

    public void testIsImportanceLockedByOEM() {
        NotificationChannel channel =
                new NotificationChannel("1", "one", IMPORTANCE_DEFAULT);
        channel.setImportanceLockedByOEM(true);
        assertTrue(channel.isImportanceLockedByOEM());
    }

    public void testSystemBlockable() {
        NotificationChannel channel = new NotificationChannel("a", "ab", IMPORTANCE_DEFAULT);
        assertEquals(false, channel.isBlockable());
        channel.setBlockable(true);
        assertEquals(true, channel.isBlockable());
    }

    public void testOriginalImportance() {
        NotificationChannel channel = new NotificationChannel("a", "ab", IMPORTANCE_DEFAULT);
        channel.setOriginalImportance(IMPORTANCE_HIGH);
        assertEquals(IMPORTANCE_HIGH, channel.getOriginalImportance());
    }

    public void testConversation() {
        NotificationChannel channel = new NotificationChannel("a", "ab", IMPORTANCE_DEFAULT);
        channel.setConversationId("parent", "conversation");

        assertEquals("parent", channel.getParentChannelId());
        assertEquals("conversation", channel.getConversationId());
        assertFalse(channel.isImportantConversation());

        channel.setImportantConversation(true);
        assertTrue(channel.isImportantConversation());
    }

    public void testHasUserSetSound() {
        NotificationChannel channel = new NotificationChannel("a", "a", IMPORTANCE_DEFAULT);
        channel.lockFields(NotificationChannel.USER_LOCKED_SOUND);

        assertTrue(channel.hasUserSetSound());
    }
}
