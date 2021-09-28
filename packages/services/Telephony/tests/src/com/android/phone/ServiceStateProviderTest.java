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

package com.android.phone;

import static android.provider.Telephony.ServiceStateTable;
import static android.provider.Telephony.ServiceStateTable.getUriForSubscriptionId;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.content.pm.ProviderInfo;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.telephony.ServiceState;
import android.telephony.SubscriptionManager;
import android.test.mock.MockContentResolver;
import android.test.suitebuilder.annotation.SmallTest;

import org.junit.Before;
import org.junit.Test;

/**
 * Tests for simple queries of ServiceStateProvider.
 *
 * Build, install and run the tests by running the commands below:
 *     runtest --path <dir or file>
 *     runtest --path <dir or file> --test-method <testMethodName>
 *     e.g.)
 *         runtest --path tests/src/com/android/phone/ServiceStateProviderTest.java \
 *                 --test-method testGetServiceState
 */
public class ServiceStateProviderTest {
    private static final String TAG = "ServiceStateProviderTest";

    private Context mContext;
    private MockContentResolver mContentResolver;
    private ServiceState mTestServiceState;
    private ServiceState mTestServiceStateForSubId1;

    private final String[] mTestProjection =
    {
        ServiceStateTable.VOICE_REG_STATE,
        ServiceStateProvider.DATA_REG_STATE,
        ServiceStateProvider.VOICE_OPERATOR_ALPHA_LONG,
        ServiceStateProvider.VOICE_OPERATOR_ALPHA_SHORT,
        ServiceStateTable.VOICE_OPERATOR_NUMERIC,
        ServiceStateProvider.DATA_OPERATOR_ALPHA_LONG,
        ServiceStateProvider.DATA_OPERATOR_ALPHA_SHORT,
        ServiceStateProvider.DATA_OPERATOR_NUMERIC,
        ServiceStateTable.IS_MANUAL_NETWORK_SELECTION,
        ServiceStateProvider.RIL_VOICE_RADIO_TECHNOLOGY,
        ServiceStateProvider.RIL_DATA_RADIO_TECHNOLOGY,
        ServiceStateProvider.CSS_INDICATOR,
        ServiceStateProvider.NETWORK_ID,
        ServiceStateProvider.SYSTEM_ID,
        ServiceStateProvider.CDMA_ROAMING_INDICATOR,
        ServiceStateProvider.CDMA_DEFAULT_ROAMING_INDICATOR,
        ServiceStateProvider.CDMA_ERI_ICON_INDEX,
        ServiceStateProvider.CDMA_ERI_ICON_MODE,
        ServiceStateProvider.IS_EMERGENCY_ONLY,
        ServiceStateProvider.IS_USING_CARRIER_AGGREGATION,
        ServiceStateProvider.OPERATOR_ALPHA_LONG_RAW,
        ServiceStateProvider.OPERATOR_ALPHA_SHORT_RAW,
    };

    @Before
    public void setUp() throws Exception {
        mContext = mock(Context.class);
        mContentResolver = new MockContentResolver() {
            @Override
            public void notifyChange(Uri uri, ContentObserver observer, boolean syncToNetwork) {
                throw new RuntimeException("notifyChange!");
            }
        };
        doReturn(mContentResolver).when(mContext).getContentResolver();

        mTestServiceState = new ServiceState();
        mTestServiceState.setStateOutOfService();
        mTestServiceStateForSubId1 = new ServiceState();
        mTestServiceStateForSubId1.setStateOff();

        // Mock out the actual phone state
        ServiceStateProvider provider = new ServiceStateProvider() {
            @Override
            public ServiceState getServiceState(int subId) {
                if (subId == 1) {
                    return mTestServiceStateForSubId1;
                } else {
                    return mTestServiceState;
                }
            }

            @Override
            public int getDefaultSubId() {
                return 0;
            }
        };
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = "service-state";
        provider.attachInfoForTesting(mContext, providerInfo);
        mContentResolver.addProvider("service-state", provider);
    }

