/*
 * Copyright (C) 2015 The Android Open Source Project
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

import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_METERED;
import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_NONE;
import static android.net.wifi.WifiConfiguration.METERED_OVERRIDE_NOT_METERED;
import static android.net.wifi.WifiConfiguration.NetworkSelectionStatus.DISABLED_NONE;
import static android.net.wifi.WifiConfiguration.NetworkSelectionStatus.DISABLED_NO_INTERNET_TEMPORARY;

import static com.android.server.wifi.ClientModeImpl.CMD_PRE_DHCP_ACTION;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyByte;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.anyObject;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.ActivityManager;
import android.app.test.MockAnswerUtil.AnswerWithArguments;
import android.app.test.TestAlarmManager;
import android.bluetooth.BluetoothAdapter;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.wifi.supplicant.V1_0.ISupplicantStaIfaceCallback;
import android.net.ConnectivityManager;
import android.net.DhcpResultsParcelable;
import android.net.InetAddresses;
import android.net.Layer2InformationParcelable;
import android.net.Layer2PacketParcelable;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.MacAddress;
import android.net.Network;
import android.net.NetworkAgent;
import android.net.NetworkAgentConfig;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkSpecifier;
import android.net.StaticIpConfiguration;
import android.net.ip.IIpClient;
import android.net.ip.IpClientCallbacks;
import android.net.wifi.IActionListener;
import android.net.wifi.ScanResult;
import android.net.wifi.SupplicantState;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiConfiguration.KeyMgmt;
import android.net.wifi.WifiEnterpriseConfig;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiNetworkAgentSpecifier;
import android.net.wifi.WifiScanner;
import android.net.wifi.WifiSsid;
import android.net.wifi.hotspot2.IProvisioningCallback;
import android.net.wifi.hotspot2.OsuProvider;
import android.net.wifi.nl80211.WifiNl80211Manager;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.BatteryStatsManager;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.IPowerManager;
import android.os.IThermalService;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.PowerManager;
import android.os.Process;
import android.os.UserManager;
import android.os.test.TestLooper;
import android.provider.Settings;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;
import android.util.Log;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import com.android.dx.mockito.inline.extended.ExtendedMockito;
import com.android.internal.util.AsyncChannel;
import com.android.internal.util.IState;
import com.android.internal.util.StateMachine;
import com.android.server.wifi.hotspot2.NetworkDetail;
import com.android.server.wifi.hotspot2.PasspointManager;
import com.android.server.wifi.hotspot2.PasspointProvisioningTestUtil;
import com.android.server.wifi.proto.nano.WifiMetricsProto;
import com.android.server.wifi.proto.nano.WifiMetricsProto.StaEvent;
import com.android.server.wifi.proto.nano.WifiMetricsProto.WifiIsUnusableEvent;
import com.android.server.wifi.proto.nano.WifiMetricsProto.WifiUsabilityStats;
import com.android.server.wifi.util.RssiUtilTest;
import com.android.server.wifi.util.ScanResultUtil;
import com.android.server.wifi.util.WifiPermissionsUtil;
import com.android.server.wifi.util.WifiPermissionsWrapper;
import com.android.wifi.resources.R;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.MockitoSession;
import org.mockito.quality.Strictness;

import java.io.ByteArrayOutputStream;
import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.function.Consumer;

/**
 * Unit tests for {@link com.android.server.wifi.ClientModeImpl}.
 */
@SmallTest
public class ClientModeImplTest extends WifiBaseTest {
    public static final String TAG = "ClientModeImplTest";

    private static final int MANAGED_PROFILE_UID = 1100000;
    private static final int OTHER_USER_UID = 1200000;
    private static final int LOG_REC_LIMIT_IN_VERBOSE_MODE =
            (ActivityManager.isLowRamDeviceStatic()
                    ? ClientModeImpl.NUM_LOG_RECS_VERBOSE_LOW_MEMORY
                    : ClientModeImpl.NUM_LOG_RECS_VERBOSE);
    private static final int FRAMEWORK_NETWORK_ID = 0;
    private static final int PASSPOINT_NETWORK_ID = 1;
    private static final int TEST_RSSI = -54;
    private static final int TEST_NETWORK_ID = 54;
    private static final int WPS_SUPPLICANT_NETWORK_ID = 5;
    private static final int WPS_FRAMEWORK_NETWORK_ID = 10;
    private static final String DEFAULT_TEST_SSID = "\"GoogleGuest\"";
    private static final String OP_PACKAGE_NAME = "com.xxx";
    private static final int TEST_UID = Process.SYSTEM_UID + 1000;
    private static final MacAddress TEST_GLOBAL_MAC_ADDRESS =
            MacAddress.fromString("10:22:34:56:78:92");
    private static final MacAddress TEST_LOCAL_MAC_ADDRESS =
            MacAddress.fromString("2a:53:43:c3:56:21");
    private static final MacAddress TEST_DEFAULT_MAC_ADDRESS =
            MacAddress.fromString(WifiInfo.DEFAULT_MAC_ADDRESS);

    // NetworkAgent creates threshold ranges with Integers
    private static final int RSSI_THRESHOLD_MAX = -30;
    private static final int RSSI_THRESHOLD_MIN = -76;
    // Threshold breach callbacks are called with bytes
    private static final byte RSSI_THRESHOLD_BREACH_MIN = -80;
    private static final byte RSSI_THRESHOLD_BREACH_MAX = -20;

    private static final int DATA_SUBID = 1;
    private static final int CARRIER_ID_1 = 100;

    private long mBinderToken;
    private MockitoSession mSession;

    private static <T> T mockWithInterfaces(Class<T> class1, Class<?>... interfaces) {
        return mock(class1, withSettings().extraInterfaces(interfaces));
    }

    private void enableDebugLogs() {
        mCmi.enableVerboseLogging(1);
    }

    private FrameworkFacade getFrameworkFacade() throws Exception {
        FrameworkFacade facade = mock(FrameworkFacade.class);

        doAnswer(new AnswerWithArguments() {
            public void answer(
                    Context context, String ifname, IpClientCallbacks callback) {
                mIpClientCallback = callback;
                callback.onIpClientCreated(mIpClient);
            }
        }).when(facade).makeIpClient(any(), anyString(), any());

        return facade;
    }

    private Context getContext() throws Exception {
        when(mPackageManager.hasSystemFeature(PackageManager.FEATURE_WIFI_DIRECT)).thenReturn(true);

        Context context = mock(Context.class);
        when(context.getPackageManager()).thenReturn(mPackageManager);

        MockContentResolver mockContentResolver = new MockContentResolver();
        mockContentResolver.addProvider(Settings.AUTHORITY,
                new MockContentProvider(context) {
                    @Override
                    public Bundle call(String method, String arg, Bundle extras) {
                        return new Bundle();
                    }
                });
        when(context.getContentResolver()).thenReturn(mockContentResolver);

        when(context.getSystemService(Context.POWER_SERVICE)).thenReturn(
                new PowerManager(context, mock(IPowerManager.class), mock(IThermalService.class),
                    new Handler()));

        mAlarmManager = new TestAlarmManager();
        when(context.getSystemService(Context.ALARM_SERVICE)).thenReturn(
                mAlarmManager.getAlarmManager());

        when(context.getSystemService(Context.CONNECTIVITY_SERVICE)).thenReturn(
                mConnectivityManager);

        when(context.getOpPackageName()).thenReturn(OP_PACKAGE_NAME);

        when(context.getSystemService(ActivityManager.class)).thenReturn(
                mock(ActivityManager.class));

        WifiP2pManager p2pm = mock(WifiP2pManager.class);
        when(context.getSystemService(WifiP2pManager.class)).thenReturn(p2pm);
        final CountDownLatch untilDone = new CountDownLatch(1);
        mP2pThread = new HandlerThread("WifiP2pMockThread") {
            @Override
            protected void onLooperPrepared() {
                untilDone.countDown();
            }
        };
        mP2pThread.start();
        untilDone.await();
        Handler handler = new Handler(mP2pThread.getLooper());
        when(p2pm.getP2pStateMachineMessenger()).thenReturn(new Messenger(handler));

        return context;
    }

    private MockResources getMockResources() {
        MockResources resources = new MockResources();
        return resources;
    }

    private IState getCurrentState() throws
            NoSuchMethodException, InvocationTargetException, IllegalAccessException {
        Method method = StateMachine.class.getDeclaredMethod("getCurrentState");
        method.setAccessible(true);
        return (IState) method.invoke(mCmi);
    }

    private static HandlerThread getCmiHandlerThread(ClientModeImpl cmi) throws
            NoSuchFieldException, InvocationTargetException, IllegalAccessException {
        Field field = StateMachine.class.getDeclaredField("mSmThread");
        field.setAccessible(true);
        return (HandlerThread) field.get(cmi);
    }

    private static void stopLooper(final Looper looper) throws Exception {
        new Handler(looper).post(new Runnable() {
            @Override
            public void run() {
                looper.quitSafely();
            }
        });
    }

    private void dumpState() {
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(stream);
        mCmi.dump(null, writer, null);
        writer.flush();
        Log.d(TAG, "ClientModeImpl state -" + stream.toString());
    }

    private static ScanDetail getGoogleGuestScanDetail(int rssi, String bssid, int freq) {
        ScanResult.InformationElement[] ie = new ScanResult.InformationElement[1];
        ie[0] = ScanResults.generateSsidIe(sSSID);
        NetworkDetail nd = new NetworkDetail(sBSSID, ie, new ArrayList<String>(), sFreq);
        ScanDetail detail = new ScanDetail(nd, sWifiSsid, bssid, "", rssi, freq,
                Long.MAX_VALUE, /* needed so that scan results aren't rejected because
                                   there older than scan start */
                ie, new ArrayList<String>(), ScanResults.generateIERawDatafromScanResultIE(ie));

        return detail;
    }

