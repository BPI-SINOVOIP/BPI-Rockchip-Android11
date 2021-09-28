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

package com.android.server.wifi;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.when;
import static com.android.server.wifi.WifiCarrierInfoManager.NOTIFICATION_USER_ALLOWED_CARRIER_INTENT_ACTION;
import static com.android.server.wifi.WifiCarrierInfoManager.NOTIFICATION_USER_CLICKED_INTENT_ACTION;
import static com.android.server.wifi.WifiCarrierInfoManager.NOTIFICATION_USER_DISALLOWED_CARRIER_INTENT_ACTION;
import static com.android.server.wifi.WifiCarrierInfoManager.NOTIFICATION_USER_DISMISSED_INTENT_ACTION;

import static org.junit.Assert.*;
import static org.mockito.Mockito.*;

import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.database.ContentObserver;
import android.net.Uri;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiEnterpriseConfig;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.net.wifi.hotspot2.pps.Credential;
import android.os.Handler;
import android.os.PersistableBundle;
import android.os.test.TestLooper;
import android.telephony.CarrierConfigManager;
import android.telephony.ImsiEncryptionInfo;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.util.Base64;
import android.util.Pair;
import android.view.Window;

import androidx.test.filters.SmallTest;

import com.android.dx.mockito.inline.extended.ExtendedMockito;
import com.android.internal.messages.nano.SystemMessageProto.SystemMessage;
import com.android.server.wifi.WifiCarrierInfoManager.SimAuthRequestData;
import com.android.server.wifi.WifiCarrierInfoManager.SimAuthResponseData;
import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.MockitoSession;
import org.mockito.quality.Strictness;

import java.security.PublicKey;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

import javax.crypto.BadPaddingException;
import javax.crypto.Cipher;

/**
 * Unit tests for {@link WifiCarrierInfoManager}.
 */
@SmallTest
public class WifiCarrierInfoManagerTest extends WifiBaseTest {
    private WifiCarrierInfoManager mWifiCarrierInfoManager;

    private static final int DATA_SUBID = 1;
    private static final int NON_DATA_SUBID = 2;
    private static final int INVALID_SUBID = -1;
    private static final int DATA_CARRIER_ID = 10;
    private static final int PARENT_DATA_CARRIER_ID = 11;
    private static final int NON_DATA_CARRIER_ID = 20;
    private static final int PARENT_NON_DATA_CARRIER_ID = 21;
    private static final int DEACTIVE_CARRIER_ID = 30;
    private static final String MATCH_PREFIX_IMSI = "123456*";
    private static final String DATA_FULL_IMSI = "123456789123456";
    private static final String NON_DATA_FULL_IMSI = "123456987654321";
    private static final String NO_MATCH_FULL_IMSI = "654321123456789";
    private static final String NO_MATCH_PREFIX_IMSI = "654321*";
    private static final String DATA_OPERATOR_NUMERIC = "123456";
    private static final String NON_DATA_OPERATOR_NUMERIC = "123456";
    private static final String NO_MATCH_OPERATOR_NUMERIC = "654321";
    private static final String TEST_PACKAGE = "com.test12345";
    private static final String ANONYMOUS_IDENTITY = "anonymous@wlan.mnc456.mcc123.3gppnetwork.org";
    private static final String CARRIER_NAME = "Google";

    @Mock CarrierConfigManager mCarrierConfigManager;
    @Mock WifiContext mContext;
    @Mock Resources mResources;
    @Mock FrameworkFacade mFrameworkFacade;
    @Mock TelephonyManager mTelephonyManager;
    @Mock TelephonyManager mDataTelephonyManager;
    @Mock TelephonyManager mNonDataTelephonyManager;
    @Mock SubscriptionManager mSubscriptionManager;
    @Mock SubscriptionInfo mDataSubscriptionInfo;
    @Mock SubscriptionInfo mNonDataSubscriptionInfo;
    @Mock WifiConfigStore mWifiConfigStore;
    @Mock WifiInjector mWifiInjector;
    @Mock WifiConfigManager mWifiConfigManager;
    @Mock ImsiPrivacyProtectionExemptionStoreData mImsiPrivacyProtectionExemptionStoreData;
    @Mock NotificationManager mNotificationManger;
    @Mock Notification.Builder mNotificationBuilder;
    @Mock Notification mNotification;
    @Mock AlertDialog.Builder mAlertDialogBuilder;
    @Mock AlertDialog mAlertDialog;
    @Mock WifiCarrierInfoManager.OnUserApproveCarrierListener mListener;
    @Mock WifiMetrics mWifiMetrics;

    private List<SubscriptionInfo> mSubInfoList;

