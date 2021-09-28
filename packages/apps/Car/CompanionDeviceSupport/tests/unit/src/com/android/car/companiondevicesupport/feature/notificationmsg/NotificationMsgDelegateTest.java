/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.companiondevicesupport.feature.notificationmsg;


import static android.app.NotificationManager.IMPORTANCE_HIGH;
import static android.app.NotificationManager.IMPORTANCE_LOW;

import static androidx.core.app.NotificationCompat.CATEGORY_MESSAGE;

import static com.android.car.messenger.NotificationMsgProto.NotificationMsg.Action.ActionName.DISMISS;
import static com.android.car.messenger.NotificationMsgProto.NotificationMsg.Action.ActionName.MARK_AS_READ;
import static com.android.car.messenger.NotificationMsgProto.NotificationMsg.Action.ActionName.REPLY;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Icon;

import androidx.core.app.NotificationCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.Action;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.AvatarIconSync;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.CarToPhoneMessage;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.ClearAppDataRequest;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.ConversationNotification;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.MessagingStyle;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.MessagingStyleMessage;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.Person;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.PhoneMetadata;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg.PhoneToCarMessage;
import com.android.car.messenger.common.ConversationKey;
import com.android.car.messenger.common.ProjectionStateListener;
import com.android.car.messenger.common.SenderKey;
import com.android.car.protobuf.ByteString;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;

@RunWith(AndroidJUnit4.class)
public class NotificationMsgDelegateTest {
    private static final String NOTIFICATION_KEY_1 = "notification_key_1";
    private static final String NOTIFICATION_KEY_2 = "notification_key_2";

    private static final String COMPANION_DEVICE_ID = "sampleId";
    private static final String COMPANION_DEVICE_NAME = "sampleName";
    private static final String DEVICE_ADDRESS = UUID.randomUUID().toString();
    private static final String BT_DEVICE_ADDRESS = UUID.randomUUID().toString();

    private static final String MESSAGING_APP_NAME = "Messaging App";
    private static final String MESSAGING_PACKAGE_NAME = "com.android.messaging.app";
    private static final String CONVERSATION_TITLE = "Conversation";
    private static final String USER_DISPLAY_NAME = "User";
    private static final String SENDER_1 = "Sender";
    private static final String MESSAGE_TEXT_1 = "Message 1";
    private static final String MESSAGE_TEXT_2 = "Message 2";

    /** ConversationKey for {@link NotificationMsgDelegateTest#VALID_CONVERSATION_MSG}. **/
    private static final ConversationKey CONVERSATION_KEY_1
            = new ConversationKey(COMPANION_DEVICE_ID, NOTIFICATION_KEY_1);

    private static final MessagingStyleMessage MESSAGE_2 = MessagingStyleMessage.newBuilder()
            .setTextMessage(MESSAGE_TEXT_2)
            .setSender(Person.newBuilder()
                    .setName(SENDER_1))
            .setTimestamp((long) 1577909718950f)
            .build();

    private static final MessagingStyle VALID_STYLE = MessagingStyle.newBuilder()
            .setConvoTitle(CONVERSATION_TITLE)
            .setUserDisplayName(USER_DISPLAY_NAME)
            .setIsGroupConvo(false)
            .addMessagingStyleMsg(MessagingStyleMessage.newBuilder()
                    .setTextMessage(MESSAGE_TEXT_1)
                    .setSender(Person.newBuilder()
                            .setName(SENDER_1))
                    .setTimestamp((long) 1577909718050f)
                    .build())
            .build();

    private static final ConversationNotification VALID_CONVERSATION =
            ConversationNotification.newBuilder()
                    .setMessagingAppDisplayName(MESSAGING_APP_NAME)
                    .setMessagingAppPackageName(MESSAGING_PACKAGE_NAME)
                    .setTimeMs((long) 1577909716000f)
                    .setMessagingStyle(VALID_STYLE)
                    .build();

    private static final PhoneToCarMessage VALID_CONVERSATION_MSG = PhoneToCarMessage.newBuilder()
            .setNotificationKey(NOTIFICATION_KEY_1)
            .setConversation(VALID_CONVERSATION)
            .build();