    @Test
    @SmallTest
    public void testQueryServiceStateWithNoSubId() {
        // Verify that when calling query with no subId in the uri the default ServiceState is
        // returned.
        // In this case the subId is set to 0 and the expected service state is
        // mTestServiceState.
        verifyServiceStateForSubId(ServiceStateTable.CONTENT_URI, mTestServiceState);
    }

    @Test
    @SmallTest
    public void testGetServiceStateWithDefaultSubId() {
        // Verify that when calling with the DEFAULT_SUBSCRIPTION_ID the correct ServiceState is
        // returned
        // In this case the subId is set to 0 and the expected service state is
        // mTestServiceState.
        verifyServiceStateForSubId(
                getUriForSubscriptionId(SubscriptionManager.DEFAULT_SUBSCRIPTION_ID),
                mTestServiceState);
    }

    /**
     * Test querying the service state for a given subId
     */
    @Test
    @SmallTest
    public void testGetServiceStateForSubId() {
        // Verify that when calling with a specific subId the correct ServiceState is returned
        // In this case the subId is set to 1 and the expected service state is
        // mTestServiceStateForSubId1
        verifyServiceStateForSubId(getUriForSubscriptionId(1), mTestServiceStateForSubId1);
    }

    private void verifyServiceStateForSubId(Uri uri, ServiceState ss) {
        Cursor cursor = mContentResolver.query(uri, mTestProjection, "",
                null, null);
        assertNotNull(cursor);
        cursor.moveToFirst();

        final int voiceRegState = ss.getState();
        final int dataRegState = ss.getDataRegistrationState();
        final String voiceOperatorAlphaLong = ss.getOperatorAlphaLong();
        final String voiceOperatorAlphaShort = ss.getOperatorAlphaShort();
        final String voiceOperatorNumeric = ss.getOperatorNumeric();
        final String dataOperatorAlphaLong = ss.getOperatorAlphaLong();
        final String dataOperatorAlphaShort = ss.getOperatorAlphaShort();
        final String dataOperatorNumeric = ss.getOperatorNumeric();
        final int isManualNetworkSelection = (ss.getIsManualSelection()) ? 1 : 0;
        final int rilVoiceRadioTechnology = ss.getRilVoiceRadioTechnology();
        final int rilDataRadioTechnology = ss.getRilDataRadioTechnology();
        final int cssIndicator = ss.getCssIndicator();
        final int networkId = ss.getCdmaNetworkId();
        final int systemId = ss.getCdmaSystemId();
        final int cdmaRoamingIndicator = ss.getCdmaRoamingIndicator();
        final int cdmaDefaultRoamingIndicator = ss.getCdmaDefaultRoamingIndicator();
        final int cdmaEriIconIndex = ss.getCdmaEriIconIndex();
        final int cdmaEriIconMode = ss.getCdmaEriIconMode();
        final int isEmergencyOnly = (ss.isEmergencyOnly()) ? 1 : 0;
        final int isUsingCarrierAggregation = (ss.isUsingCarrierAggregation()) ? 1 : 0;
        final String operatorAlphaLongRaw = ss.getOperatorAlphaLongRaw();
        final String operatorAlphaShortRaw = ss.getOperatorAlphaShortRaw();

        assertEquals(voiceRegState, cursor.getInt(0));
        assertEquals(dataRegState, cursor.getInt(1));
        assertEquals(voiceOperatorAlphaLong, cursor.getString(2));
        assertEquals(voiceOperatorAlphaShort, cursor.getString(3));
        assertEquals(voiceOperatorNumeric, cursor.getString(4));
        assertEquals(dataOperatorAlphaLong, cursor.getString(5));
        assertEquals(dataOperatorAlphaShort, cursor.getString(6));
        assertEquals(dataOperatorNumeric, cursor.getString(7));
        assertEquals(isManualNetworkSelection, cursor.getInt(8));
        assertEquals(rilVoiceRadioTechnology, cursor.getInt(9));
        assertEquals(rilDataRadioTechnology, cursor.getInt(10));
        assertEquals(cssIndicator, cursor.getInt(11));
        assertEquals(networkId, cursor.getInt(12));
        assertEquals(systemId, cursor.getInt(13));
        assertEquals(cdmaRoamingIndicator, cursor.getInt(14));
        assertEquals(cdmaDefaultRoamingIndicator, cursor.getInt(15));
        assertEquals(cdmaEriIconIndex, cursor.getInt(16));
        assertEquals(cdmaEriIconMode, cursor.getInt(17));
        assertEquals(isEmergencyOnly, cursor.getInt(18));
        assertEquals(isUsingCarrierAggregation, cursor.getInt(19));
        assertEquals(operatorAlphaLongRaw, cursor.getString(20));
        assertEquals(operatorAlphaShortRaw, cursor.getString(21));
    }

