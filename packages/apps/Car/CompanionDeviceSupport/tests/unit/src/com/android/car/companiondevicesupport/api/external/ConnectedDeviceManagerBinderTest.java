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

package com.android.car.companiondevicesupport.api.external;

import static com.android.car.connecteddevice.ConnectedDeviceManager.*;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.mockitoSession;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.annotation.NonNull;
import android.os.ParcelUuid;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.connecteddevice.ConnectedDeviceManager;
import com.android.car.connecteddevice.model.ConnectedDevice;
import com.android.car.connecteddevice.util.ByteUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoSession;

import java.util.Collections;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.Executor;
import java.util.concurrent.Semaphore;

@RunWith(AndroidJUnit4.class)
public class ConnectedDeviceManagerBinderTest {

    private final ParcelUuid mRecipientId = new ParcelUuid(UUID.randomUUID());

    @Mock
    private ConnectedDeviceManager mMockConnectedDeviceManager;

    private ConnectedDeviceManagerBinder mBinder;

    private MockitoSession mMockingSession;

    @Before
    public void setUp() {
        mMockingSession = mockitoSession()
                .initMocks(this)
                .startMocking();
        mBinder = new ConnectedDeviceManagerBinder(mMockConnectedDeviceManager);
    }

    @After
    public void tearDown() {
        if (mMockingSession != null) {
            mMockingSession.finishMocking();
        }
    }

    @Test
    public void getActiveUserConnectedDevices_mirrorsConnectedDeviceManager() {
        ConnectedDevice connectedDevice = new ConnectedDevice(UUID.randomUUID().toString(),
                /* deviceName = */ null, /* belongsToActiveUser = */ false,
                /* hasSecureChannel = */ false);
        CompanionDevice expectedDevice = new CompanionDevice(connectedDevice);
        when(mMockConnectedDeviceManager.getActiveUserConnectedDevices())
                .thenReturn(Collections.singletonList(connectedDevice));
        List<CompanionDevice> returnedDevices = mBinder.getActiveUserConnectedDevices();
        assertThat(returnedDevices).containsExactly(expectedDevice);
    }

    @Test
    public void registerActiveUserConnectionCallback_mirrorsConnectedDeviceManager() {
        mBinder.registerActiveUserConnectionCallback(
                createConnectionCallback(new Semaphore(0)));
        verify(mMockConnectedDeviceManager).registerActiveUserConnectionCallback(
                any(ConnectionCallback.class), any(Executor.class));
    }

    @Test
    public void unregisterConnectionCallback_mirrorsConnectedDeviceManager() {
        IConnectionCallback callback = createConnectionCallback(new Semaphore(0));
        mBinder.registerActiveUserConnectionCallback(callback);
        mBinder.unregisterConnectionCallback(callback);
        verify(mMockConnectedDeviceManager).unregisterConnectionCallback(
                any(ConnectionCallback.class));
    }

    @Test
    public void registerDeviceCallback_mirrorsConnectedDeviceManager() {
        IDeviceCallback deviceCallback = createDeviceCallback(new Semaphore(0));
        CompanionDevice companionDevice = new CompanionDevice(UUID.randomUUID().toString(),
                /* deviceName = */ null, /* isActiveUser = */ false,
                /* hasSecureChannel = */ false);
        mBinder.registerDeviceCallback(companionDevice, mRecipientId,
                deviceCallback);
        verify(mMockConnectedDeviceManager).registerDeviceCallback(
                refEq(companionDevice.toConnectedDevice()), refEq(mRecipientId.getUuid()),
                any(DeviceCallback.class), any(Executor.class));
    }

    @Test
    public void unregisterDeviceCallback_mirrorsConnectedDeviceManager() {
        IDeviceCallback deviceCallback = createDeviceCallback(new Semaphore(0));
        CompanionDevice companionDevice = new CompanionDevice(UUID.randomUUID().toString(),
                /* deviceName = */ null, /* isActiveUser = */ false,
                /* hasSecureChannel = */ false);
        mBinder.registerDeviceCallback(companionDevice, mRecipientId, deviceCallback);
        mBinder.unregisterDeviceCallback(companionDevice, mRecipientId, deviceCallback);
        verify(mMockConnectedDeviceManager).unregisterDeviceCallback(
                refEq(companionDevice.toConnectedDevice()), refEq(mRecipientId.getUuid()),
                any(DeviceCallback.class));
    }

