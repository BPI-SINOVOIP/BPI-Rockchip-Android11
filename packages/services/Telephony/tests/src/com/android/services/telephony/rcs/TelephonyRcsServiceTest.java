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

package com.android.services.telephony.rcs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.BroadcastReceiver;
import android.content.Intent;
import android.os.PersistableBundle;
import android.telephony.CarrierConfigManager;
import android.telephony.SubscriptionManager;

import androidx.test.runner.AndroidJUnit4;

import com.android.TelephonyTestBase;
import com.android.ims.FeatureConnector;
import com.android.ims.RcsFeatureManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;

@RunWith(AndroidJUnit4.class)
public class TelephonyRcsServiceTest extends TelephonyTestBase {

    @Captor ArgumentCaptor<BroadcastReceiver> mReceiverCaptor;
    @Mock TelephonyRcsService.FeatureFactory mFeatureFactory;
    @Mock UserCapabilityExchangeImpl mMockUceSlot0;
    @Mock UserCapabilityExchangeImpl mMockUceSlot1;
    @Mock RcsFeatureController.RegistrationHelperFactory mRegistrationFactory;
    @Mock RcsFeatureController.FeatureConnectorFactory<RcsFeatureManager> mFeatureConnectorFactory;
    @Mock FeatureConnector<RcsFeatureManager> mFeatureConnector;

    private RcsFeatureController mFeatureControllerSlot0;
    private RcsFeatureController mFeatureControllerSlot1;

