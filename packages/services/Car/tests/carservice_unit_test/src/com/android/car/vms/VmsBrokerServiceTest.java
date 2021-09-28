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

package com.android.car.vms;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import static java.util.Arrays.asList;
import static java.util.Collections.emptySet;

import android.car.vms.IVmsClientCallback;
import android.car.vms.VmsAssociatedLayer;
import android.car.vms.VmsAvailableLayers;
import android.car.vms.VmsLayer;
import android.car.vms.VmsLayerDependency;
import android.car.vms.VmsProviderInfo;
import android.car.vms.VmsRegistrationInfo;
import android.car.vms.VmsSubscriptionState;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.SharedMemory;
import android.os.UserHandle;
import android.util.ArrayMap;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import com.android.car.stats.CarStatsService;
import com.android.car.stats.VmsClientLogger;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

@RunWith(MockitoJUnitRunner.class)
@SmallTest
public class VmsBrokerServiceTest {
    private static final int USER_ID = 10;

    private static final String TEST_PACKAGE1 = "test.package1";
    private static final String TEST_PACKAGE2 = "test.package2";

    private static final int TEST_APP_ID1 = 12345;
    private static final int TEST_APP_ID2 = 54321;
    private static final int TEST_APP_UID1 = UserHandle.getUid(USER_ID, TEST_APP_ID1);
    private static final int TEST_APP_UID2 = UserHandle.getUid(USER_ID, TEST_APP_ID2);
    private static final int NO_SUBSCRIBERS_UID = -1;

    private static final VmsProviderInfo PROVIDER_INFO1 =
            new VmsProviderInfo(new byte[]{1, 2, 3, 4, 5});
    private static final VmsProviderInfo PROVIDER_INFO2 =
            new VmsProviderInfo(new byte[]{5, 4, 3, 2, 1});

    private static final VmsAvailableLayers DEFAULT_AVAILABLE_LAYERS =
            new VmsAvailableLayers(0, emptySet());
    private static final VmsSubscriptionState DEFAULT_SUBSCRIPTION_STATE =
            new VmsSubscriptionState(0, emptySet(), emptySet());

    private static final VmsLayer LAYER1 = new VmsLayer(1, 1, 1);
    private static final VmsLayer LAYER2 = new VmsLayer(2, 1, 1);
    private static final VmsLayer LAYER3 = new VmsLayer(3, 1, 1);

    private static final byte[] PAYLOAD = {1, 2, 3, 4, 5, 6, 7, 8};
    private static final int LARGE_PACKET_SIZE = 54321;

    @Mock
    private Context mContext;
    @Mock
    private PackageManager mPackageManager;
    @Mock
    private CarStatsService mStatsService;

    @Mock
    private IVmsClientCallback mClientCallback1;
    @Mock
    private IBinder mClientBinder1;
    @Mock
    private VmsClientLogger mClientLog1;

    @Mock
    private IVmsClientCallback mClientCallback2;
    @Mock
    private IBinder mClientBinder2;
    @Mock
    private VmsClientLogger mClientLog2;

    @Mock
    private VmsClientLogger mNoSubscribersLog;

    private final IBinder mClientToken1 = new Binder();
    private final IBinder mClientToken2 = new Binder();
    private SharedMemory mLargePacket;

    private VmsBrokerService mBrokerService;
    private int mCallingAppUid;
    private Map<IBinder /* token */, Pair<IBinder /* callback */, IBinder.DeathRecipient>>
            mDeathRecipients = new ArrayMap<>();

    @Before
    public void setUp() throws Exception {
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        mBrokerService = new VmsBrokerService(mContext, mStatsService, () -> mCallingAppUid);

        when(mPackageManager.getNameForUid(TEST_APP_UID1)).thenReturn(TEST_PACKAGE1);
        when(mPackageManager.getNameForUid(TEST_APP_UID2)).thenReturn(TEST_PACKAGE2);

        when(mStatsService.getVmsClientLogger(TEST_APP_UID1)).thenReturn(mClientLog1);
        when(mStatsService.getVmsClientLogger(TEST_APP_UID2)).thenReturn(mClientLog2);
        when(mStatsService.getVmsClientLogger(NO_SUBSCRIBERS_UID)).thenReturn(mNoSubscribersLog);

        when(mClientCallback1.asBinder()).thenReturn(mClientBinder1);
        when(mClientCallback2.asBinder()).thenReturn(mClientBinder2);

        mCallingAppUid = TEST_APP_UID1;
    }

