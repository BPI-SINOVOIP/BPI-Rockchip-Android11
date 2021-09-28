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

package android.app.notification.legacy20.cts;

import static junit.framework.Assert.fail;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Instrumentation;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.provider.Telephony.Threads;
import android.service.notification.StatusBarNotification;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import junit.framework.Assert;

import org.junit.After;
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
public class LegacyNotificationManager20Test {
    final String TAG = "LegacyNoMan20Test";

    private PackageManager mPackageManager;
    final String NOTIFICATION_CHANNEL_ID = "LegacyNotificationManagerTest";
    private NotificationManager mNotificationManager;
    private Context mContext;

    private TestNotificationListener mListener;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);
        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mNotificationManager.createNotificationChannel(new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", NotificationManager.IMPORTANCE_DEFAULT));
        mPackageManager = mContext.getPackageManager();
    }

    @After
    public void tearDown() throws Exception {
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);
        Thread.sleep(500); // wait for listener to disconnect
        assertTrue(mListener == null || !mListener.isConnected);
    }

    @Test
    public void testNotificationListener_cancelNotifications() throws Exception {
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);
        final int notificationId = 1;

        sendNotification(notificationId, R.drawable.icon_black);
        Thread.sleep(500); // wait for notification listener to receive notification

        StatusBarNotification sbn = findPostedNotification(notificationId);

        mListener.cancelNotification(sbn.getPackageName(), sbn.getTag(), sbn.getId());
        if (mContext.getApplicationInfo().targetSdkVersion < Build.VERSION_CODES.LOLLIPOP) {
            if (checkNotificationExistence(notificationId, /*shouldExist=*/ true)) {
                fail("Failed to cancel notification. targetSdk="
                        + mContext.getApplicationInfo().targetSdkVersion);
            }
        }

        sendNotification(notificationId, R.drawable.icon_black);
        Thread.sleep(500); // wait for notification listener to receive notification

        mListener.cancelNotifications(new String[]{ sbn.getKey() });
        if (!checkNotificationExistence(notificationId, /*shouldExist=*/ false)) {
            fail("Failed to cancel notification id=" + notificationId);
        }
    }

    private void sendNotification(final int id, final int icon) throws Exception {
        sendNotification(id, null, icon);
    }

    private void sendNotification(final int id, String groupKey, final int icon) throws Exception {
        final Intent intent = new Intent(Intent.ACTION_MAIN, Threads.CONTENT_URI);

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intent.setAction(Intent.ACTION_MAIN);

        final PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);
        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(icon)
                        .setWhen(System.currentTimeMillis())
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .setContentIntent(pendingIntent)
                        .setGroup(groupKey)
                        .build();
        mNotificationManager.notify(id, notification);
    }

    private void toggleNotificationPolicyAccess(String packageName,
            Instrumentation instrumentation, boolean on) throws IOException {

        String command = " cmd notification " + (on ? "allow_dnd " : "disallow_dnd ") + packageName;

        runCommand(command, instrumentation);

        NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        Assert.assertEquals("Notification Policy Access Grant is " +
                        nm.isNotificationPolicyAccessGranted() + " not " + on, on,
                nm.isNotificationPolicyAccessGranted());
    }

    private void suspendPackage(String packageName,
            Instrumentation instrumentation, boolean suspend) throws IOException {
        String command = " cmd package " + (suspend ? "suspend "
                : "unsuspend ") + packageName;

        runCommand(command, instrumentation);
    }

    private void toggleListenerAccess(String componentName, Instrumentation instrumentation,
            boolean on) throws IOException {

        String command = " cmd notification " + (on ? "allow_listener " : "disallow_listener ")
                + componentName;

        runCommand(command, instrumentation);

        final NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        final ComponentName listenerComponent = TestNotificationListener.getComponentName();
        Assert.assertTrue(listenerComponent + " has not been granted access",
                nm.isNotificationListenerAccessGranted(listenerComponent) == on);
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

    private boolean checkNotificationExistence(int id, boolean shouldExist) {
        // notification is a bit asynchronous so it may take a few ms to appear in
        // getActiveNotifications()
        // we will check for it for up to 300ms before giving up
        boolean found = false;
        for (int tries = 3; tries--> 0;) {
            // Need reset flag.
            found = false;
            final StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
            for (StatusBarNotification sbn : sbns) {
                Log.d(TAG, "Found " + sbn.getKey());
                if (sbn.getId() == id) {
                    found = true;
                    break;
                }
            }
            if (found == shouldExist) break;
            try {
                Thread.sleep(100);
            } catch (InterruptedException ex) {
                // pass
            }
        }
        return found == shouldExist;
    }

    private StatusBarNotification findPostedNotification(int id) {
        // notification is a bit asynchronous so it may take a few ms to appear in
        // getActiveNotifications()
        // we will check for it for up to 300ms before giving up
        StatusBarNotification n = null;
        for (int tries = 3; tries-- > 0; ) {
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
}
