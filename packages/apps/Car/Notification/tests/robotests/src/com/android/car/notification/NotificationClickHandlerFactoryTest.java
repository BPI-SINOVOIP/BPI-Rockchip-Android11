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
 * limitations under the License
 */

package com.android.car.notification;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.os.RemoteException;
import android.os.UserHandle;
import android.service.notification.NotificationStats;
import android.service.notification.StatusBarNotification;
import android.view.View;
import android.widget.Button;

import androidx.test.core.app.ApplicationProvider;

import com.android.car.notification.testutils.ShadowCarAssistUtils;
import com.android.internal.statusbar.IStatusBarService;
import com.android.internal.statusbar.NotificationVisibility;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.ArrayList;
import java.util.List;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowCarAssistUtils.class})
// TODO(b/149339447): Resolve failure and re-enable.
@Ignore
public class NotificationClickHandlerFactoryTest {

    private Context mContext;
    private NotificationClickHandlerFactory mNotificationClickHandlerFactory;
    private AlertEntry mAlertEntry1;
    private AlertEntry mAlertEntry2;

    @Mock
    private IStatusBarService mBarService;
    @Mock
    private NotificationClickHandlerFactory.OnNotificationClickListener mListener1;
    @Mock
    private NotificationClickHandlerFactory.OnNotificationClickListener mListener2;
    @Mock
    private View mView;
    @Mock
    private StatusBarNotification mStatusBarNotification1;
    @Mock
    private StatusBarNotification mStatusBarNotification2;
    @Mock
    private UserHandle mUser1;
    @Mock
    private UserHandle mUser2;
    @Mock
    private PendingIntent mActionIntent;
    @Mock
    private RemoteInput mRemoteInput;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mNotificationClickHandlerFactory = new NotificationClickHandlerFactory(mBarService);
        Notification notification1 = new Notification();
        Notification notification2 = new Notification();
        notification1.contentIntent = PendingIntent.getForegroundService(
                mContext, /* requestCode= */ 0, new Intent(), /* flags= */ 0);
        notification2.contentIntent = PendingIntent.getForegroundService(
                mContext, /* requestCode= */ 0, new Intent(), /* flags= */ 0);
        when(mStatusBarNotification1.getNotification()).thenReturn(notification1);
        when(mStatusBarNotification2.getNotification()).thenReturn(notification2);
        when(mStatusBarNotification1.getKey()).thenReturn("TEST_KEY_1");
        when(mStatusBarNotification2.getKey()).thenReturn("TEST_KEY_2");
        when(mStatusBarNotification1.getUser()).thenReturn(mUser1);
        when(mStatusBarNotification2.getUser()).thenReturn(mUser2);
        mAlertEntry1 = new AlertEntry(mStatusBarNotification1);
        mAlertEntry2 = new AlertEntry(mStatusBarNotification2);
        when(mView.getContext()).thenReturn(mContext);
    }

    @After
    public void tearDown() {
        ShadowCarAssistUtils.reset();
    }

    @Test
    public void onClickClickHandler_noIntent_returnsImmediately() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        // overriding the non-null value assigned in setup.
        mAlertEntry1.getNotification().contentIntent = null;
        mAlertEntry1.getNotification().fullScreenIntent = null;

        mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1).onClick(mView);

        verify(mListener1, never()).onNotificationClicked(anyInt(), any(AlertEntry.class));
        try {
            verify(mBarService, never())
                    .onNotificationClick(anyString(), any(NotificationVisibility.class));
        } catch (RemoteException ex) {
            // ignore
        }
    }

    @Test
    public void onClickClickHandler_intentExists_callsBarServiceNotificationClicked() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);

        mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1).onClick(mView);

        try {
            verify(mBarService)
                    .onNotificationClick(anyString(), any(NotificationVisibility.class));
        } catch (RemoteException ex) {
            // ignore
        }
    }

    @Test
    public void onClickClickHandler_intentExists_invokesRegisteredClickListeners() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);

        mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1).onClick(mView);

        verify(mListener1).onNotificationClicked(anyInt(), any(AlertEntry.class));
    }

    @Test
    public void onClickClickHandler_shouldAutoCancel_callsBarServiceOnNotificationClear() {
        mAlertEntry1.getStatusBarNotification().getNotification().flags =
                mStatusBarNotification1.getNotification().flags | Notification.FLAG_AUTO_CANCEL;
        UserHandle user = new UserHandle( /* handle= */ 0);
        when(mStatusBarNotification1.getUser()).thenReturn(user);
        mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1).onClick(mView);
        NotificationVisibility notificationVisibility = NotificationVisibility.obtain(
                mAlertEntry1.getKey(), /* rank= */ -1, /* count= */ -1, /* visible= */ true);

        try {
            verify(mBarService).onNotificationClear(
                    mAlertEntry1.getStatusBarNotification().getPackageName(),
                    mAlertEntry1.getStatusBarNotification().getTag(),
                    mAlertEntry1.getStatusBarNotification().getId(),
                    mAlertEntry1.getStatusBarNotification().getUser().getIdentifier(),
                    mAlertEntry1.getKey(),
                    NotificationStats.DISMISSAL_SHADE,
                    NotificationStats.DISMISS_SENTIMENT_NEUTRAL,
                    notificationVisibility);
        } catch (RemoteException ex) {
            // ignore
        }

    }

    @Test
    public void onClickActionClickHandler_isReplyAction_sendsPendingIntent() {
        ShadowCarAssistUtils.addMessageNotification(mStatusBarNotification1.getKey());
        PendingIntent pendingIntent = PendingIntent.getForegroundService(
                mContext, /* requestCode= */ 0,
                new Intent(), /* flags= */ 0);
        Notification.Action action = new Notification.Action
                .Builder(/* icon= */ null, "ACTION_TITLE", pendingIntent)
                .setSemanticAction(Notification.Action.SEMANTIC_ACTION_REPLY)
                .addRemoteInput(mRemoteInput)
                .build();
        action.actionIntent = mActionIntent;
        Notification.Action[] actions = {action};
        mStatusBarNotification1.getNotification().actions = actions;

        mNotificationClickHandlerFactory.getActionClickHandler(mAlertEntry1, /* index= */
                0).onClick(mView);
        try {
            verify(mActionIntent).sendAndReturnResult(any(Context.class), eq(0), any(Intent.class),
                    isNull(), isNull(), isNull(), isNull());
        } catch (PendingIntent.CanceledException e) {
            // ignore
        }
    }

    @Test
    public void onClickActionClickHandler_isReplyAction_doesNotInvokeRegisteredClickListeners() {
        ShadowCarAssistUtils.addMessageNotification(mStatusBarNotification1.getKey());
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        PendingIntent pendingIntent = PendingIntent.getForegroundService(
                mContext, /* requestCode= */ 0,
                new Intent(), /* flags= */ 0);
        Notification.Action action = new Notification.Action
                .Builder(/* icon= */ null, "ACTION_TITLE", pendingIntent)
                .setSemanticAction(Notification.Action.SEMANTIC_ACTION_REPLY)
                .addRemoteInput(mRemoteInput)
                .build();
        action.actionIntent = mActionIntent;
        Notification.Action[] actions = {action};
        mStatusBarNotification1.getNotification().actions = actions;

        mNotificationClickHandlerFactory.getActionClickHandler(mAlertEntry1, /* index= */
                0).onClick(mView);

        verify(mListener1, never()).onNotificationClicked(anyInt(), any(AlertEntry.class));
    }

    @Test
    public void onClickActionClickHandler_notCarCompatibleMessage_sendsPendingIntent() {
        PendingIntent pendingIntent = PendingIntent.getForegroundService(
                mContext, /* requestCode= */0,
                new Intent(), /* flags= */ 0);
        Notification.Action action = new Notification.Action
                .Builder(/* icon= */ null, "ACTION_TITLE", pendingIntent)
                .setSemanticAction(Notification.Action.SEMANTIC_ACTION_REPLY)
                .addRemoteInput(mRemoteInput)
                .build();
        action.actionIntent = mActionIntent;
        Notification.Action[] actions = {action};
        mStatusBarNotification1.getNotification().actions = actions;

        mNotificationClickHandlerFactory.getActionClickHandler(mAlertEntry1, /* index= */
                0).onClick(mView);
        try {
            verify(mActionIntent).sendAndReturnResult(isNull(), eq(0), isNull(), isNull(),
                    isNull(), isNull(), isNull());
        } catch (PendingIntent.CanceledException e) {
            // ignore
        }
    }

    @Test
    public void onClickActionClickHandler_notCarCompatibleMessage_invokesRegisteredListeners() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        PendingIntent pendingIntent = PendingIntent.getForegroundService(
                mContext, /* requestCode= */0,
                new Intent(), /* flags= */ 0);
        Notification.Action action = new Notification.Action
                .Builder(/* icon= */ null, "ACTION_TITLE", pendingIntent)
                .setSemanticAction(Notification.Action.SEMANTIC_ACTION_REPLY)
                .addRemoteInput(mRemoteInput)
                .build();
        action.actionIntent = mActionIntent;
        Notification.Action[] actions = {action};
        mStatusBarNotification1.getNotification().actions = actions;

        mNotificationClickHandlerFactory.getActionClickHandler(mAlertEntry1, /* index= */
                0).onClick(mView);

        verify(mListener1).onNotificationClicked(anyInt(), any(AlertEntry.class));
    }

    @Test
    public void onClickPlayClickHandler_notCarCompatibleMessage_returnsImmediately() {
        mNotificationClickHandlerFactory.getPlayClickHandler(mAlertEntry1).onClick(mView);

        assertThat(ShadowCarAssistUtils.getRequestAssistantVoiceActionCount()).isEqualTo(0);
    }

    @Test
    public void onClickPlayClickHandler_isCarCompatibleMessage_requestsAssistantVoiceAction() {
        ShadowCarAssistUtils.addMessageNotification(mStatusBarNotification1.getKey());

        mNotificationClickHandlerFactory.getPlayClickHandler(mAlertEntry1).onClick(mView);

        assertThat(ShadowCarAssistUtils.getRequestAssistantVoiceActionCount()).isEqualTo(1);
    }

    @Test
    public void onClickMuteClickHandler_togglesMute() {
        ShadowCarAssistUtils.addMessageNotification(mStatusBarNotification1.getKey());
        NotificationDataManager notificationDataManager = new NotificationDataManager();
        notificationDataManager.addNewMessageNotification(mAlertEntry1);
        mNotificationClickHandlerFactory.setNotificationDataManager(notificationDataManager);
        Button button = new Button(mContext);

        // first make sure it is not muted by default
        assertThat(notificationDataManager.isMessageNotificationMuted(mAlertEntry1)).isFalse();

        mNotificationClickHandlerFactory.getMuteClickHandler(button, mAlertEntry1).onClick(mView);

        assertThat(notificationDataManager.isMessageNotificationMuted(mAlertEntry1)).isTrue();

        mNotificationClickHandlerFactory.getMuteClickHandler(button, mAlertEntry1).onClick(mView);

        assertThat(notificationDataManager.isMessageNotificationMuted(mAlertEntry1)).isFalse();
    }

    @Test
    public void onClickMuteClickHandler_isMuted_showsUnmuteLabel() {
        ShadowCarAssistUtils.addMessageNotification(mStatusBarNotification1.getKey());
        NotificationDataManager notificationDataManager = new NotificationDataManager();
        notificationDataManager.addNewMessageNotification(mAlertEntry1);
        mNotificationClickHandlerFactory.setNotificationDataManager(notificationDataManager);
        Button button = new Button(mContext);

        // first make sure it is not muted by default
        assertThat(notificationDataManager.isMessageNotificationMuted(mAlertEntry1)).isFalse();

        mNotificationClickHandlerFactory.getMuteClickHandler(button, mAlertEntry1).onClick(mView);

        assertThat(button.getText()).isEqualTo(mContext.getString(R.string.action_unmute_long));
    }

    @Test
    public void onClickMuteClickHandler_isUnmuted_showsMuteLabel() {
        ShadowCarAssistUtils.addMessageNotification(mStatusBarNotification1.getKey());
        NotificationDataManager notificationDataManager = new NotificationDataManager();
        notificationDataManager.addNewMessageNotification(mAlertEntry1);
        mNotificationClickHandlerFactory.setNotificationDataManager(notificationDataManager);
        Button button = new Button(mContext);

        // first make sure it is not muted by default
        assertThat(notificationDataManager.isMessageNotificationMuted(mAlertEntry1)).isFalse();

        mNotificationClickHandlerFactory.getMuteClickHandler(button, mAlertEntry1).onClick(mView);
        mNotificationClickHandlerFactory.getMuteClickHandler(button, mAlertEntry1).onClick(mView);

        assertThat(button.getText()).isEqualTo(mContext.getString(R.string.action_mute_long));
    }

    @Test
    public void onNotificationClicked_twoListenersRegistered_invokesTwoHandlerCallbacks() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        mNotificationClickHandlerFactory.registerClickListener(mListener2);
        mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1).onClick(mView);

        verify(mListener1).onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry1);
        verify(mListener2).onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry1);
    }

    @Test
    public void onNotificationClicked_oneListenersUnregistered_invokesRegisteredCallback() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        mNotificationClickHandlerFactory.registerClickListener(mListener2);
        mNotificationClickHandlerFactory.unregisterClickListener(mListener2);

        mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1).onClick(mView);

        verify(mListener1).onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry1);
        verify(mListener2, never()).onNotificationClicked(ActivityManager.START_SUCCESS,
                mAlertEntry1);
    }

    @Test
    public void onNotificationClicked_duplicateListenersRegistered_invokesCallbackOnce() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1).onClick(mView);

        verify(mListener1).onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry1);
    }

    @Test
    public void onNotificationClicked_twoAlertEntriesSubscribe_passesTheRightAlertEntry() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        View.OnClickListener clickListener1 =
                mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1);
        View.OnClickListener clickListener2 =
                mNotificationClickHandlerFactory.getClickHandler(mAlertEntry2);

        clickListener2.onClick(mView);

        verify(mListener1, never())
                .onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry1);
        verify(mListener1).onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry2);
    }

    @Test
    public void onNotificationClicked_multipleListenersAndAes_passesTheRightAeToAllListeners() {
        mNotificationClickHandlerFactory.registerClickListener(mListener1);
        mNotificationClickHandlerFactory.registerClickListener(mListener2);
        View.OnClickListener clickListener1 =
                mNotificationClickHandlerFactory.getClickHandler(mAlertEntry1);
        View.OnClickListener clickListener2 =
                mNotificationClickHandlerFactory.getClickHandler(mAlertEntry2);

        clickListener2.onClick(mView);

        verify(mListener1, never())
                .onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry1);
        verify(mListener1).onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry2);

        verify(mListener2, never())
                .onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry1);
        verify(mListener2).onNotificationClicked(ActivityManager.START_SUCCESS, mAlertEntry2);
    }

    @Test
    public void onClearNotifications_groupNotificationWithSummary_clearsSummary() {
        List<NotificationGroup> notificationsToClear = new ArrayList<>();
        NotificationGroup notificationGroup = new NotificationGroup();
        notificationGroup.setGroupSummaryNotification(mAlertEntry1);
        notificationGroup.addNotification(mAlertEntry2);
        notificationGroup.addNotification(mAlertEntry2);
        notificationGroup.addNotification(mAlertEntry2);
        notificationsToClear.add(notificationGroup);
        NotificationVisibility notificationVisibility = NotificationVisibility.obtain(
                mAlertEntry1.getKey(),
                /* rank= */ -1,
                /* count= */ -1,
                /* visible= */ true);

        mNotificationClickHandlerFactory.clearNotifications(notificationsToClear);

        try {
            verify(mBarService).onNotificationClear(
                    mAlertEntry1.getStatusBarNotification().getPackageName(),
                    mAlertEntry1.getStatusBarNotification().getTag(),
                    mAlertEntry1.getStatusBarNotification().getId(),
                    mAlertEntry1.getStatusBarNotification().getUser().getIdentifier(),
                    mAlertEntry1.getStatusBarNotification().getKey(),
                    NotificationStats.DISMISSAL_SHADE,
                    NotificationStats.DISMISS_SENTIMENT_NEUTRAL,
                    notificationVisibility);
        } catch (RemoteException e) {
            // ignore.
        }
    }

    @Test
    public void onClearNotifications_groupNotificationWithSummary_clearsAllChildren() {
        List<NotificationGroup> notificationsToClear = new ArrayList<>();
        NotificationGroup notificationGroup = new NotificationGroup();
        notificationGroup.setGroupSummaryNotification(mAlertEntry1);
        notificationGroup.addNotification(mAlertEntry2);
        notificationGroup.addNotification(mAlertEntry2);
        notificationGroup.addNotification(mAlertEntry2);
        notificationsToClear.add(notificationGroup);
        NotificationVisibility notificationVisibility = NotificationVisibility.obtain(
                mAlertEntry2.getKey(),
                /* rank= */ -1,
                /* count= */ -1,
                /* visible= */ true);

        mNotificationClickHandlerFactory.clearNotifications(notificationsToClear);

        try {
            verify(mBarService, times(3)).onNotificationClear(
                    mAlertEntry2.getStatusBarNotification().getPackageName(),
                    mAlertEntry2.getStatusBarNotification().getTag(),
                    mAlertEntry2.getStatusBarNotification().getId(),
                    mAlertEntry2.getStatusBarNotification().getUser().getIdentifier(),
                    mAlertEntry2.getStatusBarNotification().getKey(),
                    NotificationStats.DISMISSAL_SHADE,
                    NotificationStats.DISMISS_SENTIMENT_NEUTRAL,
                    notificationVisibility);
        } catch (RemoteException e) {
            // ignore.
        }
    }

    @Test
    public void onClearNotifications_groupNotificationWithoutSummary_clearsAllChildren() {
        List<NotificationGroup> notificationsToClear = new ArrayList<>();
        NotificationGroup notificationGroup = new NotificationGroup();
        notificationGroup.addNotification(mAlertEntry2);
        notificationGroup.addNotification(mAlertEntry2);
        notificationGroup.addNotification(mAlertEntry2);
        notificationsToClear.add(notificationGroup);
        NotificationVisibility notificationVisibility = NotificationVisibility.obtain(
                mAlertEntry2.getKey(),
                /* rank= */ -1,
                /* count= */ -1,
                /* visible= */ true);

        mNotificationClickHandlerFactory.clearNotifications(notificationsToClear);

        try {
            verify(mBarService, times(3)).onNotificationClear(
                    mAlertEntry2.getStatusBarNotification().getPackageName(),
                    mAlertEntry2.getStatusBarNotification().getTag(),
                    mAlertEntry2.getStatusBarNotification().getId(),
                    mAlertEntry2.getStatusBarNotification().getUser().getIdentifier(),
                    mAlertEntry2.getStatusBarNotification().getKey(),
                    NotificationStats.DISMISSAL_SHADE,
                    NotificationStats.DISMISS_SENTIMENT_NEUTRAL,
                    notificationVisibility);
        } catch (RemoteException e) {
            // ignore.
        }
    }
}