    MockitoSession mMockingSession = null;
    TestLooper mLooper;
    private ImsiPrivacyProtectionExemptionStoreData.DataSource mImsiDataSource;
    private ArgumentCaptor<BroadcastReceiver> mBroadcastReceiverCaptor =
            ArgumentCaptor.forClass(BroadcastReceiver.class);

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mLooper = new TestLooper();
        when(mContext.getSystemService(Context.CARRIER_CONFIG_SERVICE))
                .thenReturn(mCarrierConfigManager);
        when(mContext.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(Context.NOTIFICATION_SERVICE))
                .thenReturn(mNotificationManger);
        when(mContext.getWifiOverlayApkPkgName()).thenReturn("test.com.android.wifi.resources");
        when(mFrameworkFacade.makeAlertDialogBuilder(any()))
                .thenReturn(mAlertDialogBuilder);
        when(mFrameworkFacade.makeNotificationBuilder(any(), anyString()))
                .thenReturn(mNotificationBuilder);
        when(mFrameworkFacade.getBroadcast(any(), anyInt(), any(), anyInt()))
                .thenReturn(mock(PendingIntent.class));
        when(mAlertDialogBuilder.setTitle(any())).thenReturn(mAlertDialogBuilder);
        when(mAlertDialogBuilder.setMessage(any())).thenReturn(mAlertDialogBuilder);
        when(mAlertDialogBuilder.setPositiveButton(any(), any())).thenReturn(mAlertDialogBuilder);
        when(mAlertDialogBuilder.setNegativeButton(any(), any())).thenReturn(mAlertDialogBuilder);
        when(mAlertDialogBuilder.setOnDismissListener(any())).thenReturn(mAlertDialogBuilder);
        when(mAlertDialogBuilder.setOnCancelListener(any())).thenReturn(mAlertDialogBuilder);
        when(mAlertDialogBuilder.create()).thenReturn(mAlertDialog);
        when(mAlertDialog.getWindow()).thenReturn(mock(Window.class));
        when(mNotificationBuilder.setSmallIcon(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setTicker(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setContentTitle(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setStyle(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setContentIntent(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setDeleteIntent(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setShowWhen(anyBoolean())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setLocalOnly(anyBoolean())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setColor(anyInt())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.addAction(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.build()).thenReturn(mNotification);
        when(mWifiInjector.makeImsiProtectionExemptionStoreData(any()))
                .thenReturn(mImsiPrivacyProtectionExemptionStoreData);
        when(mWifiInjector.getWifiConfigManager()).thenReturn(mWifiConfigManager);
        mWifiCarrierInfoManager = new WifiCarrierInfoManager(mTelephonyManager,
                mSubscriptionManager, mWifiInjector, mFrameworkFacade, mContext, mWifiConfigStore,
                new Handler(mLooper.getLooper()), mWifiMetrics);
        ArgumentCaptor<ImsiPrivacyProtectionExemptionStoreData.DataSource>
                imsiDataSourceArgumentCaptor =
                ArgumentCaptor.forClass(ImsiPrivacyProtectionExemptionStoreData.DataSource.class);
        verify(mContext).registerReceiver(mBroadcastReceiverCaptor.capture(), any(), any(), any());
        verify(mWifiInjector).makeImsiProtectionExemptionStoreData(imsiDataSourceArgumentCaptor
                .capture());
        mImsiDataSource = imsiDataSourceArgumentCaptor.getValue();
        assertNotNull(mImsiDataSource);
        mSubInfoList = new ArrayList<>();
        mSubInfoList.add(mDataSubscriptionInfo);
        mSubInfoList.add(mNonDataSubscriptionInfo);
        when(mTelephonyManager.createForSubscriptionId(eq(DATA_SUBID)))
                .thenReturn(mDataTelephonyManager);
        when(mTelephonyManager.createForSubscriptionId(eq(NON_DATA_SUBID)))
                .thenReturn(mNonDataTelephonyManager);
        when(mTelephonyManager.getSimState(anyInt())).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mSubscriptionManager.getActiveSubscriptionInfoList()).thenReturn(mSubInfoList);
        mMockingSession = ExtendedMockito.mockitoSession().strictness(Strictness.LENIENT)
                .mockStatic(SubscriptionManager.class).startMocking();

        doReturn(DATA_SUBID).when(
                () -> SubscriptionManager.getDefaultDataSubscriptionId());
        doReturn(true).when(
                () -> SubscriptionManager.isValidSubscriptionId(DATA_SUBID));
        doReturn(true).when(
                () -> SubscriptionManager.isValidSubscriptionId(NON_DATA_SUBID));

        when(mDataSubscriptionInfo.getCarrierId()).thenReturn(DATA_CARRIER_ID);
        when(mDataSubscriptionInfo.getSubscriptionId()).thenReturn(DATA_SUBID);
        when(mNonDataSubscriptionInfo.getCarrierId()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataSubscriptionInfo.getSubscriptionId()).thenReturn(NON_DATA_SUBID);
        when(mDataTelephonyManager.getSubscriberId()).thenReturn(DATA_FULL_IMSI);
        when(mNonDataTelephonyManager.getSubscriberId()).thenReturn(NON_DATA_FULL_IMSI);
        when(mDataTelephonyManager.getSimOperator()).thenReturn(DATA_OPERATOR_NUMERIC);
        when(mDataTelephonyManager.getSimCarrierIdName()).thenReturn(CARRIER_NAME);
        when(mNonDataTelephonyManager.getSimCarrierIdName()).thenReturn(null);
        when(mNonDataTelephonyManager.getSimOperator())
                .thenReturn(NON_DATA_OPERATOR_NUMERIC);
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mNonDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mSubscriptionManager.getActiveSubscriptionIdList())
                .thenReturn(new int[]{DATA_SUBID, NON_DATA_SUBID});

        // setup resource strings for IMSI protection notification.
        when(mResources.getString(eq(R.string.wifi_suggestion_imsi_privacy_title), anyString()))
                .thenAnswer(s -> "blah" + s.getArguments()[1]);
        when(mResources.getString(eq(R.string.wifi_suggestion_imsi_privacy_content)))
                .thenReturn("blah");
        when(mResources.getText(
                eq(R.string.wifi_suggestion_action_allow_imsi_privacy_exemption_carrier)))
                .thenReturn("blah");
        when(mResources.getText(
                eq(R.string.wifi_suggestion_action_disallow_imsi_privacy_exemption_carrier)))
                .thenReturn("blah");
        when(mResources.getString(
                eq(R.string.wifi_suggestion_imsi_privacy_exemption_confirmation_title)))
                .thenReturn("blah");
        when(mResources.getString(
                eq(R.string.wifi_suggestion_imsi_privacy_exemption_confirmation_content),
                anyString())).thenAnswer(s -> "blah" + s.getArguments()[1]);
        when(mResources.getText(
                eq(R.string.wifi_suggestion_action_allow_imsi_privacy_exemption_confirmation)))
                .thenReturn("blah");
        when(mResources.getText(
                eq(R.string.wifi_suggestion_action_disallow_imsi_privacy_exemption_confirmation)))
                .thenReturn("blah");
        mWifiCarrierInfoManager.addImsiExemptionUserApprovalListener(mListener);
        mImsiDataSource.fromDeserialized(new HashMap<>());
    }

    @After
    public void cleanUp() throws Exception {
        if (mMockingSession != null) {
            mMockingSession.finishMocking();
        }
    }

    /**
     * Verify that the IMSI encryption info is not updated  when non
     * {@link CarrierConfigManager#ACTION_CARRIER_CONFIG_CHANGED} intent is received.
     *
     * @throws Exception
     */
    @Test
    public void receivedNonCarrierConfigChangedIntent() throws Exception {
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));
        receiver.getValue().onReceive(mContext, new Intent("dummyIntent"));
        verify(mCarrierConfigManager, never()).getConfig();
    }

    private PersistableBundle generateTestCarrierConfig(boolean requiresImsiEncryption) {
        PersistableBundle bundle = new PersistableBundle();
        if (requiresImsiEncryption) {
            bundle.putInt(CarrierConfigManager.IMSI_KEY_AVAILABILITY_INT,
                    TelephonyManager.KEY_TYPE_WLAN);
        }
        return bundle;
    }

    private PersistableBundle generateTestCarrierConfig(boolean requiresImsiEncryption,
            boolean requiresEapMethodPrefix) {
        PersistableBundle bundle = generateTestCarrierConfig(requiresImsiEncryption);
        if (requiresEapMethodPrefix) {
            bundle.putBoolean(CarrierConfigManager.ENABLE_EAP_METHOD_PREFIX_BOOL, true);
        }
        return bundle;
    }

    /**
     * Verify getting value about that if the IMSI encryption is required or not when
     * {@link CarrierConfigManager#ACTION_CARRIER_CONFIG_CHANGED} intent is received.
     */
    @Test
    public void receivedCarrierConfigChangedIntent() throws Exception {
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(true));
        when(mCarrierConfigManager.getConfigForSubId(NON_DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));

        assertTrue(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(NON_DATA_SUBID));
    }

    /**
     * Verify the IMSI encryption is cleared when the configuration in CarrierConfig is removed.
     */
    @Test
    public void imsiEncryptionRequiredInfoIsCleared() {
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(true));
        when(mCarrierConfigManager.getConfigForSubId(NON_DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(true));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));

