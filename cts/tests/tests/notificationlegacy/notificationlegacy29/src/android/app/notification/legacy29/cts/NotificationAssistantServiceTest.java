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

package android.app.notification.legacy29.cts;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;
import static junit.framework.TestCase.assertFalse;
import static junit.framework.TestCase.assertNotNull;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.fail;

import android.app.Instrumentation;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.StatusBarManager;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.UserHandle;
import android.provider.Telephony;
import android.service.notification.Adjustment;
import android.service.notification.NotificationAssistantService;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.Reader;
import java.nio.CharBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class NotificationAssistantServiceTest {

    final String TAG = "NotAsstServiceTest";
    final String NOTIFICATION_CHANNEL_ID = "NotificationAssistantServiceTest";
    final int ICON_ID = android.R.drawable.sym_def_app_icon;
    final long SLEEP_TIME = 500; // milliseconds

    private TestNotificationAssistant mNotificationAssistantService;
    private TestNotificationListener mNotificationListenerService;
    private NotificationManager mNotificationManager;
    private StatusBarManager mStatusBarManager;
    private Context mContext;
    private UiAutomation mUi;

    @Before
    public void setUp() throws IOException {
        mUi = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        mContext = InstrumentationRegistry.getContext();
        mNotificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mNotificationManager.createNotificationChannel(new NotificationChannel(
                NOTIFICATION_CHANNEL_ID, "name", NotificationManager.IMPORTANCE_DEFAULT));
        mStatusBarManager = (StatusBarManager) mContext.getSystemService(
                Context.STATUS_BAR_SERVICE);
    }

    @After
    public void tearDown() throws IOException {
        if (mNotificationListenerService != null) mNotificationListenerService.resetData();

        toggleListenerAccess(false);
        toggleAssistantAccess(false);
        mUi.dropShellPermissionIdentity();
    }

    @Test
    public void testOnNotificationEnqueued() throws Exception {
        toggleListenerAccess(true);
        Thread.sleep(SLEEP_TIME);

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_USER_SENTIMENT);
        mUi.dropShellPermissionIdentity();

        mNotificationListenerService = TestNotificationListener.getInstance();

        sendNotification(1, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        NotificationListenerService.Ranking out = new NotificationListenerService.Ranking();
        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        // No modification because the Notification Assistant is not enabled
        assertEquals(NotificationListenerService.Ranking.USER_SENTIMENT_NEUTRAL,
                out.getUserSentiment());
        mNotificationListenerService.resetData();

        toggleAssistantAccess(true);
        Thread.sleep(SLEEP_TIME); // wait for listener and assistant to be allowed
        mNotificationAssistantService = TestNotificationAssistant.getInstance();

        sendNotification(1, ICON_ID);
        sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        // Assistant modifies notification
        assertEquals(NotificationListenerService.Ranking.USER_SENTIMENT_POSITIVE,
                out.getUserSentiment());
    }

    @Test
    public void testAdjustNotification_userSentimentKey() throws Exception {
        setUpListeners();

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_USER_SENTIMENT);
        mUi.dropShellPermissionIdentity();

        sendNotification(1, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        NotificationListenerService.Ranking out = new NotificationListenerService.Ranking();
        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        assertEquals(NotificationListenerService.Ranking.USER_SENTIMENT_POSITIVE,
                out.getUserSentiment());

        Bundle signals = new Bundle();
        signals.putInt(Adjustment.KEY_USER_SENTIMENT,
                NotificationListenerService.Ranking.USER_SENTIMENT_NEGATIVE);
        Adjustment adjustment = new Adjustment(sbn.getPackageName(), sbn.getKey(), signals, "",
                sbn.getUser());

        mNotificationAssistantService.adjustNotification(adjustment);
        Thread.sleep(SLEEP_TIME); // wait for adjustment to be processed

        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        assertEquals(NotificationListenerService.Ranking.USER_SENTIMENT_NEGATIVE,
                out.getUserSentiment());
    }

    @Test
    public void testAdjustNotification_importanceKey() throws Exception {
        setUpListeners();

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_IMPORTANCE);
        mUi.dropShellPermissionIdentity();

        sendNotification(1, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        NotificationListenerService.Ranking out = new NotificationListenerService.Ranking();
        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        int currentImportance = out.getImportance();
        int newImportance = currentImportance == NotificationManager.IMPORTANCE_DEFAULT
                ? NotificationManager.IMPORTANCE_HIGH : NotificationManager.IMPORTANCE_DEFAULT;

        Bundle signals = new Bundle();
        signals.putInt(Adjustment.KEY_IMPORTANCE, newImportance);
        Adjustment adjustment = new Adjustment(sbn.getPackageName(), sbn.getKey(), signals, "",
                sbn.getUser());

        mNotificationAssistantService.adjustNotification(adjustment);
        Thread.sleep(SLEEP_TIME); // wait for adjustment to be processed

        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        assertEquals(newImportance, out.getImportance());
    }

    @Test
    public void testAdjustNotifications_rankingScoreKey() throws Exception {
        setUpListeners();

        try {
            mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
            mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_RANKING_SCORE);
            mUi.dropShellPermissionIdentity();

            sendNotification(1, ICON_ID);
            StatusBarNotification sbn1 = getFirstNotificationFromPackage(
                    TestNotificationListener.PKG);
            NotificationListenerService.Ranking out1 = new NotificationListenerService.Ranking();

            sendNotification(2, ICON_ID);
            StatusBarNotification sbn2 = getFirstNotificationFromPackage(
                    TestNotificationListener.PKG);
            NotificationListenerService.Ranking out2 = new NotificationListenerService.Ranking();

            mNotificationListenerService.mRankingMap.getRanking(sbn1.getKey(), out1);
            mNotificationListenerService.mRankingMap.getRanking(sbn2.getKey(), out2);

            int currentRank1 = out1.getRank();
            int currentRank2 = out2.getRank();

            float rankingScore1 = (currentRank1 > currentRank2) ? 1f : 0;
            float rankingScore2 = (currentRank1 > currentRank2) ? 0 : 1f;

            Bundle signals = new Bundle();
            signals.putFloat(Adjustment.KEY_RANKING_SCORE, rankingScore1);
            Adjustment adjustment = new Adjustment(sbn1.getPackageName(), sbn1.getKey(), signals, "",
                    sbn1.getUser());
            Bundle signals2 = new Bundle();
            signals2.putFloat(Adjustment.KEY_RANKING_SCORE, rankingScore2);
            Adjustment adjustment2 = new Adjustment(sbn2.getPackageName(), sbn2.getKey(), signals2, "",
                    sbn2.getUser());
            mNotificationAssistantService.adjustNotifications(List.of(adjustment, adjustment2));
            Thread.sleep(SLEEP_TIME); // wait for adjustments to be processed

            mNotificationListenerService.mRankingMap.getRanking(sbn1.getKey(), out1);
            mNotificationListenerService.mRankingMap.getRanking(sbn2.getKey(), out2);

            // verify the relative ordering changed
            int newRank1 = out1.getRank();
            int newRank2 = out2.getRank();
            if (currentRank1 > currentRank2) {
                assertTrue(newRank1 < newRank2);
            } else {
                assertTrue(newRank1 > newRank2);
            }
        } finally {
            mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
            mNotificationManager.disallowAssistantAdjustment(Adjustment.KEY_RANKING_SCORE);
            mUi.dropShellPermissionIdentity();
        }
    }

    @Test
    public void testAdjustNotification_smartActionKey() throws Exception {
        setUpListeners();

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_CONTEXTUAL_ACTIONS);
        mUi.dropShellPermissionIdentity();

        PendingIntent sendIntent = PendingIntent.getActivity(mContext, 0,
                new Intent(Intent.ACTION_SEND), 0);
        Notification.Action sendAction = new Notification.Action.Builder(ICON_ID, "SEND",
                sendIntent).build();

        sendNotification(1, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        NotificationListenerService.Ranking out = new NotificationListenerService.Ranking();
        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        List<Notification.Action> smartActions = out.getSmartActions();
        if (smartActions != null) {
            for (int i = 0; i < smartActions.size(); i++) {
                Notification.Action action = smartActions.get(i);
                assertNotEquals(sendIntent, action.actionIntent);
            }
        }

        ArrayList<Notification.Action> extraAction = new ArrayList<>();
        extraAction.add(sendAction);
        Bundle signals = new Bundle();
        signals.putParcelableArrayList(Adjustment.KEY_CONTEXTUAL_ACTIONS, extraAction);
        Adjustment adjustment = new Adjustment(sbn.getPackageName(), sbn.getKey(), signals, "",
                sbn.getUser());

        mNotificationAssistantService.adjustNotification(adjustment);
        Thread.sleep(SLEEP_TIME); //wait for adjustment to be processed

        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        boolean actionFound = false;
        smartActions = out.getSmartActions();
        for (int i = 0; i < smartActions.size(); i++) {
            Notification.Action action = smartActions.get(i);
            actionFound = actionFound || action.actionIntent.equals(sendIntent);
        }
        assertTrue(actionFound);
    }

    @Test
    public void testAdjustNotification_smartReplyKey() throws Exception {
        setUpListeners();
        CharSequence smartReply = "Smart Reply!";

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_TEXT_REPLIES);
        mUi.dropShellPermissionIdentity();

        sendNotification(1, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        NotificationListenerService.Ranking out = new NotificationListenerService.Ranking();
        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        List<CharSequence> smartReplies = out.getSmartReplies();
        if (smartReplies != null) {
            for (int i = 0; i < smartReplies.size(); i++) {
                CharSequence reply = smartReplies.get(i);
                assertNotEquals(smartReply, reply);
            }
        }

        ArrayList<CharSequence> extraReply = new ArrayList<>();
        extraReply.add(smartReply);
        Bundle signals = new Bundle();
        signals.putCharSequenceArrayList(Adjustment.KEY_TEXT_REPLIES, extraReply);
        Adjustment adjustment = new Adjustment(sbn.getPackageName(), sbn.getKey(), signals, "",
                sbn.getUser());

        mNotificationAssistantService.adjustNotification(adjustment);
        Thread.sleep(SLEEP_TIME); //wait for adjustment to be processed

        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        boolean replyFound = false;
        smartReplies = out.getSmartReplies();
        for (int i = 0; i < smartReplies.size(); i++) {
            CharSequence reply = smartReplies.get(i);
            replyFound = replyFound || reply.equals(smartReply);
        }
        assertTrue(replyFound);
    }

    @Test
    public void testAdjustNotification_importanceKey_notAllowed() throws Exception {
        setUpListeners();

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.disallowAssistantAdjustment(Adjustment.KEY_IMPORTANCE);
        mUi.dropShellPermissionIdentity();

        sendNotification(1, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(
                TestNotificationListener.PKG);
        NotificationListenerService.Ranking out = new NotificationListenerService.Ranking();
        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        int currentImportance = out.getImportance();
        int newImportance = currentImportance == NotificationManager.IMPORTANCE_DEFAULT
                ? NotificationManager.IMPORTANCE_HIGH : NotificationManager.IMPORTANCE_DEFAULT;

        Bundle signals = new Bundle();
        signals.putInt(Adjustment.KEY_IMPORTANCE, newImportance);
        Adjustment adjustment = new Adjustment(sbn.getPackageName(), sbn.getKey(), signals, "",
                sbn.getUser());

        mNotificationAssistantService.adjustNotification(adjustment);
        Thread.sleep(SLEEP_TIME); // wait for adjustment to be processed

        mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);

        assertEquals(currentImportance, out.getImportance());

    }

    @Test
    public void testAdjustNotification_rankingScoreKey_notAllowed() throws Exception {
        setUpListeners();

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.cancelAll();
        mNotificationManager.disallowAssistantAdjustment(Adjustment.KEY_RANKING_SCORE);
        mUi.dropShellPermissionIdentity();

        sendNotification(1, ICON_ID);
        StatusBarNotification sbn1 = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        NotificationListenerService.Ranking out1 = new NotificationListenerService.Ranking();

        sendNotification(2, ICON_ID);
        StatusBarNotification sbn2 = getFirstNotificationFromPackage(TestNotificationListener.PKG);
        NotificationListenerService.Ranking out2 = new NotificationListenerService.Ranking();

        mNotificationListenerService.mRankingMap.getRanking(sbn1.getKey(), out1);
        mNotificationListenerService.mRankingMap.getRanking(sbn2.getKey(), out2);

        int currentRank1 = out1.getRank();
        int currentRank2 = out2.getRank();

        float rankingScore1 = (currentRank1 > currentRank2) ? 1f: 0;
        float rankingScore2 = (currentRank1 > currentRank2) ? 0: 1f;

        Bundle signals = new Bundle();
        signals.putFloat(Adjustment.KEY_RANKING_SCORE, rankingScore1);
        Adjustment adjustment = new Adjustment(sbn1.getPackageName(), sbn1.getKey(), signals, "",
                sbn1.getUser());
        mNotificationAssistantService.adjustNotification(adjustment);
        signals = new Bundle();
        signals.putFloat(Adjustment.KEY_RANKING_SCORE, rankingScore2);
        adjustment = new Adjustment(sbn2.getPackageName(), sbn2.getKey(), signals, "",
                sbn2.getUser());
        mNotificationAssistantService.adjustNotification(adjustment);
        Thread.sleep(SLEEP_TIME); // wait for adjustments to be processed

        mNotificationListenerService.mRankingMap.getRanking(sbn1.getKey(), out1);
        mNotificationListenerService.mRankingMap.getRanking(sbn2.getKey(), out2);

        // verify the relative ordering remains the same
        int newRank1 = out1.getRank();
        int newRank2 = out2.getRank();
        if (currentRank1 > currentRank2) {
            assertTrue(newRank1 > newRank2);
        } else {
            assertTrue(newRank1 < newRank2);
        }
    }

    @Test
    public void testGetAllowedAssistantCapabilities_permission() throws Exception {
        toggleAssistantAccess(false);

        try {
            mNotificationManager.getAllowedAssistantAdjustments();
            fail(" Non assistants cannot call this method");
        } catch (SecurityException e) {
            //pass
        }
    }

    @Test
    public void testGetAllowedAssistantCapabilities() throws Exception {
        toggleAssistantAccess(true);
        Thread.sleep(SLEEP_TIME); // wait for assistant to be allowed
        mNotificationAssistantService = TestNotificationAssistant.getInstance();
        mNotificationAssistantService.onAllowedAdjustmentsChanged();
        assertNotNull(mNotificationAssistantService.currentCapabilities);

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE");
        mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_SNOOZE_CRITERIA);

        Thread.sleep(SLEEP_TIME);
        assertTrue(mNotificationAssistantService.currentCapabilities.contains(
                Adjustment.KEY_SNOOZE_CRITERIA));

        mNotificationManager.disallowAssistantAdjustment(Adjustment.KEY_SNOOZE_CRITERIA);
        Thread.sleep(SLEEP_TIME);
        assertFalse(mNotificationAssistantService.currentCapabilities.contains(
                Adjustment.KEY_SNOOZE_CRITERIA));

        // just in case KEY_SNOOZE_CRITERIA was included in the original set, test adding again
        mNotificationManager.allowAssistantAdjustment(Adjustment.KEY_SNOOZE_CRITERIA);
        Thread.sleep(SLEEP_TIME);
        assertTrue(mNotificationAssistantService.currentCapabilities.contains(
                Adjustment.KEY_SNOOZE_CRITERIA));

        mUi.dropShellPermissionIdentity();
    }

    @Test
    public void testOnNotificationSnoozedUntilContext() throws Exception {
        final String snoozeContext = "@SnoozeContext1@";

        setUpListeners(); // also enables assistant

        sendNotification(1001, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);

        // simulate the user snoozing the notification
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        runCommand(String.format("cmd notification snooze --condition %s %s", snoozeContext,
                sbn.getKey()), instrumentation);

        Thread.sleep(SLEEP_TIME);

        assertTrue(String.format("snoozed notification <%s> was not removed", sbn.getKey()),
                mNotificationListenerService.checkRemovedKey(sbn.getKey()));

        assertEquals(String.format("snoozed notification <%s> was not observed by NAS", sbn.getKey()),
                sbn.getKey(), mNotificationAssistantService.snoozedKey);
        assertEquals(snoozeContext, mNotificationAssistantService.snoozedUntilContext);
    }

    @Test
    public void testUnsnoozeFromNAS() throws Exception {
        final String snoozeContext = "@SnoozeContext2@";

        setUpListeners(); // also enables assistant

        sendNotification(1002, ICON_ID);
        StatusBarNotification sbn = getFirstNotificationFromPackage(TestNotificationListener.PKG);

        // simulate the user snoozing the notification
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        runCommand(String.format("cmd notification snooze --context %s %s", snoozeContext,
            sbn.getKey()), instrumentation);

        Thread.sleep(SLEEP_TIME);

        // unsnooze from listener
        mNotificationAssistantService = TestNotificationAssistant.getInstance();
        android.util.Log.v(TAG, "unsnoozing from listener: " + sbn.getKey());
        mNotificationAssistantService.unsnoozeNotification(sbn.getKey());

        Thread.sleep(SLEEP_TIME);

        NotificationListenerService.Ranking out = new NotificationListenerService.Ranking();
        boolean found = mNotificationListenerService.mRankingMap.getRanking(sbn.getKey(), out);
        assertTrue("notification <" + sbn.getKey()
                + "> was not restored when unsnoozed from listener",
                found);
    }

    @Test
    public void testOnActionInvoked_methodExists() throws Exception {
        setUpListeners();
        final Intent intent = new Intent(Intent.ACTION_MAIN, Telephony.Threads.CONTENT_URI);

        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP
                | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intent.setAction(Intent.ACTION_MAIN);

        final PendingIntent pendingIntent = PendingIntent.getActivity(mContext, 0, intent, 0);
        Notification.Action action = new Notification.Action.Builder(null, "",
                pendingIntent).build();
        // This method has to exist and the call cannot fail
        mNotificationAssistantService.onActionInvoked("", action,
                NotificationAssistantService.SOURCE_FROM_APP);
    }

    @Test
    public void testOnNotificationDirectReplied_methodExists() throws Exception {
        setUpListeners();
        // This method has to exist and the call cannot fail
        mNotificationAssistantService.onNotificationDirectReplied("");
    }

    @Test
    public void testOnNotificationExpansionChanged_methodExists() throws Exception {
        setUpListeners();
        // This method has to exist and the call cannot fail
        mNotificationAssistantService.onNotificationExpansionChanged("", true, true);
    }

    @Test
    public void testOnNotificationVisibilityChanged() throws Exception {
        if (isTelevision()) {
            return;
        }
        setUpListeners();
        turnScreenOn();
        mUi.adoptShellPermissionIdentity("android.permission.EXPAND_STATUS_BAR");

        mNotificationAssistantService.resetNotificationVisibilityCounts();

        // Initialize as closed
        mStatusBarManager.collapsePanels();

        sendNotification(1, ICON_ID);
        assertEquals(0, mNotificationAssistantService.notificationVisibleCount);
        assertEquals(0, mNotificationAssistantService.notificationHiddenCount);

        mStatusBarManager.expandNotificationsPanel();
        Thread.sleep(SLEEP_TIME * 2);
        assertTrue(mNotificationAssistantService.notificationVisibleCount > 0);
        assertEquals(0, mNotificationAssistantService.notificationHiddenCount);

        mStatusBarManager.collapsePanels();
        Thread.sleep(SLEEP_TIME * 2);
        assertTrue(mNotificationAssistantService.notificationVisibleCount > 0);
        assertTrue(mNotificationAssistantService.notificationHiddenCount > 0);

        mUi.dropShellPermissionIdentity();
    }

    @Test
    public void testOnNotificationsSeen() throws Exception {
        if (isTelevision()) {
            return;
        }
        setUpListeners();
        turnScreenOn();
        mUi.adoptShellPermissionIdentity("android.permission.EXPAND_STATUS_BAR");

        mNotificationAssistantService.resetNotificationVisibilityCounts();

        // Initialize as closed
        mStatusBarManager.collapsePanels();

        sendNotification(1, ICON_ID);
        assertEquals(0, mNotificationAssistantService.notificationSeenCount);

        mStatusBarManager.expandNotificationsPanel();
        Thread.sleep(SLEEP_TIME * 2);
        assertTrue(mNotificationAssistantService.notificationSeenCount > 0);

        mStatusBarManager.collapsePanels();
        mUi.dropShellPermissionIdentity();
    }

    @Test
    public void testOnPanelRevealedAndHidden() throws Exception {
        if (isTelevision()) {
            return;
        }
        setUpListeners();
        turnScreenOn();
        mUi.adoptShellPermissionIdentity("android.permission.EXPAND_STATUS_BAR");

        // Initialize as closed
        mStatusBarManager.collapsePanels();
        assertFalse(mNotificationAssistantService.isPanelOpen);

        mStatusBarManager.expandNotificationsPanel();
        Thread.sleep(SLEEP_TIME * 2);
        assertTrue(mNotificationAssistantService.isPanelOpen);

        mStatusBarManager.collapsePanels();
        Thread.sleep(SLEEP_TIME * 2);
        assertFalse(mNotificationAssistantService.isPanelOpen);

        mUi.dropShellPermissionIdentity();
    }

    @Test
    public void testOnSuggestedReplySent_methodExists() throws Exception {
        setUpListeners();
        // This method has to exist and the call cannot fail
        mNotificationAssistantService.onSuggestedReplySent("", "",
                NotificationAssistantService.SOURCE_FROM_APP);
    }

    private StatusBarNotification getFirstNotificationFromPackage(String PKG)
            throws InterruptedException {
        StatusBarNotification sbn = mNotificationListenerService.mPosted.poll(SLEEP_TIME,
                TimeUnit.MILLISECONDS);
        assertNotNull(sbn);
        while (!sbn.getPackageName().equals(PKG)) {
            sbn = mNotificationListenerService.mPosted.poll(SLEEP_TIME, TimeUnit.MILLISECONDS);
        }
        assertNotNull(sbn);
        return sbn;
    }

    private void setUpListeners() throws Exception {
        toggleListenerAccess(true);
        toggleAssistantAccess(true);
        Thread.sleep(2 * SLEEP_TIME); // wait for listener and assistant to be allowed

        mNotificationListenerService = TestNotificationListener.getInstance();
        mNotificationAssistantService = TestNotificationAssistant.getInstance();

        assertNotNull(mNotificationListenerService);
        assertNotNull(mNotificationAssistantService);
    }

    private void sendNotification(final int id, final int icon) throws Exception {
        sendNotification(id, null, icon);
    }

    private void sendNotification(final int id, String groupKey, final int icon) throws Exception {
        final Intent intent = new Intent(Intent.ACTION_MAIN, Telephony.Threads.CONTENT_URI);

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

    private void turnScreenOn() throws IOException {
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        runCommand("input keyevent KEYCODE_WAKEUP", instrumentation);
        runCommand("wm dismiss-keyguard", instrumentation);
    }

    private boolean isTelevision() {
        PackageManager packageManager = mContext.getPackageManager();
        return packageManager != null
                && (packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK)
                || packageManager.hasSystemFeature(PackageManager.FEATURE_TELEVISION));
    }

    private void toggleListenerAccess(boolean on) throws IOException {

        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        String componentName = TestNotificationListener.getId();

        String command = " cmd notification " + (on ? "allow_listener " : "disallow_listener ")
                + componentName;

        runCommand(command, instrumentation);

        final ComponentName listenerComponent = TestNotificationListener.getComponentName();
        final NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        Assert.assertTrue(listenerComponent + " has not been " + (on ? "allowed" : "disallowed"),
                nm.isNotificationListenerAccessGranted(listenerComponent) == on);
    }

    private void toggleAssistantAccess(boolean on) {
        final ComponentName assistantComponent = TestNotificationAssistant.getComponentName();

        mUi.adoptShellPermissionIdentity("android.permission.STATUS_BAR_SERVICE",
                "android.permission.REQUEST_NOTIFICATION_ASSISTANT_SERVICE");
        mNotificationManager.setNotificationAssistantAccessGranted(assistantComponent, on);

        assertTrue(assistantComponent + " has not been " + (on ? "allowed" : "disallowed"),
                mNotificationManager.isNotificationAssistantAccessGranted(assistantComponent)
                        == on);
        if (on) {
            assertEquals(assistantComponent,
                    mNotificationManager.getAllowedNotificationAssistant());
        } else {
            assertNotEquals(assistantComponent,
                    mNotificationManager.getAllowedNotificationAssistant());
        }

        mUi.dropShellPermissionIdentity();
    }

    private void runCommand(String command, Instrumentation instrumentation) throws IOException {
        UiAutomation uiAutomation = instrumentation.getUiAutomation();
        // Execute command
        System.out.println("runCommand: <<<" + command + ">>>");
        try (ParcelFileDescriptor fd = uiAutomation.executeShellCommand(command)) {
            assertNotNull("Failed to execute shell command: " + command, fd);
            // Wait for the command to finish by reading until EOF
            try (BufferedReader in = new BufferedReader(new FileReader(fd.getFileDescriptor()))) {
                String line;
                while (null != (line = in.readLine())) {
                    android.util.Log.v(TAG, "runCommand: output: " + line);
                }
            } catch (IOException e) {
                throw new IOException("Could not read stdout of command: " + command, e);
            }
        }
    }
}