    @Before
    public void setUp() throws Exception {
        super.setUp();
        doReturn(mFeatureConnector).when(mFeatureConnectorFactory).create(any(), anyInt(),
                any(), any(), any());
        mFeatureControllerSlot0 = createFeatureController(0 /*slotId*/);
        mFeatureControllerSlot1 = createFeatureController(1 /*slotId*/);
        doReturn(mFeatureControllerSlot0).when(mFeatureFactory).createController(any(), eq(0));
        doReturn(mFeatureControllerSlot1).when(mFeatureFactory).createController(any(), eq(1));
        doReturn(mMockUceSlot0).when(mFeatureFactory).createUserCapabilityExchange(any(), eq(0),
                anyInt());
        doReturn(mMockUceSlot1).when(mFeatureFactory).createUserCapabilityExchange(any(), eq(1),
                anyInt());
        //set up default slot-> sub ID mappings.
        setSlotToSubIdMapping(0 /*slotId*/, 1/*subId*/);
        setSlotToSubIdMapping(1 /*slotId*/, 2/*subId*/);
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Test
    public void testUserCapabilityExchangePresenceConnected() {
        setCarrierConfig(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, true /*isEnabled*/);
        createRcsService(1 /*numSlots*/);
        verify(mFeatureControllerSlot0).addFeature(mMockUceSlot0, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0).connect();
    }

    @Test
    public void testUserCapabilityExchangeOptionsConnected() {
        setCarrierConfig(CarrierConfigManager.KEY_USE_RCS_SIP_OPTIONS_BOOL, true /*isEnabled*/);
        createRcsService(1 /*numSlots*/);
        verify(mFeatureControllerSlot0).addFeature(mMockUceSlot0, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0).connect();
    }

    @Test
    public void testNoFeaturesEnabled() {
        createRcsService(1 /*numSlots*/);
        // No carrier config set for UCE.
        verify(mFeatureControllerSlot0, never()).addFeature(mMockUceSlot0,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0, never()).connect();
    }

    @Test
    public void testNoFeaturesEnabledCarrierConfigChanged() {
        createRcsService(1 /*numSlots*/);
        // No carrier config set for UCE.

        sendCarrierConfigChanged(0, SubscriptionManager.INVALID_SUBSCRIPTION_ID);
        verify(mFeatureControllerSlot0, never()).addFeature(mMockUceSlot0,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0, never()).connect();
        verify(mFeatureControllerSlot0, never()).updateAssociatedSubscription(anyInt());
    }


    @Test
    public void testSlotUpdates() {
        setCarrierConfig(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, true /*isEnabled*/);
        TelephonyRcsService service = createRcsService(1 /*numSlots*/);
        verify(mFeatureControllerSlot0).addFeature(mMockUceSlot0, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0).connect();

        // there should be no changes if the new num slots = old num
        service.updateFeatureControllerSize(1 /*newNumSlots*/);
        verify(mFeatureControllerSlot0, times(1)).addFeature(mMockUceSlot0,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0, times(1)).connect();

        // Add a new slot.
        verify(mFeatureControllerSlot1, never()).addFeature(mMockUceSlot1,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot1, never()).connect();
        service.updateFeatureControllerSize(2 /*newNumSlots*/);
        // This shouldn't have changed for slot 0.
        verify(mFeatureControllerSlot0, times(1)).addFeature(mMockUceSlot0,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0, times(1)).connect();
        verify(mFeatureControllerSlot1).addFeature(mMockUceSlot1, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot1, times(1)).connect();

        // Remove a slot.
        verify(mFeatureControllerSlot0, never()).destroy();
        verify(mFeatureControllerSlot1, never()).destroy();
        service.updateFeatureControllerSize(1 /*newNumSlots*/);
        // addFeature/connect shouldn't have been called again
        verify(mFeatureControllerSlot0, times(1)).addFeature(mMockUceSlot0,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0, times(1)).connect();
        verify(mFeatureControllerSlot1, times(1)).addFeature(mMockUceSlot1,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot1, times(1)).connect();
        // Verify destroy is only called for slot 1.
        verify(mFeatureControllerSlot0, never()).destroy();
        verify(mFeatureControllerSlot1, times(1)).destroy();
    }

    @Test
    public void testCarrierConfigUpdate() {
        setCarrierConfig(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, true /*isEnabled*/);
        createRcsService(2 /*numSlots*/);
        verify(mFeatureControllerSlot0).addFeature(mMockUceSlot0, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot1).addFeature(mMockUceSlot1, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0).connect();
        verify(mFeatureControllerSlot1).connect();


        // Send carrier config update for each slot.
        sendCarrierConfigChanged(0 /*slotId*/, 1 /*subId*/);
        verify(mFeatureControllerSlot0).updateAssociatedSubscription(1);
        verify(mFeatureControllerSlot1, never()).updateAssociatedSubscription(1);
        sendCarrierConfigChanged(1 /*slotId*/, 2 /*subId*/);
        verify(mFeatureControllerSlot0, never()).updateAssociatedSubscription(2);
        verify(mFeatureControllerSlot1, times(1)).updateAssociatedSubscription(2);
    }

    @Test
    public void testCarrierConfigUpdateUceToNoUce() {
        setCarrierConfig(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, true /*isEnabled*/);
        createRcsService(1 /*numSlots*/);
        verify(mFeatureControllerSlot0).addFeature(mMockUceSlot0, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0).connect();


        // Send carrier config update for each slot.
        setCarrierConfig(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, false /*isEnabled*/);
        sendCarrierConfigChanged(0 /*slotId*/, 1 /*subId*/);
        verify(mFeatureControllerSlot0).removeFeature(UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0).updateAssociatedSubscription(1);
    }

    @Test
    public void testCarrierConfigUpdateNoUceToUce() {
        createRcsService(1 /*numSlots*/);
        verify(mFeatureControllerSlot0, never()).addFeature(mMockUceSlot0,
                UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0, never()).connect();


        // Send carrier config update for each slot.
        setCarrierConfig(CarrierConfigManager.KEY_USE_RCS_PRESENCE_BOOL, true /*isEnabled*/);
        sendCarrierConfigChanged(0 /*slotId*/, 1 /*subId*/);
        verify(mFeatureControllerSlot0).addFeature(mMockUceSlot0, UserCapabilityExchangeImpl.class);
        verify(mFeatureControllerSlot0).connect();
        verify(mFeatureControllerSlot0).updateAssociatedSubscription(1);
    }

    private void sendCarrierConfigChanged(int slotId, int subId) {
        Intent intent = new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        intent.putExtra(CarrierConfigManager.EXTRA_SLOT_INDEX, slotId);
        intent.putExtra(CarrierConfigManager.EXTRA_SUBSCRIPTION_INDEX, subId);
        mReceiverCaptor.getValue().onReceive(mContext, intent);
    }

    private void setCarrierConfig(String key, boolean value) {
        PersistableBundle bundle = mContext.getCarrierConfig();
        bundle.putBoolean(key, value);
    }

    private void setSlotToSubIdMapping(int slotId, int loadedSubId) {
        SubscriptionManager m = mContext.getSystemService(SubscriptionManager.class);
        int [] subIds = new int[1];
        subIds[0] = loadedSubId;
        doReturn(subIds).when(m).getSubscriptionIds(eq(slotId));
    }

    private TelephonyRcsService createRcsService(int numSlots) {
        TelephonyRcsService service = new TelephonyRcsService(mContext, numSlots);
        service.setFeatureFactory(mFeatureFactory);
        service.initialize();
        verify(mContext).registerReceiver(mReceiverCaptor.capture(), any());
        return service;
    }

    private RcsFeatureController createFeatureController(int slotId) {
        // Create a spy instead of a mock because TelephonyRcsService relies on state provided by
        // RcsFeatureController.
        RcsFeatureController controller = spy(new RcsFeatureController(mContext, slotId,
                mRegistrationFactory));
        controller.setFeatureConnectorFactory(mFeatureConnectorFactory);
        return controller;
    }
}
