/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.car.hal;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.car.hardware.property.VehicleHalStatusCode;
import android.car.vms.VmsAssociatedLayer;
import android.car.vms.VmsAvailableLayers;
import android.car.vms.VmsClient;
import android.car.vms.VmsClientManager.VmsClientCallback;
import android.car.vms.VmsLayer;
import android.car.vms.VmsLayerDependency;
import android.car.vms.VmsSubscriptionState;
import android.content.Context;
import android.content.res.Resources;
import android.hardware.automotive.vehicle.V2_0.VehiclePropConfig;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyGroup;
import android.hardware.automotive.vehicle.V2_0.VmsMessageType;
import android.os.Handler;
import android.os.ServiceSpecificException;

import com.android.car.R;
import com.android.car.test.utils.TemporaryFile;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnitRunner;

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(MockitoJUnitRunner.class)
public class VmsHalServiceTest {
    private static final int LAYER_TYPE = 1;
    private static final int LAYER_SUBTYPE = 2;
    private static final int LAYER_VERSION = 3;
    private static final VmsLayer LAYER = new VmsLayer(LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION);
    private static final int PUBLISHER_ID = 12345;
    private static final byte[] PAYLOAD = new byte[]{1, 2, 3, 4};
    private static final List<Byte> PAYLOAD_AS_LIST = Arrays.asList(new Byte[]{1, 2, 3, 4});
    private static final int CORE_ID = 54321;
    private static final int CLIENT_ID = 98765;

    @Mock
    private Context mContext;
    @Mock
    private Resources mResources;
    @Mock
    private VehicleHal mVehicleHal;
    @Mock
    private VmsClient mVmsClient;

    private VmsHalService mHalService;
    private VmsClientCallback mEventCallback;
    private int mVmsInitCount;

    @Before
    public void setUp() throws Exception {
        initHalService(true);
    }

    private void initHalService(boolean propagatePropertyException) throws Exception {
        mVmsInitCount = 0;
        when(mContext.getResources()).thenReturn(mResources);
        mHalService = new VmsHalService(mContext, mVehicleHal, () -> (long) CORE_ID,
                this::initVmsClient, propagatePropertyException);

        VehiclePropConfig propConfig = new VehiclePropConfig();
        propConfig.prop = VehicleProperty.VEHICLE_MAP_SERVICE;
        mHalService.takeProperties(Collections.singleton(propConfig));

        when(mVmsClient.getAvailableLayers()).thenReturn(
                new VmsAvailableLayers(Collections.emptySet(), 0));
        mHalService.init();
        waitForHandlerCompletion();

        // Verify START_SESSION message was sent
        InOrder initOrder = Mockito.inOrder(mVehicleHal);
        initOrder.verify(mVehicleHal).subscribeProperty(mHalService,
                VehicleProperty.VEHICLE_MAP_SERVICE);
        initOrder.verify(mVehicleHal).set(createHalMessage(
                VmsMessageType.START_SESSION, // Message type
                CORE_ID,                      // Core ID
                -1));                          // Client ID (unknown)

        // Verify no more interections until handshake received
        initOrder.verifyNoMoreInteractions();

        // Send START_SESSION response from client
        sendHalMessage(createHalMessage(
                VmsMessageType.START_SESSION,  // Message type
                CORE_ID,                       // Core ID
                CLIENT_ID                      // Client ID
        ));
        waitForHandlerCompletion();

        initOrder.verify(mVehicleHal).set(createHalMessage(
                VmsMessageType.AVAILABILITY_CHANGE, // Message type
                0,                                  // Sequence number
                0));                                // # of associated layers


        waitForHandlerCompletion();
        initOrder.verifyNoMoreInteractions();
        assertEquals(1, mVmsInitCount);
        reset(mVmsClient, mVehicleHal);
    }

    private VmsClient initVmsClient(Handler handler, VmsClientCallback callback) {
        mEventCallback = callback;
        mVmsInitCount++;
        return mVmsClient;
    }