    /**
     * Test that we don't notify for certain field changes. (e.g. we don't notify when the NetworkId
     * or SystemId change) This is an intentional behavior change from the broadcast.
     */
    @Test
    @SmallTest
    public void testNoNotify() {
        int subId = 0;

        ServiceState oldSS = new ServiceState();
        oldSS.setStateOutOfService();
        oldSS.setCdmaSystemAndNetworkId(1, 1);

        ServiceState newSS = new ServiceState();
        newSS.setStateOutOfService();
        newSS.setCdmaSystemAndNetworkId(0, 0);

        // Test that notifyChange is not called for these fields
        boolean notifyChangeWasCalled = false;
        try {
            ServiceStateProvider.notifyChangeForSubIdAndField(mContext, oldSS, newSS, subId);
        } catch (RuntimeException e) {
            final String message = e.getMessage();
            if (message != null &&  message.equals("notifyChange!")) {
                notifyChangeWasCalled = true;
            }
        }
        assertFalse(notifyChangeWasCalled);
    }

    @Test
    @SmallTest
    public void testNotifyChanged() {
        int subId = 0;

        ServiceState oldSS = new ServiceState();
        oldSS.setStateOutOfService();
        oldSS.setVoiceRegState(ServiceState.STATE_OUT_OF_SERVICE);

        ServiceState copyOfOldSS = new ServiceState();
        copyOfOldSS.setStateOutOfService();
        copyOfOldSS.setVoiceRegState(ServiceState.STATE_OUT_OF_SERVICE);

        ServiceState newSS = new ServiceState();
        newSS.setStateOutOfService();
        newSS.setVoiceRegState(ServiceState.STATE_POWER_OFF);

        // Test that notifyChange is not called with no change in notifyChangeForSubIdAndField
        boolean notifyChangeWasCalled = false;
        try {
            ServiceStateProvider.notifyChangeForSubIdAndField(mContext, oldSS, copyOfOldSS, subId);
        } catch (RuntimeException e) {
            final String message = e.getMessage();
            if (message != null &&  message.equals("notifyChange!")) {
                notifyChangeWasCalled = true;
            }
        }
        assertFalse(notifyChangeWasCalled);

        // Test that notifyChange is not called with no change in notifyChangeForSubId
        notifyChangeWasCalled = false;
        try {
            ServiceStateProvider.notifyChangeForSubId(mContext, oldSS, copyOfOldSS, subId);
        } catch (RuntimeException e) {
            final String message = e.getMessage();
            if (message != null &&  message.equals("notifyChange!")) {
                notifyChangeWasCalled = true;
            }
        }
        assertFalse(notifyChangeWasCalled);

        // Test that notifyChange is called by notifyChangeForSubIdAndField when the voice_reg_state
        // changes
        notifyChangeWasCalled = false;
        try {
            ServiceStateProvider.notifyChangeForSubIdAndField(mContext, oldSS, newSS, subId);
        } catch (RuntimeException e) {
            final String message = e.getMessage();
            if (message != null &&  message.equals("notifyChange!")) {
                notifyChangeWasCalled = true;
            }
        }
        assertTrue(notifyChangeWasCalled);

        // Test that notifyChange is called by notifyChangeForSubId when the voice_reg_state changes
        notifyChangeWasCalled = false;
        try {
            ServiceStateProvider.notifyChangeForSubId(mContext, oldSS, newSS, subId);
        } catch (RuntimeException e) {
            final String message = e.getMessage();
            if (message != null &&  message.equals("notifyChange!")) {
                notifyChangeWasCalled = true;
            }
        }
        assertTrue(notifyChangeWasCalled);
    }
}
