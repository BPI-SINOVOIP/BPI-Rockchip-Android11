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

package com.android.server.wifi;

import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;
import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE;
import static android.app.ActivityManager.RunningAppProcessInfo.IMPORTANCE_SERVICE;
import static android.app.AppOpsManager.MODE_ALLOWED;
import static android.app.AppOpsManager.MODE_IGNORED;
import static android.app.AppOpsManager.OPSTR_CHANGE_WIFI_STATE;

import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.when;
import static com.android.server.wifi.WifiNetworkSuggestionsManager.NOTIFICATION_USER_ALLOWED_APP_INTENT_ACTION;
import static com.android.server.wifi.WifiNetworkSuggestionsManager.NOTIFICATION_USER_DISALLOWED_APP_INTENT_ACTION;
import static com.android.server.wifi.WifiNetworkSuggestionsManager.NOTIFICATION_USER_DISMISSED_INTENT_ACTION;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.*;

import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.AppOpsManager;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.NetworkScoreManager;
import android.net.util.MacAddressUtils;
import android.net.wifi.EAPConstants;
import android.net.wifi.ISuggestionConnectionStatusListener;
import android.net.wifi.ScanResult;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiEnterpriseConfig;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiNetworkSuggestion;
import android.net.wifi.WifiScanner;
import android.net.wifi.WifiSsid;
import android.net.wifi.hotspot2.PasspointConfiguration;
import android.net.wifi.hotspot2.pps.Credential;
import android.net.wifi.hotspot2.pps.HomeSp;
import android.os.Handler;
import android.os.IBinder;
import android.os.UserHandle;
import android.os.UserManager;
import android.os.test.TestLooper;
import android.telephony.TelephonyManager;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.LayoutInflater;
import android.view.Window;

import com.android.dx.mockito.inline.extended.ExtendedMockito;
import com.android.internal.messages.nano.SystemMessageProto.SystemMessage;
import com.android.server.wifi.WifiNetworkSuggestionsManager.ExtendedWifiNetworkSuggestion;
import com.android.server.wifi.WifiNetworkSuggestionsManager.PerAppInfo;
import com.android.server.wifi.hotspot2.PasspointManager;
import com.android.server.wifi.util.LruConnectionTracker;
import com.android.server.wifi.util.WifiPermissionsUtil;
import com.android.wifi.resources.R;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.MockitoSession;
import org.mockito.quality.Strictness;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Unit tests for {@link com.android.server.wifi.WifiNetworkSuggestionsManager}.
 */
@SmallTest
public class WifiNetworkSuggestionsManagerTest extends WifiBaseTest {

    private static final String TEST_PACKAGE_1 = "com.test12345";
    private static final String TEST_PACKAGE_2 = "com.test54321";
    private static final String TEST_APP_NAME_1 = "test12345";
    private static final String TEST_APP_NAME_2 = "test54321";
    private static final String TEST_FEATURE = "testFeature";
    private static final String TEST_BSSID = "00:11:22:33:44:55";
    private static final String TEST_FQDN = "FQDN";
    private static final String TEST_FRIENDLY_NAME = "test_friendly_name";
    private static final String TEST_REALM = "realm.test.com";
    private static final String TEST_CARRIER_NAME = "test_carrier";
    private static final int TEST_UID_1 = 5667;
    private static final int TEST_UID_2 = 4537;
    private static final int NETWORK_CALLBACK_ID = 1100;
    private static final int VALID_CARRIER_ID = 100;
    private static final int TEST_SUBID = 1;
    private static final int TEST_NETWORK_ID = 110;
    private static final int TEST_CARRIER_ID = 1911;
    private static final String TEST_IMSI = "123456*";

    private @Mock WifiContext mContext;
    private @Mock Resources mResources;
    private @Mock AppOpsManager mAppOpsManager;
    private @Mock NotificationManager mNotificationManger;
    private @Mock NetworkScoreManager mNetworkScoreManager;
    private @Mock PackageManager mPackageManager;
    private @Mock WifiPermissionsUtil mWifiPermissionsUtil;
    private @Mock WifiInjector mWifiInjector;
    private @Mock FrameworkFacade mFrameworkFacade;
    private @Mock WifiConfigStore mWifiConfigStore;
    private @Mock WifiConfigManager mWifiConfigManager;
    private @Mock NetworkSuggestionStoreData mNetworkSuggestionStoreData;
    private @Mock WifiMetrics mWifiMetrics;
    private @Mock WifiCarrierInfoManager mWifiCarrierInfoManager;
    private @Mock PasspointManager mPasspointManager;
    private @Mock ISuggestionConnectionStatusListener mListener;
    private @Mock IBinder mBinder;
    private @Mock ActivityManager mActivityManager;
    private @Mock WifiScoreCard mWifiScoreCard;
    private @Mock WifiKeyStore mWifiKeyStore;
    private @Mock AlertDialog.Builder mAlertDialogBuilder;
    private @Mock AlertDialog mAlertDialog;
    private @Mock Notification.Builder mNotificationBuilder;
    private @Mock Notification mNotification;
    private @Mock LruConnectionTracker mLruConnectionTracker;
    private @Mock UserManager mUserManager;
    private TestLooper mLooper;
    private ArgumentCaptor<AppOpsManager.OnOpChangedListener> mAppOpChangedListenerCaptor =
            ArgumentCaptor.forClass(AppOpsManager.OnOpChangedListener.class);
    private ArgumentCaptor<BroadcastReceiver> mBroadcastReceiverCaptor =
            ArgumentCaptor.forClass(BroadcastReceiver.class);
    private ArgumentCaptor<WifiCarrierInfoManager.OnUserApproveCarrierListener>
            mUserApproveCarrierListenerArgumentCaptor = ArgumentCaptor.forClass(
            WifiCarrierInfoManager.OnUserApproveCarrierListener.class);

    private InOrder mInorder;

    private WifiNetworkSuggestionsManager mWifiNetworkSuggestionsManager;
    private NetworkSuggestionStoreData.DataSource mDataSource;

    /**
     * Setup the mocks.
     */
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mLooper = new TestLooper();

        mInorder = inOrder(mContext, mWifiPermissionsUtil);

