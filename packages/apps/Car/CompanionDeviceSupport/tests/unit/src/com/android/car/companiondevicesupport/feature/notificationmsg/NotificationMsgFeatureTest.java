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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.ServiceConnection;
import android.os.IBinder;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.messenger.NotificationMsgProto.NotificationMsg;
import com.android.car.messenger.common.CompositeKey;
import com.android.car.messenger.common.ConversationKey;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.UUID;
import java.util.function.Predicate;

@RunWith(AndroidJUnit4.class)
public class NotificationMsgFeatureTest {

    private static final String DEVICE_ID = UUID.randomUUID().toString();
    private static final NotificationMsg.PhoneToCarMessage PHONE_TO_CAR_MESSAGE =
            NotificationMsg.PhoneToCarMessage.newBuilder()
                    .build();

    @Mock
    private Context mContext;
    @Mock
    private IBinder mIBinder;
    @Mock
    private NotificationMsgDelegate mNotificationMsgDelegate;
    @Mock
    private CompanionDevice mCompanionDevice;


    private NotificationMsgFeature mNotificationMsgFeature;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mContext.getString(eq(R.string.notification_msg_feature_id))).thenReturn(
                UUID.randomUUID().toString());
        when(mContext.bindServiceAsUser(any(), any(), anyInt(), any())).thenReturn(true);
        when(mCompanionDevice.getDeviceId()).thenReturn(DEVICE_ID);

        mNotificationMsgFeature = new NotificationMsgFeature(mContext, mNotificationMsgDelegate);
    }

    @Test
    public void startShouldClearInternalMemory() {
        mNotificationMsgFeature.start();

        ArgumentCaptor<Predicate<CompositeKey>> predicateArgumentCaptor = ArgumentCaptor.forClass(
                Predicate.class);
        verify(mNotificationMsgDelegate).cleanupMessagesAndNotifications(
                predicateArgumentCaptor.capture());

        // There's no way to test if two predicates have the same logic, so test the logic itself.
        // The expected predicate should return true for any CompositeKey. Since CompositeKey is
        // an abstract class, test with a class that extends it.
        ConversationKey device_key = new ConversationKey(DEVICE_ID, "subKey");
        ConversationKey device_key_1 = new ConversationKey("NOT_DEVICE_ID", "subKey");
        assertThat(predicateArgumentCaptor.getValue().test(device_key)).isTrue();
        assertThat(predicateArgumentCaptor.getValue().test(device_key_1)).isTrue();
        assertThat(predicateArgumentCaptor.getValue().test(null)).isTrue();
    }

    @Test
    public void stopShouldDestroyDelegate() {
        mNotificationMsgFeature.start();

        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mContext).bindServiceAsUser(any(), serviceConnectionCaptor.capture(), anyInt(),
                any());
        serviceConnectionCaptor.getValue().onServiceConnected(null, mIBinder);
        mNotificationMsgFeature.stop();
        verify(mNotificationMsgDelegate).onDestroy();
    }

    @Test
    public void onMessageReceivedShouldPassMessageToDelegate() {
        startWithSecureDevice();

        mNotificationMsgFeature.onMessageReceived(mCompanionDevice,
                PHONE_TO_CAR_MESSAGE.toByteArray());
        verify(mNotificationMsgDelegate).onMessageReceived(mCompanionDevice, PHONE_TO_CAR_MESSAGE);
    }

    @Test
    public void onMessageReceivedShouldCheckDeviceConnection() {
        when(mCompanionDevice.hasSecureChannel()).thenReturn(false);
        when(mCompanionDevice.isActiveUser()).thenReturn(true);
        mNotificationMsgFeature.start();

        mNotificationMsgFeature.onMessageReceived(mCompanionDevice,
                PHONE_TO_CAR_MESSAGE.toByteArray());
        verify(mNotificationMsgDelegate, never()).onMessageReceived(mCompanionDevice,
                PHONE_TO_CAR_MESSAGE);
    }

    @Test
    public void unknownDeviceDisconnectedShouldDoNothing() {
        when(mCompanionDevice.hasSecureChannel()).thenReturn(true);
        when(mCompanionDevice.isActiveUser()).thenReturn(true);
        mNotificationMsgFeature.start();

        mNotificationMsgFeature.onDeviceDisconnected(mCompanionDevice);
        verify(mNotificationMsgDelegate, never()).onDeviceDisconnected(DEVICE_ID);
    }

    @Test
    public void secureDeviceDisconnectedShouldAlertDelegate() {
        startWithSecureDevice();

        mNotificationMsgFeature.onDeviceDisconnected(mCompanionDevice);
        verify(mNotificationMsgDelegate).onDeviceDisconnected(DEVICE_ID);
    }

    private void startWithSecureDevice() {
        when(mCompanionDevice.hasSecureChannel()).thenReturn(true);
        when(mCompanionDevice.isActiveUser()).thenReturn(true);
        mNotificationMsgFeature.start();
        mNotificationMsgFeature.onSecureChannelEstablished(mCompanionDevice);
    }
}