        assertTrue(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));
        assertTrue(mWifiCarrierInfoManager.requiresImsiEncryption(NON_DATA_SUBID));

        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        when(mCarrierConfigManager.getConfigForSubId(NON_DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));

        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(NON_DATA_SUBID));
    }

    /**
     * Verify that if the IMSI encryption is downloaded.
     */
    @Test
    public void availableOfImsiEncryptionInfoIsUpdated() {
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(true));
        when(mCarrierConfigManager.getConfigForSubId(NON_DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN))
                .thenReturn(null);
        when(mNonDataTelephonyManager
                .getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN)).thenReturn(null);

        ArgumentCaptor<ContentObserver> observerCaptor =
                ArgumentCaptor.forClass(ContentObserver.class);
        verify(mFrameworkFacade).registerContentObserver(eq(mContext), any(Uri.class), eq(false),
                observerCaptor.capture());
        ContentObserver observer = observerCaptor.getValue();

        observer.onChange(false);

        assertTrue(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));
        assertFalse(mWifiCarrierInfoManager.isImsiEncryptionInfoAvailable(DATA_SUBID));

        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN))
                .thenReturn(mock(ImsiEncryptionInfo.class));

        observer.onChange(false);

        assertTrue(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));
        assertTrue(mWifiCarrierInfoManager.isImsiEncryptionInfoAvailable(DATA_SUBID));
    }

    /**
     * Verify that if the IMSI encryption information is cleared
     */
    @Test
    public void availableOfImsiEncryptionInfoIsCleared() {
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(true));
        when(mCarrierConfigManager.getConfigForSubId(NON_DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(true));
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN))
                .thenReturn(mock(ImsiEncryptionInfo.class));
        when(mNonDataTelephonyManager
                .getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN))
                        .thenReturn(mock(ImsiEncryptionInfo.class));

        ArgumentCaptor<ContentObserver> observerCaptor =
                ArgumentCaptor.forClass(ContentObserver.class);
        verify(mFrameworkFacade).registerContentObserver(eq(mContext), any(Uri.class), eq(false),
                observerCaptor.capture());
        ContentObserver observer = observerCaptor.getValue();

        observer.onChange(false);

        assertTrue(mWifiCarrierInfoManager.isImsiEncryptionInfoAvailable(DATA_SUBID));
        assertTrue(mWifiCarrierInfoManager.isImsiEncryptionInfoAvailable(NON_DATA_SUBID));

        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN))
                .thenReturn(null);
        when(mNonDataTelephonyManager
                .getCarrierInfoForImsiEncryption(TelephonyManager.KEY_TYPE_WLAN)).thenReturn(null);

        observer.onChange(false);

        assertFalse(mWifiCarrierInfoManager.isImsiEncryptionInfoAvailable(DATA_SUBID));
        assertFalse(mWifiCarrierInfoManager.isImsiEncryptionInfoAvailable(NON_DATA_SUBID));
    }

    @Test
    public void getSimIdentityEapSim() {
        final Pair<String, String> expectedIdentity = Pair.create(
                "13214561234567890@wlan.mnc456.mcc321.3gppnetwork.org", "");

        when(mDataTelephonyManager.getSubscriberId()).thenReturn("3214561234567890");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn("321456");
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(anyInt())).thenReturn(null);
        WifiConfiguration simConfig =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        simConfig.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(simConfig));

        WifiConfiguration peapSimConfig =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.PEAP,
                        WifiEnterpriseConfig.Phase2.SIM);
        peapSimConfig.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(peapSimConfig));
    }

    @Test
    public void getSimIdentityEapAka() {
        final Pair<String, String> expectedIdentity = Pair.create(
                "03214561234567890@wlan.mnc456.mcc321.3gppnetwork.org", "");
        when(mDataTelephonyManager.getSubscriberId()).thenReturn("3214561234567890");

        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn("321456");
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(anyInt())).thenReturn(null);
        WifiConfiguration akaConfig =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.AKA,
                        WifiEnterpriseConfig.Phase2.NONE);
        akaConfig.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(akaConfig));

        WifiConfiguration peapAkaConfig =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.PEAP,
                        WifiEnterpriseConfig.Phase2.AKA);
        peapAkaConfig.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(peapAkaConfig));
    }

    @Test
    public void getSimIdentityEapAkaPrime() {
        final Pair<String, String> expectedIdentity = Pair.create(
                "63214561234567890@wlan.mnc456.mcc321.3gppnetwork.org", "");

        when(mDataTelephonyManager.getSubscriberId()).thenReturn("3214561234567890");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn("321456");
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(anyInt())).thenReturn(null);
        WifiConfiguration akaPConfig =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.AKA_PRIME,
                        WifiEnterpriseConfig.Phase2.NONE);
        akaPConfig.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(akaPConfig));

        WifiConfiguration peapAkaPConfig =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.PEAP,
                        WifiEnterpriseConfig.Phase2.AKA_PRIME);
        peapAkaPConfig.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(peapAkaPConfig));
    }

    /**
     * Verify that an expected identity is returned when using the encrypted identity
     * encoded by RFC4648.
     */
    @Test
    public void getEncryptedIdentity_WithRfc4648() throws Exception {
        Cipher cipher = mock(Cipher.class);
        PublicKey key = null;
        String imsi = "3214561234567890";
        String permanentIdentity = "03214561234567890@wlan.mnc456.mcc321.3gppnetwork.org";
        String encryptedImsi = Base64.encodeToString(permanentIdentity.getBytes(), 0,
                permanentIdentity.getBytes().length, Base64.NO_WRAP);
        String encryptedIdentity = "\0" + encryptedImsi;
        final Pair<String, String> expectedIdentity = Pair.create(permanentIdentity,
                encryptedIdentity);

        // static mocking
        MockitoSession session = ExtendedMockito.mockitoSession().mockStatic(
                Cipher.class).startMocking();
        try {
            when(Cipher.getInstance(anyString())).thenReturn(cipher);
            when(cipher.doFinal(any(byte[].class))).thenReturn(permanentIdentity.getBytes());
            when(mDataTelephonyManager.getSubscriberId()).thenReturn(imsi);
            when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
            when(mDataTelephonyManager.getSimOperator()).thenReturn("321456");
            ImsiEncryptionInfo info = new ImsiEncryptionInfo("321", "456",
                    TelephonyManager.KEY_TYPE_WLAN, null, key, null);
            when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(
                    eq(TelephonyManager.KEY_TYPE_WLAN)))
                    .thenReturn(info);
            WifiConfiguration config =
                    WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.AKA,
                            WifiEnterpriseConfig.Phase2.NONE);
            config.carrierId = DATA_CARRIER_ID;

            assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(config));
        } finally {
            session.finishMocking();
        }
    }

    /**
     * Verify that {@code null} will be returned when IMSI encryption failed.
     *
     * @throws Exception
     */
    @Test
    public void getEncryptedIdentityFailed() throws Exception {
        Cipher cipher = mock(Cipher.class);
        String keyIdentifier = "key=testKey";
        String imsi = "3214561234567890";
        // static mocking
        MockitoSession session = ExtendedMockito.mockitoSession().mockStatic(
                Cipher.class).startMocking();
        try {
            when(Cipher.getInstance(anyString())).thenReturn(cipher);
            when(cipher.doFinal(any(byte[].class))).thenThrow(BadPaddingException.class);
            when(mDataTelephonyManager.getSubscriberId()).thenReturn(imsi);
            when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
            when(mDataTelephonyManager.getSimOperator()).thenReturn("321456");
            ImsiEncryptionInfo info = new ImsiEncryptionInfo("321", "456",
                    TelephonyManager.KEY_TYPE_WLAN, keyIdentifier, (PublicKey) null, null);
            when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(
                    eq(TelephonyManager.KEY_TYPE_WLAN)))
                    .thenReturn(info);

            WifiConfiguration config =
                    WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.AKA,
                            WifiEnterpriseConfig.Phase2.NONE);
            config.carrierId = DATA_CARRIER_ID;

            assertNull(mWifiCarrierInfoManager.getSimIdentity(config));
        } finally {
            session.finishMocking();
        }
    }

    @Test
    public void getSimIdentity2DigitMnc() {
        final Pair<String, String> expectedIdentity = Pair.create(
                "1321560123456789@wlan.mnc056.mcc321.3gppnetwork.org", "");

        when(mDataTelephonyManager.getSubscriberId()).thenReturn("321560123456789");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn("32156");
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(anyInt())).thenReturn(null);
        WifiConfiguration config =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        config.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(config));
    }

    @Test
    public void getSimIdentityUnknownMccMnc() {
        final Pair<String, String> expectedIdentity = Pair.create(
                "13214560123456789@wlan.mnc456.mcc321.3gppnetwork.org", "");

        when(mDataTelephonyManager.getSubscriberId()).thenReturn("3214560123456789");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_UNKNOWN);
        when(mDataTelephonyManager.getSimOperator()).thenReturn(null);
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(anyInt())).thenReturn(null);
        WifiConfiguration config =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        config.carrierId = DATA_CARRIER_ID;

        assertEquals(expectedIdentity, mWifiCarrierInfoManager.getSimIdentity(config));
    }

    @Test
    public void getSimIdentityNonTelephonyConfig() {
        when(mDataTelephonyManager.getSubscriberId()).thenReturn("321560123456789");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn("32156");

        assertEquals(null,
                mWifiCarrierInfoManager.getSimIdentity(WifiConfigurationTestUtil.createEapNetwork(
                        WifiEnterpriseConfig.Eap.TTLS, WifiEnterpriseConfig.Phase2.SIM)));
        assertEquals(null,
                mWifiCarrierInfoManager.getSimIdentity(WifiConfigurationTestUtil.createEapNetwork(
                        WifiEnterpriseConfig.Eap.PEAP, WifiEnterpriseConfig.Phase2.MSCHAPV2)));
        assertEquals(null,
                mWifiCarrierInfoManager.getSimIdentity(WifiConfigurationTestUtil.createEapNetwork(
                        WifiEnterpriseConfig.Eap.TLS, WifiEnterpriseConfig.Phase2.NONE)));
        assertEquals(null,
                mWifiCarrierInfoManager.getSimIdentity(new WifiConfiguration()));
    }

    /**
     * Produce a base64 encoded length byte + data.
     */
    private static String createSimChallengeRequest(byte[] challengeValue) {
        byte[] challengeLengthAndValue = new byte[challengeValue.length + 1];
        challengeLengthAndValue[0] = (byte) challengeValue.length;
        for (int i = 0; i < challengeValue.length; ++i) {
            challengeLengthAndValue[i + 1] = challengeValue[i];
        }
        return Base64.encodeToString(challengeLengthAndValue, android.util.Base64.NO_WRAP);
    }

    /**
     * Produce a base64 encoded data without length.
     */
    private static String create2gUsimChallengeRequest(byte[] challengeValue) {
        return Base64.encodeToString(challengeValue, android.util.Base64.NO_WRAP);
    }

    /**
     * Produce a base64 encoded sres length byte + sres + kc length byte + kc.
     */
    private static String createGsmSimAuthResponse(byte[] sresValue, byte[] kcValue) {
        int overallLength = sresValue.length + kcValue.length + 2;
        byte[] result = new byte[sresValue.length + kcValue.length + 2];
        int idx = 0;
        result[idx++] = (byte) sresValue.length;
        for (int i = 0; i < sresValue.length; ++i) {
            result[idx++] = sresValue[i];
        }
        result[idx++] = (byte) kcValue.length;
        for (int i = 0; i < kcValue.length; ++i) {
            result[idx++] = kcValue[i];
        }
        return Base64.encodeToString(result, Base64.NO_WRAP);
    }

    /**
     * Produce a base64 encoded sres + kc without length.
     */
    private static String create2gUsimAuthResponse(byte[] sresValue, byte[] kcValue) {
        int overallLength = sresValue.length + kcValue.length;
        byte[] result = new byte[sresValue.length + kcValue.length];
        int idx = 0;
        for (int i = 0; i < sresValue.length; ++i) {
            result[idx++] = sresValue[i];
        }
        for (int i = 0; i < kcValue.length; ++i) {
            result[idx++] = kcValue[i];
        }
        return Base64.encodeToString(result, Base64.NO_WRAP);
    }

    @Test
    public void getGsmSimAuthResponseInvalidRequest() {
        final String[] invalidRequests = { null, "", "XXXX" };
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals("", mWifiCarrierInfoManager.getGsmSimAuthResponse(invalidRequests, config));
    }

    @Test
    public void getGsmSimAuthResponseFailedSimResponse() {
        final String[] failedRequests = { "5E5F" };
        when(mDataTelephonyManager.getIccAuthentication(anyInt(), anyInt(),
                eq(createSimChallengeRequest(new byte[] { 0x5e, 0x5f })))).thenReturn(null);
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(null, mWifiCarrierInfoManager.getGsmSimAuthResponse(failedRequests, config));
    }

    @Test
    public void getGsmSimAuthResponseUsim() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        createSimChallengeRequest(new byte[] { 0x1b, 0x2b })))
                .thenReturn(createGsmSimAuthResponse(new byte[] { 0x1D, 0x2C },
                                new byte[] { 0x3B, 0x4A }));
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        createSimChallengeRequest(new byte[] { 0x01, 0x22 })))
                .thenReturn(createGsmSimAuthResponse(new byte[] { 0x11, 0x11 },
                                new byte[] { 0x12, 0x34 }));
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(":3b4a:1d2c:1234:1111", mWifiCarrierInfoManager.getGsmSimAuthResponse(
                        new String[] { "1B2B", "0122" }, config));
    }

    @Test
    public void getGsmSimpleSimAuthResponseInvalidRequest() {
        final String[] invalidRequests = { null, "", "XXXX" };
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals("",
                mWifiCarrierInfoManager.getGsmSimpleSimAuthResponse(invalidRequests, config));
    }

    @Test
    public void getGsmSimpleSimAuthResponseFailedSimResponse() {
        final String[] failedRequests = { "5E5F" };
        when(mDataTelephonyManager.getIccAuthentication(anyInt(), anyInt(),
                eq(createSimChallengeRequest(new byte[] { 0x5e, 0x5f })))).thenReturn(null);
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(null,
                mWifiCarrierInfoManager.getGsmSimpleSimAuthResponse(failedRequests, config));
    }

    @Test
    public void getGsmSimpleSimAuthResponse() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_SIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        createSimChallengeRequest(new byte[] { 0x1a, 0x2b })))
                .thenReturn(createGsmSimAuthResponse(new byte[] { 0x1D, 0x2C },
                                new byte[] { 0x3B, 0x4A }));
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_SIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        createSimChallengeRequest(new byte[] { 0x01, 0x23 })))
                .thenReturn(createGsmSimAuthResponse(new byte[] { 0x33, 0x22 },
                                new byte[] { 0x11, 0x00 }));
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(":3b4a:1d2c:1100:3322", mWifiCarrierInfoManager.getGsmSimpleSimAuthResponse(
                        new String[] { "1A2B", "0123" }, config));
    }

    @Test
    public void getGsmSimpleSimNoLengthAuthResponseInvalidRequest() {
        final String[] invalidRequests = { null, "", "XXXX" };
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals("", mWifiCarrierInfoManager.getGsmSimpleSimNoLengthAuthResponse(
                invalidRequests, config));
    }

    @Test
    public void getGsmSimpleSimNoLengthAuthResponseFailedSimResponse() {
        final String[] failedRequests = { "5E5F" };
        when(mDataTelephonyManager.getIccAuthentication(anyInt(), anyInt(),
                eq(create2gUsimChallengeRequest(new byte[] { 0x5e, 0x5f })))).thenReturn(null);
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(null, mWifiCarrierInfoManager.getGsmSimpleSimNoLengthAuthResponse(
                failedRequests, config));
    }

    @Test
    public void getGsmSimpleSimNoLengthAuthResponse() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_SIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        create2gUsimChallengeRequest(new byte[] { 0x1a, 0x2b })))
                .thenReturn(create2gUsimAuthResponse(new byte[] { 0x1a, 0x2b, 0x3c, 0x4d },
                                new byte[] { 0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x7a, 0x1a }));
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_SIM,
                        TelephonyManager.AUTHTYPE_EAP_SIM,
                        create2gUsimChallengeRequest(new byte[] { 0x01, 0x23 })))
                .thenReturn(create2gUsimAuthResponse(new byte[] { 0x12, 0x34, 0x56, 0x78 },
                                new byte[] { 0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78 }));
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(":1a2b3c4d5e6f7a1a:1a2b3c4d:1234567812345678:12345678",
                mWifiCarrierInfoManager.getGsmSimpleSimNoLengthAuthResponse(
                        new String[] { "1A2B", "0123" }, config));
    }

    /**
     * Produce a base64 encoded tag + res length byte + res + ck length byte + ck + ik length byte +
     * ik.
     */
    private static String create3GSimAuthUmtsAuthResponse(byte[] res, byte[] ck, byte[] ik) {
        byte[] result = new byte[res.length + ck.length + ik.length + 4];
        int idx = 0;
        result[idx++] = (byte) 0xdb;
        result[idx++] = (byte) res.length;
        for (int i = 0; i < res.length; ++i) {
            result[idx++] = res[i];
        }
        result[idx++] = (byte) ck.length;
        for (int i = 0; i < ck.length; ++i) {
            result[idx++] = ck[i];
        }
        result[idx++] = (byte) ik.length;
        for (int i = 0; i < ik.length; ++i) {
            result[idx++] = ik[i];
        }
        return Base64.encodeToString(result, Base64.NO_WRAP);
    }

    private static String create3GSimAuthUmtsAutsResponse(byte[] auts) {
        byte[] result = new byte[auts.length + 2];
        int idx = 0;
        result[idx++] = (byte) 0xdc;
        result[idx++] = (byte) auts.length;
        for (int i = 0; i < auts.length; ++i) {
            result[idx++] = auts[i];
        }
        return Base64.encodeToString(result, Base64.NO_WRAP);
    }

    @Test
    public void get3GAuthResponseInvalidRequest() {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(null, mWifiCarrierInfoManager.get3GAuthResponse(
                new SimAuthRequestData(0, 0, "SSID", new String[]{"0123"}), config));
        assertEquals(null, mWifiCarrierInfoManager.get3GAuthResponse(
                new SimAuthRequestData(0, 0, "SSID", new String[]{"xyz2", "1234"}),
                config));
        verifyNoMoreInteractions(mDataTelephonyManager);
    }

    @Test
    public void get3GAuthResponseNullIccAuthentication() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA, "AgEjAkVn")).thenReturn(null);
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        SimAuthResponseData response = mWifiCarrierInfoManager.get3GAuthResponse(
                new SimAuthRequestData(0, 0, "SSID", new String[]{"0123", "4567"}),
                config);

        assertNull(response);
    }

    @Test
    public void get3GAuthResponseIccAuthenticationTooShort() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA, "AgEjAkVn"))
                .thenReturn(Base64.encodeToString(new byte[] {(byte) 0xdc}, Base64.NO_WRAP));
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        SimAuthResponseData response = mWifiCarrierInfoManager.get3GAuthResponse(
                new SimAuthRequestData(0, 0, "SSID", new String[]{"0123", "4567"}),
                config);

        assertNull(response);
    }

    @Test
    public void get3GAuthResponseBadTag() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA, "AgEjAkVn"))
                .thenReturn(Base64.encodeToString(new byte[] {0x31, 0x1, 0x2, 0x3, 0x4},
                                Base64.NO_WRAP));
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        SimAuthResponseData response = mWifiCarrierInfoManager.get3GAuthResponse(
                new SimAuthRequestData(0, 0, "SSID", new String[]{"0123", "4567"}),
                config);

        assertNull(response);
    }

    @Test
    public void get3GAuthResponseUmtsAuth() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA, "AgEjAkVn"))
                .thenReturn(create3GSimAuthUmtsAuthResponse(new byte[] {0x11, 0x12},
                                new byte[] {0x21, 0x22, 0x23}, new byte[] {0x31}));
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        SimAuthResponseData response = mWifiCarrierInfoManager.get3GAuthResponse(
                new SimAuthRequestData(0, 0, "SSID", new String[]{"0123", "4567"}),
                config);

        assertNotNull(response);
        assertEquals("UMTS-AUTH", response.type);
        assertEquals(":31:212223:1112", response.response);
    }

    @Test
    public void get3GAuthResponseUmtsAuts() {
        when(mDataTelephonyManager.getIccAuthentication(TelephonyManager.APPTYPE_USIM,
                        TelephonyManager.AUTHTYPE_EAP_AKA, "AgEjAkVn"))
                .thenReturn(create3GSimAuthUmtsAutsResponse(new byte[] {0x22, 0x33}));
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        SimAuthResponseData response = mWifiCarrierInfoManager.get3GAuthResponse(
                new SimAuthRequestData(0, 0, "SSID", new String[]{"0123", "4567"}),
                config);
        assertNotNull(response);
        assertEquals("UMTS-AUTS", response.type);
        assertEquals(":2233", response.response);
    }

    /**
     * Verify that anonymous identity should be a valid format based on MCC/MNC of current SIM.
     */
    @Test
    public void getAnonymousIdentityWithSim() {
        String mccmnc = "123456";
        String expectedIdentity = ANONYMOUS_IDENTITY;
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn(mccmnc);
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(expectedIdentity,
                mWifiCarrierInfoManager.getAnonymousIdentityWith3GppRealm(config));
    }

    /**
     * Verify that anonymous identity should be {@code null} when SIM is absent.
     */
    @Test
    public void getAnonymousIdentityWithoutSim() {
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_ABSENT);
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);

        assertNull(mWifiCarrierInfoManager.getAnonymousIdentityWith3GppRealm(config));
    }

    /**
     * Verify SIM is present.
     */
    @Test
    public void isSimPresentWithValidSubscriptionIdList() {
        SubscriptionInfo subInfo1 = mock(SubscriptionInfo.class);
        when(subInfo1.getSubscriptionId()).thenReturn(DATA_SUBID);
        SubscriptionInfo subInfo2 = mock(SubscriptionInfo.class);
        when(subInfo2.getSubscriptionId()).thenReturn(NON_DATA_SUBID);
        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Arrays.asList(subInfo1, subInfo2));
        assertTrue(mWifiCarrierInfoManager.isSimPresent(DATA_SUBID));
    }

    /**
     * Verify SIM is not present.
     */
    @Test
    public void isSimPresentWithInvalidOrEmptySubscriptionIdList() {
        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Collections.emptyList());

        assertFalse(mWifiCarrierInfoManager.isSimPresent(DATA_SUBID));

        SubscriptionInfo subInfo = mock(SubscriptionInfo.class);
        when(subInfo.getSubscriptionId()).thenReturn(NON_DATA_SUBID);
        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Arrays.asList(subInfo));
        assertFalse(mWifiCarrierInfoManager.isSimPresent(DATA_SUBID));
    }

    /**
     * Verity SIM is consider not present when SIM state is not ready
     */
    @Test
    public void isSimPresentWithValidSubscriptionIdListWithSimStateNotReady() {
        SubscriptionInfo subInfo1 = mock(SubscriptionInfo.class);
        when(subInfo1.getSubscriptionId()).thenReturn(DATA_SUBID);
        SubscriptionInfo subInfo2 = mock(SubscriptionInfo.class);
        when(subInfo2.getSubscriptionId()).thenReturn(NON_DATA_SUBID);
        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Arrays.asList(subInfo1, subInfo2));
        when(mTelephonyManager.getSimState(anyInt()))
                .thenReturn(TelephonyManager.SIM_STATE_NETWORK_LOCKED);
        assertFalse(mWifiCarrierInfoManager.isSimPresent(DATA_SUBID));
    }

    /**
     * The active SubscriptionInfo List may be null or empty from Telephony.
     */
    @Test
    public void getBestMatchSubscriptionIdWithEmptyActiveSubscriptionInfoList() {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        when(mSubscriptionManager.getActiveSubscriptionInfoList()).thenReturn(null);
        when(mSubscriptionManager.getActiveSubscriptionIdList()).thenReturn(new int[0]);

        assertEquals(INVALID_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(config));

        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Collections.emptyList());

        assertEquals(INVALID_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(config));
    }

    /**
     * The matched Subscription ID should be that of data SIM when carrier ID is not specified.
     */
    @Test
    public void getBestMatchSubscriptionIdForEnterpriseWithoutCarrierIdFieldForSimConfig() {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(DATA_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(config));
    }

    /**
     * The matched Subscription ID should be invalid if the configuration does not require
     * SIM card and the carrier ID is not specified.
     */
    @Test
    public void getBestMatchSubscriptionIdForEnterpriseWithoutCarrierIdFieldForNonSimConfig() {
        WifiConfiguration config = new WifiConfiguration();

        assertEquals(INVALID_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(config));
    }

    /**
     * If the carrier ID is specifed for EAP-SIM configuration, the corresponding Subscription ID
     * should be returned.
     */
    @Test
    public void getBestMatchSubscriptionIdForEnterpriseWithNonDataCarrierId() {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        config.carrierId = NON_DATA_CARRIER_ID;

        assertEquals(NON_DATA_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(config));

        config.carrierId = DATA_CARRIER_ID;
        assertEquals(DATA_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(config));
    }

    /**
     * If the passpoint profile have valid carrier ID, the matching sub ID should be returned.
     */
    @Test
    public void getBestMatchSubscriptionIdForPasspointWithValidCarrierId() {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        config.carrierId = DATA_CARRIER_ID;
        WifiConfiguration spyConfig = spy(config);
        doReturn(true).when(spyConfig).isPasspoint();

        assertEquals(DATA_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(spyConfig));
    }

    /**
     * If there is no matching SIM card, the matching sub ID should be invalid.
     */
    @Test
    public void getBestMatchSubscriptionIdForPasspointInvalidCarrierId() {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        WifiConfiguration spyConfig = spy(config);
        doReturn(true).when(spyConfig).isPasspoint();

        assertEquals(INVALID_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(spyConfig));
    }

    /**
     * The matched Subscription ID should be invalid if the SIM card for the specified carrier ID
     * is absent.
     */
    @Test
    public void getBestMatchSubscriptionIdWithDeactiveCarrierId() {
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.AKA, WifiEnterpriseConfig.Phase2.NONE);
        config.carrierId = DEACTIVE_CARRIER_ID;

        assertEquals(INVALID_SUBID, mWifiCarrierInfoManager.getBestMatchSubscriptionId(config));
    }

    /**
     * Verify that the result is null if no active SIM is matched.
     */
    @Test
    public void getMatchingImsiCarrierIdWithDeactiveCarrierId() {
        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Collections.emptyList());

        assertNull(mWifiCarrierInfoManager.getMatchingImsi(DEACTIVE_CARRIER_ID));
    }

    /**
     * Verify that a SIM is matched with carrier ID, and it requires IMSI encryption,
     * when the IMSI encryption info is not available, it should return null.
     */
    @Test
    public void getMatchingImsiCarrierIdWithValidCarrierIdForImsiEncryptionCheck() {
        WifiCarrierInfoManager spyTu = spy(mWifiCarrierInfoManager);
        doReturn(true).when(spyTu).requiresImsiEncryption(DATA_SUBID);
        doReturn(false).when(spyTu).isImsiEncryptionInfoAvailable(DATA_SUBID);

        assertNull(spyTu.getMatchingImsi(DATA_CARRIER_ID));
    }

    /**
     * Verify that if there is SIM card whose carrier ID is the same as the input, the correct IMSI
     * and carrier ID would be returned.
     */
    @Test
    public void getMatchingImsiCarrierIdWithValidCarrierId() {
        assertEquals(DATA_FULL_IMSI,
                mWifiCarrierInfoManager.getMatchingImsi(DATA_CARRIER_ID));
    }

    /**
     * Verify that if there is no SIM, it should match nothing.
     */
    @Test
    public void getMatchingImsiCarrierIdWithEmptyActiveSubscriptionInfoList() {
        when(mSubscriptionManager.getActiveSubscriptionInfoList()).thenReturn(null);

        assertNull(mWifiCarrierInfoManager.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));

        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Collections.emptyList());

        assertNull(mWifiCarrierInfoManager.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));
    }

    /**
     * Verify that if there is no matching SIM, it should match nothing.
     */
    @Test
    public void getMatchingImsiCarrierIdWithNoMatchImsi() {
        // data SIM is MNO.
        when(mDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSimCarrierId()).thenReturn(DATA_CARRIER_ID);
        // non data SIM is MNO.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);

        assertNull(mWifiCarrierInfoManager.getMatchingImsiCarrierId(NO_MATCH_PREFIX_IMSI));
    }

    /**
     * Verify that if the matched SIM is the default data SIM and a MNO SIM, the information of it
     * should be returned.
     */
    @Test
    public void getMatchingImsiCarrierIdForDataAndMnoSimMatch() {
        // data SIM is MNO.
        when(mDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSimCarrierId()).thenReturn(DATA_CARRIER_ID);
        // non data SIM is MNO.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);

        Pair<String, Integer> ic = mWifiCarrierInfoManager
                .getMatchingImsiCarrierId(MATCH_PREFIX_IMSI);

        assertEquals(new Pair<>(DATA_FULL_IMSI, DATA_CARRIER_ID), ic);

        // non data SIM is MVNO
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc())
                .thenReturn(PARENT_NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);

        assertEquals(new Pair<>(DATA_FULL_IMSI, DATA_CARRIER_ID),
                mWifiCarrierInfoManager.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));

        // non data SIM doesn't match.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSubscriberId()).thenReturn(NO_MATCH_FULL_IMSI);
        when(mNonDataTelephonyManager.getSimOperator())
                .thenReturn(NO_MATCH_OPERATOR_NUMERIC);

        assertEquals(new Pair<>(DATA_FULL_IMSI, DATA_CARRIER_ID),
                mWifiCarrierInfoManager.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));
    }

    /**
     * Verify that if the matched SIM is the default data SIM and a MVNO SIM, and no MNO SIM was
     * matched, the information of it should be returned.
     */
    @Test
    public void getMatchingImsiCarrierIdForDataAndMvnoSimMatch() {
        // data SIM is MVNO.
        when(mDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(PARENT_DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSimCarrierId()).thenReturn(DATA_CARRIER_ID);
        // non data SIM is MVNO.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc())
                .thenReturn(PARENT_NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);

        Pair<String, Integer> ic = mWifiCarrierInfoManager
                .getMatchingImsiCarrierId(MATCH_PREFIX_IMSI);

        assertEquals(new Pair<>(DATA_FULL_IMSI, DATA_CARRIER_ID), ic);

        // non data SIM doesn't match.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSubscriberId()).thenReturn(NO_MATCH_FULL_IMSI);
        when(mNonDataTelephonyManager.getSimOperator())
                .thenReturn(NO_MATCH_OPERATOR_NUMERIC);

        assertEquals(new Pair<>(DATA_FULL_IMSI, DATA_CARRIER_ID),
                mWifiCarrierInfoManager.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));
    }

    /**
     * Verify that if the matched SIM is a MNO SIM, even the default data SIM is matched as a MVNO
     * SIM, the information of MNO SIM still should be returned.
     */
    @Test
    public void getMatchingImsiCarrierIdForNonDataAndMnoSimMatch() {
        // data SIM is MVNO.
        when(mDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(PARENT_DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSimCarrierId()).thenReturn(DATA_CARRIER_ID);
        // non data SIM is MNO.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);


        Pair<String, Integer> ic = mWifiCarrierInfoManager
                .getMatchingImsiCarrierId(MATCH_PREFIX_IMSI);

        assertEquals(new Pair<>(NON_DATA_FULL_IMSI, NON_DATA_CARRIER_ID), ic);

        // data SIM doesn't match
        when(mDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSimCarrierId()).thenReturn(DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSubscriberId()).thenReturn(NO_MATCH_FULL_IMSI);
        when(mDataTelephonyManager.getSimOperator()).thenReturn(NO_MATCH_OPERATOR_NUMERIC);

        assertEquals(new Pair<>(NON_DATA_FULL_IMSI, NON_DATA_CARRIER_ID),
                mWifiCarrierInfoManager.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));
    }

    /**
     * Verify that if only a MVNO SIM is matched, the information of it should be returned.
     */
    @Test
    public void getMatchingImsiCarrierIdForMvnoSimMatch() {
        // data SIM is MNO, but IMSI doesn't match.
        when(mDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSimCarrierId()).thenReturn(DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSubscriberId()).thenReturn(NO_MATCH_FULL_IMSI);
        when(mDataTelephonyManager.getSimOperator()).thenReturn(NO_MATCH_OPERATOR_NUMERIC);
        // non data SIM is MVNO.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc())
                .thenReturn(PARENT_NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);

        assertEquals(new Pair<>(NON_DATA_FULL_IMSI, NON_DATA_CARRIER_ID),
                mWifiCarrierInfoManager.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));
    }

    /**
     * Verify that a SIM is matched, and it requires IMSI encryption, when the IMSI encryption
     * info is not available, it should return null.
     */
    @Test
    public void getMatchingImsiCarrierIdForImsiEncryptionCheck() {
        // data SIM is MNO.
        when(mDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(DATA_CARRIER_ID);
        when(mDataTelephonyManager.getSimCarrierId()).thenReturn(DATA_CARRIER_ID);
        // non data SIM does not match.
        when(mNonDataTelephonyManager.getCarrierIdFromSimMccMnc()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSimCarrierId()).thenReturn(NON_DATA_CARRIER_ID);
        when(mNonDataTelephonyManager.getSubscriberId()).thenReturn(NO_MATCH_FULL_IMSI);
        when(mNonDataTelephonyManager.getSimOperator())
                .thenReturn(NO_MATCH_OPERATOR_NUMERIC);
        WifiCarrierInfoManager spyTu = spy(mWifiCarrierInfoManager);
        doReturn(true).when(spyTu).requiresImsiEncryption(eq(DATA_SUBID));
        doReturn(false).when(spyTu).isImsiEncryptionInfoAvailable(eq(DATA_SUBID));

        assertNull(spyTu.getMatchingImsiCarrierId(MATCH_PREFIX_IMSI));
    }

    /**
     * Verify that if there is no any SIM card, the carrier ID should be updated.
     */
    @Test
    public void tryUpdateCarrierIdForPasspointWithEmptyActiveSubscriptionList() {
        PasspointConfiguration config = mock(PasspointConfiguration.class);
        when(config.getCarrierId()).thenReturn(DATA_CARRIER_ID);
        when(mSubscriptionManager.getActiveSubscriptionInfoList()).thenReturn(null);

        assertFalse(mWifiCarrierInfoManager.tryUpdateCarrierIdForPasspoint(config));

        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Collections.emptyList());

        assertFalse(mWifiCarrierInfoManager.tryUpdateCarrierIdForPasspoint(config));
    }

    /**
     * Verify that if the carrier ID has been assigned, it shouldn't be updated.
     */
    @Test
    public void tryUpdateCarrierIdForPasspointWithValidCarrieId() {
        PasspointConfiguration config = mock(PasspointConfiguration.class);
        when(config.getCarrierId()).thenReturn(DATA_CARRIER_ID);

        assertFalse(mWifiCarrierInfoManager.tryUpdateCarrierIdForPasspoint(config));
    }

    /**
     * Verify that if the passpoint profile doesn't have SIM credential, it shouldn't be updated.
     */
    @Test
    public void tryUpdateCarrierIdForPasspointWithNonSimCredential() {
        Credential credential = mock(Credential.class);
        PasspointConfiguration spyConfig = spy(new PasspointConfiguration());
        doReturn(credential).when(spyConfig).getCredential();
        when(credential.getSimCredential()).thenReturn(null);

        assertFalse(mWifiCarrierInfoManager.tryUpdateCarrierIdForPasspoint(spyConfig));
    }

    /**
     * Verify that if the passpoint profile only have IMSI prefix(mccmnc*) parameter,
     * it shouldn't be updated.
     */
    @Test
    public void tryUpdateCarrierIdForPasspointWithPrefixImsi() {
        Credential credential = mock(Credential.class);
        PasspointConfiguration spyConfig = spy(new PasspointConfiguration());
        doReturn(credential).when(spyConfig).getCredential();
        Credential.SimCredential simCredential = mock(Credential.SimCredential.class);
        when(credential.getSimCredential()).thenReturn(simCredential);
        when(simCredential.getImsi()).thenReturn(MATCH_PREFIX_IMSI);

        assertFalse(mWifiCarrierInfoManager.tryUpdateCarrierIdForPasspoint(spyConfig));
    }

    /**
     * Verify that if the passpoint profile has the full IMSI and wasn't assigned valid
     * carrier ID, it should be updated.
     */
    @Test
    public void tryUpdateCarrierIdForPasspointWithFullImsiAndActiveSim() {
        Credential credential = mock(Credential.class);
        PasspointConfiguration spyConfig = spy(new PasspointConfiguration());
        doReturn(credential).when(spyConfig).getCredential();
        Credential.SimCredential simCredential = mock(Credential.SimCredential.class);
        when(credential.getSimCredential()).thenReturn(simCredential);
        when(simCredential.getImsi()).thenReturn(DATA_FULL_IMSI);

        assertTrue(mWifiCarrierInfoManager.tryUpdateCarrierIdForPasspoint(spyConfig));
        assertEquals(DATA_CARRIER_ID, spyConfig.getCarrierId());
    }

    /**
     * Verify that if there is no SIM card matching the given IMSI, it shouldn't be updated.
     */
    @Test
    public void tryUpdateCarrierIdForPasspointWithFullImsiAndInactiveSim() {
        Credential credential = mock(Credential.class);
        PasspointConfiguration spyConfig = spy(new PasspointConfiguration());
        doReturn(credential).when(spyConfig).getCredential();
        Credential.SimCredential simCredential = mock(Credential.SimCredential.class);
        when(credential.getSimCredential()).thenReturn(simCredential);
        when(simCredential.getImsi()).thenReturn(NO_MATCH_PREFIX_IMSI);

        assertFalse(mWifiCarrierInfoManager.tryUpdateCarrierIdForPasspoint(spyConfig));
    }

    private void testIdentityWithSimAndEapAkaMethodPrefix(int method, String methodStr)
            throws Exception {
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(true, true));
        when(mCarrierConfigManager.getConfigForSubId(NON_DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));

        assertTrue(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        String mccmnc = "123456";
        String expectedIdentity = methodStr + ANONYMOUS_IDENTITY;
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn(mccmnc);
        WifiConfiguration config = WifiConfigurationTestUtil.createEapNetwork(
                method, WifiEnterpriseConfig.Phase2.NONE);

        assertEquals(expectedIdentity,
                mWifiCarrierInfoManager.getAnonymousIdentityWith3GppRealm(config));
    }

    /**
     * Verify that EAP Method prefix is added to the anonymous identity when required
     */
    @Test
    public void getAnonymousIdentityWithSimAndEapAkaMethodPrefix() throws Exception {
        testIdentityWithSimAndEapAkaMethodPrefix(WifiEnterpriseConfig.Eap.AKA, "0");
    }

    /**
     * Verify that EAP Method prefix is added to the anonymous identity when required
     */
    @Test
    public void getAnonymousIdentityWithSimAndEapSimMethodPrefix() throws Exception {
        testIdentityWithSimAndEapAkaMethodPrefix(WifiEnterpriseConfig.Eap.SIM, "1");
    }

    /**
     * Verify that EAP Method prefix is added to the anonymous identity when required
     */
    @Test
    public void getAnonymousIdentityWithSimAndEapAkaPrimeMethodPrefix() throws Exception {
        testIdentityWithSimAndEapAkaMethodPrefix(WifiEnterpriseConfig.Eap.AKA_PRIME, "6");
    }

    /**
     * Verify that isAnonymousAtRealmIdentity works as expected for anonymous identities with and
     * without a prefix.
     */
    @Test
    public void testIsAnonymousAtRealmIdentity() throws Exception {
        assertTrue(mWifiCarrierInfoManager.isAnonymousAtRealmIdentity(ANONYMOUS_IDENTITY));
        assertTrue(mWifiCarrierInfoManager.isAnonymousAtRealmIdentity("0" + ANONYMOUS_IDENTITY));
        assertTrue(mWifiCarrierInfoManager.isAnonymousAtRealmIdentity("1" + ANONYMOUS_IDENTITY));
        assertTrue(mWifiCarrierInfoManager.isAnonymousAtRealmIdentity("6" + ANONYMOUS_IDENTITY));
        assertFalse(mWifiCarrierInfoManager.isAnonymousAtRealmIdentity("AKA" + ANONYMOUS_IDENTITY));
    }

    /**
     * Verify when no subscription available, get carrier id for target package will return
     * UNKNOWN_CARRIER_ID.
     */
    @Test
    public void getCarrierPrivilegeWithNoActiveSubscription() {
        when(mSubscriptionManager.getActiveSubscriptionInfoList()).thenReturn(null);
        assertEquals(TelephonyManager.UNKNOWN_CARRIER_ID,
                mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE));

        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Collections.emptyList());
        assertEquals(TelephonyManager.UNKNOWN_CARRIER_ID,
                mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE));
    }

    /**
     * Verify when package has no carrier privileges, get carrier id for that package will return
     * UNKNOWN_CARRIER_ID.
     */
    @Test
    public void getCarrierPrivilegeWithPackageHasNoPrivilege() {
        SubscriptionInfo subInfo = mock(SubscriptionInfo.class);
        when(subInfo.getSubscriptionId()).thenReturn(DATA_SUBID);
        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Arrays.asList(subInfo));
        when(mDataTelephonyManager.checkCarrierPrivilegesForPackage(TEST_PACKAGE))
                .thenReturn(TelephonyManager.CARRIER_PRIVILEGE_STATUS_NO_ACCESS);
        assertEquals(TelephonyManager.UNKNOWN_CARRIER_ID,
                mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE));
    }

    /**
     * Verify when package get carrier privileges from carrier, get carrier id for that package will
     * return the carrier id for that carrier.
     */
    @Test
    public void getCarrierPrivilegeWithPackageHasPrivilege() {
        SubscriptionInfo subInfo = mock(SubscriptionInfo.class);
        when(subInfo.getSubscriptionId()).thenReturn(DATA_SUBID);
        when(subInfo.getCarrierId()).thenReturn(DATA_CARRIER_ID);
        when(mSubscriptionManager.getActiveSubscriptionInfoList())
                .thenReturn(Arrays.asList(subInfo));
        when(mDataTelephonyManager.checkCarrierPrivilegesForPackage(TEST_PACKAGE))
                .thenReturn(TelephonyManager.CARRIER_PRIVILEGE_STATUS_HAS_ACCESS);
        assertEquals(DATA_CARRIER_ID,
                mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE));
    }

    /**
     * Verify getCarrierNameforSubId returns right value.
     */
    @Test
    public void getCarrierNameFromSubId() {
        assertEquals(CARRIER_NAME, mWifiCarrierInfoManager.getCarrierNameforSubId(DATA_SUBID));
        assertNull(mWifiCarrierInfoManager.getCarrierNameforSubId(NON_DATA_SUBID));
    }

    @Test
    public void testIsCarrierNetworkFromNonDataSim() {
        WifiConfiguration config = new WifiConfiguration();
        assertFalse(mWifiCarrierInfoManager.isCarrierNetworkFromNonDefaultDataSim(config));
        config.carrierId = DATA_CARRIER_ID;
        assertFalse(mWifiCarrierInfoManager.isCarrierNetworkFromNonDefaultDataSim(config));
        config.carrierId = NON_DATA_CARRIER_ID;
        assertTrue(mWifiCarrierInfoManager.isCarrierNetworkFromNonDefaultDataSim(config));
    }

    @Test
    public void testCheckSetClearImsiProtectionExemption() {
        InOrder inOrder = inOrder(mWifiConfigManager);
        assertFalse(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
        mWifiCarrierInfoManager.setHasUserApprovedImsiPrivacyExemptionForCarrier(true,
                DATA_CARRIER_ID);
        verify(mListener).onUserAllowed(DATA_CARRIER_ID);
        inOrder.verify(mWifiConfigManager).saveToStore(true);
        assertTrue(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
        mWifiCarrierInfoManager.clearImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID);
        inOrder.verify(mWifiConfigManager).saveToStore(true);
        assertFalse(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
    }

    @Test
    public void testSendImsiProtectionExemptionNotificationWithUserAllowed() {
        // Setup carrier without IMSI privacy protection
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
        // Simulate user clicking on allow in the notification.
        sendBroadcastForUserActionOnImsi(NOTIFICATION_USER_ALLOWED_CARRIER_INTENT_ACTION,
                CARRIER_NAME, DATA_CARRIER_ID);
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);
        verify(mWifiMetrics).addUserApprovalCarrierUiReaction(
                WifiCarrierInfoManager.ACTION_USER_ALLOWED_CARRIER, false);
        verify(mWifiConfigManager).saveToStore(true);
        assertTrue(mImsiDataSource.hasNewDataToSerialize());
        assertTrue(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
        verify(mListener).onUserAllowed(DATA_CARRIER_ID);
        verify(mWifiMetrics).addUserApprovalCarrierUiReaction(
                WifiCarrierInfoManager.ACTION_USER_ALLOWED_CARRIER, false);
    }

    @Test
    public void testSendImsiProtectionExemptionNotificationWithUserDisallowed() {
        // Setup carrier without IMSI privacy protection
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
        // Simulate user clicking on disallow in the notification.
        sendBroadcastForUserActionOnImsi(NOTIFICATION_USER_DISALLOWED_CARRIER_INTENT_ACTION,
                CARRIER_NAME, DATA_CARRIER_ID);
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);
        verify(mAlertDialog, never()).show();

        verify(mWifiConfigManager).saveToStore(true);
        assertTrue(mImsiDataSource.hasNewDataToSerialize());
        assertFalse(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
        verify(mListener, never()).onUserAllowed(DATA_CARRIER_ID);
        verify(mWifiMetrics).addUserApprovalCarrierUiReaction(
                WifiCarrierInfoManager.ACTION_USER_DISALLOWED_CARRIER, false);
    }

    @Test
    public void testSendImsiProtectionExemptionNotificationWithUserDismissal() {
        // Setup carrier without IMSI privacy protection
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
        //Simulate user dismissal the notification
        sendBroadcastForUserActionOnImsi(NOTIFICATION_USER_DISMISSED_INTENT_ACTION,
                CARRIER_NAME, DATA_CARRIER_ID);
        verify(mWifiMetrics).addUserApprovalCarrierUiReaction(
                WifiCarrierInfoManager.ACTION_USER_DISMISS, false);
        reset(mNotificationManger);
        // No Notification is active, should send notification again.
        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
        reset(mNotificationManger);

        // As there is notification is active, should not send notification again.
        sendBroadcastForUserActionOnImsi(NOTIFICATION_USER_DISMISSED_INTENT_ACTION,
                CARRIER_NAME, DATA_CARRIER_ID);
        verifyNoMoreInteractions(mNotificationManger);
        verify(mWifiConfigManager, never()).saveToStore(true);
        assertFalse(mImsiDataSource.hasNewDataToSerialize());
        assertFalse(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
        verify(mListener, never()).onUserAllowed(DATA_CARRIER_ID);
    }

    @Test
    public void testSendImsiProtectionExemptionConfirmationDialogWithUserDisallowed() {
        // Setup carrier without IMSI privacy protection
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
        // Simulate user clicking on the notification.
        sendBroadcastForUserActionOnImsi(NOTIFICATION_USER_CLICKED_INTENT_ACTION,
                CARRIER_NAME, DATA_CARRIER_ID);
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);
        validateUserApprovalDialog(CARRIER_NAME);

        // Simulate user clicking on disallow in the dialog.
        ArgumentCaptor<DialogInterface.OnClickListener> clickListenerCaptor =
                ArgumentCaptor.forClass(DialogInterface.OnClickListener.class);
        verify(mAlertDialogBuilder, atLeastOnce()).setNegativeButton(
                any(), clickListenerCaptor.capture());
        assertNotNull(clickListenerCaptor.getValue());
        clickListenerCaptor.getValue().onClick(mAlertDialog, 0);
        mLooper.dispatchAll();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).sendBroadcast(intentCaptor.capture());
        assertEquals(Intent.ACTION_CLOSE_SYSTEM_DIALOGS, intentCaptor.getValue().getAction());
        verify(mWifiConfigManager).saveToStore(true);
        assertTrue(mImsiDataSource.hasNewDataToSerialize());
        assertFalse(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
        verify(mListener, never()).onUserAllowed(DATA_CARRIER_ID);
        verify(mWifiMetrics).addUserApprovalCarrierUiReaction(
                WifiCarrierInfoManager.ACTION_USER_DISALLOWED_CARRIER, true);
    }

    @Test
    public void testSendImsiProtectionExemptionConfirmationDialogWithUserDismissal() {
        // Setup carrier without IMSI privacy protection
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
        // Simulate user clicking on the notification.
        sendBroadcastForUserActionOnImsi(NOTIFICATION_USER_CLICKED_INTENT_ACTION,
                CARRIER_NAME, DATA_CARRIER_ID);
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);
        validateUserApprovalDialog(CARRIER_NAME);

        // Simulate user clicking on dismissal in the dialog.
        ArgumentCaptor<DialogInterface.OnDismissListener> dismissListenerCaptor =
                ArgumentCaptor.forClass(DialogInterface.OnDismissListener.class);
        verify(mAlertDialogBuilder, atLeastOnce()).setOnDismissListener(
                dismissListenerCaptor.capture());
        assertNotNull(dismissListenerCaptor.getValue());
        dismissListenerCaptor.getValue().onDismiss(mAlertDialog);
        mLooper.dispatchAll();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).sendBroadcast(intentCaptor.capture());
        assertEquals(Intent.ACTION_CLOSE_SYSTEM_DIALOGS, intentCaptor.getValue().getAction());

        // As no notification is active, new notification should be sent
        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);

        verify(mWifiConfigManager, never()).saveToStore(true);
        assertFalse(mImsiDataSource.hasNewDataToSerialize());
        assertFalse(mWifiCarrierInfoManager
                .hasUserApprovedImsiPrivacyExemptionForCarrier(DATA_CARRIER_ID));
        verify(mListener, never()).onUserAllowed(DATA_CARRIER_ID);
        verify(mWifiMetrics).addUserApprovalCarrierUiReaction(
                WifiCarrierInfoManager.ACTION_USER_DISMISS, true);
    }

    @Test
    public void testSendImsiProtectionExemptionDialogWithUserAllowed() {
        // Setup carrier without IMSI privacy protection
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
        // Simulate user clicking on the notification.
        sendBroadcastForUserActionOnImsi(NOTIFICATION_USER_CLICKED_INTENT_ACTION,
                CARRIER_NAME, DATA_CARRIER_ID);
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);
        validateUserApprovalDialog(CARRIER_NAME);

        // Simulate user clicking on allow in the dialog.
        ArgumentCaptor<DialogInterface.OnClickListener> clickListenerCaptor =
                ArgumentCaptor.forClass(DialogInterface.OnClickListener.class);
        verify(mAlertDialogBuilder, atLeastOnce()).setPositiveButton(
                any(), clickListenerCaptor.capture());
        assertNotNull(clickListenerCaptor.getValue());
        clickListenerCaptor.getValue().onClick(mAlertDialog, 0);
        mLooper.dispatchAll();
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).sendBroadcast(intentCaptor.capture());
        assertEquals(Intent.ACTION_CLOSE_SYSTEM_DIALOGS, intentCaptor.getValue().getAction());
        verify(mWifiConfigManager).saveToStore(true);
        assertTrue(mImsiDataSource.hasNewDataToSerialize());
        verify(mListener).onUserAllowed(DATA_CARRIER_ID);
        verify(mWifiMetrics).addUserApprovalCarrierUiReaction(
                WifiCarrierInfoManager.ACTION_USER_ALLOWED_CARRIER, true);
    }

    @Test
    public void testUserDataStoreIsNotLoadedNotificationWillNotBeSent() {
        // reset data source to unloaded state.
        mImsiDataSource.reset();
        // Setup carrier without IMSI privacy protection
        when(mCarrierConfigManager.getConfigForSubId(DATA_SUBID))
                .thenReturn(generateTestCarrierConfig(false));
        ArgumentCaptor<BroadcastReceiver> receiver =
                ArgumentCaptor.forClass(BroadcastReceiver.class);
        verify(mContext).registerReceiver(receiver.capture(), any(IntentFilter.class));

        receiver.getValue().onReceive(mContext,
                new Intent(CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED));
        assertFalse(mWifiCarrierInfoManager.requiresImsiEncryption(DATA_SUBID));

        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        verifyNoMoreInteractions(mNotificationManger);

        // Loaded user data store, notification should be sent
        mImsiDataSource.fromDeserialized(new HashMap<>());
        mWifiCarrierInfoManager.sendImsiProtectionExemptionNotificationIfRequired(DATA_CARRIER_ID);
        validateImsiProtectionNotification(CARRIER_NAME);
    }

    private void validateImsiProtectionNotification(String carrierName) {
        verify(mNotificationManger, atLeastOnce()).notify(
                eq(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE),
                eq(mNotification));
        ArgumentCaptor<CharSequence> contentCaptor =
                ArgumentCaptor.forClass(CharSequence.class);
        verify(mNotificationBuilder, atLeastOnce()).setContentTitle(contentCaptor.capture());
        CharSequence content = contentCaptor.getValue();
        assertNotNull(content);
        assertTrue(content.toString().contains(carrierName));
    }

    private void validateUserApprovalDialog(String... anyOfExpectedAppNames) {
        verify(mAlertDialog, atLeastOnce()).show();
        ArgumentCaptor<CharSequence> contentCaptor =
                ArgumentCaptor.forClass(CharSequence.class);
        verify(mAlertDialogBuilder, atLeastOnce()).setMessage(contentCaptor.capture());
        CharSequence content = contentCaptor.getValue();
        assertNotNull(content);

        boolean foundMatch = false;
        for (int i = 0; i < anyOfExpectedAppNames.length; i++) {
            foundMatch = content.toString().contains(anyOfExpectedAppNames[i]);
            if (foundMatch) break;
        }
        assertTrue(foundMatch);
    }

    private void sendBroadcastForUserActionOnImsi(String action, String carrierName,
            int carrierId) {
        Intent intent = new Intent()
                .setAction(action)
                .putExtra(WifiCarrierInfoManager.EXTRA_CARRIER_NAME, carrierName)
                .putExtra(WifiCarrierInfoManager.EXTRA_CARRIER_ID, carrierId);
        assertNotNull(mBroadcastReceiverCaptor.getValue());
        mBroadcastReceiverCaptor.getValue().onReceive(mContext, intent);
    }
}
