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
package com.android.ons;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.os.Looper;
import android.os.RemoteException;
import android.telephony.AvailableNetworkInfo;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyFrameworkInitializer;
import android.telephony.TelephonyManager;
import android.util.Log;

import androidx.test.runner.AndroidJUnit4;

import com.android.internal.telephony.IOns;
import com.android.internal.telephony.ISetOpportunisticDataCallback;
import com.android.internal.telephony.IUpdateAvailableNetworksCallback;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import java.util.ArrayList;
import java.util.HashMap;

@RunWith(AndroidJUnit4.class)
public class OpportunisticNetworkServiceTest extends ONSBaseTest {
    private static final String TAG = "ONSTest";
    private String pkgForDebug;
    private String pkgForFeature;
    private int mResult;
    private IOns iOpportunisticNetworkService;
    private Looper mLooper;
    private OpportunisticNetworkService mOpportunisticNetworkService;
    private static final String CARRIER_APP_CONFIG_NAME = "carrierApp";
    private static final String SYSTEM_APP_CONFIG_NAME = "systemApp";

    @Mock
    private HashMap<String, ONSConfigInput> mockONSConfigInputHashMap;
    @Mock
    private ONSProfileSelector mockProfileSelector;

    @Before
    public void setUp() throws Exception {
        super.setUp("ONSTest");
        pkgForDebug = mContext != null ? mContext.getOpPackageName() : "<unknown>";
        pkgForFeature = mContext != null ? mContext.getAttributionTag() : null;
        Intent intent = new Intent(mContext, OpportunisticNetworkService.class);
        new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
                mOpportunisticNetworkService = new OpportunisticNetworkService();
                mOpportunisticNetworkService.initialize(mContext);
                mOpportunisticNetworkService.mSubscriptionManager = mSubscriptionManager;
                iOpportunisticNetworkService = (IOns) mOpportunisticNetworkService.onBind(null);
                mLooper = Looper.myLooper();
                setReady(true);
                Looper.loop();
            }
        }).start();
        waitUntilReady(200);
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
        mOpportunisticNetworkService.onDestroy();
        if (mLooper != null) {
            mLooper.quit();
            mLooper.getThread().join();
        }
    }

    @Test
    public void testCheckEnable() {
        boolean isEnable = true;
        try {
            iOpportunisticNetworkService.setEnable(false, pkgForDebug);
            isEnable = iOpportunisticNetworkService.isEnabled(pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        assertEquals(false, isEnable);
    }

    @Test
    public void testHandleSimStateChange() {
        mResult = -1;
        ArrayList<String> mccMncs = new ArrayList<>();
        mccMncs.add("310210");
        AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(1, 1, mccMncs,
                new ArrayList<Integer>());
        ArrayList<AvailableNetworkInfo> availableNetworkInfos =
                new ArrayList<AvailableNetworkInfo>();
        availableNetworkInfos.add(availableNetworkInfo);
        IUpdateAvailableNetworksCallback mCallback = new IUpdateAvailableNetworksCallback.Stub() {
            @Override
            public void onComplete(int result) {
                mResult = result;
                Log.d(TAG, "result: " + result);
            }
        };
        ONSConfigInput onsConfigInput = new ONSConfigInput(availableNetworkInfos, mCallback);
        onsConfigInput.setPrimarySub(1);
        onsConfigInput.setPreferredDataSub(availableNetworkInfos.get(0).getSubId());
        ArrayList<SubscriptionInfo> subscriptionInfos = new ArrayList<SubscriptionInfo>();

        // Case 1: There is no Carrier app using ONS.
        doReturn(null).when(mockONSConfigInputHashMap).get(CARRIER_APP_CONFIG_NAME);
        mOpportunisticNetworkService.mIsEnabled = true;
        mOpportunisticNetworkService.mONSConfigInputHashMap = mockONSConfigInputHashMap;
        mOpportunisticNetworkService.handleSimStateChange();
        waitForMs(500);
        verify(mockONSConfigInputHashMap, never()).get(SYSTEM_APP_CONFIG_NAME);

        // Case 2: There is a Carrier app using ONS and no System app input.
        doReturn(subscriptionInfos).when(mSubscriptionManager).getActiveSubscriptionInfoList(false);
        doReturn(onsConfigInput).when(mockONSConfigInputHashMap).get(CARRIER_APP_CONFIG_NAME);
        doReturn(null).when(mockONSConfigInputHashMap).get(SYSTEM_APP_CONFIG_NAME);
        mOpportunisticNetworkService.mIsEnabled = true;
        mOpportunisticNetworkService.mONSConfigInputHashMap = mockONSConfigInputHashMap;
        mOpportunisticNetworkService.handleSimStateChange();
        waitForMs(50);
        verify(mockONSConfigInputHashMap,times(1)).get(SYSTEM_APP_CONFIG_NAME);
    }

    @Test
    public void testSystemPreferredDataWhileCarrierAppIsActive() {
        mResult = -1;
        ArrayList<String> mccMncs = new ArrayList<>();
        mccMncs.add("310210");
        AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(1, 1, mccMncs,
            new ArrayList<Integer>());
        ArrayList<AvailableNetworkInfo> availableNetworkInfos =
            new ArrayList<AvailableNetworkInfo>();
        availableNetworkInfos.add(availableNetworkInfo);
        IUpdateAvailableNetworksCallback mCallback = new IUpdateAvailableNetworksCallback.Stub() {
            @Override
            public void onComplete(int result) {
                mResult = result;
                Log.d(TAG, "result: " + result);
            }
        };
        ONSConfigInput onsConfigInput = new ONSConfigInput(availableNetworkInfos, mCallback);
        onsConfigInput.setPrimarySub(1);
        onsConfigInput.setPreferredDataSub(availableNetworkInfos.get(0).getSubId());
        ArrayList<SubscriptionInfo> subscriptionInfos = new ArrayList<SubscriptionInfo>();

        doReturn(subscriptionInfos).when(mSubscriptionManager).getActiveSubscriptionInfoList(false);
        doReturn(onsConfigInput).when(mockONSConfigInputHashMap).get(CARRIER_APP_CONFIG_NAME);
        mOpportunisticNetworkService.mIsEnabled = true;
        mOpportunisticNetworkService.mONSConfigInputHashMap = mockONSConfigInputHashMap;

        mResult = -1;
        ISetOpportunisticDataCallback callbackStub = new ISetOpportunisticDataCallback.Stub() {
            @Override
            public void onComplete(int result) {
                Log.d(TAG, "callbackStub, mResult end:" + result);
                mResult = result;
            }
        };

        try {
            IOns onsBinder = (IOns)mOpportunisticNetworkService.onBind(null);
            onsBinder.setPreferredDataSubscriptionId(
                    SubscriptionManager.DEFAULT_SUBSCRIPTION_ID, false, callbackStub,
                    pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        waitForMs(50);
        assertEquals(TelephonyManager.SET_OPPORTUNISTIC_SUB_VALIDATION_FAILED, mResult);
    }

    @Test
    public void testSetPreferredDataSubscriptionId() {
        mResult = -1;
        ISetOpportunisticDataCallback callbackStub = new ISetOpportunisticDataCallback.Stub() {
            @Override
            public void onComplete(int result) {
                Log.d(TAG, "callbackStub, mResult end:" + result);
                mResult = result;
            }
        };

        try {
            iOpportunisticNetworkService.setPreferredDataSubscriptionId(5, false, callbackStub,
                    pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        assertEquals(
                TelephonyManager.SET_OPPORTUNISTIC_SUB_NO_OPPORTUNISTIC_SUB_AVAILABLE, mResult);
    }

    @Test
    public void testGetPreferredDataSubscriptionId() {
        assertNotNull(iOpportunisticNetworkService);
        mResult = -1;
        try {
            mResult = iOpportunisticNetworkService.getPreferredDataSubscriptionId(pkgForDebug,
                    pkgForFeature);
            Log.d(TAG, "testGetPreferredDataSubscriptionId: " + mResult);
            assertNotNull(mResult);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
    }

    @Test
    public void testUpdateAvailableNetworksWithInvalidArguments() {
        mResult = -1;
        IUpdateAvailableNetworksCallback mCallback = new IUpdateAvailableNetworksCallback.Stub() {
            @Override
            public void onComplete(int result) {
                Log.d(TAG, "mResult end:" + result);
                mResult = result;
            }
        };

        ArrayList<String> mccMncs = new ArrayList<>();
        mccMncs.add("310210");
        AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(1, 1, mccMncs,
                new ArrayList<Integer>());
        ArrayList<AvailableNetworkInfo> availableNetworkInfos = new ArrayList<>();
        availableNetworkInfos.add(availableNetworkInfo);

        try {
            iOpportunisticNetworkService.updateAvailableNetworks(availableNetworkInfos, mCallback,
                    pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        assertEquals(
                TelephonyManager.UPDATE_AVAILABLE_NETWORKS_NO_OPPORTUNISTIC_SUB_AVAILABLE, mResult);
    }

    @Test
    public void testUpdateAvailableNetworksWithSuccess() {
        mResult = -1;
        IUpdateAvailableNetworksCallback mCallback = new IUpdateAvailableNetworksCallback.Stub() {
            @Override
            public void onComplete(int result) {
                Log.d(TAG, "mResult end:" + result);
                mResult = result;
            }
        };
        ArrayList<AvailableNetworkInfo> availableNetworkInfos = new ArrayList<>();
        try {
            iOpportunisticNetworkService.setEnable(false, pkgForDebug);
            iOpportunisticNetworkService.updateAvailableNetworks(availableNetworkInfos, mCallback,
                    pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        assertEquals(TelephonyManager.UPDATE_AVAILABLE_NETWORKS_SUCCESS, mResult);
    }

    @Test
    public void testPriorityRuleOfActivatingAvailableNetworks() {
        ArrayList<String> mccMncs = new ArrayList<>();
        mccMncs.add("310210");
        AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(1, 1, mccMncs,
                new ArrayList<Integer>());
        ArrayList<AvailableNetworkInfo> availableNetworkInfos =
                new ArrayList<AvailableNetworkInfo>();
        availableNetworkInfos.add(availableNetworkInfo);
        mResult = -1;
        IUpdateAvailableNetworksCallback mCallback = new IUpdateAvailableNetworksCallback.Stub() {
            @Override
            public void onComplete(int result) {
                mResult = result;
                Log.d(TAG, "result: " + result);
            }
        };
        ONSConfigInput onsConfigInput = new ONSConfigInput(availableNetworkInfos, mCallback);
        onsConfigInput.setPrimarySub(1);
        onsConfigInput.setPreferredDataSub(availableNetworkInfos.get(0).getSubId());
        doReturn(onsConfigInput).when(mockONSConfigInputHashMap).get(CARRIER_APP_CONFIG_NAME);
        doReturn(true).when(mockProfileSelector).hasOpprotunisticSub(any());
        doReturn(false).when(mockProfileSelector).containStandaloneOppSubs(any());
        mOpportunisticNetworkService.mIsEnabled = true;
        mOpportunisticNetworkService.mONSConfigInputHashMap = mockONSConfigInputHashMap;
        mOpportunisticNetworkService.mProfileSelector = mockProfileSelector;

        // Assume carrier app has updated available networks at first.
        // Then system app updated available networks which is not standalone.
        try {
            IOns onsBinder = (IOns) mOpportunisticNetworkService.onBind(null);
            onsBinder.updateAvailableNetworks(availableNetworkInfos, mCallback, pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        verify(mockProfileSelector, never()).startProfileSelection(any(), any());

        // System app updated available networks which contain standalone network.
        doReturn(true).when(mockProfileSelector).containStandaloneOppSubs(any());
        try {
            IOns onsBinder = (IOns) mOpportunisticNetworkService.onBind(null);
            onsBinder.updateAvailableNetworks(availableNetworkInfos, mCallback, pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        verify(mockProfileSelector, times(1)).startProfileSelection(any(), any());

        // System app updated available networks which equal to null.
        // Case1: start carrier app request, if there is a carrier app request.
        availableNetworkInfos.clear();
        try {
            IOns onsBinder = (IOns) mOpportunisticNetworkService.onBind(null);
            onsBinder.updateAvailableNetworks(availableNetworkInfos, mCallback, pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        verify(mockProfileSelector, times(2)).startProfileSelection(any(), any());

        // System app updated available networks which equal to null.
        // Case2: stop profile selection, if there is no any carrier app request.
        doReturn(null).when(mockONSConfigInputHashMap).get(CARRIER_APP_CONFIG_NAME);
        try {
            IOns onsBinder = (IOns) mOpportunisticNetworkService.onBind(null);
            onsBinder.updateAvailableNetworks(availableNetworkInfos, mCallback, pkgForDebug);
        } catch (RemoteException ex) {
            Log.e(TAG, "RemoteException", ex);
        }
        verify(mockProfileSelector, times(1)).stopProfileSelection(any());
    }

    private IOns getIOns() {
        return IOns.Stub.asInterface(
                TelephonyFrameworkInitializer
                        .getTelephonyServiceManager()
                        .getOpportunisticNetworkServiceRegisterer()
                        .get());
    }

    public static void waitForMs(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
            Log.d(TAG, "InterruptedException while waiting: " + e);
        }
    }
}
