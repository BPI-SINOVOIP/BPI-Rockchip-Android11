/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.app.notification.legacy29.cts;

import static android.app.NotificationManager.Policy.CONVERSATION_SENDERS_ANYONE;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_CONVERSATIONS;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;
import static junit.framework.TestCase.assertNotNull;
import static junit.framework.TestCase.assertNull;

import android.app.Instrumentation;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.UiAutomation;
import android.content.Context;
import android.content.Intent;
import android.os.ParcelFileDescriptor;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

/**
 * Home for tests that need to verify behavior for apps that target old sdk versions.
 */
@RunWith(AndroidJUnit4.class)
public class NotificationManager29Test {
    final String TAG = "LegacyNoManTest29";

    final String NOTIFICATION_CHANNEL_ID = "LegacyNoManTest29";
    private NotificationManager mNotificationManager;
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();

        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mNotificationManager.createNotificationChannel(new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", NotificationManager.IMPORTANCE_DEFAULT));
    }

    private void toggleNotificationPolicyAccess(String packageName,
            Instrumentation instrumentation, boolean on) throws IOException {

        String command = " cmd notification"
                + " " + (on ? "allow_dnd" : "disallow_dnd")
                + " " + packageName
                + " " + mContext.getUserId();

        runCommand(command, instrumentation);

        NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        Assert.assertEquals("Notification Policy Access Grant is " +
                        nm.isNotificationPolicyAccessGranted() + " not " + on, on,
                nm.isNotificationPolicyAccessGranted());
    }

    private void runCommand(String command, Instrumentation instrumentation) throws IOException {
        UiAutomation uiAutomation = instrumentation.getUiAutomation();
        // Execute command
        try (ParcelFileDescriptor fd = uiAutomation.executeShellCommand(command)) {
            Assert.assertNotNull("Failed to execute shell command: " + command, fd);
            // Wait for the command to finish by reading until EOF
            try (InputStream in = new FileInputStream(fd.getFileDescriptor())) {
                byte[] buffer = new byte[4096];
                while (in.read(buffer) > 0) {}
            } catch (IOException e) {
                throw new IOException("Could not read stdout of command: " + command, e);
            }
        } finally {
            uiAutomation.destroy();
        }
    }

    private StatusBarNotification findPostedNotification(int id) {
        // notification is a bit asynchronous so it may take a few ms to appear in
        // getActiveNotifications()
        // we will check for it for up to 500ms before giving up
        StatusBarNotification n = null;
        for (int tries = 5; tries--> 0;) {
            final StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
            for (StatusBarNotification sbn : sbns) {
                Log.d(TAG, "Found " + sbn.getKey());
                if (sbn.getId() == id) {
                    n = sbn;
                    break;
                }
            }
            if (n != null) break;
            try {
                Thread.sleep(100);
            } catch (InterruptedException ex) {
                // pass
            }
        }
        return n;
    }

    private PendingIntent getPendingIntent() {
        return PendingIntent.getActivity(
                mContext, 0, new Intent(mContext, this.getClass()), 0);
    }


    @Test
    public void testPostFullScreenIntent_noPermission() {
        // no permission? no full screen intent
        int id = 6000;
        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(android.R.id.icon)
                        .setWhen(System.currentTimeMillis())
                        .setFullScreenIntent(getPendingIntent(), true)
                        .setContentText("This is #FSI notification")
                        .setContentIntent(getPendingIntent())
                        .build();
        mNotificationManager.notify(id, notification);

        StatusBarNotification n = findPostedNotification(id);
        assertNotNull(n);
        assertNull(n.getNotification().fullScreenIntent);
    }

    @Test
    public void testApi29CannotToggleConversationsTest() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        // Pre-30 cannot toggle conversations on
        NotificationManager.Policy origPolicy = mNotificationManager.getNotificationPolicy();
        if ((origPolicy.priorityCategories & PRIORITY_CATEGORY_CONVERSATIONS) != 0) {
            return;
        }
        try {
            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                    PRIORITY_CATEGORY_CONVERSATIONS, 0, 0, 0, CONVERSATION_SENDERS_ANYONE));
            NotificationManager.Policy policy = mNotificationManager.getNotificationPolicy();
            assertEquals(origPolicy.priorityConversationSenders,
                    policy.priorityConversationSenders);
            assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_CONVERSATIONS) == 0);
        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), false);
        }
    }

    @Test
    public void testApi29CannotToggleConversationsOffTest() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        // Pre-30 cannot toggle conversations
        NotificationManager.Policy origPolicy = mNotificationManager.getNotificationPolicy();
        // can't test that we can't turn it off if it's not on in the first place
        if ((origPolicy.priorityCategories & PRIORITY_CATEGORY_CONVERSATIONS) == 0) {
            return;
        }
        try {
            // attempt to toggle off conversations:
            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(0, 0, 0));
            NotificationManager.Policy policy = mNotificationManager.getNotificationPolicy();
            assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_CONVERSATIONS) != 0);
        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), false);
        }
    }
}
