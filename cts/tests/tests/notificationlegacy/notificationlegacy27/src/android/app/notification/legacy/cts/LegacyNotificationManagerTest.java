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

package android.app.notification.legacy.cts;

import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_FULL_SCREEN_INTENT;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_LIGHTS;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_NOTIFICATION_LIST;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_PEEK;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_SCREEN_OFF;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_SCREEN_ON;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_STATUS_BAR;

import static junit.framework.Assert.assertEquals;

import static org.junit.Assert.assertTrue;

import android.app.ActivityManager;
import android.app.Instrumentation;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.ParcelFileDescriptor;
import android.provider.Telephony.Threads;
import android.service.notification.NotificationListenerService;

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
public class LegacyNotificationManagerTest {
    final String TAG = "LegacyNoManTest";

    final String NOTIFICATION_CHANNEL_ID = "LegacyNotificationManagerTest";
    private NotificationManager mNotificationManager;
    private Context mContext;

    private SecondaryNotificationListener mSecondaryListener;
    private TestNotificationListener mListener;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);
        toggleListenerAccess(SecondaryNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);
        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mNotificationManager.createNotificationChannel(new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", NotificationManager.IMPORTANCE_DEFAULT));
    }

    @After
    public void tearDown() throws Exception {
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);
        toggleListenerAccess(SecondaryNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);
        Thread.sleep(500); // wait for listener to disconnect
        assertTrue(mListener == null || !mListener.isConnected);
        assertTrue(mSecondaryListener == null || !mSecondaryListener.isConnected);
    }

    @Test
    public void testPrePCannotToggleAlarmsAndMediaTest() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        // Pre-P cannot toggle alarms and media
        NotificationManager.Policy origPolicy = mNotificationManager.getNotificationPolicy();
        int alarmBit = origPolicy.priorityCategories & NotificationManager.Policy
                .PRIORITY_CATEGORY_ALARMS;
        int mediaBit = origPolicy.priorityCategories & NotificationManager.Policy
                .PRIORITY_CATEGORY_MEDIA;
        int systemBit = origPolicy.priorityCategories & NotificationManager.Policy
                .PRIORITY_CATEGORY_SYSTEM;

        // attempt to toggle off alarms, media, system:
        mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(0, 0, 0));
        NotificationManager.Policy policy = mNotificationManager.getNotificationPolicy();
        assertEquals(alarmBit, policy.priorityCategories
                & NotificationManager.Policy.PRIORITY_CATEGORY_ALARMS);
        assertEquals(mediaBit, policy.priorityCategories
                & NotificationManager.Policy.PRIORITY_CATEGORY_MEDIA);
        assertEquals(systemBit, policy.priorityCategories
                & NotificationManager.Policy.PRIORITY_CATEGORY_SYSTEM);

        // attempt to toggle on alarms, media, system:
        mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                NotificationManager.Policy.PRIORITY_CATEGORY_ALARMS
                        | NotificationManager.Policy.PRIORITY_CATEGORY_MEDIA, 0, 0));
        policy = mNotificationManager.getNotificationPolicy();
        assertEquals(alarmBit, policy.priorityCategories
                & NotificationManager.Policy.PRIORITY_CATEGORY_ALARMS);
        assertEquals(mediaBit, policy.priorityCategories
                & NotificationManager.Policy.PRIORITY_CATEGORY_MEDIA);
        assertEquals(systemBit, policy.priorityCategories
                & NotificationManager.Policy.PRIORITY_CATEGORY_SYSTEM);

        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), false);
    }

    @Test
    public void testSetNotificationPolicy_preP_setOldFields() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        NotificationManager.Policy userPolicy = mNotificationManager.getNotificationPolicy();

        NotificationManager.Policy appPolicy = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_SCREEN_OFF);
        mNotificationManager.setNotificationPolicy(appPolicy);

        int expected = userPolicy.suppressedVisualEffects
                | SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_SCREEN_OFF
                | SUPPRESSED_EFFECT_PEEK | SUPPRESSED_EFFECT_LIGHTS
                | SUPPRESSED_EFFECT_FULL_SCREEN_INTENT;

        assertEquals(expected,
                mNotificationManager.getNotificationPolicy().suppressedVisualEffects);
    }

    @Test
    public void testSetNotificationPolicy_preP_setNewFields() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        NotificationManager.Policy userPolicy = mNotificationManager.getNotificationPolicy();

        NotificationManager.Policy appPolicy = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_NOTIFICATION_LIST);
        mNotificationManager.setNotificationPolicy(appPolicy);

        int expected = userPolicy.suppressedVisualEffects;
        expected &= ~ SUPPRESSED_EFFECT_SCREEN_OFF & ~ SUPPRESSED_EFFECT_SCREEN_ON;
        assertEquals(expected,
                mNotificationManager.getNotificationPolicy().suppressedVisualEffects);

        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), false);
    }

    @Test
    public void testSuspendPackage() throws Exception {
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        Assert.assertNotNull(mListener);

        sendNotification(1, R.drawable.icon_black);
        Thread.sleep(500); // wait for notification listener to receive notification
        assertEquals(1, mListener.mPosted.size());
        mListener.resetData();

        // suspend package, listener receives onRemoved
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                true);
        Thread.sleep(500); // wait for notification listener to get response
        assertEquals(1, mListener.mRemoved.size());

        // unsuspend package, listener receives onPosted
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                false);
        Thread.sleep(500); // wait for notification listener to get response
        assertEquals(1, mListener.mPosted.size());

        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);

        mListener.resetData();
    }

    @Test
    public void testSuspendedPackageSendNotification() throws Exception {
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        Assert.assertNotNull(mListener);

        // suspend package
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                true);
        Thread.sleep(500); // wait for notification listener to get response

        sendNotification(1, R.drawable.icon_black);
        Thread.sleep(500); // wait for notification listener in case it receives notification
        assertEquals(0, mListener.mPosted.size()); // shouldn't see any notifications posted

        // unsuspend package, listener should receive onPosted
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                false);
        Thread.sleep(500); // wait for notification listener to get response
        assertEquals(1, mListener.mPosted.size());

        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), false);

        mListener.resetData();
    }

    @Test
    public void testResetListenerHints_singleListener() throws Exception {
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        Assert.assertNotNull(mListener);

        mListener.requestListenerHints(NotificationListenerService.HINT_HOST_DISABLE_CALL_EFFECTS);

        assertEquals(NotificationListenerService.HINT_HOST_DISABLE_CALL_EFFECTS,
                mListener.getCurrentListenerHints());

        mListener.clearRequestedListenerHints();

        assertEquals(0, mListener.getCurrentListenerHints());
    }

    @Test
    public void testResetListenerHints_multiListener() throws Exception {
        toggleListenerAccess(TestNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), true);
        toggleListenerAccess(SecondaryNotificationListener.getId(),
                InstrumentationRegistry.getInstrumentation(), true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        mSecondaryListener = SecondaryNotificationListener.getInstance();
        Assert.assertNotNull(mListener);
        Assert.assertNotNull(mSecondaryListener);

        mListener.requestListenerHints(NotificationListenerService.HINT_HOST_DISABLE_CALL_EFFECTS);
        mSecondaryListener.requestListenerHints(
                NotificationListenerService.HINT_HOST_DISABLE_CALL_EFFECTS
                        | NotificationListenerService.HINT_HOST_DISABLE_NOTIFICATION_EFFECTS);

        assertEquals(NotificationListenerService.HINT_HOST_DISABLE_CALL_EFFECTS
                | NotificationListenerService.HINT_HOST_DISABLE_NOTIFICATION_EFFECTS,
                mListener.getCurrentListenerHints());

        mSecondaryListener.clearRequestedListenerHints();
        assertEquals(NotificationListenerService.HINT_HOST_DISABLE_CALL_EFFECTS,
                mSecondaryListener.getCurrentListenerHints());
    }

    @Test
    public void testSetNotificationPolicy_preP_setOldNewFields() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        NotificationManager.Policy userPolicy = mNotificationManager.getNotificationPolicy();

        NotificationManager.Policy appPolicy = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_STATUS_BAR);
        mNotificationManager.setNotificationPolicy(appPolicy);

        int expected = userPolicy.suppressedVisualEffects
                | SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_PEEK;
        expected &= ~ SUPPRESSED_EFFECT_SCREEN_OFF;
        assertEquals(expected,
                mNotificationManager.getNotificationPolicy().suppressedVisualEffects);

        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), false);
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

    private void suspendPackage(String packageName,
            Instrumentation instrumentation, boolean suspend) throws IOException {
        String command = " cmd package"
                       + " " + (suspend ? "suspend" : "unsuspend")
                       + " --user " + mContext.getUserId()
                       + " " + packageName;

        runCommand(command, instrumentation);
    }

    private void toggleListenerAccess(String componentName, Instrumentation instrumentation,
            boolean on) throws IOException {

        String command = " cmd notification"
                       + " " + (on ? "allow_listener" : "disallow_listener")
                       + " " + componentName
                       + " " + mContext.getUserId();

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
}
