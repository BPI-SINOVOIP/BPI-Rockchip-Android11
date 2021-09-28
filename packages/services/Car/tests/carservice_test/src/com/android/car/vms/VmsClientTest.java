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

import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.testng.Assert.assertThrows;

import static java.util.Collections.emptySet;

import android.car.Car;
import android.car.vms.VmsAssociatedLayer;
import android.car.vms.VmsAvailableLayers;
import android.car.vms.VmsClient;
import android.car.vms.VmsClientManager;
import android.car.vms.VmsClientManager.VmsClientCallback;
import android.car.vms.VmsLayer;
import android.car.vms.VmsLayerDependency;
import android.car.vms.VmsSubscriptionState;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import com.android.car.MockedCarTestBase;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class VmsClientTest extends MockedCarTestBase {
    private static final long CONNECT_TIMEOUT = 10_000;

    private static final byte[] PROVIDER_DESC1 = {1, 2, 3, 4, 5};
    private static final byte[] PROVIDER_DESC2 = {5, 4, 3, 2, 1};

    private static final VmsAvailableLayers DEFAULT_AVAILABLE_LAYERS =
            new VmsAvailableLayers(0, emptySet());
    private static final VmsSubscriptionState DEFAULT_SUBSCRIPTION_STATE =
            new VmsSubscriptionState(0, emptySet(), emptySet());

    private static final VmsLayer LAYER1 = new VmsLayer(1, 1, 1);
    private static final VmsLayer LAYER2 = new VmsLayer(2, 1, 1);
    private static final VmsLayer LAYER3 = new VmsLayer(3, 1, 1);

    private static final byte[] PAYLOAD = {1, 2, 3, 4, 5, 6, 7, 8};
    private static final byte[] LARGE_PAYLOAD = new byte[16 * 1024]; // 16KB

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock
    private VmsClientCallback mClientCallback1;
    @Captor
    private ArgumentCaptor<VmsClient> mClientCaptor;
    @Mock
    private VmsClientCallback mClientCallback2;

    private final ExecutorService mExecutor = Executors.newSingleThreadExecutor();

    private VmsClientManager mClientManager;

    @Before
    public void setUpTest() {
        mClientManager = (VmsClientManager) getCar().getCarManager(Car.VEHICLE_MAP_SERVICE);
        LARGE_PAYLOAD[0] = 123;
    }

    @Test
    public void testRegister() {
        VmsClient client = connectVmsClient(mClientCallback1);

        awaitTaskCompletion();
        assertThat(client.getAvailableLayers()).isEqualTo(DEFAULT_AVAILABLE_LAYERS);
        assertThat(client.getSubscriptionState()).isEqualTo(DEFAULT_SUBSCRIPTION_STATE);
        verifyLayerAvailability(mClientCallback1, DEFAULT_AVAILABLE_LAYERS);
        verifySubscriptionState(mClientCallback1, DEFAULT_SUBSCRIPTION_STATE);
    }

    @Test
    public void testRegister_ReceivesCurrentLayerAvailabilityAndSubscriptions() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)));
        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        VmsClient client2 = connectVmsClient(mClientCallback2);

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1,
                        asSet(providerId)))
        );
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345)))
        );
        assertThat(client2.getAvailableLayers()).isEqualTo(expectedLayers);
        assertThat(client2.getSubscriptionState()).isEqualTo(expectedSubscriptions);
        verify(mClientCallback2).onLayerAvailabilityChanged(expectedLayers);
        verify(mClientCallback2).onSubscriptionStateChanged(expectedSubscriptions);
    }

    @Test
    public void testRegisterProvider_SameIdForSameInfo() {
        VmsClient client = connectVmsClient(mClientCallback1);

        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC1);

        assertThat(providerId).isEqualTo(providerId2);
    }

    @Test
    public void testRegisterProvider_SameIdForSameInfo_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client2.registerProvider(PROVIDER_DESC1);

        assertThat(providerId).isEqualTo(providerId2);
    }

    @Test
    public void testRegisterProvider_DifferentIdForDifferentInfo() {
        VmsClient client = connectVmsClient(mClientCallback1);

        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        assertThat(providerId).isNotEqualTo(providerId2);
    }

    @Test
    public void testUnregisterProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));

        client.unregisterProvider(providerId);

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testUnregisterProvider_UnknownId() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.unregisterProvider(12345);
    }

    @Test
    public void testGetProviderDescription_UnknownId() {
        VmsClient client = connectVmsClient(mClientCallback1);

        assertThat(client.getProviderDescription(12345)).isNull();
    }

    @Test
    public void testGetProviderDescription_RegisteredProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);

        int providerId = client.registerProvider(PROVIDER_DESC1);

        assertThat(client.getProviderDescription(providerId)).isEqualTo(PROVIDER_DESC1);
    }

    @Test
    public void testSetSubscriptions() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_OverwriteSubscription() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER2),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_OverwriteSubscription_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER3, emptySet())
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1, LAYER3),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        client2.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_MultipleClients_SameLayer() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_OnUnregister_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback2);

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_RemoveSubscription_OnUnregister_MultipleClients_SameLayer() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback2);

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayers() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1, LAYER2),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayers_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER3, emptySet())
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER1, LAYER2, LAYER3),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER3, asSet(98765))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(
                        new VmsAssociatedLayer(LAYER1, asSet(12345)),
                        new VmsAssociatedLayer(LAYER3, asSet(98765))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription_MultipleClients_SameLayer() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(98765))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 98765))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_OverwriteSubscription_MultipleClients_SameLayerAndProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        client2.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_MultipleClients_SameLayer() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        client2.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_MultipleClients_SameLayerAndProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnUnregister_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback2);

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnUnregister_MultipleClients_SameLayer() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback2);

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndProvider_RemoveSubscription_OnUnregister_MultipleClients_SameLayerAndProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback2);

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndMultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerAndMultipleProviders_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER3, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER1, LAYER3),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_OverwriteSubscription() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                asSet(LAYER2),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_OverwriteSubscription_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER3, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(54321))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1, LAYER3),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345, 54321))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_RemoveSubscription() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        client.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_RemoveSubscription_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        client2.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndProvider_RemoveSubscription_OnUnregister_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback2);

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                asSet(LAYER1),
                asSet(new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndMultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(54321)),
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                emptySet(),
                asSet(
                        new VmsAssociatedLayer(LAYER1, asSet(54321)),
                        new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_MultipleLayersAndMultipleProviders_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(54321))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(
                        new VmsAssociatedLayer(LAYER1, asSet(54321)),
                        new VmsAssociatedLayer(LAYER2, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(1,
                asSet(LAYER1),
                emptySet());
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider_RemoveLayerSubscription() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet()),
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(2,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
    }

    @Test
    public void testSetSubscriptions_LayerOnlySupersedesLayerAndProvider_RemoveLayerSubscription_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(12345))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(emptySet());

        awaitTaskCompletion();
        VmsSubscriptionState expectedSubscriptions = new VmsSubscriptionState(3,
                emptySet(),
                asSet(new VmsAssociatedLayer(LAYER1, asSet(12345))));
        verifySubscriptionState(mClientCallback1, expectedSubscriptions);
        verifySubscriptionState(mClientCallback2, expectedSubscriptions);
    }

    @Test
    public void testSetMonitoringEnabled_Enable_NoSubscriptionChange() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setMonitoringEnabled(true);

        awaitTaskCompletion();
        verifySubscriptionState(mClientCallback1, DEFAULT_SUBSCRIPTION_STATE);
    }

    @Test
    public void testSetMonitoringEnabled_Disable_NoSubscriptionChange() {
        VmsClient client = connectVmsClient(mClientCallback1);

        client.setMonitoringEnabled(false);

        awaitTaskCompletion();
        verifySubscriptionState(mClientCallback1, DEFAULT_SUBSCRIPTION_STATE);
    }

    @Test
    public void testSetProviderOfferings_UnknownProviderId() {
        VmsClient client = connectVmsClient(mClientCallback1);

        assertThrows(
                IllegalArgumentException.class,
                () -> client.setProviderOfferings(12345, emptySet()));
    }

    @Test
    public void testSetProviderOfferings_OtherClientsProviderId() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        VmsClient client2 = connectVmsClient(mClientCallback2);

        assertThrows(
                IllegalArgumentException.class,
                () -> client2.setProviderOfferings(providerId, emptySet()));
    }

    @Test
    public void testSetProviderOfferings_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1),
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_MultipleLayers_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_OverwriteOffering_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId, emptySet());

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId2, emptySet());

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId2, emptySet());

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(3, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId, emptySet());

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        // Register second client to verify layer availability after first client disconnects
        connectVmsClient(mClientCallback2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback1);

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        // Register second client to verify layer availability after first client disconnects
        connectVmsClient(mClientCallback2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback1);

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers1 = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId, providerId2)))
        );
        VmsAvailableLayers expectedLayers2 = new VmsAvailableLayers(3, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers1);
        verifyLayerAvailability(mClientCallback2, expectedLayers2);
    }

    @Test
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback1);

        awaitTaskCompletion();
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
    public void testSetProviderOfferings_RemoveOfferings_OnUnregister_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        mClientManager.unregisterVmsClientCallback(mClientCallback1);

        awaitTaskCompletion();
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
    public void testSetProviderOfferings_DependencyMet_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2),
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_MultipleDependencies_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER3)),
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId2)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyMet_ChainedDependencies_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId)),
                new VmsAssociatedLayer(LAYER2, asSet(providerId)),
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_MultipleDependencies_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER3)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyCircular_ChainedDependencies_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER3, asSet(LAYER1))
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3)),
                new VmsLayerDependency(LAYER2)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, asSet(
                new VmsAssociatedLayer(LAYER2, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3))
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3))
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER3, asSet(providerId2)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_MultipleDependencies_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2, LAYER3))
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER3)
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, asSet(
                new VmsAssociatedLayer(LAYER3, asSet(providerId)))
        );
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2)),
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(1, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_MultipleProviders() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_MultipleClients() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        int providerId2 = client2.registerProvider(PROVIDER_DESC2);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client2.setProviderOfferings(providerId2, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testSetProviderOfferings_DependencyUnmet_ChainedDependencies_MultipleClients_SingleProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        VmsClient client2 = connectVmsClient(mClientCallback2);
        client2.registerProvider(PROVIDER_DESC1);

        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1, asSet(LAYER2))
        ));
        client2.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER2, asSet(LAYER3))
        ));

        awaitTaskCompletion();
        VmsAvailableLayers expectedLayers = new VmsAvailableLayers(2, emptySet());
        verifyLayerAvailability(mClientCallback1, expectedLayers);
        verifyLayerAvailability(mClientCallback2, expectedLayers);
    }

    @Test
    public void testPublishPacket_UnknownOffering() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        assertThrows(
                IllegalArgumentException.class,
                () -> client.publishPacket(providerId, LAYER1, PAYLOAD));
    }

    @Test
    public void testPublishPacket_NoSubscribers() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MonitorSubscriber_Enabled() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setMonitoringEnabled(true);
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MonitorSubscriber_EnabledAndDisabled() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setMonitoringEnabled(true);
        client.setMonitoringEnabled(false);
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerSubscriber() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerSubscriber_Unsubscribe() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client.setSubscriptions(emptySet());
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerSubscriber_DifferentLayer() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MultipleLayerSubscribers() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyPacketReceived(mClientCallback2, providerId, LAYER1, PAYLOAD);
    }

    @Test
    public void testPublishPacket_LayerAndProviderSubscriber() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerAndProviderSubscriber_Unsubscribe() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        client.setSubscriptions(emptySet());
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_LayerAndProviderSubscriber_DifferentProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId2))
        ));
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_MultipleLayerAndProviderSubscribers() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        VmsClient client2 = connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        client2.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        client.publishPacket(providerId, LAYER1, PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, PAYLOAD);
        verifyPacketReceived(mClientCallback2, providerId, LAYER1, PAYLOAD);
    }

    @Test
    public void testPublishPacket_Large_UnknownOffering() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);

        assertThrows(
                IllegalArgumentException.class,
                () -> client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD));
    }

    @Test
    public void testPublishPacket_Large_NoSubscribers() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_MonitorSubscriber_Enabled() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setMonitoringEnabled(true);
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, LARGE_PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_MonitorSubscriber_EnabledAndDisabled() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setMonitoringEnabled(true);
        client.setMonitoringEnabled(false);
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_LayerSubscriber() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, LARGE_PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_LayerSubscriber_Unsubscribe() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, emptySet())
        ));
        client.setSubscriptions(emptySet());
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_LayerSubscriber_DifferentLayer() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER2, emptySet())
        ));
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_LayerAndProviderSubscriber() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyPacketReceived(mClientCallback1, providerId, LAYER1, LARGE_PAYLOAD);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_LayerAndProviderSubscriber_Unsubscribe() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId))
        ));
        client.setSubscriptions(emptySet());
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    @Test
    public void testPublishPacket_Large_LayerAndProviderSubscriber_DifferentProvider() {
        VmsClient client = connectVmsClient(mClientCallback1);
        int providerId = client.registerProvider(PROVIDER_DESC1);
        int providerId2 = client.registerProvider(PROVIDER_DESC2);
        client.setProviderOfferings(providerId, asSet(
                new VmsLayerDependency(LAYER1)
        ));
        connectVmsClient(mClientCallback2);

        client.setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER1, asSet(providerId2))
        ));
        client.publishPacket(providerId, LAYER1, LARGE_PAYLOAD);

        awaitTaskCompletion();
        verifyNoPacketsReceived(mClientCallback1, providerId, LAYER1);
        verifyNoPacketsReceived(mClientCallback2, providerId, LAYER1);
    }

    private VmsClient connectVmsClient(VmsClientCallback callback) {
        mClientManager.registerVmsClientCallback(mExecutor, callback);
        verify(callback, timeout(CONNECT_TIMEOUT)).onClientConnected(mClientCaptor.capture());
        return mClientCaptor.getValue();
    }

    private void awaitTaskCompletion() {
        mExecutor.shutdown();
        try {
            mExecutor.awaitTermination(2L, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            fail("Executor termination interrupted");
        }
    }

    private static void verifyLayerAvailability(
            VmsClientCallback callback,
            VmsAvailableLayers availableLayers) {
        ArgumentCaptor<VmsAvailableLayers> availableLayersCaptor =
                ArgumentCaptor.forClass(VmsAvailableLayers.class);
        verify(callback, times(availableLayers.getSequenceNumber() + 1))
                .onLayerAvailabilityChanged(availableLayersCaptor.capture());
        assertThat(availableLayersCaptor.getValue()).isEqualTo(availableLayers);
    }

    private static void verifySubscriptionState(
            VmsClientCallback callback,
            VmsSubscriptionState subscriptionState) {
        ArgumentCaptor<VmsSubscriptionState> subscriptionStateCaptor =
                ArgumentCaptor.forClass(VmsSubscriptionState.class);
        verify(callback, times(subscriptionState.getSequenceNumber() + 1))
                .onSubscriptionStateChanged(subscriptionStateCaptor.capture());
        assertThat(subscriptionStateCaptor.getValue()).isEqualTo(subscriptionState);
    }

    private static void verifyNoPacketsReceived(
            VmsClientCallback callback,
            int providerId, VmsLayer layer) {
        verify(callback, never()).onPacketReceived(eq(providerId), eq(layer), any());
    }

    private static void verifyPacketReceived(
            VmsClientCallback callback,
            int providerId, VmsLayer layer, byte[] payload) {
        verify(callback).onPacketReceived(providerId, layer, payload);
    }

    private static <T> Set<T> asSet(T... values) {
        return new HashSet<T>(Arrays.asList(values));
    }
}