    @Test
    public void sendMessageSecurely_mirrorsConnectedDeviceManager() {
        CompanionDevice companionDevice = new CompanionDevice(UUID.randomUUID().toString(),
                /* deviceName = */ null, /* isActiveUser = */ false,
                /* hasSecureChannel = */ true);
        byte[] message = ByteUtils.randomBytes(10);
        mBinder.sendMessageSecurely(companionDevice, mRecipientId, message);
        verify(mMockConnectedDeviceManager).sendMessageSecurely(
                refEq(companionDevice.toConnectedDevice()),
                refEq(mRecipientId.getUuid()), refEq(message));
    }

    @Test
    public void sendMessageUnsecurely_mirrorsConnectedDeviceManager() {
        CompanionDevice companionDevice = new CompanionDevice(UUID.randomUUID().toString(),
                /* deviceName = */ null, /* isActiveUser = */ false,
                /* hasSecureChannel = */ false);
        byte[] message = ByteUtils.randomBytes(10);
        mBinder.sendMessageUnsecurely(companionDevice, mRecipientId, message);
        verify(mMockConnectedDeviceManager).sendMessageUnsecurely(
                refEq(companionDevice.toConnectedDevice()),
                refEq(mRecipientId.getUuid()), refEq(message));
    }

    @Test
    public void registerDeviceAssociationCallback_mirrorsConnectedDeviceManager() {
        IDeviceAssociationCallback associationCallback = createDeviceAssociationCallback(
                new Semaphore(0));
        mBinder.registerDeviceAssociationCallback(associationCallback);
        verify(mMockConnectedDeviceManager).registerDeviceAssociationCallback(
                any(DeviceAssociationCallback.class), any(Executor.class));
    }

    @Test
    public void unregisterDeviceAssociationCallback_mirrorsConnectedDeviceManager() {
        IDeviceAssociationCallback associationCallback = createDeviceAssociationCallback(
                new Semaphore(0));
        mBinder.registerDeviceAssociationCallback(associationCallback);
        mBinder.unregisterDeviceAssociationCallback(associationCallback);
        verify(mMockConnectedDeviceManager).unregisterDeviceAssociationCallback(
                any(DeviceAssociationCallback.class));
    }

    @NonNull
    private IConnectionCallback createConnectionCallback(@NonNull final Semaphore semaphore) {
        return spy(new IConnectionCallback.Stub() {
            @Override
            public void onDeviceConnected(CompanionDevice companionDevice) {
                semaphore.release();
            }

            @Override
            public void onDeviceDisconnected(CompanionDevice companionDevice) {
                semaphore.release();
            }
        });
    }

    @NonNull
    private IDeviceCallback createDeviceCallback(@NonNull final Semaphore semaphore) {
        return spy(new IDeviceCallback.Stub() {
            @Override
            public void onSecureChannelEstablished(CompanionDevice companionDevice) {
                semaphore.release();
            }

            @Override
            public void onMessageReceived(CompanionDevice companionDevice, byte[] message) {
                semaphore.release();
            }

            @Override
            public void onDeviceError(CompanionDevice companionDevice, int error) {
                semaphore.release();
            }
        });
    }

    @NonNull
    private IDeviceAssociationCallback createDeviceAssociationCallback(
            @NonNull final Semaphore semaphore) {
        return spy(new IDeviceAssociationCallback.Stub() {
            @Override
            public void onAssociatedDeviceAdded(AssociatedDevice device) {
                semaphore.release();
            }

            @Override
            public void onAssociatedDeviceRemoved(AssociatedDevice device) {
                semaphore.release();
            }

            @Override
            public void onAssociatedDeviceUpdated(AssociatedDevice device) {
                semaphore.release();
            }
        });
    }
}
