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

package com.android.car.messenger;


import static com.android.car.apps.common.util.SafeLog.logd;
import static com.android.car.apps.common.util.SafeLog.loge;
import static com.android.car.apps.common.util.SafeLog.logw;

import android.annotation.Nullable;
import android.app.PendingIntent;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothMapClient;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.widget.Toast;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import com.android.car.messenger.bluetooth.BluetoothHelper;
import com.android.car.messenger.bluetooth.BluetoothMonitor;
import com.android.car.messenger.common.BaseNotificationDelegate;
import com.android.car.messenger.common.ConversationKey;
import com.android.car.messenger.common.ConversationNotificationInfo;
import com.android.car.messenger.common.Message;
import com.android.car.messenger.common.ProjectionStateListener;
import com.android.car.messenger.common.SenderKey;
import com.android.car.messenger.common.Utils;
import com.android.car.telephony.common.TelecomUtils;
import com.android.internal.annotations.GuardedBy;

import com.bumptech.glide.Glide;
import com.bumptech.glide.request.RequestOptions;
import com.bumptech.glide.request.target.SimpleTarget;
import com.bumptech.glide.request.transition.Transition;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Consumer;

/** Delegate class responsible for handling messaging service actions */
public class MessageNotificationDelegate extends BaseNotificationDelegate implements
        BluetoothMonitor.OnBluetoothEventListener {
    private static final String TAG = "MsgNotiDelegate";
    private static final Object mMapClientLock = new Object();

    @GuardedBy("mMapClientLock")
    private BluetoothMapClient mBluetoothMapClient;
    /** Tracks whether a projection application is active in the foreground. **/
    private ProjectionStateListener mProjectionStateListener;
    private CompletableFuture<Void> mPhoneNumberInfoFuture;
    private Locale mGeneratedGroupConversationTitlesLocale;
    private static int mBitmapSize;
    private static float mCornerRadiusPercent;
    private static boolean mShouldLoadExistingMessages;
    private static int mNotificationConversationTitleLength;

    final Map<String, Long> mBtDeviceAddressToConnectionTimestamp = new HashMap<>();
    final Map<SenderKey, Bitmap> mSenderToLargeIconBitmap = new HashMap<>();
    final Map<String, String> mUriToSenderNameMap = new HashMap<>();
    final Set<ConversationKey> mGeneratedGroupConversationTitles = new HashSet<>();

    public MessageNotificationDelegate(Context context) {
        super(context, /* useLetterTile */ true);
        mProjectionStateListener = new ProjectionStateListener(context);
        loadConfigValues(context);
    }

    /** Loads all necessary values from the config.xml at creation or when values are changed. **/
    protected static void loadConfigValues(Context context) {
        mBitmapSize = 300;
        mCornerRadiusPercent = (float) 0.5;
        mShouldLoadExistingMessages = false;
        mNotificationConversationTitleLength = 30;
        try {
            mBitmapSize =
                    context.getResources()
                            .getDimensionPixelSize(R.dimen.notification_contact_photo_size);
            mCornerRadiusPercent = context.getResources()
                    .getFloat(R.dimen.contact_avatar_corner_radius_percent);
            mShouldLoadExistingMessages =
                    context.getResources().getBoolean(R.bool.config_loadExistingMessages);
            mNotificationConversationTitleLength = context.getResources().getInteger(
                    R.integer.notification_conversation_title_length);
        } catch (Resources.NotFoundException e) {
            // Should only happen for robolectric unit tests;
            loge(TAG, "Disabling loading of existing messages: " + e.getMessage());
        }
    }

    @Override
    public void onMessageReceived(Intent intent) {
        addNamesToSenderMap(intent);
        if (Utils.isGroupConversation(intent)) {
            // Group Conversations have URIs of senders whose names we need to load from the DB.
            loadNamesFromDatabase(intent);
        }
        loadAvatarIconAndProcessMessage(intent);
    }

    @Override
    public void onMessageSent(Intent intent) {
        logd(TAG, "onMessageSent");
    }

    @Override
    public void onDeviceConnected(BluetoothDevice device) {
        logd(TAG, "Device connected: " + device.getAddress());
        mBtDeviceAddressToConnectionTimestamp.put(device.getAddress(), System.currentTimeMillis());
        synchronized (mMapClientLock) {
            if (mBluetoothMapClient != null) {
                if (mShouldLoadExistingMessages) {
                    mBluetoothMapClient.getUnreadMessages(device);
                }
            } else {
                // onDeviceConnected should be sent by BluetoothMapClient, so log if we run into
                // this strange case.
                loge(TAG, "BluetoothMapClient is null after connecting to device.");
            }
        }
    }

    @Override
    public void onDeviceDisconnected(BluetoothDevice device) {
        String deviceAddress = device.getAddress();
        logd(TAG, "Device disconnected: " + deviceAddress);
        cleanupMessagesAndNotifications(key -> key.matches(deviceAddress));
        mBtDeviceAddressToConnectionTimestamp.remove(deviceAddress);
        mSenderToLargeIconBitmap.entrySet().removeIf(entry ->
                entry.getKey().getDeviceId().equals(deviceAddress));
        mGeneratedGroupConversationTitles.removeIf(
                convoKey -> convoKey.getDeviceId().equals(deviceAddress));
    }

    @Override
    public void onMapConnected(BluetoothMapClient client) {
        logd(TAG, "Connected to BluetoothMapClient");
        List<BluetoothDevice> connectedDevices;
        synchronized (mMapClientLock) {
            if (mBluetoothMapClient == client) {
                return;
            }

            mBluetoothMapClient = client;
            connectedDevices = mBluetoothMapClient.getConnectedDevices();
        }
        if (connectedDevices != null) {
            for (BluetoothDevice device : connectedDevices) {
                onDeviceConnected(device);
            }
        }
    }

    @Override
    public void onMapDisconnected() {
        logd(TAG, "Disconnected from BluetoothMapClient");
        resetInternalData();
        synchronized (mMapClientLock) {
            mBluetoothMapClient = null;
        }
    }

    @Override
    public void onSdpRecord(BluetoothDevice device, boolean supportsReply) {
        /* NO_OP */
    }

    protected void markAsRead(ConversationKey convoKey) {
        excludeFromNotification(convoKey);
    }

    protected void dismiss(ConversationKey convoKey) {
        super.dismissInternal(convoKey);
    }

    @Override
    protected boolean shouldAddReplyAction(String deviceAddress) {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null) {
            return false;
        }
        BluetoothDevice device = adapter.getRemoteDevice(deviceAddress);

        synchronized (mMapClientLock) {
            return (mBluetoothMapClient != null) && mBluetoothMapClient.isUploadingSupported(
                    device);
        }
    }

    protected void sendMessage(ConversationKey conversationKey, String messageText) {
        final boolean deviceConnected = mBtDeviceAddressToConnectionTimestamp.containsKey(
                conversationKey.getDeviceId());
        if (!deviceConnected) {
            logw(TAG, "sendMessage: device disconnected, can't send message");
            return;
        }
        boolean success = false;
        synchronized (mMapClientLock) {
            if (mBluetoothMapClient != null) {
                ConversationNotificationInfo notificationInfo = mNotificationInfos.get(
                        conversationKey);
                if (notificationInfo == null) {
                    logw(TAG, "No notificationInfo found for senderKey "
                            + conversationKey.toString());
                } else if (notificationInfo.getCcRecipientsUris().isEmpty()) {
                    logw(TAG, "No contact URI for sender!");
                } else {
                    success = sendMessageInternal(conversationKey, messageText);
                }
            }
        }

        if (!success) {
            Toast.makeText(mContext, R.string.auto_reply_failed_message, Toast.LENGTH_SHORT).show();
        }
    }

    protected void onDestroy() {
        resetInternalData();
        if (mPhoneNumberInfoFuture != null) {
            mPhoneNumberInfoFuture.cancel(true);
        }
        mProjectionStateListener.destroy();
    }

    private void resetInternalData() {
        cleanupMessagesAndNotifications(key -> true);
        mUriToSenderNameMap.clear();
        mSenderToLargeIconBitmap.clear();
        mBtDeviceAddressToConnectionTimestamp.clear();
        mGeneratedGroupConversationTitles.clear();
    }

    /**
     * Creates a new message and links it to the conversation identified by the convoKey. Then
     * posts the message notification after all loading queries from the database have finished.
     */
    private void initializeNewMessage(ConversationKey convoKey, Message message) {
        addMessageToNotificationInfo(message, convoKey);
        ConversationNotificationInfo notificationInfo = mNotificationInfos.get(convoKey);
        // Only show notifications for messages received AFTER phone was connected.
        mPhoneNumberInfoFuture.thenRun(() -> {
            setGroupConversationTitle(convoKey);
            // Only show notifications for messages received AFTER phone was connected.
            if (message.getReceivedTime()
                    >= mBtDeviceAddressToConnectionTimestamp.get(convoKey.getDeviceId())) {
                postNotification(convoKey, notificationInfo, getChannelId(convoKey.getDeviceId()),
                        mSenderToLargeIconBitmap.get(message.getSenderKey()));
            }
        });
    }

    /**
     * Creates a new conversation with all of the conversation metadata, and adds the first
     * message to the conversation.
     */
    private void initializeNewConversation(ConversationKey convoKey, Intent intent) {
        if (mNotificationInfos.containsKey(convoKey)) {
            logw(TAG, "Conversation already exists! " + convoKey.toString());
        }
        Message message = Message.parseFromIntent(intent);
        ConversationNotificationInfo notiInfo;
        try {
            // Pass in null icon, since the fallback icon represents the system app's icon.
            notiInfo =
                    ConversationNotificationInfo.createConversationNotificationInfo(intent,
                            message.getSenderName(), mContext.getClass().getName(),
                            /* appIcon */ null);
        } catch (IllegalArgumentException e) {
            logw(TAG, "initNewConvo: Message could not be created from the intent.");
            return;
        }
        mNotificationInfos.put(convoKey, notiInfo);
        initializeNewMessage(convoKey, message);
    }

    /** Loads the avatar icon, and processes the message after avatar is loaded. **/
    private void loadAvatarIconAndProcessMessage(Intent intent) {
        SenderKey senderKey = SenderKey.createSenderKey(intent);
        String phoneNumber = Utils.getPhoneNumberFromMapClient(Utils.getSenderUri(intent));
        if (mSenderToLargeIconBitmap.containsKey(senderKey) || phoneNumber == null) {
            addMessageFromIntent(intent);
            return;
        }
        loadPhoneNumberInfo(phoneNumber, phoneNumberInfo -> {
            if (phoneNumberInfo == null) {
                return;
            }
            Glide.with(mContext)
                    .asBitmap()
                    .load(phoneNumberInfo.getAvatarUri())
                    .apply(new RequestOptions().override(mBitmapSize))
                    .into(new SimpleTarget<Bitmap>() {
                        @Override
                        public void onResourceReady(Bitmap bitmap,
                                Transition<? super Bitmap> transition) {
                            RoundedBitmapDrawable roundedBitmapDrawable =
                                    RoundedBitmapDrawableFactory
                                            .create(mContext.getResources(), bitmap);
                            Icon avatarIcon = TelecomUtils
                                    .createFromRoundedBitmapDrawable(roundedBitmapDrawable,
                                            mBitmapSize,
                                            mCornerRadiusPercent);
                            mSenderToLargeIconBitmap.put(senderKey, avatarIcon.getBitmap());
                            addMessageFromIntent(intent);
                            return;
                        }

                        @Override
                        public void onLoadFailed(@Nullable Drawable fallback) {
                            addMessageFromIntent(intent);
                            return;
                        }
                    });
        });
    }

    /**
     * Extracts the message from the intent and creates a new conversation or message
     * appropriately.
     */
    private void addMessageFromIntent(Intent intent) {
        ConversationKey convoKey = ConversationKey.createConversationKey(intent);

        if (convoKey == null) return;
        logd(TAG, "Received message from " + convoKey.getDeviceId());
        if (mNotificationInfos.containsKey(convoKey)) {
            try {
                initializeNewMessage(convoKey, Message.parseFromIntent(intent));
            } catch (IllegalArgumentException e) {
                logw(TAG, "addMessage: Message could not be created from the intent.");
                return;
            }
        } else {
            initializeNewConversation(convoKey, intent);
        }
    }

    private void addNamesToSenderMap(Intent intent) {
        String senderUri = Utils.getSenderUri(intent);
        String senderName = Utils.getSenderName(intent);
        if (senderUri != null) {
            mUriToSenderNameMap.put(senderUri, senderName);
        }
    }

    /**
     * Loads the name of a sender based on the sender's contact URI.
     *
     * This is needed to load the participants' names of a group conversation since
     * {@link BluetoothMapClient} only sends the URIs of these participants.
     */
    private void loadNamesFromDatabase(Intent intent) {
        for (String uri : Utils.getInclusiveRecipientsUrisList(intent)) {
            String phoneNumber = Utils.getPhoneNumberFromMapClient(uri);
            if (phoneNumber != null && !mUriToSenderNameMap.containsKey(uri)) {
                loadPhoneNumberInfo(phoneNumber, (phoneNumberInfo) -> {
                    mUriToSenderNameMap.put(uri, phoneNumberInfo.getDisplayName());
                });
            }
        }
    }

    /**
     * Sets the group conversation title using the names of all the participants in the group.
     * If all the participants' names have been loaded from the database, then we don't need
     * to generate the title again.
     *
     * A group conversation's title should be an alphabetically sorted list of the participant's
     * names, separated by commas.
     */
    private void setGroupConversationTitle(ConversationKey conversationKey) {
        ConversationNotificationInfo notificationInfo = mNotificationInfos.get(conversationKey);
        Locale locale = Locale.getDefault();

        // Do not reuse the old titles if locale has changed. The new locale might need different
        // formatting or text direction.
        if (locale != mGeneratedGroupConversationTitlesLocale) {
            mGeneratedGroupConversationTitles.clear();
        }
        if (!notificationInfo.isGroupConvo()
                || mGeneratedGroupConversationTitles.contains(conversationKey)) {
            return;
        }

        List<String> names = new ArrayList<>();

        boolean allNamesLoaded = true;
        for (String uri : notificationInfo.getCcRecipientsUris()) {
            if (mUriToSenderNameMap.containsKey(uri)) {
                names.add(mUriToSenderNameMap.get(uri));
            } else {
                names.add(Utils.getPhoneNumberFromMapClient(uri));
                // This URI has not been loaded from the database, set allNamesLoaded to false.
                allNamesLoaded = false;
            }
        }

        notificationInfo.setConvoTitle(Utils.constructGroupConversationTitle(names,
                mContext.getString(R.string.name_separator), mNotificationConversationTitleLength));
        if (allNamesLoaded) {
            mGeneratedGroupConversationTitlesLocale = locale;
            mGeneratedGroupConversationTitles.add(conversationKey);
        }
    }

    private void loadPhoneNumberInfo(@Nullable String phoneNumber,
            Consumer<? super TelecomUtils.PhoneNumberInfo> action) {
        if (phoneNumber == null) {
            logw(TAG, " Could not load PhoneNumberInfo due to null phone number");
            return;
        }

        mPhoneNumberInfoFuture = TelecomUtils.getPhoneNumberInfo(mContext, phoneNumber)
                .thenAcceptAsync(action, mContext.getMainExecutor());
    }

    private String getChannelId(String deviceAddress) {
        if (mProjectionStateListener.isProjectionInActiveForeground(deviceAddress)) {
            return MessengerService.SILENT_SMS_CHANNEL_ID;
        }
        return MessengerService.SMS_CHANNEL_ID;
    }

    /** Sends reply message to the BluetoothMapClient to send to the connected phone. **/
    private boolean sendMessageInternal(ConversationKey conversationKey, String messageText) {
        ConversationNotificationInfo notificationInfo = mNotificationInfos.get(conversationKey);
        Uri[] recipientUrisArray = generateRecipientUriArray(notificationInfo);

        final int requestCode = conversationKey.hashCode();

        Intent intent = new Intent(BluetoothMapClient.ACTION_MESSAGE_SENT_SUCCESSFULLY);
        PendingIntent sentIntent = PendingIntent.getBroadcast(mContext, requestCode,
                intent,
                PendingIntent.FLAG_ONE_SHOT);

        try {
            return BluetoothHelper.sendMessage(mBluetoothMapClient,
                    conversationKey.getDeviceId(), recipientUrisArray, messageText,
                    sentIntent, null);
        } catch (IllegalArgumentException e) {
            logw(TAG, "Invalid device address: " + conversationKey.getDeviceId());
        }
        return false;
    }

    /**
     * Generate an array containing all the recipients' URIs that should receive the user's
     * message for the given notificationInfo.
     */
    private Uri[] generateRecipientUriArray(ConversationNotificationInfo notificationInfo) {
        List<String> ccRecipientsUris = notificationInfo.getCcRecipientsUris();
        Uri[] recipientUris = new Uri[ccRecipientsUris.size()];

        for (int i = 0; i < ccRecipientsUris.size(); i++) {
            recipientUris[i] = Uri.parse(ccRecipientsUris.get(i));
        }
        return recipientUris;
    }
}
