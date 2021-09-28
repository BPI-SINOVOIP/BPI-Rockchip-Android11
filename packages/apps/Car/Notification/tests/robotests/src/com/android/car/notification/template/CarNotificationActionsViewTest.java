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

package com.android.car.notification.template;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.service.notification.StatusBarNotification;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;

import androidx.test.core.app.ApplicationProvider;

import com.android.car.notification.AlertEntry;
import com.android.car.notification.NotificationClickHandlerFactory;
import com.android.car.notification.NotificationDataManager;
import com.android.car.notification.R;
import com.android.car.notification.testutils.ShadowCarAssistUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowCarAssistUtils.class})
public class CarNotificationActionsViewTest {

    private static final String TEST_KEY = "TEST_KEY";
    private static final String ACTION_TITLE = "ACTION_TITLE";
    private static final View.OnClickListener CLICK_LISTENER = (v) -> {
    };

    private CarNotificationActionsView mCarNotificationActionsView;
    private Notification.Action mAction;
    private PendingIntent mPendingIntent;
    private Context mContext;

    @Mock
    private StatusBarNotification mStatusBarNotification;
    @Mock
    private NotificationClickHandlerFactory mNotificationClickHandlerFactory;
    @Mock
    private NotificationDataManager mNotificationDataManager;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mPendingIntent =  PendingIntent.getForegroundService(mContext, /* requestCode= */0,
                new Intent(), /* flags= */ 0);
        mAction = new Notification.Action
                .Builder(/* icon= */ null, ACTION_TITLE , mPendingIntent).build();
        when(mStatusBarNotification.getKey()).thenReturn(TEST_KEY);
        when(mNotificationClickHandlerFactory
                .getPlayClickHandler(any(AlertEntry.class)))
                .thenReturn (CLICK_LISTENER);
        when(mNotificationClickHandlerFactory
                .getMuteClickHandler(any(Button.class), any(AlertEntry.class)))
                .thenReturn (CLICK_LISTENER);
        when(mNotificationClickHandlerFactory
                .getActionClickHandler(any(AlertEntry.class), anyInt()))
                .thenReturn(CLICK_LISTENER);
    }

    @After
    public void tearDown() {
        ShadowCarAssistUtils.reset();
    }

    @Test
    public void onFinishInflate_addsMaxNumberOfActionButtons() {
        finishInflateWithIsCall(/* isCall= */ false);

        assertThat(mCarNotificationActionsView.getActionButtons().size())
                .isEqualTo(mCarNotificationActionsView.MAX_NUM_ACTIONS);
    }

    @Test
    public void onBind_noAction_doesNotCreateButtons() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);

        for (int i = 0; i < mCarNotificationActionsView.getActionButtons().size(); i++) {
            Button button = mCarNotificationActionsView.getActionButtons().get(i);
            assertThat(button.getVisibility()).isNotEqualTo(View.VISIBLE);
            assertThat(button.hasOnClickListeners()).isFalse();
        }
    }

    @Test
    public void onBind_actionExists_isCarCompatibleMessage_playButtonIsVisible() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button playButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.FIRST_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(playButton.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void onBind_actionExists_isCarCompatibleMessage_playButtonHasClickListener() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button playButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.FIRST_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(playButton.hasOnClickListeners()).isTrue();
    }

    @Test
    public void onBind_actionExists_isCarCompatibleMessage_playButtonShowsPlayLabel() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button playButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.FIRST_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(playButton.getText())
                .isEqualTo(mContext.getString(R.string.assist_action_play_label));
    }

    @Test
    public void onBind_carCompatibleMessage_noAssistantNoFallback_playButtonIsHidden() {
        ShadowCarAssistUtils.setHasActiveAssistant(false);

        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button playButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.FIRST_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(playButton.getVisibility()).isNotEqualTo(View.VISIBLE);
    }

    @Test
    public void onBind_carCompatibleMessage_noAssistantWithFallback_playButtonIsVisible() {
        ShadowCarAssistUtils.setHasActiveAssistant(false);
        ShadowCarAssistUtils.setIsFallbackAssistantEnabled(true);

        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button playButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.FIRST_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(playButton.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(playButton.getText())
                .isEqualTo(mContext.getString(R.string.assist_action_play_label));
        assertThat(playButton.hasOnClickListeners()).isTrue();
    }

    @Test
    public void onBind_actionExists_isCarCompatibleMessage_muteButtonIsVisible() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button muteButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.SECOND_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(muteButton.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void onBind_actionExists_isCarCompatibleMessage_muteButtonHasClickListener() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button muteButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.SECOND_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(muteButton.hasOnClickListeners()).isTrue();
    }

    @Test
    public void onBind_actionExists_isCarCompatibleMessage_muted_muteButtonShowsUnmuteLabel() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);
        messageIsMuted(true);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button muteButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.SECOND_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(muteButton.getText())
                .isEqualTo(mContext.getString(R.string.action_unmute_long));
    }

    @Test
    public void onBind_actionExists_isCarCompatibleMessage_unmuted_muteButtonShowsMuteLabel() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ true);
        messageIsMuted(false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button muteButton = mCarNotificationActionsView.getActionButtons().get(
                CarNotificationActionsView.SECOND_MESSAGE_ACTION_BUTTON_INDEX);

        assertThat(muteButton.getText())
                .isEqualTo(mContext.getString(R.string.action_mute_long));
    }

    @Test
    public void onBind_actionExists_notCarCompatibleMessage_buttonIsVisible() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button button = mCarNotificationActionsView.getActionButtons().get(0);

        assertThat(button.getVisibility()).isEqualTo(View.VISIBLE);
    }

    @Test
    public void onBind_actionExists_notCarCompatibleMessage_buttonShowsActionTitle() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button button = mCarNotificationActionsView.getActionButtons().get(0);

        assertThat(button.getText()).isEqualTo(ACTION_TITLE);
    }

    @Test
    public void onBind_actionExists_notCarCompatibleMessage_hasIntent_buttonHasClickListener() {
        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button button = mCarNotificationActionsView.getActionButtons().get(0);

        assertThat(button.hasOnClickListeners()).isTrue();
    }

    @Test
    public void onBind_actionExists_notCarCompatibleMessage_hasNoIntent_buttonHasNoClickListener() {
        // Override mAction with an Action without a pending intent.
        mAction = new Notification.Action
                .Builder(/* icon= */ null, ACTION_TITLE , /* pendingIntent= */ null)
                .build();

        finishInflateWithIsCall(/* isCall= */ false);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button button = mCarNotificationActionsView.getActionButtons().get(0);

        assertThat(button.hasOnClickListeners()).isFalse();
    }

    @Test
    public void onBind_actionCountExceedsMaximum_notCarCompatibleMessage_doesNotThrowError() {
        finishInflateWithIsCall(/* isCall= */ false);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ false);

        int numberOverMaximum = mCarNotificationActionsView.MAX_NUM_ACTIONS + 10;
        Notification.Action[] actions = new Notification.Action[numberOverMaximum];
        for (int i = 0; i < actions.length; i++) {
            actions[i] = mAction;
        }
        Notification notification = new Notification();
        notification.actions = actions;
        when(mStatusBarNotification.getNotification()).thenReturn(notification);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
    }

    @Test
    public void onBind_actionExists_notCarCompatibleMessage_isCall_firstButtonHasBackground() {
        finishInflateWithIsCall(/* isCall= */ true);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button firstButton = mCarNotificationActionsView.getActionButtons().get(0);

        assertThat(firstButton.getBackground()).isNotNull();
    }

    @Test
    public void onBind_actionExists_notCarCompatibleMessage_isCall_secondButtonHasBackground() {
        finishInflateWithIsCall(/* isCall= */ true);
        statusBarNotificationHasActions(/* hasActions= */ true);
        notificationIsCarCompatibleMessage(/* isCarCompatibleMessage= */ false);

        AlertEntry alertEntry = new AlertEntry(mStatusBarNotification);
        mCarNotificationActionsView.bind(mNotificationClickHandlerFactory, alertEntry);
        Button secondButton = mCarNotificationActionsView.getActionButtons().get(1);

        assertThat(secondButton.getBackground()).isNotNull();
    }

    private void finishInflateWithIsCall(boolean isCall) {
        AttributeSet attributeSet = null;
        mCarNotificationActionsView = new CarNotificationActionsView(mContext, attributeSet);
        mCarNotificationActionsView.setCategoryIsCall(isCall);
        mCarNotificationActionsView.onFinishInflate();
    }

    private void statusBarNotificationHasActions(boolean hasActions) {
        Notification notification = new Notification();
        Notification.Action[] actions = {mAction};
        notification.actions = hasActions ? actions : null;
        when(mStatusBarNotification.getNotification()).thenReturn(notification);
    }

    private void notificationIsCarCompatibleMessage(boolean isCarCompatibleMessage) {
        if (isCarCompatibleMessage) {
            ShadowCarAssistUtils.addMessageNotification(mStatusBarNotification.getKey());
        }
    }

    private void messageIsMuted(boolean isMuted) {
        when(mNotificationDataManager.isMessageNotificationMuted(any(AlertEntry.class)))
                .thenReturn(isMuted);
        when(mNotificationClickHandlerFactory.getNotificationDataManager())
                .thenReturn(mNotificationDataManager);
    }
}