    private ArrayList<ScanDetail> getMockScanResults() {
        ScanResults sr = ScanResults.create(0, 2412, 2437, 2462, 5180, 5220, 5745, 5825);
        ArrayList<ScanDetail> list = sr.getScanDetailArrayList();

        list.add(getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        return list;
    }

    private void injectDhcpSuccess(DhcpResultsParcelable dhcpResults) {
        mIpClientCallback.onNewDhcpResults(dhcpResults);
        mIpClientCallback.onProvisioningSuccess(new LinkProperties());
    }

    private void injectDhcpFailure() {
        mIpClientCallback.onNewDhcpResults((DhcpResultsParcelable) null);
        mIpClientCallback.onProvisioningFailure(new LinkProperties());
    }

    static final String   sSSID = "\"GoogleGuest\"";
    static final String   SSID_NO_QUOTE = sSSID.replace("\"", "");
    static final WifiSsid sWifiSsid = WifiSsid.createFromAsciiEncoded(SSID_NO_QUOTE);
    static final String   sBSSID = "01:02:03:04:05:06";
    static final String   sBSSID1 = "02:01:04:03:06:05";
    static final int      sFreq = 2437;
    static final int      sFreq1 = 5240;
    static final String   WIFI_IFACE_NAME = "mockWlan";
    static final String sFilsSsid = "FILS-AP";

    ClientModeImpl mCmi;
    HandlerThread mWifiCoreThread;
    HandlerThread mP2pThread;
    HandlerThread mSyncThread;
    AsyncChannel  mCmiAsyncChannel;
    AsyncChannel  mNetworkAgentAsyncChannel;
    TestAlarmManager mAlarmManager;
    MockWifiMonitor mWifiMonitor;
    TestLooper mLooper;
    Context mContext;
    MockResources mResources;
    FrameworkFacade mFrameworkFacade;
    IpClientCallbacks mIpClientCallback;
    OsuProvider mOsuProvider;
    WifiConfiguration mConnectedNetwork;
    WifiCarrierInfoManager mWifiCarrierInfoManager;

    @Mock WifiScanner mWifiScanner;
    @Mock SupplicantStateTracker mSupplicantStateTracker;
    @Mock WifiMetrics mWifiMetrics;
    @Mock UserManager mUserManager;
    @Mock BackupManagerProxy mBackupManagerProxy;
    @Mock WifiCountryCode mCountryCode;
    @Mock WifiInjector mWifiInjector;
    @Mock WifiLastResortWatchdog mWifiLastResortWatchdog;
    @Mock BssidBlocklistMonitor mBssidBlocklistMonitor;
    @Mock PropertyService mPropertyService;
    @Mock BuildProperties mBuildProperties;
    @Mock IBinder mPackageManagerBinder;
    @Mock SarManager mSarManager;
    @Mock WifiConfigManager mWifiConfigManager;
    @Mock WifiNative mWifiNative;
    @Mock WifiScoreCard mWifiScoreCard;
    @Mock WifiHealthMonitor mWifiHealthMonitor;
    @Mock WifiTrafficPoller mWifiTrafficPoller;
    @Mock WifiConnectivityManager mWifiConnectivityManager;
    @Mock WifiStateTracker mWifiStateTracker;
    @Mock PasspointManager mPasspointManager;
    @Mock SelfRecovery mSelfRecovery;
    @Mock WifiPermissionsUtil mWifiPermissionsUtil;
    @Mock IIpClient mIpClient;
    @Mock TelephonyManager mTelephonyManager;
    @Mock TelephonyManager mDataTelephonyManager;
    @Mock WrongPasswordNotifier mWrongPasswordNotifier;
    @Mock Clock mClock;
    @Mock ScanDetailCache mScanDetailCache;
    @Mock BaseWifiDiagnostics mWifiDiagnostics;
    @Mock ConnectivityManager mConnectivityManager;
    @Mock IProvisioningCallback mProvisioningCallback;
    @Mock HandlerThread mWifiHandlerThread;
    @Mock WifiPermissionsWrapper mWifiPermissionsWrapper;
    @Mock WakeupController mWakeupController;
    @Mock WifiDataStall mWifiDataStall;
    @Mock WifiNetworkFactory mWifiNetworkFactory;
    @Mock UntrustedWifiNetworkFactory mUntrustedWifiNetworkFactory;
    @Mock WifiNetworkSuggestionsManager mWifiNetworkSuggestionsManager;
    @Mock LinkProbeManager mLinkProbeManager;
    @Mock PackageManager mPackageManager;
    @Mock WifiLockManager mWifiLockManager;
    @Mock AsyncChannel mNullAsyncChannel;
    @Mock Handler mNetworkAgentHandler;
    @Mock BatteryStatsManager mBatteryStatsManager;
    @Mock MboOceController mMboOceController;
    @Mock SubscriptionManager mSubscriptionManager;
    @Mock ConnectionFailureNotifier mConnectionFailureNotifier;
    @Mock EapFailureNotifier mEapFailureNotifier;
    @Mock SimRequiredNotifier mSimRequiredNotifier;
    @Mock ThroughputPredictor mThroughputPredictor;
    @Mock ScanRequestProxy mScanRequestProxy;
    @Mock DeviceConfigFacade mDeviceConfigFacade;
    @Mock Network mNetwork;

    final ArgumentCaptor<WifiConfigManager.OnNetworkUpdateListener> mConfigUpdateListenerCaptor =
            ArgumentCaptor.forClass(WifiConfigManager.OnNetworkUpdateListener.class);

    public ClientModeImplTest() throws Exception {
    }

    @Before
    public void setUp() throws Exception {
        Log.d(TAG, "Setting up ...");

        // Ensure looper exists
        mLooper = new TestLooper();

        MockitoAnnotations.initMocks(this);

        /** uncomment this to enable logs from ClientModeImpls */
        // enableDebugLogs();
        mWifiMonitor = new MockWifiMonitor();
        when(mWifiInjector.getWifiMetrics()).thenReturn(mWifiMetrics);
        when(mWifiInjector.getClock()).thenReturn(new Clock());
        when(mWifiInjector.getWifiLastResortWatchdog()).thenReturn(mWifiLastResortWatchdog);
        when(mWifiInjector.getPropertyService()).thenReturn(mPropertyService);
        when(mWifiInjector.getBuildProperties()).thenReturn(mBuildProperties);
        when(mWifiInjector.getWifiBackupRestore()).thenReturn(mock(WifiBackupRestore.class));
        when(mWifiInjector.getWifiDiagnostics()).thenReturn(mWifiDiagnostics);
        when(mWifiInjector.getWifiConfigManager()).thenReturn(mWifiConfigManager);
        when(mWifiInjector.getWifiScanner()).thenReturn(mWifiScanner);
        when(mWifiInjector.makeWifiConnectivityManager(any()))
                .thenReturn(mWifiConnectivityManager);
        when(mWifiInjector.getPasspointManager()).thenReturn(mPasspointManager);
        when(mWifiInjector.getWifiStateTracker()).thenReturn(mWifiStateTracker);
        when(mWifiInjector.getWifiMonitor()).thenReturn(mWifiMonitor);
        when(mWifiInjector.getWifiNative()).thenReturn(mWifiNative);
        when(mWifiInjector.getWifiTrafficPoller()).thenReturn(mWifiTrafficPoller);
        when(mWifiInjector.getSelfRecovery()).thenReturn(mSelfRecovery);
        when(mWifiInjector.getWifiPermissionsUtil()).thenReturn(mWifiPermissionsUtil);
        when(mWifiInjector.makeTelephonyManager()).thenReturn(mTelephonyManager);
        when(mTelephonyManager.createForSubscriptionId(anyInt())).thenReturn(mDataTelephonyManager);
        when(mWifiInjector.getClock()).thenReturn(mClock);
        when(mWifiHandlerThread.getLooper()).thenReturn(mLooper.getLooper());
        when(mWifiInjector.getWifiHandlerThread()).thenReturn(mWifiHandlerThread);
        when(mWifiInjector.getWifiPermissionsWrapper()).thenReturn(mWifiPermissionsWrapper);
        when(mWifiInjector.getWakeupController()).thenReturn(mWakeupController);
        when(mWifiInjector.getScoringParams()).thenReturn(new ScoringParams());
        when(mWifiInjector.getWifiDataStall()).thenReturn(mWifiDataStall);
        when(mWifiInjector.makeWifiNetworkFactory(any(), any())).thenReturn(mWifiNetworkFactory);
        when(mWifiInjector.makeUntrustedWifiNetworkFactory(any(), any()))
                .thenReturn(mUntrustedWifiNetworkFactory);
        when(mWifiInjector.getWifiNetworkSuggestionsManager())
                .thenReturn(mWifiNetworkSuggestionsManager);
        when(mWifiInjector.getWifiScoreCard()).thenReturn(mWifiScoreCard);
        when(mWifiInjector.getWifiHealthMonitor()).thenReturn(mWifiHealthMonitor);
        when(mWifiInjector.getWifiLockManager()).thenReturn(mWifiLockManager);
        when(mWifiInjector.getWifiThreadRunner())
                .thenReturn(new WifiThreadRunner(new Handler(mLooper.getLooper())));
        when(mWifiInjector.makeConnectionFailureNotifier(any()))
                .thenReturn(mConnectionFailureNotifier);
        when(mWifiInjector.getBssidBlocklistMonitor()).thenReturn(mBssidBlocklistMonitor);
        when(mWifiInjector.getThroughputPredictor()).thenReturn(mThroughputPredictor);
        when(mWifiInjector.getScanRequestProxy()).thenReturn(mScanRequestProxy);
        when(mWifiInjector.getDeviceConfigFacade()).thenReturn(mDeviceConfigFacade);
        when(mWifiNetworkFactory.getSpecificNetworkRequestUidAndPackageName(any()))
                .thenReturn(Pair.create(Process.INVALID_UID, ""));
        when(mWifiNative.initialize()).thenReturn(true);
        when(mWifiNative.getFactoryMacAddress(WIFI_IFACE_NAME)).thenReturn(
                TEST_GLOBAL_MAC_ADDRESS);
        when(mWifiNative.getMacAddress(WIFI_IFACE_NAME))
                .thenReturn(TEST_GLOBAL_MAC_ADDRESS.toString());
        WifiNative.ConnectionCapabilities cap = new WifiNative.ConnectionCapabilities();
        cap.wifiStandard = ScanResult.WIFI_STANDARD_11AC;
        when(mWifiNative.getConnectionCapabilities(WIFI_IFACE_NAME)).thenReturn(cap);
        when(mWifiNative.setMacAddress(eq(WIFI_IFACE_NAME), anyObject()))
                .then(new AnswerWithArguments() {
                    public boolean answer(String iface, MacAddress mac) {
                        when(mWifiNative.getMacAddress(iface)).thenReturn(mac.toString());
                        return true;
                    }
                });
        doAnswer(new AnswerWithArguments() {
            public MacAddress answer(
                    WifiConfiguration config) {
                return config.getRandomizedMacAddress();
            }
        }).when(mWifiConfigManager).getRandomizedMacAndUpdateIfNeeded(any());
        when(mWifiNative.connectToNetwork(any(), any())).thenReturn(true);

        when(mWifiNetworkFactory.hasConnectionRequests()).thenReturn(true);
        when(mUntrustedWifiNetworkFactory.hasConnectionRequests()).thenReturn(true);

        mFrameworkFacade = getFrameworkFacade();
        mContext = getContext();

        mResources = getMockResources();
        mResources.setBoolean(R.bool.config_wifi_connected_mac_randomization_supported, true);
        mResources.setIntArray(R.array.config_wifiRssiLevelThresholds,
                RssiUtilTest.RSSI_THRESHOLDS);
        mResources.setInteger(R.integer.config_wifiPollRssiIntervalMilliseconds, 3000);
        when(mContext.getResources()).thenReturn(mResources);
        when(mContext.createContextAsUser(any(), anyInt())).thenReturn(mContext);

        when(mFrameworkFacade.getIntegerSetting(mContext,
                Settings.Global.WIFI_FREQUENCY_BAND,
                WifiManager.WIFI_FREQUENCY_BAND_AUTO)).thenReturn(
                WifiManager.WIFI_FREQUENCY_BAND_AUTO);
        when(mWifiPermissionsUtil.checkNetworkSettingsPermission(anyInt())).thenReturn(true);
        when(mWifiPermissionsWrapper.getLocalMacAddressPermission(anyInt()))
                .thenReturn(PackageManager.PERMISSION_DENIED);
        doAnswer(inv -> {
            mIpClientCallback.onQuit();
            return null;
        }).when(mIpClient).shutdown();
        when(mConnectivityManager.registerNetworkAgent(any(), any(), any(), any(), anyInt(), any(),
                anyInt())).thenReturn(mNetwork);
        List<SubscriptionInfo> subList = Arrays.asList(mock(SubscriptionInfo.class));
        when(mSubscriptionManager.getActiveSubscriptionInfoList()).thenReturn(subList);
        when(mSubscriptionManager.getActiveSubscriptionIdList())
                .thenReturn(new int[]{DATA_SUBID});

        WifiCarrierInfoManager tu = new WifiCarrierInfoManager(mTelephonyManager,
                mSubscriptionManager, mWifiInjector, mock(FrameworkFacade.class),
                mock(WifiContext.class), mock(WifiConfigStore.class), mock(Handler.class),
                mWifiMetrics);
        mWifiCarrierInfoManager = spy(tu);
        // static mocking
        mSession = ExtendedMockito.mockitoSession().strictness(Strictness.LENIENT)
                .spyStatic(MacAddress.class)
                .startMocking();
        initializeCmi();

        mOsuProvider = PasspointProvisioningTestUtil.generateOsuProvider(true);
        mConnectedNetwork = spy(WifiConfigurationTestUtil.createOpenNetwork());
        when(mNullAsyncChannel.sendMessageSynchronously(any())).thenReturn(null);
        when(mWifiScoreCard.getL2KeyAndGroupHint(any())).thenReturn(new Pair<>(null, null));
        when(mDeviceConfigFacade.isAbnormalDisconnectionBugreportEnabled()).thenReturn(true);
        when(mDeviceConfigFacade.isAbnormalConnectionFailureBugreportEnabled()).thenReturn(true);
        when(mDeviceConfigFacade.isOverlappingConnectionBugreportEnabled()).thenReturn(true);
        when(mDeviceConfigFacade.getOverlappingConnectionDurationThresholdMs()).thenReturn(
                DeviceConfigFacade.DEFAULT_OVERLAPPING_CONNECTION_DURATION_THRESHOLD_MS);
        when(mWifiScoreCard.detectAbnormalConnectionFailure(anyString()))
                .thenReturn(WifiHealthMonitor.REASON_NO_FAILURE);
        when(mWifiScoreCard.detectAbnormalDisconnection())
                .thenReturn(WifiHealthMonitor.REASON_NO_FAILURE);
        when(mThroughputPredictor.predictMaxTxThroughput(any())).thenReturn(90);
        when(mThroughputPredictor.predictMaxRxThroughput(any())).thenReturn(80);
    }

    private void registerAsyncChannel(Consumer<AsyncChannel> consumer, Messenger messenger,
            Handler wrappedHandler) {
        final AsyncChannel channel = new AsyncChannel();
        Handler handler = new Handler(mLooper.getLooper()) {
            @Override
            public void handleMessage(Message msg) {
                switch (msg.what) {
                    case AsyncChannel.CMD_CHANNEL_HALF_CONNECTED:
                        if (msg.arg1 == AsyncChannel.STATUS_SUCCESSFUL) {
                            consumer.accept(channel);
                        } else {
                            Log.d(TAG, "Failed to connect Command channel " + this);
                        }
                        break;
                    case AsyncChannel.CMD_CHANNEL_DISCONNECTED:
                        Log.d(TAG, "Command channel disconnected " + this);
                        break;
                    case AsyncChannel.CMD_CHANNEL_FULLY_CONNECTED:
                        Log.d(TAG, "Command channel fully connected " + this);
                        break;
                    default:
                        if (wrappedHandler != null) {
                            wrappedHandler.handleMessage(msg);
                        }
                        break;
                }
            }
        };

        channel.connect(mContext, handler, messenger);
        mLooper.dispatchAll();
    }

    private void registerAsyncChannel(Consumer<AsyncChannel> consumer, Messenger messenger) {
        registerAsyncChannel(consumer, messenger, null /* wrappedHandler */);
    }

    private void initializeCmi() throws Exception {
        mCmi = new ClientModeImpl(mContext, mFrameworkFacade, mLooper.getLooper(),
                mUserManager, mWifiInjector, mBackupManagerProxy, mCountryCode, mWifiNative,
                mWrongPasswordNotifier, mSarManager, mWifiTrafficPoller, mLinkProbeManager,
                mBatteryStatsManager, mSupplicantStateTracker, mMboOceController,
                mWifiCarrierInfoManager, mEapFailureNotifier, mSimRequiredNotifier);
        mCmi.start();
        mWifiCoreThread = getCmiHandlerThread(mCmi);

        registerAsyncChannel((x) -> {
            mCmiAsyncChannel = x;
        }, mCmi.getMessenger());

        mBinderToken = Binder.clearCallingIdentity();

        /* Send the BOOT_COMPLETED message to setup some CMI state. */
        mCmi.handleBootCompleted();
        mLooper.dispatchAll();

        verify(mWifiNetworkFactory, atLeastOnce()).register();
        verify(mUntrustedWifiNetworkFactory, atLeastOnce()).register();
        verify(mWifiConfigManager, atLeastOnce()).addOnNetworkUpdateListener(
                mConfigUpdateListenerCaptor.capture());
        assertNotNull(mConfigUpdateListenerCaptor.getValue());

        mCmi.initialize();
        mLooper.dispatchAll();
    }

    @After
    public void cleanUp() throws Exception {
        Binder.restoreCallingIdentity(mBinderToken);

        if (mSyncThread != null) stopLooper(mSyncThread.getLooper());
        if (mWifiCoreThread != null) stopLooper(mWifiCoreThread.getLooper());
        if (mP2pThread != null) stopLooper(mP2pThread.getLooper());

        mWifiCoreThread = null;
        mP2pThread = null;
        mSyncThread = null;
        mCmiAsyncChannel = null;
        mNetworkAgentAsyncChannel = null;
        mNetworkAgentHandler = null;
        mCmi = null;
        mSession.finishMocking();
    }

    @Test
    public void createNew() throws Exception {
        assertEquals("DefaultState", getCurrentState().getName());

        mCmi.handleBootCompleted();
        mLooper.dispatchAll();
        assertEquals("DefaultState", getCurrentState().getName());
    }

    @Test
    public void loadComponentsInStaMode() throws Exception {
        startSupplicantAndDispatchMessages();
        assertEquals("DisconnectedState", getCurrentState().getName());
    }

    @Test
    public void checkInitialStateStickyWhenDisabledMode() throws Exception {
        mLooper.dispatchAll();
        assertEquals("DefaultState", getCurrentState().getName());
        assertEquals(ClientModeImpl.DISABLED_MODE, mCmi.getOperationalModeForTest());

        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();
        assertEquals(ClientModeImpl.DISABLED_MODE, mCmi.getOperationalModeForTest());
        assertEquals("DefaultState", getCurrentState().getName());
    }

    @Test
    public void shouldStartSupplicantWhenConnectModeRequested() throws Exception {
        // The first time we start out in DefaultState, we sit around here.
        mLooper.dispatchAll();
        assertEquals("DefaultState", getCurrentState().getName());
        assertEquals(ClientModeImpl.DISABLED_MODE, mCmi.getOperationalModeForTest());

        // But if someone tells us to enter connect mode, we start up supplicant
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        mLooper.dispatchAll();
        assertEquals("DisconnectedState", getCurrentState().getName());
    }

    /**
     *  Test that mode changes accurately reflect the value for isWifiEnabled.
     */
    @Test
    public void checkIsWifiEnabledForModeChanges() throws Exception {
        // Check initial state
        mLooper.dispatchAll();
        assertEquals("DefaultState", getCurrentState().getName());
        assertEquals(WifiManager.WIFI_STATE_DISABLED, mCmi.syncGetWifiState());

        // switch to connect mode and verify wifi is reported as enabled
        startSupplicantAndDispatchMessages();

        assertEquals("DisconnectedState", getCurrentState().getName());
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());
        assertEquals("enabled", mCmi.syncGetWifiStateByName());

        // now disable wifi and verify the reported wifi state
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_DISABLED);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();
        assertEquals(ClientModeImpl.DISABLED_MODE, mCmi.getOperationalModeForTest());
        assertEquals("DefaultState", getCurrentState().getName());
        assertEquals(WifiManager.WIFI_STATE_DISABLED, mCmi.syncGetWifiState());
        assertEquals("disabled", mCmi.syncGetWifiStateByName());
        verify(mContext, never()).sendStickyBroadcastAsUser(
                (Intent) argThat(new WifiEnablingStateIntentMatcher()), any());
    }

    private class WifiEnablingStateIntentMatcher implements ArgumentMatcher<Intent> {
        @Override
        public boolean matches(Intent intent) {
            if (WifiManager.WIFI_STATE_CHANGED_ACTION != intent.getAction()) {
                // not the correct type
                return false;
            }
            return WifiManager.WIFI_STATE_ENABLING
                    == intent.getIntExtra(WifiManager.EXTRA_WIFI_STATE,
                                          WifiManager.WIFI_STATE_DISABLED);
        }
    }

    private void canForgetNetwork() throws Exception {
        when(mWifiConfigManager.removeNetwork(eq(0), anyInt(), any())).thenReturn(true);
        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.forget(0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();
        verify(mWifiConfigManager).removeNetwork(anyInt(), anyInt(), any());
    }

    /**
     * Verifies that configs can be removed when not in client mode.
     */
    @Test
    public void canForgetNetworkConfigWhenWifiDisabled() throws Exception {
        canForgetNetwork();
    }

    /**
     * Verifies that configs can be forgotten when in client mode.
     */
    @Test
    public void canForgetNetworkConfigInClientMode() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        canForgetNetwork();
    }

    private void canSaveNetworkConfig() throws Exception {
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();

        int networkId = TEST_NETWORK_ID;
        when(mWifiConfigManager.addOrUpdateNetwork(any(WifiConfiguration.class), anyInt()))
                .thenReturn(new NetworkUpdateResult(networkId));
        when(mWifiConfigManager.enableNetwork(eq(networkId), eq(false), anyInt(), any()))
                .thenReturn(true);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.save(config, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).addOrUpdateNetwork(any(WifiConfiguration.class), anyInt());
        verify(mWifiConfigManager).enableNetwork(eq(networkId), eq(false), anyInt(), any());
    }

    /**
     * Verifies that configs can be saved when not in client mode.
     */
    @Test
    public void canSaveNetworkConfigWhenWifiDisabled() throws Exception {
        canSaveNetworkConfig();
    }

    /**
     * Verifies that configs can be saved when in client mode.
     */
    @Test
    public void canSaveNetworkConfigInClientMode() throws Exception {
        loadComponentsInStaMode();
        canSaveNetworkConfig();
    }

    /**
     * Verifies that null configs are rejected in SAVE_NETWORK message.
     */
    @Test
    public void saveNetworkConfigFailsWithNullConfig() throws Exception {
        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.save(null, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onFailure(WifiManager.ERROR);

        verify(mWifiConfigManager, never())
                .addOrUpdateNetwork(any(WifiConfiguration.class), anyInt());
        verify(mWifiConfigManager, never())
                .enableNetwork(anyInt(), anyBoolean(), anyInt(), any());
    }

    /**
     * Verifies that configs save fails when the addition of network fails.
     */
    @Test
    public void saveNetworkConfigFailsWithConfigAddFailure() throws Exception {
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();

        when(mWifiConfigManager.addOrUpdateNetwork(any(WifiConfiguration.class), anyInt()))
                .thenReturn(new NetworkUpdateResult(WifiConfiguration.INVALID_NETWORK_ID));

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.save(config, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onFailure(WifiManager.ERROR);

        verify(mWifiConfigManager).addOrUpdateNetwork(any(WifiConfiguration.class), anyInt());
        verify(mWifiConfigManager, never())
                .enableNetwork(anyInt(), anyBoolean(), anyInt(), any());
    }

    /**
     * Verifies that configs save fails when the enable of network fails.
     */
    @Test
    public void saveNetworkConfigFailsWithConfigEnableFailure() throws Exception {
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();

        int networkId = 5;
        when(mWifiConfigManager.addOrUpdateNetwork(any(WifiConfiguration.class), anyInt()))
                .thenReturn(new NetworkUpdateResult(networkId));
        when(mWifiConfigManager.enableNetwork(eq(networkId), eq(false), anyInt(), any()))
                .thenReturn(false);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.save(config, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onFailure(WifiManager.ERROR);

        verify(mWifiConfigManager).addOrUpdateNetwork(any(WifiConfiguration.class), anyInt());
        verify(mWifiConfigManager).enableNetwork(eq(networkId), eq(false), anyInt(), any());
    }

    /**
     * Helper method to move through startup states.
     */
    private void startSupplicantAndDispatchMessages() throws Exception {
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_ENABLED);
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);

        mLooper.dispatchAll();

        verify(mWifiLastResortWatchdog, atLeastOnce()).clearAllFailureCounts();

        assertEquals("DisconnectedState", getCurrentState().getName());
    }

    private void initializeMocksForAddedNetwork(boolean isHidden) throws Exception {
        WifiConfiguration config = new WifiConfiguration();
        config.networkId = FRAMEWORK_NETWORK_ID;
        config.SSID = sSSID;
        config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        config.hiddenSSID = isHidden;

        when(mWifiConfigManager.getSavedNetworks(anyInt())).thenReturn(Arrays.asList(config));
        when(mWifiConfigManager.getConfiguredNetwork(0)).thenReturn(config);
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(0)).thenReturn(config);
    }

    private void initializeAndAddNetworkAndVerifySuccess() throws Exception {
        initializeAndAddNetworkAndVerifySuccess(false);
    }

    private void initializeAndAddNetworkAndVerifySuccess(boolean isHidden) throws Exception {
        loadComponentsInStaMode();
        initializeMocksForAddedNetwork(isHidden);
    }

    private void setupAndStartConnectSequence(WifiConfiguration config) throws Exception {
        when(mWifiConfigManager.enableNetwork(
                eq(config.networkId), eq(true), anyInt(), any()))
                .thenReturn(true);
        when(mWifiConfigManager.updateLastConnectUid(eq(config.networkId), anyInt()))
                .thenReturn(true);
        when(mWifiConfigManager.getConfiguredNetwork(eq(config.networkId)))
                .thenReturn(config);
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(
                eq(config.networkId))).thenReturn(config);

        verify(mWifiNative).removeAllNetworks(WIFI_IFACE_NAME);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, config.networkId, mock(Binder.class), connectActionListener, 0,
                Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(mWifiConfigManager).userEnabledNetwork(config.networkId);
        verify(connectActionListener).onSuccess();
    }

    private void validateSuccessfulConnectSequence(WifiConfiguration config) {
        verify(mWifiConfigManager).enableNetwork(
                eq(config.networkId), eq(true), anyInt(), any());
        verify(mWifiConnectivityManager).setUserConnectChoice(eq(config.networkId));
        verify(mWifiConnectivityManager).prepareForForcedConnection(eq(config.networkId));
        verify(mWifiConfigManager).getConfiguredNetworkWithoutMasking(eq(config.networkId));
        verify(mWifiNative).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
    }

    private void validateFailureConnectSequence(WifiConfiguration config) {
        verify(mWifiConfigManager).enableNetwork(
                eq(config.networkId), eq(true), anyInt(), any());
        verify(mWifiConnectivityManager).setUserConnectChoice(eq(config.networkId));
        verify(mWifiConnectivityManager).prepareForForcedConnection(eq(config.networkId));
        verify(mWifiConfigManager, never())
                .getConfiguredNetworkWithoutMasking(eq(config.networkId));
        verify(mWifiNative, never()).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
    }

    /**
     * Tests the network connection initiation sequence with the default network request pending
     * from WifiNetworkFactory.
     * This simulates the connect sequence using the public
     * {@link WifiManager#enableNetwork(int, boolean)} and ensures that we invoke
     * {@link WifiNative#connectToNetwork(WifiConfiguration)}.
     */
    @Test
    public void triggerConnect() throws Exception {
        loadComponentsInStaMode();
        WifiConfiguration config = mConnectedNetwork;
        config.networkId = FRAMEWORK_NETWORK_ID;
        config.setRandomizedMacAddress(TEST_LOCAL_MAC_ADDRESS);
        config.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_PERSISTENT;
        setupAndStartConnectSequence(config);
        validateSuccessfulConnectSequence(config);
    }

    /**
     * Tests the network connection initiation sequence with the default network request pending
     * from WifiNetworkFactory.
     * This simulates the connect sequence using the public
     * {@link WifiManager#enableNetwork(int, boolean)} and ensures that we invoke
     * {@link WifiNative#connectToNetwork(WifiConfiguration)}.
     */
    @Test
    public void triggerConnectFromNonSettingsApp() throws Exception {
        loadComponentsInStaMode();
        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        config.networkId = FRAMEWORK_NETWORK_ID;
        when(mWifiPermissionsUtil.checkNetworkSettingsPermission(Process.myUid()))
                .thenReturn(false);
        setupAndStartConnectSequence(config);
        verify(mWifiConfigManager).enableNetwork(
                eq(config.networkId), eq(true), anyInt(), any());
        verify(mWifiConnectivityManager, never()).setUserConnectChoice(eq(config.networkId));
        verify(mWifiConnectivityManager).prepareForForcedConnection(eq(config.networkId));
        verify(mWifiConfigManager).getConfiguredNetworkWithoutMasking(eq(config.networkId));
        verify(mWifiNative).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
    }

    /**
     * Tests the network connection initiation sequence with no network request pending from
     * from WifiNetworkFactory.
     * This simulates the connect sequence using the public
     * {@link WifiManager#enableNetwork(int, boolean)} and ensures that we don't invoke
     * {@link WifiNative#connectToNetwork(WifiConfiguration)}.
     */
    @Test
    public void triggerConnectWithNoNetworkRequest() throws Exception {
        loadComponentsInStaMode();
        // Remove the network requests.
        when(mWifiNetworkFactory.hasConnectionRequests()).thenReturn(false);
        when(mUntrustedWifiNetworkFactory.hasConnectionRequests()).thenReturn(false);

        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        config.networkId = FRAMEWORK_NETWORK_ID;
        setupAndStartConnectSequence(config);
        validateFailureConnectSequence(config);
    }

    /**
     * Tests the entire successful network connection flow.
     */
    @Test
    public void connect() throws Exception {
        triggerConnect();
        WifiConfiguration config = mWifiConfigManager.getConfiguredNetwork(FRAMEWORK_NETWORK_ID);
        config.carrierId = CARRIER_ID_1;
        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);

        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.ASSOCIATED));
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());

        DhcpResultsParcelable dhcpResults = new DhcpResultsParcelable();
        dhcpResults.baseConfiguration = new StaticIpConfiguration();
        dhcpResults.baseConfiguration.gateway = InetAddresses.parseNumericAddress("1.2.3.4");
        dhcpResults.baseConfiguration.ipAddress =
                new LinkAddress(InetAddresses.parseNumericAddress("192.168.1.100"), 0);
        dhcpResults.baseConfiguration.dnsServers.add(InetAddresses.parseNumericAddress("8.8.8.8"));
        dhcpResults.leaseDuration = 3600;

        injectDhcpSuccess(dhcpResults);
        mLooper.dispatchAll();

        // Verify WifiMetrics logging for metered metrics based on DHCP results
        verify(mWifiMetrics).addMeteredStat(any(), anyBoolean());
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertNotNull(wifiInfo);
        assertEquals(sBSSID, wifiInfo.getBSSID());
        assertEquals(sFreq, wifiInfo.getFrequency());
        assertTrue(sWifiSsid.equals(wifiInfo.getWifiSsid()));
        assertNull(wifiInfo.getPasspointProviderFriendlyName());
        // Ensure the connection stats for the network is updated.
        verify(mWifiConfigManager).updateNetworkAfterConnect(FRAMEWORK_NETWORK_ID);
        verify(mWifiConfigManager).updateRandomizedMacExpireTime(any(), anyLong());

        // Anonymous Identity is not set.
        assertEquals("", mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());
        verify(mWifiStateTracker).updateState(eq(WifiStateTracker.CONNECTED));
        assertEquals("ConnectedState", getCurrentState().getName());
        verify(mWifiMetrics).incrementNumOfCarrierWifiConnectionSuccess();
        verify(mWifiLockManager).updateWifiClientConnected(true);
        verify(mWifiNative).getConnectionCapabilities(any());
        verify(mThroughputPredictor).predictMaxTxThroughput(any());
        verify(mWifiMetrics).setConnectionMaxSupportedLinkSpeedMbps(90, 80);
        verify(mWifiDataStall).setConnectionCapabilities(any());
        assertEquals(90, wifiInfo.getMaxSupportedTxLinkSpeedMbps());
    }

    private void setupEapSimConnection() throws Exception {
        mConnectedNetwork = spy(WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE));
        mConnectedNetwork.carrierId = CARRIER_ID_1;
        doReturn(DATA_SUBID).when(mWifiCarrierInfoManager)
                .getBestMatchSubscriptionId(any(WifiConfiguration.class));
        when(mDataTelephonyManager.getSimOperator()).thenReturn("123456");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        mConnectedNetwork.enterpriseConfig.setAnonymousIdentity("");

        triggerConnect();

        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);
        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        assertEquals("ObtainingIpState", getCurrentState().getName());
    }

    /**
     * When the SIM card was removed, if the current wifi connection is not using it, the connection
     * should be kept.
     */
    @Test
    public void testResetSimWhenNonConnectedSimRemoved() throws Exception {
        setupEapSimConnection();
        doReturn(true).when(mWifiCarrierInfoManager).isSimPresent(eq(DATA_SUBID));
        mCmi.sendMessage(ClientModeImpl.CMD_RESET_SIM_NETWORKS,
                ClientModeImpl.RESET_SIM_REASON_SIM_REMOVED);
        mLooper.dispatchAll();

        verify(mSimRequiredNotifier, never()).showSimRequiredNotification(any(), any());

        assertEquals("ObtainingIpState", getCurrentState().getName());
    }

    /**
     * When the SIM card was removed, if the current wifi connection is using it, the connection
     * should be disconnected, otherwise, the connection shouldn't be impacted.
     */
    @Test
    public void testResetSimWhenConnectedSimRemoved() throws Exception {
        setupEapSimConnection();
        doReturn(false).when(mWifiCarrierInfoManager).isSimPresent(eq(DATA_SUBID));
        mCmi.sendMessage(ClientModeImpl.CMD_RESET_SIM_NETWORKS,
                ClientModeImpl.RESET_SIM_REASON_SIM_REMOVED);
        mLooper.dispatchAll();

        verify(mSimRequiredNotifier).showSimRequiredNotification(any(), any());
        assertEquals("DisconnectingState", getCurrentState().getName());
    }

    /**
     * When the SIM card was removed, if the current wifi connection is using it, the connection
     * should be disconnected, otherwise, the connection shouldn't be impacted.
     */
    @Test
    public void testResetSimWhenConnectedSimRemovedAfterNetworkRemoval() throws Exception {
        setupEapSimConnection();
        doReturn(false).when(mWifiCarrierInfoManager).isSimPresent(eq(DATA_SUBID));
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(null);
        mCmi.sendMessage(ClientModeImpl.CMD_RESET_SIM_NETWORKS,
                ClientModeImpl.RESET_SIM_REASON_SIM_REMOVED);
        mLooper.dispatchAll();

        verify(mSimRequiredNotifier, never()).showSimRequiredNotification(any(), any());
        assertEquals("ObtainingIpState", getCurrentState().getName());
    }

    /**
     * When the default data SIM is changed, if the current wifi connection is carrier wifi,
     * the connection should be disconnected.
     */
    @Test
    public void testResetSimWhenDefaultDataSimChanged() throws Exception {
        setupEapSimConnection();
        mCmi.sendMessage(ClientModeImpl.CMD_RESET_SIM_NETWORKS,
                ClientModeImpl.RESET_SIM_REASON_DEFAULT_DATA_SIM_CHANGED);
        mLooper.dispatchAll();

        assertEquals("DisconnectingState", getCurrentState().getName());
    }

    /**
     * Tests anonymous identity is set again whenever a connection is established for the carrier
     * that supports encrypted IMSI and anonymous identity and no real pseudonym was provided.
     */
    @Test
    public void testSetAnonymousIdentityWhenConnectionIsEstablishedNoPseudonym() throws Exception {
        mConnectedNetwork = spy(WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE));
        when(mDataTelephonyManager.getSimOperator()).thenReturn("123456");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        mConnectedNetwork.enterpriseConfig.setAnonymousIdentity("");

        String expectedAnonymousIdentity = "anonymous@wlan.mnc456.mcc123.3gppnetwork.org";
        MockitoSession mockSession = ExtendedMockito.mockitoSession()
                .mockStatic(SubscriptionManager.class)
                .startMocking();
        when(SubscriptionManager.getDefaultDataSubscriptionId()).thenReturn(DATA_SUBID);
        doReturn(true).when(mWifiCarrierInfoManager).isImsiEncryptionInfoAvailable(anyInt());

        // Initial value should be "not set"
        assertEquals("", mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());

        triggerConnect();

        // CMD_START_CONNECT should have set anonymousIdentity to anonymous@<realm>
        assertEquals(expectedAnonymousIdentity,
                mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());

        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);
        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());
        when(mWifiNative.getEapAnonymousIdentity(anyString()))
                .thenReturn(expectedAnonymousIdentity);

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(mWifiNative).getEapAnonymousIdentity(any());

        // Post connection value should remain "not set"
        assertEquals("", mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());
        // verify that WifiConfigManager#addOrUpdateNetwork() was called to clear any previously
        // stored pseudonym. i.e. to enable Encrypted IMSI for subsequent connections.
        // Note: This test will fail if future logic will have additional conditions that would
        // trigger "add or update network" operation. The test needs to be updated to account for
        // this change.
        verify(mWifiConfigManager).addOrUpdateNetwork(any(), anyInt());
        mockSession.finishMocking();
    }

    /**
     * Tests anonymous identity is set again whenever a connection is established for the carrier
     * that supports encrypted IMSI and anonymous identity but real pseudonym was provided for
     * subsequent connections.
     */
    @Test
    public void testSetAnonymousIdentityWhenConnectionIsEstablishedWithPseudonym()
            throws Exception {
        mConnectedNetwork = spy(WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE));
        when(mDataTelephonyManager.getSimOperator()).thenReturn("123456");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        mConnectedNetwork.enterpriseConfig.setAnonymousIdentity("");

        String expectedAnonymousIdentity = "anonymous@wlan.mnc456.mcc123.3gppnetwork.org";
        String pseudonym = "83bcca9384fca@wlan.mnc456.mcc123.3gppnetwork.org";
        MockitoSession mockSession = ExtendedMockito.mockitoSession()
                .mockStatic(SubscriptionManager.class)
                .startMocking();
        when(SubscriptionManager.getDefaultDataSubscriptionId()).thenReturn(DATA_SUBID);
        doReturn(true).when(mWifiCarrierInfoManager).isImsiEncryptionInfoAvailable(anyInt());

        triggerConnect();

        // CMD_START_CONNECT should have set anonymousIdentity to anonymous@<realm>
        assertEquals(expectedAnonymousIdentity,
                mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());

        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);
        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());
        when(mWifiNative.getEapAnonymousIdentity(anyString()))
                .thenReturn(pseudonym);

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(mWifiNative).getEapAnonymousIdentity(any());
        assertEquals(pseudonym,
                mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());
        // Verify that WifiConfigManager#addOrUpdateNetwork() was called if there we received a
        // real pseudonym to be stored. i.e. Encrypted IMSI will be used once, followed by
        // pseudonym usage in all subsequent connections.
        // Note: This test will fail if future logic will have additional conditions that would
        // trigger "add or update network" operation. The test needs to be updated to account for
        // this change.
        verify(mWifiConfigManager).addOrUpdateNetwork(any(), anyInt());
        mockSession.finishMocking();
    }

    /**
     * Tests anonymous identity is set again whenever a connection is established for the carrier
     * that supports encrypted IMSI and anonymous identity but real but not decorated pseudonym was
     * provided for subsequent connections.
     */
    @Test
    public void testSetAnonymousIdentityWhenConnectionIsEstablishedWithNonDecoratedPseudonym()
            throws Exception {
        mConnectedNetwork = spy(WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE));
        when(mDataTelephonyManager.getSimOperator()).thenReturn("123456");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        mConnectedNetwork.enterpriseConfig.setAnonymousIdentity("");

        String realm = "wlan.mnc456.mcc123.3gppnetwork.org";
        String expectedAnonymousIdentity = "anonymous";
        String pseudonym = "83bcca9384fca";
        MockitoSession mockSession = ExtendedMockito.mockitoSession()
                .mockStatic(SubscriptionManager.class)
                .startMocking();
        when(SubscriptionManager.getDefaultDataSubscriptionId()).thenReturn(DATA_SUBID);
        doReturn(true).when(mWifiCarrierInfoManager).isImsiEncryptionInfoAvailable(anyInt());

        triggerConnect();

        // CMD_START_CONNECT should have set anonymousIdentity to anonymous@<realm>
        assertEquals(expectedAnonymousIdentity + "@" + realm,
                mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());

        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);
        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());
        when(mWifiNative.getEapAnonymousIdentity(anyString()))
                .thenReturn(pseudonym);

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(mWifiNative).getEapAnonymousIdentity(any());
        assertEquals(pseudonym + "@" + realm,
                mConnectedNetwork.enterpriseConfig.getAnonymousIdentity());
        // Verify that WifiConfigManager#addOrUpdateNetwork() was called if there we received a
        // real pseudonym to be stored. i.e. Encrypted IMSI will be used once, followed by
        // pseudonym usage in all subsequent connections.
        // Note: This test will fail if future logic will have additional conditions that would
        // trigger "add or update network" operation. The test needs to be updated to account for
        // this change.
        verify(mWifiConfigManager).addOrUpdateNetwork(any(), anyInt());
        mockSession.finishMocking();
    }
    /**
     * Tests the Passpoint information is set in WifiInfo for Passpoint AP connection.
     */
    @Test
    public void connectPasspointAp() throws Exception {
        loadComponentsInStaMode();
        WifiConfiguration config = spy(WifiConfigurationTestUtil.createPasspointNetwork());
        config.SSID = sWifiSsid.toString();
        config.BSSID = sBSSID;
        config.networkId = FRAMEWORK_NETWORK_ID;
        config.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_PERSISTENT;
        setupAndStartConnectSequence(config);
        validateSuccessfulConnectSequence(config);

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(FRAMEWORK_NETWORK_ID, sWifiSsid, sBSSID,
                        SupplicantState.ASSOCIATING));
        mLooper.dispatchAll();

        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertNotNull(wifiInfo);
        assertEquals(WifiConfigurationTestUtil.TEST_FQDN, wifiInfo.getPasspointFqdn());
        assertEquals(WifiConfigurationTestUtil.TEST_PROVIDER_FRIENDLY_NAME,
                wifiInfo.getPasspointProviderFriendlyName());
    }

    /**
     * Tests that Passpoint fields in WifiInfo are reset when connecting to a non-Passpoint network
     * during DisconnectedState.
     * @throws Exception
     */
    @Test
    public void testResetWifiInfoPasspointFields() throws Exception {
        loadComponentsInStaMode();
        WifiConfiguration config = spy(WifiConfigurationTestUtil.createPasspointNetwork());
        config.SSID = sWifiSsid.toString();
        config.BSSID = sBSSID;
        config.networkId = PASSPOINT_NETWORK_ID;
        config.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_PERSISTENT;
        setupAndStartConnectSequence(config);
        validateSuccessfulConnectSequence(config);

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(PASSPOINT_NETWORK_ID, sWifiSsid, sBSSID,
                        SupplicantState.ASSOCIATING));
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(FRAMEWORK_NETWORK_ID, sWifiSsid, sBSSID,
                        SupplicantState.ASSOCIATING));
        mLooper.dispatchAll();

        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertNotNull(wifiInfo);
        assertNull(wifiInfo.getPasspointFqdn());
        assertNull(wifiInfo.getPasspointProviderFriendlyName());
    }

    /**
     * Tests the OSU information is set in WifiInfo for OSU AP connection.
     */
    @Test
    public void connectOsuAp() throws Exception {
        loadComponentsInStaMode();
        WifiConfiguration osuConfig = spy(WifiConfigurationTestUtil.createEphemeralNetwork());
        osuConfig.SSID = sWifiSsid.toString();
        osuConfig.BSSID = sBSSID;
        osuConfig.osu = true;
        osuConfig.networkId = FRAMEWORK_NETWORK_ID;
        osuConfig.providerFriendlyName = WifiConfigurationTestUtil.TEST_PROVIDER_FRIENDLY_NAME;
        osuConfig.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_PERSISTENT;
        setupAndStartConnectSequence(osuConfig);
        validateSuccessfulConnectSequence(osuConfig);

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(FRAMEWORK_NETWORK_ID, sWifiSsid, sBSSID,
                        SupplicantState.ASSOCIATING));
        mLooper.dispatchAll();

        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertNotNull(wifiInfo);
        assertTrue(wifiInfo.isOsuAp());
        assertEquals(WifiConfigurationTestUtil.TEST_PROVIDER_FRIENDLY_NAME,
                wifiInfo.getPasspointProviderFriendlyName());
    }

    /**
     * Tests that OSU fields in WifiInfo are reset when connecting to a non-OSU network during
     * DisconnectedState.
     * @throws Exception
     */
    @Test
    public void testResetWifiInfoOsuFields() throws Exception {
        loadComponentsInStaMode();
        WifiConfiguration osuConfig = spy(WifiConfigurationTestUtil.createEphemeralNetwork());
        osuConfig.SSID = sWifiSsid.toString();
        osuConfig.BSSID = sBSSID;
        osuConfig.osu = true;
        osuConfig.networkId = PASSPOINT_NETWORK_ID;
        osuConfig.providerFriendlyName = WifiConfigurationTestUtil.TEST_PROVIDER_FRIENDLY_NAME;
        osuConfig.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_PERSISTENT;
        setupAndStartConnectSequence(osuConfig);
        validateSuccessfulConnectSequence(osuConfig);

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(PASSPOINT_NETWORK_ID, sWifiSsid, sBSSID,
                        SupplicantState.ASSOCIATING));
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(FRAMEWORK_NETWORK_ID, sWifiSsid, sBSSID,
                        SupplicantState.ASSOCIATING));
        mLooper.dispatchAll();

        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertNotNull(wifiInfo);
        assertFalse(wifiInfo.isOsuAp());
    }

    /**
     * Verify that WifiStateTracker is called if wifi is disabled while connected.
     */
    @Test
    public void verifyWifiStateTrackerUpdatedWhenDisabled() throws Exception {
        connect();

        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();
        verify(mWifiStateTracker).updateState(eq(WifiStateTracker.DISCONNECTED));
    }

    /**
     * Tests the network connection initiation sequence with no network request pending from
     * from WifiNetworkFactory when we're already connected to a different network.
     * This simulates the connect sequence using the public
     * {@link WifiManager#enableNetwork(int, boolean)} and ensures that we invoke
     * {@link WifiNative#connectToNetwork(WifiConfiguration)}.
     */
    @Test
    public void triggerConnectWithNoNetworkRequestAndAlreadyConnected() throws Exception {
        // Simulate the first connection.
        connect();

        // Remove the network requests.
        when(mWifiNetworkFactory.hasConnectionRequests()).thenReturn(false);
        when(mUntrustedWifiNetworkFactory.hasConnectionRequests()).thenReturn(false);

        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        config.networkId = FRAMEWORK_NETWORK_ID + 1;
        setupAndStartConnectSequence(config);
        validateSuccessfulConnectSequence(config);
        verify(mWifiPermissionsUtil, atLeastOnce()).checkNetworkSettingsPermission(anyInt());
    }

    /**
     * Tests the network connection initiation sequence from a non-privileged app with no network
     * request pending from from WifiNetworkFactory when we're already connected to a different
     * network.
     * This simulates the connect sequence using the public
     * {@link WifiManager#enableNetwork(int, boolean)} and ensures that we don't invoke
     * {@link WifiNative#connectToNetwork(WifiConfiguration)}.
     */
    @Test
    public void triggerConnectWithNoNetworkRequestAndAlreadyConnectedButNonPrivilegedApp()
            throws Exception {
        // Simulate the first connection.
        connect();

        // Remove the network requests.
        when(mWifiNetworkFactory.hasConnectionRequests()).thenReturn(false);
        when(mUntrustedWifiNetworkFactory.hasConnectionRequests()).thenReturn(false);

        when(mWifiPermissionsUtil.checkNetworkSettingsPermission(anyInt())).thenReturn(false);

        WifiConfiguration config = WifiConfigurationTestUtil.createOpenNetwork();
        config.networkId = FRAMEWORK_NETWORK_ID + 1;
        setupAndStartConnectSequence(config);
        verify(mWifiConfigManager).enableNetwork(
                eq(config.networkId), eq(true), anyInt(), any());
        verify(mWifiConnectivityManager, never()).setUserConnectChoice(eq(config.networkId));
        verify(mWifiConnectivityManager).prepareForForcedConnection(eq(config.networkId));
        verify(mWifiConfigManager, never())
                .getConfiguredNetworkWithoutMasking(eq(config.networkId));
        verify(mWifiNative, never()).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
        verify(mWifiPermissionsUtil, times(4)).checkNetworkSettingsPermission(anyInt());
    }

    @Test
    public void enableWithInvalidNetworkId() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        when(mWifiConfigManager.getConfiguredNetwork(eq(0))).thenReturn(null);

        verify(mWifiNative).removeAllNetworks(WIFI_IFACE_NAME);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onFailure(anyInt());

        verify(mWifiConfigManager, never()).enableNetwork(eq(0), eq(true), anyInt(), any());
        verify(mWifiConfigManager, never()).updateLastConnectUid(eq(0), anyInt());
    }

    /**
     * If caller tries to connect to a network that is already connected, the connection request
     * should succeed.
     *
     * Test: Create and connect to a network, then try to reconnect to the same network. Verify
     * that connection request returns with CONNECT_NETWORK_SUCCEEDED.
     */
    @Test
    public void reconnectToConnectedNetworkWithNetworkId() throws Exception {
        connect();

        // try to reconnect
        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, FRAMEWORK_NETWORK_ID, mock(Binder.class), connectActionListener, 0,
                Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        // Verify that we didn't trigger a second connection.
        verify(mWifiNative, times(1)).connectToNetwork(eq(WIFI_IFACE_NAME), any());
    }

    /**
     * If caller tries to connect to a network that is already connected, the connection request
     * should succeed.
     *
     * Test: Create and connect to a network, then try to reconnect to the same network. Verify
     * that connection request returns with CONNECT_NETWORK_SUCCEEDED.
     */
    @Test
    public void reconnectToConnectedNetworkWithConfig() throws Exception {
        connect();

        // try to reconnect
        WifiConfiguration config = new WifiConfiguration();
        config.networkId = FRAMEWORK_NETWORK_ID;
        when(mWifiConfigManager.addOrUpdateNetwork(eq(config), anyInt()))
                .thenReturn(new NetworkUpdateResult(FRAMEWORK_NETWORK_ID));
        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(config, WifiConfiguration.INVALID_NETWORK_ID, mock(Binder.class),
                connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        // Verify that we didn't trigger a second connection.
        verify(mWifiNative, times(1)).connectToNetwork(eq(WIFI_IFACE_NAME), any());
    }

    /**
     * If caller tries to connect to a network that is already connecting, the connection request
     * should succeed.
     *
     * Test: Create and trigger connect to a network, then try to reconnect to the same network.
     * Verify that connection request returns with CONNECT_NETWORK_SUCCEEDED and did not trigger a
     * new connection.
     */
    @Test
    public void reconnectToConnectingNetwork() throws Exception {
        triggerConnect();

        // try to reconnect to the same network (before connection is established).
        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, FRAMEWORK_NETWORK_ID, mock(Binder.class), connectActionListener, 0,
                Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        // Verify that we didn't trigger a second connection.
        verify(mWifiNative, times(1)).connectToNetwork(eq(WIFI_IFACE_NAME), any());
    }

    /**
     * If caller tries to connect to a network that is already connecting, the connection request
     * should succeed.
     *
     * Test: Create and trigger connect to a network, then try to reconnect to the same network.
     * Verify that connection request returns with CONNECT_NETWORK_SUCCEEDED and did trigger a new
     * connection.
     */
    @Test
    public void reconnectToConnectingNetworkWithCredentialChange() throws Exception {
        triggerConnect();

        // try to reconnect to the same network with a credential changed (before connection is
        // established).
        WifiConfiguration config = new WifiConfiguration();
        config.networkId = FRAMEWORK_NETWORK_ID;
        NetworkUpdateResult networkUpdateResult =
                new NetworkUpdateResult(false /* ip */, false /* proxy */, true /* credential */);
        networkUpdateResult.setNetworkId(FRAMEWORK_NETWORK_ID);
        when(mWifiConfigManager.addOrUpdateNetwork(eq(config), anyInt()))
                .thenReturn(networkUpdateResult);
        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(config, WifiConfiguration.INVALID_NETWORK_ID, mock(Binder.class),
                connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        // Verify that we triggered a second connection.
        verify(mWifiNative, times(2)).connectToNetwork(eq(WIFI_IFACE_NAME), any());
    }

    /**
     * If caller tries to connect to a network that previously failed connection, the connection
     * request should succeed.
     *
     * Test: Create and trigger connect to a network, then fail the connection. Now try to reconnect
     * to the same network. Verify that connection request returns with CONNECT_NETWORK_SUCCEEDED
     * and did trigger a new * connection.
     */
    @Test
    public void connectAfterAssociationRejection() throws Exception {
        triggerConnect();

        // fail the connection.
        mCmi.sendMessage(WifiMonitor.ASSOCIATION_REJECTION_EVENT, 0,
                ISupplicantStaIfaceCallback.StatusCode.AP_UNABLE_TO_HANDLE_NEW_STA, sBSSID);
        mLooper.dispatchAll();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, FRAMEWORK_NETWORK_ID, mock(Binder.class), connectActionListener, 0,
                Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        // Verify that we triggered a second connection.
        verify(mWifiNative, times(2)).connectToNetwork(eq(WIFI_IFACE_NAME), any());
    }

    /**
     * If caller tries to connect to a network that previously failed connection, the connection
     * request should succeed.
     *
     * Test: Create and trigger connect to a network, then fail the connection. Now try to reconnect
     * to the same network. Verify that connection request returns with CONNECT_NETWORK_SUCCEEDED
     * and did trigger a new * connection.
     */
    @Test
    public void connectAfterConnectionFailure() throws Exception {
        triggerConnect();

        // fail the connection.
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, FRAMEWORK_NETWORK_ID, 0, sBSSID);
        mLooper.dispatchAll();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, FRAMEWORK_NETWORK_ID, mock(Binder.class), connectActionListener, 0,
                Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        // Verify that we triggered a second connection.
        verify(mWifiNative, times(2)).connectToNetwork(eq(WIFI_IFACE_NAME), any());
    }

    /**
     * If caller tries to connect to a new network while still provisioning the current one,
     * the connection attempt should succeed.
     */
    @Test
    public void connectWhileObtainingIp() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        verify(mWifiNative).removeAllNetworks(WIFI_IFACE_NAME);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());
        reset(mWifiNative);

        // Connect to a different network
        WifiConfiguration config = new WifiConfiguration();
        config.networkId = TEST_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_NETWORK_ID)).thenReturn(config);

        mCmi.connect(null, TEST_NETWORK_ID, mock(Binder.class), null, 0, Binder.getCallingUid());
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.DISCONNECTED));
        mLooper.dispatchAll();

        verify(mWifiConnectivityManager).prepareForForcedConnection(eq(config.networkId));

    }

    /**
     * Tests that manual connection to a network (from settings app) logs the correct nominator ID.
     */
    @Test
    public void testManualConnectNominator() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        WifiConfiguration config = new WifiConfiguration();
        config.networkId = TEST_NETWORK_ID;
        when(mWifiConfigManager.getConfiguredNetwork(TEST_NETWORK_ID)).thenReturn(config);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, TEST_NETWORK_ID, mock(Binder.class), connectActionListener, 0,
                Process.SYSTEM_UID);
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiMetrics).setNominatorForNetwork(TEST_NETWORK_ID,
                WifiMetricsProto.ConnectionEvent.NOMINATOR_MANUAL);
    }

    @Test
    public void testDhcpFailure() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.ASSOCIATED));
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        verify(mBssidBlocklistMonitor).handleBssidConnectionSuccess(sBSSID, sSSID);

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());
        injectDhcpFailure();
        mLooper.dispatchAll();

        assertEquals("DisconnectingState", getCurrentState().getName());
        // Verify this is not counted as a IP renewal failure
        verify(mWifiMetrics, never()).incrementIpRenewalFailure();
        // Verifies that WifiLastResortWatchdog be notified
        // by DHCP failure
        verify(mWifiLastResortWatchdog, times(2)).noteConnectionFailureAndTriggerIfNeeded(
                sSSID, sBSSID, WifiLastResortWatchdog.FAILURE_CODE_DHCP);
        verify(mBssidBlocklistMonitor, times(2)).handleBssidConnectionFailure(eq(sBSSID),
                eq(sSSID), eq(BssidBlocklistMonitor.REASON_DHCP_FAILURE), anyInt());
        verify(mBssidBlocklistMonitor, times(2)).handleBssidConnectionFailure(eq(sBSSID), eq(sSSID),
                eq(BssidBlocklistMonitor.REASON_DHCP_FAILURE), anyInt());
        verify(mBssidBlocklistMonitor, never()).handleDhcpProvisioningSuccess(sBSSID, sSSID);
        verify(mBssidBlocklistMonitor, never()).handleNetworkValidationSuccess(sBSSID, sSSID);
    }

    /**
     * Verify that a IP renewal failure is logged when IP provisioning fail in the
     * ConnectedState.
     */
    @Test
    public void testDhcpRenewalMetrics() throws Exception {
        connect();
        injectDhcpFailure();
        mLooper.dispatchAll();

        verify(mWifiMetrics).incrementIpRenewalFailure();
    }

    /**
     * Verify that the network selection status will be updated with DISABLED_AUTHENTICATION_FAILURE
     * when wrong password authentication failure is detected and the network had been
     * connected previously.
     */
    @Test
    public void testWrongPasswordWithPreviouslyConnected() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        WifiConfiguration config = new WifiConfiguration();
        config.getNetworkSelectionStatus().setHasEverConnected(true);
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(config);

        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD);
        mLooper.dispatchAll();

        verify(mWrongPasswordNotifier, never()).onWrongPasswordError(anyString());
        verify(mWifiConfigManager).updateNetworkSelectionStatus(anyInt(),
                eq(WifiConfiguration.NetworkSelectionStatus.DISABLED_AUTHENTICATION_FAILURE));

        assertEquals("DisconnectedState", getCurrentState().getName());
    }

    /**
     * Verify that the network selection status will be updated with DISABLED_BY_WRONG_PASSWORD
     * when wrong password authentication failure is detected and the network has never been
     * connected.
     */
    @Test
    public void testWrongPasswordWithNeverConnected() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        WifiConfiguration config = new WifiConfiguration();
        config.SSID = sSSID;
        config.getNetworkSelectionStatus().setHasEverConnected(false);
        config.carrierId = CARRIER_ID_1;
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(config);

        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD);
        mLooper.dispatchAll();

        verify(mWrongPasswordNotifier).onWrongPasswordError(eq(sSSID));
        verify(mWifiConfigManager).updateNetworkSelectionStatus(anyInt(),
                eq(WifiConfiguration.NetworkSelectionStatus.DISABLED_BY_WRONG_PASSWORD));
        verify(mWifiMetrics).incrementNumOfCarrierWifiConnectionAuthFailure();
        assertEquals("DisconnectedState", getCurrentState().getName());
    }

    /**
     * Verify that the network selection status will be updated with DISABLED_BY_WRONG_PASSWORD
     * when wrong password authentication failure is detected and the network is unknown.
     */
    @Test
    public void testWrongPasswordWithNullNetwork() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(null);

        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD);
        mLooper.dispatchAll();

        verify(mWifiConfigManager).updateNetworkSelectionStatus(anyInt(),
                eq(WifiConfiguration.NetworkSelectionStatus.DISABLED_BY_WRONG_PASSWORD));
        assertEquals("DisconnectedState", getCurrentState().getName());
    }

    /**
     * Verify that the function resetCarrierKeysForImsiEncryption() in TelephonyManager
     * is called when a Authentication failure is detected with a vendor specific EAP Error
     * of certification expired while using EAP-SIM
     * In this test case, it is assumed that the network had been connected previously.
     */
    @Test
    public void testEapSimErrorVendorSpecific() throws Exception {
        when(mWifiMetrics.startConnectionEvent(any(), anyString(), anyInt())).thenReturn(80000);
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        WifiConfiguration config = new WifiConfiguration();
        config.SSID = sSSID;
        config.getNetworkSelectionStatus().setHasEverConnected(true);
        config.allowedKeyManagement.set(KeyMgmt.IEEE8021X);
        config.enterpriseConfig.setEapMethod(WifiEnterpriseConfig.Eap.SIM);
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(config);
        MockitoSession mockSession = ExtendedMockito.mockitoSession()
                .mockStatic(SubscriptionManager.class)
                .startMocking();
        when(SubscriptionManager.getDefaultDataSubscriptionId()).thenReturn(DATA_SUBID);
        when(SubscriptionManager.isValidSubscriptionId(anyInt())).thenReturn(true);
        when(mWifiScoreCard.detectAbnormalConnectionFailure(anyString()))
                .thenReturn(WifiHealthMonitor.REASON_AUTH_FAILURE);

        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_EAP_FAILURE,
                WifiNative.EAP_SIM_VENDOR_SPECIFIC_CERT_EXPIRED);
        mLooper.dispatchAll();

        verify(mEapFailureNotifier).onEapFailure(
                WifiNative.EAP_SIM_VENDOR_SPECIFIC_CERT_EXPIRED, config);
        verify(mDataTelephonyManager).resetCarrierKeysForImsiEncryption();
        mockSession.finishMocking();
        verify(mDeviceConfigFacade).isAbnormalConnectionFailureBugreportEnabled();
        verify(mWifiScoreCard).detectAbnormalConnectionFailure(anyString());
        verify(mWifiDiagnostics, times(2)).takeBugReport(anyString(), anyString());
    }

    /**
     * Verify that the function resetCarrierKeysForImsiEncryption() in TelephonyManager
     * is not called when a Authentication failure is detected with a vendor specific EAP Error
     * of certification expired while using other methods than EAP-SIM, EAP-AKA, or EAP-AKA'.
     */
    @Test
    public void testEapTlsErrorVendorSpecific() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        WifiConfiguration config = new WifiConfiguration();
        config.getNetworkSelectionStatus().setHasEverConnected(true);
        config.enterpriseConfig.setEapMethod(WifiEnterpriseConfig.Eap.TLS);
        config.allowedKeyManagement.set(KeyMgmt.IEEE8021X);
        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(config);

        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_EAP_FAILURE,
                WifiNative.EAP_SIM_VENDOR_SPECIFIC_CERT_EXPIRED);
        mLooper.dispatchAll();

        verify(mDataTelephonyManager, never()).resetCarrierKeysForImsiEncryption();
    }

    /**
     * Verify that the network selection status will be updated with
     * DISABLED_AUTHENTICATION_NO_SUBSCRIBED when service is not subscribed.
     */
    @Test
    public void testEapSimNoSubscribedError() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        when(mWifiConfigManager.getConfiguredNetwork(anyInt())).thenReturn(null);

        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_EAP_FAILURE,
                WifiNative.EAP_SIM_NOT_SUBSCRIBED);
        mLooper.dispatchAll();

        verify(mWifiConfigManager).updateNetworkSelectionStatus(anyInt(),
                eq(WifiConfiguration.NetworkSelectionStatus
                        .DISABLED_AUTHENTICATION_NO_SUBSCRIPTION));
    }

    @Test
    public void testBadNetworkEvent() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("DisconnectedState", getCurrentState().getName());
        verify(mWifiDiagnostics, never()).takeBugReport(anyString(), anyString());
    }


    @Test
    public void getWhatToString() throws Exception {
        assertEquals("CMD_CHANNEL_HALF_CONNECTED", mCmi.getWhatToString(
                AsyncChannel.CMD_CHANNEL_HALF_CONNECTED));
        assertEquals("CMD_PRE_DHCP_ACTION", mCmi.getWhatToString(CMD_PRE_DHCP_ACTION));
        assertEquals("CMD_IP_REACHABILITY_LOST", mCmi.getWhatToString(
                ClientModeImpl.CMD_IP_REACHABILITY_LOST));
    }

    @Test
    public void disconnect() throws Exception {
        when(mWifiScoreCard.detectAbnormalDisconnection())
                .thenReturn(WifiHealthMonitor.REASON_SHORT_CONNECTION_NONLOCAL);
        InOrder inOrderWifiLockManager = inOrder(mWifiLockManager);
        connect();
        inOrderWifiLockManager.verify(mWifiLockManager).updateWifiClientConnected(true);

        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, -1, 3, sBSSID);
        mLooper.dispatchAll();
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.DISCONNECTED));
        mLooper.dispatchAll();

        verify(mWifiStateTracker).updateState(eq(WifiStateTracker.DISCONNECTED));
        verify(mWifiNetworkSuggestionsManager).handleDisconnect(any(), any());
        assertEquals("DisconnectedState", getCurrentState().getName());
        inOrderWifiLockManager.verify(mWifiLockManager).updateWifiClientConnected(false);
        verify(mWifiScoreCard).detectAbnormalDisconnection();
        verify(mWifiDiagnostics).takeBugReport(anyString(), anyString());
    }

    /**
     * Successfully connecting to a network will set WifiConfiguration's value of HasEverConnected
     * to true.
     *
     * Test: Successfully create and connect to a network. Check the config and verify
     * WifiConfiguration.getHasEverConnected() is true.
     */
    @Test
    public void setHasEverConnectedTrueOnConnect() throws Exception {
        connect();
        verify(mWifiConfigManager, atLeastOnce()).updateNetworkAfterConnect(0);
    }

    /**
     * Fail network connection attempt and verify HasEverConnected remains false.
     *
     * Test: Successfully create a network but fail when connecting. Check the config and verify
     * WifiConfiguration.getHasEverConnected() is false.
     */
    @Test
    public void connectionFailureDoesNotSetHasEverConnectedTrue() throws Exception {
        testDhcpFailure();
        verify(mWifiConfigManager, never()).updateNetworkAfterConnect(0);
    }

    @Test
    public void iconQueryTest() throws Exception {
        // TODO(b/31065385): Passpoint config management.
    }

    @Test
    public void verboseLogRecSizeIsGreaterThanNormalSize() {
        assertTrue(LOG_REC_LIMIT_IN_VERBOSE_MODE > ClientModeImpl.NUM_LOG_RECS_NORMAL);
    }

    /**
     * Verifies that, by default, we allow only the "normal" number of log records.
     */
    @Test
    public void normalLogRecSizeIsUsedByDefault() {
        assertEquals(ClientModeImpl.NUM_LOG_RECS_NORMAL, mCmi.getLogRecMaxSize());
    }

    /**
     * Verifies that, in verbose mode, we allow a larger number of log records.
     */
    @Test
    public void enablingVerboseLoggingUpdatesLogRecSize() {
        mCmi.enableVerboseLogging(1);
        assertEquals(LOG_REC_LIMIT_IN_VERBOSE_MODE, mCmi.getLogRecMaxSize());
    }

    @Test
    public void disablingVerboseLoggingClearsRecords() {
        mCmi.sendMessage(ClientModeImpl.CMD_DISCONNECT);
        mLooper.dispatchAll();
        assertTrue(mCmi.getLogRecSize() >= 1);

        mCmi.enableVerboseLogging(0);
        assertEquals(0, mCmi.getLogRecSize());
    }

    @Test
    public void disablingVerboseLoggingUpdatesLogRecSize() {
        mCmi.enableVerboseLogging(1);
        mCmi.enableVerboseLogging(0);
        assertEquals(ClientModeImpl.NUM_LOG_RECS_NORMAL, mCmi.getLogRecMaxSize());
    }

    @Test
    public void logRecsIncludeDisconnectCommand() {
        // There's nothing special about the DISCONNECT command. It's just representative of
        // "normal" commands.
        mCmi.sendMessage(ClientModeImpl.CMD_DISCONNECT);
        mLooper.dispatchAll();
        assertEquals(1, mCmi.copyLogRecs()
                .stream()
                .filter(logRec -> logRec.getWhat() == ClientModeImpl.CMD_DISCONNECT)
                .count());
    }

    @Test
    public void logRecsExcludeRssiPollCommandByDefault() {
        mCmi.sendMessage(ClientModeImpl.CMD_RSSI_POLL);
        mLooper.dispatchAll();
        assertEquals(0, mCmi.copyLogRecs()
                .stream()
                .filter(logRec -> logRec.getWhat() == ClientModeImpl.CMD_RSSI_POLL)
                .count());
    }

    @Test
    public void logRecsIncludeRssiPollCommandWhenVerboseLoggingIsEnabled() {
        mCmi.enableVerboseLogging(1);
        mCmi.sendMessage(ClientModeImpl.CMD_RSSI_POLL);
        mLooper.dispatchAll();
        assertEquals(1, mCmi.copyLogRecs()
                .stream()
                .filter(logRec -> logRec.getWhat() == ClientModeImpl.CMD_RSSI_POLL)
                .count());
    }

    /**
     * Verify that syncStartSubscriptionProvisioning will redirect calls with right parameters
     * to {@link PasspointManager} with expected true being returned when in client mode.
     */
    @Test
    public void syncStartSubscriptionProvisioningInClientMode() throws Exception {
        loadComponentsInStaMode();
        when(mPasspointManager.startSubscriptionProvisioning(anyInt(),
                any(OsuProvider.class), any(IProvisioningCallback.class))).thenReturn(true);
        mLooper.startAutoDispatch();
        assertTrue(mCmi.syncStartSubscriptionProvisioning(
                OTHER_USER_UID, mOsuProvider, mProvisioningCallback, mCmiAsyncChannel));
        verify(mPasspointManager).startSubscriptionProvisioning(OTHER_USER_UID, mOsuProvider,
                mProvisioningCallback);
        mLooper.stopAutoDispatch();
    }

    /**
     * Verify that syncStartSubscriptionProvisioning will be a no-op and return false before
     * SUPPLICANT_START command is received by the CMI.
     */
    @Test
    public void syncStartSubscriptionProvisioningBeforeSupplicantOrAPStart() throws Exception {
        mLooper.startAutoDispatch();
        assertFalse(mCmi.syncStartSubscriptionProvisioning(
                OTHER_USER_UID, mOsuProvider, mProvisioningCallback, mCmiAsyncChannel));
        mLooper.stopAutoDispatch();
        verify(mPasspointManager, never()).startSubscriptionProvisioning(
                anyInt(), any(OsuProvider.class), any(IProvisioningCallback.class));
    }

    /**
     * Verify that syncStartSubscriptionProvisioning will be a no-op and return false when not in
     * client mode.
     */
    @Test
    public void syncStartSubscriptionProvisioningNoOpWifiDisabled() throws Exception {
        mLooper.startAutoDispatch();
        assertFalse(mCmi.syncStartSubscriptionProvisioning(
                OTHER_USER_UID, mOsuProvider, mProvisioningCallback, mCmiAsyncChannel));
        mLooper.stopAutoDispatch();
        verify(mPasspointManager, never()).startSubscriptionProvisioning(
                anyInt(), any(OsuProvider.class), any(IProvisioningCallback.class));
    }

    /**
     *  Test that we disconnect from a network if it was removed while we are in the
     *  ObtainingIpState.
     */
    @Test
    public void disconnectFromNetworkWhenRemovedWhileObtainingIpAddr() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        when(mWifiConfigManager.enableNetwork(eq(0), eq(true), anyInt(), any()))
                .thenReturn(true);
        when(mWifiConfigManager.updateLastConnectUid(eq(0), anyInt())).thenReturn(true);

        verify(mWifiNative).removeAllNetworks(WIFI_IFACE_NAME);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());
        verify(mWifiConnectivityManager).setUserConnectChoice(eq(0));
        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);

        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());

        // now remove the config
        reset(connectActionListener);
        when(mWifiConfigManager.removeNetwork(eq(FRAMEWORK_NETWORK_ID), anyInt(), any()))
                .thenReturn(true);
        mCmi.forget(FRAMEWORK_NETWORK_ID, mock(Binder.class), connectActionListener, 0,
                Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();
        verify(mWifiConfigManager).removeNetwork(eq(FRAMEWORK_NETWORK_ID), anyInt(), any());
        // trigger removal callback to trigger disconnect.
        WifiConfiguration removedConfig = new WifiConfiguration();
        removedConfig.networkId = FRAMEWORK_NETWORK_ID;
        mConfigUpdateListenerCaptor.getValue().onNetworkRemoved(removedConfig);

        reset(mWifiConfigManager);

        when(mWifiConfigManager.getConfiguredNetwork(FRAMEWORK_NETWORK_ID)).thenReturn(null);

        DhcpResultsParcelable dhcpResults = new DhcpResultsParcelable();
        dhcpResults.baseConfiguration = new StaticIpConfiguration();
        dhcpResults.baseConfiguration.gateway = InetAddresses.parseNumericAddress("1.2.3.4");
        dhcpResults.baseConfiguration.ipAddress =
                new LinkAddress(InetAddresses.parseNumericAddress("192.168.1.100"), 0);
        dhcpResults.baseConfiguration.dnsServers.add(InetAddresses.parseNumericAddress("8.8.8.8"));
        dhcpResults.leaseDuration = 3600;

        injectDhcpSuccess(dhcpResults);
        mLooper.dispatchAll();

        assertEquals("DisconnectingState", getCurrentState().getName());
    }

    /**
     * Test verifying that interface Supplicant update for inactive driver does not trigger
     * SelfRecovery when WifiNative reports the interface is up.
     */
    @Test
    public void testSupplicantUpdateDriverInactiveIfaceUpClientModeDoesNotTriggerSelfRecovery()
            throws Exception {
        // Trigger initialize to capture the death handler registration.
        loadComponentsInStaMode();

        when(mWifiNative.isInterfaceUp(eq(WIFI_IFACE_NAME))).thenReturn(true);

        // make sure supplicant has been reported as inactive
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, WifiSsid.createFromAsciiEncoded(""), null,
                        SupplicantState.INTERFACE_DISABLED));
        mLooper.dispatchAll();

        // CMI should trigger self recovery, but not disconnect until externally triggered
        verify(mSelfRecovery, never()).trigger(eq(SelfRecovery.REASON_STA_IFACE_DOWN));
    }

    /**
     * Verifies that WifiInfo is updated upon SUPPLICANT_STATE_CHANGE_EVENT.
     */
    @Test
    public void testWifiInfoUpdatedUponSupplicantStateChangedEvent() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|.
        connect();

        // Set the scan detail cache for roaming target.
        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);
        when(mScanDetailCache.getScanDetail(sBSSID1)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID1, sFreq1));
        when(mScanDetailCache.getScanResult(sBSSID1)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID1, sFreq1).getScanResult());

        // This simulates the behavior of roaming to network with |sBSSID1|, |sFreq1|.
        // Send a SUPPLICANT_STATE_CHANGE_EVENT, verify WifiInfo is updated.
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID1, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertEquals(sBSSID1, wifiInfo.getBSSID());
        assertEquals(sFreq1, wifiInfo.getFrequency());
        assertEquals(SupplicantState.COMPLETED, wifiInfo.getSupplicantState());

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID1, SupplicantState.DISCONNECTED));
        mLooper.dispatchAll();

        wifiInfo = mCmi.getWifiInfo();
        assertNull(wifiInfo.getBSSID());
        assertEquals(WifiManager.UNKNOWN_SSID, wifiInfo.getSSID());
        assertEquals(WifiConfiguration.INVALID_NETWORK_ID, wifiInfo.getNetworkId());
        assertEquals(SupplicantState.DISCONNECTED, wifiInfo.getSupplicantState());
    }

    /**
     * Verifies that WifiInfo is updated upon CMD_ASSOCIATED_BSSID event.
     */
    @Test
    public void testWifiInfoUpdatedUponAssociatedBSSIDEvent() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|.
        connect();

        // Set the scan detail cache for roaming target.
        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);
        when(mScanDetailCache.getScanDetail(sBSSID1)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID1, sFreq1));
        when(mScanDetailCache.getScanResult(sBSSID1)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID1, sFreq1).getScanResult());

        // This simulates the behavior of roaming to network with |sBSSID1|, |sFreq1|.
        // Send a CMD_ASSOCIATED_BSSID, verify WifiInfo is updated.
        mCmi.sendMessage(WifiMonitor.ASSOCIATED_BSSID_EVENT, 0, 0, sBSSID1);
        mLooper.dispatchAll();

        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertEquals(sBSSID1, wifiInfo.getBSSID());
        assertEquals(sFreq1, wifiInfo.getFrequency());
        assertEquals(SupplicantState.COMPLETED, wifiInfo.getSupplicantState());
    }

    /**
     * Verifies that WifiInfo is cleared upon exiting and entering WifiInfo, and that it is not
     * updated by SUPPLICAN_STATE_CHANGE_EVENTs in ScanModeState.
     * This protects ClientModeImpl from  getting into a bad state where WifiInfo says wifi is
     * already Connected or Connecting, (when it is in-fact Disconnected), so
     * WifiConnectivityManager does not attempt any new Connections, freezing wifi.
     */
    @Test
    public void testWifiInfoCleanedUpEnteringExitingConnectModeState() throws Exception {
        InOrder inOrder = inOrder(mWifiConnectivityManager, mWifiNetworkFactory);
        InOrder inOrderSarMgr = inOrder(mSarManager);
        InOrder inOrderMetrics = inOrder(mWifiMetrics);
        Log.i(TAG, mCmi.getCurrentState().getName());
        String initialBSSID = "aa:bb:cc:dd:ee:ff";
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        wifiInfo.setBSSID(initialBSSID);

        // Set CMI to CONNECT_MODE and verify state, and wifi enabled in ConnectivityManager
        startSupplicantAndDispatchMessages();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());
        inOrder.verify(mWifiConnectivityManager).setWifiEnabled(eq(true));
        inOrder.verify(mWifiNetworkFactory).setWifiState(eq(true));
        inOrderMetrics.verify(mWifiMetrics)
                .setWifiState(WifiMetricsProto.WifiLog.WIFI_DISCONNECTED);
        inOrderMetrics.verify(mWifiMetrics).logStaEvent(StaEvent.TYPE_WIFI_ENABLED);
        assertNull(wifiInfo.getBSSID());

        // Send a SUPPLICANT_STATE_CHANGE_EVENT, verify WifiInfo is updated
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();
        assertEquals(sBSSID, wifiInfo.getBSSID());
        assertEquals(SupplicantState.COMPLETED, wifiInfo.getSupplicantState());

        // Set CMI to DISABLED_MODE, verify state and wifi disabled in ConnectivityManager, and
        // WifiInfo is reset() and state set to DISCONNECTED
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_DISABLED);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();

        assertEquals(ClientModeImpl.DISABLED_MODE, mCmi.getOperationalModeForTest());
        assertEquals("DefaultState", getCurrentState().getName());
        assertEquals(WifiManager.WIFI_STATE_DISABLED, mCmi.syncGetWifiState());
        inOrder.verify(mWifiConnectivityManager).setWifiEnabled(eq(false));
        inOrder.verify(mWifiNetworkFactory).setWifiState(eq(false));
        inOrderMetrics.verify(mWifiMetrics).setWifiState(WifiMetricsProto.WifiLog.WIFI_DISABLED);
        inOrderMetrics.verify(mWifiMetrics).logStaEvent(StaEvent.TYPE_WIFI_DISABLED);
        assertNull(wifiInfo.getBSSID());
        assertEquals(SupplicantState.DISCONNECTED, wifiInfo.getSupplicantState());
        verify(mPasspointManager).clearAnqpRequestsAndFlushCache();

        // Send a SUPPLICANT_STATE_CHANGE_EVENT, verify WifiInfo is not updated
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();
        assertNull(wifiInfo.getBSSID());
        assertEquals(SupplicantState.DISCONNECTED, wifiInfo.getSupplicantState());

        // Set the bssid to something, so we can verify it is cleared (just in case)
        wifiInfo.setBSSID(initialBSSID);

        // Set CMI to CONNECT_MODE and verify state, and wifi enabled in ConnectivityManager,
        // and WifiInfo has been reset
        startSupplicantAndDispatchMessages();

        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());
        inOrder.verify(mWifiConnectivityManager).setWifiEnabled(eq(true));
        inOrder.verify(mWifiNetworkFactory).setWifiState(eq(true));
        inOrderMetrics.verify(mWifiMetrics)
                .setWifiState(WifiMetricsProto.WifiLog.WIFI_DISCONNECTED);
        inOrderMetrics.verify(mWifiMetrics).logStaEvent(StaEvent.TYPE_WIFI_ENABLED);
        assertEquals("DisconnectedState", getCurrentState().getName());
        assertEquals(SupplicantState.DISCONNECTED, wifiInfo.getSupplicantState());
        assertNull(wifiInfo.getBSSID());
    }

    /**
     * Test that connected SSID and BSSID are exposed to system server.
     * Also tests that {@link ClientModeImpl#syncRequestConnectionInfo()} always
     * returns a copy of WifiInfo.
     */
    @Test
    public void testConnectedIdsAreVisibleFromSystemServer() throws Exception {
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        // Get into a connected state, with known BSSID and SSID
        connect();
        assertEquals(sBSSID, wifiInfo.getBSSID());
        assertEquals(sWifiSsid, wifiInfo.getWifiSsid());

        WifiInfo connectionInfo = mCmi.syncRequestConnectionInfo();

        assertNotEquals(wifiInfo, connectionInfo);
        assertEquals(wifiInfo.getSSID(), connectionInfo.getSSID());
        assertEquals(wifiInfo.getBSSID(), connectionInfo.getBSSID());
        assertEquals(wifiInfo.getMacAddress(), connectionInfo.getMacAddress());
    }

    /**
     * Test that reconnectCommand() triggers connectivity scan when ClientModeImpl
     * is in DisconnectedMode.
     */
    @Test
    public void testReconnectCommandWhenDisconnected() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|, and then disconnect.
        disconnect();

        mCmi.reconnectCommand(ClientModeImpl.WIFI_WORK_SOURCE);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager).forceConnectivityScan(ClientModeImpl.WIFI_WORK_SOURCE);
    }

    /**
     * Test that reconnectCommand() doesn't trigger connectivity scan when ClientModeImpl
     * is in ConnectedMode.
     */
    @Test
    public void testReconnectCommandWhenConnected() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|.
        connect();

        mCmi.reconnectCommand(ClientModeImpl.WIFI_WORK_SOURCE);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, never())
                .forceConnectivityScan(ClientModeImpl.WIFI_WORK_SOURCE);
    }

    /**
     * Adds the network without putting ClientModeImpl into ConnectMode.
     */
    @Test
    public void addNetworkInDefaultState() throws Exception {
        // We should not be in initial state now.
        assertTrue("DefaultState".equals(getCurrentState().getName()));
        initializeMocksForAddedNetwork(false);
        verify(mWifiConnectivityManager, never()).setUserConnectChoice(eq(0));
    }

    /**
     * Verifies that ClientModeImpl sets and unsets appropriate 'RecentFailureReason' values
     * on a WifiConfiguration when it fails association, authentication, or successfully connects
     */
    @Test
    public void testExtraFailureReason_ApIsBusy() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        // Trigger a connection to this (CMD_START_CONNECT will actually fail, but it sets up
        // targetNetworkId state)
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        // Simulate an ASSOCIATION_REJECTION_EVENT, due to the AP being busy
        mCmi.sendMessage(WifiMonitor.ASSOCIATION_REJECTION_EVENT, 0,
                ISupplicantStaIfaceCallback.StatusCode.AP_UNABLE_TO_HANDLE_NEW_STA, sBSSID);
        mLooper.dispatchAll();
        verify(mWifiConfigManager).setRecentFailureAssociationStatus(eq(0),
                eq(WifiConfiguration.RECENT_FAILURE_AP_UNABLE_TO_HANDLE_NEW_STA));
        assertEquals("DisconnectedState", getCurrentState().getName());

        // Simulate an AUTHENTICATION_FAILURE_EVENT, which should clear the ExtraFailureReason
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT, 0, 0, null);
        mLooper.dispatchAll();
        verify(mWifiConfigManager, times(1)).clearRecentFailureReason(eq(0));
        verify(mWifiConfigManager, times(1)).setRecentFailureAssociationStatus(anyInt(), anyInt());

        // Simulate a NETWORK_CONNECTION_EVENT which should clear the ExtraFailureReason
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, null);
        mLooper.dispatchAll();
        verify(mWifiConfigManager, times(2)).clearRecentFailureReason(eq(0));
        verify(mWifiConfigManager, times(1)).setRecentFailureAssociationStatus(anyInt(), anyInt());
    }

    private WifiConfiguration makeLastSelectedWifiConfiguration(int lastSelectedNetworkId,
            long timeSinceLastSelected) {
        long lastSelectedTimestamp = 45666743454L;

        when(mClock.getElapsedSinceBootMillis()).thenReturn(
                lastSelectedTimestamp + timeSinceLastSelected);
        when(mWifiConfigManager.getLastSelectedTimeStamp()).thenReturn(lastSelectedTimestamp);
        when(mWifiConfigManager.getLastSelectedNetwork()).thenReturn(lastSelectedNetworkId);

        WifiConfiguration currentConfig = new WifiConfiguration();
        currentConfig.networkId = lastSelectedNetworkId;
        return currentConfig;
    }

    /**
     * Test that the helper method
     * {@link ClientModeImpl#shouldEvaluateWhetherToSendExplicitlySelected(WifiConfiguration)}
     * returns true when we connect to the last selected network before expiration of
     * {@link ClientModeImpl#LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS}.
     */
    @Test
    public void testShouldEvaluateWhetherToSendExplicitlySelected_SameNetworkNotExpired() {
        WifiConfiguration currentConfig = makeLastSelectedWifiConfiguration(5,
                ClientModeImpl.LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS - 1);
        assertTrue(mCmi.shouldEvaluateWhetherToSendExplicitlySelected(currentConfig));
    }

    /**
     * Test that the helper method
     * {@link ClientModeImpl#shouldEvaluateWhetherToSendExplicitlySelected(WifiConfiguration)}
     * returns false when we connect to the last selected network after expiration of
     * {@link ClientModeImpl#LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS}.
     */
    @Test
    public void testShouldEvaluateWhetherToSendExplicitlySelected_SameNetworkExpired() {
        WifiConfiguration currentConfig = makeLastSelectedWifiConfiguration(5,
                ClientModeImpl.LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS + 1);
        assertFalse(mCmi.shouldEvaluateWhetherToSendExplicitlySelected(currentConfig));
    }

    /**
     * Test that the helper method
     * {@link ClientModeImpl#shouldEvaluateWhetherToSendExplicitlySelected(WifiConfiguration)}
     * returns false when we connect to a different network to the last selected network.
     */
    @Test
    public void testShouldEvaluateWhetherToSendExplicitlySelected_DifferentNetwork() {
        WifiConfiguration currentConfig = makeLastSelectedWifiConfiguration(5,
                ClientModeImpl.LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS - 1);
        currentConfig.networkId = 4;
        assertFalse(mCmi.shouldEvaluateWhetherToSendExplicitlySelected(currentConfig));
    }

    private void expectRegisterNetworkAgent(Consumer<NetworkAgentConfig> configChecker,
            Consumer<NetworkCapabilities> networkCapabilitiesChecker) {
        // Expects that the code calls registerNetworkAgent and provides a way for the test to
        // verify the messages sent through the NetworkAgent to ConnectivityService.
        // We cannot just use a mock object here because mWifiNetworkAgent is private to CMI.
        // TODO (b/134538181): consider exposing WifiNetworkAgent and using mocks.
        ArgumentCaptor<Messenger> messengerCaptor = ArgumentCaptor.forClass(Messenger.class);
        ArgumentCaptor<NetworkAgentConfig> configCaptor =
                ArgumentCaptor.forClass(NetworkAgentConfig.class);
        ArgumentCaptor<NetworkCapabilities> networkCapabilitiesCaptor =
                ArgumentCaptor.forClass(NetworkCapabilities.class);
        verify(mConnectivityManager).registerNetworkAgent(messengerCaptor.capture(),
                any(NetworkInfo.class), any(LinkProperties.class),
                networkCapabilitiesCaptor.capture(),
                anyInt(), configCaptor.capture(), anyInt());

        registerAsyncChannel((x) -> {
            mNetworkAgentAsyncChannel = x;
        }, messengerCaptor.getValue(), mNetworkAgentHandler);
        configChecker.accept(configCaptor.getValue());
        networkCapabilitiesChecker.accept(networkCapabilitiesCaptor.getValue());

        mNetworkAgentAsyncChannel.sendMessage(AsyncChannel.CMD_CHANNEL_FULL_CONNECTION);
        mLooper.dispatchAll();
    }

    private void expectUnregisterNetworkAgent() {
        // We cannot just use a mock object here because mWifiNetworkAgent is private to CMI.
        // TODO (b/134538181): consider exposing WifiNetworkAgent and using mocks.
        ArgumentCaptor<Message> messageCaptor = ArgumentCaptor.forClass(Message.class);
        mLooper.dispatchAll();
        verify(mNetworkAgentHandler).handleMessage(messageCaptor.capture());
        Message message = messageCaptor.getValue();
        assertNotNull(message);
        assertEquals(NetworkAgent.EVENT_NETWORK_INFO_CHANGED, message.what);
        NetworkInfo networkInfo = (NetworkInfo) message.obj;
        assertEquals(NetworkInfo.DetailedState.DISCONNECTED, networkInfo.getDetailedState());
    }

    private void expectNetworkAgentUpdateCapabilities(
            Consumer<NetworkCapabilities> networkCapabilitiesChecker) {
        // We cannot just use a mock object here because mWifiNetworkAgent is private to CMI.
        // TODO (b/134538181): consider exposing WifiNetworkAgent and using mocks.
        ArgumentCaptor<Message> messageCaptor = ArgumentCaptor.forClass(Message.class);
        mLooper.dispatchAll();
        verify(mNetworkAgentHandler).handleMessage(messageCaptor.capture());
        Message message = messageCaptor.getValue();
        assertNotNull(message);
        assertEquals(NetworkAgent.EVENT_NETWORK_CAPABILITIES_CHANGED, message.what);
        networkCapabilitiesChecker.accept((NetworkCapabilities) message.obj);
    }

    /**
     * Verify that when a network is explicitly selected, but noInternetAccessExpected is false,
     * the {@link NetworkAgentConfig} contains the right values of explicitlySelected,
     * acceptUnvalidated and acceptPartialConnectivity.
     */
    @Test
    public void testExplicitlySelected_ExplicitInternetExpected() throws Exception {
        // Network is explicitly selected.
        WifiConfiguration config = makeLastSelectedWifiConfiguration(FRAMEWORK_NETWORK_ID,
                ClientModeImpl.LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS - 1);
        mConnectedNetwork.noInternetAccessExpected = false;

        connect();
        expectRegisterNetworkAgent((agentConfig) -> {
            assertTrue(agentConfig.explicitlySelected);
            assertFalse(agentConfig.acceptUnvalidated);
            assertFalse(agentConfig.acceptPartialConnectivity);
        }, (cap) -> { });
    }

    /**
     * Verify that when a network is not explicitly selected, but noInternetAccessExpected is true,
     * the {@link NetworkAgentConfig} contains the right values of explicitlySelected,
     * acceptUnvalidated and acceptPartialConnectivity.
     */
    @Test
    public void testExplicitlySelected_NotExplicitNoInternetExpected() throws Exception {
        // Network is no longer explicitly selected.
        WifiConfiguration config = makeLastSelectedWifiConfiguration(FRAMEWORK_NETWORK_ID,
                ClientModeImpl.LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS + 1);
        mConnectedNetwork.noInternetAccessExpected = true;

        connect();
        expectRegisterNetworkAgent((agentConfig) -> {
            assertFalse(agentConfig.explicitlySelected);
            assertFalse(agentConfig.acceptUnvalidated);
            assertTrue(agentConfig.acceptPartialConnectivity);
        }, (cap) -> { });
    }

    /**
     * Verify that when a network is explicitly selected, and noInternetAccessExpected is true,
     * the {@link NetworkAgentConfig} contains the right values of explicitlySelected,
     * acceptUnvalidated and acceptPartialConnectivity.
     */
    @Test
    public void testExplicitlySelected_ExplicitNoInternetExpected() throws Exception {
        // Network is explicitly selected.
        WifiConfiguration config = makeLastSelectedWifiConfiguration(FRAMEWORK_NETWORK_ID,
                ClientModeImpl.LAST_SELECTED_NETWORK_EXPIRATION_AGE_MILLIS - 1);
        mConnectedNetwork.noInternetAccessExpected = true;

        connect();
        expectRegisterNetworkAgent((agentConfig) -> {
            assertTrue(agentConfig.explicitlySelected);
            assertTrue(agentConfig.acceptUnvalidated);
            assertTrue(agentConfig.acceptPartialConnectivity);
        }, (cap) -> { });
    }

    /**
     * Verify that CMI dump includes WakeupController.
     */
    @Test
    public void testDumpShouldDumpWakeupController() {
        ByteArrayOutputStream stream = new ByteArrayOutputStream();
        PrintWriter writer = new PrintWriter(stream);
        mCmi.dump(null, writer, null);
        verify(mWakeupController).dump(null, writer, null);
    }

    @Test
    public void takeBugReportCallsWifiDiagnostics() {
        mCmi.takeBugReport(anyString(), anyString());
        verify(mWifiDiagnostics).takeBugReport(anyString(), anyString());
    }

    /**
     * Verify that Rssi Monitoring is started and the callback registered after connecting.
     */
    @Test
    public void verifyRssiMonitoringCallbackIsRegistered() throws Exception {
        // Simulate the first connection.
        connect();
        ArgumentCaptor<Messenger> messengerCaptor = ArgumentCaptor.forClass(Messenger.class);
        verify(mConnectivityManager).registerNetworkAgent(messengerCaptor.capture(),
                any(NetworkInfo.class), any(LinkProperties.class), any(NetworkCapabilities.class),
                anyInt(), any(NetworkAgentConfig.class), anyInt());

        ArrayList<Integer> thresholdsArray = new ArrayList<>();
        thresholdsArray.add(RSSI_THRESHOLD_MAX);
        thresholdsArray.add(RSSI_THRESHOLD_MIN);
        Bundle thresholds = new Bundle();
        thresholds.putIntegerArrayList("thresholds", thresholdsArray);
        Message message = new Message();
        message.what = NetworkAgent.CMD_SET_SIGNAL_STRENGTH_THRESHOLDS;
        message.obj  = thresholds;
        messengerCaptor.getValue().send(message);
        mLooper.dispatchAll();

        ArgumentCaptor<WifiNative.WifiRssiEventHandler> rssiEventHandlerCaptor =
                ArgumentCaptor.forClass(WifiNative.WifiRssiEventHandler.class);
        verify(mWifiNative).startRssiMonitoring(anyString(), anyByte(), anyByte(),
                rssiEventHandlerCaptor.capture());

        // breach below min
        rssiEventHandlerCaptor.getValue().onRssiThresholdBreached(RSSI_THRESHOLD_BREACH_MIN);
        mLooper.dispatchAll();
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertEquals(RSSI_THRESHOLD_BREACH_MIN, wifiInfo.getRssi());

        // breach above max
        rssiEventHandlerCaptor.getValue().onRssiThresholdBreached(RSSI_THRESHOLD_BREACH_MAX);
        mLooper.dispatchAll();
        assertEquals(RSSI_THRESHOLD_BREACH_MAX, wifiInfo.getRssi());
    }

    /**
     * Verify that RSSI and link layer stats polling works in connected mode
     */
    @Test
    public void verifyConnectedModeRssiPolling() throws Exception {
        final long startMillis = 1_500_000_000_100L;
        WifiLinkLayerStats llStats = new WifiLinkLayerStats();
        llStats.txmpdu_be = 1000;
        llStats.rxmpdu_bk = 2000;
        WifiNl80211Manager.SignalPollResult signalPollResult =
                new WifiNl80211Manager.SignalPollResult(-42, 65, 54, sFreq);
        when(mWifiNative.getWifiLinkLayerStats(any())).thenReturn(llStats);
        when(mWifiNative.signalPoll(any())).thenReturn(signalPollResult);
        when(mClock.getWallClockMillis()).thenReturn(startMillis + 0);
        mCmi.enableRssiPolling(true);
        connect();
        mLooper.dispatchAll();
        when(mClock.getWallClockMillis()).thenReturn(startMillis + 3333);
        mLooper.dispatchAll();
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertEquals(llStats.txmpdu_be, wifiInfo.txSuccess);
        assertEquals(llStats.rxmpdu_bk, wifiInfo.rxSuccess);
        assertEquals(signalPollResult.currentRssiDbm, wifiInfo.getRssi());
        assertEquals(signalPollResult.txBitrateMbps, wifiInfo.getLinkSpeed());
        assertEquals(signalPollResult.txBitrateMbps, wifiInfo.getTxLinkSpeedMbps());
        assertEquals(signalPollResult.rxBitrateMbps, wifiInfo.getRxLinkSpeedMbps());
        assertEquals(sFreq, wifiInfo.getFrequency());
        verify(mWifiDataStall, atLeastOnce()).getTxThroughputKbps();
        verify(mWifiDataStall, atLeastOnce()).getRxThroughputKbps();
        verify(mWifiScoreCard).noteSignalPoll(any());
    }

    /**
     * Verify RSSI polling with verbose logging
     */
    @Test
    public void verifyConnectedModeRssiPollingWithVerboseLogging() throws Exception {
        mCmi.enableVerboseLogging(1);
        verifyConnectedModeRssiPolling();
    }

    /**
     * Verify that calls to start and stop filtering multicast packets are passed on to the IpClient
     * instance.
     */
    @Test
    public void verifyMcastLockManagerFilterControllerCallsUpdateIpClient() throws Exception {
        loadComponentsInStaMode();
        reset(mIpClient);
        WifiMulticastLockManager.FilterController filterController =
                mCmi.getMcastLockManagerFilterController();
        filterController.startFilteringMulticastPackets();
        verify(mIpClient).setMulticastFilter(eq(true));
        filterController.stopFilteringMulticastPackets();
        verify(mIpClient).setMulticastFilter(eq(false));
    }

    /**
     * Verifies that when
     * 1. Global feature support flag is set to false
     * 2. connected MAC randomization is on and
     * 3. macRandomizationSetting of the WifiConfiguration is RANDOMIZATION_PERSISTENT and
     * 4. randomized MAC for the network to connect to is different from the current MAC.
     *
     * The factory MAC address is used for the connection, and no attempt is made to change it.
     */
    @Test
    public void testConnectedMacRandomizationNotSupported() throws Exception {
        mResources.setBoolean(R.bool.config_wifi_connected_mac_randomization_supported, false);
        initializeCmi();
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        connect();
        assertEquals(TEST_GLOBAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());
        verify(mWifiNative, never()).setMacAddress(any(), any());
        verify(mWifiNative, never()).getFactoryMacAddress(any());
    }

    /**
     * Verifies that when
     * 1. connected MAC randomization is on and
     * 2. macRandomizationSetting of the WifiConfiguration is RANDOMIZATION_PERSISTENT and
     * 3. randomized MAC for the network to connect to is different from the current MAC.
     *
     * Then the current MAC gets set to the randomized MAC when CMD_START_CONNECT executes.
     */
    @Test
    public void testConnectedMacRandomizationRandomizationPersistentDifferentMac()
            throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        connect();
        verify(mWifiNative).setMacAddress(WIFI_IFACE_NAME, TEST_LOCAL_MAC_ADDRESS);
        verify(mWifiMetrics)
                .logStaEvent(eq(StaEvent.TYPE_MAC_CHANGE), any(WifiConfiguration.class));
        assertEquals(TEST_LOCAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());
    }

    /**
     * Verifies that when
     * 1. connected MAC randomization is on and
     * 2. macRandomizationSetting of the WifiConfiguration is RANDOMIZATION_PERSISTENT and
     * 3. randomized MAC for the network to connect to is same as the current MAC.
     *
     * Then MAC change should not occur when CMD_START_CONNECT executes.
     */
    @Test
    public void testConnectedMacRandomizationRandomizationPersistentSameMac() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        when(mWifiNative.getMacAddress(WIFI_IFACE_NAME))
                .thenReturn(TEST_LOCAL_MAC_ADDRESS.toString());

        connect();
        verify(mWifiNative, never()).setMacAddress(WIFI_IFACE_NAME, TEST_LOCAL_MAC_ADDRESS);
        verify(mWifiMetrics, never())
                .logStaEvent(eq(StaEvent.TYPE_MAC_CHANGE), any(WifiConfiguration.class));
        assertEquals(TEST_LOCAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());
    }

    /**
     * Verifies that when
     * 1. connected MAC randomization is on and
     * 2. macRandomizationSetting of the WifiConfiguration is RANDOMIZATION_NONE and
     * 3. current MAC address is not the factory MAC.
     *
     * Then the current MAC gets set to the factory MAC when CMD_START_CONNECT executes.
     * @throws Exception
     */
    @Test
    public void testConnectedMacRandomizationRandomizationNoneDifferentMac() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        when(mWifiNative.getMacAddress(WIFI_IFACE_NAME))
                .thenReturn(TEST_LOCAL_MAC_ADDRESS.toString());

        WifiConfiguration config = new WifiConfiguration();
        config.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_NONE;
        config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(0)).thenReturn(config);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(mWifiNative).setMacAddress(WIFI_IFACE_NAME, TEST_GLOBAL_MAC_ADDRESS);
        verify(mWifiMetrics)
                .logStaEvent(eq(StaEvent.TYPE_MAC_CHANGE), any(WifiConfiguration.class));
        assertEquals(TEST_GLOBAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());
    }

    /**
     * Verifies that when
     * 1. connected MAC randomization is on and
     * 2. macRandomizationSetting of the WifiConfiguration is RANDOMIZATION_NONE and
     *
     * Then the factory MAC should be used to connect to the network.
     * @throws Exception
     */
    @Test
    public void testConnectedMacRandomizationRandomizationNoneSameMac() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = new WifiConfiguration();
        config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        config.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_NONE;
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(0)).thenReturn(config);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        assertEquals(TEST_GLOBAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());
    }

    /**
     * Verifies that WifiInfo returns DEFAULT_MAC_ADDRESS as mac address when Connected MAC
     * Randomization is on and the device is not connected to a wifi network.
     */
    @Test
    public void testWifiInfoReturnDefaultMacWhenDisconnectedWithRandomization() throws Exception {
        when(mWifiNative.getMacAddress(WIFI_IFACE_NAME))
                .thenReturn(TEST_LOCAL_MAC_ADDRESS.toString());

        connect();
        assertEquals(TEST_LOCAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());

        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, -1, 3, sBSSID);
        mLooper.dispatchAll();
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.DISCONNECTED));
        mLooper.dispatchAll();

        assertEquals("DisconnectedState", getCurrentState().getName());
        assertEquals(WifiInfo.DEFAULT_MAC_ADDRESS, mCmi.getWifiInfo().getMacAddress());
        assertFalse(mCmi.getWifiInfo().hasRealMacAddress());
    }

    /**
     * Verifies that we don't set MAC address when config returns an invalid MAC address.
     */
    @Test
    public void testDoNotSetMacWhenInvalid() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = new WifiConfiguration();
        config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.NONE);
        config.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_PERSISTENT;
        config.setRandomizedMacAddress(MacAddress.fromString(WifiInfo.DEFAULT_MAC_ADDRESS));
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(0)).thenReturn(config);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        // setMacAddress is invoked once when ClientModeImpl starts to prevent leak of factory MAC.
        verify(mWifiNative).setMacAddress(eq(WIFI_IFACE_NAME), any(MacAddress.class));
    }

    /**
     * Verify that we don't crash when WifiNative returns null as the current MAC address.
     * @throws Exception
     */
    @Test
    public void testMacRandomizationWifiNativeReturningNull() throws Exception {
        when(mWifiNative.getMacAddress(anyString())).thenReturn(null);
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        connect();
        verify(mWifiNative).setMacAddress(WIFI_IFACE_NAME, TEST_LOCAL_MAC_ADDRESS);
    }

    /**
     * Verifies that a notification is posted when a connection failure happens on a network
     * in the hotlist. Then verify that tapping on the notification launches an dialog, which
     * could be used to set the randomization setting for a network to "Trusted".
     */
    @Test
    public void testConnectionFailureSendRandomizationSettingsNotification() throws Exception {
        when(mWifiConfigManager.isInFlakyRandomizationSsidHotlist(anyInt())).thenReturn(true);
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, FRAMEWORK_NETWORK_ID, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_TIMEOUT);
        mLooper.dispatchAll();

        WifiConfiguration config = mCmi.getCurrentWifiConfiguration();
        verify(mConnectionFailureNotifier)
                .showFailedToConnectDueToNoRandomizedMacSupportNotification(FRAMEWORK_NETWORK_ID);
    }

    /**
     * Verifies that a notification is not posted when a wrong password failure happens on a
     * network in the hotlist.
     */
    @Test
    public void testNotCallingIsInFlakyRandomizationSsidHotlistOnWrongPassword() throws Exception {
        when(mWifiConfigManager.isInFlakyRandomizationSsidHotlist(anyInt())).thenReturn(true);
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, FRAMEWORK_NETWORK_ID, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD);
        mLooper.dispatchAll();

        verify(mConnectionFailureNotifier, never())
                .showFailedToConnectDueToNoRandomizedMacSupportNotification(anyInt());
    }

    /**
     * Verifies that CMD_START_CONNECT make WifiDiagnostics report
     * CONNECTION_EVENT_STARTED
     * @throws Exception
     */
    @Test
    public void testReportConnectionEventIsCalledAfterCmdStartConnect() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        verify(mWifiDiagnostics, never()).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_STARTED));
        mLooper.dispatchAll();
        verify(mWifiDiagnostics).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_STARTED));
    }

    /**
     * Verifies that CMD_DIAG_CONNECT_TIMEOUT is processed after the timeout threshold if we
     * start a connection but do not finish it.
     * @throws Exception
     */
    @Test
    public void testCmdDiagsConnectTimeoutIsGeneratedAfterCmdStartConnect() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        mLooper.moveTimeForward(ClientModeImpl.DIAGS_CONNECT_TIMEOUT_MILLIS);
        mLooper.dispatchAll();
        verify(mWifiDiagnostics).reportConnectionEvent(
                eq(BaseWifiDiagnostics.CONNECTION_EVENT_TIMEOUT));
    }

    /**
     * Verifies that CMD_DIAG_CONNECT_TIMEOUT does not get processed before the timeout threshold.
     * @throws Exception
     */
    @Test
    public void testCmdDiagsConnectTimeoutIsNotProcessedBeforeTimerExpires() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        mLooper.moveTimeForward(ClientModeImpl.DIAGS_CONNECT_TIMEOUT_MILLIS - 1000);
        mLooper.dispatchAll();
        verify(mWifiDiagnostics, never()).reportConnectionEvent(
                eq(BaseWifiDiagnostics.CONNECTION_EVENT_TIMEOUT));
    }

    private void verifyConnectionEventTimeoutDoesNotOccur() {
        mLooper.moveTimeForward(ClientModeImpl.DIAGS_CONNECT_TIMEOUT_MILLIS);
        mLooper.dispatchAll();
        verify(mWifiDiagnostics, never()).reportConnectionEvent(
                eq(BaseWifiDiagnostics.CONNECTION_EVENT_TIMEOUT));
    }

    /**
     * Verifies that association failures make WifiDiagnostics report CONNECTION_EVENT_FAILED
     * and then cancel any pending timeouts.
     * Also, send connection status to {@link WifiNetworkFactory} & {@link WifiConnectivityManager}.
     * @throws Exception
     */
    @Test
    public void testReportConnectionEventIsCalledAfterAssociationFailure() throws Exception {
        mConnectedNetwork.getNetworkSelectionStatus()
                .setCandidate(getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.ASSOCIATION_REJECTION_EVENT, 0,
                ISupplicantStaIfaceCallback.StatusCode.AP_UNABLE_TO_HANDLE_NEW_STA, sBSSID);
        verify(mWifiDiagnostics, never()).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_FAILED));
        mLooper.dispatchAll();
        verify(mWifiDiagnostics).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_FAILED));
        verify(mWifiConnectivityManager).handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_ASSOCIATION_REJECTION, sBSSID, sSSID);
        verify(mWifiNetworkFactory).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_ASSOCIATION_REJECTION),
                any(WifiConfiguration.class));
        verify(mWifiNetworkSuggestionsManager).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_ASSOCIATION_REJECTION),
                any(WifiConfiguration.class), eq(null));
        verify(mWifiMetrics, never())
                .incrementNumBssidDifferentSelectionBetweenFrameworkAndFirmware();
        verifyConnectionEventTimeoutDoesNotOccur();
    }

    /**
     * Verifies that authentication failures make WifiDiagnostics report
     * CONNECTION_EVENT_FAILED and then cancel any pending timeouts.
     * Also, send connection status to {@link WifiNetworkFactory} & {@link WifiConnectivityManager}.
     * @throws Exception
     */
    @Test
    public void testReportConnectionEventIsCalledAfterAuthenticationFailure() throws Exception {
        mConnectedNetwork.getNetworkSelectionStatus()
                .setCandidate(getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD);
        verify(mWifiDiagnostics, never()).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_FAILED));
        mLooper.dispatchAll();
        verify(mWifiDiagnostics).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_FAILED));
        verify(mWifiConnectivityManager).handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_AUTHENTICATION_FAILURE, sBSSID, sSSID);
        verify(mWifiNetworkFactory).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_AUTHENTICATION_FAILURE),
                any(WifiConfiguration.class));
        verify(mWifiNetworkSuggestionsManager).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_AUTHENTICATION_FAILURE),
                any(WifiConfiguration.class), eq(null));
        verify(mWifiMetrics, never())
                .incrementNumBssidDifferentSelectionBetweenFrameworkAndFirmware();
        verifyConnectionEventTimeoutDoesNotOccur();
    }

    /**
     * Verify that if a NETWORK_DISCONNECTION_EVENT is received in ConnectedState, then an
     * abnormal disconnect is reported to BssidBlocklistMonitor.
     */
    @Test
    public void testAbnormalDisconnectNotifiesBssidBlocklistMonitor() throws Exception {
        // trigger RSSI poll to update WifiInfo
        mCmi.enableRssiPolling(true);
        WifiLinkLayerStats llStats = new WifiLinkLayerStats();
        llStats.txmpdu_be = 1000;
        llStats.rxmpdu_bk = 2000;
        WifiNl80211Manager.SignalPollResult signalPollResult =
                new WifiNl80211Manager.SignalPollResult(TEST_RSSI, 65, 54, sFreq);
        when(mWifiNative.getWifiLinkLayerStats(any())).thenReturn(llStats);
        when(mWifiNative.signalPoll(any())).thenReturn(signalPollResult);

        connect();
        mLooper.dispatchAll();
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(eq(sBSSID), eq(sSSID),
                eq(BssidBlocklistMonitor.REASON_ABNORMAL_DISCONNECT), anyInt());
    }

    /**
     * Verify that ClientModeImpl notifies BssidBlocklistMonitor correctly when the RSSI is
     * too low.
     */
    @Test
    public void testNotifiesBssidBlocklistMonitorLowRssi() throws Exception {
        int testLowRssi = -80;
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, FRAMEWORK_NETWORK_ID, 0,
                ClientModeImpl.SUPPLICANT_BSSID_ANY);
        mCmi.sendMessage(WifiMonitor.ASSOCIATION_REJECTION_EVENT, 1, 0, sBSSID);
        when(mWifiConfigManager.findScanRssi(eq(FRAMEWORK_NETWORK_ID), anyInt())).thenReturn(-80);
        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);
        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(testLowRssi, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(testLowRssi, sBSSID, sFreq).getScanResult());
        mLooper.dispatchAll();

        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(sBSSID, sSSID,
                BssidBlocklistMonitor.REASON_ASSOCIATION_TIMEOUT, testLowRssi);
    }

    /**
     * Verifies that the BssidBlocklistMonitor is notified, but the WifiLastResortWatchdog is
     * not notified of association rejections of type REASON_CODE_AP_UNABLE_TO_HANDLE_NEW_STA.
     * @throws Exception
     */
    @Test
    public void testAssociationRejectionWithReasonApUnableToHandleNewStaUpdatesWatchdog()
            throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.ASSOCIATION_REJECTION_EVENT, 0,
                ClientModeImpl.REASON_CODE_AP_UNABLE_TO_HANDLE_NEW_STA, sBSSID);
        mLooper.dispatchAll();
        verify(mWifiLastResortWatchdog, never()).noteConnectionFailureAndTriggerIfNeeded(
                anyString(), anyString(), anyInt());
        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(eq(sBSSID), eq(sSSID),
                eq(BssidBlocklistMonitor.REASON_AP_UNABLE_TO_HANDLE_NEW_STA), anyInt());
    }

    /**
     * Verifies that WifiLastResortWatchdog and BssidBlocklistMonitor is notified of
     * general association rejection failures.
     * @throws Exception
     */
    @Test
    public void testAssociationRejectionUpdatesWatchdog() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        WifiConfiguration config = mWifiConfigManager.getConfiguredNetwork(FRAMEWORK_NETWORK_ID);
        config.carrierId = CARRIER_ID_1;
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.ASSOCIATION_REJECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        verify(mWifiLastResortWatchdog).noteConnectionFailureAndTriggerIfNeeded(
                anyString(), anyString(), anyInt());
        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(eq(sBSSID), eq(sSSID),
                eq(BssidBlocklistMonitor.REASON_ASSOCIATION_REJECTION), anyInt());
        verify(mWifiMetrics).incrementNumOfCarrierWifiConnectionNonAuthFailure();
    }

    /**
     * Verifies that WifiLastResortWatchdog is not notified of authentication failures of type
     * ERROR_AUTH_FAILURE_WRONG_PSWD.
     * @throws Exception
     */
    @Test
    public void testFailureWrongPassIsIgnoredByWatchdog() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_WRONG_PSWD);
        mLooper.dispatchAll();
        verify(mWifiLastResortWatchdog, never()).noteConnectionFailureAndTriggerIfNeeded(
                anyString(), anyString(), anyInt());
        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(eq(sBSSID), eq(sSSID),
                eq(BssidBlocklistMonitor.REASON_WRONG_PASSWORD), anyInt());
    }

    /**
     * Verifies that WifiLastResortWatchdog is not notified of authentication failures of type
     * ERROR_AUTH_FAILURE_EAP_FAILURE.
     * @throws Exception
     */
    @Test
    public void testEapFailureIsIgnoredByWatchdog() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_EAP_FAILURE);
        mLooper.dispatchAll();
        verify(mWifiLastResortWatchdog, never()).noteConnectionFailureAndTriggerIfNeeded(
                anyString(), anyString(), anyInt());
        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(eq(sBSSID), eq(sSSID),
                eq(BssidBlocklistMonitor.REASON_EAP_FAILURE), anyInt());
    }

    /**
     * Verifies that WifiLastResortWatchdog is notified of other types of authentication failures.
     * @throws Exception
     */
    @Test
    public void testAuthenticationFailureUpdatesWatchdog() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mCmi.sendMessage(WifiMonitor.AUTHENTICATION_FAILURE_EVENT,
                WifiManager.ERROR_AUTH_FAILURE_TIMEOUT);
        mLooper.dispatchAll();
        verify(mWifiLastResortWatchdog).noteConnectionFailureAndTriggerIfNeeded(
                anyString(), anyString(), anyInt());
        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(eq(sBSSID), eq(sSSID),
                eq(BssidBlocklistMonitor.REASON_AUTHENTICATION_FAILURE), anyInt());
    }

    /**
     * Verify that BssidBlocklistMonitor is notified of the SSID pre-connection so that it could
     * send down to firmware the list of blocked BSSIDs.
     */
    @Test
    public void testBssidBlocklistSentToFirmwareAfterCmdStartConnect() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        verify(mBssidBlocklistMonitor, never()).updateFirmwareRoamingConfiguration(sSSID);
        mLooper.dispatchAll();
        verify(mBssidBlocklistMonitor).updateFirmwareRoamingConfiguration(sSSID);
        // But don't expect to see connection success yet
        verify(mWifiScoreCard, never()).noteIpConfiguration(any());
        // And certainly not validation success
        verify(mWifiScoreCard, never()).noteValidationSuccess(any());
    }

    /**
     * Verifies that dhcp failures make WifiDiagnostics report CONNECTION_EVENT_FAILED and then
     * cancel any pending timeouts.
     * Also, send connection status to {@link WifiNetworkFactory} & {@link WifiConnectivityManager}.
     * @throws Exception
     */
    @Test
    public void testReportConnectionEventIsCalledAfterDhcpFailure() throws Exception {
        mConnectedNetwork.getNetworkSelectionStatus()
                .setCandidate(getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());
        testDhcpFailure();
        verify(mWifiDiagnostics, atLeastOnce()).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_FAILED));
        verify(mWifiConnectivityManager, atLeastOnce()).handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_DHCP, sBSSID, sSSID);
        verify(mWifiNetworkFactory, atLeastOnce()).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_DHCP), any(WifiConfiguration.class));
        verify(mWifiNetworkSuggestionsManager, atLeastOnce()).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_DHCP), any(WifiConfiguration.class),
                any(String.class));
        verify(mWifiMetrics, never())
                .incrementNumBssidDifferentSelectionBetweenFrameworkAndFirmware();
        verifyConnectionEventTimeoutDoesNotOccur();
    }

    /**
     * Verifies that a successful validation make WifiDiagnostics report CONNECTION_EVENT_SUCCEEDED
     * and then cancel any pending timeouts.
     * Also, send connection status to {@link WifiNetworkFactory} & {@link WifiConnectivityManager}.
     * @throws Exception
     */
    @Test
    public void testReportConnectionEventIsCalledAfterSuccessfulConnection() throws Exception {
        mConnectedNetwork.getNetworkSelectionStatus()
                .setCandidate(getGoogleGuestScanDetail(TEST_RSSI, sBSSID1, sFreq).getScanResult());
        connect();
        ArgumentCaptor<Messenger> messengerCaptor = ArgumentCaptor.forClass(Messenger.class);
        verify(mConnectivityManager).registerNetworkAgent(messengerCaptor.capture(),
                any(NetworkInfo.class), any(LinkProperties.class), any(NetworkCapabilities.class),
                anyInt(), any(NetworkAgentConfig.class), anyInt());

        Message message = new Message();
        message.what = NetworkAgent.CMD_REPORT_NETWORK_STATUS;
        message.arg1 = NetworkAgent.VALID_NETWORK;
        message.obj = new Bundle();
        messengerCaptor.getValue().send(message);
        mLooper.dispatchAll();

        verify(mWifiDiagnostics).reportConnectionEvent(
                eq(WifiDiagnostics.CONNECTION_EVENT_SUCCEEDED));
        verify(mWifiConnectivityManager).handleConnectionAttemptEnded(
                WifiMetrics.ConnectionEvent.FAILURE_NONE, sBSSID, sSSID);
        verify(mWifiNetworkFactory).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_NONE), any(WifiConfiguration.class));
        verify(mWifiNetworkSuggestionsManager).handleConnectionAttemptEnded(
                eq(WifiMetrics.ConnectionEvent.FAILURE_NONE), any(WifiConfiguration.class),
                any(String.class));
        // BSSID different, record this connection.
        verify(mWifiMetrics).incrementNumBssidDifferentSelectionBetweenFrameworkAndFirmware();
        verifyConnectionEventTimeoutDoesNotOccur();
    }

    /**
     * Verify that score card is notified of a connection attempt
     */
    @Test
    public void testScoreCardNoteConnectionAttemptAfterCmdStartConnect() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        verify(mWifiScoreCard, never()).noteConnectionAttempt(any(), anyInt(), anyString());
        mLooper.dispatchAll();
        verify(mWifiScoreCard).noteConnectionAttempt(any(), anyInt(), anyString());
        verify(mWifiConfigManager).findScanRssi(anyInt(), anyInt());
        // But don't expect to see connection success yet
        verify(mWifiScoreCard, never()).noteIpConfiguration(any());
        // And certainly not validation success
        verify(mWifiScoreCard, never()).noteValidationSuccess(any());

    }

    /**
     * Verify that score card is notified of a successful connection
     */
    @Test
    public void testScoreCardNoteConnectionComplete() throws Exception {
        Pair<String, String> l2KeyAndCluster = Pair.create("Wad", "Gab");
        when(mWifiScoreCard.getL2KeyAndGroupHint(any())).thenReturn(l2KeyAndCluster);
        connect();
        mLooper.dispatchAll();
        verify(mWifiScoreCard).noteIpConfiguration(any());
        ArgumentCaptor<Layer2InformationParcelable> captor =
                ArgumentCaptor.forClass(Layer2InformationParcelable.class);
        verify(mIpClient, atLeastOnce()).updateLayer2Information(captor.capture());
        final Layer2InformationParcelable info = captor.getValue();
        assertEquals(info.l2Key, "Wad");
        assertEquals(info.cluster, "Gab");
    }

    /**
     * Verify that score card/health monitor are notified when wifi is disabled while disconnected
     */
    @Test
    public void testScoreCardNoteWifiDisabledWhileDisconnected() throws Exception {
        // connecting and disconnecting shouldn't note wifi disabled
        disconnect();
        mLooper.dispatchAll();

        verify(mWifiScoreCard, times(1)).resetConnectionState();
        verify(mWifiScoreCard, never()).noteWifiDisabled(any());
        verify(mWifiHealthMonitor, never()).setWifiEnabled(false);

        // disabling while disconnected should note wifi disabled
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_DISABLED);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();
        verify(mWifiScoreCard, times(2)).resetConnectionState();
        verify(mWifiHealthMonitor).setWifiEnabled(false);
    }

    /**
     * Verify that score card/health monitor are notified when wifi is disabled while connected
     */
    @Test
    public void testScoreCardNoteWifiDisabledWhileConnected() throws Exception {
        // Get into connected state
        connect();
        mLooper.dispatchAll();
        verify(mWifiScoreCard, never()).noteWifiDisabled(any());
        verify(mWifiHealthMonitor, never()).setWifiEnabled(false);

        // disabling while connected should note wifi disabled
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_DISABLED);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();

        verify(mWifiScoreCard).noteWifiDisabled(any());
        verify(mWifiScoreCard).resetConnectionState();
        verify(mWifiHealthMonitor).setWifiEnabled(false);
    }

    /**
     * Verify that we do not crash on quick toggling wifi on/off
     */
    @Test
    public void quickTogglesDoNotCrash() throws Exception {
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();

        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        mLooper.dispatchAll();

        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        mLooper.dispatchAll();

        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();
    }

    /**
     * Verify that valid calls to set the current wifi state are returned when requested.
     */
    @Test
    public void verifySetAndGetWifiStateCallsWorking() throws Exception {
        // we start off disabled
        assertEquals(WifiManager.WIFI_STATE_DISABLED, mCmi.syncGetWifiState());

        // now check after updating
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_UNKNOWN);
        assertEquals(WifiManager.WIFI_STATE_UNKNOWN, mCmi.syncGetWifiState());

        // check after two updates
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_ENABLING);
        mCmi.setWifiStateForApiCalls(WifiManager.WIFI_STATE_ENABLED);
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());
    }

    /**
     * Verify that invalid states do not change the saved wifi state.
     */
    @Test
    public void verifyInvalidStatesDoNotChangeSavedWifiState() throws Exception {
        int invalidStateNegative = -1;
        int invalidStatePositive = 5;

        // we start off disabled
        assertEquals(WifiManager.WIFI_STATE_DISABLED, mCmi.syncGetWifiState());

        mCmi.setWifiStateForApiCalls(invalidStateNegative);
        assertEquals(WifiManager.WIFI_STATE_DISABLED, mCmi.syncGetWifiState());

        mCmi.setWifiStateForApiCalls(invalidStatePositive);
        assertEquals(WifiManager.WIFI_STATE_DISABLED, mCmi.syncGetWifiState());
    }

    /**
     * Verify that IPClient instance is shutdown when wifi is disabled.
     */
    @Test
    public void verifyIpClientShutdownWhenDisabled() throws Exception {
        loadComponentsInStaMode();

        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();
        verify(mIpClient).shutdown();
        verify(mWifiConfigManager).removeAllEphemeralOrPasspointConfiguredNetworks();
        verify(mWifiConfigManager).clearUserTemporarilyDisabledList();
    }

    /**
     * Verify that WifiInfo's MAC address is updated when the state machine receives
     * NETWORK_CONNECTION_EVENT while in ConnectedState.
     */
    @Test
    public void verifyWifiInfoMacUpdatedWithNetworkConnectionWhileConnected() throws Exception {
        connect();
        assertEquals("ConnectedState", getCurrentState().getName());
        assertEquals(TEST_LOCAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());

        // Verify receiving a NETWORK_CONNECTION_EVENT changes the MAC in WifiInfo
        when(mWifiNative.getMacAddress(WIFI_IFACE_NAME))
                .thenReturn(TEST_GLOBAL_MAC_ADDRESS.toString());
        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        assertEquals(TEST_GLOBAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());
    }

    /**
     * Verify that WifiInfo's MAC address is updated when the state machine receives
     * NETWORK_CONNECTION_EVENT while in DisconnectedState.
     */
    @Test
    public void verifyWifiInfoMacUpdatedWithNetworkConnectionWhileDisconnected() throws Exception {
        disconnect();
        assertEquals("DisconnectedState", getCurrentState().getName());
        // Since MAC randomization is enabled, wifiInfo's MAC should be set to default MAC
        // when disconnect happens.
        assertEquals(WifiInfo.DEFAULT_MAC_ADDRESS, mCmi.getWifiInfo().getMacAddress());

        when(mWifiNative.getMacAddress(WIFI_IFACE_NAME))
                .thenReturn(TEST_LOCAL_MAC_ADDRESS.toString());
        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        assertEquals(TEST_LOCAL_MAC_ADDRESS.toString(), mCmi.getWifiInfo().getMacAddress());
    }

    /**
     * Verify that we temporarily disable the network when auto-connected to a network
     * with no internet access.
     */
    @Test
    public void verifyAutoConnectedNetworkWithInternetValidationFailure() throws Exception {
        // Setup RSSI poll to update WifiInfo with low RSSI
        mCmi.enableRssiPolling(true);
        WifiLinkLayerStats llStats = new WifiLinkLayerStats();
        llStats.txmpdu_be = 1000;
        llStats.rxmpdu_bk = 2000;
        WifiNl80211Manager.SignalPollResult signalPollResult =
                new WifiNl80211Manager.SignalPollResult(RSSI_THRESHOLD_BREACH_MIN, 65, 54, sFreq);
        when(mWifiNative.getWifiLinkLayerStats(any())).thenReturn(llStats);
        when(mWifiNative.signalPoll(any())).thenReturn(signalPollResult);

        // Simulate the first connection.
        connect();
        ArgumentCaptor<Messenger> messengerCaptor = ArgumentCaptor.forClass(Messenger.class);
        verify(mConnectivityManager).registerNetworkAgent(messengerCaptor.capture(),
                any(NetworkInfo.class), any(LinkProperties.class), any(NetworkCapabilities.class),
                anyInt(), any(NetworkAgentConfig.class), anyInt());

        WifiConfiguration currentNetwork = new WifiConfiguration();
        currentNetwork.networkId = FRAMEWORK_NETWORK_ID;
        currentNetwork.SSID = DEFAULT_TEST_SSID;
        currentNetwork.noInternetAccessExpected = false;
        currentNetwork.numNoInternetAccessReports = 1;
        when(mWifiConfigManager.getConfiguredNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(currentNetwork);
        when(mWifiConfigManager.getLastSelectedNetwork()).thenReturn(FRAMEWORK_NETWORK_ID + 1);

        Message message = new Message();
        message.what = NetworkAgent.CMD_REPORT_NETWORK_STATUS;
        message.arg1 = NetworkAgent.INVALID_NETWORK;
        message.obj = new Bundle();
        messengerCaptor.getValue().send(message);
        mLooper.dispatchAll();

        verify(mWifiConfigManager)
                .incrementNetworkNoInternetAccessReports(FRAMEWORK_NETWORK_ID);
        verify(mWifiConfigManager).updateNetworkSelectionStatus(
                FRAMEWORK_NETWORK_ID, DISABLED_NO_INTERNET_TEMPORARY);
        verify(mBssidBlocklistMonitor).handleBssidConnectionFailure(sBSSID, sSSID,
                BssidBlocklistMonitor.REASON_NETWORK_VALIDATION_FAILURE, RSSI_THRESHOLD_BREACH_MIN);
        verify(mWifiScoreCard).noteValidationFailure(any());
    }

    /**
     * Verify that we don't temporarily disable the network when user selected to connect to a
     * network with no internet access.
     */
    @Test
    public void verifyLastSelectedNetworkWithInternetValidationFailure() throws Exception {
        // Simulate the first connection.
        connect();
        ArgumentCaptor<Messenger> messengerCaptor = ArgumentCaptor.forClass(Messenger.class);
        verify(mConnectivityManager).registerNetworkAgent(messengerCaptor.capture(),
                any(NetworkInfo.class), any(LinkProperties.class), any(NetworkCapabilities.class),
                anyInt(), any(NetworkAgentConfig.class), anyInt());

        WifiConfiguration currentNetwork = new WifiConfiguration();
        currentNetwork.networkId = FRAMEWORK_NETWORK_ID;
        currentNetwork.noInternetAccessExpected = false;
        currentNetwork.numNoInternetAccessReports = 1;
        when(mWifiConfigManager.getConfiguredNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(currentNetwork);
        when(mWifiConfigManager.getLastSelectedNetwork()).thenReturn(FRAMEWORK_NETWORK_ID);

        Message message = new Message();
        message.what = NetworkAgent.CMD_REPORT_NETWORK_STATUS;
        message.arg1 = NetworkAgent.INVALID_NETWORK;
        message.obj = new Bundle();
        messengerCaptor.getValue().send(message);
        mLooper.dispatchAll();

        verify(mWifiConfigManager)
                .incrementNetworkNoInternetAccessReports(FRAMEWORK_NETWORK_ID);
        verify(mWifiConfigManager, never()).updateNetworkSelectionStatus(
                FRAMEWORK_NETWORK_ID, DISABLED_NO_INTERNET_TEMPORARY);
    }

    /**
     * Verify that we temporarily disable the network when auto-connected to a network
     * with no internet access.
     */
    @Test
    public void verifyAutoConnectedNoInternetExpectedNetworkWithInternetValidationFailure()
            throws Exception {
        // Simulate the first connection.
        connect();
        ArgumentCaptor<Messenger> messengerCaptor = ArgumentCaptor.forClass(Messenger.class);
        verify(mConnectivityManager).registerNetworkAgent(messengerCaptor.capture(),
                any(NetworkInfo.class), any(LinkProperties.class), any(NetworkCapabilities.class),
                anyInt(), any(NetworkAgentConfig.class), anyInt());

        WifiConfiguration currentNetwork = new WifiConfiguration();
        currentNetwork.networkId = FRAMEWORK_NETWORK_ID;
        currentNetwork.noInternetAccessExpected = true;
        currentNetwork.numNoInternetAccessReports = 1;
        when(mWifiConfigManager.getConfiguredNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(currentNetwork);
        when(mWifiConfigManager.getLastSelectedNetwork()).thenReturn(FRAMEWORK_NETWORK_ID + 1);

        Message message = new Message();
        message.what = NetworkAgent.CMD_REPORT_NETWORK_STATUS;
        message.arg1 = NetworkAgent.INVALID_NETWORK;
        message.obj = new Bundle();
        messengerCaptor.getValue().send(message);
        mLooper.dispatchAll();

        verify(mWifiConfigManager)
                .incrementNetworkNoInternetAccessReports(FRAMEWORK_NETWORK_ID);
        verify(mWifiConfigManager, never()).updateNetworkSelectionStatus(
                FRAMEWORK_NETWORK_ID, DISABLED_NO_INTERNET_TEMPORARY);
    }

    /**
     * Verify that we enable the network when we detect validated internet access.
     */
    @Test
    public void verifyNetworkSelectionEnableOnInternetValidation() throws Exception {
        // Simulate the first connection.
        connect();
        verify(mBssidBlocklistMonitor).handleBssidConnectionSuccess(sBSSID, sSSID);
        verify(mBssidBlocklistMonitor).handleDhcpProvisioningSuccess(sBSSID, sSSID);
        verify(mBssidBlocklistMonitor, never()).handleNetworkValidationSuccess(sBSSID, sSSID);

        ArgumentCaptor<Messenger> messengerCaptor = ArgumentCaptor.forClass(Messenger.class);
        verify(mConnectivityManager).registerNetworkAgent(messengerCaptor.capture(),
                any(NetworkInfo.class), any(LinkProperties.class), any(NetworkCapabilities.class),
                anyInt(), any(NetworkAgentConfig.class), anyInt());

        when(mWifiConfigManager.getLastSelectedNetwork()).thenReturn(FRAMEWORK_NETWORK_ID + 1);

        Message message = new Message();
        message.what = NetworkAgent.CMD_REPORT_NETWORK_STATUS;
        message.arg1 = NetworkAgent.VALID_NETWORK;
        message.obj = new Bundle();
        messengerCaptor.getValue().send(message);
        mLooper.dispatchAll();

        verify(mWifiConfigManager)
                .setNetworkValidatedInternetAccess(FRAMEWORK_NETWORK_ID, true);
        verify(mWifiConfigManager).updateNetworkSelectionStatus(
                FRAMEWORK_NETWORK_ID, DISABLED_NONE);
        verify(mWifiScoreCard).noteValidationSuccess(any());
        verify(mBssidBlocklistMonitor).handleNetworkValidationSuccess(sBSSID, sSSID);
    }

    private void connectWithValidInitRssi(int initRssiDbm) throws Exception {
        triggerConnect();
        mCmi.getWifiInfo().setRssi(initRssiDbm);
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.ASSOCIATED));
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());
    }

    /**
     * Verify that we set the INTERNET and bandwidth capability in the network agent when connected
     * as a result of auto-join/legacy API's. Also verify up/down stream bandwidth values when
     * Rx link speed is unavailable.
     */
    @Test
    public void verifyNetworkCapabilities() throws Exception {
        when(mWifiDataStall.getTxThroughputKbps()).thenReturn(70_000);
        when(mWifiDataStall.getRxThroughputKbps()).thenReturn(-1);
        when(mWifiNetworkFactory.getSpecificNetworkRequestUidAndPackageName(any()))
                .thenReturn(Pair.create(Process.INVALID_UID, ""));
        // Simulate the first connection.
        connectWithValidInitRssi(-42);

        ArgumentCaptor<NetworkCapabilities> networkCapabilitiesCaptor =
                ArgumentCaptor.forClass(NetworkCapabilities.class);
        verify(mConnectivityManager).registerNetworkAgent(any(Messenger.class),
                any(NetworkInfo.class), any(LinkProperties.class),
                networkCapabilitiesCaptor.capture(), anyInt(), any(NetworkAgentConfig.class),
                anyInt());

        NetworkCapabilities networkCapabilities = networkCapabilitiesCaptor.getValue();
        assertNotNull(networkCapabilities);

        // Should have internet capability.
        assertTrue(networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET));
        assertNull(networkCapabilities.getNetworkSpecifier());

        assertEquals(mConnectedNetwork.creatorUid, networkCapabilities.getOwnerUid());
        assertArrayEquals(
                new int[] {mConnectedNetwork.creatorUid},
                networkCapabilities.getAdministratorUids());

        // Should set bandwidth correctly
        assertEquals(-42, mCmi.getWifiInfo().getRssi());
        assertEquals(70_000, networkCapabilities.getLinkUpstreamBandwidthKbps());
        assertEquals(70_000, networkCapabilities.getLinkDownstreamBandwidthKbps());
    }

    /**
     * Verify that we don't set the INTERNET capability in the network agent when connected
     * as a result of the new network request API. Also verify up/down stream bandwidth values
     * when both Tx and Rx link speed are unavailable.
     */
    @Test
    public void verifyNetworkCapabilitiesForSpecificRequest() throws Exception {
        when(mWifiDataStall.getTxThroughputKbps()).thenReturn(-1);
        when(mWifiDataStall.getRxThroughputKbps()).thenReturn(-1);
        when(mWifiNetworkFactory.getSpecificNetworkRequestUidAndPackageName(any()))
                .thenReturn(Pair.create(TEST_UID, OP_PACKAGE_NAME));
        // Simulate the first connection.
        connectWithValidInitRssi(-42);
        ArgumentCaptor<NetworkCapabilities> networkCapabilitiesCaptor =
                ArgumentCaptor.forClass(NetworkCapabilities.class);
        verify(mConnectivityManager).registerNetworkAgent(any(Messenger.class),
                any(NetworkInfo.class), any(LinkProperties.class),
                networkCapabilitiesCaptor.capture(), anyInt(), any(NetworkAgentConfig.class),
                anyInt());

        NetworkCapabilities networkCapabilities = networkCapabilitiesCaptor.getValue();
        assertNotNull(networkCapabilities);

        // should not have internet capability.
        assertFalse(networkCapabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET));

        NetworkSpecifier networkSpecifier = networkCapabilities.getNetworkSpecifier();
        assertTrue(networkSpecifier instanceof WifiNetworkAgentSpecifier);
        WifiNetworkAgentSpecifier wifiNetworkAgentSpecifier =
                (WifiNetworkAgentSpecifier) networkSpecifier;
        WifiNetworkAgentSpecifier expectedWifiNetworkAgentSpecifier =
                new WifiNetworkAgentSpecifier(mCmi.getCurrentWifiConfiguration());
        assertEquals(expectedWifiNetworkAgentSpecifier, wifiNetworkAgentSpecifier);
        assertEquals(TEST_UID, networkCapabilities.getRequestorUid());
        assertEquals(OP_PACKAGE_NAME, networkCapabilities.getRequestorPackageName());
        assertEquals(90_000, networkCapabilities.getLinkUpstreamBandwidthKbps());
        assertEquals(80_000, networkCapabilities.getLinkDownstreamBandwidthKbps());
    }

    /**
     * Verify that we check for data stall during rssi poll
     * and then check that wifi link layer usage data are being updated.
     */
    @Test
    public void verifyRssiPollChecksDataStall() throws Exception {
        mCmi.enableRssiPolling(true);
        connect();

        WifiLinkLayerStats oldLLStats = new WifiLinkLayerStats();
        when(mWifiNative.getWifiLinkLayerStats(any())).thenReturn(oldLLStats);
        mCmi.sendMessage(ClientModeImpl.CMD_RSSI_POLL, 1);
        mLooper.dispatchAll();
        WifiLinkLayerStats newLLStats = new WifiLinkLayerStats();
        when(mWifiNative.getWifiLinkLayerStats(any())).thenReturn(newLLStats);
        mCmi.sendMessage(ClientModeImpl.CMD_RSSI_POLL, 1);
        mLooper.dispatchAll();
        verify(mWifiDataStall).checkDataStallAndThroughputSufficiency(
                oldLLStats, newLLStats, mCmi.getWifiInfo());
        verify(mWifiMetrics).incrementWifiLinkLayerUsageStats(newLLStats);
    }

    /**
     * Verify that we update wifi usability stats entries during rssi poll and that when we get
     * a data stall we label and save the current list of usability stats entries.
     * @throws Exception
     */
    @Test
    public void verifyRssiPollUpdatesWifiUsabilityMetrics() throws Exception {
        mCmi.enableRssiPolling(true);
        connect();

        WifiLinkLayerStats stats = new WifiLinkLayerStats();
        when(mWifiNative.getWifiLinkLayerStats(any())).thenReturn(stats);
        when(mWifiDataStall.checkDataStallAndThroughputSufficiency(any(), any(), any()))
                .thenReturn(WifiIsUnusableEvent.TYPE_UNKNOWN);
        mCmi.sendMessage(ClientModeImpl.CMD_RSSI_POLL, 1);
        mLooper.dispatchAll();
        verify(mWifiMetrics).updateWifiUsabilityStatsEntries(any(), eq(stats));
        verify(mWifiMetrics, never()).addToWifiUsabilityStatsList(WifiUsabilityStats.LABEL_BAD,
                eq(anyInt()), eq(-1));

        when(mWifiDataStall.checkDataStallAndThroughputSufficiency(any(), any(), any()))
                .thenReturn(WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX);
        when(mClock.getElapsedSinceBootMillis()).thenReturn(10L);
        mCmi.sendMessage(ClientModeImpl.CMD_RSSI_POLL, 1);
        mLooper.dispatchAll();
        verify(mWifiMetrics, times(2)).updateWifiUsabilityStatsEntries(any(), eq(stats));
        when(mClock.getElapsedSinceBootMillis())
                .thenReturn(10L + ClientModeImpl.DURATION_TO_WAIT_ADD_STATS_AFTER_DATA_STALL_MS);
        mCmi.sendMessage(ClientModeImpl.CMD_RSSI_POLL, 1);
        mLooper.dispatchAll();
        verify(mWifiMetrics).addToWifiUsabilityStatsList(WifiUsabilityStats.LABEL_BAD,
                WifiIsUnusableEvent.TYPE_DATA_STALL_BAD_TX, -1);
    }

    /**
     * Verify that when ordered to setPowerSave(true) while Interface is created,
     * WifiNative is called with the right powerSave mode.
     */
    @Test
    public void verifySetPowerSaveTrueSuccess() throws Exception {
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        assertTrue(mCmi.setPowerSave(true));
        verify(mWifiNative).setPowerSave(WIFI_IFACE_NAME, true);
    }

    /**
     * Verify that when ordered to setPowerSave(false) while Interface is created,
     * WifiNative is called with the right powerSave mode.
     */
    @Test
    public void verifySetPowerSaveFalseSuccess() throws Exception {
        mCmi.setOperationalMode(ClientModeImpl.CONNECT_MODE, WIFI_IFACE_NAME);
        assertTrue(mCmi.setPowerSave(false));
        verify(mWifiNative).setPowerSave(WIFI_IFACE_NAME, false);
    }

    /**
     * Verify that when interface is not created yet (InterfaceName is null),
     * then setPowerSave() returns with error and no call in WifiNative happens.
     */
    @Test
    public void verifySetPowerSaveFailure() throws Exception {
        boolean powerSave = true;
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        assertFalse(mCmi.setPowerSave(powerSave));
        verify(mWifiNative, never()).setPowerSave(anyString(), anyBoolean());
    }

    /**
     * Verify that we call into WifiTrafficPoller during rssi poll
     */
    @Test
    public void verifyRssiPollCallsWifiTrafficPoller() throws Exception {
        mCmi.enableRssiPolling(true);
        connect();

        verify(mWifiTrafficPoller).notifyOnDataActivity(anyLong(), anyLong());
    }

    /**
     * Verify that LinkProbeManager is updated during RSSI poll
     */
    @Test
    public void verifyRssiPollCallsLinkProbeManager() throws Exception {
        mCmi.enableRssiPolling(true);

        connect();
        // reset() should be called when RSSI polling is enabled and entering L2ConnectedState
        verify(mLinkProbeManager).resetOnNewConnection(); // called first time here
        verify(mLinkProbeManager, never()).resetOnScreenTurnedOn(); // not called
        verify(mLinkProbeManager).updateConnectionStats(any(), any());

        mCmi.enableRssiPolling(false);
        mLooper.dispatchAll();
        // reset() should be called when in L2ConnectedState (or child states) and RSSI polling
        // becomes enabled
        mCmi.enableRssiPolling(true);
        mLooper.dispatchAll();
        verify(mLinkProbeManager, times(1)).resetOnNewConnection(); // verify not called again
        verify(mLinkProbeManager).resetOnScreenTurnedOn(); // verify called here
    }

    /**
     * Verify that when ordered to setLowLatencyMode(true),
     * WifiNative is called with the right lowLatency mode.
     */
    @Test
    public void verifySetLowLatencyTrueSuccess() throws Exception {
        when(mWifiNative.setLowLatencyMode(anyBoolean())).thenReturn(true);
        assertTrue(mCmi.setLowLatencyMode(true));
        verify(mWifiNative).setLowLatencyMode(true);
    }

    /**
     * Verify that when ordered to setLowLatencyMode(false),
     * WifiNative is called with the right lowLatency mode.
     */
    @Test
    public void verifySetLowLatencyFalseSuccess() throws Exception {
        when(mWifiNative.setLowLatencyMode(anyBoolean())).thenReturn(true);
        assertTrue(mCmi.setLowLatencyMode(false));
        verify(mWifiNative).setLowLatencyMode(false);
    }

    /**
     * Verify that when WifiNative fails to set low latency mode,
     * then the call setLowLatencyMode() returns with failure,
     */
    @Test
    public void verifySetLowLatencyModeFailure() throws Exception {
        final boolean lowLatencyMode = true;
        when(mWifiNative.setLowLatencyMode(anyBoolean())).thenReturn(false);
        assertFalse(mCmi.setLowLatencyMode(lowLatencyMode));
        verify(mWifiNative).setLowLatencyMode(eq(lowLatencyMode));
    }

    /**
     * Verify getting the factory MAC address success case.
     */
    @Test
    public void testGetFactoryMacAddressSuccess() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(TEST_GLOBAL_MAC_ADDRESS.toString(), mCmi.getFactoryMacAddress());
        verify(mWifiNative).getFactoryMacAddress(WIFI_IFACE_NAME);
        verify(mWifiNative).getMacAddress(anyString()); // called once when setting up client mode
    }

    /**
     * Verify getting the factory MAC address failure case.
     */
    @Test
    public void testGetFactoryMacAddressFail() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        when(mWifiNative.getFactoryMacAddress(WIFI_IFACE_NAME)).thenReturn(null);
        assertNull(mCmi.getFactoryMacAddress());
        verify(mWifiNative).getFactoryMacAddress(WIFI_IFACE_NAME);
        verify(mWifiNative).getMacAddress(anyString()); // called once when setting up client mode
    }

    /**
     * Verify that when WifiNative#getFactoryMacAddress fails, if the device does not support
     * MAC randomization then the currently programmed MAC address gets returned.
     */
    @Test
    public void testGetFactoryMacAddressFailWithNoMacRandomizationSupport() throws Exception {
        mResources.setBoolean(R.bool.config_wifi_connected_mac_randomization_supported, false);
        initializeCmi();
        initializeAndAddNetworkAndVerifySuccess();
        when(mWifiNative.getFactoryMacAddress(WIFI_IFACE_NAME)).thenReturn(null);
        mCmi.getFactoryMacAddress();
        verify(mWifiNative).getFactoryMacAddress(anyString());
        verify(mWifiNative, times(2)).getMacAddress(WIFI_IFACE_NAME);
    }

    @Test
    public void testSetDeviceMobilityState() {
        mCmi.setDeviceMobilityState(WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
        verify(mWifiConnectivityManager).setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
        verify(mWifiHealthMonitor).setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
        verify(mWifiDataStall).setDeviceMobilityState(
                WifiManager.DEVICE_MOBILITY_STATE_STATIONARY);
    }

    /**
     * Verify the MAC address is being randomized at start to prevent leaking the factory MAC.
     */
    @Test
    public void testRandomizeMacAddressOnStart() throws Exception {
        ArgumentCaptor<MacAddress> macAddressCaptor = ArgumentCaptor.forClass(MacAddress.class);
        loadComponentsInStaMode();
        verify(mWifiNative).setMacAddress(anyString(), macAddressCaptor.capture());
        MacAddress currentMac = macAddressCaptor.getValue();

        assertNotEquals("The currently programmed MAC address should be different from the factory "
                + "MAC address after ClientModeImpl starts",
                mCmi.getFactoryMacAddress(), currentMac.toString());
    }

    /**
     * Verify the MAC address is being randomized at start to prevent leaking the factory MAC.
     */
    @Test
    public void testNoRandomizeMacAddressOnStartIfMacRandomizationNotEnabled() throws Exception {
        mResources.setBoolean(R.bool.config_wifi_connected_mac_randomization_supported, false);
        loadComponentsInStaMode();
        verify(mWifiNative, never()).setMacAddress(anyString(), any());
    }

    /**
     * Verify bugreport will be taken when get IP_REACHABILITY_LOST
     */
    @Test
    public void testTakebugreportbyIpReachabilityLost() throws Exception {
        connect();

        mCmi.sendMessage(ClientModeImpl.CMD_IP_REACHABILITY_LOST);
        mLooper.dispatchAll();
        verify(mWifiDiagnostics).captureBugReportData(
                eq(WifiDiagnostics.REPORT_REASON_REACHABILITY_LOST));
    }

    /**
     * Verify removing sim will also remove an ephemeral Passpoint Provider. And reset carrier
     * privileged suggestor apps.
     */
    @Test
    public void testResetSimNetworkWhenRemovingSim() throws Exception {
        // Switch to connect mode and verify wifi is reported as enabled
        startSupplicantAndDispatchMessages();

        // Indicate that sim is removed.
        mCmi.sendMessage(ClientModeImpl.CMD_RESET_SIM_NETWORKS,
                ClientModeImpl.RESET_SIM_REASON_SIM_REMOVED);
        mLooper.dispatchAll();

        verify(mWifiConfigManager).resetSimNetworks();
        verify(mWifiNetworkSuggestionsManager).resetCarrierPrivilegedApps();
    }

    /**
     * Verify inserting sim will reset carrier privileged suggestor apps.
     * and remove any previous notifications due to sim removal
     */
    @Test
    public void testResetCarrierPrivilegedAppsWhenInsertingSim() throws Exception {
        // Switch to connect mode and verify wifi is reported as enabled
        startSupplicantAndDispatchMessages();

        // Indicate that sim is inserted.
        mCmi.sendMessage(ClientModeImpl.CMD_RESET_SIM_NETWORKS,
                ClientModeImpl.RESET_SIM_REASON_SIM_INSERTED);
        mLooper.dispatchAll();

        verify(mWifiConfigManager, never()).resetSimNetworks();
        verify(mWifiNetworkSuggestionsManager).resetCarrierPrivilegedApps();
        verify(mSimRequiredNotifier).dismissSimRequiredNotification();
    }

    /**
     * Verifies that WifiLastResortWatchdog is notified of FOURWAY_HANDSHAKE_TIMEOUT.
     */
    @Test
    public void testHandshakeTimeoutUpdatesWatchdog() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        // Verifies that WifiLastResortWatchdog won't be notified
        // by other reason code
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 2, sBSSID);
        mLooper.dispatchAll();

        assertEquals("DisconnectedState", getCurrentState().getName());
        verify(mWifiLastResortWatchdog, never()).noteConnectionFailureAndTriggerIfNeeded(
                sSSID, sBSSID, WifiLastResortWatchdog.FAILURE_CODE_AUTHENTICATION);

        // Verifies that WifiLastResortWatchdog be notified
        // for FOURWAY_HANDSHAKE_TIMEOUT.
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 15, sBSSID);
        mLooper.dispatchAll();

        assertEquals("DisconnectedState", getCurrentState().getName());
        verify(mWifiLastResortWatchdog).noteConnectionFailureAndTriggerIfNeeded(
                sSSID, sBSSID, WifiLastResortWatchdog.FAILURE_CODE_AUTHENTICATION);

    }

    /**
     * Verify that WifiInfo is correctly populated after connection.
     */
    @Test
    public void verifyWifiInfoGetNetworkSpecifierPackageName() throws Exception {
        mConnectedNetwork.fromWifiNetworkSpecifier = true;
        mConnectedNetwork.ephemeral = true;
        mConnectedNetwork.trusted = true;
        mConnectedNetwork.creatorName = OP_PACKAGE_NAME;
        connect();

        assertTrue(mCmi.getWifiInfo().isEphemeral());
        assertTrue(mCmi.getWifiInfo().isTrusted());
        assertEquals(OP_PACKAGE_NAME,
                mCmi.getWifiInfo().getRequestingPackageName());
    }

    /**
     * Verify that WifiInfo is correctly populated after connection.
     */
    @Test
    public void verifyWifiInfoGetNetworkSuggestionPackageName() throws Exception {
        mConnectedNetwork.fromWifiNetworkSuggestion = true;
        mConnectedNetwork.ephemeral = true;
        mConnectedNetwork.trusted = true;
        mConnectedNetwork.creatorName = OP_PACKAGE_NAME;
        connect();

        assertTrue(mCmi.getWifiInfo().isEphemeral());
        assertTrue(mCmi.getWifiInfo().isTrusted());
        assertEquals(OP_PACKAGE_NAME,
                mCmi.getWifiInfo().getRequestingPackageName());
    }

    /**
     * Verify that a WifiIsUnusableEvent is logged and the current list of usability stats entries
     * are labeled and saved when receiving an IP reachability lost message.
     * @throws Exception
     */
    @Test
    public void verifyIpReachabilityLostMsgUpdatesWifiUsabilityMetrics() throws Exception {
        connect();

        mCmi.sendMessage(ClientModeImpl.CMD_IP_REACHABILITY_LOST);
        mLooper.dispatchAll();
        verify(mWifiMetrics).logWifiIsUnusableEvent(
                WifiIsUnusableEvent.TYPE_IP_REACHABILITY_LOST);
        verify(mWifiMetrics).addToWifiUsabilityStatsList(WifiUsabilityStats.LABEL_BAD,
                WifiUsabilityStats.TYPE_IP_REACHABILITY_LOST, -1);
    }

    /**
     * Tests that when {@link ClientModeImpl} receives a SUP_REQUEST_IDENTITY message, it responds
     * to the supplicant with the SIM identity.
     */
    @Test
    public void testSupRequestIdentity_setsIdentityResponse() throws Exception {
        mConnectedNetwork = spy(WifiConfigurationTestUtil.createEapNetwork(
                WifiEnterpriseConfig.Eap.SIM, WifiEnterpriseConfig.Phase2.NONE));
        mConnectedNetwork.SSID = DEFAULT_TEST_SSID;

        when(mDataTelephonyManager.getSubscriberId()).thenReturn("3214561234567890");
        when(mDataTelephonyManager.getSimState()).thenReturn(TelephonyManager.SIM_STATE_READY);
        when(mDataTelephonyManager.getSimOperator()).thenReturn("321456");
        when(mDataTelephonyManager.getCarrierInfoForImsiEncryption(anyInt())).thenReturn(null);
        MockitoSession mockSession = ExtendedMockito.mockitoSession()
                .mockStatic(SubscriptionManager.class)
                .startMocking();
        when(SubscriptionManager.getDefaultDataSubscriptionId()).thenReturn(DATA_SUBID);
        when(SubscriptionManager.isValidSubscriptionId(anyInt())).thenReturn(true);

        triggerConnect();

        mCmi.sendMessage(WifiMonitor.SUP_REQUEST_IDENTITY,
                0, FRAMEWORK_NETWORK_ID, DEFAULT_TEST_SSID);
        mLooper.dispatchAll();

        verify(mWifiNative).simIdentityResponse(WIFI_IFACE_NAME,
                "13214561234567890@wlan.mnc456.mcc321.3gppnetwork.org", "");
        mockSession.finishMocking();
    }

    /**
     * Verifies that WifiLastResortWatchdog is notified of DHCP failures when recevied
     * NETWORK_DISCONNECTION_EVENT while in ObtainingIpState.
     */
    @Test
    public void testDhcpFailureUpdatesWatchdog_WhenDisconnectedWhileObtainingIpAddr()
            throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        verify(mWifiNative).removeAllNetworks(WIFI_IFACE_NAME);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());

        // Verifies that WifiLastResortWatchdog be notified.
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        assertEquals("DisconnectedState", getCurrentState().getName());
        verify(mWifiLastResortWatchdog).noteConnectionFailureAndTriggerIfNeeded(
                sSSID, sBSSID, WifiLastResortWatchdog.FAILURE_CODE_DHCP);
    }

    /**
     * Verifies that we trigger a disconnect when the {@link WifiConfigManager}.
     * OnNetworkUpdateListener#onNetworkRemoved(WifiConfiguration)} is invoked.
     */
    @Test
    public void testOnNetworkRemoved() throws Exception {
        connect();

        WifiConfiguration removedNetwork = new WifiConfiguration();
        removedNetwork.networkId = FRAMEWORK_NETWORK_ID;
        mConfigUpdateListenerCaptor.getValue().onNetworkRemoved(removedNetwork);
        mLooper.dispatchAll();

        verify(mWifiNative).removeNetworkCachedData(FRAMEWORK_NETWORK_ID);
        assertEquals("DisconnectingState", getCurrentState().getName());
    }

    /**
     * Verifies that we trigger a disconnect when the {@link WifiConfigManager
     * .OnNetworkUpdateListener#onNetworkPermanentlyDisabled(WifiConfiguration, int)} is invoked.
     */
    @Test
    public void testOnNetworkPermanentlyDisabled() throws Exception {
        connect();

        WifiConfiguration disabledNetwork = new WifiConfiguration();
        disabledNetwork.networkId = FRAMEWORK_NETWORK_ID;
        mConfigUpdateListenerCaptor.getValue().onNetworkPermanentlyDisabled(disabledNetwork,
                WifiConfiguration.NetworkSelectionStatus.DISABLED_BY_WRONG_PASSWORD);
        mLooper.dispatchAll();

        assertEquals("DisconnectingState", getCurrentState().getName());
    }

    /**
     * Verifies that we don't trigger a disconnect when the {@link WifiConfigManager
     * .OnNetworkUpdateListener#onNetworkPermanentlyDisabled(WifiConfiguration, int)} is invoked.
     */
    @Test
    public void testOnNetworkPermanentlyDisabledWithNoInternet() throws Exception {
        connect();

        WifiConfiguration disabledNetwork = new WifiConfiguration();
        disabledNetwork.networkId = FRAMEWORK_NETWORK_ID;
        mConfigUpdateListenerCaptor.getValue().onNetworkPermanentlyDisabled(disabledNetwork,
                WifiConfiguration.NetworkSelectionStatus.DISABLED_NO_INTERNET_PERMANENT);
        mLooper.dispatchAll();

        assertEquals("ConnectedState", getCurrentState().getName());
    }

    /**
     * Verifies that we don't trigger a disconnect when the {@link WifiConfigManager
     * .OnNetworkUpdateListener#onNetworkTemporarilyDisabled(WifiConfiguration, int)} is invoked.
     */
    @Test
    public void testOnNetworkTemporarilyDisabledWithNoInternet() throws Exception {
        connect();

        WifiConfiguration disabledNetwork = new WifiConfiguration();
        disabledNetwork.networkId = FRAMEWORK_NETWORK_ID;
        mConfigUpdateListenerCaptor.getValue().onNetworkTemporarilyDisabled(disabledNetwork,
                WifiConfiguration.NetworkSelectionStatus.DISABLED_NO_INTERNET_TEMPORARY);
        mLooper.dispatchAll();

        assertEquals("ConnectedState", getCurrentState().getName());
    }

    /**
     * Verify that MboOce/WifiDataStall enable/disable methods are called in ClientMode.
     */
    @Test
    public void verifyMboOceWifiDataStallSetupInClientMode() throws Exception {
        startSupplicantAndDispatchMessages();
        verify(mMboOceController).enable();
        verify(mWifiDataStall).enablePhoneStateListener();
        mCmi.setOperationalMode(ClientModeImpl.DISABLED_MODE, null);
        mLooper.dispatchAll();
        verify(mMboOceController).disable();
        verify(mWifiDataStall).disablePhoneStateListener();
    }

    /**
     * Verify that Bluetooth active is set correctly with BT state/connection state changes
     */
    @Test
    public void verifyBluetoothStateAndConnectionStateChanges() throws Exception {
        startSupplicantAndDispatchMessages();
        mCmi.sendBluetoothAdapterStateChange(BluetoothAdapter.STATE_ON);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, times(1)).setBluetoothConnected(false);

        mCmi.sendBluetoothAdapterConnectionStateChange(BluetoothAdapter.STATE_CONNECTED);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, times(1)).setBluetoothConnected(true);

        mCmi.sendBluetoothAdapterStateChange(BluetoothAdapter.STATE_OFF);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, times(2)).setBluetoothConnected(false);

        mCmi.sendBluetoothAdapterStateChange(BluetoothAdapter.STATE_ON);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, times(3)).setBluetoothConnected(false);

        mCmi.sendBluetoothAdapterConnectionStateChange(BluetoothAdapter.STATE_CONNECTING);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, times(2)).setBluetoothConnected(true);

        mCmi.sendBluetoothAdapterConnectionStateChange(BluetoothAdapter.STATE_DISCONNECTED);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, times(4)).setBluetoothConnected(false);

        mCmi.sendBluetoothAdapterConnectionStateChange(BluetoothAdapter.STATE_CONNECTED);
        mLooper.dispatchAll();
        verify(mWifiConnectivityManager, times(3)).setBluetoothConnected(true);
    }

    /**
     * Test that handleBssTransitionRequest() blocklist the BSS upon
     * receiving BTM request frame that contains MBO-OCE IE with an
     * association retry delay attribute.
     */
    @Test
    public void testBtmFrameWithMboAssocretryDelayBlockListTheBssid() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|.
        connect();

        MboOceController.BtmFrameData btmFrmData = new MboOceController.BtmFrameData();

        btmFrmData.mStatus = MboOceConstants.BTM_RESPONSE_STATUS_REJECT_UNSPECIFIED;
        btmFrmData.mBssTmDataFlagsMask = MboOceConstants.BTM_DATA_FLAG_DISASSOCIATION_IMMINENT
                | MboOceConstants.BTM_DATA_FLAG_MBO_ASSOC_RETRY_DELAY_INCLUDED;
        btmFrmData.mBlockListDurationMs = 60000;

        mCmi.sendMessage(WifiMonitor.MBO_OCE_BSS_TM_HANDLING_DONE, btmFrmData);
        mLooper.dispatchAll();

        verify(mWifiMetrics, times(1)).incrementSteeringRequestCountIncludingMboAssocRetryDelay();
        verify(mBssidBlocklistMonitor).blockBssidForDurationMs(eq(sBSSID), eq(sSSID),
                eq(btmFrmData.mBlockListDurationMs), anyInt(), anyInt());
    }

    /**
     * Test that handleBssTransitionRequest() blocklist the BSS upon
     * receiving BTM request frame that contains disassociation imminent bit
     * set to 1.
     */
    @Test
    public void testBtmFrameWithDisassocImminentBitBlockListTheBssid() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|.
        connect();

        MboOceController.BtmFrameData btmFrmData = new MboOceController.BtmFrameData();

        btmFrmData.mStatus = MboOceConstants.BTM_RESPONSE_STATUS_ACCEPT;
        btmFrmData.mBssTmDataFlagsMask = MboOceConstants.BTM_DATA_FLAG_DISASSOCIATION_IMMINENT;

        mCmi.sendMessage(WifiMonitor.MBO_OCE_BSS_TM_HANDLING_DONE, btmFrmData);
        mLooper.dispatchAll();

        verify(mWifiMetrics, never()).incrementSteeringRequestCountIncludingMboAssocRetryDelay();
        verify(mBssidBlocklistMonitor).blockBssidForDurationMs(eq(sBSSID), eq(sSSID),
                eq(MboOceConstants.DEFAULT_BLOCKLIST_DURATION_MS), anyInt(), anyInt());
    }

    /**
     * Test that handleBssTransitionRequest() trigger force scan for
     * network selection when status code is REJECT.
     */
    @Test
    public void testBTMRequestRejectTriggerNetworkSelction() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|.
        connect();

        MboOceController.BtmFrameData btmFrmData = new MboOceController.BtmFrameData();

        btmFrmData.mStatus = MboOceConstants.BTM_RESPONSE_STATUS_REJECT_UNSPECIFIED;
        btmFrmData.mBssTmDataFlagsMask = MboOceConstants.BTM_DATA_FLAG_DISASSOCIATION_IMMINENT
                | MboOceConstants.BTM_DATA_FLAG_BSS_TERMINATION_INCLUDED
                | MboOceConstants.BTM_DATA_FLAG_MBO_CELL_DATA_CONNECTION_PREFERENCE_INCLUDED;
        btmFrmData.mBlockListDurationMs = 60000;

        mCmi.sendMessage(WifiMonitor.MBO_OCE_BSS_TM_HANDLING_DONE, btmFrmData);
        mLooper.dispatchAll();

        verify(mBssidBlocklistMonitor, never()).blockBssidForDurationMs(eq(sBSSID), eq(sSSID),
                eq(btmFrmData.mBlockListDurationMs), anyInt(), anyInt());
        verify(mWifiConnectivityManager).forceConnectivityScan(ClientModeImpl.WIFI_WORK_SOURCE);
        verify(mWifiMetrics, times(1)).incrementMboCellularSwitchRequestCount();
        verify(mWifiMetrics, times(1))
                .incrementForceScanCountDueToSteeringRequest();

    }

    /**
     * Test that handleBssTransitionRequest() does not trigger force
     * scan when status code is accept.
     */
    @Test
    public void testBTMRequestAcceptDoNotTriggerNetworkSelction() throws Exception {
        // Connect to network with |sBSSID|, |sFreq|.
        connect();

        MboOceController.BtmFrameData btmFrmData = new MboOceController.BtmFrameData();

        btmFrmData.mStatus = MboOceConstants.BTM_RESPONSE_STATUS_ACCEPT;
        btmFrmData.mBssTmDataFlagsMask = MboOceConstants.BTM_DATA_FLAG_DISASSOCIATION_IMMINENT;

        mCmi.sendMessage(WifiMonitor.MBO_OCE_BSS_TM_HANDLING_DONE, btmFrmData);
        mLooper.dispatchAll();

        verify(mWifiConnectivityManager, never())
                .forceConnectivityScan(ClientModeImpl.WIFI_WORK_SOURCE);
    }

    /**
     * Test that the interval for poll RSSI is read from config overlay correctly.
     */
    @Test
    public void testPollRssiIntervalIsSetCorrectly() throws Exception {
        assertEquals(3000, mCmi.getPollRssiIntervalMsecs());
        mResources.setInteger(R.integer.config_wifiPollRssiIntervalMilliseconds, 6000);
        assertEquals(6000, mCmi.getPollRssiIntervalMsecs());
        mResources.setInteger(R.integer.config_wifiPollRssiIntervalMilliseconds, 7000);
        assertEquals(6000, mCmi.getPollRssiIntervalMsecs());
    }

    /**
     * Verifies that the logic does not modify PSK key management when WPA3 auto upgrade feature is
     * disabled.
     *
     * @throws Exception
     */
    @Test
    public void testNoWpa3UpgradeWhenOverlaysAreOff() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = mock(WifiConfiguration.class);
        BitSet allowedKeyManagement = mock(BitSet.class);
        config.allowedKeyManagement = allowedKeyManagement;
        when(config.allowedKeyManagement.get(eq(WifiConfiguration.KeyMgmt.WPA_PSK))).thenReturn(
                true);
        when(config.getNetworkSelectionStatus())
                .thenReturn(new WifiConfiguration.NetworkSelectionStatus());
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(0)).thenReturn(config);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeEnabled, false);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeOffloadEnabled, false);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(config, never()).setSecurityParams(eq(WifiConfiguration.SECURITY_TYPE_SAE));
    }

    /**
     * Verifies that the logic always enables SAE key management when WPA3 auto upgrade offload
     * feature is enabled.
     *
     * @throws Exception
     */
    @Test
    public void testWpa3UpgradeOffload() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = mock(WifiConfiguration.class);
        BitSet allowedKeyManagement = mock(BitSet.class);
        BitSet allowedAuthAlgorithms = mock(BitSet.class);
        config.allowedKeyManagement = allowedKeyManagement;
        config.allowedAuthAlgorithms = allowedAuthAlgorithms;
        when(config.allowedKeyManagement.get(eq(WifiConfiguration.KeyMgmt.WPA_PSK))).thenReturn(
                true);
        when(config.getNetworkSelectionStatus())
                .thenReturn(new WifiConfiguration.NetworkSelectionStatus());
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(0)).thenReturn(config);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeEnabled, true);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeOffloadEnabled, true);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(allowedKeyManagement).set(eq(WifiConfiguration.KeyMgmt.SAE));
        verify(allowedAuthAlgorithms).clear();
    }

    /**
     * Verifies that the logic does not enable SAE key management when WPA3 auto upgrade feature is
     * enabled but no SAE candidate is available.
     *
     * @throws Exception
     */
    @Test
    public void testNoWpa3UpgradeWithPskCandidate() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = mock(WifiConfiguration.class);
        BitSet allowedKeyManagement = mock(BitSet.class);
        config.allowedKeyManagement = allowedKeyManagement;
        when(config.allowedKeyManagement.get(eq(WifiConfiguration.KeyMgmt.WPA_PSK)))
                .thenReturn(true);
        WifiConfiguration.NetworkSelectionStatus networkSelectionStatus =
                mock(WifiConfiguration.NetworkSelectionStatus.class);
        when(config.getNetworkSelectionStatus()).thenReturn(networkSelectionStatus);
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(0)).thenReturn(config);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeEnabled, true);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeOffloadEnabled, false);

        String ssid = "WPA2-Network";
        String caps = "[WPA2-FT/PSK+PSK][ESS][WPS]";
        ScanResult scanResult = new ScanResult(WifiSsid.createFromAsciiEncoded(ssid), ssid,
                "ab:cd:01:ef:45:89", 1245, 0, caps, -78, 2450, 1025, 22, 33, 20, 0,
                0, true);
        scanResult.informationElements = new ScanResult.InformationElement[]{
                createIE(ScanResult.InformationElement.EID_SSID,
                        ssid.getBytes(StandardCharsets.UTF_8))
        };
        when(networkSelectionStatus.getCandidate()).thenReturn(scanResult);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(config, never()).setSecurityParams(eq(WifiConfiguration.SECURITY_TYPE_SAE));
    }

    /**
     * Verifies that the logic enables SAE key management when WPA3 auto upgrade feature is
     * enabled and an SAE candidate is available.
     *
     * @throws Exception
     */
    @Test
    public void testWpa3UpgradeWithSaeCandidate() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = mock(WifiConfiguration.class);
        BitSet allowedKeyManagement = mock(BitSet.class);
        BitSet allowedAuthAlgorithms = mock(BitSet.class);
        BitSet allowedProtocols = mock(BitSet.class);
        BitSet allowedPairwiseCiphers = mock(BitSet.class);
        BitSet allowedGroupCiphers = mock(BitSet.class);
        config.allowedKeyManagement = allowedKeyManagement;
        config.allowedAuthAlgorithms = allowedAuthAlgorithms;
        config.allowedProtocols = allowedProtocols;
        config.allowedPairwiseCiphers = allowedPairwiseCiphers;
        config.allowedGroupCiphers = allowedGroupCiphers;
        when(config.allowedKeyManagement.get(eq(WifiConfiguration.KeyMgmt.WPA_PSK)))
                .thenReturn(true);
        WifiConfiguration.NetworkSelectionStatus networkSelectionStatus =
                mock(WifiConfiguration.NetworkSelectionStatus.class);
        when(config.getNetworkSelectionStatus()).thenReturn(networkSelectionStatus);
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(config);
        when(mWifiConfigManager.getScanDetailCacheForNetwork(anyInt())).thenReturn(null);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeEnabled, true);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeOffloadEnabled, false);

        final String ssid = "WPA3-Network";
        String caps = "[WPA2-FT/SAE+SAE][ESS][WPS]";
        ScanResult scanResult = new ScanResult(WifiSsid.createFromAsciiEncoded(ssid), ssid,
                "ab:cd:01:ef:45:89", 1245, 0, caps, -78, 2450, 1025, 22, 33, 20, 0,
                0, true);
        scanResult.informationElements = new ScanResult.InformationElement[]{
                createIE(ScanResult.InformationElement.EID_SSID,
                        ssid.getBytes(StandardCharsets.UTF_8))
        };
        when(networkSelectionStatus.getCandidate()).thenReturn(scanResult);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(config).setSecurityParams(eq(WifiConfiguration.SECURITY_TYPE_SAE));
    }

    /**
     * Verifies that the logic does not enable SAE key management when WPA3 auto upgrade feature is
     * enabled and an SAE candidate is available, while another WPA2 AP is in range.
     *
     * @throws Exception
     */
    @Test
    public void testWpa3UpgradeWithSaeCandidateAndPskApInRange() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = mock(WifiConfiguration.class);
        BitSet allowedKeyManagement = mock(BitSet.class);
        BitSet allowedAuthAlgorithms = mock(BitSet.class);
        BitSet allowedProtocols = mock(BitSet.class);
        BitSet allowedPairwiseCiphers = mock(BitSet.class);
        BitSet allowedGroupCiphers = mock(BitSet.class);
        config.allowedKeyManagement = allowedKeyManagement;
        config.allowedAuthAlgorithms = allowedAuthAlgorithms;
        config.allowedProtocols = allowedProtocols;
        config.allowedPairwiseCiphers = allowedPairwiseCiphers;
        config.allowedGroupCiphers = allowedGroupCiphers;
        when(config.allowedKeyManagement.get(eq(WifiConfiguration.KeyMgmt.WPA_PSK)))
                .thenReturn(true);
        WifiConfiguration.NetworkSelectionStatus networkSelectionStatus =
                mock(WifiConfiguration.NetworkSelectionStatus.class);
        when(config.getNetworkSelectionStatus()).thenReturn(networkSelectionStatus);
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(config);
        when(mWifiConfigManager.getScanDetailCacheForNetwork(anyInt())).thenReturn(null);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeEnabled, true);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeOffloadEnabled, false);

        final String saeBssid = "ab:cd:01:ef:45:89";
        final String pskBssid = "ab:cd:01:ef:45:9a";

        final String ssidSae = "Mixed-Network";
        config.SSID = ScanResultUtil.createQuotedSSID(ssidSae);
        config.networkId = 1;

        String capsSae = "[WPA2-FT/SAE+SAE][ESS][WPS]";
        ScanResult scanResultSae = new ScanResult(WifiSsid.createFromAsciiEncoded(ssidSae), ssidSae,
                saeBssid, 1245, 0, capsSae, -78, 2412, 1025, 22, 33, 20, 0,
                0, true);
        ScanResult.InformationElement ieSae = createIE(ScanResult.InformationElement.EID_SSID,
                ssidSae.getBytes(StandardCharsets.UTF_8));
        scanResultSae.informationElements = new ScanResult.InformationElement[]{ieSae};
        when(networkSelectionStatus.getCandidate()).thenReturn(scanResultSae);
        ScanResult.InformationElement[] ieArr = new ScanResult.InformationElement[1];
        ieArr[0] = ieSae;

        final String ssidPsk = "Mixed-Network";
        String capsPsk = "[WPA2-FT/PSK+PSK][ESS][WPS]";
        ScanResult scanResultPsk = new ScanResult(WifiSsid.createFromAsciiEncoded(ssidPsk), ssidPsk,
                pskBssid, 1245, 0, capsPsk, -48, 2462, 1025, 22, 33, 20, 0,
                0, true);
        ScanResult.InformationElement iePsk = createIE(ScanResult.InformationElement.EID_SSID,
                ssidPsk.getBytes(StandardCharsets.UTF_8));
        scanResultPsk.informationElements = new ScanResult.InformationElement[]{iePsk};
        ScanResult.InformationElement[] ieArrPsk = new ScanResult.InformationElement[1];
        ieArrPsk[0] = iePsk;
        List<ScanResult> scanResults = new ArrayList<>();
        scanResults.add(scanResultPsk);
        scanResults.add(scanResultSae);

        when(mScanRequestProxy.getScanResults()).thenReturn(scanResults);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(config, never()).setSecurityParams(eq(WifiConfiguration.SECURITY_TYPE_SAE));
    }

    /**
     * Verifies that the logic enables SAE key management when WPA3 auto upgrade feature is
     * enabled and no candidate is available (i.e. user selected a WPA2 saved network and the
     * network is actually WPA3.
     *
     * @throws Exception
     */
    @Test
    public void testWpa3UpgradeWithNoCandidate() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = mock(WifiConfiguration.class);
        BitSet allowedKeyManagement = mock(BitSet.class);
        BitSet allowedAuthAlgorithms = mock(BitSet.class);
        BitSet allowedProtocols = mock(BitSet.class);
        BitSet allowedPairwiseCiphers = mock(BitSet.class);
        BitSet allowedGroupCiphers = mock(BitSet.class);
        config.allowedKeyManagement = allowedKeyManagement;
        config.allowedAuthAlgorithms = allowedAuthAlgorithms;
        config.allowedProtocols = allowedProtocols;
        config.allowedPairwiseCiphers = allowedPairwiseCiphers;
        config.allowedGroupCiphers = allowedGroupCiphers;
        when(config.allowedKeyManagement.get(eq(WifiConfiguration.KeyMgmt.WPA_PSK)))
                .thenReturn(true);
        WifiConfiguration.NetworkSelectionStatus networkSelectionStatus =
                mock(WifiConfiguration.NetworkSelectionStatus.class);
        when(config.getNetworkSelectionStatus()).thenReturn(networkSelectionStatus);
        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(config);

        mResources.setBoolean(R.bool.config_wifiSaeUpgradeEnabled, true);
        mResources.setBoolean(R.bool.config_wifiSaeUpgradeOffloadEnabled, false);

        final String saeBssid = "ab:cd:01:ef:45:89";
        final String ssidSae = "WPA3-Network";
        config.SSID = ScanResultUtil.createQuotedSSID(ssidSae);
        config.networkId = 1;

        String capsSae = "[WPA2-FT/SAE+SAE][ESS][WPS]";
        ScanResult scanResultSae = new ScanResult(WifiSsid.createFromAsciiEncoded(ssidSae), ssidSae,
                saeBssid, 1245, 0, capsSae, -78, 2412, 1025, 22, 33, 20, 0,
                0, true);
        ScanResult.InformationElement ieSae = createIE(ScanResult.InformationElement.EID_SSID,
                ssidSae.getBytes(StandardCharsets.UTF_8));
        scanResultSae.informationElements = new ScanResult.InformationElement[]{ieSae};
        when(networkSelectionStatus.getCandidate()).thenReturn(null);
        List<ScanResult> scanResults = new ArrayList<>();
        scanResults.add(scanResultSae);

        when(mScanRequestProxy.getScanResults()).thenReturn(scanResults);

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        verify(config).setSecurityParams(eq(WifiConfiguration.SECURITY_TYPE_SAE));
    }

    private static ScanResult.InformationElement createIE(int id, byte[] bytes) {
        ScanResult.InformationElement ie = new ScanResult.InformationElement();
        ie.id = id;
        ie.bytes = bytes;
        return ie;
    }

    /*
     * Verify isWifiBandSupported for 5GHz with an overlay override config
     */
    @Test
    public void testIsWifiBandSupported5gWithOverride() throws Exception {
        mResources.setBoolean(R.bool.config_wifi5ghzSupport, true);
        assertTrue(mCmi.isWifiBandSupported(WifiScanner.WIFI_BAND_5_GHZ));
        verify(mWifiNative, never()).getChannelsForBand(anyInt());
    }

    /**
     * Verify isWifiBandSupported for 6GHz with an overlay override config
     */
    @Test
    public void testIsWifiBandSupported6gWithOverride() throws Exception {
        mResources.setBoolean(R.bool.config_wifi6ghzSupport, true);
        assertTrue(mCmi.isWifiBandSupported(WifiScanner.WIFI_BAND_6_GHZ));
        verify(mWifiNative, never()).getChannelsForBand(anyInt());
    }

    /**
     * Verify isWifiBandSupported for 5GHz with no overlay override config no channels
     */
    @Test
    public void testIsWifiBandSupported5gNoOverrideNoChannels() throws Exception {
        final int[] emptyArray = {};
        mResources.setBoolean(R.bool.config_wifi5ghzSupport, false);
        when(mWifiNative.getChannelsForBand(anyInt())).thenReturn(emptyArray);
        assertFalse(mCmi.isWifiBandSupported(WifiScanner.WIFI_BAND_5_GHZ));
        verify(mWifiNative).getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ);
    }

    /**
     * Verify isWifiBandSupported for 5GHz with no overlay override config with channels
     */
    @Test
    public void testIsWifiBandSupported5gNoOverrideWithChannels() throws Exception {
        final int[] channelArray = {5170};
        mResources.setBoolean(R.bool.config_wifi5ghzSupport, false);
        when(mWifiNative.getChannelsForBand(anyInt())).thenReturn(channelArray);
        assertTrue(mCmi.isWifiBandSupported(WifiScanner.WIFI_BAND_5_GHZ));
        verify(mWifiNative).getChannelsForBand(WifiScanner.WIFI_BAND_5_GHZ);
    }

    /**
     * Verify isWifiBandSupported for 6GHz with no overlay override config no channels
     */
    @Test
    public void testIsWifiBandSupported6gNoOverrideNoChannels() throws Exception {
        final int[] emptyArray = {};
        mResources.setBoolean(R.bool.config_wifi6ghzSupport, false);
        when(mWifiNative.getChannelsForBand(anyInt())).thenReturn(emptyArray);
        assertFalse(mCmi.isWifiBandSupported(WifiScanner.WIFI_BAND_6_GHZ));
        verify(mWifiNative).getChannelsForBand(WifiScanner.WIFI_BAND_6_GHZ);
    }

    /**
     * Verify isWifiBandSupported for 6GHz with no overlay override config with channels
     */
    @Test
    public void testIsWifiBandSupported6gNoOverrideWithChannels() throws Exception {
        final int[] channelArray = {6420};
        mResources.setBoolean(R.bool.config_wifi6ghzSupport, false);
        when(mWifiNative.getChannelsForBand(anyInt())).thenReturn(channelArray);
        assertTrue(mCmi.isWifiBandSupported(WifiScanner.WIFI_BAND_6_GHZ));
        verify(mWifiNative).getChannelsForBand(WifiScanner.WIFI_BAND_6_GHZ);
    }

    /**
     * Helper function for setting up fils test.
     *
     * @param isDriverSupportFils true if driver support fils.
     * @return wifi configuration.
     */
    private WifiConfiguration setupFilsTest(boolean isDriverSupportFils) {
        assertEquals(ClientModeImpl.CONNECT_MODE, mCmi.getOperationalModeForTest());
        assertEquals(WifiManager.WIFI_STATE_ENABLED, mCmi.syncGetWifiState());

        WifiConfiguration config = new WifiConfiguration();
        config.setSecurityParams(WifiConfiguration.SECURITY_TYPE_EAP);
        config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_EAP);
        config.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.IEEE8021X);
        config.SSID = ScanResultUtil.createQuotedSSID(sFilsSsid);
        config.networkId = 1;
        config.setRandomizedMacAddress(TEST_LOCAL_MAC_ADDRESS);
        config.macRandomizationSetting = WifiConfiguration.RANDOMIZATION_PERSISTENT;

        when(mWifiConfigManager.getConfiguredNetworkWithoutMasking(anyInt())).thenReturn(config);
        if (isDriverSupportFils) {
            when(mWifiNative.getSupportedFeatureSet(WIFI_IFACE_NAME)).thenReturn(
                    WifiManager.WIFI_FEATURE_FILS_SHA256 | WifiManager.WIFI_FEATURE_FILS_SHA384);
        } else {
            when(mWifiNative.getSupportedFeatureSet(WIFI_IFACE_NAME)).thenReturn((long) 0);
        }

        return config;
    }

    /**
     * Helper function for setting up a scan result with FILS supported AP.
     *
     */
    private void setupFilsEnabledApInScanResult() {
        String caps = "[WPA2-EAP+EAP-SHA256+EAP-FILS-SHA256-CCMP]"
                + "[RSN-EAP+EAP-SHA256+EAP-FILS-SHA256-CCMP][ESS]";
        ScanResult scanResult = new ScanResult(WifiSsid.createFromAsciiEncoded(sFilsSsid),
                sFilsSsid, sBSSID, 1245, 0, caps, -78, 2412, 1025, 22, 33, 20, 0, 0, true);
        ScanResult.InformationElement ie = createIE(ScanResult.InformationElement.EID_SSID,
                sFilsSsid.getBytes(StandardCharsets.UTF_8));
        scanResult.informationElements = new ScanResult.InformationElement[]{ie};
        List<ScanResult> scanResults = new ArrayList<>();
        scanResults.add(scanResult);

        when(mScanRequestProxy.getScanResults()).thenReturn(scanResults);
    }


    /**
     * Helper function to send CMD_START_FILS_CONNECTION along with HLP IEs.
     *
     */
    private void prepareFilsHlpPktAndSendStartConnect() {
        Layer2PacketParcelable l2Packet = new Layer2PacketParcelable();
        l2Packet.dstMacAddress = TEST_GLOBAL_MAC_ADDRESS;
        l2Packet.payload = new byte[] {0x00, 0x12, 0x13, 0x00, 0x12, 0x13, 0x00, 0x12, 0x13,
                0x12, 0x13, 0x00, 0x12, 0x13, 0x00, 0x12, 0x13, 0x00, 0x12, 0x13, 0x55, 0x66};
        mCmi.sendMessage(ClientModeImpl.CMD_START_FILS_CONNECTION, 0, 0,
                Collections.singletonList(l2Packet));
        mLooper.dispatchAll();
    }

    /**
     * Verifies that while connecting to AP, the logic looks into the scan result and
     * looks for AP matching the network type and ssid and update the wificonfig with FILS
     * AKM if supported.
     *
     * @throws Exception
     */
    @Test
    public void testFilsAKMUpdateBeforeConnect() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        WifiConfiguration config = setupFilsTest(true);
        setupFilsEnabledApInScanResult();

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        assertTrue(config.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.FILS_SHA256));
        verify(mWifiNative, never()).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
    }

    /**
     * Verifies that while connecting to AP, framework updates the wifi config with
     * FILS AKM only if underlying driver support FILS feature.
     *
     * @throws Exception
     */
    @Test
    public void testFilsAkmIsNotAddedinWifiConfigIfDriverDoesNotSupportFils() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        WifiConfiguration config = setupFilsTest(false);
        setupFilsEnabledApInScanResult();

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        assertFalse(config.allowedKeyManagement.get(WifiConfiguration.KeyMgmt.FILS_SHA256));
        verify(mWifiNative).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
    }


    /**
     * Verifies that the HLP (DHCP) packets are send to wpa_supplicant
     * prior to Fils connection.
     *
     * @throws Exception
     */
    @Test
    public void testFilsHlpUpdateBeforeFilsConnection() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        WifiConfiguration config = setupFilsTest(true);
        setupFilsEnabledApInScanResult();

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        prepareFilsHlpPktAndSendStartConnect();

        verify(mWifiNative).flushAllHlp(eq(WIFI_IFACE_NAME));
        verify(mWifiNative).addHlpReq(eq(WIFI_IFACE_NAME), any(), any());
        verify(mWifiNative).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
    }

    /**
     * Verifies that an association rejection in first FILS connect attempt doesn't block
     * the second connection attempt.
     *
     * @throws Exception
     */
    @Test
    public void testFilsSecondConnectAttemptIsNotBLockedAfterAssocReject() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        WifiConfiguration config = setupFilsTest(true);
        setupFilsEnabledApInScanResult();

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        prepareFilsHlpPktAndSendStartConnect();

        verify(mWifiNative, times(1)).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));

        mCmi.sendMessage(WifiMonitor.ASSOCIATION_REJECTION_EVENT, 0, 2, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        prepareFilsHlpPktAndSendStartConnect();

        verify(mWifiNative, times(2)).connectToNetwork(eq(WIFI_IFACE_NAME), eq(config));
    }

    /**
     * Verifies Fils connection.
     *
     * @throws Exception
     */
    @Test
    public void testFilsConnection() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();
        WifiConfiguration config = setupFilsTest(true);
        setupFilsEnabledApInScanResult();

        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        prepareFilsHlpPktAndSendStartConnect();

        verify(mWifiMetrics, times(1)).incrementConnectRequestWithFilsAkmCount();

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 1, sBSSID);
        mLooper.dispatchAll();

        verify(mWifiMetrics, times(1)).incrementL2ConnectionThroughFilsAuthCount();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, WifiSsid.createFromAsciiEncoded(sFilsSsid),
                sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());

        DhcpResultsParcelable dhcpResults = new DhcpResultsParcelable();
        dhcpResults.baseConfiguration = new StaticIpConfiguration();
        dhcpResults.baseConfiguration.gateway = InetAddresses.parseNumericAddress("1.2.3.4");
        dhcpResults.baseConfiguration.ipAddress =
                new LinkAddress(InetAddresses.parseNumericAddress("192.168.1.100"), 0);
        dhcpResults.baseConfiguration.dnsServers.add(InetAddresses.parseNumericAddress("8.8.8.8"));
        dhcpResults.leaseDuration = 3600;

        injectDhcpSuccess(dhcpResults);
        mLooper.dispatchAll();

        WifiInfo wifiInfo = mCmi.getWifiInfo();
        assertNotNull(wifiInfo);
        assertEquals(sBSSID, wifiInfo.getBSSID());
        assertTrue(WifiSsid.createFromAsciiEncoded(sFilsSsid).equals(wifiInfo.getWifiSsid()));
        assertEquals("ConnectedState", getCurrentState().getName());
    }

    /**
     * Tests the wifi info is updated correctly for connecting network.
     */
    @Test
    public void testWifiInfoOnConnectingNextNetwork() throws Exception {

        mConnectedNetwork.ephemeral = true;
        mConnectedNetwork.trusted = true;
        mConnectedNetwork.osu = true;

        triggerConnect();
        when(mWifiConfigManager.getScanDetailCacheForNetwork(FRAMEWORK_NETWORK_ID))
                .thenReturn(mScanDetailCache);

        when(mScanDetailCache.getScanDetail(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq));
        when(mScanDetailCache.getScanResult(sBSSID)).thenReturn(
                getGoogleGuestScanDetail(TEST_RSSI, sBSSID, sFreq).getScanResult());

        // before the fist success connection, there is no valid wifi info.
        assertEquals(WifiConfiguration.INVALID_NETWORK_ID, mCmi.getWifiInfo().getNetworkId());

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(FRAMEWORK_NETWORK_ID,
                    sWifiSsid, sBSSID, SupplicantState.ASSOCIATED));
        mLooper.dispatchAll();

        // retrieve correct wifi info on receiving the supplicant state change event.
        assertEquals(FRAMEWORK_NETWORK_ID, mCmi.getWifiInfo().getNetworkId());
        assertEquals(mConnectedNetwork.ephemeral, mCmi.getWifiInfo().isEphemeral());
        assertEquals(mConnectedNetwork.trusted, mCmi.getWifiInfo().isTrusted());
        assertEquals(mConnectedNetwork.osu, mCmi.getWifiInfo().isOsuAp());
    }

    /**
     * Verify that we disconnect when we mark a previous unmetered network metered.
     */
    @Test
    public void verifyDisconnectOnMarkingNetworkMetered() throws Exception {
        connect();
        expectRegisterNetworkAgent((config) -> { }, (cap) -> {
            assertTrue(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });

        WifiConfiguration oldConfig = new WifiConfiguration(mConnectedNetwork);
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_METERED;

        mConfigUpdateListenerCaptor.getValue().onNetworkUpdated(mConnectedNetwork, oldConfig);
        mLooper.dispatchAll();
        assertEquals("DisconnectingState", getCurrentState().getName());
    }

    /**
     * Verify that we only update capabilites when we mark a previous unmetered network metered.
     */
    @Test
    public void verifyUpdateCapabilitiesOnMarkingNetworkUnmetered() throws Exception {
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_METERED;
        connect();
        expectRegisterNetworkAgent((config) -> { }, (cap) -> {
            assertFalse(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });
        reset(mNetworkAgentHandler);

        WifiConfiguration oldConfig = new WifiConfiguration(mConnectedNetwork);
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_NOT_METERED;

        mConfigUpdateListenerCaptor.getValue().onNetworkUpdated(mConnectedNetwork, oldConfig);
        mLooper.dispatchAll();
        assertEquals("ConnectedState", getCurrentState().getName());

        expectNetworkAgentUpdateCapabilities((cap) -> {
            assertTrue(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });
    }


    /**
     * Verify that we disconnect when we mark a previous unmetered network metered.
     */
    @Test
    public void verifyDisconnectOnMarkingNetworkAutoMeteredWithMeteredHint() throws Exception {
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_NOT_METERED;
        connect();
        expectRegisterNetworkAgent((config) -> { }, (cap) -> {
            assertTrue(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });
        reset(mNetworkAgentHandler);

        // Mark network metered none.
        WifiConfiguration oldConfig = new WifiConfiguration(mConnectedNetwork);
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_NONE;

        // Set metered hint in WifiInfo (either via DHCP or ScanResult IE).
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        wifiInfo.setMeteredHint(true);

        mConfigUpdateListenerCaptor.getValue().onNetworkUpdated(mConnectedNetwork, oldConfig);
        mLooper.dispatchAll();
        assertEquals("DisconnectingState", getCurrentState().getName());
    }

    /**
     * Verify that we only update capabilites when we mark a previous unmetered network metered.
     */
    @Test
    public void verifyUpdateCapabilitiesOnMarkingNetworkAutoMeteredWithoutMeteredHint()
            throws Exception {
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_METERED;
        connect();
        expectRegisterNetworkAgent((config) -> { }, (cap) -> {
            assertFalse(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });
        reset(mNetworkAgentHandler);

        WifiConfiguration oldConfig = new WifiConfiguration(mConnectedNetwork);
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_NONE;

        // Reset metered hint in WifiInfo.
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        wifiInfo.setMeteredHint(false);

        mConfigUpdateListenerCaptor.getValue().onNetworkUpdated(mConnectedNetwork, oldConfig);
        mLooper.dispatchAll();
        assertEquals("ConnectedState", getCurrentState().getName());

        expectNetworkAgentUpdateCapabilities((cap) -> {
            assertTrue(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });
    }

    /**
     * Verify that we do nothing on no metered change.
     */
    @Test
    public void verifyDoNothingMarkingNetworkAutoMeteredWithMeteredHint() throws Exception {
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_METERED;
        connect();
        expectRegisterNetworkAgent((config) -> { }, (cap) -> {
            assertFalse(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });
        reset(mNetworkAgentHandler);

        // Mark network metered none.
        WifiConfiguration oldConfig = new WifiConfiguration(mConnectedNetwork);
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_NONE;

        // Set metered hint in WifiInfo (either via DHCP or ScanResult IE).
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        wifiInfo.setMeteredHint(true);

        mConfigUpdateListenerCaptor.getValue().onNetworkUpdated(mConnectedNetwork, oldConfig);
        mLooper.dispatchAll();
        assertEquals("ConnectedState", getCurrentState().getName());

        verifyNoMoreInteractions(mNetworkAgentHandler);
    }

    /**
     * Verify that we do nothing on no metered change.
     */
    @Test
    public void verifyDoNothingMarkingNetworkAutoMeteredWithoutMeteredHint() throws Exception {
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_NOT_METERED;
        connect();
        expectRegisterNetworkAgent((config) -> { }, (cap) -> {
            assertTrue(cap.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED));
        });
        reset(mNetworkAgentHandler);

        // Mark network metered none.
        WifiConfiguration oldConfig = new WifiConfiguration(mConnectedNetwork);
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_NONE;

        // Reset metered hint in WifiInfo.
        WifiInfo wifiInfo = mCmi.getWifiInfo();
        wifiInfo.setMeteredHint(false);

        mConfigUpdateListenerCaptor.getValue().onNetworkUpdated(mConnectedNetwork, oldConfig);
        mLooper.dispatchAll();
        assertEquals("ConnectedState", getCurrentState().getName());

        verifyNoMoreInteractions(mNetworkAgentHandler);
    }

    /*
     * Verify that network cached data is cleared correctly in
     * disconnected state.
     */
    @Test
    public void testNetworkCachedDataIsClearedCorrectlyInDisconnectedState() throws Exception {
        // Setup CONNECT_MODE & a WifiConfiguration
        initializeAndAddNetworkAndVerifySuccess();
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        // got UNSPECIFIED during this connection attempt
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 1, sBSSID);
        mLooper.dispatchAll();

        assertEquals("DisconnectedState", getCurrentState().getName());
        verify(mWifiNative, never()).removeNetworkCachedData(anyInt());

        // got 4WAY_HANDSHAKE_TIMEOUT during this connection attempt
        mCmi.sendMessage(ClientModeImpl.CMD_START_CONNECT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 15, sBSSID);
        mLooper.dispatchAll();

        assertEquals("DisconnectedState", getCurrentState().getName());
        verify(mWifiNative).removeNetworkCachedData(FRAMEWORK_NETWORK_ID);
    }

    /*
     * Verify that network cached data is cleared correctly in
     * disconnected state.
     */
    @Test
    public void testNetworkCachedDataIsClearedCorrectlyInObtainingIpState() throws Exception {
        initializeAndAddNetworkAndVerifySuccess();

        verify(mWifiNative).removeAllNetworks(WIFI_IFACE_NAME);

        IActionListener connectActionListener = mock(IActionListener.class);
        mCmi.connect(null, 0, mock(Binder.class), connectActionListener, 0, Binder.getCallingUid());
        mLooper.dispatchAll();
        verify(connectActionListener).onSuccess();

        verify(mWifiConfigManager).enableNetwork(eq(0), eq(true), anyInt(), any());

        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();

        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.COMPLETED));
        mLooper.dispatchAll();

        assertEquals("ObtainingIpState", getCurrentState().getName());

        // got 4WAY_HANDSHAKE_TIMEOUT during this connection attempt
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 15, sBSSID);
        mLooper.dispatchAll();

        verify(mWifiNative).removeNetworkCachedData(FRAMEWORK_NETWORK_ID);
    }

    /*
     * Verify that network cached data is NOT cleared in ConnectedState.
     */
    @Test
    public void testNetworkCachedDataIsClearedIf4WayHandshakeFailure() throws Exception {
        when(mWifiScoreCard.detectAbnormalDisconnection())
                .thenReturn(WifiHealthMonitor.REASON_SHORT_CONNECTION_NONLOCAL);
        InOrder inOrderWifiLockManager = inOrder(mWifiLockManager);
        connect();
        inOrderWifiLockManager.verify(mWifiLockManager).updateWifiClientConnected(true);

        // got 4WAY_HANDSHAKE_TIMEOUT
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 15, sBSSID);
        mLooper.dispatchAll();
        verify(mWifiNative, never()).removeNetworkCachedData(anyInt());
    }

    /**
     * Verify that network cached data is cleared on updating a network.
     */
    @Test
    public void testNetworkCachedDataIsClearedOnUpdatingNetwork() throws Exception {
        WifiConfiguration oldConfig = new WifiConfiguration(mConnectedNetwork);
        mConnectedNetwork.meteredOverride = METERED_OVERRIDE_METERED;

        mConfigUpdateListenerCaptor.getValue().onNetworkUpdated(mConnectedNetwork, oldConfig);
        mLooper.dispatchAll();
        verify(mWifiNative).removeNetworkCachedData(eq(oldConfig.networkId));
    }


    @Test
    public void testIpReachabilityLostAndRoamEventsRace() throws Exception {
        connect();
        expectRegisterNetworkAgent((agentConfig) -> { }, (cap) -> { });
        reset(mNetworkAgentHandler);

        // Trigger ip reachibility loss and ensure we trigger a disconnect & we're in
        // "DisconnectingState"
        mCmi.sendMessage(ClientModeImpl.CMD_IP_REACHABILITY_LOST);
        mLooper.dispatchAll();
        verify(mWifiNative).disconnect(any());
        assertEquals("DisconnectingState", getCurrentState().getName());

        // Now send a network connection (indicating a roam) event before we get the disconnect
        // event.
        mCmi.sendMessage(WifiMonitor.NETWORK_CONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        // ensure that we ignored the transient roam while we're disconnecting.
        assertEquals("DisconnectingState", getCurrentState().getName());
        verifyNoMoreInteractions(mNetworkAgentHandler);

        // Now send the disconnect event and ensure that we transition to "DisconnectedState".
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, 0, 0, sBSSID);
        mLooper.dispatchAll();
        assertEquals("DisconnectedState", getCurrentState().getName());
        expectUnregisterNetworkAgent();

        verifyNoMoreInteractions(mNetworkAgentHandler);
    }

    @Test
    public void testSyncGetCurrentNetwork() throws Exception {
        // syncGetCurrentNetwork() returns null when disconnected
        mLooper.startAutoDispatch();
        assertNull(mCmi.syncGetCurrentNetwork(mCmiAsyncChannel));
        mLooper.stopAutoDispatch();

        connect();

        // syncGetCurrentNetwork() returns non-null Network when connected
        mLooper.startAutoDispatch();
        assertEquals(mNetwork, mCmi.syncGetCurrentNetwork(mCmiAsyncChannel));
        mLooper.stopAutoDispatch();
    }

    @Test
    public void clearRequestingPackageNameInWifiInfoOnConnectionFailure() throws Exception {
        mConnectedNetwork.fromWifiNetworkSpecifier = true;
        mConnectedNetwork.ephemeral = true;
        mConnectedNetwork.creatorName = OP_PACKAGE_NAME;

        triggerConnect();

        // association completed
        mCmi.sendMessage(WifiMonitor.SUPPLICANT_STATE_CHANGE_EVENT, 0, 0,
                new StateChangeResult(0, sWifiSsid, sBSSID, SupplicantState.ASSOCIATED));
        mLooper.dispatchAll();

        assertTrue(mCmi.getWifiInfo().isEphemeral());
        assertEquals(OP_PACKAGE_NAME, mCmi.getWifiInfo().getRequestingPackageName());

        // fail the connection.
        mCmi.sendMessage(WifiMonitor.NETWORK_DISCONNECTION_EVENT, FRAMEWORK_NETWORK_ID, 0, sBSSID);
        mLooper.dispatchAll();

        assertFalse(mCmi.getWifiInfo().isEphemeral());
        assertNull(mCmi.getWifiInfo().getRequestingPackageName());
    }
}
