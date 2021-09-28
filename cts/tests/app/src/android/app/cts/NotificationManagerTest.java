/*
 * Copyright (C) 2008 The Android Open Source Project
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

import static android.app.Notification.FLAG_BUBBLE;
import static android.app.NotificationManager.IMPORTANCE_DEFAULT;
import static android.app.NotificationManager.IMPORTANCE_HIGH;
import static android.app.NotificationManager.IMPORTANCE_LOW;
import static android.app.NotificationManager.IMPORTANCE_MIN;
import static android.app.NotificationManager.IMPORTANCE_NONE;
import static android.app.NotificationManager.IMPORTANCE_UNSPECIFIED;
import static android.app.NotificationManager.INTERRUPTION_FILTER_ALARMS;
import static android.app.NotificationManager.INTERRUPTION_FILTER_ALL;
import static android.app.NotificationManager.INTERRUPTION_FILTER_NONE;
import static android.app.NotificationManager.INTERRUPTION_FILTER_PRIORITY;
import static android.app.NotificationManager.Policy.CONVERSATION_SENDERS_ANYONE;
import static android.app.NotificationManager.Policy.CONVERSATION_SENDERS_NONE;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_ALARMS;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_CALLS;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_CONVERSATIONS;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_EVENTS;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_MEDIA;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_MESSAGES;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_REMINDERS;
import static android.app.NotificationManager.Policy.PRIORITY_CATEGORY_SYSTEM;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_AMBIENT;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_FULL_SCREEN_INTENT;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_LIGHTS;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_NOTIFICATION_LIST;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_PEEK;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_SCREEN_OFF;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_SCREEN_ON;
import static android.app.NotificationManager.Policy.SUPPRESSED_EFFECT_STATUS_BAR;
import static android.app.stubs.BubblesTestService.EXTRA_TEST_CASE;
import static android.app.stubs.BubblesTestService.TEST_CALL;
import static android.app.stubs.BubblesTestService.TEST_MESSAGING;
import static android.app.stubs.SendBubbleActivity.BUBBLE_NOTIF_ID;
import static android.content.Intent.FLAG_ACTIVITY_MULTIPLE_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.content.pm.PackageManager.FEATURE_WATCH;

import android.app.ActivityManager;
import android.app.AutomaticZenRule;
import android.app.Instrumentation;
import android.app.KeyguardManager;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationChannelGroup;
import android.app.NotificationManager;
import android.app.NotificationManager.Policy;
import android.app.PendingIntent;
import android.app.Person;
import android.app.UiAutomation;
import android.app.stubs.AutomaticZenRuleActivity;
import android.app.stubs.BubbledActivity;
import android.app.stubs.BubblesTestService;
import android.app.stubs.R;
import android.app.stubs.SendBubbleActivity;
import android.app.stubs.TestNotificationListener;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.OperationApplicationException;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.pm.ShortcutInfo;
import android.content.pm.ShortcutManager;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.session.MediaSession;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.SystemClock;
import android.os.UserHandle;
import android.provider.ContactsContract;
import android.provider.ContactsContract.CommonDataKinds.Email;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.CommonDataKinds.StructuredName;
import android.provider.ContactsContract.Data;
import android.provider.Settings;
import android.provider.Telephony.Threads;
import android.service.notification.Condition;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import android.service.notification.ZenPolicy;
import android.test.AndroidTestCase;
import android.util.ArraySet;
import android.util.Log;
import android.widget.RemoteViews;

import androidx.annotation.NonNull;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.FeatureUtil;
import com.android.compatibility.common.util.SystemUtil;
import com.android.test.notificationlistener.INotificationUriAccessService;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/* This tests NotificationListenerService together with NotificationManager, as you need to have
 * notifications to manipulate in order to test the listener service. */
public class NotificationManagerTest extends AndroidTestCase {
    public static final String NOTIFICATIONPROVIDER = "com.android.test.notificationprovider";
    public static final String RICH_NOTIFICATION_ACTIVITY =
            "com.android.test.notificationprovider.RichNotificationActivity";
    final String TAG = NotificationManagerTest.class.getSimpleName();
    final boolean DEBUG = false;
    static final String NOTIFICATION_CHANNEL_ID = "NotificationManagerTest";

    private static final String DELEGATOR = "com.android.test.notificationdelegator";
    private static final String DELEGATE_POST_CLASS = DELEGATOR + ".NotificationDelegateAndPost";
    private static final String REVOKE_CLASS = DELEGATOR + ".NotificationRevoker";
    private static final long SHORT_WAIT_TIME = 100;
    private static final long MAX_WAIT_TIME = 2000;
    private static final String SHARE_SHORTCUT_ID = "shareShortcut";
    private static final String SHARE_SHORTCUT_CATEGORY =
            "android.app.stubs.SHARE_SHORTCUT_CATEGORY";

    private PackageManager mPackageManager;
    private AudioManager mAudioManager;
    private NotificationManager mNotificationManager;
    private ActivityManager mActivityManager;
    private String mId;
    private TestNotificationListener mListener;
    private List<String> mRuleIds;
    private BroadcastReceiver mBubbleBroadcastReceiver;
    private boolean mBubblesEnabledSettingToRestore;
    private INotificationUriAccessService mNotificationUriAccessService;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        // This will leave a set of channels on the device with each test run.
        mId = UUID.randomUUID().toString();
        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        // clear the deck so that our getActiveNotifications results are predictable
        mNotificationManager.cancelAll();

        assertEquals("Previous test left system in a bad state",
                0, mNotificationManager.getActiveNotifications().length);