    private Bitmap mIconBitmap;
    private byte[] mIconByteArray;

    @Mock
    CompanionDevice mCompanionDevice;
    @Mock
    NotificationManager mMockNotificationManager;
    @Mock
    ProjectionStateListener mMockProjectionStateListener;

    @Captor
    ArgumentCaptor<Notification> mNotificationCaptor;
    @Captor
    ArgumentCaptor<Integer> mNotificationIdCaptor;

    Context mContext = ApplicationProvider.getApplicationContext();
    NotificationMsgDelegate mNotificationMsgDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mCompanionDevice.getDeviceId()).thenReturn(COMPANION_DEVICE_ID);
        when(mCompanionDevice.getDeviceName()).thenReturn(COMPANION_DEVICE_NAME);

        mNotificationMsgDelegate = new NotificationMsgDelegate(mContext);
        mNotificationMsgDelegate.setNotificationManager(mMockNotificationManager);
        mNotificationMsgDelegate.setProjectionStateListener(mMockProjectionStateListener);

        mIconBitmap = Bitmap.createBitmap(200, 200, Bitmap.Config.ARGB_8888);

        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        mIconBitmap.compress(Bitmap.CompressFormat.PNG, 100, stream);
        mIconByteArray = stream.toByteArray();
        stream.reset();

    }

    @After
    public void tearDown() {
        mIconBitmap.recycle();
    }

    @Test
    public void newConversationShouldPostNewNotification() {
        // Test that a new conversation notification is posted with the correct fields.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());

        Notification postedNotification = mNotificationCaptor.getValue();
        verifyNotification(VALID_CONVERSATION, postedNotification);
    }

    @Test
    public void multipleNewConversationShouldPostMultipleNewNotifications() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockNotificationManager).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));
        int firstNotificationId = mNotificationIdCaptor.getValue();

        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice,
                createSecondConversation());
        verify(mMockNotificationManager, times(2)).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));

        // Verify the notification id is different than the first.
        assertThat((long) mNotificationIdCaptor.getValue()).isNotEqualTo(firstNotificationId);
    }

    @Test
    public void invalidConversationShouldDoNothing() {
        // Test that a conversation without all the required fields is dropped.
        PhoneToCarMessage newConvo = PhoneToCarMessage.newBuilder()
                .setNotificationKey(NOTIFICATION_KEY_1)
                .setConversation(VALID_CONVERSATION.toBuilder().clearMessagingStyle())
                .build();
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, newConvo);

        verify(mMockNotificationManager, never()).notify(anyInt(), any(Notification.class));
    }

    @Test
    public void newMessageShouldUpdateConversationNotification() {
        // Check whether a new message updates the notification of the conversation it belongs to.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));
        int notificationId = mNotificationIdCaptor.getValue();
        int messageCount = VALID_CONVERSATION_MSG.getConversation().getMessagingStyle()
                .getMessagingStyleMsgCount();

        // Post a new message in this conversation.
        updateConversationWithMessage2();

        // Verify same notification id is posted twice.
        verify(mMockNotificationManager, times(2)).notify(eq(notificationId),
                mNotificationCaptor.capture());

        // Verify the notification contains one more message.
        NotificationCompat.MessagingStyle messagingStyle = getMessagingStyle(
                mNotificationCaptor.getValue());
        assertThat(messagingStyle.getMessages().size()).isEqualTo(messageCount + 1);

        // Verify notification's latest message matches the new message.
        verifyMessage(MESSAGE_2, messagingStyle.getMessages().get(messageCount));
    }

    @Test
    public void existingConversationShouldUpdateNotification() {
        // Test that a conversation that already exists, but gets a new conversation message
        // is updated with the new conversation metadata.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));
        int notificationId = mNotificationIdCaptor.getValue();

        ConversationNotification updatedConversation = addSecondMessageToConversation().toBuilder()
                .setMessagingAppDisplayName("New Messaging App")
                .build();
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, PhoneToCarMessage.newBuilder()
                .setNotificationKey(NOTIFICATION_KEY_1)
                .setConversation(updatedConversation)
                .build());

        verify(mMockNotificationManager, times(2)).notify(eq(notificationId),
                mNotificationCaptor.capture());
        Notification postedNotification = mNotificationCaptor.getValue();

        // Verify Conversation level metadata does NOT change
        verifyConversationLevelMetadata(VALID_CONVERSATION, postedNotification);
        // Verify the MessagingStyle metadata does update with the new message.
        verifyMessagingStyle(updatedConversation.getMessagingStyle(), postedNotification);
    }

    @Test
    public void messageForUnknownConversationShouldDoNothing() {
        // A message for an unknown conversation should be dropped.
        updateConversationWithMessage2();

        verify(mMockNotificationManager, never()).notify(anyInt(), any(Notification.class));
    }

    @Test
    public void invalidMessageShouldDoNothing() {
        // Message without all the required fields is dropped.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        // Create a MessagingStyleMessage without a required field (Sender information).
        MessagingStyleMessage invalidMsgStyleMessage = MessagingStyleMessage.newBuilder()
                .setTextMessage("Message 2")
                .setTimestamp((long) 1577909718950f)
                .build();
        PhoneToCarMessage invalidMessage = PhoneToCarMessage.newBuilder()
                .setNotificationKey(NOTIFICATION_KEY_1)
                .setMessage(invalidMsgStyleMessage)
                .build();
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, invalidMessage);

        // Verify only one notification is posted, and never updated.
        verify(mMockNotificationManager).notify(anyInt(), any(Notification.class));
    }

    @Test
    public void invalidAvatarIconSyncShouldDoNothing() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        // Create AvatarIconSync message without required field (Icon), ensure it's treated as an
        // invalid message.
        PhoneToCarMessage invalidMessage = PhoneToCarMessage.newBuilder()
                .setNotificationKey(NOTIFICATION_KEY_1)
                .setAvatarIconSync(AvatarIconSync.newBuilder()
                        .setPerson(Person.newBuilder()
                                .setName(SENDER_1))
                        .build())
                .build();
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, invalidMessage);
        assertThat(mNotificationMsgDelegate.mOneOnOneConversationAvatarMap.isEmpty()).isTrue();
    }

    @Test
    public void avatarIconSyncForGroupConversationShouldDoNothing() {
        // We only sync avatars for 1-1 conversations.
        sendGroupConversationMessage();

        sendValidAvatarIconSyncMessage();

        assertThat(mNotificationMsgDelegate.mOneOnOneConversationAvatarMap.isEmpty()).isTrue();
    }

    @Test
    public void avatarIconSyncForUnknownConversationShouldDoNothing() {
        // Drop avatar if it's for a conversation that is unknown.
        sendValidAvatarIconSyncMessage();

        assertThat(mNotificationMsgDelegate.mOneOnOneConversationAvatarMap.isEmpty()).isTrue();
    }

    @Test
    public void avatarIconSyncSetsAvatarInNotification() {
        // Check that a conversation that didn't have an avatar, but gets this message posts
        // a notification with the avatar.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());
        Icon notSetIcon = mNotificationCaptor.getValue().getLargeIcon();

        sendValidAvatarIconSyncMessage();

        // Post an update so we update the notification and can see the new icon.
        updateConversationWithMessage2();

        verify(mMockNotificationManager, times(2)).notify(anyInt(), mNotificationCaptor.capture());
        Icon newIcon = mNotificationCaptor.getValue().getLargeIcon();
        assertThat(newIcon).isNotEqualTo(notSetIcon);
    }


    @Test
    public void avatarIconSyncStoresBitmapCorrectly() {
        // Post a conversation notification first, so we don't drop the avatar message.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        sendValidAvatarIconSyncMessage();

        AvatarIconSync iconSync = createValidAvatarIconSync();
        SenderKey senderKey = SenderKey.createSenderKey(CONVERSATION_KEY_1, iconSync.getPerson());
        byte[] iconArray = iconSync.getPerson().getAvatar().toByteArray();
        Bitmap bitmap = BitmapFactory.decodeByteArray(iconArray, /* offset= */ 0, iconArray.length);

        assertThat(mNotificationMsgDelegate.mOneOnOneConversationAvatarMap).hasSize(1);
        Bitmap actualBitmap = mNotificationMsgDelegate.mOneOnOneConversationAvatarMap.get(
                senderKey);
        assertThat(actualBitmap).isNotNull();
        assertThat(actualBitmap.sameAs(bitmap)).isTrue();
    }

    @Test
    public void phoneMetadataUsedToCheckProjectionStatus_projectionActive() {
        // Assert projectionListener gets called with phone metadata address.
        when(mMockProjectionStateListener.isProjectionInActiveForeground(
                BT_DEVICE_ADDRESS)).thenReturn(true);
        sendValidPhoneMetadataMessage();

        // Send a new conversation to trigger Projection State check.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockProjectionStateListener).isProjectionInActiveForeground(BT_DEVICE_ADDRESS);
        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());
        checkChannelImportanceLevel(
                mNotificationCaptor.getValue().getChannelId(), /* isLowImportance= */ true);
    }

    @Test
    public void phoneMetadataUsedCorrectlyToCheckProjectionStatus_projectionInactive() {
        // Assert projectionListener gets called with phone metadata address.
        when(mMockProjectionStateListener.isProjectionInActiveForeground(
                BT_DEVICE_ADDRESS)).thenReturn(false);
        sendValidPhoneMetadataMessage();

        // Send a new conversation to trigger Projection State check.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockProjectionStateListener).isProjectionInActiveForeground(BT_DEVICE_ADDRESS);
        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());
        checkChannelImportanceLevel(
                mNotificationCaptor.getValue().getChannelId(), /* isLowImportance= */ false);
    }

    @Test
    public void delegateChecksProjectionStatus_projectionActive() {
        // Assert projectionListener gets called with phone metadata address.
        when(mMockProjectionStateListener.isProjectionInActiveForeground(null)).thenReturn(true);

        // Send a new conversation to trigger Projection State check.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockProjectionStateListener).isProjectionInActiveForeground(null);
        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());
        checkChannelImportanceLevel(
                mNotificationCaptor.getValue().getChannelId(), /* isLowImportance= */ true);
    }

    @Test
    public void delegateChecksProjectionStatus_projectionInactive() {
        // Assert projectionListener gets called with phone metadata address.
        when(mMockProjectionStateListener.isProjectionInActiveForeground(null)).thenReturn(false);

        // Send a new conversation to trigger Projection State check.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockProjectionStateListener).isProjectionInActiveForeground(null);
        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());
        checkChannelImportanceLevel(
                mNotificationCaptor.getValue().getChannelId(), /* isLowImportance= */ false);
    }

    @Test
    public void clearAllAppDataShouldClearInternalDataAndNotifications() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));
        int notificationId = mNotificationIdCaptor.getValue();

        sendClearAppDataRequest(NotificationMsgDelegate.REMOVE_ALL_APP_DATA);

        verify(mMockNotificationManager).cancel(eq(notificationId));
    }

    @Test
    public void clearSpecificAppDataShouldDoNothing() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(anyInt(), any(Notification.class));

        sendClearAppDataRequest(MESSAGING_PACKAGE_NAME);

        verify(mMockNotificationManager, never()).cancel(anyInt());
    }

    @Test
    public void conversationsFromSameApplicationPostedOnSameChannel() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());
        String firstChannelId = mNotificationCaptor.getValue().getChannelId();

        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice,
                VALID_CONVERSATION_MSG.toBuilder()
                        .setNotificationKey(NOTIFICATION_KEY_2)
                        .setConversation(VALID_CONVERSATION.toBuilder()
                                .setMessagingStyle(
                                        VALID_STYLE.toBuilder().addMessagingStyleMsg(MESSAGE_2))
                                .build())
                        .build());
        verify(mMockNotificationManager, times(2)).notify(anyInt(), mNotificationCaptor.capture());

        assertThat(mNotificationCaptor.getValue().getChannelId()).isEqualTo(firstChannelId);
    }

    @Test
    public void messageDataNotSetShouldDoNothing() {
        // For a PhoneToCarMessage w/ no MessageData
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, PhoneToCarMessage.newBuilder()
                .setNotificationKey(NOTIFICATION_KEY_1)
                .build());

        verifyZeroInteractions(mMockNotificationManager);
    }

    @Test
    public void dismissShouldCreateCarToPhoneMessage() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        CarToPhoneMessage dismissMessage = mNotificationMsgDelegate.dismiss(CONVERSATION_KEY_1);

        verifyCarToPhoneActionMessage(dismissMessage, NOTIFICATION_KEY_1, DISMISS);
    }

    @Test
    public void dismissShouldDismissNotification() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));
        int notificationId = mNotificationIdCaptor.getValue();

        mNotificationMsgDelegate.dismiss(CONVERSATION_KEY_1);

        verify(mMockNotificationManager).cancel(eq(notificationId));
    }

    @Test
    public void markAsReadShouldCreateCarToPhoneMessage() {
        // Mark message as read, verify message sent to phone.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        CarToPhoneMessage markAsRead = mNotificationMsgDelegate.markAsRead(CONVERSATION_KEY_1);

        verifyCarToPhoneActionMessage(markAsRead, NOTIFICATION_KEY_1, MARK_AS_READ);
    }

    @Test
    public void markAsReadShouldExcludeMessageFromNotification() {
        // Mark message as read, verify when new message comes in, read
        // messages are not in notification.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(anyInt(), any(Notification.class));

        mNotificationMsgDelegate.markAsRead(CONVERSATION_KEY_1);
        // Post an update to this conversation to ensure the now read message is not in
        // notification.
        updateConversationWithMessage2();
        verify(mMockNotificationManager, times(2)).notify(anyInt(),
                mNotificationCaptor.capture());

        // Verify the notification contains only the latest message.
        NotificationCompat.MessagingStyle messagingStyle =
                NotificationCompat.MessagingStyle.extractMessagingStyleFromNotification(
                        mNotificationCaptor.getValue());
        assertThat(messagingStyle.getMessages().size()).isEqualTo(1);
        verifyMessage(MESSAGE_2, messagingStyle.getMessages().get(0));

    }

    @Test
    public void replyShouldCreateCarToPhoneMessage() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        CarToPhoneMessage reply = mNotificationMsgDelegate.reply(CONVERSATION_KEY_1,
                MESSAGE_TEXT_2);
        Action replyAction = reply.getActionRequest();
        NotificationMsg.MapEntry replyEntry = replyAction.getMapEntry(0);

        verifyCarToPhoneActionMessage(reply, NOTIFICATION_KEY_1, REPLY);
        assertThat(replyAction.getMapEntryCount()).isEqualTo(1);
        assertThat(replyEntry.getKey()).isEqualTo(NotificationMsgDelegate.REPLY_KEY);
        assertThat(replyEntry.getValue()).isEqualTo(MESSAGE_TEXT_2);
    }

    @Test
    public void onDestroyShouldClearInternalDataAndNotifications() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));
        int notificationId = mNotificationIdCaptor.getValue();
        sendValidAvatarIconSyncMessage();

        mNotificationMsgDelegate.onDestroy();

        assertThat(mNotificationMsgDelegate.mOneOnOneConversationAvatarMap.isEmpty()).isTrue();
        verify(mMockNotificationManager).cancel(eq(notificationId));
    }

    @Test
    public void deviceDisconnectedShouldClearDeviceNotificationsAndMetadata() {
        // Test that after a device disconnects, all the avatars, notifications for the device
        // is removed.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);
        verify(mMockNotificationManager).notify(mNotificationIdCaptor.capture(),
                any(Notification.class));
        int notificationId = mNotificationIdCaptor.getValue();
        sendValidAvatarIconSyncMessage();

        mNotificationMsgDelegate.onDeviceDisconnected(COMPANION_DEVICE_ID);

        assertThat(mNotificationMsgDelegate.mOneOnOneConversationAvatarMap.isEmpty()).isTrue();
        verify(mMockNotificationManager).cancel(eq(notificationId));
    }

    @Test
    public void deviceDisconnectedShouldResetProjectionDeviceAddress() {
        // Test that after a device disconnects, then reconnects, the projection device address
        // is reset.
        when(mMockProjectionStateListener.isProjectionInActiveForeground(
                BT_DEVICE_ADDRESS)).thenReturn(true);
        sendValidPhoneMetadataMessage();

        mNotificationMsgDelegate.onDeviceDisconnected(COMPANION_DEVICE_ID);

        // Now post a new notification for this device and ensure it is not posted silently.
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, VALID_CONVERSATION_MSG);

        verify(mMockProjectionStateListener).isProjectionInActiveForeground(null);
        verify(mMockNotificationManager).notify(anyInt(), mNotificationCaptor.capture());
        checkChannelImportanceLevel(
                mNotificationCaptor.getValue().getChannelId(), /* isLowImportance= */ false);
    }

    private void verifyNotification(ConversationNotification expected, Notification notification) {
        verifyConversationLevelMetadata(expected, notification);
        verifyMessagingStyle(expected.getMessagingStyle(), notification);
    }

    /**
     * Verifies the conversation level metadata and other aspects of a notification that do not
     * change when a new message is added to it (such as the actions, intents).
     */
    private void verifyConversationLevelMetadata(ConversationNotification expected,
            Notification notification) {
        assertThat(notification.category).isEqualTo(CATEGORY_MESSAGE);

        assertThat(notification.getSmallIcon()).isNotNull();
        if (!expected.getAppIcon().isEmpty()) {
            byte[] iconBytes = expected.getAppIcon().toByteArray();
            Icon appIcon = Icon.createWithData(iconBytes, 0, iconBytes.length);
            assertThat(notification.getSmallIcon()).isEqualTo(appIcon);
        }

        assertThat(notification.deleteIntent).isNotNull();

        if (expected.getMessagingAppPackageName() != null) {
            CharSequence appName = notification.extras.getCharSequence(
                    Notification.EXTRA_SUBSTITUTE_APP_NAME);
            assertThat(appName).isEqualTo(expected.getMessagingAppDisplayName());
        }

        assertThat(notification.actions.length).isEqualTo(2);
        for (NotificationCompat.Action action : getAllActions(notification)) {
            if (action.getSemanticAction() == NotificationCompat.Action.SEMANTIC_ACTION_REPLY) {
                assertThat(action.getRemoteInputs().length).isEqualTo(1);
            }
            assertThat(action.getShowsUserInterface()).isFalse();
        }
    }

    private void verifyMessagingStyle(MessagingStyle expected, Notification notification) {
        final NotificationCompat.MessagingStyle messagingStyle = getMessagingStyle(notification);

        assertThat(messagingStyle.getUser().getName()).isEqualTo(expected.getUserDisplayName());
        assertThat(messagingStyle.isGroupConversation()).isEqualTo(expected.getIsGroupConvo());
        assertThat(messagingStyle.getMessages().size()).isEqualTo(expected.getMessagingStyleMsgCount());

        for (int i = 0; i < expected.getMessagingStyleMsgCount(); i++) {
            MessagingStyleMessage expectedMsg = expected.getMessagingStyleMsg(i);
            NotificationCompat.MessagingStyle.Message actualMsg = messagingStyle.getMessages().get(
                    i);
            verifyMessage(expectedMsg, actualMsg);

        }
    }

    private void verifyMessage(MessagingStyleMessage expectedMsg,
            NotificationCompat.MessagingStyle.Message actualMsg) {
        assertThat(actualMsg.getTimestamp()).isEqualTo(expectedMsg.getTimestamp());
        assertThat(actualMsg.getText()).isEqualTo(expectedMsg.getTextMessage());

        Person expectedSender = expectedMsg.getSender();
        androidx.core.app.Person actualSender = actualMsg.getPerson();
        assertThat(actualSender.getName()).isEqualTo(expectedSender.getName());
        if (!expectedSender.getAvatar().isEmpty()) {
            assertThat(actualSender.getIcon()).isNotNull();
        } else {
            assertThat(actualSender.getIcon()).isNull();
        }
    }

    private void verifyCarToPhoneActionMessage(CarToPhoneMessage message, String notificationKey,
            Action.ActionName actionName) {
        assertThat(message.getNotificationKey()).isEqualTo(notificationKey);
        assertThat(message.getActionRequest()).isNotNull();
        assertThat(message.getActionRequest().getNotificationKey()).isEqualTo(notificationKey);
        assertThat(message.getActionRequest().getActionName()).isEqualTo(actionName);
    }

    private void checkChannelImportanceLevel(String channelId, boolean isLowImportance) {
        ArgumentCaptor<NotificationChannel> channelCaptor = ArgumentCaptor.forClass(
                NotificationChannel.class);
        verify(mMockNotificationManager, atLeastOnce()).createNotificationChannel(
                channelCaptor.capture());

        int desiredImportance = isLowImportance ? IMPORTANCE_LOW : IMPORTANCE_HIGH;
        List<String> desiredImportanceChannelIds = new ArrayList<>();
        // Each messaging app has 2 channels, one high and one low importance.
        for (NotificationChannel notificationChannel : channelCaptor.getAllValues()) {
            if (notificationChannel.getImportance() == desiredImportance) {
                desiredImportanceChannelIds.add(notificationChannel.getId());
            }
        }
        assertThat(desiredImportanceChannelIds.contains(channelId)).isTrue();
    }

    private NotificationCompat.MessagingStyle getMessagingStyle(Notification notification) {
        return NotificationCompat.MessagingStyle.extractMessagingStyleFromNotification(
                notification);
    }

    private List<NotificationCompat.Action> getAllActions(Notification notification) {
        List<NotificationCompat.Action> actions = new ArrayList<>();
        actions.addAll(NotificationCompat.getInvisibleActions(notification));
        for (int i = 0; i < NotificationCompat.getActionCount(notification); i++) {
            actions.add(NotificationCompat.getAction(notification, i));
        }
        return actions;
    }

    private PhoneToCarMessage createSecondConversation() {
        return VALID_CONVERSATION_MSG.toBuilder()
                .setNotificationKey(NOTIFICATION_KEY_2)
                .setConversation(addSecondMessageToConversation())
                .build();
    }

    private ConversationNotification addSecondMessageToConversation() {
        return VALID_CONVERSATION.toBuilder()
                .setMessagingStyle(
                        VALID_STYLE.toBuilder().addMessagingStyleMsg(MESSAGE_2)).build();
    }

    private AvatarIconSync createValidAvatarIconSync() {
        return AvatarIconSync.newBuilder()
                .setMessagingAppPackageName(MESSAGING_PACKAGE_NAME)
                .setMessagingAppDisplayName(MESSAGING_APP_NAME)
                .setPerson(Person.newBuilder()
                        .setName(SENDER_1)
                        .setAvatar(ByteString.copyFrom(mIconByteArray))
                        .build())
                .build();
    }

    /**
     * Small helper method that updates {@link NotificationMsgDelegateTest#VALID_CONVERSATION} with
     * a new message.
     */
    private void updateConversationWithMessage2() {
        PhoneToCarMessage updateConvo = PhoneToCarMessage.newBuilder()
                .setNotificationKey(NOTIFICATION_KEY_1)
                .setMessage(MESSAGE_2)
                .build();
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, updateConvo);
    }

    private void sendValidAvatarIconSyncMessage() {
        PhoneToCarMessage validMessage = PhoneToCarMessage.newBuilder()
                .setNotificationKey(NOTIFICATION_KEY_1)
                .setAvatarIconSync(createValidAvatarIconSync())
                .build();
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, validMessage);
    }

    private void sendValidPhoneMetadataMessage() {
        PhoneToCarMessage metadataMessage = PhoneToCarMessage.newBuilder()
                .setPhoneMetadata(PhoneMetadata.newBuilder()
                        .setBluetoothDeviceAddress(BT_DEVICE_ADDRESS)
                        .build())
                .build();
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice, metadataMessage);
    }

    private void sendGroupConversationMessage() {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice,
                VALID_CONVERSATION_MSG.toBuilder()
                        .setConversation(VALID_CONVERSATION.toBuilder()
                                .setMessagingStyle(VALID_STYLE.toBuilder()
                                        .setIsGroupConvo(true)
                                        .build())
                                .build())
                        .build());
    }


    private void sendClearAppDataRequest(String messagingAppPackageName) {
        mNotificationMsgDelegate.onMessageReceived(mCompanionDevice,
                PhoneToCarMessage.newBuilder()
                        .setClearAppDataRequest(ClearAppDataRequest.newBuilder()
                                .setMessagingAppPackageName(messagingAppPackageName)
                                .build())
                        .build());
    }
}