    @Test
    public void testCoreId_IntegerOverflow() throws Exception {
        mHalService = new VmsHalService(mContext, mVehicleHal,
                () -> (long) Integer.MAX_VALUE + CORE_ID, this::initVmsClient, true);

        VehiclePropConfig propConfig = new VehiclePropConfig();
        propConfig.prop = VehicleProperty.VEHICLE_MAP_SERVICE;
        mHalService.takeProperties(Collections.singleton(propConfig));

        when(mVmsClient.getAvailableLayers()).thenReturn(
                new VmsAvailableLayers(Collections.emptySet(), 0));
        mHalService.init();
        waitForHandlerCompletion();

        verify(mVehicleHal).set(createHalMessage(
                VmsMessageType.START_SESSION, // Message type
                CORE_ID,                      // Core ID
                -1));                          // Client ID (unknown)
    }

    @Test
    public void testTakeSupportedProperties() {
        VehiclePropConfig vmsPropConfig = new VehiclePropConfig();
        vmsPropConfig.prop = VehicleProperty.VEHICLE_MAP_SERVICE;

        VehiclePropConfig otherPropConfig = new VehiclePropConfig();
        otherPropConfig.prop = VehicleProperty.CURRENT_GEAR;
        mHalService.takeProperties(Arrays.asList(vmsPropConfig));
    }

    /**
     * DATA message format:
     * <ul>
     * <li>Message type
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Publisher ID
     * <li>Payload
     * </ul>
     */
    @Test
    public void testHandleDataEvent() {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.DATA,                       // Message type
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // VmsLayer
                PUBLISHER_ID                               // PublisherId
        );
        message.value.bytes.addAll(PAYLOAD_AS_LIST);