        mNotificationManager.createNotificationChannel(new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", IMPORTANCE_DEFAULT));
        mActivityManager = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
        mPackageManager = mContext.getPackageManager();
        mAudioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        mRuleIds = new ArrayList<>();

        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);
        mNotificationManager.setInterruptionFilter(INTERRUPTION_FILTER_ALL);
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), false);

        // This setting is forced on / off for certain tests, save it & restore what's on the
        // device after tests are run
        mBubblesEnabledSettingToRestore = Settings.Global.getInt(mContext.getContentResolver(),
                Settings.Global.NOTIFICATION_BUBBLES) == 1;

        // delay between tests so notifications aren't dropped by the rate limiter
        try {
            Thread.sleep(500);
        } catch (InterruptedException e) {
        }
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        mNotificationManager.cancelAll();

        for (String id : mRuleIds) {
            mNotificationManager.removeAutomaticZenRule(id);
        }

        assertExpectedDndState(INTERRUPTION_FILTER_ALL);

        List<NotificationChannel> channels = mNotificationManager.getNotificationChannels();
        // Delete all channels.
        for (NotificationChannel nc : channels) {
            if (NotificationChannel.DEFAULT_CHANNEL_ID.equals(nc.getId())) {
                continue;
            }
            mNotificationManager.deleteNotificationChannel(nc.getId());
        }

        // Unsuspend package if it was suspended in the test
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                false);

        toggleListenerAccess(false);
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), false);

        List<NotificationChannelGroup> groups = mNotificationManager.getNotificationChannelGroups();
        // Delete all groups.
        for (NotificationChannelGroup ncg : groups) {
            mNotificationManager.deleteNotificationChannelGroup(ncg.getId());
        }

        // Restore bubbles setting
        setBubblesGlobal(mBubblesEnabledSettingToRestore);
    }

    private void assertNotificationCancelled(int id, boolean all) {
        for (long totalWait = 0; totalWait < MAX_WAIT_TIME; totalWait += SHORT_WAIT_TIME) {
            StatusBarNotification sbn = findNotificationNoWait(id, all);
            if (sbn == null) return;
            try {
                Thread.sleep(SHORT_WAIT_TIME);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        assertNull(findNotificationNoWait(id, all));
    }

    private void insertSingleContact(String name, String phone, String email, boolean starred) {
        final ArrayList<ContentProviderOperation> operationList =
                new ArrayList<ContentProviderOperation>();
        ContentProviderOperation.Builder builder =
                ContentProviderOperation.newInsert(ContactsContract.RawContacts.CONTENT_URI);
        builder.withValue(ContactsContract.RawContacts.STARRED, starred ? 1 : 0);
        operationList.add(builder.build());

        builder = ContentProviderOperation.newInsert(Data.CONTENT_URI);
        builder.withValueBackReference(StructuredName.RAW_CONTACT_ID, 0);
        builder.withValue(Data.MIMETYPE, StructuredName.CONTENT_ITEM_TYPE);
        builder.withValue(StructuredName.DISPLAY_NAME, name);
        operationList.add(builder.build());

        if (phone != null) {
            builder = ContentProviderOperation.newInsert(Data.CONTENT_URI);
            builder.withValueBackReference(Phone.RAW_CONTACT_ID, 0);
            builder.withValue(Data.MIMETYPE, Phone.CONTENT_ITEM_TYPE);
            builder.withValue(Phone.TYPE, Phone.TYPE_MOBILE);
            builder.withValue(Phone.NUMBER, phone);
            builder.withValue(Data.IS_PRIMARY, 1);
            operationList.add(builder.build());
        }
        if (email != null) {
            builder = ContentProviderOperation.newInsert(Data.CONTENT_URI);
            builder.withValueBackReference(Email.RAW_CONTACT_ID, 0);
            builder.withValue(Data.MIMETYPE, Email.CONTENT_ITEM_TYPE);
            builder.withValue(Email.TYPE, Email.TYPE_HOME);
            builder.withValue(Email.DATA, email);
            operationList.add(builder.build());
        }

        try {
            mContext.getContentResolver().applyBatch(ContactsContract.AUTHORITY, operationList);
        } catch (RemoteException e) {
            Log.e(TAG, String.format("%s: %s", e.toString(), e.getMessage()));
        } catch (OperationApplicationException e) {
            Log.e(TAG, String.format("%s: %s", e.toString(), e.getMessage()));
        }
    }

    private Uri lookupContact(String phone) {
        Cursor c = null;
        try {
            Uri phoneUri = Uri.withAppendedPath(ContactsContract.PhoneLookup.CONTENT_FILTER_URI,
                    Uri.encode(phone));
            String[] projection = new String[]{ContactsContract.Contacts._ID,
                    ContactsContract.Contacts.LOOKUP_KEY};
            c = mContext.getContentResolver().query(phoneUri, projection, null, null, null);
            if (c != null && c.getCount() > 0) {
                c.moveToFirst();
                int lookupIdx = c.getColumnIndex(ContactsContract.Contacts.LOOKUP_KEY);
                int idIdx = c.getColumnIndex(ContactsContract.Contacts._ID);
                String lookupKey = c.getString(lookupIdx);
                long contactId = c.getLong(idIdx);
                return ContactsContract.Contacts.getLookupUri(contactId, lookupKey);
            }
        } catch (Throwable t) {
            Log.w(TAG, "Problem getting content resolver or performing contacts query.", t);
        } finally {
            if (c != null) {
                c.close();
            }
        }
        return null;
    }

    private StatusBarNotification findPostedNotification(int id, boolean all) {
        // notification is a bit asynchronous so it may take a few ms to appear in
        // getActiveNotifications()
        // we will check for it for up to 1000ms before giving up
        for (long totalWait = 0; totalWait < MAX_WAIT_TIME; totalWait += SHORT_WAIT_TIME) {
            StatusBarNotification n = findNotificationNoWait(id, all);
            if (n != null) {
                return n;
            }
            try {
                Thread.sleep(SHORT_WAIT_TIME);
            } catch (InterruptedException ex) {
                // pass
            }
        }
        return findNotificationNoWait(id, all);
    }

    private StatusBarNotification findNotificationNoWait(int id, boolean all) {
        for (StatusBarNotification sbn : getActiveNotifications(all)) {
            if (sbn.getId() == id) {
                return sbn;
            }
        }
        return null;
    }

    private StatusBarNotification[] getActiveNotifications(boolean all) {
        if (all) {
            return mListener.getActiveNotifications();
        } else {
            return mNotificationManager.getActiveNotifications();
        }
    }

    private PendingIntent getPendingIntent() {
        return PendingIntent.getActivity(
                getContext(), 0, new Intent(getContext(), this.getClass()), 0);
    }

    private boolean isGroupSummary(Notification n) {
        return n.getGroup() != null && (n.flags & Notification.FLAG_GROUP_SUMMARY) != 0;
    }

    private void assertOnlySomeNotificationsAutogrouped(List<Integer> autoGroupedIds) {
        String expectedGroupKey = null;
        try {
            // Posting can take ~100 ms
            Thread.sleep(150);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
        for (StatusBarNotification sbn : sbns) {
            if (isGroupSummary(sbn.getNotification())
                    || autoGroupedIds.contains(sbn.getId())) {
                assertTrue(sbn.getKey() + " is unexpectedly not autogrouped",
                        sbn.getOverrideGroupKey() != null);
                if (expectedGroupKey == null) {
                    expectedGroupKey = sbn.getGroupKey();
                }
                assertEquals(expectedGroupKey, sbn.getGroupKey());
            } else {
                assertTrue(sbn.isGroup());
                assertTrue(sbn.getKey() + " is unexpectedly autogrouped,",
                        sbn.getOverrideGroupKey() == null);
                assertTrue(sbn.getKey() + " has an unusual group key",
                        sbn.getGroupKey() != expectedGroupKey);
            }
        }
    }

    private void assertAllPostedNotificationsAutogrouped() {
        String expectedGroupKey = null;
        try {
            // Posting can take ~100 ms
            Thread.sleep(150);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
        for (StatusBarNotification sbn : sbns) {
            // all notis should be in a group determined by autogrouping
            assertTrue(sbn.getOverrideGroupKey() != null);
            if (expectedGroupKey == null) {
                expectedGroupKey = sbn.getGroupKey();
            }
            // all notis should be in the same group
            assertEquals(expectedGroupKey, sbn.getGroupKey());
        }
    }

    private void cancelAndPoll(int id) {
        mNotificationManager.cancel(id);

        try {
            Thread.sleep(500);
        } catch (InterruptedException ex) {
            // pass
        }
        if (!checkNotificationExistence(id, /*shouldExist=*/ false)) {
            fail("canceled notification was still alive, id=" + id);
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

        if (!checkNotificationExistence(id, /*shouldExist=*/ true)) {
            fail("couldn't find posted notification id=" + id);
        }
    }

    private void setUpNotifListener() {
        try {
            toggleListenerAccess(true);
            mListener = TestNotificationListener.getInstance();
            mListener.resetData();
            assertNotNull(mListener);
        } catch (IOException e) {
        }
    }

    private void sendAndVerifyBubble(final int id, Notification.Builder builder,
            Notification.BubbleMetadata data, boolean shouldBeBubble) {
        setUpNotifListener();

        final Intent intent = new Intent(mContext, BubbledActivity.class);

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intent.setAction(Intent.ACTION_MAIN);
        final PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);

        if (data == null) {
            data = new Notification.BubbleMetadata.Builder(pendingIntent,
                    Icon.createWithResource(mContext, R.drawable.black))
                    .build();
        }
        if (builder == null) {
            builder = new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                    .setSmallIcon(R.drawable.black)
                    .setWhen(System.currentTimeMillis())
                    .setContentTitle("notify#" + id)
                    .setContentText("This is #" + id + "notification  ")
                    .setContentIntent(pendingIntent);
        }
        builder.setBubbleMetadata(data);

        Notification notif = builder.build();
        mNotificationManager.notify(id, notif);

        verifyNotificationBubbleState(id, shouldBeBubble);
    }

    /**
     * Make sure {@link #setUpNotifListener()} is called prior to sending the notif and verifying
     * in this method.
     */
    private void verifyNotificationBubbleState(int id, boolean shouldBeBubble) {
        try {
            // FLAG_BUBBLE relies on notification being posted, wait for notification listener
            Thread.sleep(500);
        } catch (InterruptedException ex) {
        }

        for (StatusBarNotification sbn : mListener.mPosted) {
            if (sbn.getId() == id) {
                boolean isBubble = (sbn.getNotification().flags & FLAG_BUBBLE) != 0;
                if (isBubble != shouldBeBubble) {
                    final String failure = shouldBeBubble
                            ? "Notification with id= " + id + " wasn't a bubble"
                            : "Notification with id= " + id + " was a bubble and shouldn't be";
                    fail(failure);
                } else {
                    // pass
                    return;
                }
            }
        }
        fail("Couldn't find posted notification with id= " + id);
    }

    private boolean checkNotificationExistence(int id, boolean shouldExist) {
        // notification is a bit asynchronous so it may take a few ms to appear in
        // getActiveNotifications()
        // we will check for it for up to 300ms before giving up
        boolean found = false;
        for (int tries = 3; tries-- > 0; ) {
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

    private void assertNotificationCount(int expectedCount) {
        // notification is a bit asynchronous so it may take a few ms to appear in
        // getActiveNotifications()
        // we will check for it for up to 400ms before giving up
        int lastCount = 0;
        for (int tries = 4; tries-- > 0; ) {
            final StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
            lastCount = sbns.length;
            if (expectedCount == lastCount) return;
            try {
                Thread.sleep(100);
            } catch (InterruptedException ex) {
                // pass
            }
        }
        fail("Expected " + expectedCount + " posted notifications, were " + lastCount);
    }

    private void compareChannels(NotificationChannel expected, NotificationChannel actual) {
        if (actual == null) {
            fail("actual channel is null");
            return;
        }
        if (expected == null) {
            fail("expected channel is null");
            return;
        }
        assertEquals(expected.getId(), actual.getId());
        assertEquals(expected.getName(), actual.getName());
        assertEquals(expected.getDescription(), actual.getDescription());
        assertEquals(expected.shouldVibrate(), actual.shouldVibrate());
        assertEquals(expected.shouldShowLights(), actual.shouldShowLights());
        assertEquals(expected.getLightColor(), actual.getLightColor());
        assertEquals(expected.getImportance(), actual.getImportance());
        if (expected.getSound() == null) {
            assertEquals(Settings.System.DEFAULT_NOTIFICATION_URI, actual.getSound());
            assertEquals(Notification.AUDIO_ATTRIBUTES_DEFAULT, actual.getAudioAttributes());
        } else {
            assertEquals(expected.getSound(), actual.getSound());
            assertEquals(expected.getAudioAttributes(), actual.getAudioAttributes());
        }
        assertTrue(Arrays.equals(expected.getVibrationPattern(), actual.getVibrationPattern()));
        assertEquals(expected.getGroup(), actual.getGroup());
        assertEquals(expected.getConversationId(), actual.getConversationId());
        assertEquals(expected.getParentChannelId(), actual.getParentChannelId());
    }

    private void toggleNotificationPolicyAccess(String packageName,
            Instrumentation instrumentation, boolean on) throws IOException {

        String command = " cmd notification " + (on ? "allow_dnd " : "disallow_dnd ") + packageName;

        runCommand(command, instrumentation);

        NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        assertEquals("Notification Policy Access Grant is "
                        + nm.isNotificationPolicyAccessGranted() + " not " + on, on,
                nm.isNotificationPolicyAccessGranted());
    }

    private void suspendPackage(String packageName,
            Instrumentation instrumentation, boolean suspend) throws IOException {
        int userId = mContext.getUserId();
        String command = " cmd package " + (suspend ? "suspend " : "unsuspend ")
                + "--user " + userId + " " + packageName;

        runCommand(command, instrumentation);
    }

    private void toggleListenerAccess(boolean on) throws IOException {
        String command = " cmd notification " + (on ? "allow_listener " : "disallow_listener ")
                + TestNotificationListener.getId();

        runCommand(command, InstrumentationRegistry.getInstrumentation());

        final NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        final ComponentName listenerComponent = TestNotificationListener.getComponentName();
        assertEquals(listenerComponent + " has incorrect listener access",
                on, nm.isNotificationListenerAccessGranted(listenerComponent));
    }

    private void toggleExternalListenerAccess(ComponentName listenerComponent, boolean on)
            throws IOException {
        String command = " cmd notification " + (on ? "allow_listener " : "disallow_listener ")
                + listenerComponent.flattenToString();
        runCommand(command, InstrumentationRegistry.getInstrumentation());
    }

    private void setBubblesGlobal(boolean enabled)
            throws InterruptedException {
        SystemUtil.runWithShellPermissionIdentity(() ->
                Settings.Global.putInt(mContext.getContentResolver(),
                        Settings.Global.NOTIFICATION_BUBBLES, enabled ? 1 : 0));
        Thread.sleep(500); // wait for ranking update
    }

    private void setBubblesAppPref(int pref) throws Exception {
        int userId = mContext.getUser().getIdentifier();
        String pkg = mContext.getPackageName();
        String command = " cmd notification set_bubbles " + pkg
                + " " + Integer.toString(pref)
                + " " + userId;
        runCommand(command, InstrumentationRegistry.getInstrumentation());
        Thread.sleep(500); // wait for ranking update
    }

    private void setBubblesChannelAllowed(boolean allowed) throws Exception {
        int userId = mContext.getUser().getIdentifier();
        String pkg = mContext.getPackageName();
        String command = " cmd notification set_bubbles_channel " + pkg
                + " " + NOTIFICATION_CHANNEL_ID
                + " " + Boolean.toString(allowed)
                + " " + userId;
        runCommand(command, InstrumentationRegistry.getInstrumentation());
        Thread.sleep(500); // wait for ranking update
    }

    @SuppressWarnings("StatementWithEmptyBody")
    private void runCommand(String command, Instrumentation instrumentation) throws IOException {
        UiAutomation uiAutomation = instrumentation.getUiAutomation();
        // Execute command
        try (ParcelFileDescriptor fd = uiAutomation.executeShellCommand(command)) {
            assertNotNull("Failed to execute shell command: " + command, fd);
            // Wait for the command to finish by reading until EOF
            try (InputStream in = new FileInputStream(fd.getFileDescriptor())) {
                byte[] buffer = new byte[4096];
                while (in.read(buffer) > 0) {
                    // discard output
                }
            } catch (IOException e) {
                throw new IOException("Could not read stdout of command: " + command, e);
            }
        } finally {
            uiAutomation.destroy();
        }
    }

    private boolean areRulesSame(AutomaticZenRule a, AutomaticZenRule b) {
        return a.isEnabled() == b.isEnabled()
                && Objects.equals(a.getName(), b.getName())
                && a.getInterruptionFilter() == b.getInterruptionFilter()
                && Objects.equals(a.getConditionId(), b.getConditionId())
                && Objects.equals(a.getOwner(), b.getOwner())
                && Objects.equals(a.getZenPolicy(), b.getZenPolicy())
                && Objects.equals(a.getConfigurationActivity(), b.getConfigurationActivity());
    }

    private AutomaticZenRule createRule(String name, int filter) {
        return new AutomaticZenRule(name, null,
                new ComponentName(mContext, AutomaticZenRuleActivity.class),
                new Uri.Builder().scheme("scheme")
                        .appendPath("path")
                        .appendQueryParameter("fake_rule", "fake_value")
                        .build(), null, filter, true);
    }

    private AutomaticZenRule createRule(String name) {
        return createRule(name, INTERRUPTION_FILTER_PRIORITY);
    }

    private void assertExpectedDndState(int expectedState) {
        int tries = 3;
        for (int i = tries; i >= 0; i--) {
            if (expectedState ==
                    mNotificationManager.getCurrentInterruptionFilter()) {
                break;
            }
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        assertEquals(expectedState, mNotificationManager.getCurrentInterruptionFilter());
    }

    /** Creates a dynamic, longlived, sharing shortcut. Call {@link #deleteShortcuts()} after. */
    private void createDynamicShortcut() {
        Person person = new Person.Builder()
                .setBot(false)
                .setIcon(Icon.createWithResource(mContext, R.drawable.icon_black))
                .setName("BubbleBot")
                .setImportant(true)
                .build();

        Set<String> categorySet = new ArraySet<>();
        categorySet.add(SHARE_SHORTCUT_CATEGORY);
        Intent shortcutIntent = new Intent(mContext, SendBubbleActivity.class);
        shortcutIntent.setAction(Intent.ACTION_VIEW);

        ShortcutInfo shortcut = new ShortcutInfo.Builder(mContext, SHARE_SHORTCUT_ID)
                .setShortLabel(SHARE_SHORTCUT_ID)
                .setIcon(Icon.createWithResource(mContext, R.drawable.icon_black))
                .setIntent(shortcutIntent)
                .setPerson(person)
                .setCategories(categorySet)
                .setLongLived(true)
                .build();

        ShortcutManager scManager =
                (ShortcutManager) mContext.getSystemService(Context.SHORTCUT_SERVICE);
        scManager.addDynamicShortcuts(Arrays.asList(shortcut));
    }

    private void deleteShortcuts() {
        ShortcutManager scManager =
                (ShortcutManager) mContext.getSystemService(Context.SHORTCUT_SERVICE);
        scManager.removeAllDynamicShortcuts();
        scManager.removeLongLivedShortcuts(Collections.singletonList(SHARE_SHORTCUT_ID));
    }

    /**
     * Notification fulfilling conversation policy; for the shortcut to be valid
     * call {@link #createDynamicShortcut()}
     */
    private Notification.Builder getConversationNotification() {
        Person person = new Person.Builder()
                .setName("bubblebot")
                .build();
        Notification.Builder nb = new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                .setContentTitle("foo")
                .setShortcutId(SHARE_SHORTCUT_ID)
                .setStyle(new Notification.MessagingStyle(person)
                        .setConversationTitle("Bubble Chat")
                        .addMessage("Hello?",
                                SystemClock.currentThreadTimeMillis() - 300000, person)
                        .addMessage("Is it me you're looking for?",
                                SystemClock.currentThreadTimeMillis(), person)
                )
                .setSmallIcon(android.R.drawable.sym_def_app_icon);
        return nb;
    }

    /**
     * Starts an activity that is able to send a bubble; also handles unlocking the device.
     * Any tests that use this method should be sure to call {@link #cleanupSendBubbleActivity()}
     * to unregister the related broadcast receiver.
     *
     * @return the SendBubbleActivity that was opened.
     */
    private SendBubbleActivity startSendBubbleActivity() {
        final CountDownLatch latch = new CountDownLatch(2);
        mBubbleBroadcastReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                latch.countDown();
            }
        };
        IntentFilter filter = new IntentFilter(SendBubbleActivity.BUBBLE_ACTIVITY_OPENED);
        mContext.registerReceiver(mBubbleBroadcastReceiver, filter);

        // Start & get the activity
        Class clazz = SendBubbleActivity.class;

        Instrumentation.ActivityResult result =
                new Instrumentation.ActivityResult(0, new Intent());
        Instrumentation.ActivityMonitor monitor =
                new Instrumentation.ActivityMonitor(clazz.getName(), result, false);
        InstrumentationRegistry.getInstrumentation().addMonitor(monitor);

        Intent i = new Intent(mContext, SendBubbleActivity.class);
        i.setFlags(FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(i);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        SendBubbleActivity sendBubbleActivity = (SendBubbleActivity) monitor.waitForActivity();

        // Make sure device is unlocked
        KeyguardManager keyguardManager = mContext.getSystemService(KeyguardManager.class);
        keyguardManager.requestDismissKeyguard(sendBubbleActivity,
                new KeyguardManager.KeyguardDismissCallback() {
                    @Override
                    public void onDismissSucceeded() {
                        latch.countDown();
                    }
                });
        try {
            latch.await(500, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        return sendBubbleActivity;
    }

    private void cleanupSendBubbleActivity() {
        mContext.unregisterReceiver(mBubbleBroadcastReceiver);
    }

    public void testConsolidatedNotificationPolicy() throws Exception {
        final int originalFilter = mNotificationManager.getCurrentInterruptionFilter();
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), true);

            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                    PRIORITY_CATEGORY_ALARMS | PRIORITY_CATEGORY_MEDIA,
                    0, 0));
            // turn on manual DND
            mNotificationManager.setInterruptionFilter(INTERRUPTION_FILTER_PRIORITY);
            assertExpectedDndState(INTERRUPTION_FILTER_PRIORITY);

            // no custom ZenPolicy, so consolidatedPolicy should equal the default notif policy
            assertEquals(mNotificationManager.getConsolidatedNotificationPolicy(),
                    mNotificationManager.getNotificationPolicy());

            // turn off manual DND
            mNotificationManager.setInterruptionFilter(INTERRUPTION_FILTER_ALL);
            assertExpectedDndState(INTERRUPTION_FILTER_ALL);

            // setup custom ZenPolicy for an automatic rule
            AutomaticZenRule rule = createRule("test_consolidated_policy",
                    INTERRUPTION_FILTER_PRIORITY);
            rule.setZenPolicy(new ZenPolicy.Builder()
                    .allowReminders(true)
                    .build());
            String id = mNotificationManager.addAutomaticZenRule(rule);
            mRuleIds.add(id);
            // set condition of the automatic rule to TRUE
            Condition condition = new Condition(rule.getConditionId(), "summary",
                    Condition.STATE_TRUE);
            mNotificationManager.setAutomaticZenRuleState(id, condition);
            assertExpectedDndState(INTERRUPTION_FILTER_PRIORITY);

            NotificationManager.Policy consolidatedPolicy =
                    mNotificationManager.getConsolidatedNotificationPolicy();

            // alarms and media are allowed from default notification policy
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_ALARMS) != 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_MEDIA) != 0);

            // reminders is allowed from the automatic rule's custom ZenPolicy
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_REMINDERS) != 0);

            // other sounds aren't allowed
            assertTrue((consolidatedPolicy.priorityCategories
                    & PRIORITY_CATEGORY_CONVERSATIONS) == 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_CALLS) == 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_MESSAGES) == 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_SYSTEM) == 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_EVENTS) == 0);
        } finally {
            mNotificationManager.setInterruptionFilter(originalFilter);
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testConsolidatedNotificationPolicyMultiRules() throws Exception {
        final int originalFilter = mNotificationManager.getCurrentInterruptionFilter();
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), true);

            // default allows no sounds
            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                    PRIORITY_CATEGORY_ALARMS, 0, 0));

            // setup custom ZenPolicy for two automatic rules
            AutomaticZenRule rule1 = createRule("test_consolidated_policyq",
                    INTERRUPTION_FILTER_PRIORITY);
            rule1.setZenPolicy(new ZenPolicy.Builder()
                    .allowReminders(false)
                    .allowAlarms(false)
                    .allowSystem(true)
                    .build());
            AutomaticZenRule rule2 = createRule("test_consolidated_policy2",
                    INTERRUPTION_FILTER_PRIORITY);
            rule2.setZenPolicy(new ZenPolicy.Builder()
                    .allowReminders(true)
                    .allowMedia(true)
                    .build());
            String id1 = mNotificationManager.addAutomaticZenRule(rule1);
            String id2 = mNotificationManager.addAutomaticZenRule(rule2);
            Condition onCondition1 = new Condition(rule1.getConditionId(), "summary",
                    Condition.STATE_TRUE);
            Condition onCondition2 = new Condition(rule2.getConditionId(), "summary",
                    Condition.STATE_TRUE);
            mNotificationManager.setAutomaticZenRuleState(id1, onCondition1);
            mNotificationManager.setAutomaticZenRuleState(id2, onCondition2);

            Thread.sleep(300); // wait for rules to be applied - it's done asynchronously

            mRuleIds.add(id1);
            mRuleIds.add(id2);
            assertExpectedDndState(INTERRUPTION_FILTER_PRIORITY);

            NotificationManager.Policy consolidatedPolicy =
                    mNotificationManager.getConsolidatedNotificationPolicy();

            // reminders aren't allowed from rule1 overriding rule2
            // (not allowed takes precedence over allowed)
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_REMINDERS) == 0);

            // alarms aren't allowed from rule1
            // (rule's custom zenPolicy overrides default policy)
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_ALARMS) == 0);

            // system is allowed from rule1, media is allowed from rule2
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_SYSTEM) != 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_MEDIA) != 0);

            // other sounds aren't allowed (from default policy)
            assertTrue((consolidatedPolicy.priorityCategories
                    & PRIORITY_CATEGORY_CONVERSATIONS) == 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_CALLS) == 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_MESSAGES) == 0);
            assertTrue((consolidatedPolicy.priorityCategories & PRIORITY_CATEGORY_EVENTS) == 0);
        } finally {
            mNotificationManager.setInterruptionFilter(originalFilter);
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testPostPCanToggleAlarmsMediaSystemTest() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        NotificationManager.Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {
                // Post-P can toggle alarms, media, system
                // toggle on alarms, media, system:
                mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                        PRIORITY_CATEGORY_ALARMS
                                | PRIORITY_CATEGORY_MEDIA
                                | PRIORITY_CATEGORY_SYSTEM, 0, 0));
                NotificationManager.Policy policy = mNotificationManager.getNotificationPolicy();
                assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_ALARMS) != 0);
                assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_MEDIA) != 0);
                assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_SYSTEM) != 0);

                // toggle off alarms, media, system
                mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(0, 0, 0));
                policy = mNotificationManager.getNotificationPolicy();
                assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_ALARMS) == 0);
                assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_MEDIA) == 0);
                assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_SYSTEM) == 0);
            }
        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testPostRCanToggleConversationsTest() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        NotificationManager.Policy origPolicy = mNotificationManager.getNotificationPolicy();

        try {
            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                    0, 0, 0, 0));
            NotificationManager.Policy policy = mNotificationManager.getNotificationPolicy();
            assertEquals(0, (policy.priorityCategories & PRIORITY_CATEGORY_CONVERSATIONS));
            assertEquals(CONVERSATION_SENDERS_NONE, policy.priorityConversationSenders);

            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                    PRIORITY_CATEGORY_CONVERSATIONS, 0, 0, 0, CONVERSATION_SENDERS_ANYONE));
            policy = mNotificationManager.getNotificationPolicy();
            assertTrue((policy.priorityCategories & PRIORITY_CATEGORY_CONVERSATIONS) != 0);
            assertEquals(CONVERSATION_SENDERS_ANYONE, policy.priorityConversationSenders);

        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testCreateChannelGroup() throws Exception {
        final NotificationChannelGroup ncg = new NotificationChannelGroup("a group", "a label");
        final NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup(ncg.getId());
        mNotificationManager.createNotificationChannelGroup(ncg);
        final NotificationChannel ungrouped =
                new NotificationChannel(mId + "!", "name", IMPORTANCE_DEFAULT);
        try {
            mNotificationManager.createNotificationChannel(channel);
            mNotificationManager.createNotificationChannel(ungrouped);

            List<NotificationChannelGroup> ncgs =
                    mNotificationManager.getNotificationChannelGroups();
            assertEquals(1, ncgs.size());
            assertEquals(ncg.getName(), ncgs.get(0).getName());
            assertEquals(ncg.getDescription(), ncgs.get(0).getDescription());
            assertEquals(channel.getId(), ncgs.get(0).getChannels().get(0).getId());
        } finally {
            mNotificationManager.deleteNotificationChannelGroup(ncg.getId());
        }
    }

    public void testGetChannelGroup() throws Exception {
        final NotificationChannelGroup ncg = new NotificationChannelGroup("a group", "a label");
        ncg.setDescription("bananas");
        final NotificationChannelGroup ncg2 = new NotificationChannelGroup("group 2", "label 2");
        final NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup(ncg.getId());

        mNotificationManager.createNotificationChannelGroup(ncg);
        mNotificationManager.createNotificationChannelGroup(ncg2);
        mNotificationManager.createNotificationChannel(channel);

        NotificationChannelGroup actual =
                mNotificationManager.getNotificationChannelGroup(ncg.getId());
        assertEquals(ncg.getId(), actual.getId());
        assertEquals(ncg.getName(), actual.getName());
        assertEquals(ncg.getDescription(), actual.getDescription());
        assertEquals(channel.getId(), actual.getChannels().get(0).getId());
    }

    public void testGetChannelGroups() throws Exception {
        final NotificationChannelGroup ncg = new NotificationChannelGroup("a group", "a label");
        ncg.setDescription("bananas");
        final NotificationChannelGroup ncg2 = new NotificationChannelGroup("group 2", "label 2");
        final NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup(ncg2.getId());

        mNotificationManager.createNotificationChannelGroup(ncg);
        mNotificationManager.createNotificationChannelGroup(ncg2);
        mNotificationManager.createNotificationChannel(channel);

        List<NotificationChannelGroup> actual =
                mNotificationManager.getNotificationChannelGroups();
        assertEquals(2, actual.size());
        for (NotificationChannelGroup group : actual) {
            if (group.getId().equals(ncg.getId())) {
                assertEquals(group.getName(), ncg.getName());
                assertEquals(group.getDescription(), ncg.getDescription());
                assertEquals(0, group.getChannels().size());
            } else if (group.getId().equals(ncg2.getId())) {
                assertEquals(group.getName(), ncg2.getName());
                assertEquals(group.getDescription(), ncg2.getDescription());
                assertEquals(1, group.getChannels().size());
                assertEquals(channel.getId(), group.getChannels().get(0).getId());
            } else {
                fail("Extra group found " + group.getId());
            }
        }
    }

    public void testDeleteChannelGroup() throws Exception {
        final NotificationChannelGroup ncg = new NotificationChannelGroup("a group", "a label");
        final NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup(ncg.getId());
        mNotificationManager.createNotificationChannelGroup(ncg);
        mNotificationManager.createNotificationChannel(channel);

        mNotificationManager.deleteNotificationChannelGroup(ncg.getId());

        assertNull(mNotificationManager.getNotificationChannel(channel.getId()));
        assertEquals(0, mNotificationManager.getNotificationChannelGroups().size());
    }

    public void testCreateChannel() throws Exception {
        final NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setDescription("bananas");
        channel.enableVibration(true);
        channel.setVibrationPattern(new long[]{5, 8, 2, 1});
        channel.setSound(new Uri.Builder().scheme("test").build(),
                new AudioAttributes.Builder().setUsage(
                        AudioAttributes.USAGE_NOTIFICATION_COMMUNICATION_DELAYED).build());
        channel.enableLights(true);
        channel.setBypassDnd(true);
        channel.setLockscreenVisibility(Notification.VISIBILITY_SECRET);
        mNotificationManager.createNotificationChannel(channel);
        final NotificationChannel createdChannel =
                mNotificationManager.getNotificationChannel(mId);
        compareChannels(channel, createdChannel);
        // Lockscreen Visibility and canBypassDnd no longer settable.
        assertTrue(createdChannel.getLockscreenVisibility() != Notification.VISIBILITY_SECRET);
        assertFalse(createdChannel.canBypassDnd());
    }

    public void testCreateChannel_rename() throws Exception {
        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        mNotificationManager.createNotificationChannel(channel);
        channel.setName("new name");
        mNotificationManager.createNotificationChannel(channel);
        final NotificationChannel createdChannel =
                mNotificationManager.getNotificationChannel(mId);
        compareChannels(channel, createdChannel);

        channel.setImportance(NotificationManager.IMPORTANCE_HIGH);
        mNotificationManager.createNotificationChannel(channel);
        assertEquals(NotificationManager.IMPORTANCE_DEFAULT,
                mNotificationManager.getNotificationChannel(mId).getImportance());
    }

    public void testCreateChannel_addToGroup() throws Exception {
        String oldGroup = null;
        String newGroup = "new group";
        mNotificationManager.createNotificationChannelGroup(
                new NotificationChannelGroup(newGroup, newGroup));

        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup(oldGroup);
        mNotificationManager.createNotificationChannel(channel);

        channel.setGroup(newGroup);
        mNotificationManager.createNotificationChannel(channel);

        final NotificationChannel updatedChannel =
                mNotificationManager.getNotificationChannel(mId);
        assertEquals("Failed to add non-grouped channel to a group on update ",
                newGroup, updatedChannel.getGroup());
    }

    public void testCreateChannel_cannotChangeGroup() throws Exception {
        String oldGroup = "old group";
        String newGroup = "new group";
        mNotificationManager.createNotificationChannelGroup(
                new NotificationChannelGroup(oldGroup, oldGroup));
        mNotificationManager.createNotificationChannelGroup(
                new NotificationChannelGroup(newGroup, newGroup));

        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup(oldGroup);
        mNotificationManager.createNotificationChannel(channel);
        channel.setGroup(newGroup);
        mNotificationManager.createNotificationChannel(channel);
        final NotificationChannel updatedChannel =
                mNotificationManager.getNotificationChannel(mId);
        assertEquals("Channels should not be allowed to change groups",
                oldGroup, updatedChannel.getGroup());
    }

    public void testCreateSameChannelDoesNotUpdate() throws Exception {
        final NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        mNotificationManager.createNotificationChannel(channel);
        final NotificationChannel channelDupe =
                new NotificationChannel(mId, "name", IMPORTANCE_HIGH);
        mNotificationManager.createNotificationChannel(channelDupe);
        final NotificationChannel createdChannel =
                mNotificationManager.getNotificationChannel(mId);
        compareChannels(channel, createdChannel);
    }

    public void testCreateChannelAlreadyExistsNoOp() throws Exception {
        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        mNotificationManager.createNotificationChannel(channel);
        NotificationChannel channelDupe =
                new NotificationChannel(mId, "name", IMPORTANCE_HIGH);
        mNotificationManager.createNotificationChannel(channelDupe);
        compareChannels(channel, mNotificationManager.getNotificationChannel(channel.getId()));
    }

    public void testCreateChannelWithGroup() throws Exception {
        NotificationChannelGroup ncg = new NotificationChannelGroup("g", "n");
        mNotificationManager.createNotificationChannelGroup(ncg);
        try {
            NotificationChannel channel =
                    new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
            channel.setGroup(ncg.getId());
            mNotificationManager.createNotificationChannel(channel);
            compareChannels(channel, mNotificationManager.getNotificationChannel(channel.getId()));
        } finally {
            mNotificationManager.deleteNotificationChannelGroup(ncg.getId());
        }
    }

    public void testCreateChannelWithBadGroup() throws Exception {
        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup("garbage");
        try {
            mNotificationManager.createNotificationChannel(channel);
            fail("Created notification with bad group");
        } catch (IllegalArgumentException e) {
        }
    }

    public void testCreateChannelInvalidImportance() throws Exception {
        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_UNSPECIFIED);
        try {
            mNotificationManager.createNotificationChannel(channel);
        } catch (IllegalArgumentException e) {
            //success
        }
    }

    public void testDeleteChannel() throws Exception {
        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_LOW);
        mNotificationManager.createNotificationChannel(channel);
        compareChannels(channel, mNotificationManager.getNotificationChannel(channel.getId()));
        mNotificationManager.deleteNotificationChannel(channel.getId());
        assertNull(mNotificationManager.getNotificationChannel(channel.getId()));
    }

    public void testCannotDeleteDefaultChannel() throws Exception {
        try {
            mNotificationManager.deleteNotificationChannel(NotificationChannel.DEFAULT_CHANNEL_ID);
            fail("Deleted default channel");
        } catch (IllegalArgumentException e) {
            //success
        }
    }

    public void testGetChannel() throws Exception {
        NotificationChannel channel1 =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        NotificationChannel channel2 =
                new NotificationChannel(
                        UUID.randomUUID().toString(), "name2", IMPORTANCE_HIGH);
        NotificationChannel channel3 =
                new NotificationChannel(
                        UUID.randomUUID().toString(), "name3", IMPORTANCE_LOW);
        NotificationChannel channel4 =
                new NotificationChannel(
                        UUID.randomUUID().toString(), "name4", IMPORTANCE_MIN);
        mNotificationManager.createNotificationChannel(channel1);
        mNotificationManager.createNotificationChannel(channel2);
        mNotificationManager.createNotificationChannel(channel3);
        mNotificationManager.createNotificationChannel(channel4);

        compareChannels(channel2,
                mNotificationManager.getNotificationChannel(channel2.getId()));
        compareChannels(channel3,
                mNotificationManager.getNotificationChannel(channel3.getId()));
        compareChannels(channel1,
                mNotificationManager.getNotificationChannel(channel1.getId()));
        compareChannels(channel4,
                mNotificationManager.getNotificationChannel(channel4.getId()));
    }

    public void testGetChannels() throws Exception {
        NotificationChannel channel1 =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        NotificationChannel channel2 =
                new NotificationChannel(
                        UUID.randomUUID().toString(), "name2", IMPORTANCE_HIGH);
        NotificationChannel channel3 =
                new NotificationChannel(
                        UUID.randomUUID().toString(), "name3", IMPORTANCE_LOW);
        NotificationChannel channel4 =
                new NotificationChannel(
                        UUID.randomUUID().toString(), "name4", IMPORTANCE_MIN);

        Map<String, NotificationChannel> channelMap = new HashMap<>();
        channelMap.put(channel1.getId(), channel1);
        channelMap.put(channel2.getId(), channel2);
        channelMap.put(channel3.getId(), channel3);
        channelMap.put(channel4.getId(), channel4);
        mNotificationManager.createNotificationChannel(channel1);
        mNotificationManager.createNotificationChannel(channel2);
        mNotificationManager.createNotificationChannel(channel3);
        mNotificationManager.createNotificationChannel(channel4);

        mNotificationManager.deleteNotificationChannel(channel3.getId());

        List<NotificationChannel> channels = mNotificationManager.getNotificationChannels();
        for (NotificationChannel nc : channels) {
            if (NotificationChannel.DEFAULT_CHANNEL_ID.equals(nc.getId())) {
                continue;
            }
            if (NOTIFICATION_CHANNEL_ID.equals(nc.getId())) {
                continue;
            }
            assertFalse(channel3.getId().equals(nc.getId()));
            if (!channelMap.containsKey(nc.getId())) {
                // failed cleanup from prior test run; ignore
                continue;
            }
            compareChannels(channelMap.get(nc.getId()), nc);
        }
    }

    public void testRecreateDeletedChannel() throws Exception {
        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setShowBadge(true);
        NotificationChannel newChannel = new NotificationChannel(
                channel.getId(), channel.getName(), IMPORTANCE_HIGH);
        mNotificationManager.createNotificationChannel(channel);
        mNotificationManager.deleteNotificationChannel(channel.getId());

        mNotificationManager.createNotificationChannel(newChannel);

        compareChannels(channel,
                mNotificationManager.getNotificationChannel(newChannel.getId()));
    }

    public void testNotify() throws Exception {
        mNotificationManager.cancelAll();

        final int id = 1;
        sendNotification(id, R.drawable.black);
        // test updating the same notification
        sendNotification(id, R.drawable.blue);
        sendNotification(id, R.drawable.yellow);

        // assume that sendNotification tested to make sure individual notifications were present
        StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
        for (StatusBarNotification sbn : sbns) {
            if (sbn.getId() != id) {
                fail("we got back other notifications besides the one we posted: "
                        + sbn.getKey());
            }
        }
    }

    public void testSuspendPackage_withoutShellPermission() throws Exception {
        if (mActivityManager.isLowRamDevice() && !mPackageManager.hasSystemFeature(FEATURE_WATCH)) {
            return;
        }

        try {
            Process proc = Runtime.getRuntime().exec("cmd notification suspend_package "
                    + mContext.getPackageName());

            // read output of command
            BufferedReader reader =
                    new BufferedReader(new InputStreamReader(proc.getInputStream()));
            StringBuilder output = new StringBuilder();
            String line = reader.readLine();
            while (line != null) {
                output.append(line);
                line = reader.readLine();
            }
            reader.close();
            final String outputString = output.toString();

            proc.waitFor();

            // check that the output string had an error / disallowed call since it didn't have
            // shell permission to suspend the package
            assertTrue(outputString, outputString.contains("error"));
            assertTrue(outputString, outputString.contains("permission denied"));
        } catch (InterruptedException e) {
            fail("Unsuccessful shell command");
        }
    }

    public void testSuspendPackage() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        sendNotification(1, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification
        assertEquals(1, mListener.mPosted.size());

        // suspend package, ranking should be updated with suspended = true
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                true);
        Thread.sleep(500); // wait for notification listener to get response
        NotificationListenerService.RankingMap rankingMap = mListener.mRankingMap;
        NotificationListenerService.Ranking outRanking = new NotificationListenerService.Ranking();
        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);
                Log.d(TAG, "key=" + key + " suspended=" + outRanking.isSuspended());
                assertTrue(outRanking.isSuspended());
            }
        }

        // unsuspend package, ranking should be updated with suspended = false
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                false);
        Thread.sleep(500); // wait for notification listener to get response
        rankingMap = mListener.mRankingMap;
        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);
                Log.d(TAG, "key=" + key + " suspended=" + outRanking.isSuspended());
                assertFalse(outRanking.isSuspended());
            }
        }

        mListener.resetData();
    }

    public void testSuspendedPackageSendsNotification() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        // suspend package, post notification while package is suspended, see notification
        // in ranking map with suspended = true
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                true);
        sendNotification(1, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification
        assertEquals(1, mListener.mPosted.size()); // apps targeting P receive notification
        NotificationListenerService.RankingMap rankingMap = mListener.mRankingMap;
        NotificationListenerService.Ranking outRanking = new NotificationListenerService.Ranking();
        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);
                Log.d(TAG, "key=" + key + " suspended=" + outRanking.isSuspended());
                assertTrue(outRanking.isSuspended());
            }
        }

        // unsuspend package, ranking should be updated with suspended = false
        suspendPackage(mContext.getPackageName(), InstrumentationRegistry.getInstrumentation(),
                false);
        Thread.sleep(500); // wait for notification listener to get response
        assertEquals(1, mListener.mPosted.size()); // should see previously posted notification
        rankingMap = mListener.mRankingMap;
        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);
                Log.d(TAG, "key=" + key + " suspended=" + outRanking.isSuspended());
                assertFalse(outRanking.isSuspended());
            }
        }

        mListener.resetData();
    }

    public void testCanBubble_ranking() throws Exception {
        if ((mActivityManager.isLowRamDevice() && !FeatureUtil.isWatch())
                || FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            return;
        }

        // turn on bubbles globally
        setBubblesGlobal(true);

        assertEquals(1, Settings.Global.getInt(
                mContext.getContentResolver(), Settings.Global.NOTIFICATION_BUBBLES));

        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        sendNotification(1, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification
        NotificationListenerService.RankingMap rankingMap = mListener.mRankingMap;
        NotificationListenerService.Ranking outRanking =
                new NotificationListenerService.Ranking();
        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);
                // by default nothing can bubble
                assertFalse(outRanking.canBubble());
            }
        }

        // turn off bubbles globally
        setBubblesGlobal(false);

        rankingMap = mListener.mRankingMap;
        outRanking = new NotificationListenerService.Ranking();
        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);
                assertFalse(outRanking.canBubble());
            }
        }

        mListener.resetData();
    }

    public void testShowBadging_ranking() throws Exception {
        final int originalBadging = Settings.Secure.getInt(
                mContext.getContentResolver(), Settings.Secure.NOTIFICATION_BADGING);

        SystemUtil.runWithShellPermissionIdentity(() ->
                Settings.Secure.putInt(mContext.getContentResolver(),
                        Settings.Secure.NOTIFICATION_BADGING, 1));
        assertEquals(1, Settings.Secure.getInt(
                mContext.getContentResolver(), Settings.Secure.NOTIFICATION_BADGING));

        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);
        try {
            sendNotification(1, R.drawable.black);
            Thread.sleep(500); // wait for notification listener to receive notification
            NotificationListenerService.RankingMap rankingMap = mListener.mRankingMap;
            NotificationListenerService.Ranking outRanking =
                    new NotificationListenerService.Ranking();
            for (String key : rankingMap.getOrderedKeys()) {
                if (key.contains(mListener.getPackageName())) {
                    rankingMap.getRanking(key, outRanking);
                    assertTrue(outRanking.canShowBadge());
                }
            }

            // turn off badging globally
            SystemUtil.runWithShellPermissionIdentity(() ->
                    Settings.Secure.putInt(mContext.getContentResolver(),
                            Settings.Secure.NOTIFICATION_BADGING, 0));

            Thread.sleep(500); // wait for ranking update

            rankingMap = mListener.mRankingMap;
            outRanking = new NotificationListenerService.Ranking();
            for (String key : rankingMap.getOrderedKeys()) {
                if (key.contains(mListener.getPackageName())) {
                    assertFalse(outRanking.canShowBadge());
                }
            }

            mListener.resetData();
        } finally {
            SystemUtil.runWithShellPermissionIdentity(() ->
                    Settings.Secure.putInt(mContext.getContentResolver(),
                            Settings.Secure.NOTIFICATION_BADGING, originalBadging));
        }
    }

    public void testGetSuppressedVisualEffectsOff_ranking() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        final int notificationId = 1;
        sendNotification(notificationId, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification

        NotificationListenerService.RankingMap rankingMap = mListener.mRankingMap;
        NotificationListenerService.Ranking outRanking =
                new NotificationListenerService.Ranking();

        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);

                // check notification key match
                assertEquals(0, outRanking.getSuppressedVisualEffects());
            }
        }
    }

    public void testGetSuppressedVisualEffects_ranking() throws Exception {
        final int originalFilter = mNotificationManager.getCurrentInterruptionFilter();
        NotificationManager.Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            toggleListenerAccess(true);
            Thread.sleep(500); // wait for listener to be allowed

            mListener = TestNotificationListener.getInstance();
            assertNotNull(mListener);

            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), true);
            if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {
                mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(0, 0, 0,
                        SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_PEEK));
            } else {
                mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(0, 0, 0,
                        SUPPRESSED_EFFECT_SCREEN_ON));
            }
            mNotificationManager.setInterruptionFilter(INTERRUPTION_FILTER_PRIORITY);

            final int notificationId = 1;
            // update notification
            sendNotification(notificationId, R.drawable.black);
            Thread.sleep(500); // wait for notification listener to receive notification

            NotificationListenerService.RankingMap rankingMap = mListener.mRankingMap;
            NotificationListenerService.Ranking outRanking =
                    new NotificationListenerService.Ranking();

            for (String key : rankingMap.getOrderedKeys()) {
                if (key.contains(mListener.getPackageName())) {
                    rankingMap.getRanking(key, outRanking);

                    if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {
                        assertEquals(SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_PEEK,
                                outRanking.getSuppressedVisualEffects());
                    } else {
                        assertEquals(SUPPRESSED_EFFECT_SCREEN_ON,
                                outRanking.getSuppressedVisualEffects());
                    }
                }
            }
        } finally {
            // reset notification policy
            mNotificationManager.setInterruptionFilter(originalFilter);
            mNotificationManager.setNotificationPolicy(origPolicy);
        }

    }

    public void testKeyChannelGroupOverrideImportanceExplanation_ranking() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        final int notificationId = 1;
        sendNotification(notificationId, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification

        NotificationListenerService.RankingMap rankingMap = mListener.mRankingMap;
        NotificationListenerService.Ranking outRanking =
                new NotificationListenerService.Ranking();

        StatusBarNotification sbn = findPostedNotification(notificationId, false);

        // check that the key and channel ids are the same in the ranking as the posted notification
        for (String key : rankingMap.getOrderedKeys()) {
            if (key.contains(mListener.getPackageName())) {
                rankingMap.getRanking(key, outRanking);

                // check notification key match
                assertEquals(sbn.getKey(), outRanking.getKey());

                // check notification channel ids match
                assertEquals(sbn.getNotification().getChannelId(), outRanking.getChannel().getId());

                // check override group key match
                assertEquals(sbn.getOverrideGroupKey(), outRanking.getOverrideGroupKey());

                // check importance explanation isn't null
                assertNotNull(outRanking.getImportanceExplanation());
            }
        }
    }

    public void testNotify_blockedChannel() throws Exception {
        mNotificationManager.cancelAll();

        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_NONE);
        mNotificationManager.createNotificationChannel(channel);

        int id = 1;
        final Notification notification =
                new Notification.Builder(mContext, mId)
                        .setSmallIcon(R.drawable.black)
                        .setWhen(System.currentTimeMillis())
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .build();
        mNotificationManager.notify(id, notification);

        if (!checkNotificationExistence(id, /*shouldExist=*/ false)) {
            fail("found unexpected notification id=" + id);
        }
    }

    public void testNotify_blockedChannelGroup() throws Exception {
        mNotificationManager.cancelAll();

        NotificationChannelGroup group = new NotificationChannelGroup(mId, "group name");
        group.setBlocked(true);
        mNotificationManager.createNotificationChannelGroup(group);
        NotificationChannel channel =
                new NotificationChannel(mId, "name", IMPORTANCE_DEFAULT);
        channel.setGroup(mId);
        mNotificationManager.createNotificationChannel(channel);

        int id = 1;
        final Notification notification =
                new Notification.Builder(mContext, mId)
                        .setSmallIcon(R.drawable.black)
                        .setWhen(System.currentTimeMillis())
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .build();
        mNotificationManager.notify(id, notification);

        if (!checkNotificationExistence(id, /*shouldExist=*/ false)) {
            fail("found unexpected notification id=" + id);
        }
    }

    public void testCancel() throws Exception {
        final int id = 9;
        sendNotification(id, R.drawable.black);
        // Wait for the notification posted not just enqueued
        try {
            Thread.sleep(500);
        } catch(InterruptedException e) {}
        mNotificationManager.cancel(id);

        if (!checkNotificationExistence(id, /*shouldExist=*/ false)) {
            fail("canceled notification was still alive, id=" + id);
        }
    }

    public void testCancelAll() throws Exception {
        sendNotification(1, R.drawable.black);
        sendNotification(2, R.drawable.blue);
        sendNotification(3, R.drawable.yellow);

        if (DEBUG) {
            Log.d(TAG, "posted 3 notifications, here they are: ");
            StatusBarNotification[] sbns = mNotificationManager.getActiveNotifications();
            for (StatusBarNotification sbn : sbns) {
                Log.d(TAG, "  " + sbn);
            }
            Log.d(TAG, "about to cancel...");
        }
        mNotificationManager.cancelAll();

        for (int id = 1; id <= 3; id++) {
            if (!checkNotificationExistence(id, /*shouldExist=*/ false)) {
                fail("Failed to cancel notification id=" + id);
            }
        }

    }

    public void testNotifyWithTimeout() throws Exception {
        mNotificationManager.cancelAll();
        final int id = 128;
        final long timeout = 1000;

        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(R.drawable.black)
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .setTimeoutAfter(timeout)
                        .build();
        mNotificationManager.notify(id, notification);

        if (!checkNotificationExistence(id, /*shouldExist=*/ true)) {
            fail("couldn't find posted notification id=" + id);
        }

        try {
            Thread.sleep(timeout);
        } catch (InterruptedException ex) {
            // pass
        }
        checkNotificationExistence(id, false);
    }

    public void testStyle() throws Exception {
        Notification.Style style = new Notification.Style() {
            public boolean areNotificationsVisiblyDifferent(Notification.Style other) {
                return false;
            }
        };

        Notification.Builder builder = new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID);
        style.setBuilder(builder);

        Notification notification = null;
        try {
            notification = style.build();
        } catch (IllegalArgumentException e) {
            fail(e.getMessage());
        }

        assertNotNull(notification);

        Notification builderNotification = builder.build();
        assertEquals(builderNotification, notification);
    }

    public void testStyle_getStandardView() throws Exception {
        Notification.Builder builder = new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID);
        int layoutId = 0;

        TestStyle overrideStyle = new TestStyle();
        overrideStyle.setBuilder(builder);
        RemoteViews result = overrideStyle.testGetStandardView(layoutId);

        assertNotNull(result);
        assertEquals(layoutId, result.getLayoutId());
    }

    private class TestStyle extends Notification.Style {
        public boolean areNotificationsVisiblyDifferent(Notification.Style other) {
            return false;
        }

        public RemoteViews testGetStandardView(int layoutId) {
            // Wrapper method, since getStandardView is protected and otherwise unused in Android
            return getStandardView(layoutId);
        }
    }

    public void testMediaStyle_empty() {
        Notification.MediaStyle style = new Notification.MediaStyle();
        assertNotNull(style);
    }

    public void testMediaStyle() {
        mNotificationManager.cancelAll();
        final int id = 99;
        MediaSession session = new MediaSession(getContext(), "media");

        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(R.drawable.black)
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_black),
                                "play", getPendingIntent()).build())
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_blue),
                                "pause", getPendingIntent()).build())
                        .setStyle(new Notification.MediaStyle()
                                .setShowActionsInCompactView(0, 1)
                                .setMediaSession(session.getSessionToken()))
                        .build();
        mNotificationManager.notify(id, notification);

        if (!checkNotificationExistence(id, /*shouldExist=*/ true)) {
            fail("couldn't find posted notification id=" + id);
        }
    }

    public void testInboxStyle() {
        final int id = 100;

        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(R.drawable.black)
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_black),
                                "a1", getPendingIntent()).build())
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_blue),
                                "a2", getPendingIntent()).build())
                        .setStyle(new Notification.InboxStyle().addLine("line")
                                .setSummaryText("summary"))
                        .build();
        mNotificationManager.notify(id, notification);

        if (!checkNotificationExistence(id, /*shouldExist=*/ true)) {
            fail("couldn't find posted notification id=" + id);
        }
    }

    public void testBigTextStyle() {
        final int id = 101;

        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(R.drawable.black)
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_black),
                                "a1", getPendingIntent()).build())
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_blue),
                                "a2", getPendingIntent()).build())
                        .setStyle(new Notification.BigTextStyle()
                                .setBigContentTitle("big title")
                                .bigText("big text")
                                .setSummaryText("summary"))
                        .build();
        mNotificationManager.notify(id, notification);

        if (!checkNotificationExistence(id, /*shouldExist=*/ true)) {
            fail("couldn't find posted notification id=" + id);
        }
    }

    public void testBigPictureStyle() {
        final int id = 102;

        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(R.drawable.black)
                        .setContentTitle("notify#" + id)
                        .setContentText("This is #" + id + "notification  ")
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_black),
                                "a1", getPendingIntent()).build())
                        .addAction(new Notification.Action.Builder(
                                Icon.createWithResource(getContext(), R.drawable.icon_blue),
                                "a2", getPendingIntent()).build())
                        .setStyle(new Notification.BigPictureStyle()
                                .setBigContentTitle("title")
                                .bigPicture(Bitmap.createBitmap(100, 100, Bitmap.Config.RGB_565))
                                .bigLargeIcon(
                                        Icon.createWithResource(getContext(), R.drawable.icon_blue))
                                .setSummaryText("summary"))
                        .build();
        mNotificationManager.notify(id, notification);

        if (!checkNotificationExistence(id, /*shouldExist=*/ true)) {
            fail("couldn't find posted notification id=" + id);
        }
    }

    public void testAutogrouping() throws Exception {
        sendNotification(801, R.drawable.black);
        sendNotification(802, R.drawable.blue);
        sendNotification(803, R.drawable.yellow);
        sendNotification(804, R.drawable.yellow);

        assertNotificationCount(5);
        assertAllPostedNotificationsAutogrouped();
    }

    public void testAutogrouping_autogroupStaysUntilAllNotificationsCanceled() throws Exception {
        sendNotification(701, R.drawable.black);
        sendNotification(702, R.drawable.blue);
        sendNotification(703, R.drawable.yellow);
        sendNotification(704, R.drawable.yellow);

        assertNotificationCount(5);
        assertAllPostedNotificationsAutogrouped();

        // Assert all notis stay in the same autogroup until all children are canceled
        for (int i = 704; i > 701; i--) {
            cancelAndPoll(i);
            assertNotificationCount(i - 700);
            assertAllPostedNotificationsAutogrouped();
        }
        cancelAndPoll(701);
        assertNotificationCount(0);
    }

    public void testAutogrouping_autogroupStaysUntilAllNotificationsAddedToGroup()
            throws Exception {
        String newGroup = "new!";
        sendNotification(901, R.drawable.black);
        sendNotification(902, R.drawable.blue);
        sendNotification(903, R.drawable.yellow);
        sendNotification(904, R.drawable.yellow);

        List<Integer> postedIds = new ArrayList<>();
        postedIds.add(901);
        postedIds.add(902);
        postedIds.add(903);
        postedIds.add(904);

        assertNotificationCount(5);
        assertAllPostedNotificationsAutogrouped();

        // Assert all notis stay in the same autogroup until all children are canceled
        for (int i = 904; i > 901; i--) {
            sendNotification(i, newGroup, R.drawable.blue);
            postedIds.remove(postedIds.size() - 1);
            assertNotificationCount(5);
            assertOnlySomeNotificationsAutogrouped(postedIds);
        }
        sendNotification(901, newGroup, R.drawable.blue);
        assertNotificationCount(4); // no more autogroup summary
        postedIds.remove(0);
        assertOnlySomeNotificationsAutogrouped(postedIds);
    }

    public void testNewNotificationsAddedToAutogroup_ifOriginalNotificationsCanceled()
            throws Exception {
        String newGroup = "new!";
        sendNotification(910, R.drawable.black);
        sendNotification(920, R.drawable.blue);
        sendNotification(930, R.drawable.yellow);
        sendNotification(940, R.drawable.yellow);

        List<Integer> postedIds = new ArrayList<>();
        postedIds.add(910);
        postedIds.add(920);
        postedIds.add(930);
        postedIds.add(940);

        assertNotificationCount(5);
        assertAllPostedNotificationsAutogrouped();

        // regroup all but one of the children
        for (int i = postedIds.size() - 1; i > 0; i--) {
            try {
                Thread.sleep(200);
            } catch (InterruptedException ex) {
                // pass
            }
            int id = postedIds.remove(i);
            sendNotification(id, newGroup, R.drawable.blue);
            assertNotificationCount(5);
            assertOnlySomeNotificationsAutogrouped(postedIds);
        }

        // send a new non-grouped notification. since the autogroup summary still exists,
        // the notification should be added to it
        sendNotification(950, R.drawable.blue);
        postedIds.add(950);
        try {
            Thread.sleep(200);
        } catch (InterruptedException ex) {
            // pass
        }
        assertOnlySomeNotificationsAutogrouped(postedIds);
    }

    public void testTotalSilenceOnlyMuteStreams() throws Exception {
        final int originalFilter = mNotificationManager.getCurrentInterruptionFilter();
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), true);

            // ensure volume is not muted/0 to start test
            mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, 1, 0);
            mAudioManager.setStreamVolume(AudioManager.STREAM_ALARM, 1, 0);
            mAudioManager.setStreamVolume(AudioManager.STREAM_SYSTEM, 1, 0);
            mAudioManager.setStreamVolume(AudioManager.STREAM_RING, 1, 0);

            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                    PRIORITY_CATEGORY_ALARMS | PRIORITY_CATEGORY_MEDIA, 0, 0));
            AutomaticZenRule rule = createRule("test_total_silence", INTERRUPTION_FILTER_NONE);
            String id = mNotificationManager.addAutomaticZenRule(rule);
            mRuleIds.add(id);
            Condition condition =
                    new Condition(rule.getConditionId(), "summary", Condition.STATE_TRUE);
            mNotificationManager.setAutomaticZenRuleState(id, condition);
            mNotificationManager.setInterruptionFilter(INTERRUPTION_FILTER_PRIORITY);

            // delay for streams to get into correct mute states
            Thread.sleep(50);
            assertTrue("Music (media) stream should be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_MUSIC));
            assertTrue("System stream should be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_SYSTEM));
            assertTrue("Alarm stream should be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_ALARM));

            // Test requires that the phone's default state has no channels that can bypass dnd
            assertTrue("Ringer stream should be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_RING));
        } finally {
            mNotificationManager.setInterruptionFilter(originalFilter);
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testAlarmsOnlyMuteStreams() throws Exception {
        final int originalFilter = mNotificationManager.getCurrentInterruptionFilter();
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), true);

            // ensure volume is not muted/0 to start test
            mAudioManager.setStreamVolume(AudioManager.STREAM_MUSIC, 1, 0);
            mAudioManager.setStreamVolume(AudioManager.STREAM_ALARM, 1, 0);
            mAudioManager.setStreamVolume(AudioManager.STREAM_SYSTEM, 1, 0);
            mAudioManager.setStreamVolume(AudioManager.STREAM_RING, 1, 0);

            mNotificationManager.setNotificationPolicy(new NotificationManager.Policy(
                    PRIORITY_CATEGORY_ALARMS | PRIORITY_CATEGORY_MEDIA, 0, 0));
            AutomaticZenRule rule = createRule("test_alarms", INTERRUPTION_FILTER_ALARMS);
            String id = mNotificationManager.addAutomaticZenRule(rule);
            mRuleIds.add(id);
            Condition condition =
                    new Condition(rule.getConditionId(), "summary", Condition.STATE_TRUE);
            mNotificationManager.setAutomaticZenRuleState(id, condition);
            mNotificationManager.setInterruptionFilter(INTERRUPTION_FILTER_PRIORITY);

            // delay for streams to get into correct mute states
            Thread.sleep(50);
            assertFalse("Music (media) stream should not be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_MUSIC));
            assertTrue("System stream should be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_SYSTEM));
            assertFalse("Alarm stream should not be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_ALARM));

            // Test requires that the phone's default state has no channels that can bypass dnd
            assertTrue("Ringer stream should be muted",
                    mAudioManager.isStreamMute(AudioManager.STREAM_RING));
        } finally {
            mNotificationManager.setInterruptionFilter(originalFilter);
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testAddAutomaticZenRule_configActivity() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        AutomaticZenRule ruleToCreate = createRule("Rule");
        String id = mNotificationManager.addAutomaticZenRule(ruleToCreate);

        assertNotNull(id);
        mRuleIds.add(id);
        assertTrue(areRulesSame(ruleToCreate, mNotificationManager.getAutomaticZenRule(id)));
    }

    public void testUpdateAutomaticZenRule_configActivity() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        AutomaticZenRule ruleToCreate = createRule("Rule");
        String id = mNotificationManager.addAutomaticZenRule(ruleToCreate);
        ruleToCreate.setEnabled(false);
        mNotificationManager.updateAutomaticZenRule(id, ruleToCreate);

        assertNotNull(id);
        mRuleIds.add(id);
        assertTrue(areRulesSame(ruleToCreate, mNotificationManager.getAutomaticZenRule(id)));
    }

    public void testRemoveAutomaticZenRule_configActivity() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        AutomaticZenRule ruleToCreate = createRule("Rule");
        String id = mNotificationManager.addAutomaticZenRule(ruleToCreate);

        assertNotNull(id);
        mRuleIds.add(id);
        mNotificationManager.removeAutomaticZenRule(id);

        assertNull(mNotificationManager.getAutomaticZenRule(id));
        assertEquals(0, mNotificationManager.getAutomaticZenRules().size());
    }

    public void testSetAutomaticZenRuleState() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        AutomaticZenRule ruleToCreate = createRule("Rule");
        String id = mNotificationManager.addAutomaticZenRule(ruleToCreate);
        mRuleIds.add(id);

        // make sure DND is off
        assertExpectedDndState(INTERRUPTION_FILTER_ALL);

        // enable DND
        Condition condition =
                new Condition(ruleToCreate.getConditionId(), "summary", Condition.STATE_TRUE);
        mNotificationManager.setAutomaticZenRuleState(id, condition);

        assertExpectedDndState(ruleToCreate.getInterruptionFilter());
    }

    public void testSetAutomaticZenRuleState_turnOff() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        AutomaticZenRule ruleToCreate = createRule("Rule");
        String id = mNotificationManager.addAutomaticZenRule(ruleToCreate);
        mRuleIds.add(id);

        // make sure DND is off
        // make sure DND is off
        assertExpectedDndState(INTERRUPTION_FILTER_ALL);

        // enable DND
        Condition condition =
                new Condition(ruleToCreate.getConditionId(), "on", Condition.STATE_TRUE);
        mNotificationManager.setAutomaticZenRuleState(id, condition);

        assertExpectedDndState(ruleToCreate.getInterruptionFilter());

        // disable DND
        condition = new Condition(ruleToCreate.getConditionId(), "off", Condition.STATE_FALSE);

        mNotificationManager.setAutomaticZenRuleState(id, condition);

        // make sure DND is off
        assertExpectedDndState(INTERRUPTION_FILTER_ALL);
    }

    public void testSetAutomaticZenRuleState_deletedRule() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        AutomaticZenRule ruleToCreate = createRule("Rule");
        String id = mNotificationManager.addAutomaticZenRule(ruleToCreate);
        mRuleIds.add(id);

        // make sure DND is off
        assertExpectedDndState(INTERRUPTION_FILTER_ALL);

        // enable DND
        Condition condition =
                new Condition(ruleToCreate.getConditionId(), "summary", Condition.STATE_TRUE);
        mNotificationManager.setAutomaticZenRuleState(id, condition);

        assertExpectedDndState(ruleToCreate.getInterruptionFilter());

        mNotificationManager.removeAutomaticZenRule(id);

        // make sure DND is off
        assertExpectedDndState(INTERRUPTION_FILTER_ALL);
    }

    public void testSetAutomaticZenRuleState_multipleRules() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);

        AutomaticZenRule ruleToCreate = createRule("Rule");
        String id = mNotificationManager.addAutomaticZenRule(ruleToCreate);
        mRuleIds.add(id);

        AutomaticZenRule secondRuleToCreate = createRule("Rule 2");
        secondRuleToCreate.setInterruptionFilter(INTERRUPTION_FILTER_NONE);
        String secondId = mNotificationManager.addAutomaticZenRule(secondRuleToCreate);
        mRuleIds.add(secondId);

        // make sure DND is off
        assertExpectedDndState(INTERRUPTION_FILTER_ALL);

        // enable DND
        Condition condition =
                new Condition(ruleToCreate.getConditionId(), "summary", Condition.STATE_TRUE);
        mNotificationManager.setAutomaticZenRuleState(id, condition);
        Condition secondCondition =
                new Condition(secondRuleToCreate.getConditionId(), "summary", Condition.STATE_TRUE);
        mNotificationManager.setAutomaticZenRuleState(secondId, secondCondition);

        // the second rule has a 'more silent' DND filter, so the system wide DND should be
        // using its filter
        assertExpectedDndState(secondRuleToCreate.getInterruptionFilter());

        // remove intense rule, system should fallback to other rule
        mNotificationManager.removeAutomaticZenRule(secondId);
        assertExpectedDndState(ruleToCreate.getInterruptionFilter());
    }

    public void testSetNotificationPolicy_P_setOldFields() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {
                NotificationManager.Policy appPolicy = new NotificationManager.Policy(0, 0, 0,
                        SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_SCREEN_OFF);
                mNotificationManager.setNotificationPolicy(appPolicy);

                int expected = SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_SCREEN_OFF
                        | SUPPRESSED_EFFECT_PEEK | SUPPRESSED_EFFECT_AMBIENT
                        | SUPPRESSED_EFFECT_LIGHTS | SUPPRESSED_EFFECT_FULL_SCREEN_INTENT;

                assertEquals(expected,
                        mNotificationManager.getNotificationPolicy().suppressedVisualEffects);
            }
        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testSetNotificationPolicy_P_setNewFields() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {
                NotificationManager.Policy appPolicy = new NotificationManager.Policy(0, 0, 0,
                        SUPPRESSED_EFFECT_NOTIFICATION_LIST | SUPPRESSED_EFFECT_AMBIENT
                                | SUPPRESSED_EFFECT_LIGHTS | SUPPRESSED_EFFECT_FULL_SCREEN_INTENT);
                mNotificationManager.setNotificationPolicy(appPolicy);

                int expected = SUPPRESSED_EFFECT_NOTIFICATION_LIST | SUPPRESSED_EFFECT_SCREEN_OFF
                        | SUPPRESSED_EFFECT_AMBIENT | SUPPRESSED_EFFECT_LIGHTS
                        | SUPPRESSED_EFFECT_FULL_SCREEN_INTENT;
                assertEquals(expected,
                        mNotificationManager.getNotificationPolicy().suppressedVisualEffects);
            }
        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testSetNotificationPolicy_P_setOldNewFields() throws Exception {
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.P) {

                NotificationManager.Policy appPolicy = new NotificationManager.Policy(0, 0, 0,
                        SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_STATUS_BAR);
                mNotificationManager.setNotificationPolicy(appPolicy);

                int expected = SUPPRESSED_EFFECT_STATUS_BAR;
                assertEquals(expected,
                        mNotificationManager.getNotificationPolicy().suppressedVisualEffects);

                appPolicy = new NotificationManager.Policy(0, 0, 0,
                        SUPPRESSED_EFFECT_SCREEN_ON | SUPPRESSED_EFFECT_AMBIENT
                                | SUPPRESSED_EFFECT_LIGHTS | SUPPRESSED_EFFECT_FULL_SCREEN_INTENT);
                mNotificationManager.setNotificationPolicy(appPolicy);

                expected = SUPPRESSED_EFFECT_SCREEN_OFF | SUPPRESSED_EFFECT_AMBIENT
                        | SUPPRESSED_EFFECT_LIGHTS | SUPPRESSED_EFFECT_FULL_SCREEN_INTENT;
                assertEquals(expected,
                        mNotificationManager.getNotificationPolicy().suppressedVisualEffects);
            }
        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
        }
    }

    public void testPostFullScreenIntent_permission() {
        int id = 6000;

        final Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(R.drawable.black)
                        .setWhen(System.currentTimeMillis())
                        .setFullScreenIntent(getPendingIntent(), true)
                        .setContentText("This is #FSI notification")
                        .setContentIntent(getPendingIntent())
                        .build();
        mNotificationManager.notify(id, notification);

        StatusBarNotification n = findPostedNotification(id, false);
        assertNotNull(n);
        assertEquals(notification.fullScreenIntent, n.getNotification().fullScreenIntent);
    }

    public void testNotificationPolicyVisualEffectsEqual() {
        NotificationManager.Policy policy = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_SCREEN_ON);
        NotificationManager.Policy policy2 = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_PEEK);
        assertTrue(policy.equals(policy2));
        assertTrue(policy2.equals(policy));

        policy = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_SCREEN_ON);
        policy2 = new NotificationManager.Policy(0, 0, 0,
                0);
        assertFalse(policy.equals(policy2));
        assertFalse(policy2.equals(policy));

        policy = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_SCREEN_OFF);
        policy2 = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_FULL_SCREEN_INTENT | SUPPRESSED_EFFECT_AMBIENT
                        | SUPPRESSED_EFFECT_LIGHTS);
        assertTrue(policy.equals(policy2));
        assertTrue(policy2.equals(policy));

        policy = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_SCREEN_OFF);
        policy2 = new NotificationManager.Policy(0, 0, 0,
                SUPPRESSED_EFFECT_LIGHTS);
        assertFalse(policy.equals(policy2));
        assertFalse(policy2.equals(policy));
    }

    public void testNotificationDelegate_grantAndPost() throws Exception {
        // grant this test permission to post
        final Intent activityIntent = new Intent();
        activityIntent.setPackage(DELEGATOR);
        activityIntent.setAction(Intent.ACTION_MAIN);
        activityIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // wait for the activity to launch and finish
        mContext.startActivity(activityIntent);
        Thread.sleep(1000);

        // send notification
        Notification n = new Notification.Builder(mContext, "channel")
                .setSmallIcon(android.R.id.icon)
                .build();
        mNotificationManager.notifyAsPackage(DELEGATOR, "tag", 0, n);

        assertNotNull(findPostedNotification(0, false));
        final Intent revokeIntent = new Intent();
        revokeIntent.setClassName(DELEGATOR, REVOKE_CLASS);
        revokeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(revokeIntent);
        Thread.sleep(1000);
    }

    public void testNotificationDelegate_grantAndPostAndCancel() throws Exception {
        // grant this test permission to post
        final Intent activityIntent = new Intent();
        activityIntent.setPackage(DELEGATOR);
        activityIntent.setAction(Intent.ACTION_MAIN);
        activityIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // wait for the activity to launch and finish
        mContext.startActivity(activityIntent);
        Thread.sleep(1000);

        // send notification
        Notification n = new Notification.Builder(mContext, "channel")
                .setSmallIcon(android.R.id.icon)
                .build();
        mNotificationManager.notifyAsPackage(DELEGATOR, "toBeCanceled", 10000, n);
        assertNotNull(findPostedNotification(10000, false));
        mNotificationManager.cancelAsPackage(DELEGATOR, "toBeCanceled", 10000);
        assertNotificationCancelled(10000, false);
        final Intent revokeIntent = new Intent();
        revokeIntent.setClassName(DELEGATOR, REVOKE_CLASS);
        revokeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(revokeIntent);
        Thread.sleep(1000);
    }

    public void testNotificationDelegate_cannotCancelNotificationsPostedByDelegator()
            throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        // grant this test permission to post
        final Intent activityIntent = new Intent();
        activityIntent.setClassName(DELEGATOR, DELEGATE_POST_CLASS);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mContext.startActivity(activityIntent);

        Thread.sleep(1000);

        assertNotNull(findPostedNotification(9, true));

        try {
            mNotificationManager.cancelAsPackage(DELEGATOR, null, 9);
            fail("Delegate should not be able to cancel notification they did not post");
        } catch (SecurityException e) {
            // yay
        }

        // double check that the notification does still exist
        assertNotNull(findPostedNotification(9, true));

        final Intent revokeIntent = new Intent();
        revokeIntent.setClassName(DELEGATOR, REVOKE_CLASS);
        revokeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(revokeIntent);
        Thread.sleep(1000);
    }

    public void testNotificationDelegate_grantAndReadChannels() throws Exception {
        // grant this test permission to post
        final Intent activityIntent = new Intent();
        activityIntent.setPackage(DELEGATOR);
        activityIntent.setAction(Intent.ACTION_MAIN);
        activityIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // wait for the activity to launch and finish
        mContext.startActivity(activityIntent);
        Thread.sleep(500);

        List<NotificationChannel> channels =
                mContext.createPackageContextAsUser(DELEGATOR, /* flags= */ 0, mContext.getUser())
                        .getSystemService(NotificationManager.class)
                        .getNotificationChannels();

        assertNotNull(channels);

        final Intent revokeIntent = new Intent();
        revokeIntent.setClassName(DELEGATOR, REVOKE_CLASS);
        revokeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(revokeIntent);
        Thread.sleep(500);
    }

    public void testNotificationDelegate_grantAndReadChannel() throws Exception {
        // grant this test permission to post
        final Intent activityIntent = new Intent();
        activityIntent.setPackage(DELEGATOR);
        activityIntent.setAction(Intent.ACTION_MAIN);
        activityIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // wait for the activity to launch and finish
        mContext.startActivity(activityIntent);
        Thread.sleep(500);

        NotificationChannel channel =
                mContext.createPackageContextAsUser(DELEGATOR, /* flags= */ 0, mContext.getUser())
                        .getSystemService(NotificationManager.class)
                        .getNotificationChannel("channel");

        assertNotNull(channel);

        final Intent revokeIntent = new Intent();
        revokeIntent.setClassName(DELEGATOR, REVOKE_CLASS);
        revokeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(revokeIntent);
        Thread.sleep(500);
    }

    public void testNotificationDelegate_grantAndRevoke() throws Exception {
        // grant this test permission to post
        final Intent activityIntent = new Intent();
        activityIntent.setPackage(DELEGATOR);
        activityIntent.setAction(Intent.ACTION_MAIN);
        activityIntent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mContext.startActivity(activityIntent);
        Thread.sleep(500);

        assertTrue(mNotificationManager.canNotifyAsPackage(DELEGATOR));

        final Intent revokeIntent = new Intent();
        revokeIntent.setClassName(DELEGATOR, REVOKE_CLASS);
        revokeIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(revokeIntent);
        Thread.sleep(500);

        try {
            // send notification
            Notification n = new Notification.Builder(mContext, "channel")
                    .setSmallIcon(android.R.id.icon)
                    .build();
            mNotificationManager.notifyAsPackage(DELEGATOR, "tag", 0, n);
            fail("Should not be able to post as a delegate when permission revoked");
        } catch (SecurityException e) {
            // yay
        }
    }

    public void testAreBubblesAllowed_appNone() throws Exception {
        setBubblesAppPref(0 /* none */);
        assertFalse(mNotificationManager.areBubblesAllowed());
    }

    public void testAreBubblesAllowed_appSelected() throws Exception {
        setBubblesAppPref(2 /* selected */);
        assertFalse(mNotificationManager.areBubblesAllowed());
    }

    public void testAreBubblesAllowed_appAll() throws Exception {
        setBubblesAppPref(1 /* all */);
        assertTrue(mNotificationManager.areBubblesAllowed());
    }

    public void testNotificationIcon() {
        int id = 6000;

        Notification notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(android.R.id.icon)
                        .setWhen(System.currentTimeMillis())
                        .setFullScreenIntent(getPendingIntent(), true)
                        .setContentText("This notification has a resource icon")
                        .setContentIntent(getPendingIntent())
                        .build();
        mNotificationManager.notify(id, notification);

        notification =
                new Notification.Builder(mContext, NOTIFICATION_CHANNEL_ID)
                        .setSmallIcon(Icon.createWithResource(mContext, android.R.id.icon))
                        .setWhen(System.currentTimeMillis())
                        .setFullScreenIntent(getPendingIntent(), true)
                        .setContentText("This notification has an Icon icon")
                        .setContentIntent(getPendingIntent())
                        .build();
        mNotificationManager.notify(id, notification);

        StatusBarNotification n = findPostedNotification(id, false);
        assertNotNull(n);
    }

    public void testShouldHideSilentStatusIcons() throws Exception {
        try {
            mNotificationManager.shouldHideSilentStatusBarIcons();
            fail("Non-privileged apps should not get this information");
        } catch (SecurityException e) {
            // pass
        }

        toggleListenerAccess(true);
        // no exception this time
        mNotificationManager.shouldHideSilentStatusBarIcons();
    }

    public void testMatchesCallFilter() throws Exception {
        // allow all callers
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);
        Policy origPolicy = mNotificationManager.getNotificationPolicy();
        try {
            NotificationManager.Policy currPolicy = mNotificationManager.getNotificationPolicy();
            NotificationManager.Policy newPolicy = new NotificationManager.Policy(
                    NotificationManager.Policy.PRIORITY_CATEGORY_CALLS
                            | NotificationManager.Policy.PRIORITY_CATEGORY_REPEAT_CALLERS,
                    NotificationManager.Policy.PRIORITY_SENDERS_ANY,
                    currPolicy.priorityMessageSenders,
                    currPolicy.suppressedVisualEffects);
            mNotificationManager.setNotificationPolicy(newPolicy);

            // add a contact
            String ALICE = "Alice";
            String ALICE_PHONE = "+16175551212";
            String ALICE_EMAIL = "alice@_foo._bar";

            insertSingleContact(ALICE, ALICE_PHONE, ALICE_EMAIL, false);

            final Bundle peopleExtras = new Bundle();
            ArrayList<Person> personList = new ArrayList<>();
            personList.add(
                    new Person.Builder().setUri(lookupContact(ALICE_PHONE).toString()).build());
            peopleExtras.putParcelableArrayList(Notification.EXTRA_PEOPLE_LIST, personList);
            SystemUtil.runWithShellPermissionIdentity(() ->
                    assertTrue(mNotificationManager.matchesCallFilter(peopleExtras)));
        } finally {
            mNotificationManager.setNotificationPolicy(origPolicy);
        }

    }

    /* Confirm that the optional methods of TestNotificationListener still exist and
     * don't fail. */
    public void testNotificationListenerMethods() {
        NotificationListenerService listener = new TestNotificationListener();
        listener.onListenerConnected();

        listener.onSilentStatusBarIconsVisibilityChanged(false);

        listener.onNotificationPosted(null);
        listener.onNotificationPosted(null, null);

        listener.onNotificationRemoved(null);
        listener.onNotificationRemoved(null, null);

        listener.onNotificationChannelGroupModified("", UserHandle.CURRENT, null,
                NotificationListenerService.NOTIFICATION_CHANNEL_OR_GROUP_ADDED);
        listener.onNotificationChannelModified("", UserHandle.CURRENT, null,
                NotificationListenerService.NOTIFICATION_CHANNEL_OR_GROUP_ADDED);

        listener.onListenerDisconnected();
    }

    private void performNotificationProviderAction(@NonNull String action) {
        // Create an intent to launch an activity which just posts or cancels notifications
        Intent activityIntent = new Intent();
        activityIntent.setClassName(NOTIFICATIONPROVIDER, RICH_NOTIFICATION_ACTIVITY);
        activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activityIntent.putExtra("action", action);
        mContext.startActivity(activityIntent);
    }

    public void testNotificationUriPermissionsGranted() throws Exception {
        Uri background7Uri = Uri.parse(
                "content://com.android.test.notificationprovider.provider/background7.png");
        Uri background8Uri = Uri.parse(
                "content://com.android.test.notificationprovider.provider/background8.png");

        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        try {
            // Post #7
            performNotificationProviderAction("send-7");

            assertEquals(background7Uri, getNotificationBackgroundImageUri(7));
            assertNotificationCancelled(8, true);
            assertAccessible(background7Uri);
            assertInaccessible(background8Uri);

            // Post #8
            performNotificationProviderAction("send-8");

            assertEquals(background7Uri, getNotificationBackgroundImageUri(7));
            assertEquals(background8Uri, getNotificationBackgroundImageUri(8));
            assertAccessible(background7Uri);
            assertAccessible(background8Uri);

            // Cancel #7
            performNotificationProviderAction("cancel-7");

            assertNotificationCancelled(7, true);
            assertEquals(background8Uri, getNotificationBackgroundImageUri(8));
            assertInaccessible(background7Uri);
            assertAccessible(background8Uri);

            // Cancel #8
            performNotificationProviderAction("cancel-8");

            assertNotificationCancelled(7, true);
            assertNotificationCancelled(8, true);
            assertInaccessible(background7Uri);
            assertInaccessible(background8Uri);

        } finally {
            // Clean up -- reset any remaining notifications
            performNotificationProviderAction("reset");
            Thread.sleep(500);
        }
    }

    public void testNotificationUriPermissionsGrantedToNewListeners() throws Exception {
        Uri background7Uri = Uri.parse(
                "content://com.android.test.notificationprovider.provider/background7.png");

        try {
            // Post #7
            performNotificationProviderAction("send-7");

            Thread.sleep(500);
            // Don't have access the notification yet, but we can test the URI
            assertInaccessible(background7Uri);

            toggleListenerAccess(true);
            Thread.sleep(500); // wait for listener to be allowed

            mListener = TestNotificationListener.getInstance();
            assertNotNull(mListener);

            assertEquals(background7Uri, getNotificationBackgroundImageUri(7));
            assertAccessible(background7Uri);

        } finally {
            // Clean Up -- Cancel #7
            performNotificationProviderAction("cancel-7");
            Thread.sleep(500);
        }
    }

    public void testNotificationUriPermissionsRevokedFromRemovedListeners() throws Exception {
        Uri background7Uri = Uri.parse(
                "content://com.android.test.notificationprovider.provider/background7.png");

        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        try {
            // Post #7
            performNotificationProviderAction("send-7");

            mListener = TestNotificationListener.getInstance();
            assertNotNull(mListener);

            assertEquals(background7Uri, getNotificationBackgroundImageUri(7));
            assertAccessible(background7Uri);

            // Remove the listener to ensure permissions get revoked
            toggleListenerAccess(false);
            Thread.sleep(500); // wait for listener to be disabled

            assertInaccessible(background7Uri);

        } finally {
            // Clean Up -- Cancel #7
            performNotificationProviderAction("cancel-7");
            Thread.sleep(500);
        }
    }

    private class NotificationListenerConnection implements ServiceConnection {
        private final Semaphore mSemaphore = new Semaphore(0);

        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            mNotificationUriAccessService = INotificationUriAccessService.Stub.asInterface(service);
            mSemaphore.release();
        }

        @Override
        public void onServiceDisconnected(ComponentName className) {
            mNotificationUriAccessService = null;
        }

        public void waitForService() {
            try {
                if (mSemaphore.tryAcquire(5, TimeUnit.SECONDS)) {
                    return;
                }
            } catch (InterruptedException e) {
            }
            fail("failed to connec to service");
        }
    }

    ;

    public void testNotificationUriPermissionsRevokedOnlyFromRemovedListeners() throws Exception {
        Uri background7Uri = Uri.parse(
                "content://com.android.test.notificationprovider.provider/background7.png");

        // Connect to a service in the NotificationListener app which allows us to validate URI
        // permissions granted to a second app, so that we show that permissions aren't being
        // revoked too broadly.
        final Intent intent = new Intent();
        intent.setComponent(new ComponentName("com.android.test.notificationlistener",
                "com.android.test.notificationlistener.NotificationUriAccessService"));
        NotificationListenerConnection connection = new NotificationListenerConnection();
        mContext.bindService(intent, connection, Context.BIND_AUTO_CREATE);
        connection.waitForService();

        // Before starting the test, make sure the service works, that there is no listener, and
        // that the URI starts inaccessible to that process.
        mNotificationUriAccessService.ensureNotificationListenerServiceConnected(false);
        assertFalse(mNotificationUriAccessService.isFileUriAccessible(background7Uri));

        // Give the NotificationListener app access to notifications, and validate that.
        toggleExternalListenerAccess(new ComponentName("com.android.test.notificationlistener",
                "com.android.test.notificationlistener.TestNotificationListener"), true);
        Thread.sleep(500);
        mNotificationUriAccessService.ensureNotificationListenerServiceConnected(true);
        assertFalse(mNotificationUriAccessService.isFileUriAccessible(background7Uri));

        // Give the test app access to notifications, and get that listener
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed
        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        try {
            try {
                // Post #7
                performNotificationProviderAction("send-7");

                // Check that both the test app (this code) and the external app have URI access.
                assertEquals(background7Uri, getNotificationBackgroundImageUri(7));
                assertAccessible(background7Uri);
                assertTrue(mNotificationUriAccessService.isFileUriAccessible(background7Uri));

                // Remove the listener to ensure permissions get revoked
                toggleListenerAccess(false);
                Thread.sleep(500); // wait for listener to be disabled

                // Ensure that revoking listener access to this one app does not effect the other.
                assertInaccessible(background7Uri);
                assertTrue(mNotificationUriAccessService.isFileUriAccessible(background7Uri));

            } finally {
                // Clean Up -- Cancel #7
                performNotificationProviderAction("cancel-7");
                Thread.sleep(500);
            }

            // Finally, cancelling the permission must still revoke those other permissions.
            assertFalse(mNotificationUriAccessService.isFileUriAccessible(background7Uri));

        } finally {
            // Clean Up -- Make sure the external listener is has access revoked
            toggleExternalListenerAccess(new ComponentName("com.android.test.notificationlistener",
                    "com.android.test.notificationlistener.TestNotificationListener"), false);
        }
    }

    private void assertAccessible(Uri uri)
            throws IOException {
        ContentResolver contentResolver = mContext.getContentResolver();
        try (AssetFileDescriptor fd = contentResolver.openAssetFile(uri, "r", null)) {
            assertNotNull(fd);
        } catch (SecurityException e) {
            throw new AssertionError("URI should be accessible: " + uri, e);
        }
    }

    private void assertInaccessible(Uri uri)
            throws IOException {
        ContentResolver contentResolver = mContext.getContentResolver();
        try (AssetFileDescriptor fd = contentResolver.openAssetFile(uri, "r", null)) {
            fail("URI should be inaccessible: " + uri);
        } catch (SecurityException e) {
            // pass
        }
    }

    @NonNull
    private Uri getNotificationBackgroundImageUri(int notificationId) {
        StatusBarNotification sbn = findPostedNotification(notificationId, true);
        assertNotNull(sbn);
        String imageUriString = sbn.getNotification().extras
                .getString(Notification.EXTRA_BACKGROUND_IMAGE_URI);
        assertNotNull(imageUriString);
        return Uri.parse(imageUriString);
    }

    public void testNotificationListener_setNotificationsShown() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);
        final int notificationId1 = 1003;
        final int notificationId2 = 1004;

        sendNotification(notificationId1, R.drawable.black);
        sendNotification(notificationId2, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification

        StatusBarNotification sbn1 = findPostedNotification(notificationId1, false);
        StatusBarNotification sbn2 = findPostedNotification(notificationId2, false);
        mListener.setNotificationsShown(new String[]{ sbn1.getKey() });

        toggleListenerAccess(false);
        Thread.sleep(500); // wait for listener to be disallowed
        try {
            mListener.setNotificationsShown(new String[]{ sbn2.getKey() });
            fail("Should not be able to set shown if listener access isn't granted");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNotificationListener_getNotificationChannels() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        try {
            mListener.getNotificationChannels(mContext.getPackageName(), UserHandle.CURRENT);
            fail("Shouldn't be able get channels without CompanionDeviceManager#getAssociations()");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNotificationListener_getNotificationChannelGroups() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);
        try {
            mListener.getNotificationChannelGroups(mContext.getPackageName(), UserHandle.CURRENT);
            fail("Should not be able get groups without CompanionDeviceManager#getAssociations()");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNotificationListener_updateNotificationChannel() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        NotificationChannel channel = new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", IMPORTANCE_DEFAULT);
        try {
            mListener.updateNotificationChannel(mContext.getPackageName(), UserHandle.CURRENT,
                    channel);
            fail("Shouldn't be able to update channel without "
                    + "CompanionDeviceManager#getAssociations()");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testNotificationListener_getActiveNotifications() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);
        final int notificationId1 = 1001;
        final int notificationId2 = 1002;

        sendNotification(notificationId1, R.drawable.black);
        sendNotification(notificationId2, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification

        StatusBarNotification sbn1 = findPostedNotification(notificationId1, false);
        StatusBarNotification sbn2 = findPostedNotification(notificationId2, false);
        StatusBarNotification[] notifs =
                mListener.getActiveNotifications(new String[]{ sbn2.getKey(), sbn1.getKey() });
        assertEquals(sbn2.getKey(), notifs[0].getKey());
        assertEquals(sbn2.getId(), notifs[0].getId());
        assertEquals(sbn2.getPackageName(), notifs[0].getPackageName());

        assertEquals(sbn1.getKey(), notifs[1].getKey());
        assertEquals(sbn1.getId(), notifs[1].getId());
        assertEquals(sbn1.getPackageName(), notifs[1].getPackageName());
    }


    public void testNotificationListener_getCurrentRanking() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);

        sendNotification(1, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification

        assertEquals(mListener.mRankingMap, mListener.getCurrentRanking());
    }

    public void testNotificationListener_cancelNotifications() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(500); // wait for listener to be allowed

        mListener = TestNotificationListener.getInstance();
        assertNotNull(mListener);
        final int notificationId = 1006;

        sendNotification(notificationId, R.drawable.black);
        Thread.sleep(500); // wait for notification listener to receive notification

        StatusBarNotification sbn = findPostedNotification(notificationId, false);

        mListener.cancelNotification(sbn.getPackageName(), sbn.getTag(), sbn.getId());
        if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.LOLLIPOP) {
            if (!checkNotificationExistence(notificationId, /*shouldExist=*/ true)) {
                fail("Notification shouldn't have been cancelled. "
                        + "cancelNotification(String, String, int) shouldn't cancel notif for L+");
            }
        } else {
            // Tested in LegacyNotificationManager20Test
            if (checkNotificationExistence(notificationId, /*shouldExist=*/ true)) {
                fail("Notification should have been cancelled for targetSdk below L.  targetSdk="
                        + mContext.getApplicationInfo().targetSdkVersion);
            }
        }

        mListener.cancelNotifications(new String[]{ sbn.getKey() });
        if (!checkNotificationExistence(notificationId, /*shouldExist=*/ false)) {
            fail("Failed to cancel notification id=" + notificationId);
        }
    }

    public void testNotificationManagerPolicy_priorityCategoriesToString() {
        String zeroString = NotificationManager.Policy.priorityCategoriesToString(0);
        assertEquals("priorityCategories of 0 produces empty string", "", zeroString);

        String oneString = NotificationManager.Policy.priorityCategoriesToString(1);
        assertNotNull("priorityCategories of 1 returns a string", oneString);
        boolean lengthGreaterThanZero = oneString.length() > 0;
        assertTrue("priorityCategories of 1 returns a string with length greater than 0",
                lengthGreaterThanZero);

        String badNumberString = NotificationManager.Policy.priorityCategoriesToString(1234567);
        assertNotNull("priorityCategories with a non-relevant int returns a string",
                badNumberString);
    }

    public void testNotificationManagerPolicy_prioritySendersToString() {
        String zeroString = NotificationManager.Policy.prioritySendersToString(0);
        assertNotNull("prioritySenders of 1 returns a string", zeroString);
        boolean lengthGreaterThanZero = zeroString.length() > 0;
        assertTrue("prioritySenders of 1 returns a string with length greater than 0",
                lengthGreaterThanZero);

        String badNumberString = NotificationManager.Policy.prioritySendersToString(1234567);
        assertNotNull("prioritySenders with a non-relevant int returns a string", badNumberString);
    }

    public void testNotificationManagerPolicy_suppressedEffectsToString() {
        String zeroString = NotificationManager.Policy.suppressedEffectsToString(0);
        assertEquals("suppressedEffects of 0 produces empty string", "", zeroString);

        String oneString = NotificationManager.Policy.suppressedEffectsToString(1);
        assertNotNull("suppressedEffects of 1 returns a string", oneString);
        boolean lengthGreaterThanZero = oneString.length() > 0;
        assertTrue("suppressedEffects of 1 returns a string with length greater than 0",
                lengthGreaterThanZero);

        String badNumberString = NotificationManager.Policy.suppressedEffectsToString(1234567);
        assertNotNull("suppressedEffects with a non-relevant int returns a string",
                badNumberString);
    }

    public void testNotificationManagerBubblePolicy_flag_intentBubble()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);
            createDynamicShortcut();

            Notification.Builder nb = getConversationNotification();
            boolean shouldBeBubble = !mActivityManager.isLowRamDevice();
            sendAndVerifyBubble(1, nb, null /* use default metadata */, shouldBeBubble);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_noFlag_service()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        Intent serviceIntent = new Intent(mContext, BubblesTestService.class);
        serviceIntent.putExtra(EXTRA_TEST_CASE, TEST_MESSAGING);
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            setUpNotifListener();

            mContext.startService(serviceIntent);

            // No services in R (allowed in Q)
            verifyNotificationBubbleState(BUBBLE_NOTIF_ID, false /* shouldBeBubble */);
        } finally {
            deleteShortcuts();
            mContext.stopService(serviceIntent);
        }
    }

    public void testNotificationManagerBubblePolicy_noFlag_phonecall()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        Intent serviceIntent = new Intent(mContext, BubblesTestService.class);
        serviceIntent.putExtra(EXTRA_TEST_CASE, TEST_CALL);

        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            setUpNotifListener();

            mContext.startService(serviceIntent);

            // No phonecalls in R (allowed in Q)
            verifyNotificationBubbleState(BUBBLE_NOTIF_ID, false /* shouldBeBubble */);
        } finally {
            deleteShortcuts();
            mContext.stopService(serviceIntent);
        }
    }

    public void testNotificationManagerBubblePolicy_noFlag_foreground() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            setUpNotifListener();

            // Start & get the activity
            SendBubbleActivity a = startSendBubbleActivity();
            // Send a bubble that doesn't fulfill policy from foreground
            a.sendInvalidBubble(false /* autoExpand */);

            // No foreground bubbles that don't fulfill policy in R (allowed in Q)
            verifyNotificationBubbleState(BUBBLE_NOTIF_ID, false /* shouldBeBubble */);
        } finally {
            deleteShortcuts();
            cleanupSendBubbleActivity();
        }
    }

    public void testNotificationManagerBubble_checkActivityFlagsDocumentLaunchMode()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()
                || mActivityManager.isLowRamDevice()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            setUpNotifListener();

            // make ourselves foreground so we can auto-expand the bubble & check the intent flags
            SendBubbleActivity a = startSendBubbleActivity();

            // Prep to find bubbled activity
            Class clazz = BubbledActivity.class;
            Instrumentation.ActivityResult result =
                    new Instrumentation.ActivityResult(0, new Intent());
            Instrumentation.ActivityMonitor monitor =
                    new Instrumentation.ActivityMonitor(clazz.getName(), result, false);
            InstrumentationRegistry.getInstrumentation().addMonitor(monitor);

            a.sendBubble(true /* autoExpand */, false /* suppressNotif */);

            boolean shouldBeBubble = !mActivityManager.isLowRamDevice();
            verifyNotificationBubbleState(BUBBLE_NOTIF_ID, shouldBeBubble);

            InstrumentationRegistry.getInstrumentation().waitForIdleSync();

            BubbledActivity activity = (BubbledActivity) monitor.waitForActivity();
            assertTrue((activity.getIntent().getFlags() & FLAG_ACTIVITY_NEW_DOCUMENT) != 0);
            assertTrue((activity.getIntent().getFlags() & FLAG_ACTIVITY_MULTIPLE_TASK) != 0);
        } finally {
            deleteShortcuts();
            cleanupSendBubbleActivity();
        }
    }

    public void testNotificationManagerBubblePolicy_flag_shortcutBubble()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();

            Notification.Builder nb = getConversationNotification();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();

            boolean shouldBeBubble = !mActivityManager.isLowRamDevice();
            sendAndVerifyBubble(1, nb, data, shouldBeBubble);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_noFlag_invalidShortcut()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();

            Notification.Builder nb = getConversationNotification();
            nb.setShortcutId("invalid");
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder("invalid")
                            .build();

            sendAndVerifyBubble(1, nb, data, false);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_noFlag_invalidNotif()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();

            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();

            sendAndVerifyBubble(1, null /* use default notif builder */, data,
                    false /* shouldBeBubble */);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_appAll_globalOn() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();
            Notification.Builder nb = getConversationNotification();

            boolean shouldBeBubble = !mActivityManager.isLowRamDevice();
            sendAndVerifyBubble(1, nb, data, shouldBeBubble);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_appAll_globalOff() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(false);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();
            Notification.Builder nb = getConversationNotification();

            sendAndVerifyBubble(1, nb, data, false);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_appAll_channelNo() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(false);

            createDynamicShortcut();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();
            Notification.Builder nb = getConversationNotification();

            sendAndVerifyBubble(1, nb, data, false);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_appSelected_channelNo() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(2 /* selected */);
            setBubblesChannelAllowed(false);

            createDynamicShortcut();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();
            Notification.Builder nb = getConversationNotification();

            sendAndVerifyBubble(1, nb, data, false);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_appSelected_channelYes() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(2 /* selected */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();
            Notification.Builder nb = getConversationNotification();

            boolean shouldBeBubble = !mActivityManager.isLowRamDevice();
            sendAndVerifyBubble(1, nb, data, shouldBeBubble);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_appNone_channelNo() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(0 /* none */);
            setBubblesChannelAllowed(false);

            createDynamicShortcut();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();
            Notification.Builder nb = getConversationNotification();

            sendAndVerifyBubble(1, nb, data, false);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubblePolicy_noFlag_shortcutRemoved()
            throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()
                    || mActivityManager.isLowRamDevice()) {
            // These do not support bubbles.
            return;
        }

        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);
            createDynamicShortcut();
            Notification.BubbleMetadata data =
                    new Notification.BubbleMetadata.Builder(SHARE_SHORTCUT_ID)
                            .build();
            Notification.Builder nb = getConversationNotification();

            boolean shouldBeBubble = !mActivityManager.isLowRamDevice();
            sendAndVerifyBubble(42, nb, data, shouldBeBubble);
            mListener.resetData();

            deleteShortcuts();
            verifyNotificationBubbleState(42, false /* should be bubble */);
        } finally {
            deleteShortcuts();
        }
    }

    public void testNotificationManagerBubbleNotificationSuppression() throws Exception {
        if (FeatureUtil.isAutomotive() || FeatureUtil.isTV()
                || mActivityManager.isLowRamDevice()) {
            // These do not support bubbles.
            return;
        }
        try {
            setBubblesGlobal(true);
            setBubblesAppPref(1 /* all */);
            setBubblesChannelAllowed(true);

            createDynamicShortcut();
            setUpNotifListener();

            // make ourselves foreground so we can specify suppress notification flag
            SendBubbleActivity a = startSendBubbleActivity();

            // send the bubble with notification suppressed
            a.sendBubble(false /* autoExpand */, true /* suppressNotif */);
            boolean shouldBeBubble = !mActivityManager.isLowRamDevice();
            verifyNotificationBubbleState(BUBBLE_NOTIF_ID, shouldBeBubble);

            // check for the notification
            StatusBarNotification sbnSuppressed = mListener.mPosted.get(0);
            assertNotNull(sbnSuppressed);
            // check for suppression state
            Notification.BubbleMetadata metadata =
                    sbnSuppressed.getNotification().getBubbleMetadata();
            assertNotNull(metadata);
            assertTrue(metadata.isNotificationSuppressed());

            mListener.resetData();

            // send the bubble with notification NOT suppressed
            a.sendBubble(false /* autoExpand */, false /* suppressNotif */);
            verifyNotificationBubbleState(BUBBLE_NOTIF_ID, shouldBeBubble);

            // check for the notification
            StatusBarNotification sbnNotSuppressed = mListener.mPosted.get(0);
            assertNotNull(sbnNotSuppressed);
            // check for suppression state
            metadata = sbnNotSuppressed.getNotification().getBubbleMetadata();
            assertNotNull(metadata);
            assertFalse(metadata.isNotificationSuppressed());
        } finally {
            cleanupSendBubbleActivity();
            deleteShortcuts();
        }
    }

    public void testOriginalChannelImportance() {
        NotificationChannel channel = new NotificationChannel(
                "my channel", "my channel", IMPORTANCE_HIGH);

        mNotificationManager.createNotificationChannel(channel);

        NotificationChannel actual = mNotificationManager.getNotificationChannel(channel.getId());
        assertEquals(IMPORTANCE_HIGH, actual.getImportance());
        assertEquals(IMPORTANCE_HIGH, actual.getOriginalImportance());

        // Apps are allowed to downgrade channel importance if the user has not changed any
        // fields on this channel yet.
        channel.setImportance(IMPORTANCE_DEFAULT);
        mNotificationManager.createNotificationChannel(channel);

        actual = mNotificationManager.getNotificationChannel(channel.getId());
        assertEquals(IMPORTANCE_DEFAULT, actual.getImportance());
        assertEquals(IMPORTANCE_HIGH, actual.getOriginalImportance());
    }

    public void testCreateConversationChannel() {
        final NotificationChannel channel =
                new NotificationChannel(mId, "Messages", IMPORTANCE_DEFAULT);

        String conversationId = "person a";

        final NotificationChannel conversationChannel =
                new NotificationChannel(mId + "child",
                        "Messages from " + conversationId, IMPORTANCE_DEFAULT);
        conversationChannel.setConversationId(channel.getId(), conversationId);

        mNotificationManager.createNotificationChannel(channel);
        mNotificationManager.createNotificationChannel(conversationChannel);

        compareChannels(conversationChannel,
                mNotificationManager.getNotificationChannel(channel.getId(), conversationId));
    }
}