    // Used by PublishLargePacket tests
    private void setupLargePacket() throws Exception {
        mLargePacket = Mockito.spy(SharedMemory.create("VmsBrokerServiceTest", LARGE_PACKET_SIZE));
    }

    @After
    public void tearDown() {
        if (mLargePacket != null) {
            mLargePacket.close();
        }
    }

    @Test
    public void testRegister() {
        VmsRegistrationInfo registrationInfo =
                registerClient(mClientToken1, mClientCallback1, false);

        verify(mClientLog1).logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);
        assertThat(registrationInfo.getAvailableLayers()).isEqualTo(DEFAULT_AVAILABLE_LAYERS);
        assertThat(registrationInfo.getSubscriptionState()).isEqualTo(DEFAULT_SUBSCRIPTION_STATE);
    }

    @Test
    public void testRegister_DeadCallback() throws Exception {
        doThrow(RemoteException.class).when(mClientBinder1).linkToDeath(any(), anyInt());
        assertThrows(
                IllegalStateException.class,
                () -> registerClient(mClientToken1, mClientCallback1));

        verify(mClientLog1).logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);
        verify(mClientLog1).logConnectionState(VmsClientLogger.ConnectionState.DISCONNECTED);
    }

    @Test
    public void testRegister_LegacyClient() {
        VmsRegistrationInfo registrationInfo =
                registerClient(mClientToken1, mClientCallback1, true);

        verify(mClientLog1).logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);
        assertThat(registrationInfo.getAvailableLayers()).isEqualTo(DEFAULT_AVAILABLE_LAYERS);
        assertThat(registrationInfo.getSubscriptionState()).isEqualTo(DEFAULT_SUBSCRIPTION_STATE);
    }

    @Test
    public void testRegister_LegacyClient_DeadCallback() throws Exception {
        doThrow(RemoteException.class).when(mClientBinder1).linkToDeath(any(), anyInt());
        assertThrows(
                IllegalStateException.class,
                () -> registerClient(mClientToken1, mClientCallback1, true));

        verify(mClientLog1).logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);
        verify(mClientLog1).logConnectionState(VmsClientLogger.ConnectionState.DISCONNECTED);
    }

    @Test
    public void testRegister_TwoClients_OneProcess() {
        VmsRegistrationInfo registrationInfo =
                registerClient(mClientToken1, mClientCallback1, false);
        VmsRegistrationInfo registrationInfo2 =
                registerClient(mClientToken2, mClientCallback2, false);

        verify(mClientLog1, times(2))
                .logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);
        assertThat(registrationInfo).isEqualTo(registrationInfo2);
    }

    @Test
    public void testRegister_TwoClients_TwoProcesses() {
        VmsRegistrationInfo registrationInfo =
                registerClient(mClientToken1, mClientCallback1, false);
        mCallingAppUid = TEST_APP_UID2;
        VmsRegistrationInfo registrationInfo2 =
                registerClient(mClientToken2, mClientCallback2, false);

        verify(mClientLog1).logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);
        verify(mClientLog2).logConnectionState(VmsClientLogger.ConnectionState.CONNECTED);
        assertThat(registrationInfo).isEqualTo(registrationInfo2);
    }

    @Test
    public void testRegister_ReceivesCurrentLayerAvailabilityAndSubscriptions() {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)));
        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));

        VmsRegistrationInfo registrationInfo =
                registerClient(mClientToken2, mClientCallback2, false);

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1,
                        asSet(providerId)))
        );
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345)))
        );
        VmsRegistrationInfo expectedRegistrationInfo =
                new VmsRegistrationInfo(expectedLayers, expectedSubscriptions);
        assertThat(registrationInfo).isEqualTo(expectedRegistrationInfo);
    }

    @Test
    public void testUnregisterClient_UnknownClient() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        mBrokerService.unregisterClient(mClientToken2);

        verify(mClientCallback1, never()).onLayerAvailabilityChanged(any());
        verify(mClientCallback1, never()).onSubscriptionStateChanged(any());
    }

    @Test
    public void testRegisterProvider_UnknownClient() {
        assertThrows(
                IllegalStateException.class,
                () -> mBrokerService.registerProvider(new Binder(), PROVIDER_INFO1));
    }

    @Test
    public void testRegisterProvider_SameIdForSameInfo() {
        registerClient(mClientToken1, mClientCallback1);

        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThat(providerId).isEqualTo(providerId2);
    }

    @Test
    public void testRegisterProvider_SameIdForSameInfo_MultipleClients() {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        assertThat(providerId).isEqualTo(providerId2);
    }

    @Test
    public void testRegisterProvider_DifferentIdForDifferentInfo() {
        registerClient(mClientToken1, mClientCallback1);

        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        assertThat(providerId).isNotEqualTo(providerId2);
    }

    @Test
    public void testGetProviderInfo_UnknownClient() {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThrows(
                IllegalStateException.class,
                () -> mBrokerService.getProviderInfo(new Binder(), providerId));
    }

    @Test
    public void testGetProviderInfo_UnknownId() {
        registerClient(mClientToken1, mClientCallback1);

        assertThat(mBrokerService.getProviderInfo(mClientToken1, 12345).getDescription()).isNull();
    }

    @Test
    public void testGetProviderInfo_RegisteredProvider() {
        registerClient(mClientToken1, mClientCallback1);

        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThat(mBrokerService.getProviderInfo(mClientToken1, providerId))
                .isEqualTo(PROVIDER_INFO1);
    }

    @Test
    public void testSetSubscriptions_UnknownClient() {
        assertThrows(
                IllegalStateException.class,
                () -> mBrokerService.setSubscriptions(new Binder(), asList()));
    }

    @Test
    public void testSetSubscriptions() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_OverwriteSubscription() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER2),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_OverwriteSubscription_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER3, emptySet())
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1, LAYER3),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_MultipleClients_SameLayer()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_OnUnregister_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        unregisterClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_OnUnregister_MultipleClients_SameLayer()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        unregisterClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_OnDisconnect_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        disconnectClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_OnDisconnect_MultipleClients_SameLayer()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        disconnectClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }


    @Test
    public void testSetSubscriptions_MultipleLayers() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1, LAYER2),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayers_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER3, emptySet())
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER1, LAYER2, LAYER3),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER3, asSet(98765))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(
                        new VmsAssociatedLayer(LAYER1, asSet(12345)),
                        new VmsAssociatedLayer(LAYER3, asSet(98765))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription_MultipleClients_SameLayer()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(98765))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 98765))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription_MultipleClients_SameLayerAndProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_MultipleClients_SameLayer()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_MultipleClients_SameLayerAndProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnUnregister_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        unregisterClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnUnregister_MultipleClients_SameLayer()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        unregisterClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnUnregister_MultipleClients_SameLayerAndProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        unregisterClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnDisconnect_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        disconnectClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnDisconnect_MultipleClients_SameLayer()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        disconnectClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnDisconnect_MultipleClients_SameLayerAndProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        disconnectClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndMultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndMultipleProviders_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER3, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER1, LAYER3),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_OverwriteSubscription()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER2),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_OverwriteSubscription_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER3, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1, LAYER3),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_RemoveSubscription()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_RemoveSubscription_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_RemoveSubscription_OnUnregister_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        unregisterClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_RemoveSubscription_OnDisconnect_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        disconnectClient(mClientToken2);

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndMultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(54321)),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(
                        new VmsAssociatedLayer(LAYER1, asSet(54321)),
                        new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndMultipleProviders_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(
                        new VmsAssociatedLayer(LAYER1, asSet(54321)),
                        new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider_RemoveLayerSubscription()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider_RemoveLayerSubscription_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList());

        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetMonitoringEnabled_UnknownClient() {
        assertThrows(
                IllegalStateException.class,
                () -> mBrokerService.setMonitoringEnabled(new Binder(), true));
    }

    @Test
    public void testSetMonitoringEnabled_Enable_NoSubscriptionChange() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setMonitoringEnabled(mClientToken1, true);

        verify(mClientCallback1, never()).onSubscriptionStateChanged(any());
    }

    @Test
    public void testSetMonitoringEnabled_Disable_NoSubscriptionChange() throws Exception {
        registerClient(mClientToken1, mClientCallback1);

        mBrokerService.setMonitoringEnabled(mClientToken1, false);

        verify(mClientCallback1, never()).onSubscriptionStateChanged(any());
    }

    @Test
    public void testSetProviderOfferings_UnknownClient() {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThrows(
                IllegalStateException.class,
                () -> mBrokerService.setProviderOfferings(new Binder(), providerId, asList()));
    }

    @Test
    public void testSetProviderOfferings_UnknownProviderId() {
        registerClient(mClientToken1, mClientCallback1);

        assertThrows(
                IllegalArgumentException.class,
                () -> mBrokerService.setProviderOfferings(mClientToken1, 12345, asList()));
    }

    @Test
    public void testSetProviderOfferings_UnknownProviderId_LegacyClient() throws Exception {
        registerClient(mClientToken1, mClientCallback1, true);

        mBrokerService.setProviderOfferings(mClientToken1, 12345, asList(
                new VmsLayerDependency(LAYER1)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OtherClientsProviderId() {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        registerClient(mClientToken2, mClientCallback2);

        assertThrows(
                IllegalArgumentException.class,
                () -> mBrokerService.setProviderOfferings(mClientToken2, providerId, asList()));
    }

    @Test
    public void testSetProviderOfferings_OtherClientsProviderId_LegacyClient() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        registerClient(mClientToken2, mClientCallback2, true);

        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleClients_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1),
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_MultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_MultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList());

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_MultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList());

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList());

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList());

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        // Register second client to verify layer availability after first client disconnects
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        unregisterClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        // Register second client to verify layer availability after first client disconnects
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        unregisterClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(3, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        unregisterClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        disconnectClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnDisconnect_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        // Register second client to verify layer availability after first client disconnects
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        disconnectClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnDisconnect_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        // Register second client to verify layer availability after first client disconnects
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        disconnectClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(3, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnDisconnect_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER1)
        ));
        disconnectClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnDisconnect_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        disconnectClient(mClientToken1);

        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2),
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER3)),
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleProviders() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleClients() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER3)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_SingleProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3))
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER3)
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_MultipleProviders()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken1, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_MultipleClients()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        int providerId2 = mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId2, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_MultipleClients_SingleProvider()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        registerClient(mClientToken2, mClientCallback2);
        mBrokerService.registerProvider(mClientToken2, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        mBrokerService.setProviderOfferings(mClientToken2, providerId, asList(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testPublishPacket_UnknownClient() {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThrows(
                IllegalStateException.class,
                () -> mBrokerService.publishPacket(new Binder(), providerId, LAYER1, PAYLOAD));
    }

    @Test
    public void testPublishPacket_UnknownOffering() {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThrows(
                IllegalArgumentException.class,
                () -> mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD));
    }

    @Test
    public void testPublishPacket_UnknownOffering_LegacyClient() throws Exception {
        registerClient(mClientToken1, mClientCallback1, true);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishPacket(mClientToken1, 12345, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, 12345, LAYER1, PAYLOAD);
    }


    @Test
    public void testPublishPacket_NoSubscribers() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, PAYLOAD.length);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MonitorSubscriber_Enabled() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setMonitoringEnabled(mClientToken1, true);
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MonitorSubscriber_EnabledAndDisabled() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setMonitoringEnabled(mClientToken1, true);
        mBrokerService.setMonitoringEnabled(mClientToken1, false);
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, PAYLOAD.length);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerSubscriber() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerSubscriber_Unsubscribe() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList());
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, PAYLOAD.length);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerSubscriber_DifferentLayer() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, PAYLOAD.length);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MultipleLayerSubscribers() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1, times(2)).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyPacketReceived(mClientCallback2, providerId, LAYER1, PAYLOAD);
    }

    @Test
    public void testPublishPacket_MultipleLayerSubscribers_DifferentProcesses() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mCallingAppUid = TEST_APP_UID2;
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1).logPacketReceived(LAYER1, PAYLOAD.length);
        verify(mClientLog2).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyPacketReceived(mClientCallback2, providerId, LAYER1, PAYLOAD);
    }

    @Test
    public void testPublishPacket_LayerAndProviderSubscriber() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerAndProviderSubscriber_Unsubscribe() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList());
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, PAYLOAD.length);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerAndProviderSubscriber_DifferentProvider() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId2))
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, PAYLOAD.length);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MultipleLayerAndProviderSubscribers() throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1, times(2)).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyPacketReceived(mClientCallback2, providerId, LAYER1, PAYLOAD);
    }

    @Test
    public void testPublishPacket_MultipleLayerAndProviderSubscribers_DifferentProcesses()
            throws Exception {
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mCallingAppUid = TEST_APP_UID2;
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.publishPacket(mClientToken1, providerId, LAYER1, PAYLOAD);

        verify(mClientLog1).logPacketSent(LAYER1, PAYLOAD.length);
        verify(mClientLog1).logPacketReceived(LAYER1, PAYLOAD.length);
        verify(mClientLog2).logPacketReceived(LAYER1, PAYLOAD.length);
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyPacketReceived(mClientCallback2, providerId, LAYER1, PAYLOAD);
    }

    @Test
    public void testPublishLargePacket_UnknownClient() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThrows(
                IllegalStateException.class,
                () -> mBrokerService.publishLargePacket(
                        new Binder(), providerId, LAYER1, mLargePacket));

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_UnknownOffering() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        assertThrows(
                IllegalArgumentException.class,
                () -> mBrokerService.publishLargePacket(
                        mClientToken1, providerId, LAYER1, mLargePacket));

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_UnknownOffering_LegacyClient() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1, true);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishLargePacket(mClientToken1, 12345, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, 12345, LAYER1, mLargePacket);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_NoSubscribers() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, LARGE_PACKET_SIZE);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_MonitorSubscriber_Enabled() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setMonitoringEnabled(mClientToken1, true);
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, providerId, LAYER1, mLargePacket);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_MonitorSubscriber_EnabledAndDisabled() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setMonitoringEnabled(mClientToken1, true);
        mBrokerService.setMonitoringEnabled(mClientToken1, false);
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, LARGE_PACKET_SIZE);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_LayerSubscriber() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, providerId, LAYER1, mLargePacket);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_LayerSubscriber_Unsubscribe() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList());
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, LARGE_PACKET_SIZE);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_LayerSubscriber_DifferentLayer() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, LARGE_PACKET_SIZE);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_MultipleLayerSubscribers() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1, times(2)).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, providerId, LAYER1, mLargePacket);
        verifyLargePacketReceived(mClientCallback2, providerId, LAYER1, mLargePacket);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_MultipleLayerSubscribers_DifferentProcesses()
            throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mCallingAppUid = TEST_APP_UID2;
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog2).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, providerId, LAYER1, mLargePacket);
        verifyLargePacketReceived(mClientCallback2, providerId, LAYER1, mLargePacket);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_LayerAndProviderSubscriber() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, providerId, LAYER1, mLargePacket);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_LayerAndProviderSubscriber_Unsubscribe() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.setSubscriptions(mClientToken1, asList());
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, LARGE_PACKET_SIZE);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_LayerAndProviderSubscriber_DifferentProvider()
            throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);
        int providerId2 = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO2);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId2))
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mNoSubscribersLog).logPacketDropped(LAYER1, LARGE_PACKET_SIZE);
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_MultipleLayerAndProviderSubscribers() throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1, times(2)).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, providerId, LAYER1, mLargePacket);
        verifyLargePacketReceived(mClientCallback2, providerId, LAYER1, mLargePacket);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    @Test
    public void testPublishLargePacket_MultipleLayerAndProviderSubscribers_DifferentProcesses()
            throws Exception {
        setupLargePacket();
        registerClient(mClientToken1, mClientCallback1);
        int providerId = mBrokerService.registerProvider(mClientToken1, PROVIDER_INFO1);

        mBrokerService.setProviderOfferings(mClientToken1, providerId, asList(
                new VmsLayerDependency(LAYER1)
        ));
        mCallingAppUid = TEST_APP_UID2;
        registerClient(mClientToken2, mClientCallback2);

        mBrokerService.setSubscriptions(mClientToken1, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.setSubscriptions(mClientToken2, asList(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        mBrokerService.publishLargePacket(mClientToken1, providerId, LAYER1, mLargePacket);

        verify(mClientLog1).logPacketSent(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog1).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verify(mClientLog2).logPacketReceived(LAYER1, LARGE_PACKET_SIZE);
        verifyLargePacketReceived(mClientCallback1, providerId, LAYER1, mLargePacket);
        verifyLargePacketReceived(mClientCallback2, providerId, LAYER1, mLargePacket);

        verify(mLargePacket, atLeastOnce()).getSize();
        verify(mLargePacket).close();
        verifyNoMoreInteractions(mLargePacket);
    }

    private void registerClient(IBinder token, IVmsClientCallback callback) {
        registerClient(token, callback, false);
    }

    private VmsRegistrationInfo registerClient(IBinder token, IVmsClientCallback callback,
            boolean legacyClient) {
        VmsRegistrationInfo registrationInfo =
                mBrokerService.registerClient(token, callback, legacyClient);

        IBinder callbackBinder = callback.asBinder();
        try {
            if (!mDeathRecipients.containsKey(token)) {
                ArgumentCaptor<IBinder.DeathRecipient> deathRecipientCaptor =
                        ArgumentCaptor.forClass(IBinder.DeathRecipient.class);
                verify(callbackBinder).linkToDeath(deathRecipientCaptor.capture(), eq(0));
                mDeathRecipients.put(token,
                        Pair.create(callbackBinder, deathRecipientCaptor.getValue()));
            } else {
                verify(callbackBinder, never()).linkToDeath(any(), anyInt());
            }
        } catch (RemoteException e) {
            e.rethrowAsRuntimeException();
        }

        return registrationInfo;
    }

    private void unregisterClient(IBinder token) {
        mBrokerService.unregisterClient(token);

        Pair<IBinder, IBinder.DeathRecipient> deathRecipientPair = mDeathRecipients.get(token);
        assertThat(deathRecipientPair).isNotNull();
        verify(deathRecipientPair.first).unlinkToDeath(same(deathRecipientPair.second), eq(0));
    }

    private void disconnectClient(IBinder token) {
        Pair<IBinder, IBinder.DeathRecipient> deathRecipientPair = mDeathRecipients.get(token);
        assertThat(deathRecipientPair).isNotNull();

        deathRecipientPair.second.binderDied();
        verify(deathRecipientPair.first).unlinkToDeath(same(deathRecipientPair.second), eq(0));
    }

    private static void verifyLayerAvailability(
            IVmsClientCallback callback,
            VmsAvailableLayers availableLayers) throws RemoteException {
        ArgumentCaptor<VmsAvailableLayers> availableLayersCaptor =
                ArgumentCaptor.forClass(VmsAvailableLayers.class);
        verify(callback, times(availableLayers.getSequenceNumber()))
                .onLayerAvailabilityChanged(availableLayersCaptor.capture());
        assertThat(availableLayersCaptor.getValue()).isEqualTo(availableLayers);
    }

    private static void verifySubscriptionState(
            IVmsClientCallback callback,
            VmsSubscriptionState subscriptionState) throws RemoteException {
        ArgumentCaptor<VmsSubscriptionState> subscriptionStateCaptor =
                ArgumentCaptor.forClass(VmsSubscriptionState.class);
        verify(callback, times(subscriptionState.getSequenceNumber()))
                .onSubscriptionStateChanged(subscriptionStateCaptor.capture());
        assertThat(subscriptionStateCaptor.getValue()).isEqualTo(subscriptionState);
    }

    private static void verifyNoPacketsReceived(
            IVmsClientCallback callback,
            int providerId, VmsLayer layer) throws RemoteException {
        verify(callback, never()).onPacketReceived(eq(providerId), eq(layer), any());
        verify(callback, never()).onLargePacketReceived(eq(providerId), eq(layer), any());
    }

    private static void verifyPacketReceived(
            IVmsClientCallback callback,
            int providerId, VmsLayer layer, byte[] payload) throws RemoteException {
        verify(callback).onPacketReceived(providerId, layer, payload);
    }

    private static void verifyLargePacketReceived(
            IVmsClientCallback callback,
            int providerId, VmsLayer layer, SharedMemory packet) throws RemoteException {
        verify(callback).onLargePacketReceived(providerId, layer, packet);
    }

    private static <T> Set<T> asSet(T... values) {
        return new HashSet<T>(Arrays.asList(values));
    }
}