        sendHalMessage(message);
        verify(mVmsClient).publishPacket(PUBLISHER_ID, LAYER, PAYLOAD);
    }

    @Test
    public void testOnPacketReceivedEvent() throws Exception {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.DATA,                       // Message type
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // VmsLayer
                PUBLISHER_ID                               // PublisherId
        );
        message.value.bytes.addAll(PAYLOAD_AS_LIST);

        mEventCallback.onPacketReceived(PUBLISHER_ID, LAYER, PAYLOAD);
        verify(mVehicleHal).set(message);
    }

    /**
     * SUBSCRIBE message format:
     * <ul>
     * <li>Message type
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * </ul>
     */
    @Test
    public void testHandleSubscribeEvent() {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.SUBSCRIBE,                 // Message type
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION  // VmsLayer
        );

        sendHalMessage(message);
        verify(mVmsClient).setSubscriptions(asSet(new VmsAssociatedLayer(LAYER, asSet())));
    }

    /**
     * SUBSCRIBE_TO_PUBLISHER message format:
     * <ul>
     * <li>Message type
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Publisher ID
     * </ul>
     */
    @Test
    public void testHandleSubscribeToPublisherEvent() {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.SUBSCRIBE_TO_PUBLISHER,     // Message type
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // VmsLayer
                PUBLISHER_ID                               // PublisherId
        );

        sendHalMessage(message);
        verify(mVmsClient).setSubscriptions(asSet(
                new VmsAssociatedLayer(LAYER, asSet(PUBLISHER_ID))));
    }

    /**
     * UNSUBSCRIBE message format:
     * <ul>
     * <li>Message type
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * </ul>
     */
    @Test
    public void testHandleUnsubscribeEvent() {
        testHandleSubscribeEvent();

        VehiclePropValue message = createHalMessage(
                VmsMessageType.UNSUBSCRIBE,               // Message type
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION  // VmsLayer
        );

        sendHalMessage(message);
        verify(mVmsClient).setSubscriptions(asSet());
    }

    /**
     * UNSUBSCRIBE_TO_PUBLISHER message format:
     * <ul>
     * <li>Message type
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Publisher ID
     * </ul>
     */
    @Test
    public void testHandleUnsubscribeFromPublisherEvent() {
        testHandleSubscribeToPublisherEvent();

        VehiclePropValue message = createHalMessage(
                VmsMessageType.UNSUBSCRIBE_TO_PUBLISHER,   // Message type
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // VmsLayer
                PUBLISHER_ID                               // PublisherId
        );

        sendHalMessage(message);
        verify(mVmsClient).setSubscriptions(asSet());
    }

    /**
     * PUBLISHER_ID_REQUEST message format:
     * <ul>
     * <li>Message type
     * <li>Publisher info (bytes)
     * </ul>
     *
     * PUBLISHER_ID_RESPONSE message format:
     * <ul>
     * <li>Message type
     * <li>Publisher ID
     * </ul>
     */
    @Test
    public void testHandlePublisherIdRequestEvent() throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.PUBLISHER_ID_REQUEST  // Message type
        );
        request.value.bytes.addAll(PAYLOAD_AS_LIST);

        when(mVmsClient.registerProvider(PAYLOAD)).thenReturn(PUBLISHER_ID);

        VehiclePropValue response = createHalMessage(
                VmsMessageType.PUBLISHER_ID_RESPONSE,  // Message type
                PUBLISHER_ID                           // Publisher ID
        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    /**
     * PUBLISHER_INFORMATION_REQUEST message format:
     * <ul>
     * <li>Message type
     * <li>Publisher ID
     * </ul>
     *
     * PUBLISHER_INFORMATION_RESPONSE message format:
     * <ul>
     * <li>Message type
     * <li>Publisher info (bytes)
     * </ul>
     */
    @Test
    public void testHandlePublisherInformationRequestEvent() throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.PUBLISHER_INFORMATION_REQUEST,  // Message type
                PUBLISHER_ID                                   // Publisher ID
        );

        when(mVmsClient.getProviderDescription(PUBLISHER_ID)).thenReturn(PAYLOAD);

        VehiclePropValue response = createHalMessage(
                VmsMessageType.PUBLISHER_INFORMATION_RESPONSE  // Message type
        );
        response.value.bytes.addAll(PAYLOAD_AS_LIST);

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    /**
     * OFFERING message format:
     * <ul>
     * <li>Message type
     * <li>Publisher ID
     * <li>Number of offerings.
     * <li>Offerings (x number of offerings)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Number of layer dependencies.
     * <li>Layer dependencies (x number of layer dependencies)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * </ul>
     * </ul>
     * </ul>
     */
    @Test
    public void testHandleOfferingEvent_ZeroOfferings() {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.OFFERING,  // Message type
                PUBLISHER_ID,             // PublisherId
                0                         // # of offerings
        );

        sendHalMessage(message);
        verify(mVmsClient).setProviderOfferings(PUBLISHER_ID, asSet());
    }

    @Test
    public void testHandleOfferingEvent_LayerOnly() {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.OFFERING,                   // Message type
                PUBLISHER_ID,                              // PublisherId
                1,                                         // # of offerings
                // Offered layer
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,
                0                                          // # of dependencies
        );

        sendHalMessage(message);
        verify(mVmsClient).setProviderOfferings(
                PUBLISHER_ID,
                asSet(new VmsLayerDependency(LAYER)));
    }

    @Test
    public void testHandleOfferingEvent_LayerAndDependency() {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.OFFERING,                   // Message type
                PUBLISHER_ID,                              // PublisherId
                1,                                         // # of offerings
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                1,                                         // # of dependencies
                4, 5, 6                                    // Dependency layer
        );

        sendHalMessage(message);
        verify(mVmsClient).setProviderOfferings(
                PUBLISHER_ID,
                asSet(
                        new VmsLayerDependency(LAYER, Collections.singleton(
                                new VmsLayer(4, 5, 6)))));
    }

    @Test
    public void testHandleOfferingEvent_MultipleLayersAndDependencies() {
        VehiclePropValue message = createHalMessage(
                VmsMessageType.OFFERING,                   // Message type
                PUBLISHER_ID,                              // PublisherId
                3,                                         // # of offerings
                // Offered layer #1
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                2,                                         // # of dependencies
                4, 5, 6,                                   // Dependency layer
                7, 8, 9,                                   // Dependency layer
                // Offered layer #2
                3, 2, 1,                                   // Layer
                0,                                         // # of dependencies
                // Offered layer #3
                6, 5, 4,                                   // Layer
                1,                                         // # of dependencies
                7, 8, 9                                    // Dependency layer
        );

        sendHalMessage(message);
        verify(mVmsClient).setProviderOfferings(
                PUBLISHER_ID,
                asSet(
                        new VmsLayerDependency(LAYER, new LinkedHashSet<>(Arrays.asList(
                                new VmsLayer(4, 5, 6),
                                new VmsLayer(7, 8, 9)
                        ))),
                        new VmsLayerDependency(new VmsLayer(3, 2, 1), Collections.emptySet()),
                        new VmsLayerDependency(new VmsLayer(6, 5, 4), Collections.singleton(
                                new VmsLayer(7, 8, 9)
                        ))));
    }

    /**
     * AVAILABILITY_REQUEST message format:
     * <ul>
     * <li>Message type
     * </ul>
     *
     * AVAILABILITY_RESPONSE message format:
     * <ul>
     * <li>Message type
     * <li>Sequence number.
     * <li>Number of associated layers.
     * <li>Associated layers (x number of associated layers)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Number of publishers
     * <li>Publisher ID (x number of publishers)
     * </ul>
     * </ul>
     */
    @Test
    public void testHandleAvailabilityRequestEvent_ZeroLayers() throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.AVAILABILITY_REQUEST  // Message type
        );

        when(mVmsClient.getAvailableLayers()).thenReturn(
                new VmsAvailableLayers(Collections.emptySet(), 123));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.AVAILABILITY_RESPONSE,  // Message type
                123,                                   // Sequence number
                0                                      // # of associated layers
        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testHandleAvailabilityRequestEvent_OneLayer() throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.AVAILABILITY_REQUEST  // Message type
        );

        when(mVmsClient.getAvailableLayers()).thenReturn(
                new VmsAvailableLayers(Collections.singleton(
                        new VmsAssociatedLayer(LAYER, Collections.singleton(PUBLISHER_ID))), 123));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.AVAILABILITY_RESPONSE,      // Message type
                123,                                       // Sequence number
                1,                                         // # of associated layers
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                1,                                         // # of publisher IDs
                PUBLISHER_ID                               // Publisher ID
        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }


    @Test
    public void testHandleAvailabilityRequestEvent_MultipleLayers() throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.AVAILABILITY_REQUEST  // Message type
        );

        when(mVmsClient.getAvailableLayers()).thenReturn(
                new VmsAvailableLayers(new LinkedHashSet<>(Arrays.asList(
                        new VmsAssociatedLayer(LAYER,
                                new LinkedHashSet<>(Arrays.asList(PUBLISHER_ID, 54321))),
                        new VmsAssociatedLayer(new VmsLayer(3, 2, 1),
                                Collections.emptySet()),
                        new VmsAssociatedLayer(new VmsLayer(6, 5, 4),
                                Collections.singleton(99999)))),
                        123));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.AVAILABILITY_RESPONSE,      // Message type
                123,                                       // Sequence number
                3,                                         // # of associated layers
                // Associated layer #1
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                2,                                         // # of publisher IDs
                PUBLISHER_ID,                              // Publisher ID
                54321,                                     // Publisher ID #2
                // Associated layer #2
                3, 2, 1,                                   // Layer
                0,                                         // # of publisher IDs
                // Associated layer #3
                6, 5, 4,                                   // Layer
                1,                                         // # of publisher IDs
                99999                                      // Publisher ID

        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    /**
     * START_SESSION message format:
     * <ul>
     * <li>Message type
     * <li>Core ID
     * <li>Client ID
     * </ul>
     */
    @Test
    public void testHandleStartSessionEvent() throws Exception {
        when(mVmsClient.getAvailableLayers()).thenReturn(
                new VmsAvailableLayers(Collections.emptySet(), 5));

        VehiclePropValue request = createHalMessage(
                VmsMessageType.START_SESSION,  // Message type
                -1,                            // Core ID (unknown)
                CLIENT_ID                      // Client ID
        );

        VehiclePropValue response = createHalMessage(
                VmsMessageType.START_SESSION,  // Message type
                CORE_ID,                               // Core ID
                CLIENT_ID                              // Client ID
        );

        sendHalMessage(request);

        InOrder inOrder = Mockito.inOrder(mVehicleHal, mVmsClient);
        inOrder.verify(mVmsClient).unregister();
        inOrder.verify(mVehicleHal).set(response);
        assertEquals(2, mVmsInitCount);

        waitForHandlerCompletion();
        inOrder.verify(mVehicleHal).set(createHalMessage(
                VmsMessageType.AVAILABILITY_CHANGE, // Message type
                5,                                  // Sequence number
                0));                                // # of associated layers

    }

    /**
     * AVAILABILITY_CHANGE message format:
     * <ul>
     * <li>Message type
     * <li>Sequence number.
     * <li>Number of associated layers.
     * <li>Associated layers (x number of associated layers)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Number of publishers
     * <li>Publisher ID (x number of publishers)
     * </ul>
     * </ul>
     */
    @Test
    public void testOnLayersAvailabilityChanged_ZeroLayers() throws Exception {
        mEventCallback.onLayerAvailabilityChanged(
                new VmsAvailableLayers(Collections.emptySet(), 123));

        VehiclePropValue message = createHalMessage(
                VmsMessageType.AVAILABILITY_CHANGE,    // Message type
                123,                                   // Sequence number
                0                                      // # of associated layers
        );

        waitForHandlerCompletion();
        verify(mVehicleHal).set(message);
    }

    @Test
    public void testOnLayersAvailabilityChanged_OneLayer() throws Exception {
        mEventCallback.onLayerAvailabilityChanged(
                new VmsAvailableLayers(Collections.singleton(
                        new VmsAssociatedLayer(LAYER, Collections.singleton(PUBLISHER_ID))), 123));

        VehiclePropValue message = createHalMessage(
                VmsMessageType.AVAILABILITY_CHANGE,        // Message type
                123,                                       // Sequence number
                1,                                         // # of associated layers
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                1,                                         // # of publisher IDs
                PUBLISHER_ID                               // Publisher ID
        );

        waitForHandlerCompletion();
        verify(mVehicleHal).set(message);
    }


    @Test
    public void testOnLayersAvailabilityChanged_MultipleLayers() throws Exception {
        mEventCallback.onLayerAvailabilityChanged(
                new VmsAvailableLayers(new LinkedHashSet<>(Arrays.asList(
                        new VmsAssociatedLayer(LAYER,
                                new LinkedHashSet<>(Arrays.asList(PUBLISHER_ID, 54321))),
                        new VmsAssociatedLayer(new VmsLayer(3, 2, 1),
                                Collections.emptySet()),
                        new VmsAssociatedLayer(new VmsLayer(6, 5, 4),
                                Collections.singleton(99999)))),
                        123));

        VehiclePropValue message = createHalMessage(
                VmsMessageType.AVAILABILITY_CHANGE,      // Message type
                123,                                       // Sequence number
                3,                                         // # of associated layers
                // Associated layer #1
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                2,                                         // # of publisher IDs
                PUBLISHER_ID,                              // Publisher ID
                54321,                                     // Publisher ID #2
                // Associated layer #2
                3, 2, 1,                                   // Layer
                0,                                         // # of publisher IDs
                // Associated layer #3
                6, 5, 4,                                   // Layer
                1,                                         // # of publisher IDs
                99999                                      // Publisher ID

        );

        waitForHandlerCompletion();
        verify(mVehicleHal).set(message);
    }

    /**
     * SUBSCRIPTION_REQUEST message format:
     * <ul>
     * <li>Message type
     * </ul>
     *
     * SUBSCRIPTION_RESPONSE message format:
     * <ul>
     * <li>Message type
     * <li>Sequence number
     * <li>Number of layers
     * <li>Number of associated layers
     * <li>Layers (x number of layers)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * </ul>
     * <li>Associated layers (x number of associated layers)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Number of publishers
     * <li>Publisher ID (x number of publishers)
     * </ul>
     * </ul>
     */
    @Test
    public void testHandleSubscriptionsRequestEvent_ZeroLayers() throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_REQUEST  // Message type
        );

        when(mVmsClient.getSubscriptionState()).thenReturn(
                new VmsSubscriptionState(123, Collections.emptySet(), Collections.emptySet()));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_RESPONSE,  // Message type
                123,                                    // Sequence number
                0,                                      // # of layers
                0                                       // # of associated layers
        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testHandleSubscriptionsRequestEvent_OneLayer_ZeroAssociatedLayers()
            throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_REQUEST  // Message type
        );

        when(mVmsClient.getSubscriptionState()).thenReturn(
                new VmsSubscriptionState(123, Collections.singleton(LAYER),
                        Collections.emptySet()));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_RESPONSE,     // Message type
                123,                                       // Sequence number
                1,                                         // # of layers
                0,                                         // # of associated layers
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION   // Layer
        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testHandleSubscriptionsRequestEvent_ZeroLayers_OneAssociatedLayer()
            throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_REQUEST  // Message type
        );

        when(mVmsClient.getSubscriptionState()).thenReturn(
                new VmsSubscriptionState(123, Collections.emptySet(), Collections.singleton(
                        new VmsAssociatedLayer(LAYER, Collections.singleton(PUBLISHER_ID)))));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_RESPONSE,     // Message type
                123,                                       // Sequence number
                0,                                         // # of layers
                1,                                         // # of associated layers
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                1,                                         // # of publisher IDs
                PUBLISHER_ID                               // Publisher ID
        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testHandleSubscriptionsRequestEvent_MultipleLayersAndAssociatedLayers()
            throws Exception {
        VehiclePropValue request = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_REQUEST  // Message type
        );

        when(mVmsClient.getSubscriptionState()).thenReturn(
                new VmsSubscriptionState(123,
                        new LinkedHashSet<>(Arrays.asList(
                                LAYER,
                                new VmsLayer(4, 5, 6),
                                new VmsLayer(7, 8, 9)
                        )),
                        new LinkedHashSet<>(Arrays.asList(
                                new VmsAssociatedLayer(LAYER, Collections.emptySet()),
                                new VmsAssociatedLayer(new VmsLayer(6, 5, 4),
                                        new LinkedHashSet<>(Arrays.asList(
                                                PUBLISHER_ID,
                                                54321))))))
        );

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_RESPONSE,     // Message type
                123,                                       // Sequence number
                3,                                         // # of layers
                2,                                         // # of associated layers
                // Layer #1
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                // Layer #2
                4, 5, 6,                                   // Layer
                // Layer #3
                7, 8, 9,                                   // Layer
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                0,                                         // # of publisher IDs
                6, 5, 4,                                   // Layer
                2,                                         // # of publisher IDs
                PUBLISHER_ID,                              // Publisher ID
                54321                                      // Publisher ID #2
        );

        sendHalMessage(request);
        verify(mVehicleHal).set(response);
    }

    /**
     * SUBSCRIPTIONS_CHANGE message format:
     * <ul>
     * <li>Message type
     * <li>Sequence number
     * <li>Number of layers
     * <li>Number of associated layers
     * <li>Layers (x number of layers)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * </ul>
     * <li>Associated layers (x number of associated layers)
     * <ul>
     * <li>Layer ID
     * <li>Layer subtype
     * <li>Layer version
     * <li>Number of publishers
     * <li>Publisher ID (x number of publishers)
     * </ul>
     * </ul>
     */
    @Test
    public void testOnVmsSubscriptionChange_ZeroLayers() throws Exception {
        mEventCallback.onSubscriptionStateChanged(
                new VmsSubscriptionState(123, Collections.emptySet(), Collections.emptySet()));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_CHANGE,    // Message type
                123,                                    // Sequence number
                0,                                      // # of layers
                0                                       // # of associated layers
        );

        waitForHandlerCompletion();
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testOnVmsSubscriptionChange_OneLayer_ZeroAssociatedLayers()
            throws Exception {
        mEventCallback.onSubscriptionStateChanged(
                new VmsSubscriptionState(123, Collections.singleton(LAYER),
                        Collections.emptySet()));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_CHANGE,       // Message type
                123,                                       // Sequence number
                1,                                         // # of layers
                0,                                         // # of associated layers
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION   // Layer
        );

        waitForHandlerCompletion();
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testOnVmsSubscriptionChange_ZeroLayers_OneAssociatedLayer()
            throws Exception {
        mEventCallback.onSubscriptionStateChanged(
                new VmsSubscriptionState(123, Collections.emptySet(), Collections.singleton(
                        new VmsAssociatedLayer(LAYER, Collections.singleton(PUBLISHER_ID)))));

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_CHANGE,       // Message type
                123,                                       // Sequence number
                0,                                         // # of layers
                1,                                         // # of associated layers
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                1,                                         // # of publisher IDs
                PUBLISHER_ID                               // Publisher ID
        );

        waitForHandlerCompletion();
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testOnVmsSubscriptionChange_MultipleLayersAndAssociatedLayers()
            throws Exception {
        mEventCallback.onSubscriptionStateChanged(
                new VmsSubscriptionState(123,
                        new LinkedHashSet<>(Arrays.asList(
                                LAYER,
                                new VmsLayer(4, 5, 6),
                                new VmsLayer(7, 8, 9)
                        )),
                        new LinkedHashSet<>(Arrays.asList(
                                new VmsAssociatedLayer(LAYER, Collections.emptySet()),
                                new VmsAssociatedLayer(new VmsLayer(6, 5, 4),
                                        new LinkedHashSet<>(Arrays.asList(
                                                PUBLISHER_ID,
                                                54321))))))
        );

        VehiclePropValue response = createHalMessage(
                VmsMessageType.SUBSCRIPTIONS_CHANGE,       // Message type
                123,                                       // Sequence number
                3,                                         // # of layers
                2,                                         // # of associated layers
                // Layer #1
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                // Layer #2
                4, 5, 6,                                   // Layer
                // Layer #3
                7, 8, 9,                                   // Layer
                LAYER_TYPE, LAYER_SUBTYPE, LAYER_VERSION,  // Layer
                0,                                         // # of publisher IDs
                6, 5, 4,                                   // Layer
                2,                                         // # of publisher IDs
                PUBLISHER_ID,                              // Publisher ID
                54321                                      // Publisher ID #2
        );

        waitForHandlerCompletion();
        verify(mVehicleHal).set(response);
    }

    @Test
    public void testPropertySetExceptionNotPropagated_CoreStartSession() throws Exception {
        doThrow(new RuntimeException()).when(mVehicleHal).set(any());
        initHalService(false);

        waitForHandlerCompletion();
    }

    @Test
    public void testPropertySetExceptionNotPropagated_ClientStartSession() throws Exception {
        initHalService(false);

        when(mVmsClient.getAvailableLayers()).thenReturn(
                new VmsAvailableLayers(Collections.emptySet(), 0));
        doThrow(new RuntimeException()).when(mVehicleHal).set(any());

        VehiclePropValue request = createHalMessage(
                VmsMessageType.START_SESSION,  // Message type
                -1,                            // Core ID (unknown)
                CLIENT_ID                      // Client ID
        );

        sendHalMessage(request);
        waitForHandlerCompletion();
    }

    @Test
    public void testDumpMetrics_DefaultConfig() {
        mHalService.dumpMetrics(new FileDescriptor());
        verifyZeroInteractions(mVehicleHal);
    }

    @Test
    public void testDumpMetrics_NonVendorProperty() throws Exception {
        VehiclePropValue vehicleProp = new VehiclePropValue();
        vehicleProp.value.bytes.addAll(PAYLOAD_AS_LIST);
        when(mVehicleHal.get(anyInt())).thenReturn(vehicleProp);

        when(mResources.getInteger(
                R.integer.vmsHalClientMetricsProperty)).thenReturn(
                VehicleProperty.VEHICLE_MAP_SERVICE);
        setUp();

        mHalService.dumpMetrics(new FileDescriptor());
        verifyZeroInteractions(mVehicleHal);
    }

    @Test
    public void testDumpMetrics_VendorProperty() throws Exception {
        int metricsPropertyId = VehiclePropertyGroup.VENDOR | 1;
        when(mResources.getInteger(
                R.integer.vmsHalClientMetricsProperty)).thenReturn(
                metricsPropertyId);
        setUp();

        VehiclePropValue metricsProperty = new VehiclePropValue();
        metricsProperty.value.bytes.addAll(PAYLOAD_AS_LIST);
        when(mVehicleHal.get(metricsPropertyId)).thenReturn(metricsProperty);

        try (TemporaryFile dumpsysFile = new TemporaryFile("VmsHalServiceTest")) {
            FileOutputStream outputStream = new FileOutputStream(dumpsysFile.getFile());
            mHalService.dumpMetrics(outputStream.getFD());

            verify(mVehicleHal).get(metricsPropertyId);
            FileInputStream inputStream = new FileInputStream(dumpsysFile.getFile());
            byte[] dumpsysOutput = new byte[PAYLOAD.length];
            assertEquals(PAYLOAD.length, inputStream.read(dumpsysOutput));
            assertArrayEquals(PAYLOAD, dumpsysOutput);
        }
    }

    @Test
    public void testDumpMetrics_VendorProperty_Timeout() throws Exception {
        int metricsPropertyId = VehiclePropertyGroup.VENDOR | 1;
        when(mResources.getInteger(
                R.integer.vmsHalClientMetricsProperty)).thenReturn(
                metricsPropertyId);
        setUp();

        when(mVehicleHal.get(metricsPropertyId))
                .thenThrow(new ServiceSpecificException(VehicleHalStatusCode.STATUS_TRY_AGAIN,
                        "propertyId: 0x" + Integer.toHexString(metricsPropertyId)));

        mHalService.dumpMetrics(new FileDescriptor());
        verify(mVehicleHal).get(metricsPropertyId);
    }

    @Test
    public void testDumpMetrics_VendorProperty_Unavailable() throws Exception {
        int metricsPropertyId = VehiclePropertyGroup.VENDOR | 1;
        when(mResources.getInteger(
                R.integer.vmsHalClientMetricsProperty)).thenReturn(
                metricsPropertyId);
        setUp();

        when(mVehicleHal.get(metricsPropertyId)).thenReturn(null);

        mHalService.dumpMetrics(new FileDescriptor());
        verify(mVehicleHal).get(metricsPropertyId);
    }

    private static VehiclePropValue createHalMessage(Integer... message) {
        VehiclePropValue result = new VehiclePropValue();
        result.prop = VehicleProperty.VEHICLE_MAP_SERVICE;
        result.value.int32Values.addAll(Arrays.asList(message));
        return result;
    }

    private void sendHalMessage(VehiclePropValue message) {
        mHalService.onHalEvents(Collections.singletonList(message));
    }

    private void waitForHandlerCompletion() throws Exception {
        final CountDownLatch latch = new CountDownLatch(1);
        mHalService.getHandler().post(latch::countDown);
        latch.await(5, TimeUnit.SECONDS);
    }

    @SafeVarargs
    private static <T> Set<T> asSet(T... values) {
        return new HashSet<>(Arrays.asList(values));
    }
}