        when(mWifiInjector.makeNetworkSuggestionStoreData(any()))
                .thenReturn(mNetworkSuggestionStoreData);
        when(mWifiInjector.getFrameworkFacade()).thenReturn(mFrameworkFacade);
        when(mWifiInjector.getPasspointManager()).thenReturn(mPasspointManager);
        when(mWifiInjector.getWifiScoreCard()).thenReturn(mWifiScoreCard);
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
        when(mNotificationBuilder.setDeleteIntent(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setShowWhen(anyBoolean())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setLocalOnly(anyBoolean())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.setColor(anyInt())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.addAction(any())).thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.build()).thenReturn(mNotification);
        when(mFrameworkFacade.makeAlertDialogBuilder(any()))
                .thenReturn(mAlertDialogBuilder);
        when(mFrameworkFacade.makeNotificationBuilder(any(), anyString()))
                .thenReturn(mNotificationBuilder);
        when(mFrameworkFacade.getBroadcast(any(), anyInt(), any(), anyInt()))
                .thenReturn(mock(PendingIntent.class));
        when(mContext.getResources()).thenReturn(mResources);
        when(mContext.getSystemService(Context.APP_OPS_SERVICE)).thenReturn(mAppOpsManager);
        when(mContext.getSystemService(Context.NOTIFICATION_SERVICE))
                .thenReturn(mNotificationManger);
        when(mContext.getSystemService(NetworkScoreManager.class)).thenReturn(mNetworkScoreManager);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mContext.getSystemService(ActivityManager.class)).thenReturn(mActivityManager);
        when(mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE))
                .thenReturn(mock(LayoutInflater.class));
        when(mContext.getWifiOverlayApkPkgName()).thenReturn("test.com.android.wifi.resources");
        when(mActivityManager.isLowRamDevice()).thenReturn(false);
        when(mActivityManager.getPackageImportance(any())).thenReturn(
                IMPORTANCE_FOREGROUND_SERVICE);
        when(mWifiPermissionsUtil.doesUidBelongToCurrentUser(anyInt())).thenReturn(true);

        // setup resource strings for notification.
        when(mResources.getString(eq(R.string.wifi_suggestion_title), anyString()))
                .thenAnswer(s -> "blah" + s.getArguments()[1]);
        when(mResources.getString(eq(R.string.wifi_suggestion_content), anyString()))
                .thenAnswer(s -> "blah" + s.getArguments()[1]);
        when(mResources.getText(eq(R.string.wifi_suggestion_action_allow_app)))
                .thenReturn("blah");
        when(mResources.getText(eq(R.string.wifi_suggestion_action_disallow_app)))
                .thenReturn("blah");


        // Our app Info. Needed for notification builder.
        ApplicationInfo ourAppInfo = new ApplicationInfo();
        when(mContext.getApplicationInfo()).thenReturn(ourAppInfo);
        // test app info
        ApplicationInfo appInfO1 = new ApplicationInfo();
        when(mPackageManager.getApplicationInfoAsUser(eq(TEST_PACKAGE_1), eq(0), any()))
            .thenReturn(appInfO1);
        when(mPackageManager.getApplicationLabel(appInfO1)).thenReturn(TEST_APP_NAME_1);
        ApplicationInfo appInfO2 = new ApplicationInfo();
        when(mPackageManager.getApplicationInfoAsUser(eq(TEST_PACKAGE_2), eq(0), any()))
            .thenReturn(appInfO2);
        when(mPackageManager.getApplicationLabel(appInfO2)).thenReturn(TEST_APP_NAME_2);
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(any())).thenReturn(
                TelephonyManager.UNKNOWN_CARRIER_ID);

        when(mWifiKeyStore.updateNetworkKeys(any(), any())).thenReturn(true);

        mWifiNetworkSuggestionsManager =
                new WifiNetworkSuggestionsManager(mContext, new Handler(mLooper.getLooper()),
                        mWifiInjector, mWifiPermissionsUtil, mWifiConfigManager, mWifiConfigStore,
                        mWifiMetrics, mWifiCarrierInfoManager, mWifiKeyStore,
                        mLruConnectionTracker);
        verify(mContext).getResources();
        verify(mContext).getSystemService(Context.APP_OPS_SERVICE);
        verify(mContext).getSystemService(Context.NOTIFICATION_SERVICE);
        verify(mContext).getPackageManager();
        verify(mContext).registerReceiver(mBroadcastReceiverCaptor.capture(), any(), any(), any());

        ArgumentCaptor<NetworkSuggestionStoreData.DataSource> dataSourceArgumentCaptor =
                ArgumentCaptor.forClass(NetworkSuggestionStoreData.DataSource.class);

        verify(mWifiInjector).makeNetworkSuggestionStoreData(dataSourceArgumentCaptor.capture());
        mDataSource = dataSourceArgumentCaptor.getValue();
        assertNotNull(mDataSource);
        mDataSource.fromDeserialized(Collections.EMPTY_MAP);

        verify(mWifiCarrierInfoManager).addImsiExemptionUserApprovalListener(
                mUserApproveCarrierListenerArgumentCaptor.capture());

        mWifiNetworkSuggestionsManager.enableVerboseLogging(1);
    }

    /**
     * Verify successful addition of network suggestions.
     */
    @Test
    public void testAddNetworkSuggestionsSuccess() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                dummyConfiguration, passpointConfiguration, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion1);
            }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion2);
            }};
        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        verify(mPasspointManager).addOrUpdateProvider(
                passpointConfiguration, TEST_UID_2, TEST_PACKAGE_2, true, true);
        verify(mWifiMetrics, times(2))
                .incrementNetworkSuggestionApiUsageNumOfAppInType(
                        WifiNetworkSuggestionsManager.APP_TYPE_NON_PRIVILEGED);
        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
            }};
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        verify(mWifiMetrics, times(2)).incrementNetworkSuggestionApiNumModification();
        ArgumentCaptor<List<Integer>> maxSizesCaptor = ArgumentCaptor.forClass(List.class);
        verify(mWifiMetrics, times(2)).noteNetworkSuggestionApiListSizeHistogram(
                maxSizesCaptor.capture());
        assertNotNull(maxSizesCaptor.getValue());
        assertEquals(maxSizesCaptor.getValue(), new ArrayList<Integer>() {{ add(1); add(1); }});
    }

    /**
     * Verify successful removal of network suggestions.
     */
    @Test
    public void testRemoveNetworkSuggestionsSuccess() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.setPasspointUniqueId(passpointConfiguration.getUniqueId());
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                dummyConfiguration, passpointConfiguration, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion1);
            }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion2);
            }};
        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));

        // Now remove all of them.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList1,
                        TEST_UID_1, TEST_PACKAGE_1));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList2,
                        TEST_UID_2, TEST_PACKAGE_2));
        verify(mPasspointManager).removeProvider(eq(TEST_UID_2), eq(false),
                eq(passpointConfiguration.getUniqueId()), isNull());
        verify(mWifiScoreCard).removeNetwork(anyString());
        verify(mLruConnectionTracker).removeNetwork(any());

        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());

        verify(mWifiMetrics, times(4)).incrementNetworkSuggestionApiNumModification();
        ArgumentCaptor<List<Integer>> maxSizesCaptor = ArgumentCaptor.forClass(List.class);
        verify(mWifiMetrics, times(4)).noteNetworkSuggestionApiListSizeHistogram(
                maxSizesCaptor.capture());
        assertNotNull(maxSizesCaptor.getValue());
        assertEquals(maxSizesCaptor.getValue(), new ArrayList<Integer>() {{ add(1); add(1); }});
    }

    /**
     * Add or remove suggestion before user data store loaded will fail.
     */
    @Test
    public void testAddRemoveSuggestionBeforeUserDataLoaded() {
        // Clear the data source, and user data store is not loaded
        mDataSource.reset();
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = Arrays.asList(networkSuggestion);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_INTERNAL,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_INTERNAL,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1));
    }

    @Test
    public void testAddRemoveEnterpriseNetworkSuggestion() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createEapNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createEapNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
                }};
        when(mWifiKeyStore.updateNetworkKeys(eq(networkSuggestion1.wifiConfiguration), any()))
                .thenReturn(true);
        when(mWifiKeyStore.updateNetworkKeys(eq(networkSuggestion2.wifiConfiguration), any()))
                .thenReturn(false);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        assertEquals(1, allNetworkSuggestions.size());
        WifiNetworkSuggestion removingSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createEapNetwork(), null, false, false, true, true);
        removingSuggestion.wifiConfiguration.SSID = networkSuggestion1.wifiConfiguration.SSID;
        // Remove with the networkSuggestion from external.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(
                        new ArrayList<WifiNetworkSuggestion>() {{ add(removingSuggestion); }},
                        TEST_UID_1, TEST_PACKAGE_1));
        // Make sure remove the keyStore with the internal config
        verify(mWifiKeyStore).removeKeys(networkSuggestion1.wifiConfiguration.enterpriseConfig);
        verify(mLruConnectionTracker).removeNetwork(any());
    }

    @Test
    public void testAddInsecureEnterpriseNetworkSuggestion() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createEapNetwork(), null, false, false, true, true);
        networkSuggestion.wifiConfiguration.enterpriseConfig.setCaPath(null);
        List<WifiNetworkSuggestion> networkSuggestionList = Arrays.asList(networkSuggestion);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_INVALID,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createEapNetwork(), null, false, false, true, true);
        networkSuggestion.wifiConfiguration.enterpriseConfig.setDomainSuffixMatch("");
        networkSuggestionList = Arrays.asList(networkSuggestion);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_INVALID,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
    }

    /**
     * Verify successful removal of all network suggestions.
     */
    @Test
    public void testRemoveAllNetworkSuggestionsSuccess() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                dummyConfiguration, passpointConfiguration, false, false, true, true);


        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));

        // Now remove all of them by sending an empty list.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(new ArrayList<>(), TEST_UID_1,
                        TEST_PACKAGE_1));
        verify(mLruConnectionTracker).removeNetwork(any());
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(new ArrayList<>(), TEST_UID_2,
                        TEST_PACKAGE_2));
        verify(mPasspointManager).removeProvider(eq(TEST_UID_2), eq(false),
                eq(passpointConfiguration.getUniqueId()), isNull());

        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());
    }

    /**
     * Verify successful replace (add,remove, add) of network suggestions.
     */
    @Test
    public void testReplaceNetworkSuggestionsSuccess() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion);
            }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                add(networkSuggestion);
            }};
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);
    }

    /**
     * Verify that modify networks that are already active is allowed.
     */
    @Test
    public void testAddNetworkSuggestionsSuccessOnInPlaceModificationWhenNotInWcm() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion);
            }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        // Assert that the original config was not metered.
        assertEquals(WifiConfiguration.METERED_OVERRIDE_NONE,
                networkSuggestion.wifiConfiguration.meteredOverride);

        // Nothing in WCM.
        when(mWifiConfigManager.getConfiguredNetwork(networkSuggestion.wifiConfiguration.getKey()))
                .thenReturn(null);

        // Modify the original suggestion to mark it metered.
        networkSuggestion.wifiConfiguration.meteredOverride =
                WifiConfiguration.METERED_OVERRIDE_METERED;

        // Replace attempt should success.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiConfiguration.METERED_OVERRIDE_METERED,
                mWifiNetworkSuggestionsManager
                        .get(TEST_PACKAGE_1, TEST_UID_1).get(0).wifiConfiguration.meteredOverride);
        // Verify we did not update config in WCM.
        verify(mWifiConfigManager, never()).addOrUpdateNetwork(any(), anyInt(), any());
    }

    /**
     * Verify that modify networks that are already active and is cached in WifiConfigManager is
     * allowed and also updates the cache in WifiConfigManager.
     */
    @Test
    public void testAddNetworkSuggestionsSuccessOnInPlaceModificationWhenInWcm() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        // Assert that the original config was not metered.
        assertEquals(WifiConfiguration.METERED_OVERRIDE_NONE,
                networkSuggestion.wifiConfiguration.meteredOverride);
        verify(mWifiMetrics).incrementNetworkSuggestionApiUsageNumOfAppInType(
                WifiNetworkSuggestionsManager.APP_TYPE_NON_PRIVILEGED);
        reset(mWifiMetrics);
        // Store the original WifiConfiguration from WifiConfigManager.
        WifiConfiguration configInWcm =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        configInWcm.creatorUid = TEST_UID_1;
        configInWcm.creatorName = TEST_PACKAGE_1;
        configInWcm.fromWifiNetworkSuggestion = true;
        setupGetConfiguredNetworksFromWcm(configInWcm);

        // Modify the original suggestion to mark it metered.
        networkSuggestion.wifiConfiguration.meteredOverride =
                WifiConfiguration.METERED_OVERRIDE_METERED;

        when(mWifiConfigManager.addOrUpdateNetwork(any(), eq(TEST_UID_1), eq(TEST_PACKAGE_1)))
                .thenReturn(new NetworkUpdateResult(TEST_NETWORK_ID));
        // Replace attempt should success.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiConfiguration.METERED_OVERRIDE_METERED,
                mWifiNetworkSuggestionsManager
                        .get(TEST_PACKAGE_1, TEST_UID_1).get(0).wifiConfiguration.meteredOverride);
        verify(mWifiMetrics, never()).incrementNetworkSuggestionApiUsageNumOfAppInType(anyInt());
        // Verify we did update config in WCM.
        ArgumentCaptor<WifiConfiguration> configCaptor =
                ArgumentCaptor.forClass(WifiConfiguration.class);
        verify(mWifiConfigManager).addOrUpdateNetwork(
                configCaptor.capture(), eq(TEST_UID_1), eq(TEST_PACKAGE_1));
        assertNotNull(configCaptor.getValue());
        assertEquals(WifiConfiguration.METERED_OVERRIDE_METERED,
                configCaptor.getValue().meteredOverride);
    }

    /**
     * Verify that an attempt to add networks beyond the max per app is rejected.
     */
    @Test
    public void testAddNetworkSuggestionsFailureOnExceedsMaxPerApp() {
        // Add the max per app first.
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        for (int i = 0; i < WifiManager.NETWORK_SUGGESTIONS_MAX_PER_APP_HIGH_RAM; i++) {
            networkSuggestionList.add(new WifiNetworkSuggestion(
                    WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true));
        }
        // The first add should succeed.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        List<WifiNetworkSuggestion> originalNetworkSuggestionsList = networkSuggestionList;

        // Now add 3 more.
        networkSuggestionList = new ArrayList<>();
        for (int i = 0; i < 3; i++) {
            networkSuggestionList.add(new WifiNetworkSuggestion(
                    WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true));
        }
        // The second add should fail.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_EXCEEDS_MAX_PER_APP,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        // Now remove 3 of the initially added ones.
        networkSuggestionList = new ArrayList<>();
        for (int i = 0; i < 3; i++) {
            networkSuggestionList.add(originalNetworkSuggestionsList.get(i));
        }
        // The remove should succeed.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1));

        // Now add 2 more.
        networkSuggestionList = new ArrayList<>();
        for (int i = 0; i < 2; i++) {
            networkSuggestionList.add(new WifiNetworkSuggestion(
                    WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true));
        }
        // This add should now succeed.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
    }

    /**
     * Verify that an attempt to remove an invalid set of network suggestions is rejected.
     */
    @Test
    public void testRemoveNetworkSuggestionsFailureOnInvalid() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion1);
            }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion2);
            }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        // Remove should fail because the network list is different.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_REMOVE_INVALID,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList2, TEST_UID_1,
                        TEST_PACKAGE_1));
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the provided scan detail.
     */
    @Test
    public void
            testGetNetworkSuggestionsForScanDetailSuccessWithOneMatchForCarrierProvisioningApp() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        // This app should be pre-approved. No need to explicitly call
        // |setHasUserApprovedForApp(true, TEST_PACKAGE_1)|
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(mWifiMetrics).incrementNetworkSuggestionApiUsageNumOfAppInType(
                WifiNetworkSuggestionsManager.APP_TYPE_NETWORK_PROVISIONING);
        ScanDetail scanDetail = createScanDetailForNetwork(networkSuggestion.wifiConfiguration);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify add or remove suggestion list with null object will result error code.
     */
    @Test
    public void testAddRemoveNetworkSuggestionWithNullObject() {
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_INVALID,
                mWifiNetworkSuggestionsManager.add(Collections.singletonList(null),
                        TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE));
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(Collections.singletonList(networkSuggestion),
                        TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_REMOVE_INVALID,
                mWifiNetworkSuggestionsManager.remove(Collections.singletonList(null),
                        TEST_UID_1, TEST_PACKAGE_1));
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the provided scan detail.
     */
    @Test
    public void testGetNetworkSuggestionsForScanDetailSuccessWithOneMatch() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        ScanDetail scanDetail = createScanDetailForNetwork(networkSuggestion.wifiConfiguration);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify a successful lookup of multiple network suggestions matching the provided scan detail.
     */
    @Test
    public void testGetNetworkSuggestionsForScanDetailSuccessWithMultipleMatch() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);
        // Reuse the same network credentials to ensure they both match.
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion1);
            }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion2);
            }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);

        ScanDetail scanDetail = createScanDetailForNetwork(networkSuggestion1.wifiConfiguration);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                add(networkSuggestion1);
                add(networkSuggestion2);
            }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the provided scan detail.
     */
    @Test
    public void testGetNetworkSuggestionsForScanDetailSuccessWithBssidOneMatch() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail = createScanDetailForNetwork(wifiConfiguration);
        wifiConfiguration.BSSID = scanDetail.getBSSIDString();

        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify a successful lookup of multiple network suggestions matching the provided scan detail.
     */
    @Test
    public void testGetNetworkSuggestionsForScanDetailSuccessWithBssidMultipleMatch() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail = createScanDetailForNetwork(wifiConfiguration);
        wifiConfiguration.BSSID = scanDetail.getBSSIDString();

        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);
        // Reuse the same network credentials to ensure they both match.
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify a successful lookup of multiple network suggestions matching the provided scan detail.
     */
    @Test
    public void
            testGetNetworkSuggestionsForScanDetailSuccessWithBssidMultipleMatchFromSamePackage() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail = createScanDetailForNetwork(wifiConfiguration);
        wifiConfiguration.BSSID = scanDetail.getBSSIDString();

        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);
        // Reuse the same network credentials to ensure they both match.
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify a successful lookup of multiple network suggestions matching the provided scan detail.
     */
    @Test
    public void
            testGetNetworkSuggestionsForScanDetailSuccessWithBssidAndWithoutBssidMultipleMatch() {
        WifiConfiguration wifiConfiguration1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail = createScanDetailForNetwork(wifiConfiguration1);
        WifiConfiguration wifiConfiguration2 = new WifiConfiguration(wifiConfiguration1);
        wifiConfiguration2.BSSID = scanDetail.getBSSIDString();

        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration1, null, false, false, true, true);
        // Reuse the same network credentials to ensure they both match.
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration2, null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);

        // Now change the bssid of the scan result to a different value, now only the general
        // (without bssid) suggestion.
        scanDetail.getScanResult().BSSID = MacAddressUtils.createRandomUnicastAddress().toString();
        matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify failure to lookup any network suggestion matching the provided scan detail when the
     * app providing the suggestion has not been approved.
     */
    @Test
    public void testGetNetworkSuggestionsForScanDetailFailureOnAppNotApproved() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager.hasUserApprovedForApp(TEST_PACKAGE_1));

        ScanDetail scanDetail = createScanDetailForNetwork(networkSuggestion.wifiConfiguration);

        assertNull(mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail));
    }

    /**
     * Verify failure to lookup any network suggestion matching the provided scan detail.
     */
    @Test
    public void testGetNetworkSuggestionsForScanDetailFailureOnSuggestionRemoval() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);
        ScanDetail scanDetail = createScanDetailForNetwork(wifiConfiguration);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                add(networkSuggestion);
            }};

        // add the suggestion & ensure lookup works.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        assertNotNull(mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                scanDetail));

        // remove the suggestion & ensure lookup fails.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(Collections.EMPTY_LIST, TEST_UID_1,
                        TEST_PACKAGE_1));
        assertNull(mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail));
    }

    /**
     * Verify failure to lookup any network suggestion matching the provided scan detail.
     */
    @Test
    public void testGetNetworkSuggestionsForScanDetailFailureOnWrongNetwork() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        // Create a scan result corresponding to a different network.
        ScanDetail scanDetail = createScanDetailForNetwork(
                WifiConfigurationTestUtil.createPskNetwork());

        assertNull(mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail));
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the connected network.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * This should trigger a broadcast to the app.
     * This should not trigger a connection failure callback to the app.
     */
    @Test
    public void testOnNetworkConnectionSuccessWithOneMatch() throws Exception {
        assertTrue(mWifiNetworkSuggestionsManager
                .registerSuggestionConnectionStatusListener(mBinder, mListener,
                        NETWORK_CALLBACK_ID, TEST_PACKAGE_1, TEST_UID_1));
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        // Simulate connecting to the network.
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_PACKAGE_1;
        connectNetwork.creatorUid = TEST_UID_1;
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectSuccess();

        // Verify that the correct broadcast was sent out.
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(eq(TEST_PACKAGE_1),
                eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));
        validatePostConnectionBroadcastSent(TEST_PACKAGE_1, networkSuggestion);

        // Verify no more broadcast were sent out.
        mInorder.verifyNoMoreInteractions();
    }

    @Test
    public void testOnNetworkConnectionSuccessWithOneMatchFromCarrierPrivilegedApp() {
        assertTrue(mWifiNetworkSuggestionsManager
                .registerSuggestionConnectionStatusListener(mBinder, mListener,
                        NETWORK_CALLBACK_ID, TEST_PACKAGE_1, TEST_UID_1));
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(TEST_CARRIER_ID);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createPskNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager.hasUserApprovedForApp(TEST_PACKAGE_1));

        // Simulate connecting to the network.
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_PACKAGE_1;
        connectNetwork.creatorUid = TEST_UID_1;
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectSuccess();

        // Verify that the correct broadcast was sent out.
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(eq(TEST_PACKAGE_1),
                eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));
        validatePostConnectionBroadcastSent(TEST_PACKAGE_1, networkSuggestion);

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1));
        verify(mWifiConfigManager).removeSuggestionConfiguredNetwork(anyString());
        mInorder.verify(mWifiPermissionsUtil).doesUidBelongToCurrentUser(eq(TEST_UID_1));

        // Verify no more broadcast were sent out.
        mInorder.verifyNoMoreInteractions();
    }

    /**
     * Verify if a user saved network connected and it can match suggestions. Only the
     * carrier-privileged suggestor app can receive the broadcast if
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set to true.
     */
    @Test
    public void testOnSavedOpenNetworkConnectionSuccessWithMultipleMatch() throws Exception {
        assertTrue(mWifiNetworkSuggestionsManager
                .registerSuggestionConnectionStatusListener(mBinder, mListener,
                        NETWORK_CALLBACK_ID, TEST_PACKAGE_1, TEST_UID_1));
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(true);
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                new WifiConfiguration(config), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                new WifiConfiguration(config), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);

        // Simulate connecting to the user saved open network.
        WifiConfiguration connectNetwork = new WifiConfiguration(config);
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectSuccess();
        // Verify that the correct broadcast was sent out.
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(eq(TEST_PACKAGE_1),
                eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));
        validatePostConnectionBroadcastSent(TEST_PACKAGE_1, networkSuggestion1);

        // Verify no more broadcast were sent out.
        mInorder.verifyNoMoreInteractions();
    }

    @Test
    public void testOnNetworkConnectionFailureWithOneMatchButCallbackOnBinderDied()
            throws Exception {
        ArgumentCaptor<IBinder.DeathRecipient> drCaptor =
                ArgumentCaptor.forClass(IBinder.DeathRecipient.class);
        assertTrue(mWifiNetworkSuggestionsManager
                .registerSuggestionConnectionStatusListener(mBinder, mListener,
                        NETWORK_CALLBACK_ID, TEST_PACKAGE_1, TEST_UID_1));
        verify(mBinder).linkToDeath(drCaptor.capture(), anyInt());
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        // Simulate binder was died.
        drCaptor.getValue().binderDied();
        mLooper.dispatchAll();
        verify(mBinder).unlinkToDeath(any(), anyInt());
        // Simulate connecting to the network.
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_AUTHENTICATION_FAILURE,
                connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectFailure();
        // Verify no connection failure event was sent out.
        mInorder.verify(mWifiPermissionsUtil, never()).enforceCanAccessScanResults(
                eq(TEST_PACKAGE_1), eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));
        verify(mListener, never()).onConnectionStatus(any(), anyInt());

        // Verify no more broadcast were sent out.
        mInorder.verify(mContext,  never()).sendBroadcastAsUser(
                any(), any());
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the current network
     * connection failure.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * This should trigger a connection failure callback to the app
     */
    @Test
    public void testOnNetworkConnectionFailureWithOneMatch() throws Exception {
        assertTrue(mWifiNetworkSuggestionsManager
                .registerSuggestionConnectionStatusListener(mBinder, mListener,
                        NETWORK_CALLBACK_ID, TEST_PACKAGE_1, TEST_UID_1));
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;
        // Simulate connecting to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_DHCP, connectNetwork, TEST_BSSID);

        // Verify right callback were sent out.
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(eq(TEST_PACKAGE_1),
                eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));
        verify(mListener)
                .onConnectionStatus(networkSuggestion,
                        WifiManager.STATUS_SUGGESTION_CONNECTION_FAILURE_IP_PROVISIONING);
        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectFailure();

        // Verify no more broadcast were sent out.
        mInorder.verify(mContext,  never()).sendBroadcastAsUser(
                any(), any());
    }

    /**
     * Verify a successful lookup of multiple network suggestion matching the connected network.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * This should trigger a broadcast to all the apps.
     */
    @Test
    public void testOnNetworkConnectionSuccessWithMultipleMatch() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);

        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion1.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_PACKAGE_1;
        connectNetwork.creatorUid = TEST_UID_1;

        // Simulate connecting to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectSuccess();

        ArgumentCaptor<String> packageNameCaptor = ArgumentCaptor.forClass(String.class);
        ArgumentCaptor<String> featureIdCaptor = ArgumentCaptor.forClass(String.class);
        ArgumentCaptor<Integer> uidCaptor = ArgumentCaptor.forClass(Integer.class);
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(
                packageNameCaptor.capture(), featureIdCaptor.capture(), uidCaptor.capture(),
                nullable(String.class));
        assertEquals(TEST_FEATURE, featureIdCaptor.getValue());
        assertEquals(packageNameCaptor.getValue(), TEST_PACKAGE_1);
        assertEquals(Integer.valueOf(TEST_UID_1), uidCaptor.getValue());
        validatePostConnectionBroadcastSent(TEST_PACKAGE_1, networkSuggestion1);

        // Verify no more broadcast were sent out.
        mInorder.verifyNoMoreInteractions();
    }

    /**
     * Verify a successful lookup of multiple network suggestion matching the connected network.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * This should trigger a broadcast to all the apps.
     */
    @Test
    public void testOnNetworkConnectionSuccessWithBssidMultipleMatch() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        wifiConfiguration.BSSID = TEST_BSSID;
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion1.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_PACKAGE_1;
        connectNetwork.creatorUid = TEST_UID_1;
        // Simulate connecting to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectSuccess();

        // Verify that the correct broadcasts were sent out.
        ArgumentCaptor<String> packageNameCaptor = ArgumentCaptor.forClass(String.class);
        ArgumentCaptor<String> featureIdCaptor = ArgumentCaptor.forClass(String.class);
        ArgumentCaptor<Integer> uidCaptor = ArgumentCaptor.forClass(Integer.class);
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(
                packageNameCaptor.capture(), featureIdCaptor.capture(), uidCaptor.capture(),
                nullable(String.class));
        assertEquals(TEST_FEATURE, featureIdCaptor.getValue());
        assertEquals(packageNameCaptor.getValue(), TEST_PACKAGE_1);
        assertEquals(Integer.valueOf(TEST_UID_1), uidCaptor.getValue());
        validatePostConnectionBroadcastSent(TEST_PACKAGE_1, networkSuggestion1);

        // Verify no more broadcast were sent out.
        mInorder.verifyNoMoreInteractions();
    }

    /**
     * Verify a successful lookup of multiple network suggestion matching the connected network.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * This should trigger a broadcast to all the apps.
     */
    @Test
    public void testOnNetworkConnectionSuccessWithBssidAndWithoutBssidMultipleMatch() {
        WifiConfiguration wifiConfiguration1 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration wifiConfiguration2 = new WifiConfiguration(wifiConfiguration1);
        wifiConfiguration2.BSSID = TEST_BSSID;
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration1, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration2, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);

        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion1.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_PACKAGE_1;
        connectNetwork.creatorUid = TEST_UID_1;

        // Simulate connecting to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectSuccess();

        // Verify that the correct broadcasts were sent out.
        ArgumentCaptor<String> packageNameCaptor = ArgumentCaptor.forClass(String.class);
        ArgumentCaptor<String> featureIdCaptor = ArgumentCaptor.forClass(String.class);
        ArgumentCaptor<Integer> uidCaptor = ArgumentCaptor.forClass(Integer.class);
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(
                packageNameCaptor.capture(), featureIdCaptor.capture(), uidCaptor.capture(),
                nullable(String.class));
        assertEquals(TEST_FEATURE, featureIdCaptor.getValue());
        assertEquals(packageNameCaptor.getValue(), TEST_PACKAGE_1);
        assertEquals(Integer.valueOf(TEST_UID_1), uidCaptor.getValue());
        validatePostConnectionBroadcastSent(TEST_PACKAGE_1, networkSuggestion1);

        // Verify no more broadcast were sent out.
        mInorder.verifyNoMoreInteractions();
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the connected network.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * c) App has not been approved by the user.
     * This should not trigger a broadcast to the app.
     */
    @Test
    public void testOnNetworkConnectionWhenAppNotApproved() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(mWifiPermissionsUtil, times(2))
                .checkNetworkCarrierProvisioningPermission(TEST_UID_1);
        assertFalse(mWifiNetworkSuggestionsManager.hasUserApprovedForApp(TEST_PACKAGE_1));

        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;

        // Simulate connecting to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        // Verify no broadcast was sent out.
        mInorder.verify(mWifiPermissionsUtil, never()).enforceCanAccessScanResults(
                anyString(), nullable(String.class), anyInt(), nullable(String.class));
        mInorder.verify(mContext,  never()).sendBroadcastAsUser(
                any(), any());
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the connected network.
     * a) The corresponding network suggestion does not have the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * This should not trigger a broadcast to the app.
     */
    @Test
    public void testOnNetworkConnectionWhenIsAppInteractionRequiredNotSet() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(mWifiPermissionsUtil, times(2))
                .checkNetworkCarrierProvisioningPermission(TEST_UID_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;

        // Simulate connecting to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        // Verify no broadcast was sent out.
        mInorder.verify(mWifiPermissionsUtil, never()).enforceCanAccessScanResults(
                anyString(), nullable(String.class), anyInt(), nullable(String.class));
        mInorder.verify(mContext,  never()).sendBroadcastAsUser(
                any(), any());
    }

    /**
     * Verify a successful lookup of a single network suggestion matching the connected network.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app does not hold location permission.
     * This should not trigger a broadcast to the app.
     */
    @Test
    public void testOnNetworkConnectionWhenAppDoesNotHoldLocationPermission() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(mWifiPermissionsUtil, times(2))
                .checkNetworkCarrierProvisioningPermission(TEST_UID_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        doThrow(new SecurityException()).when(mWifiPermissionsUtil).enforceCanAccessScanResults(
                eq(TEST_PACKAGE_1), eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));

        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;

        // Simulate connecting to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(eq(TEST_PACKAGE_1),
                eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));

        // Verify no broadcast was sent out.
        mInorder.verifyNoMoreInteractions();
    }

    /**
     * Verify triggering of config store write after successful addition of network suggestions.
     */
    @Test
    public void testAddNetworkSuggestionsConfigStoreWrite() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        // Verify config store interactions.
        verify(mWifiConfigManager).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());

        Map<String, PerAppInfo> networkSuggestionsMapToWrite = mDataSource.toSerialize();
        assertEquals(1, networkSuggestionsMapToWrite.size());
        assertTrue(networkSuggestionsMapToWrite.keySet().contains(TEST_PACKAGE_1));
        assertFalse(networkSuggestionsMapToWrite.get(TEST_PACKAGE_1).hasUserApproved);
        Set<ExtendedWifiNetworkSuggestion> extNetworkSuggestionsToWrite =
                networkSuggestionsMapToWrite.get(TEST_PACKAGE_1).extNetworkSuggestions;
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(expectedAllNetworkSuggestions,
                extNetworkSuggestionsToWrite
                        .stream()
                        .collect(Collectors.mapping(
                                n -> n.wns,
                                Collectors.toSet())));

        // Ensure that the new data flag has been reset after read.
        assertFalse(mDataSource.hasNewDataToSerialize());
    }

    /**
     * Verify triggering of config store write after successful removal of network suggestions.
     */
    @Test
    public void testRemoveNetworkSuggestionsConfigStoreWrite() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1));

        // Verify config store interactions.
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());

        // Expect a single app entry with no active suggestions.
        Map<String, PerAppInfo> networkSuggestionsMapToWrite = mDataSource.toSerialize();
        assertEquals(1, networkSuggestionsMapToWrite.size());
        assertTrue(networkSuggestionsMapToWrite.keySet().contains(TEST_PACKAGE_1));
        assertFalse(networkSuggestionsMapToWrite.get(TEST_PACKAGE_1).hasUserApproved);
        assertTrue(
                networkSuggestionsMapToWrite.get(TEST_PACKAGE_1).extNetworkSuggestions.isEmpty());

        // Ensure that the new data flag has been reset after read.
        assertFalse(mDataSource.hasNewDataToSerialize());
    }

    /**
     * Verify handling of initial config store read.
     */
    @Test
    public void testNetworkSuggestionsConfigStoreLoad() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        PerAppInfo appInfo = new PerAppInfo(TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        appInfo.hasUserApproved = true;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                dummyConfiguration, passpointConfiguration, false, false, true, true);
        appInfo.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion, appInfo, true));
        appInfo.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion1, appInfo, true));
        mDataSource.fromDeserialized(new HashMap<String, PerAppInfo>() {{
                        put(TEST_PACKAGE_1, appInfo);
                }});

        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                    add(networkSuggestion1);
                }};
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        // Ensure we can lookup the network.
        ScanDetail scanDetail = createScanDetailForNetwork(networkSuggestion.wifiConfiguration);
        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);

        // Ensure we can lookup the passpoint network.
        WifiConfiguration connectNetwork = WifiConfigurationTestUtil.createPasspointNetwork();
        connectNetwork.FQDN = TEST_FQDN;
        connectNetwork.providerFriendlyName = TEST_FRIENDLY_NAME;
        connectNetwork.setPasspointUniqueId(passpointConfiguration.getUniqueId());
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_PACKAGE_1;
        connectNetwork.creatorUid = TEST_UID_1;

        matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager
                        .getNetworkSuggestionsForWifiConfiguration(connectNetwork, null);
        Set<ExtendedWifiNetworkSuggestion> expectedMatchingExtNetworkSuggestions =
                new HashSet<ExtendedWifiNetworkSuggestion>() {{
                    add(ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion1, appInfo, true));
                }};
        assertEquals(expectedMatchingExtNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify handling of config store read after user switch.
     */
    @Test
    public void testNetworkSuggestionsConfigStoreLoadAfterUserSwitch() {
        // Read the store initially.
        PerAppInfo appInfo1 = new PerAppInfo(TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        appInfo1.hasUserApproved = true;
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        appInfo1.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion1, appInfo1, true));
        mDataSource.fromDeserialized(new HashMap<String, PerAppInfo>() {{
                    put(TEST_PACKAGE_1, appInfo1);
                }});


        // Now simulate user switch.
        mDataSource.reset();
        PerAppInfo appInfo2 = new PerAppInfo(TEST_UID_2, TEST_PACKAGE_2, TEST_FEATURE);
        appInfo2.hasUserApproved = true;
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        appInfo2.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion2, appInfo2, true));
        mDataSource.fromDeserialized(new HashMap<String, PerAppInfo>() {{
                    put(TEST_PACKAGE_2, appInfo2);
                }});

        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        // Ensure we can lookup the new network.
        ScanDetail scanDetail2 = createScanDetailForNetwork(networkSuggestion2.wifiConfiguration);
        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail2);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);

        // Ensure that the previous network can no longer be looked up.
        ScanDetail scanDetail1 = createScanDetailForNetwork(networkSuggestion1.wifiConfiguration);
        assertNull(mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail1));
    }

    /**
     * Verify that we will disconnect from the network if the only network suggestion matching the
     * connected network is removed.
     */
    @Test
    public void
            testRemoveNetworkSuggestionsMatchingConnectionSuccessWithOneMatch() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        // Simulate connecting to the network.
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        // Now remove the network suggestion and ensure we did trigger a disconnect.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1));
        verify(mWifiConfigManager).removeSuggestionConfiguredNetwork(
                networkSuggestion.wifiConfiguration.getKey());
    }

    /**
     * Verify that we will disconnect from network when App removed all its suggestions by remove
     * empty list.
     */
    @Test
    public void
            testRemoveAllNetworkSuggestionsMatchingConnectionSuccessWithOneMatch() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        // Simulate connecting to the network.
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        // Now remove all network suggestion and ensure we did trigger a disconnect.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(new ArrayList<>(), TEST_UID_1,
                        TEST_PACKAGE_1));
        verify(mWifiConfigManager).removeSuggestionConfiguredNetwork(
                networkSuggestion.wifiConfiguration.getKey());
    }


    /**
     * Verify that we do not disconnect from the network if removed suggestions is not currently
     * connected. Verify that we do disconnect from the network is removed suggestions is currently
     * connected.
     */
    @Test
    public void testRemoveAppMatchingConnectionSuccessWithMultipleMatch() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);

        // Simulate connecting to the network.
        WifiConfiguration connectNetwork =
                new WifiConfiguration(wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        // Now remove the current connected app and ensure we did trigger a disconnect.
        mWifiNetworkSuggestionsManager.removeApp(TEST_PACKAGE_1);
        verify(mWifiConfigManager).removeSuggestionConfiguredNetwork(
                networkSuggestion1.wifiConfiguration.getKey());

        // Now remove the other app and ensure we did not trigger a disconnect.
        mWifiNetworkSuggestionsManager.removeApp(TEST_PACKAGE_2);
        verify(mWifiConfigManager).removeSuggestionConfiguredNetwork(anyString());
    }

    /**
     * Verify that we do not disconnect from the network if there are no network suggestion matching
     * the connected network when one of the app is removed.
     */
    @Test
    public void testRemoveAppNotMatchingConnectionSuccess() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        // Simulate connecting to some other network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE,
                WifiConfigurationTestUtil.createEapNetwork(), TEST_BSSID);

        // Now remove the app and ensure we did not trigger a disconnect.
        mWifiNetworkSuggestionsManager.removeApp(TEST_PACKAGE_1);
        verify(mWifiConfigManager, never()).removeSuggestionConfiguredNetwork(anyString());
    }

    /**
     * Verify that we do not disconnect from the network if there are no network suggestion matching
     * the connected network when one of them is removed.
     */
    @Test
    public void testRemoveNetworkSuggestionsNotMatchingConnectionSuccessAfterConnectionFailure() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        WifiConfiguration connectNetwork =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;
        // Simulate failing connection to the network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_DHCP, connectNetwork, TEST_BSSID);

        // Simulate connecting to some other network.
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE,
                WifiConfigurationTestUtil.createEapNetwork(), TEST_BSSID);

        // Now remove the app and ensure we did not trigger a disconnect.
        mWifiNetworkSuggestionsManager.removeApp(TEST_PACKAGE_1);
        verify(mWifiConfigManager, never()).removeSuggestionConfiguredNetwork(anyString());
    }

    /**
     * Verify that we start tracking app-ops on first network suggestion add & stop tracking on the
     * last network suggestion remove.
     */
    @Test
    public void testAddRemoveNetworkSuggestionsStartStopAppOpsWatch() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        mInorder = inOrder(mAppOpsManager);

        // Watch app-ops changes on first add.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mInorder.verify(mAppOpsManager).startWatchingMode(eq(OPSTR_CHANGE_WIFI_STATE),
                eq(TEST_PACKAGE_1), mAppOpChangedListenerCaptor.capture());

        // Nothing happens on second add.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        // Now remove first add, nothing happens.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1));
        // Stop watching app-ops changes on last remove.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList2, TEST_UID_1,
                        TEST_PACKAGE_1));
        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());
        mInorder.verify(mAppOpsManager).stopWatchingMode(mAppOpChangedListenerCaptor.getValue());

        mInorder.verifyNoMoreInteractions();
    }

    /**
     * Verify app-ops disable/enable after suggestions add.
     */
    @Test
    public void testAppOpsChangeAfterSuggestionsAdd() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        verify(mAppOpsManager).startWatchingMode(eq(OPSTR_CHANGE_WIFI_STATE), eq(TEST_PACKAGE_1),
                mAppOpChangedListenerCaptor.capture());
        AppOpsManager.OnOpChangedListener listener = mAppOpChangedListenerCaptor.getValue();
        assertNotNull(listener);

        // allow change wifi state.
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                        TEST_PACKAGE_1))
                .thenReturn(MODE_ALLOWED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        allNetworkSuggestions = mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        // disallow change wifi state & ensure we remove the app from database.
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                        TEST_PACKAGE_1))
                .thenReturn(MODE_IGNORED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        verify(mAppOpsManager).stopWatchingMode(mAppOpChangedListenerCaptor.getValue());
        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());
        verify(mWifiMetrics).incrementNetworkSuggestionUserRevokePermission();
    }

    /**
     * Verify app-ops disable/enable after config store load.
     */
    @Test
    public void testAppOpsChangeAfterConfigStoreLoad() {
        PerAppInfo appInfo = new PerAppInfo(TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        appInfo.extNetworkSuggestions.add(
                ExtendedWifiNetworkSuggestion.fromWns(networkSuggestion, appInfo, true));
        mDataSource.fromDeserialized(new HashMap<String, PerAppInfo>() {{
                    put(TEST_PACKAGE_1, appInfo);
                }});

        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        verify(mAppOpsManager).startWatchingMode(eq(OPSTR_CHANGE_WIFI_STATE), eq(TEST_PACKAGE_1),
                mAppOpChangedListenerCaptor.capture());
        AppOpsManager.OnOpChangedListener listener = mAppOpChangedListenerCaptor.getValue();
        assertNotNull(listener);

        // allow change wifi state.
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                        TEST_PACKAGE_1))
                .thenReturn(MODE_ALLOWED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        allNetworkSuggestions = mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        // disallow change wifi state & ensure we remove all the suggestions for that app.
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                        TEST_PACKAGE_1))
                .thenReturn(MODE_IGNORED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());
    }

    /**
     * Verify app-ops disable with wrong uid to package mapping.
     */
    @Test
    public void testAppOpsChangeWrongUid() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        Set<WifiNetworkSuggestion> allNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        Set<WifiNetworkSuggestion> expectedAllNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);

        verify(mAppOpsManager).startWatchingMode(eq(OPSTR_CHANGE_WIFI_STATE), eq(TEST_PACKAGE_1),
                mAppOpChangedListenerCaptor.capture());
        AppOpsManager.OnOpChangedListener listener = mAppOpChangedListenerCaptor.getValue();
        assertNotNull(listener);

        // disallow change wifi state & ensure we don't remove all the suggestions for that app.
        doThrow(new SecurityException()).when(mAppOpsManager).checkPackage(
                eq(TEST_UID_1), eq(TEST_PACKAGE_1));
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                        TEST_PACKAGE_1))
                .thenReturn(MODE_IGNORED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        allNetworkSuggestions = mWifiNetworkSuggestionsManager.getAllNetworkSuggestions();
        assertEquals(expectedAllNetworkSuggestions, allNetworkSuggestions);
    }

    /**
     * Verify that we stop tracking the package on its removal.
     */
    @Test
    public void testRemoveApp() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));

        // Remove all suggestions from TEST_PACKAGE_1 & TEST_PACKAGE_2, we should continue to track.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2));

        assertTrue(mDataSource.hasNewDataToSerialize());
        Map<String, PerAppInfo> networkSuggestionsMapToWrite = mDataSource.toSerialize();
        assertEquals(2, networkSuggestionsMapToWrite.size());
        assertTrue(networkSuggestionsMapToWrite.keySet().contains(TEST_PACKAGE_1));
        assertTrue(networkSuggestionsMapToWrite.keySet().contains(TEST_PACKAGE_2));
        assertTrue(
                networkSuggestionsMapToWrite.get(TEST_PACKAGE_1).extNetworkSuggestions.isEmpty());
        assertTrue(
                networkSuggestionsMapToWrite.get(TEST_PACKAGE_2).extNetworkSuggestions.isEmpty());

        // Now remove TEST_PACKAGE_1, continue to track TEST_PACKAGE_2.
        mWifiNetworkSuggestionsManager.removeApp(TEST_PACKAGE_1);
        assertTrue(mDataSource.hasNewDataToSerialize());
        networkSuggestionsMapToWrite = mDataSource.toSerialize();
        assertEquals(1, networkSuggestionsMapToWrite.size());
        assertTrue(networkSuggestionsMapToWrite.keySet().contains(TEST_PACKAGE_2));
        assertTrue(
                networkSuggestionsMapToWrite.get(TEST_PACKAGE_2).extNetworkSuggestions.isEmpty());

        // Now remove TEST_PACKAGE_2.
        mWifiNetworkSuggestionsManager.removeApp(TEST_PACKAGE_2);
        assertTrue(mDataSource.hasNewDataToSerialize());
        networkSuggestionsMapToWrite = mDataSource.toSerialize();
        assertTrue(networkSuggestionsMapToWrite.isEmpty());

        // Verify that we stopped watching these apps for app-ops changes.
        verify(mAppOpsManager, times(2)).stopWatchingMode(any());
    }


    /**
     * Verify that we stop tracking all packages & it's suggestions on network settings reset.
     */
    @Test
    public void testClear() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));

        // Remove all suggestions from TEST_PACKAGE_1 & TEST_PACKAGE_2, we should continue to track.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2));

        assertTrue(mDataSource.hasNewDataToSerialize());
        Map<String, PerAppInfo> networkSuggestionsMapToWrite = mDataSource.toSerialize();
        assertEquals(2, networkSuggestionsMapToWrite.size());
        assertTrue(networkSuggestionsMapToWrite.keySet().contains(TEST_PACKAGE_1));
        assertTrue(networkSuggestionsMapToWrite.keySet().contains(TEST_PACKAGE_2));
        assertTrue(
                networkSuggestionsMapToWrite.get(TEST_PACKAGE_1).extNetworkSuggestions.isEmpty());
        assertTrue(
                networkSuggestionsMapToWrite.get(TEST_PACKAGE_2).extNetworkSuggestions.isEmpty());

        // Now clear everything.
        mWifiNetworkSuggestionsManager.clear();
        assertTrue(mDataSource.hasNewDataToSerialize());
        networkSuggestionsMapToWrite = mDataSource.toSerialize();
        assertTrue(networkSuggestionsMapToWrite.isEmpty());

        // Verify that we stopped watching these apps for app-ops changes.
        verify(mAppOpsManager, times(2)).stopWatchingMode(any());
    }

    /**
     * Verify user dismissal notification when first time add suggestions and dismissal the user
     * approval notification when framework gets scan results.
     */
    @Test
    public void testUserApprovalNotificationDismissalWhenGetScanResult() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        validateUserApprovalNotification(TEST_APP_NAME_1);
        // Simulate user dismissal notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_DISMISSED_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);
        reset(mNotificationManger);
        verify(mWifiMetrics).addUserApprovalSuggestionAppUiReaction(
                WifiNetworkSuggestionsManager.ACTION_USER_DISMISS, false);
        // Simulate finding the network in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        validateUserApprovalNotification(TEST_APP_NAME_1);

        // Simulate user dismissal notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_DISMISSED_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);

        reset(mNotificationManger);
        // We should resend the notification next time the network is found in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        validateUserApprovalNotification(TEST_APP_NAME_1);
        verifyNoMoreInteractions(mNotificationManger);
    }

    /**
     * Verify user dismissal notification when first time add suggestions and click on allow on
     * the user approval notification when framework gets scan results.
     */
    @Test
    public void testUserApprovalNotificationClickOnAllowWhenGetScanResult() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        validateUserApprovalNotification(TEST_APP_NAME_1);

        // Simulate user dismissal notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_DISMISSED_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);
        reset(mNotificationManger);

        // Simulate finding the network in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        validateUserApprovalNotification(TEST_APP_NAME_1);

        // Simulate user clicking on allow in the notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_ALLOWED_APP_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);
        // Cancel the notification.
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);

        // Verify config store interactions.
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());
        verify(mWifiMetrics).addUserApprovalSuggestionAppUiReaction(
                WifiNetworkSuggestionsManager.ACTION_USER_ALLOWED_APP, false);

        reset(mNotificationManger);
        // We should not resend the notification next time the network is found in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));
        verifyNoMoreInteractions(mNotificationManger);
    }

    /**
     * Verify user dismissal notification when first time add suggestions and click on disallow on
     * the user approval notification when framework gets scan results.
     */
    @Test
    public void testUserApprovalNotificationClickOnDisallowWhenGetScanResult() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(mAppOpsManager).startWatchingMode(eq(OPSTR_CHANGE_WIFI_STATE),
                eq(TEST_PACKAGE_1), mAppOpChangedListenerCaptor.capture());
        validateUserApprovalNotification(TEST_APP_NAME_1);

        // Simulate user dismissal notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_DISMISSED_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);
        reset(mNotificationManger);

        // Simulate finding the network in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        validateUserApprovalNotification(TEST_APP_NAME_1);

        // Simulate user clicking on disallow in the notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_DISALLOWED_APP_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);
        // Ensure we turn off CHANGE_WIFI_STATE app-ops.
        verify(mAppOpsManager).setMode(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1, TEST_PACKAGE_1, MODE_IGNORED);
        // Cancel the notification.
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);

        // Verify config store interactions.
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());
        verify(mWifiMetrics).addUserApprovalSuggestionAppUiReaction(
                WifiNetworkSuggestionsManager.ACTION_USER_DISALLOWED_APP, false);

        reset(mNotificationManger);

        // Now trigger the app-ops callback to ensure we remove all of their suggestions.
        AppOpsManager.OnOpChangedListener listener = mAppOpChangedListenerCaptor.getValue();
        assertNotNull(listener);
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                        TEST_PACKAGE_1))
                .thenReturn(MODE_IGNORED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());

        // Assuming the user re-enabled the app again & added the same suggestions back.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        // We should resend the notification when the network is again found in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        validateUserApprovalNotification(TEST_APP_NAME_1);
        verifyNoMoreInteractions(mNotificationManger);
    }

    /**
     * Verify that we don't send a new notification when a pending notification is active.
     */
    @Test
    public void testUserApprovalNotificationWhilePreviousNotificationActive() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        // Simulate finding the network in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        validateUserApprovalNotification(TEST_APP_NAME_1);

        reset(mNotificationManger);
        // We should not resend the notification next time the network is found in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        verifyNoMoreInteractions(mNotificationManger);
    }

    /**
     * Verify get network suggestion return the right result
     * 1. App never suggested, should return empty list.
     * 2. App has network suggestions, return all its suggestion.
     * 3. App suggested and remove them all, should return empty list.
     */
    @Test
    public void testGetNetworkSuggestions() {
        // test App never suggested.
        List<WifiNetworkSuggestion> storedNetworkSuggestionListPerApp =
                mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1);
        assertEquals(storedNetworkSuggestionListPerApp.size(), 0);

        // App add network suggestions then get stored suggestions.
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOweNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion3 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createSaeNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion4 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createPskNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion1);
        networkSuggestionList.add(networkSuggestion2);
        networkSuggestionList.add(networkSuggestion3);
        networkSuggestionList.add(networkSuggestion4);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        storedNetworkSuggestionListPerApp =
                mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1);
        assertEquals(new HashSet<>(networkSuggestionList),
                new HashSet<>(storedNetworkSuggestionListPerApp));

        // App remove all network suggestions, expect empty list.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(new ArrayList<>(), TEST_UID_1,
                        TEST_PACKAGE_1));
        storedNetworkSuggestionListPerApp =
                mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1);
        assertEquals(storedNetworkSuggestionListPerApp.size(), 0);
    }

    /**
     * Verify get hidden networks from All user approve network suggestions
     */
    @Test
    public void testGetHiddenNetworks() {

        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        WifiNetworkSuggestion hiddenNetworkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createPskHiddenNetwork(), null, true, false, true, true);
        WifiNetworkSuggestion hiddenNetworkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createPskHiddenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                    add(hiddenNetworkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(hiddenNetworkSuggestion2);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(false, TEST_PACKAGE_2);
        List<WifiScanner.ScanSettings.HiddenNetwork> hiddenNetworks =
                mWifiNetworkSuggestionsManager.retrieveHiddenNetworkList();
        assertEquals(1, hiddenNetworks.size());
        assertEquals(hiddenNetworkSuggestion1.wifiConfiguration.SSID, hiddenNetworks.get(0).ssid);
    }

    /**
     * Verify handling of user clicking allow on the user approval dialog when first time
     * add suggestions.
     */
    @Test
    public void testUserApprovalDialogClickOnAllowDuringAddingSuggestionsFromFgApp() {
        // Fg app
        when(mActivityManager.getPackageImportance(any())).thenReturn(IMPORTANCE_FOREGROUND);

        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        validateUserApprovalDialog(TEST_APP_NAME_1);

        // Simulate user clicking on allow in the dialog.
        ArgumentCaptor<DialogInterface.OnClickListener> clickListenerCaptor =
                ArgumentCaptor.forClass(DialogInterface.OnClickListener.class);
        verify(mAlertDialogBuilder, atLeastOnce()).setPositiveButton(
                any(), clickListenerCaptor.capture());
        assertNotNull(clickListenerCaptor.getValue());
        clickListenerCaptor.getValue().onClick(mAlertDialog, 0);
        mLooper.dispatchAll();

        // Verify config store interactions.
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());
        verify(mWifiMetrics).addUserApprovalSuggestionAppUiReaction(
                WifiNetworkSuggestionsManager.ACTION_USER_ALLOWED_APP, true);

        // We should not resend the notification next time the network is found in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));
        verifyNoMoreInteractions(mNotificationManger);
    }

    /**
     * Verify handling of user clicking Disallow on the user approval dialog when first time
     * add suggestions.
     */
    @Test
    public void testUserApprovalDialogClickOnDisallowWhenAddSuggestionsFromFgApp() {
        // Fg app
        when(mActivityManager.getPackageImportance(any())).thenReturn(IMPORTANCE_FOREGROUND);

        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false,  true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(mAppOpsManager).startWatchingMode(eq(OPSTR_CHANGE_WIFI_STATE),
                eq(TEST_PACKAGE_1), mAppOpChangedListenerCaptor.capture());
        validateUserApprovalDialog(TEST_APP_NAME_1);

        // Simulate user clicking on disallow in the dialog.
        ArgumentCaptor<DialogInterface.OnClickListener> clickListenerCaptor =
                ArgumentCaptor.forClass(DialogInterface.OnClickListener.class);
        verify(mAlertDialogBuilder, atLeastOnce()).setNegativeButton(
                any(), clickListenerCaptor.capture());
        assertNotNull(clickListenerCaptor.getValue());
        clickListenerCaptor.getValue().onClick(mAlertDialog, 0);
        mLooper.dispatchAll();
        // Ensure we turn off CHANGE_WIFI_STATE app-ops.
        verify(mAppOpsManager).setMode(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1, TEST_PACKAGE_1, MODE_IGNORED);

        // Verify config store interactions.
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());
        verify(mWifiMetrics).addUserApprovalSuggestionAppUiReaction(
                WifiNetworkSuggestionsManager.ACTION_USER_DISALLOWED_APP, true);

        // Now trigger the app-ops callback to ensure we remove all of their suggestions.
        AppOpsManager.OnOpChangedListener listener = mAppOpChangedListenerCaptor.getValue();
        assertNotNull(listener);
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                TEST_PACKAGE_1))
                .thenReturn(MODE_IGNORED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());

        // Assuming the user re-enabled the app again & added the same suggestions back.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        validateUserApprovalDialog(TEST_APP_NAME_1);
        verifyNoMoreInteractions(mNotificationManger);
    }

    /**
     * Verify handling of dismissal of the user approval dialog when first time
     * add suggestions.
     */
    @Test
    public void testUserApprovalDialiogDismissDuringAddingSuggestionsFromFgApp() {
        // Fg app
        when(mActivityManager.getPackageImportance(any())).thenReturn(IMPORTANCE_FOREGROUND);

        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        validateUserApprovalDialog(TEST_APP_NAME_1);

        // Simulate user clicking on allow in the dialog.
        ArgumentCaptor<DialogInterface.OnDismissListener> dismissListenerCaptor =
                ArgumentCaptor.forClass(DialogInterface.OnDismissListener.class);
        verify(mAlertDialogBuilder, atLeastOnce()).setOnDismissListener(
                dismissListenerCaptor.capture());
        assertNotNull(dismissListenerCaptor.getValue());
        dismissListenerCaptor.getValue().onDismiss(mAlertDialog);
        mLooper.dispatchAll();

        // Verify no new config store or app-op interactions.
        verify(mWifiConfigManager).saveToStore(true); // 1 already done for add
        verify(mAppOpsManager, never()).setMode(any(), anyInt(), any(), anyInt());
        verify(mWifiMetrics).addUserApprovalSuggestionAppUiReaction(
                WifiNetworkSuggestionsManager.ACTION_USER_DISMISS, true);

        // We should resend the notification next time the network is found in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));
        validateUserApprovalNotification(TEST_APP_NAME_1);
    }

    @Test
    public void testAddNetworkSuggestions_activeFgScorer_doesNotRequestForApproval() {
        // Fg app
        when(mActivityManager.getPackageImportance(any())).thenReturn(IMPORTANCE_FOREGROUND);
        // Active scorer
        when(mNetworkScoreManager.getActiveScorerPackage()).thenReturn(TEST_PACKAGE_1);

        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        verifyZeroInteractions(mAlertDialog);
        verifyZeroInteractions(mNotificationManger);
    }

    @Test
    public void testAddNetworkSuggestions_activeBgScorer_doesNotRequestForApproval() {
        // Bg app
        when(mActivityManager.getPackageImportance(any())).thenReturn(IMPORTANCE_SERVICE);
        // Active scorer
        when(mNetworkScoreManager.getActiveScorerPackage()).thenReturn(TEST_PACKAGE_1);

        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        verifyZeroInteractions(mAlertDialog);
        verifyZeroInteractions(mNotificationManger);
    }

    /**
     * Verify handling of user clicking allow on the user approval notification when first time
     * add suggestions.
     */
    @Test
    public void testUserApprovalNotificationClickOnAllowDuringAddingSuggestionsFromNonFgApp() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        validateUserApprovalNotification(TEST_APP_NAME_1);

        // Simulate user clicking on allow in the notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_ALLOWED_APP_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);
        // Cancel the notification.
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);

        // Verify config store interactions.
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());

        reset(mNotificationManger);
        // We should not resend the notification next time the network is found in scan results.
        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));
        verifyNoMoreInteractions(mNotificationManger);
    }

    @Test
    public void getNetworkSuggestionsForScanDetail_exemptsActiveScorerFromUserApproval() {
        when(mNetworkScoreManager.getActiveScorerPackage()).thenReturn(TEST_PACKAGE_1);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(
                createScanDetailForNetwork(networkSuggestion.wifiConfiguration));

        verifyZeroInteractions(mNotificationManger);
        verifyZeroInteractions(mAlertDialog);
    }

    /**
     * Verify handling of user clicking Disallow on the user approval notification when first time
     * add suggestions.
     */
    @Test
    public void testUserApprovalNotificationClickOnDisallowWhenAddSuggestionsFromNonFgApp() {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false,  true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(mAppOpsManager).startWatchingMode(eq(OPSTR_CHANGE_WIFI_STATE),
                eq(TEST_PACKAGE_1), mAppOpChangedListenerCaptor.capture());
        validateUserApprovalNotification(TEST_APP_NAME_1);

        // Simulate user clicking on disallow in the notification.
        sendBroadcastForUserActionOnApp(
                NOTIFICATION_USER_DISALLOWED_APP_INTENT_ACTION, TEST_PACKAGE_1, TEST_UID_1);
        // Ensure we turn off CHANGE_WIFI_STATE app-ops.
        verify(mAppOpsManager).setMode(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1, TEST_PACKAGE_1, MODE_IGNORED);
        // Cancel the notification.
        verify(mNotificationManger).cancel(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE);

        // Verify config store interactions.
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        assertTrue(mDataSource.hasNewDataToSerialize());

        reset(mNotificationManger);

        // Now trigger the app-ops callback to ensure we remove all of their suggestions.
        AppOpsManager.OnOpChangedListener listener = mAppOpChangedListenerCaptor.getValue();
        assertNotNull(listener);
        when(mAppOpsManager.unsafeCheckOpNoThrow(
                OPSTR_CHANGE_WIFI_STATE, TEST_UID_1,
                TEST_PACKAGE_1))
                .thenReturn(MODE_IGNORED);
        listener.onOpChanged(OPSTR_CHANGE_WIFI_STATE, TEST_PACKAGE_1);
        mLooper.dispatchAll();
        assertTrue(mWifiNetworkSuggestionsManager.getAllNetworkSuggestions().isEmpty());

        // Assuming the user re-enabled the app again & added the same suggestions back.
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        validateUserApprovalNotification(TEST_APP_NAME_1);
        verifyNoMoreInteractions(mNotificationManger);
    }

    /**
     * Verify a successful lookup of a single passpoint network suggestion matching the
     * connected network.
     * a) The corresponding network suggestion has the
     * {@link WifiNetworkSuggestion#isAppInteractionRequired} flag set.
     * b) The app holds location permission.
     * This should trigger a broadcast to the app.
     */
    @Test
    public void testOnPasspointNetworkConnectionSuccessWithOneMatch() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                dummyConfiguration, passpointConfiguration, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        when(mPasspointManager.addOrUpdateProvider(any(), anyInt(), anyString(), anyBoolean(),
                anyBoolean())).thenReturn(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);

        // Simulate connecting to the network.
        WifiConfiguration connectNetwork = WifiConfigurationTestUtil.createPasspointNetwork();
        connectNetwork.FQDN = TEST_FQDN;
        connectNetwork.providerFriendlyName = TEST_FRIENDLY_NAME;
        connectNetwork.fromWifiNetworkSuggestion = true;
        connectNetwork.ephemeral = true;
        connectNetwork.creatorName = TEST_APP_NAME_1;
        connectNetwork.creatorUid = TEST_UID_1;
        connectNetwork.setPasspointUniqueId(passpointConfiguration.getUniqueId());

        verify(mWifiMetrics, never()).incrementNetworkSuggestionApiNumConnectSuccess();
        mWifiNetworkSuggestionsManager.handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, connectNetwork, TEST_BSSID);

        verify(mWifiMetrics).incrementNetworkSuggestionApiNumConnectSuccess();

        // Verify that the correct broadcast was sent out.
        mInorder.verify(mWifiPermissionsUtil).enforceCanAccessScanResults(eq(TEST_PACKAGE_1),
                eq(TEST_FEATURE), eq(TEST_UID_1), nullable(String.class));
        validatePostConnectionBroadcastSent(TEST_PACKAGE_1, networkSuggestion);

        // Verify no more broadcast were sent out.
        mInorder.verifyNoMoreInteractions();
    }

    /**
     * Creates a scan detail corresponding to the provided network values.
     */
    private ScanDetail createScanDetailForNetwork(WifiConfiguration configuration) {
        return WifiConfigurationTestUtil.createScanDetailForNetwork(configuration,
                MacAddressUtils.createRandomUnicastAddress().toString(), -45, 0, 0, 0);
    }

    private void validatePostConnectionBroadcastSent(
            String expectedPackageName, WifiNetworkSuggestion expectedNetworkSuggestion) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<UserHandle> userHandleCaptor = ArgumentCaptor.forClass(UserHandle.class);
        mInorder.verify(mContext,  calls(1)).sendBroadcastAsUser(
                intentCaptor.capture(), userHandleCaptor.capture());

        assertEquals(userHandleCaptor.getValue(), UserHandle.SYSTEM);

        Intent intent = intentCaptor.getValue();
        assertEquals(WifiManager.ACTION_WIFI_NETWORK_SUGGESTION_POST_CONNECTION,
                intent.getAction());
        String packageName = intent.getPackage();
        WifiNetworkSuggestion networkSuggestion =
                intent.getParcelableExtra(WifiManager.EXTRA_NETWORK_SUGGESTION);
        assertEquals(expectedPackageName, packageName);
        assertEquals(expectedNetworkSuggestion, networkSuggestion);
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

    private void validateUserApprovalNotification(String... anyOfExpectedAppNames) {
        verify(mNotificationManger, atLeastOnce()).notify(
                eq(SystemMessage.NOTE_NETWORK_SUGGESTION_AVAILABLE),
                eq(mNotification));
        ArgumentCaptor<Notification.BigTextStyle> contentCaptor =
                ArgumentCaptor.forClass(Notification.BigTextStyle.class);
        verify(mNotificationBuilder, atLeastOnce()).setStyle(contentCaptor.capture());
        Notification.BigTextStyle content = contentCaptor.getValue();
        assertNotNull(content);

        boolean foundMatch = false;
        for (int i = 0; i < anyOfExpectedAppNames.length; i++) {
            foundMatch = content.getBigText().toString().contains(anyOfExpectedAppNames[i]);
            if (foundMatch) break;
        }
        assertTrue(foundMatch);
    }

    private void sendBroadcastForUserActionOnApp(String action, String packageName, int uid) {
        Intent intent = new Intent()
                .setAction(action)
                .putExtra(WifiNetworkSuggestionsManager.EXTRA_PACKAGE_NAME, packageName)
                .putExtra(WifiNetworkSuggestionsManager.EXTRA_UID, uid);
        assertNotNull(mBroadcastReceiverCaptor.getValue());
        mBroadcastReceiverCaptor.getValue().onReceive(mContext, intent);
    }

    @Test
    public void testAddSuggestionWithValidCarrierIdWithCarrierProvisionPermission() {
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        config.carrierId = VALID_CARRIER_ID;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(true);

        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);

        assertEquals(status, WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);
        verify(mWifiMetrics).incrementNetworkSuggestionApiUsageNumOfAppInType(
                WifiNetworkSuggestionsManager.APP_TYPE_NETWORK_PROVISIONING);
    }

    @Test
    public void testAddSuggestionWithValidCarrierIdWithoutCarrierProvisionPermission() {
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        config.carrierId = VALID_CARRIER_ID;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(false);

        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);

        assertEquals(status,
                WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_NOT_ALLOWED);
    }

    @Test
    public void testAddSuggestionWithDefaultCarrierIdWithoutCarrierProvisionPermission() {
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(false);

        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);

        assertEquals(status, WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);
    }

    /**
     * Verify we return the network suggestion matches the target FQDN and user already approved.
     */
    @Test
    public void testGetPasspointSuggestionFromFqdnWithUserApproval() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(dummyConfiguration,
                passpointConfiguration, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                TEST_PACKAGE_1, TEST_FEATURE), WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        Set<ExtendedWifiNetworkSuggestion> ewns =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForFqdn(TEST_FQDN);
        assertEquals(1, ewns.size());
        assertEquals(networkSuggestion, ewns.iterator().next().wns);
    }

    /**
     * Verify we return no network suggestion with matched target FQDN but user not approved.
     */
    @Test
    public void testGetPasspointSuggestionFromFqdnWithoutUserApproval() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(dummyConfiguration,
                passpointConfiguration, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        dummyConfiguration.creatorUid = TEST_UID_1;
        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                TEST_PACKAGE_1, TEST_FEATURE), WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);
        Set<ExtendedWifiNetworkSuggestion> ewns =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForFqdn(TEST_FQDN);
        assertNull(ewns);
    }

    @Test
    public void getNetworkSuggestionsForFqdn_activeScorer_doesNotRequestForUserApproval() {
        when(mNetworkScoreManager.getActiveScorerPackage()).thenReturn(TEST_PACKAGE_1);
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(dummyConfiguration,
                passpointConfiguration, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = Arrays.asList(networkSuggestion);
        dummyConfiguration.creatorUid = TEST_UID_1;
        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                TEST_PACKAGE_1, TEST_FEATURE), WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);

        Set<ExtendedWifiNetworkSuggestion> ewns =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForFqdn(TEST_FQDN);

        assertEquals(1, ewns.size());
        verifyZeroInteractions(mAlertDialog);
        verifyZeroInteractions(mNotificationManger);
    }

    /**
     * Verify return true when allow user manually connect and user approved the app
     */
    @Test
    public void testIsPasspointSuggestionSharedWithUserSetToTrue() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(dummyConfiguration,
                passpointConfiguration, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        dummyConfiguration.creatorUid = TEST_UID_1;
        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                TEST_PACKAGE_1, TEST_FEATURE), WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);

        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(false, TEST_PACKAGE_1);
        assertFalse(mWifiNetworkSuggestionsManager
                .isPasspointSuggestionSharedWithUser(dummyConfiguration));

        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        assertTrue(mWifiNetworkSuggestionsManager
                .isPasspointSuggestionSharedWithUser(dummyConfiguration));
        dummyConfiguration.meteredHint = true;
        when(mWifiCarrierInfoManager.isCarrierNetworkFromNonDefaultDataSim(dummyConfiguration))
                .thenReturn(true);
        assertFalse(mWifiNetworkSuggestionsManager
                .isPasspointSuggestionSharedWithUser(dummyConfiguration));
    }

    /**
     * Verify return true when disallow user manually connect and user approved the app
     */
    @Test
    public void testIsPasspointSuggestionSharedWithUserSetToFalse() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(dummyConfiguration,
                passpointConfiguration, true, false, false, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mPasspointManager.addOrUpdateProvider(any(PasspointConfiguration.class),
                anyInt(), anyString(), eq(true), eq(true))).thenReturn(true);
        assertEquals(mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                TEST_PACKAGE_1, TEST_FEATURE), WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);

        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(false, TEST_PACKAGE_1);
        assertFalse(mWifiNetworkSuggestionsManager
                .isPasspointSuggestionSharedWithUser(dummyConfiguration));

        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        assertFalse(mWifiNetworkSuggestionsManager
                .isPasspointSuggestionSharedWithUser(dummyConfiguration));
    }

    /**
     * test getWifiConfigForMatchedNetworkSuggestionsSharedWithUser.
     *  - app without user approval will not be returned.
     *  - open network will not be returned.
     *  - suggestion doesn't allow user manually connect will not be return.
     *  - Multiple duplicate ScanResults will only return single matched config.
     */
    @Test
    public void testGetWifiConfigForMatchedNetworkSuggestionsSharedWithUser() {
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createPskNetwork(), null, false, false, false, true);
        WifiNetworkSuggestion networkSuggestion3 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createPskNetwork(), null, false, false, true, true);
        List<ScanResult> scanResults = new ArrayList<>();
        scanResults.add(
                createScanDetailForNetwork(networkSuggestion1.wifiConfiguration).getScanResult());
        scanResults.add(
                createScanDetailForNetwork(networkSuggestion2.wifiConfiguration).getScanResult());

        // Create two same ScanResult for networkSuggestion3
        ScanResult scanResult1 = createScanDetailForNetwork(networkSuggestion3.wifiConfiguration)
                .getScanResult();
        ScanResult scanResult2 = new ScanResult(scanResult1);
        scanResults.add(scanResult1);
        scanResults.add(scanResult2);

        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
                    add(networkSuggestion3);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        setupGetConfiguredNetworksFromWcm(networkSuggestion1.wifiConfiguration,
                networkSuggestion2.wifiConfiguration, networkSuggestion3.wifiConfiguration);
        // When app is not approved, empty list will be returned
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(false, TEST_PACKAGE_1);
        List<WifiConfiguration> wifiConfigurationList = mWifiNetworkSuggestionsManager
                .getWifiConfigForMatchedNetworkSuggestionsSharedWithUser(scanResults);
        assertEquals(0, wifiConfigurationList.size());
        // App is approved, only allow user connect, not open network will return.
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        wifiConfigurationList = mWifiNetworkSuggestionsManager
                .getWifiConfigForMatchedNetworkSuggestionsSharedWithUser(scanResults);
        assertEquals(1, wifiConfigurationList.size());
        assertEquals(networkSuggestion3.wifiConfiguration, wifiConfigurationList.get(0));
    }

    private void assertSuggestionsEquals(Set<WifiNetworkSuggestion> expectedSuggestions,
            Set<ExtendedWifiNetworkSuggestion> actualExtSuggestions) {
        Set<WifiNetworkSuggestion> actualSuggestions = actualExtSuggestions.stream()
                .map(ewns -> ewns.wns)
                .collect(Collectors.toSet());
        assertEquals(expectedSuggestions, actualSuggestions);
    }

    private void setupGetConfiguredNetworksFromWcm(WifiConfiguration...configs) {
        for (int i = 0; i < configs.length; i++) {
            WifiConfiguration config = configs[i];
            when(mWifiConfigManager.getConfiguredNetwork(config.getKey())).thenReturn(config);
        }
    }

    /**
     * Verify error code returns when add SIM-based network from app has no carrier privileges.
     */
    @Test
    public void testAddSimCredentialNetworkWithoutCarrierPrivileges() {
        WifiConfiguration config =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(false);
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(TelephonyManager.UNKNOWN_CARRIER_ID);
        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_NOT_ALLOWED, status);
        verify(mNotificationManger, never()).notify(anyInt(), any());
        assertEquals(0, mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1).size());
        verify(mWifiMetrics, never()).incrementNetworkSuggestionApiUsageNumOfAppInType(anyInt());
    }

    /**
     * Verify success when add SIM-based network from app has carrier privileges.
     */
    @Test
    public void testAddSimCredentialNetworkWithCarrierPrivileges() {
        WifiConfiguration config =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(false);
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(VALID_CARRIER_ID);
        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS, status);
        verify(mNotificationManger, never()).notify(anyInt(), any());
        assertEquals(1,  mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1).size());
        verify(mWifiMetrics).incrementNetworkSuggestionApiUsageNumOfAppInType(
                WifiNetworkSuggestionsManager.APP_TYPE_CARRIER_PRIVILEGED);
    }

    /**
     * Verify success when add SIM-based network from app has carrier provision permission.
     */
    @Test
    public void testAddSimCredentialNetworkWithCarrierProvisionPermission() {
        WifiConfiguration config =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(true);
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(TelephonyManager.UNKNOWN_CARRIER_ID);
        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        assertEquals(status, WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS);
        verify(mNotificationManger, never()).notify(anyInt(), any());
        assertEquals(1,  mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1).size());
    }

    /**
     * Verify matched SIM-based network will return when imsi protection is available.
     */
    @Test
    public void testMatchSimBasedNetworkWithImsiProtection() {
        WifiConfiguration config =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(false);
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(VALID_CARRIER_ID);
        when(mWifiCarrierInfoManager.getMatchingSubId(VALID_CARRIER_ID)).thenReturn(TEST_SUBID);
        when(mWifiCarrierInfoManager.isSimPresent(TEST_SUBID)).thenReturn(true);
        when(mWifiCarrierInfoManager.requiresImsiEncryption(TEST_SUBID)).thenReturn(true);
        when(mWifiCarrierInfoManager.isImsiEncryptionInfoAvailable(TEST_SUBID)).thenReturn(true);
        ScanDetail scanDetail = createScanDetailForNetwork(config);
        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS, status);

        Set<ExtendedWifiNetworkSuggestion> matchingExtNetworkSuggestions =
                mWifiNetworkSuggestionsManager.getNetworkSuggestionsForScanDetail(scanDetail);
        Set<WifiNetworkSuggestion> expectedMatchingNetworkSuggestions =
                new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        verify(mNotificationManger, never()).notify(anyInt(), any());
        assertSuggestionsEquals(expectedMatchingNetworkSuggestions, matchingExtNetworkSuggestions);
    }

    /**
     * Verify when SIM changes, the app loses carrier privilege. Suggestions from this app will be
     * removed. If this app suggests again, it will be considered as normal suggestor.
     */
    @Test
    public void testSimStateChangeWillResetCarrierPrivilegedApp() {
        WifiConfiguration config =
                WifiConfigurationTestUtil.createEapNetwork(WifiEnterpriseConfig.Eap.SIM,
                        WifiEnterpriseConfig.Phase2.NONE);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                config, null, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = new ArrayList<>();
        networkSuggestionList.add(networkSuggestion);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(TEST_UID_1))
                .thenReturn(false);
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(VALID_CARRIER_ID);
        int status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS, status);
        verify(mNotificationManger, never()).notify(anyInt(), any());
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(TelephonyManager.UNKNOWN_CARRIER_ID);
        mWifiNetworkSuggestionsManager.resetCarrierPrivilegedApps();
        assertEquals(0,  mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1).size());
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_NOT_ALLOWED, status);
        networkSuggestionList.clear();
        networkSuggestionList.add(new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, true, false, true, true));
        status = mWifiNetworkSuggestionsManager
                .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS, status);
        verify(mNotificationManger).notify(anyInt(), any());
    }

    /**
     * Verify set AllowAutoJoin on suggestion network .
     */
    @Test
    public void testSetAllowAutoJoinOnSuggestionNetwork()  {
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        // No matching will return false.
        assertFalse(mWifiNetworkSuggestionsManager
                .allowNetworkSuggestionAutojoin(networkSuggestion.wifiConfiguration, false));
        verify(mWifiConfigManager, never()).saveToStore(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        reset(mWifiConfigManager);
        WifiConfiguration configuration =
                new WifiConfiguration(networkSuggestion.wifiConfiguration);
        configuration.fromWifiNetworkSuggestion = true;
        configuration.ephemeral = true;
        configuration.creatorName = TEST_PACKAGE_1;
        configuration.creatorUid = TEST_UID_1;

        assertTrue(mWifiNetworkSuggestionsManager
                .allowNetworkSuggestionAutojoin(configuration, false));
        verify(mWifiConfigManager).saveToStore(true);
        Set<ExtendedWifiNetworkSuggestion> matchedSuggestions = mWifiNetworkSuggestionsManager
                .getNetworkSuggestionsForWifiConfiguration(configuration,
                        TEST_BSSID);
        for (ExtendedWifiNetworkSuggestion ewns : matchedSuggestions) {
            assertFalse(ewns.isAutojoinEnabled);
        }
    }

    /**
     * Verify set AllowAutoJoin on passpoint suggestion network.
     */
    @Test
    public void testSetAllowAutoJoinOnPasspointSuggestionNetwork() {
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                dummyConfiguration, passpointConfiguration, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        when(mPasspointManager.addOrUpdateProvider(any(), anyInt(), anyString(), anyBoolean(),
                anyBoolean())).thenReturn(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        verify(mWifiConfigManager, times(2)).saveToStore(true);
        reset(mWifiConfigManager);
        // Create WifiConfiguration for Passpoint network.
        WifiConfiguration config = WifiConfigurationTestUtil.createPasspointNetwork();
        config.FQDN = TEST_FQDN;
        config.providerFriendlyName = TEST_FRIENDLY_NAME;
        config.setPasspointUniqueId(passpointConfiguration.getUniqueId());
        config.fromWifiNetworkSuggestion = true;
        config.ephemeral = true;
        config.creatorName = TEST_PACKAGE_1;
        config.creatorUid = TEST_UID_1;
        // When update PasspointManager is failure, will return false.
        when(mPasspointManager.enableAutojoin(anyString(), isNull(), anyBoolean()))
                .thenReturn(false);
        assertFalse(mWifiNetworkSuggestionsManager
                .allowNetworkSuggestionAutojoin(config, false));
        verify(mWifiConfigManager, never()).saveToStore(true);

        // When update PasspointManager is success, will return true and persist suggestion.
        when(mPasspointManager.enableAutojoin(anyString(), isNull(), anyBoolean()))
                .thenReturn(true);
        assertTrue(mWifiNetworkSuggestionsManager
                .allowNetworkSuggestionAutojoin(config, false));
        verify(mWifiConfigManager).saveToStore(true);
        Set<ExtendedWifiNetworkSuggestion> matchedSuggestions = mWifiNetworkSuggestionsManager
                .getNetworkSuggestionsForWifiConfiguration(config, TEST_BSSID);
        for (ExtendedWifiNetworkSuggestion ewns : matchedSuggestions) {
            assertFalse(ewns.isAutojoinEnabled);
        }
    }

    /**
     * Verify that both passpoint configuration and non passpoint configuration could match
     * the ScanResults.
     */
    @Test
    public void getMatchingScanResultsTestWithPasspointAndNonPasspointMatch() {
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        PasspointConfiguration mockPasspoint = mock(PasspointConfiguration.class);
        WifiNetworkSuggestion passpointSuggestion = new WifiNetworkSuggestion(
                dummyConfiguration, mockPasspoint, false, false, true, true);
        WifiNetworkSuggestion nonPasspointSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(),
                null, false, false, true, true);
        List<WifiNetworkSuggestion> suggestions = new ArrayList<>() {{
                add(passpointSuggestion);
                add(nonPasspointSuggestion);
                }};
        ScanResult passpointScanResult = new ScanResult();
        passpointScanResult.wifiSsid = WifiSsid.createFromAsciiEncoded("passpoint");
        List<ScanResult> ppSrList = new ArrayList<>() {{
                add(passpointScanResult);
                }};
        ScanResult nonPasspointScanResult = new ScanResult();
        nonPasspointScanResult.wifiSsid = WifiSsid.createFromAsciiEncoded(
                nonPasspointSuggestion.wifiConfiguration.SSID);
        List<ScanResult> nonPpSrList = new ArrayList<>() {{
                add(nonPasspointScanResult);
                }};
        List<ScanResult> allSrList = new ArrayList<>() {{
                add(passpointScanResult);
                add(nonPasspointScanResult);
                add(null);
                }};
        when(mPasspointManager.getMatchingScanResults(eq(mockPasspoint), eq(allSrList)))
                .thenReturn(ppSrList);
        ScanResultMatchInfo mockMatchInfo = mock(ScanResultMatchInfo.class);
        ScanResultMatchInfo nonPasspointMi = new ScanResultMatchInfo();
        nonPasspointMi.networkSsid = nonPasspointSuggestion.wifiConfiguration.SSID;
        MockitoSession session = ExtendedMockito.mockitoSession().strictness(Strictness.LENIENT)
                .mockStatic(ScanResultMatchInfo.class).startMocking();
        try {
            doReturn(nonPasspointMi).when(
                    () -> ScanResultMatchInfo.fromWifiConfiguration(
                            nonPasspointSuggestion.wifiConfiguration));
            doReturn(nonPasspointMi).when(
                    () -> ScanResultMatchInfo.fromScanResult(nonPasspointScanResult));
            doReturn(mockMatchInfo).when(
                    () -> ScanResultMatchInfo.fromScanResult(passpointScanResult));
            Map<WifiNetworkSuggestion, List<ScanResult>> result =
                    mWifiNetworkSuggestionsManager.getMatchingScanResults(suggestions, allSrList);
            assertEquals(2, result.size());
            assertEquals(1, result.get(nonPasspointSuggestion).size());
        } finally {
            session.finishMocking();
        }
    }

    /**
     * Verify that the wifi configuration doesn't match anything
     */
    @Test
    public void getMatchingScanResultsTestWithMatchNothing() {
        WifiNetworkSuggestion nonPasspointSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(),
                null, false, false, true, true);
        List<WifiNetworkSuggestion> suggestions = new ArrayList<>() {{
                add(nonPasspointSuggestion);
                }};
        ScanResult nonPasspointScanResult = new ScanResult();
        nonPasspointScanResult.wifiSsid = WifiSsid.createFromAsciiEncoded("non-passpoint");
        List<ScanResult> allSrList = new ArrayList<>() {{
                add(nonPasspointScanResult);
            }};

        MockitoSession session = ExtendedMockito.mockitoSession().mockStatic(
                ScanResultMatchInfo.class).startMocking();
        ScanResultMatchInfo mockMatchInfo = mock(ScanResultMatchInfo.class);
        ScanResultMatchInfo miFromConfig = new ScanResultMatchInfo();
        miFromConfig.networkSsid = nonPasspointSuggestion.wifiConfiguration.SSID;
        try {
            when(ScanResultMatchInfo.fromWifiConfiguration(any(WifiConfiguration.class)))
                    .thenReturn(miFromConfig);
            when(ScanResultMatchInfo.fromScanResult(eq(nonPasspointScanResult)))
                    .thenReturn(mockMatchInfo);
            Map<WifiNetworkSuggestion, List<ScanResult>> result =
                    mWifiNetworkSuggestionsManager.getMatchingScanResults(suggestions, allSrList);
            assertEquals(1, result.size());
            assertEquals(0, result.get(nonPasspointSuggestion).size());
        } finally {
            session.finishMocking();
        }
    }

    /**
     * Verify that the wifi configuration doesn't match anything if the Wificonfiguration
     * is invalid.
     */
    @Test
    public void getMatchingScanResultsTestWithInvalidWifiConfiguration() {
        WifiNetworkSuggestion nonPasspointSuggestion = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(),
                null, false, false, true, true);
        List<WifiNetworkSuggestion> suggestions = new ArrayList<>() {{
                add(nonPasspointSuggestion);
            }};
        ScanResult nonPasspointScanResult = new ScanResult();
        nonPasspointScanResult.wifiSsid = WifiSsid.createFromAsciiEncoded("non-passpoint");
        List<ScanResult> allSrList = new ArrayList<>() {{
                add(nonPasspointScanResult);
            }};

        MockitoSession session = ExtendedMockito.mockitoSession().mockStatic(
                ScanResultMatchInfo.class).startMocking();
        try {
            when(ScanResultMatchInfo.fromWifiConfiguration(any(WifiConfiguration.class)))
                    .thenReturn(null);

            Map<WifiNetworkSuggestion, List<ScanResult>> result =
                    mWifiNetworkSuggestionsManager.getMatchingScanResults(suggestions, allSrList);
            assertEquals(1, result.size());
            assertEquals(0, result.get(nonPasspointSuggestion).size());
        } finally {
            session.finishMocking();
        }
    }

    /**
     * Verify when matching a SIM-Based network without IMSI protection, framework will mark it
     * auto-join disable and send notification. If user click on allow, will restore the auto-join
     * config.
     */
    @Test
    public void testSendImsiProtectionNotificationOnUserAllowed() {
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(TEST_PACKAGE_1))
                .thenReturn(TEST_CARRIER_ID);
        when(mWifiCarrierInfoManager.getMatchingSubId(TEST_CARRIER_ID)).thenReturn(TEST_SUBID);
        when(mWifiCarrierInfoManager.getCarrierNameforSubId(TEST_SUBID))
                .thenReturn(TEST_CARRIER_NAME);
        when(mWifiCarrierInfoManager.requiresImsiEncryption(TEST_SUBID)).thenReturn(false);
        when(mWifiCarrierInfoManager.hasUserApprovedImsiPrivacyExemptionForCarrier(TEST_CARRIER_ID))
                .thenReturn(false);
        when(mPasspointManager.addOrUpdateProvider(any(), anyInt(), anyString(), anyBoolean(),
                anyBoolean())).thenReturn(true);

        WifiConfiguration eapSimConfig = WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE);
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithSimCredential(TEST_FQDN, TEST_IMSI, TEST_REALM);
        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.setPasspointUniqueId(passpointConfiguration.getUniqueId());
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                eapSimConfig, null, true, false, true, true);
        WifiNetworkSuggestion passpointSuggestion = new WifiNetworkSuggestion(
                dummyConfiguration, passpointConfiguration, true, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                Arrays.asList(networkSuggestion, passpointSuggestion);

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));

        verifyNoMoreInteractions(mNotificationManger);
        Set<ExtendedWifiNetworkSuggestion> matchedSuggestion = mWifiNetworkSuggestionsManager
                .getNetworkSuggestionsForScanDetail(createScanDetailForNetwork(eapSimConfig));
        verify(mWifiCarrierInfoManager)
                .sendImsiProtectionExemptionNotificationIfRequired(TEST_CARRIER_ID);
        for (ExtendedWifiNetworkSuggestion ewns : matchedSuggestion) {
            assertFalse(ewns.isAutojoinEnabled);
        }

        // Simulate user approved carrier
        mUserApproveCarrierListenerArgumentCaptor.getValue().onUserAllowed(TEST_CARRIER_ID);
        when(mWifiCarrierInfoManager.hasUserApprovedImsiPrivacyExemptionForCarrier(TEST_CARRIER_ID))
                .thenReturn(true);
        verify(mPasspointManager).enableAutojoin(anyString(), any(), anyBoolean());
        matchedSuggestion = mWifiNetworkSuggestionsManager
                .getNetworkSuggestionsForScanDetail(createScanDetailForNetwork(eapSimConfig));

        for (ExtendedWifiNetworkSuggestion ewns : matchedSuggestion) {
            assertTrue(ewns.isAutojoinEnabled);
        }
    }

    /**
     * Verify adding invalid suggestions will return right error reason code.
     */
    @Test
    public void testAddInvalidNetworkSuggestions() {
        WifiConfiguration invalidConfig = WifiConfigurationTestUtil.createOpenNetwork();
        invalidConfig.SSID = "";
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(invalidConfig,
                null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_INVALID,
                mWifiNetworkSuggestionsManager
                        .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE));
    }

    /**
     * Verify adding invalid passpoint suggestions will return right error reason code.
     */
    @Test
    public void testAddInvalidPasspointNetworkSuggestions() {
        PasspointConfiguration passpointConfiguration = new PasspointConfiguration();
        HomeSp homeSp = new HomeSp();
        homeSp.setFqdn(TEST_FQDN);
        passpointConfiguration.setHomeSp(homeSp);
        WifiConfiguration dummyConfig = new WifiConfiguration();
        dummyConfig.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(dummyConfig,
                passpointConfiguration, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_INVALID,
                mWifiNetworkSuggestionsManager
                        .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE));
    }

    /**
     * Verify getAllScanOptimizationSuggestionNetworks will only return user approved,
     * non-passpoint network.
     */
    @Test
    public void testGetPnoAvailableSuggestions() {
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        WifiConfiguration network2 = WifiConfigurationTestUtil.createOpenNetwork();
        PasspointConfiguration passpointConfiguration =
                createTestConfigWithUserCredential(TEST_FQDN, TEST_FRIENDLY_NAME);
        WifiConfiguration dummyConfig = new WifiConfiguration();
        dummyConfig.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion =
                new WifiNetworkSuggestion(network1, null, false, false, true, true);
        WifiNetworkSuggestion passpointSuggestion = new WifiNetworkSuggestion(dummyConfig,
                passpointConfiguration, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion);
                    add(passpointSuggestion);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager
                        .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE));
        assertTrue(mWifiNetworkSuggestionsManager
                .getAllScanOptimizationSuggestionNetworks().isEmpty());
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        List<WifiConfiguration> pnoNetwork =
                mWifiNetworkSuggestionsManager.getAllScanOptimizationSuggestionNetworks();
        assertEquals(1, pnoNetwork.size());
        assertEquals(network1.SSID, pnoNetwork.get(0).SSID);
    }

    @Test
    public void getAllScanOptimizationSuggestionNetworks_returnsActiveScorerWithoutUserApproval() {
        when(mNetworkScoreManager.getActiveScorerPackage()).thenReturn(TEST_PACKAGE_1);
        WifiConfiguration network = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion =
                new WifiNetworkSuggestion(network, null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = Arrays.asList(networkSuggestion);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager
                        .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(false, TEST_PACKAGE_1);

        List<WifiConfiguration> networks =
                mWifiNetworkSuggestionsManager.getAllScanOptimizationSuggestionNetworks();

        assertEquals(1, networks.size());
    }

    /**
     * Verify if a suggestion is mostRecently connected, flag will be persist.
     */
    @Test
    public void testIsMostRecentlyConnectedSuggestion() {
        WifiConfiguration network = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion =
                new WifiNetworkSuggestion(network, null, false, false, true, true);
        List<WifiNetworkSuggestion> networkSuggestionList = Arrays.asList(networkSuggestion);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager
                        .add(networkSuggestionList, TEST_UID_1, TEST_PACKAGE_1, TEST_FEATURE));
        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        when(mLruConnectionTracker.isMostRecentlyConnected(any())).thenReturn(true);
        Map<String, PerAppInfo> suggestionStore = new HashMap<>(mDataSource.toSerialize());
        PerAppInfo perAppInfo = suggestionStore.get(TEST_PACKAGE_1);
        ExtendedWifiNetworkSuggestion ewns = perAppInfo.extNetworkSuggestions.iterator().next();
        assertTrue(ewns.wns.wifiConfiguration.isMostRecentlyConnected);
        mDataSource.fromDeserialized(suggestionStore);
        verify(mLruConnectionTracker).addNetwork(any());
        reset(mLruConnectionTracker);

        when(mLruConnectionTracker.isMostRecentlyConnected(any())).thenReturn(false);
        suggestionStore = mDataSource.toSerialize();
        perAppInfo = suggestionStore.get(TEST_PACKAGE_1);
        ewns = perAppInfo.extNetworkSuggestions.iterator().next();
        assertFalse(ewns.wns.wifiConfiguration.isMostRecentlyConnected);
        mDataSource.fromDeserialized(suggestionStore);
        verify(mLruConnectionTracker, never()).addNetwork(any());
    }

    @Test
    public void testOnSuggestionUpdateListener() {
        WifiNetworkSuggestionsManager.OnSuggestionUpdateListener listener =
                mock(WifiNetworkSuggestionsManager.OnSuggestionUpdateListener.class);
        mWifiNetworkSuggestionsManager.addOnSuggestionUpdateListener(listener);

        WifiConfiguration dummyConfiguration = createDummyWifiConfigurationForPasspoint(TEST_FQDN);
        dummyConfiguration.FQDN = TEST_FQDN;
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                WifiConfigurationTestUtil.createOpenNetwork(), null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        verify(listener).onSuggestionsAddedOrUpdated(networkSuggestionList1);

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1));
        verify(listener).onSuggestionsRemoved(networkSuggestionList1);
    }

    @Test
    public void testShouldNotBeIgnoredBySecureSuggestionFromSameCarrierWithoutSameOpenSuggestion() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Without same open suggestion in the framework, should not be ignored.
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldNotBeIgnoredBySecureSuggestionFromSameCarrierWithoutSecureSuggestion() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Without secure suggestion in the framework, should not be ignored.
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldNotBeIgnoredWithoutCarrierProvisioningPermission() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiCarrierInfoManager.getCarrierIdForPackageWithCarrierPrivileges(anyString()))
                .thenReturn(TEST_CARRIER_ID);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Without CarrierProvisioningPermission, should not be ignored.
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1, suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldNotBeIgnoredBySecureSuggestionFromDifferentCarrierId() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = VALID_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Open and secure suggestions have different carrierId, should not be ignored.
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1, suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldNotBeIgnoredBySecureSuggestionFromSameCarrierWithAutojoinDisabled() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, false);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Secure suggestions is auto-join disabled, should not be ignored.
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1, suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldNotBeIgnoredBySecureSuggestionFromSameCarrierWithDifferentMeterness() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        network2.meteredOverride = WifiConfiguration.METERED_OVERRIDE_METERED;
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Secure suggestions is auto-join disabled, should not be ignored.
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1, suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldNotBeIgnoredBySecureSuggestionFromSameCarrierWithNetworkDisabled() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        WifiConfiguration wcmConfig = new WifiConfiguration(network2);
        WifiConfiguration.NetworkSelectionStatus status =
                mock(WifiConfiguration.NetworkSelectionStatus.class);
        when(status.isNetworkEnabled()).thenReturn(false);
        wcmConfig.setNetworkSelectionStatus(status);
        when(mWifiConfigManager.getConfiguredNetwork(network2.getKey())).thenReturn(wcmConfig);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Secure suggestions is auto-join disabled, should not be ignored.
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1, suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldNotBeIgnoredBySecureSuggestionFromSameCarrierWithOverlayFalse() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(false);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Both open and secure suggestions with same carrierId,
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1, suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testShouldBeIgnoredBySecureSuggestionFromSameCarrier() {
        when(mResources.getBoolean(
                R.bool.config_wifiIgnoreOpenSavedNetworkWhenSecureSuggestionAvailable))
                .thenReturn(true);
        when(mWifiPermissionsUtil.checkNetworkCarrierProvisioningPermission(anyInt()))
                .thenReturn(true);
        WifiConfiguration network1 = WifiConfigurationTestUtil.createOpenNetwork();
        ScanDetail scanDetail1 = createScanDetailForNetwork(network1);
        network1.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion1 = new WifiNetworkSuggestion(
                network1, null, false, false, true, true);
        WifiConfiguration network2 = WifiConfigurationTestUtil.createPskNetwork();
        ScanDetail scanDetail2 = createScanDetailForNetwork(network2);
        network2.carrierId = TEST_CARRIER_ID;
        WifiNetworkSuggestion suggestion2 = new WifiNetworkSuggestion(
                network2, null, false, false, true, true);

        List<ScanDetail> scanDetails = Arrays.asList(scanDetail1, scanDetail2);
        // Both open and secure suggestions with same carrierId,
        List<WifiNetworkSuggestion> suggestionList = Arrays.asList(suggestion1, suggestion2);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(suggestionList, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertTrue(mWifiNetworkSuggestionsManager
                .shouldBeIgnoredBySecureSuggestionFromSameCarrier(network1, scanDetails));
    }

    @Test
    public void testUnregisterSuggestionConnectionStatusListenerNeverRegistered() {
        int listenerIdentifier = 1234;
        mWifiNetworkSuggestionsManager.unregisterSuggestionConnectionStatusListener(
                listenerIdentifier, TEST_PACKAGE_1, TEST_UID_1);
    }

    /**
     * Verify that we only return user approved suggestions.
     */
    @Test
    public void testGetApprovedNetworkSuggestions() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion1 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);
        // Reuse the same network credentials to ensure they both match.
        WifiNetworkSuggestion networkSuggestion2 = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);

        List<WifiNetworkSuggestion> networkSuggestionList1 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }};
        List<WifiNetworkSuggestion> networkSuggestionList2 =
                new ArrayList<WifiNetworkSuggestion>() {{
                    add(networkSuggestion2);
                }};

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList1, TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(networkSuggestionList2, TEST_UID_2,
                        TEST_PACKAGE_2, TEST_FEATURE));

        // nothing approved, return empty.
        assertTrue(mWifiNetworkSuggestionsManager.getAllApprovedNetworkSuggestions().isEmpty());

        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_1);
        // only app 1 approved.
        assertEquals(new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                }},
                mWifiNetworkSuggestionsManager.getAllApprovedNetworkSuggestions());

        mWifiNetworkSuggestionsManager.setHasUserApprovedForApp(true, TEST_PACKAGE_2);
        // both app 1 & 2 approved.
        assertEquals(new HashSet<WifiNetworkSuggestion>() {{
                    add(networkSuggestion1);
                    add(networkSuggestion2);
                }},
                mWifiNetworkSuggestionsManager.getAllApprovedNetworkSuggestions());
    }

    /**
     * Verify when calling API from background user will fail.
     */
    @Test
    public void testCallingFromBackgroundUserWillFailed() {
        WifiConfiguration wifiConfiguration = WifiConfigurationTestUtil.createOpenNetwork();
        WifiNetworkSuggestion networkSuggestion = new WifiNetworkSuggestion(
                wifiConfiguration, null, false, false, true, true);

        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(Arrays.asList(networkSuggestion), TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        // When switch the user to background
        when(mWifiPermissionsUtil.doesUidBelongToCurrentUser(TEST_UID_1)).thenReturn(false);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_INTERNAL,
                mWifiNetworkSuggestionsManager.add(Arrays.asList(networkSuggestion), TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_INTERNAL,
                mWifiNetworkSuggestionsManager.remove(Arrays.asList(networkSuggestion), TEST_UID_1,
                        TEST_PACKAGE_1));
        assertTrue(mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1).isEmpty());
        assertFalse(mWifiNetworkSuggestionsManager.registerSuggestionConnectionStatusListener(
                mBinder, mListener, NETWORK_CALLBACK_ID, TEST_PACKAGE_1, TEST_UID_1));

        // When switch the user back to foreground
        when(mWifiPermissionsUtil.doesUidBelongToCurrentUser(TEST_UID_1)).thenReturn(true);
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.add(Arrays.asList(networkSuggestion), TEST_UID_1,
                        TEST_PACKAGE_1, TEST_FEATURE));
        assertFalse(mWifiNetworkSuggestionsManager.get(TEST_PACKAGE_1, TEST_UID_1).isEmpty());
        assertEquals(WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
                mWifiNetworkSuggestionsManager.remove(Arrays.asList(networkSuggestion), TEST_UID_1,
                        TEST_PACKAGE_1));
        assertTrue(mWifiNetworkSuggestionsManager.registerSuggestionConnectionStatusListener(
                mBinder, mListener, NETWORK_CALLBACK_ID, TEST_PACKAGE_1, TEST_UID_1));
    }

    /**
     * Helper function for creating a test configuration with user credential.
     *
     * @return {@link PasspointConfiguration}
     */
    private PasspointConfiguration createTestConfigWithUserCredential(String fqdn,
            String friendlyName) {
        PasspointConfiguration config = new PasspointConfiguration();
        HomeSp homeSp = new HomeSp();
        homeSp.setFqdn(fqdn);
        homeSp.setFriendlyName(friendlyName);
        config.setHomeSp(homeSp);
        Map<String, String> friendlyNames = new HashMap<>();
        friendlyNames.put("en", friendlyName);
        friendlyNames.put("kr", friendlyName + 1);
        friendlyNames.put("jp", friendlyName + 2);
        config.setServiceFriendlyNames(friendlyNames);
        Credential credential = new Credential();
        credential.setRealm(TEST_REALM);
        credential.setCaCertificate(FakeKeys.CA_CERT0);
        Credential.UserCredential userCredential = new Credential.UserCredential();
        userCredential.setUsername("username");
        userCredential.setPassword("password");
        userCredential.setEapType(EAPConstants.EAP_TTLS);
        userCredential.setNonEapInnerMethod(Credential.UserCredential.AUTH_METHOD_MSCHAP);
        credential.setUserCredential(userCredential);
        config.setCredential(credential);
        return config;
    }

    /**
     * Helper function for creating a test configuration with SIM credential.
     *
     * @return {@link PasspointConfiguration}
     */
    private PasspointConfiguration createTestConfigWithSimCredential(String fqdn, String imsi,
            String realm) {
        PasspointConfiguration config = new PasspointConfiguration();
        HomeSp homeSp = new HomeSp();
        homeSp.setFqdn(fqdn);
        homeSp.setFriendlyName(TEST_FRIENDLY_NAME);
        config.setHomeSp(homeSp);
        Credential credential = new Credential();
        credential.setRealm(realm);
        Credential.SimCredential simCredential = new Credential.SimCredential();
        simCredential.setImsi(imsi);
        simCredential.setEapType(EAPConstants.EAP_SIM);
        credential.setSimCredential(simCredential);
        config.setCredential(credential);
        return config;
    }

    private WifiConfiguration createDummyWifiConfigurationForPasspoint(String fqdn) {
        WifiConfiguration config = new WifiConfiguration();
        config.FQDN = fqdn;
        return config;
    }
}
