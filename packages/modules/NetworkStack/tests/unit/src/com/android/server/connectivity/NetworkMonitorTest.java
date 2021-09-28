/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.server.connectivity;

import static android.net.CaptivePortal.APP_RETURN_DISMISSED;
import static android.net.DnsResolver.TYPE_A;
import static android.net.DnsResolver.TYPE_AAAA;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_DNS;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_FALLBACK;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_HTTP;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_HTTPS;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_PRIVDNS;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_RESULT_PARTIAL;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_RESULT_VALID;
import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_METERED;
import static android.net.NetworkCapabilities.TRANSPORT_CELLULAR;
import static android.net.NetworkCapabilities.TRANSPORT_VPN;
import static android.net.NetworkCapabilities.TRANSPORT_WIFI;
import static android.net.metrics.ValidationProbeEvent.PROBE_HTTP;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_EVALUATION_TYPE;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_MIN_EVALUATE_INTERVAL;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_TCP_POLLING_INTERVAL;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_VALID_DNS_TIME_THRESHOLD;
import static android.net.util.DataStallUtils.DATA_STALL_EVALUATION_TYPE_DNS;
import static android.net.util.DataStallUtils.DATA_STALL_EVALUATION_TYPE_TCP;
import static android.net.util.DataStallUtils.DEFAULT_DATA_STALL_EVALUATION_TYPES;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_FALLBACK_PROBE_SPECS;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_OTHER_FALLBACK_URLS;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_USE_HTTPS;
import static android.net.util.NetworkStackUtils.DEFAULT_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT;
import static android.net.util.NetworkStackUtils.DISMISS_PORTAL_IN_VALIDATED_NETWORK;
import static android.net.util.NetworkStackUtils.DNS_PROBE_PRIVATE_IP_NO_INTERNET_VERSION;
import static android.net.util.NetworkStackUtils.TEST_CAPTIVE_PORTAL_HTTPS_URL;
import static android.net.util.NetworkStackUtils.TEST_CAPTIVE_PORTAL_HTTP_URL;
import static android.net.util.NetworkStackUtils.TEST_URL_EXPIRATION_TIME;
import static android.provider.DeviceConfig.NAMESPACE_CONNECTIVITY;

import static com.android.networkstack.util.DnsUtils.PRIVATE_DNS_PROBE_HOST_SUFFIX;
import static com.android.server.connectivity.NetworkMonitor.INITIAL_REEVALUATE_DELAY_MS;
import static com.android.server.connectivity.NetworkMonitor.extractCharset;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertFalse;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeFalse;
import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.after;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.atMost;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static java.lang.System.currentTimeMillis;
import static java.util.Collections.singletonList;
import static java.util.stream.Collectors.toList;

import android.annotation.NonNull;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.net.CaptivePortalData;
import android.net.ConnectivityManager;
import android.net.DataStallReportParcelable;
import android.net.DnsResolver;
import android.net.INetd;
import android.net.INetworkMonitorCallbacks;
import android.net.InetAddresses;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkTestResultParcelable;
import android.net.Uri;
import android.net.captiveportal.CaptivePortalProbeResult;
import android.net.metrics.IpConnectivityLog;
import android.net.shared.PrivateDnsConfig;
import android.net.util.SharedLog;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Process;
import android.os.RemoteException;
import android.os.SystemClock;
import android.provider.Settings;
import android.telephony.CellIdentityGsm;
import android.telephony.CellIdentityLte;
import android.telephony.CellInfo;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoLte;
import android.telephony.CellSignalStrength;
import android.telephony.TelephonyManager;
import android.util.ArrayMap;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.networkstack.NetworkStackNotifier;
import com.android.networkstack.R;
import com.android.networkstack.apishim.CaptivePortalDataShimImpl;
import com.android.networkstack.apishim.ConstantsShim;
import com.android.networkstack.apishim.common.ShimUtils;
import com.android.networkstack.metrics.DataStallDetectionStats;
import com.android.networkstack.metrics.DataStallStatsUtils;
import com.android.networkstack.netlink.TcpSocketTracker;
import com.android.server.NetworkStackService.NetworkStackServiceManager;
import com.android.server.connectivity.nano.CellularData;
import com.android.server.connectivity.nano.DnsEvent;
import com.android.server.connectivity.nano.WifiData;
import com.android.testutils.DevSdkIgnoreRule;
import com.android.testutils.DevSdkIgnoreRule.IgnoreUpTo;
import com.android.testutils.HandlerUtilsKt;

import com.google.protobuf.nano.MessageNano;

import junit.framework.AssertionFailedError;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.net.HttpURLConnection;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.URL;
import java.net.UnknownHostException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Random;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

import javax.net.ssl.SSLHandshakeException;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class NetworkMonitorTest {
    private static final String LOCATION_HEADER = "location";
    private static final String CONTENT_TYPE_HEADER = "Content-Type";

    @Rule
    public final DevSdkIgnoreRule mIgnoreRule = new DevSdkIgnoreRule();

    private @Mock Context mContext;
    private @Mock Configuration mConfiguration;
    private @Mock Resources mResources;
    private @Mock IpConnectivityLog mLogger;
    private @Mock SharedLog mValidationLogger;
    private @Mock DnsResolver mDnsResolver;
    private @Mock ConnectivityManager mCm;
    private @Mock TelephonyManager mTelephony;
    private @Mock WifiManager mWifi;
    private @Mock NetworkStackServiceManager mServiceManager;
    private @Mock NetworkStackNotifier mNotifier;
    private @Mock HttpURLConnection mHttpConnection;
    private @Mock HttpURLConnection mOtherHttpConnection1;
    private @Mock HttpURLConnection mOtherHttpConnection2;
    private @Mock HttpURLConnection mHttpsConnection;
    private @Mock HttpURLConnection mOtherHttpsConnection1;
    private @Mock HttpURLConnection mOtherHttpsConnection2;
    private @Mock HttpURLConnection mFallbackConnection;
    private @Mock HttpURLConnection mOtherFallbackConnection;
    private @Mock HttpURLConnection mTestOverriddenUrlConnection;
    private @Mock HttpURLConnection mCapportApiConnection;
    private @Mock HttpURLConnection mSpeedTestConnection;
    private @Mock Random mRandom;
    private @Mock NetworkMonitor.Dependencies mDependencies;
    // Mockito can't create a mock of INetworkMonitorCallbacks on Q because it can't find
    // CaptivePortalData on notifyCaptivePortalDataChanged. Use a spy on a mock IBinder instead.
    private INetworkMonitorCallbacks mCallbacks = spy(
            INetworkMonitorCallbacks.Stub.asInterface(mock(IBinder.class)));
    private @Spy Network mCleartextDnsNetwork = new Network(TEST_NETID);
    private @Mock Network mNetwork;
    private @Mock DataStallStatsUtils mDataStallStatsUtils;
    private @Mock TcpSocketTracker.Dependencies mTstDependencies;
    private @Mock INetd mNetd;
    private @Mock TcpSocketTracker mTst;
    private HashSet<WrappedNetworkMonitor> mCreatedNetworkMonitors;
    private HashSet<BroadcastReceiver> mRegisteredReceivers;
    private @Mock Context mMccContext;
    private @Mock Resources mMccResource;
    private @Mock WifiInfo mWifiInfo;

    private static final int TEST_NETID = 4242;
    private static final String TEST_HTTP_URL = "http://www.google.com/gen_204";
    private static final String TEST_HTTP_OTHER_URL1 = "http://other1.google.com/gen_204";
    private static final String TEST_HTTP_OTHER_URL2 = "http://other2.google.com/gen_204";
    private static final String TEST_HTTPS_URL = "https://www.google.com/gen_204";
    private static final String TEST_HTTPS_OTHER_URL1 = "https://other1.google.com/gen_204";
    private static final String TEST_HTTPS_OTHER_URL2 = "https://other2.google.com/gen_204";
    private static final String TEST_FALLBACK_URL = "http://fallback.google.com/gen_204";
    private static final String TEST_OTHER_FALLBACK_URL = "http://otherfallback.google.com/gen_204";
    private static final String TEST_INVALID_OVERRIDE_URL = "https://override.example.com/test";
    private static final String TEST_OVERRIDE_URL = "http://localhost:12345/test";
    private static final String TEST_CAPPORT_API_URL = "https://capport.example.com/api";
    private static final String TEST_LOGIN_URL = "https://testportal.example.com/login";
    private static final String TEST_VENUE_INFO_URL = "https://venue.example.com/info";
    private static final String TEST_SPEED_TEST_URL = "https://speedtest.example.com";
    private static final String TEST_RELATIVE_URL = "/test/relative/gen_204";
    private static final String TEST_MCCMNC = "123456";
    private static final String[] TEST_HTTP_URLS = {TEST_HTTP_OTHER_URL1, TEST_HTTP_OTHER_URL2};
    private static final String[] TEST_HTTPS_URLS = {TEST_HTTPS_OTHER_URL1, TEST_HTTPS_OTHER_URL2};
    private static final int TEST_TCP_FAIL_RATE = 99;
    private static final int TEST_TCP_PACKET_COUNT = 50;
    private static final long TEST_ELAPSED_TIME_MS = 123456789L;
    private static final int TEST_SIGNAL_STRENGTH = -100;
    private static final int VALIDATION_RESULT_INVALID = 0;
    private static final int VALIDATION_RESULT_PORTAL = 0;
    private static final String TEST_REDIRECT_URL = "android.com";
    private static final int PROBES_PRIVDNS_VALID = NETWORK_VALIDATION_PROBE_DNS
            | NETWORK_VALIDATION_PROBE_HTTPS | NETWORK_VALIDATION_PROBE_PRIVDNS;

    private static final int RETURN_CODE_DNS_SUCCESS = 0;
    private static final int RETURN_CODE_DNS_TIMEOUT = 255;
    private static final int DEFAULT_DNS_TIMEOUT_THRESHOLD = 5;

    private static final int HANDLER_TIMEOUT_MS = 1000;

    private static final LinkProperties TEST_LINK_PROPERTIES = new LinkProperties();

    // Cannot have a static member for the LinkProperties with captive portal API information, as
    // the initializer would crash on Q (the members in LinkProperties were introduced in R).
    private static LinkProperties makeCapportLPs() {
        final LinkProperties lp = new LinkProperties(TEST_LINK_PROPERTIES);
        lp.setCaptivePortalApiUrl(Uri.parse(TEST_CAPPORT_API_URL));
        return lp;
    }

    private static final NetworkCapabilities CELL_METERED_CAPABILITIES = new NetworkCapabilities()
            .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
            .addCapability(NET_CAPABILITY_INTERNET);

    private static final NetworkCapabilities CELL_NOT_METERED_CAPABILITIES =
            new NetworkCapabilities()
                .addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR)
                .addCapability(NET_CAPABILITY_INTERNET)
                .addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED);

    private static final NetworkCapabilities WIFI_NOT_METERED_CAPABILITIES =
            new NetworkCapabilities()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .addCapability(NET_CAPABILITY_INTERNET)
                .addCapability(NetworkCapabilities.NET_CAPABILITY_NOT_METERED);

    private static final NetworkCapabilities CELL_NO_INTERNET_CAPABILITIES =
            new NetworkCapabilities().addTransportType(NetworkCapabilities.TRANSPORT_CELLULAR);

    /**
     * Fakes DNS responses.
     *
     * Allows test methods to configure the IP addresses that will be resolved by
     * Network#getAllByName and by DnsResolver#query.
     */
    class FakeDns {
        /** Data class to record the Dns entry. */
        class DnsEntry {
            final String mHostname;
            final int mType;
            final List<InetAddress> mAddresses;
            DnsEntry(String host, int type, List<InetAddress> addr) {
                mHostname = host;
                mType = type;
                mAddresses = addr;
            }
            // Full match or partial match that target host contains the entry hostname to support
            // random private dns probe hostname.
            private boolean matches(String hostname, int type) {
                return hostname.endsWith(mHostname) && type == mType;
            }
        }
        private final ArrayList<DnsEntry> mAnswers = new ArrayList<DnsEntry>();
        private boolean mNonBypassPrivateDnsWorking = true;

        /** Whether DNS queries on mNonBypassPrivateDnsWorking should succeed. */
        private void setNonBypassPrivateDnsWorking(boolean working) {
            mNonBypassPrivateDnsWorking = working;
        }

        /** Clears all DNS entries. */
        private synchronized void clearAll() {
            mAnswers.clear();
        }

        /** Returns the answer for a given name and type on the given mock network. */
        private synchronized List<InetAddress> getAnswer(Object mock, String hostname, int type) {
            if (mock == mNetwork && !mNonBypassPrivateDnsWorking) {
                return null;
            }

            return mAnswers.stream().filter(e -> e.matches(hostname, type))
                    .map(answer -> answer.mAddresses).findFirst().orElse(null);
        }

        /** Sets the answer for a given name and type. */
        private synchronized void setAnswer(String hostname, String[] answer, int type)
                throws UnknownHostException {
            DnsEntry record = new DnsEntry(hostname, type, generateAnswer(answer));
            // Remove the existing one.
            mAnswers.removeIf(entry -> entry.matches(hostname, type));
            // Add or replace a new record.
            mAnswers.add(record);
        }

        private List<InetAddress> generateAnswer(String[] answer) {
            if (answer == null) return new ArrayList<>();
            return Arrays.stream(answer).map(addr -> InetAddress.parseNumericAddress(addr))
                    .collect(toList());
        }

        /** Simulates a getAllByName call for the specified name on the specified mock network. */
        private InetAddress[] getAllByName(Object mock, String hostname)
                throws UnknownHostException {
            List<InetAddress> answer = queryAllTypes(mock, hostname);
            if (answer == null || answer.size() == 0) {
                throw new UnknownHostException(hostname);
            }
            return answer.toArray(new InetAddress[0]);
        }

        // Regardless of the type, depends on what the responses contained in the network.
        private List<InetAddress> queryAllTypes(Object mock, String hostname) {
            List<InetAddress> answer = new ArrayList<>();
            addAllIfNotNull(answer, getAnswer(mock, hostname, TYPE_A));
            addAllIfNotNull(answer, getAnswer(mock, hostname, TYPE_AAAA));
            return answer;
        }

        private void addAllIfNotNull(List<InetAddress> list, List<InetAddress> c) {
            if (c != null) {
                list.addAll(c);
            }
        }

        /** Starts mocking DNS queries. */
        private void startMocking() throws UnknownHostException {
            // Queries on mNetwork using getAllByName.
            doAnswer(invocation -> {
                return getAllByName(invocation.getMock(), invocation.getArgument(0));
            }).when(mNetwork).getAllByName(any());

            // Queries on mCleartextDnsNetwork using DnsResolver#query.
            doAnswer(invocation -> {
                return mockQuery(invocation, 1 /* posHostname */, 3 /* posExecutor */,
                        5 /* posCallback */, -1 /* posType */);
            }).when(mDnsResolver).query(any(), any(), anyInt(), any(), any(), any());

            // Queries on mCleartextDnsNetwork using DnsResolver#query with QueryType.
            doAnswer(invocation -> {
                return mockQuery(invocation, 1 /* posHostname */, 4 /* posExecutor */,
                        6 /* posCallback */, 2 /* posType */);
            }).when(mDnsResolver).query(any(), any(), anyInt(), anyInt(), any(), any(), any());
        }

        // Mocking queries on DnsResolver#query.
        private Answer mockQuery(InvocationOnMock invocation, int posHostname, int posExecutor,
                int posCallback, int posType) {
            String hostname = (String) invocation.getArgument(posHostname);
            Executor executor = (Executor) invocation.getArgument(posExecutor);
            DnsResolver.Callback<List<InetAddress>> callback = invocation.getArgument(posCallback);
            List<InetAddress> answer;

            answer = posType != -1
                    ? getAnswer(invocation.getMock(), hostname, invocation.getArgument(posType)) :
                    queryAllTypes(invocation.getMock(), hostname);

            if (answer != null && answer.size() > 0) {
                new Handler(Looper.getMainLooper()).post(() -> {
                    executor.execute(() -> callback.onAnswer(answer, 0));
                });
            }
            // If no answers, do nothing. sendDnsProbeWithTimeout will time out and throw UHE.
            return null;
        }
    }

    private FakeDns mFakeDns;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        when(mDependencies.getPrivateDnsBypassNetwork(any())).thenReturn(mCleartextDnsNetwork);
        when(mDependencies.getDnsResolver()).thenReturn(mDnsResolver);
        when(mDependencies.getRandom()).thenReturn(mRandom);
        when(mDependencies.getSetting(any(), eq(Settings.Global.CAPTIVE_PORTAL_MODE), anyInt()))
                .thenReturn(Settings.Global.CAPTIVE_PORTAL_MODE_PROMPT);
        when(mDependencies.getDeviceConfigPropertyInt(any(), eq(CAPTIVE_PORTAL_USE_HTTPS),
                anyInt())).thenReturn(1);
        when(mDependencies.getSetting(any(), eq(Settings.Global.CAPTIVE_PORTAL_HTTP_URL), any()))
                .thenReturn(TEST_HTTP_URL);
        when(mDependencies.getSetting(any(), eq(Settings.Global.CAPTIVE_PORTAL_HTTPS_URL), any()))
                .thenReturn(TEST_HTTPS_URL);

        doReturn(mCleartextDnsNetwork).when(mNetwork).getPrivateDnsBypassingCopy();

        when(mContext.getSystemService(Context.CONNECTIVITY_SERVICE)).thenReturn(mCm);
        when(mContext.getSystemService(Context.TELEPHONY_SERVICE)).thenReturn(mTelephony);
        when(mContext.getSystemService(Context.WIFI_SERVICE)).thenReturn(mWifi);
        when(mContext.getResources()).thenReturn(mResources);

        when(mServiceManager.getNotifier()).thenReturn(mNotifier);

        when(mTelephony.getDataNetworkType()).thenReturn(TelephonyManager.NETWORK_TYPE_LTE);
        when(mTelephony.getNetworkOperator()).thenReturn(TEST_MCCMNC);
        when(mTelephony.getSimOperator()).thenReturn(TEST_MCCMNC);

        when(mResources.getString(anyInt())).thenReturn("");
        when(mResources.getStringArray(anyInt())).thenReturn(new String[0]);
        doReturn(mConfiguration).when(mResources).getConfiguration();
        when(mMccContext.getResources()).thenReturn(mMccResource);

        setFallbackUrl(TEST_FALLBACK_URL);
        setOtherFallbackUrls(TEST_OTHER_FALLBACK_URL);
        setFallbackSpecs(null); // Test with no fallback spec by default
        when(mRandom.nextInt()).thenReturn(0);

        when(mTstDependencies.getNetd()).thenReturn(mNetd);
        // DNS probe timeout should not be defined more than half of HANDLER_TIMEOUT_MS. Otherwise,
        // it will fail the test because of timeout expired for querying AAAA and A sequentially.
        when(mResources.getInteger(eq(R.integer.config_captive_portal_dns_probe_timeout)))
                .thenReturn(200);

        doAnswer((invocation) -> {
            URL url = invocation.getArgument(0);
            switch(url.toString()) {
                case TEST_HTTP_URL:
                    return mHttpConnection;
                case TEST_HTTP_OTHER_URL1:
                    return mOtherHttpConnection1;
                case TEST_HTTP_OTHER_URL2:
                    return mOtherHttpConnection2;
                case TEST_HTTPS_URL:
                    return mHttpsConnection;
                case TEST_HTTPS_OTHER_URL1:
                    return mOtherHttpsConnection1;
                case TEST_HTTPS_OTHER_URL2:
                    return mOtherHttpsConnection2;
                case TEST_FALLBACK_URL:
                    return mFallbackConnection;
                case TEST_OTHER_FALLBACK_URL:
                    return mOtherFallbackConnection;
                case TEST_OVERRIDE_URL:
                case TEST_INVALID_OVERRIDE_URL:
                    return mTestOverriddenUrlConnection;
                case TEST_CAPPORT_API_URL:
                    return mCapportApiConnection;
                case TEST_SPEED_TEST_URL:
                    return mSpeedTestConnection;
                default:
                    fail("URL not mocked: " + url.toString());
                    return null;
            }
        }).when(mCleartextDnsNetwork).openConnection(any());
        when(mHttpConnection.getRequestProperties()).thenReturn(new ArrayMap<>());
        when(mHttpsConnection.getRequestProperties()).thenReturn(new ArrayMap<>());

        mFakeDns = new FakeDns();
        mFakeDns.startMocking();
        // Set private dns suffix answer. sendPrivateDnsProbe() in NetworkMonitor send probe with
        // one time hostname. The hostname will be [random generated UUID] + HOST_SUFFIX differently
        // each time. That means the host answer cannot be pre-set into the answer list. Thus, set
        // the host suffix and use partial match in FakeDns to match the target host and reply the
        // intended answer.
        mFakeDns.setAnswer(PRIVATE_DNS_PROBE_HOST_SUFFIX, new String[]{"192.0.2.2"}, TYPE_A);
        mFakeDns.setAnswer(PRIVATE_DNS_PROBE_HOST_SUFFIX, new String[]{"2001:db8::1"}, TYPE_AAAA);

        when(mContext.registerReceiver(any(BroadcastReceiver.class), any())).then((invocation) -> {
            mRegisteredReceivers.add(invocation.getArgument(0));
            return new Intent();
        });

        doAnswer((invocation) -> {
            mRegisteredReceivers.remove(invocation.getArgument(0));
            return null;
        }).when(mContext).unregisterReceiver(any());

        resetCallbacks();

        setMinDataStallEvaluateInterval(500);
        setDataStallEvaluationType(DATA_STALL_EVALUATION_TYPE_DNS);
        setValidDataStallDnsTimeThreshold(500);
        setConsecutiveDnsTimeoutThreshold(5);
        mCreatedNetworkMonitors = new HashSet<>();
        mRegisteredReceivers = new HashSet<>();
        setDismissPortalInValidatedNetwork(false);
    }

    @After
    public void tearDown() {
        mFakeDns.clearAll();
        // Make a local copy of mCreatedNetworkMonitors because during the iteration below,
        // WrappedNetworkMonitor#onQuitting will delete elements from it on the handler threads.
        WrappedNetworkMonitor[] networkMonitors = mCreatedNetworkMonitors.toArray(
                new WrappedNetworkMonitor[0]);
        for (WrappedNetworkMonitor nm : networkMonitors) {
            nm.notifyNetworkDisconnected();
            nm.awaitQuit();
        }
        assertEquals("NetworkMonitor still running after disconnect",
                0, mCreatedNetworkMonitors.size());
        assertEquals("BroadcastReceiver still registered after disconnect",
                0, mRegisteredReceivers.size());
    }

    private void resetCallbacks() {
        resetCallbacks(6);
    }

    private void resetCallbacks(int interfaceVersion) {
        reset(mCallbacks);
        try {
            doReturn(interfaceVersion).when(mCallbacks).getInterfaceVersion();
        } catch (RemoteException e) {
            // Can't happen as mCallbacks is a mock
            fail("Error mocking getInterfaceVersion" + e);
        }
    }

    private TcpSocketTracker getTcpSocketTrackerOrNull(NetworkMonitor.Dependencies dp) {
        return ((dp.getDeviceConfigPropertyInt(
                NAMESPACE_CONNECTIVITY,
                CONFIG_DATA_STALL_EVALUATION_TYPE,
                DEFAULT_DATA_STALL_EVALUATION_TYPES)
                & DATA_STALL_EVALUATION_TYPE_TCP) != 0) ? mTst : null;
    }

    private class WrappedNetworkMonitor extends NetworkMonitor {
        private long mProbeTime = 0;
        private final ConditionVariable mQuitCv = new ConditionVariable(false);

        WrappedNetworkMonitor() {
            super(mContext, mCallbacks, mNetwork, mLogger, mValidationLogger, mServiceManager,
                    mDependencies, getTcpSocketTrackerOrNull(mDependencies));
        }

        @Override
        protected long getLastProbeTime() {
            return mProbeTime;
        }

        protected void setLastProbeTime(long time) {
            mProbeTime = time;
        }

        @Override
        protected void addDnsEvents(@NonNull final DataStallDetectionStats.Builder stats) {
            if ((getDataStallEvaluationType() & DATA_STALL_EVALUATION_TYPE_DNS) != 0) {
                generateTimeoutDnsEvent(stats, DEFAULT_DNS_TIMEOUT_THRESHOLD);
            }
        }

        @Override
        protected void onQuitting() {
            assertTrue(mCreatedNetworkMonitors.remove(this));
            mQuitCv.open();
        }

        protected void awaitQuit() {
            assertTrue("NetworkMonitor did not quit after " + HANDLER_TIMEOUT_MS + "ms",
                    mQuitCv.block(HANDLER_TIMEOUT_MS));
        }

        protected Context getContext() {
            return mContext;
        }
    }

    private WrappedNetworkMonitor makeMonitor(NetworkCapabilities nc) {
        final WrappedNetworkMonitor nm = new WrappedNetworkMonitor();
        nm.start();
        setNetworkCapabilities(nm, nc);
        HandlerUtilsKt.waitForIdle(nm.getHandler(), HANDLER_TIMEOUT_MS);
        mCreatedNetworkMonitors.add(nm);
        when(mTstDependencies.isTcpInfoParsingSupported()).thenReturn(false);

        return nm;
    }

    private WrappedNetworkMonitor makeCellMeteredNetworkMonitor() {
        final WrappedNetworkMonitor nm = makeMonitor(CELL_METERED_CAPABILITIES);
        return nm;
    }

    private WrappedNetworkMonitor makeCellNotMeteredNetworkMonitor() {
        final WrappedNetworkMonitor nm = makeMonitor(CELL_NOT_METERED_CAPABILITIES);
        return nm;
    }

    private WrappedNetworkMonitor makeWifiNotMeteredNetworkMonitor() {
        final WrappedNetworkMonitor nm = makeMonitor(WIFI_NOT_METERED_CAPABILITIES);
        return nm;
    }

    private void setNetworkCapabilities(NetworkMonitor nm, NetworkCapabilities nc) {
        nm.notifyNetworkCapabilitiesChanged(nc);
        HandlerUtilsKt.waitForIdle(nm.getHandler(), HANDLER_TIMEOUT_MS);
    }

    @Test
    public void testOnlyWifiTransport() {
        final WrappedNetworkMonitor wnm = makeCellMeteredNetworkMonitor();
        final NetworkCapabilities nc = new NetworkCapabilities()
                .addTransportType(TRANSPORT_WIFI)
                .addTransportType(TRANSPORT_VPN);
        setNetworkCapabilities(wnm, nc);
        assertFalse(wnm.onlyWifiTransport());
        nc.removeTransportType(TRANSPORT_VPN);
        setNetworkCapabilities(wnm, nc);
        assertTrue(wnm.onlyWifiTransport());
    }

    @Test
    public void testNeedEvaluatingBandwidth() throws Exception {
        // Make metered network first, the transport type is TRANSPORT_CELLULAR. That means the
        // test cannot pass the condition check in needEvaluatingBandwidth().
        final WrappedNetworkMonitor wnm1 = makeCellMeteredNetworkMonitor();
        // Don't set the config_evaluating_bandwidth_url to make
        // the condition check fail in needEvaluatingBandwidth().
        assertFalse(wnm1.needEvaluatingBandwidth());
        // Make the NetworkCapabilities to have the TRANSPORT_WIFI but it still cannot meet the
        // condition check.
        final NetworkCapabilities nc = new NetworkCapabilities()
                .addTransportType(TRANSPORT_WIFI);
        setNetworkCapabilities(wnm1, nc);
        assertFalse(wnm1.needEvaluatingBandwidth());
        // Make the network to be non-metered wifi but it still cannot meet the condition check
        // since the config_evaluating_bandwidth_url is not set.
        nc.addCapability(NET_CAPABILITY_NOT_METERED);
        setNetworkCapabilities(wnm1, nc);
        assertFalse(wnm1.needEvaluatingBandwidth());
        // All configurations are set correctly.
        doReturn(TEST_SPEED_TEST_URL).when(mResources).getString(
                R.string.config_evaluating_bandwidth_url);
        final WrappedNetworkMonitor wnm2 = makeCellMeteredNetworkMonitor();
        setNetworkCapabilities(wnm2, nc);
        assertTrue(wnm2.needEvaluatingBandwidth());
        // Set mIsBandwidthCheckPassedOrIgnored to true and expect needEvaluatingBandwidth() will
        // return false.
        wnm2.mIsBandwidthCheckPassedOrIgnored = true;
        assertFalse(wnm2.needEvaluatingBandwidth());
        // Reset mIsBandwidthCheckPassedOrIgnored back to false.
        wnm2.mIsBandwidthCheckPassedOrIgnored = false;
        // Shouldn't evaluate network bandwidth on the metered wifi.
        nc.removeCapability(NET_CAPABILITY_NOT_METERED);
        setNetworkCapabilities(wnm2, nc);
        assertFalse(wnm2.needEvaluatingBandwidth());
        // Shouldn't evaluate network bandwidth on the unmetered cellular.
        nc.addCapability(NET_CAPABILITY_NOT_METERED);
        nc.removeTransportType(TRANSPORT_WIFI);
        nc.addTransportType(TRANSPORT_CELLULAR);
        assertFalse(wnm2.needEvaluatingBandwidth());
    }

    @Test
    public void testEvaluatingBandwidthState_meteredNetwork() throws Exception {
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);
        final NetworkCapabilities meteredCap = new NetworkCapabilities()
                .addTransportType(TRANSPORT_WIFI)
                .addCapability(NET_CAPABILITY_INTERNET);
        doReturn(TEST_SPEED_TEST_URL).when(mResources).getString(
                R.string.config_evaluating_bandwidth_url);
        final NetworkMonitor nm = runNetworkTest(TEST_LINK_PROPERTIES, meteredCap,
                NETWORK_VALIDATION_RESULT_VALID, NETWORK_VALIDATION_PROBE_DNS
                | NETWORK_VALIDATION_PROBE_HTTPS, null /* redirectUrl */);
        // Evaluating bandwidth process won't be executed when the network is metered wifi.
        // Check that the connection hasn't been opened and the state should transition to validated
        // state directly.
        verify(mCleartextDnsNetwork, never()).openConnection(new URL(TEST_SPEED_TEST_URL));
        assertEquals(NETWORK_VALIDATION_RESULT_VALID,
                nm.getEvaluationState().getEvaluationResult());
    }

    @Test
    public void testEvaluatingBandwidthState_nonMeteredNetworkWithWrongConfig() throws Exception {
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);
        final NetworkCapabilities nonMeteredCap = new NetworkCapabilities()
                .addTransportType(TRANSPORT_WIFI)
                .addCapability(NET_CAPABILITY_INTERNET)
                .addCapability(NET_CAPABILITY_NOT_METERED);
        doReturn("").when(mResources).getString(R.string.config_evaluating_bandwidth_url);
        final NetworkMonitor nm = runNetworkTest(TEST_LINK_PROPERTIES, nonMeteredCap,
                NETWORK_VALIDATION_RESULT_VALID, NETWORK_VALIDATION_PROBE_DNS
                | NETWORK_VALIDATION_PROBE_HTTPS, null /* redirectUrl */);
        // Non-metered network with wrong configuration(the config_evaluating_bandwidth_url is
        // empty). Check that the connection hasn't been opened and the state should transition to
        // validated state directly.
        verify(mCleartextDnsNetwork, never()).openConnection(new URL(TEST_SPEED_TEST_URL));
        assertEquals(NETWORK_VALIDATION_RESULT_VALID,
                nm.getEvaluationState().getEvaluationResult());
    }

    @Test
    public void testMatchesHttpContent() throws Exception {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        doReturn("[\\s\\S]*line2[\\s\\S]*").when(mResources).getString(
                R.string.config_network_validation_failed_content_regexp);
        assertTrue(wnm.matchesHttpContent("This is line1\nThis is line2\nThis is line3",
                R.string.config_network_validation_failed_content_regexp));
        assertFalse(wnm.matchesHttpContent("hello",
                R.string.config_network_validation_failed_content_regexp));
        // Set an invalid regex and expect to get the false even though the regex is the same as the
        // content.
        doReturn("[").when(mResources).getString(
                R.string.config_network_validation_failed_content_regexp);
        assertFalse(wnm.matchesHttpContent("[",
                R.string.config_network_validation_failed_content_regexp));
    }

    @Test
    public void testMatchesHttpContentLength() throws Exception {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        // Set the range of content length.
        doReturn(100).when(mResources).getInteger(R.integer.config_min_matches_http_content_length);
        doReturn(1000).when(mResources).getInteger(
                R.integer.config_max_matches_http_content_length);
        assertFalse(wnm.matchesHttpContentLength(100));
        assertFalse(wnm.matchesHttpContentLength(1000));
        assertTrue(wnm.matchesHttpContentLength(500));

        // Test the invalid value.
        assertFalse(wnm.matchesHttpContentLength(-1));
        assertFalse(wnm.matchesHttpContentLength(0));
        assertFalse(wnm.matchesHttpContentLength(Integer.MAX_VALUE + 1L));

        // Set the wrong value for min and max config to make sure the function is working even
        // though the config is wrong.
        doReturn(1000).when(mResources).getInteger(
                R.integer.config_min_matches_http_content_length);
        doReturn(100).when(mResources).getInteger(
                R.integer.config_max_matches_http_content_length);
        assertFalse(wnm.matchesHttpContentLength(100));
        assertFalse(wnm.matchesHttpContentLength(1000));
        assertFalse(wnm.matchesHttpContentLength(500));
    }

    @Test
    public void testGetResStringConfig() throws Exception {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        // Set the config and expect to get the customized value.
        final String regExp = ".*HTTP.*200.*not a captive portal.*";
        doReturn(regExp).when(mResources).getString(
                R.string.config_network_validation_failed_content_regexp);
        assertEquals(regExp, wnm.getResStringConfig(mContext,
                R.string.config_network_validation_failed_content_regexp, null));
        doThrow(new Resources.NotFoundException()).when(mResources).getString(eq(
                R.string.config_network_validation_failed_content_regexp));
        // If the config is not found, then expect to get the default value - null.
        assertNull(wnm.getResStringConfig(mContext,
                R.string.config_network_validation_failed_content_regexp, null));
    }

    @Test
    public void testGetResIntConfig() throws Exception {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        // Set the config and expect to get the customized value.
        doReturn(100).when(mResources).getInteger(R.integer.config_min_matches_http_content_length);
        doReturn(1000).when(mResources).getInteger(
                R.integer.config_max_matches_http_content_length);
        assertEquals(100, wnm.getResIntConfig(mContext,
                R.integer.config_min_matches_http_content_length, Integer.MAX_VALUE));
        assertEquals(1000, wnm.getResIntConfig(mContext,
                R.integer.config_max_matches_http_content_length, 0));
        doThrow(new Resources.NotFoundException())
                .when(mResources).getInteger(
                        eq(R.integer.config_min_matches_http_content_length));
        doThrow(new Resources.NotFoundException())
                .when(mResources).getInteger(eq(R.integer.config_max_matches_http_content_length));
        // If the config is not found, then expect to get the default value.
        assertEquals(Integer.MAX_VALUE, wnm.getResIntConfig(mContext,
                R.integer.config_min_matches_http_content_length, Integer.MAX_VALUE));
        assertEquals(0, wnm.getResIntConfig(mContext,
                R.integer.config_max_matches_http_content_length, 0));
    }

    @Test
    public void testGetHttpProbeUrl() {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        // If config_captive_portal_http_url is set and the global setting is set, the config is
        // used.
        doReturn(TEST_HTTP_URL).when(mResources).getString(R.string.config_captive_portal_http_url);
        doReturn(TEST_HTTP_OTHER_URL2).when(mResources).getString(
                R.string.default_captive_portal_http_url);
        when(mDependencies.getSetting(any(), eq(Settings.Global.CAPTIVE_PORTAL_HTTP_URL), any()))
                .thenReturn(TEST_HTTP_OTHER_URL1);
        assertEquals(TEST_HTTP_URL, wnm.getCaptivePortalServerHttpUrl());
        // If config_captive_portal_http_url is unset and the global setting is set, the global
        // setting is used.
        doReturn(null).when(mResources).getString(R.string.config_captive_portal_http_url);
        assertEquals(TEST_HTTP_OTHER_URL1, wnm.getCaptivePortalServerHttpUrl());
        // If both config_captive_portal_http_url and global setting are unset,
        // default_captive_portal_http_url is used.
        when(mDependencies.getSetting(any(), eq(Settings.Global.CAPTIVE_PORTAL_HTTP_URL), any()))
                .thenReturn(null);
        assertEquals(TEST_HTTP_OTHER_URL2, wnm.getCaptivePortalServerHttpUrl());
    }

    @Test
    public void testGetLocationMcc() throws Exception {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        doReturn(PackageManager.PERMISSION_DENIED).when(mContext).checkPermission(
                eq(android.Manifest.permission.ACCESS_FINE_LOCATION),  anyInt(), anyInt());
        assertNull(wnm.getLocationMcc());
        doReturn(PackageManager.PERMISSION_GRANTED).when(mContext).checkPermission(
                eq(android.Manifest.permission.ACCESS_FINE_LOCATION),  anyInt(), anyInt());
        doReturn(new ContextWrapper(mContext)).when(mContext).createConfigurationContext(any());
        doReturn(null).when(mTelephony).getAllCellInfo();
        assertNull(wnm.getLocationMcc());
        // Prepare CellInfo and check if the vote mechanism is working or not.
        final List<CellInfo> cellList = new ArrayList<CellInfo>();
        doReturn(cellList).when(mTelephony).getAllCellInfo();
        assertNull(wnm.getLocationMcc());
        cellList.add(makeTestCellInfoGsm("460"));
        cellList.add(makeTestCellInfoGsm("460"));
        cellList.add(makeTestCellInfoLte("466"));
        // The count of 460 is 2 and the count of 466 is 1, so the getLocationMcc() should return
        // 460.
        assertEquals("460", wnm.getLocationMcc());
        // getCustomizedContextOrDefault() shouldn't return mContext when using neighbor mcc
        // is enabled and the sim is not ready.
        doReturn(true).when(mResources).getBoolean(R.bool.config_no_sim_card_uses_neighbor_mcc);
        doReturn(TelephonyManager.SIM_STATE_ABSENT).when(mTelephony).getSimState();
        assertEquals(460,
                wnm.getCustomizedContextOrDefault().getResources().getConfiguration().mcc);
        doReturn(false).when(mResources).getBoolean(R.bool.config_no_sim_card_uses_neighbor_mcc);
        assertEquals(wnm.getContext(), wnm.getCustomizedContextOrDefault());
    }

    @Test
    public void testGetMccMncOverrideInfo() {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        doReturn(new ContextWrapper(mContext)).when(mContext).createConfigurationContext(any());
        // 1839 is VZW's carrier id.
        doReturn(1839).when(mTelephony).getSimCarrierId();
        assertNull(wnm.getMccMncOverrideInfo());
        // 1854 is CTC's carrier id.
        doReturn(1854).when(mTelephony).getSimCarrierId();
        assertNotNull(wnm.getMccMncOverrideInfo());
        // Check if the mcc & mnc has changed as expected.
        assertEquals(460,
                wnm.getCustomizedContextOrDefault().getResources().getConfiguration().mcc);
        assertEquals(03,
                wnm.getCustomizedContextOrDefault().getResources().getConfiguration().mnc);
        // Every mcc and mnc should be set in sCarrierIdToMccMnc.
        // Check if there is any unset value in mcc or mnc.
        for (int i = 0; i < wnm.sCarrierIdToMccMnc.size(); i++) {
            assertNotEquals(-1, wnm.sCarrierIdToMccMnc.valueAt(i).mcc);
            assertNotEquals(-1, wnm.sCarrierIdToMccMnc.valueAt(i).mnc);
        }
    }

    private CellInfoGsm makeTestCellInfoGsm(String mcc) throws Exception {
        final CellInfoGsm info = new CellInfoGsm();
        final CellIdentityGsm ci = makeCellIdentityGsm(0, 0, 0, 0, mcc, "01", "", "");
        info.setCellIdentity(ci);
        return info;
    }

    private CellInfoLte makeTestCellInfoLte(String mcc) throws Exception {
        final CellInfoLte info = new CellInfoLte();
        final CellIdentityLte ci = makeCellIdentityLte(0, 0, 0, 0, 0, mcc, "01", "", "");
        info.setCellIdentity(ci);
        return info;
    }

    @Test
    public void testMakeFallbackUrls() throws Exception {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        // Value exist in setting provider.
        URL[] urls = wnm.makeCaptivePortalFallbackUrls();
        assertEquals(urls[0].toString(), TEST_FALLBACK_URL);

        // Clear setting provider value. Verify it to get configuration from resource instead.
        setFallbackUrl(null);
        // Verify that getting resource with exception.
        when(mResources.getStringArray(R.array.config_captive_portal_fallback_urls))
                .thenThrow(Resources.NotFoundException.class);
        urls = wnm.makeCaptivePortalFallbackUrls();
        assertEquals(urls.length, 0);

        // Verify resource return 2 different URLs.
        doReturn(new String[] {"http://testUrl1.com", "http://testUrl2.com"}).when(mResources)
                .getStringArray(R.array.config_captive_portal_fallback_urls);
        urls = wnm.makeCaptivePortalFallbackUrls();
        assertEquals(urls.length, 2);
        assertEquals("http://testUrl1.com", urls[0].toString());
        assertEquals("http://testUrl2.com", urls[1].toString());

        // Value is expected to be replaced by location resource.
        doReturn(true).when(mResources).getBoolean(R.bool.config_no_sim_card_uses_neighbor_mcc);

        final List<CellInfo> cellList = new ArrayList<CellInfo>();
        final int testMcc = 460;
        cellList.add(makeTestCellInfoGsm(Integer.toString(testMcc)));
        doReturn(cellList).when(mTelephony).getAllCellInfo();
        final Configuration config = mResources.getConfiguration();
        config.mcc = testMcc;
        doReturn(mMccContext).when(mContext).createConfigurationContext(eq(config));
        doReturn(new String[] {"http://testUrl3.com"}).when(mMccResource)
                .getStringArray(R.array.config_captive_portal_fallback_urls);
        urls = wnm.makeCaptivePortalFallbackUrls();
        assertEquals(urls.length, 1);
        assertEquals("http://testUrl3.com", urls[0].toString());
    }

    private static CellIdentityGsm makeCellIdentityGsm(int lac, int cid, int arfcn, int bsic,
            String mccStr, String mncStr, String alphal, String alphas)
            throws ReflectiveOperationException {
        if (ShimUtils.isAtLeastR()) {
            return new CellIdentityGsm(lac, cid, arfcn, bsic, mccStr, mncStr, alphal, alphas,
                    Collections.emptyList() /* additionalPlmns */);
        } else {
            // API <= Q does not have the additionalPlmns parameter
            final Constructor<CellIdentityGsm> constructor = CellIdentityGsm.class.getConstructor(
                    int.class, int.class, int.class, int.class, String.class, String.class,
                    String.class, String.class);
            return constructor.newInstance(lac, cid, arfcn, bsic, mccStr, mncStr, alphal, alphas);
        }
    }

    private static CellIdentityLte makeCellIdentityLte(int ci, int pci, int tac, int earfcn,
            int bandwidth, String mccStr, String mncStr, String alphal, String alphas)
            throws ReflectiveOperationException {
        if (ShimUtils.isAtLeastR()) {
            return new CellIdentityLte(ci, pci, tac, earfcn, new int[] {} /* bands */,
                    bandwidth, mccStr, mncStr, alphal, alphas,
                    Collections.emptyList() /* additionalPlmns */, null /* csgInfo */);
        } else {
            // API <= Q does not have the additionalPlmns and csgInfo parameters
            final Constructor<CellIdentityLte> constructor = CellIdentityLte.class.getConstructor(
                    int.class, int.class, int.class, int.class, int.class, String.class,
                    String.class, String.class, String.class);
            return constructor.newInstance(ci, pci, tac, earfcn, bandwidth, mccStr, mncStr, alphal,
                    alphas);
        }
    }

    @Test
    public void testGetIntSetting() throws Exception {
        WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();

        // No config resource, no device config. Expect to get default resource.
        doThrow(new Resources.NotFoundException())
                .when(mResources).getInteger(eq(R.integer.config_captive_portal_dns_probe_timeout));
        doAnswer(invocation -> {
            int defaultValue = invocation.getArgument(2);
            return defaultValue;
        }).when(mDependencies).getDeviceConfigPropertyInt(any(),
                eq(NetworkMonitor.CONFIG_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT),
                anyInt());
        assertEquals(DEFAULT_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT, wnm.getIntSetting(mContext,
                R.integer.config_captive_portal_dns_probe_timeout,
                NetworkMonitor.CONFIG_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT,
                DEFAULT_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT));

        // Set device config. Expect to get device config.
        when(mDependencies.getDeviceConfigPropertyInt(any(),
                eq(NetworkMonitor.CONFIG_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT), anyInt()))
                        .thenReturn(1234);
        assertEquals(1234, wnm.getIntSetting(mContext,
                R.integer.config_captive_portal_dns_probe_timeout,
                NetworkMonitor.CONFIG_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT,
                DEFAULT_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT));

        // Set config resource. Expect to get config resource.
        when(mResources.getInteger(eq(R.integer.config_captive_portal_dns_probe_timeout)))
                .thenReturn(5678);
        assertEquals(5678, wnm.getIntSetting(mContext,
                R.integer.config_captive_portal_dns_probe_timeout,
                NetworkMonitor.CONFIG_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT,
                DEFAULT_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT));
    }

    @Test
    public void testIsCaptivePortal_HttpProbeIsPortal() throws Exception {
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);
        runPortalNetworkTest();
    }

    private void setupPrivateIpResponse(String privateAddr) throws Exception {
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);
        final String httpHost = new URL(TEST_HTTP_URL).getHost();
        mFakeDns.setAnswer(httpHost, new String[] { "2001:db8::123" }, TYPE_AAAA);
        final InetAddress parsedPrivateAddr = InetAddresses.parseNumericAddress(privateAddr);
        mFakeDns.setAnswer(httpHost, new String[] { privateAddr },
                (parsedPrivateAddr instanceof Inet6Address) ? TYPE_AAAA : TYPE_A);
    }

    @Test
    public void testIsCaptivePortal_PrivateIpNotPortal_Enabled_IPv4() throws Exception {
        when(mDependencies.isFeatureEnabled(any(), eq(DNS_PROBE_PRIVATE_IP_NO_INTERNET_VERSION)))
                .thenReturn(true);
        setupPrivateIpResponse("192.168.1.1");
        runFailedNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_PrivateIpNotPortal_Enabled_IPv6() throws Exception {
        when(mDependencies.isFeatureEnabled(any(), eq(DNS_PROBE_PRIVATE_IP_NO_INTERNET_VERSION)))
                .thenReturn(true);
        setupPrivateIpResponse("fec0:1234::1");
        runFailedNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_PrivateIpNotPortal_Disabled() throws Exception {
        setupPrivateIpResponse("192.168.1.1");
        runPortalNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_HttpsProbeIsNotPortal() throws Exception {
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 500);

        runValidatedNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_FallbackProbeIsPortal() throws Exception {
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setPortal302(mFallbackConnection);
        runPortalNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_FallbackProbeIsNotPortal() throws Exception {
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setStatus(mFallbackConnection, 500);

        // Fallback probe did not see portal, HTTPS failed -> inconclusive
        runFailedNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_OtherFallbackProbeIsPortal() throws Exception {
        // Set all fallback probes but one to invalid URLs to verify they are being skipped
        setFallbackUrl(TEST_FALLBACK_URL);
        setOtherFallbackUrls(TEST_FALLBACK_URL + "," + TEST_OTHER_FALLBACK_URL);

        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setStatus(mFallbackConnection, 500);
        setPortal302(mOtherFallbackConnection);

        // TEST_OTHER_FALLBACK_URL is third
        when(mRandom.nextInt()).thenReturn(2);

        // First check always uses the first fallback URL: inconclusive
        final NetworkMonitor monitor = runFailedNetworkTest();
        verify(mFallbackConnection, times(1)).getResponseCode();
        verify(mOtherFallbackConnection, never()).getResponseCode();

        // Second check should be triggered automatically after the reevaluate delay, and uses the
        // URL chosen by mRandom
        // This test is appropriate to cover reevaluate behavior as long as the timeout is short
        assertTrue(INITIAL_REEVALUATE_DELAY_MS < 2000);
        verify(mOtherFallbackConnection, timeout(INITIAL_REEVALUATE_DELAY_MS + HANDLER_TIMEOUT_MS))
                .getResponseCode();
        verifyNetworkTested(VALIDATION_RESULT_PORTAL, 0 /* probesSucceeded */, TEST_LOGIN_URL);
    }

    @Test
    public void testIsCaptivePortal_AllProbesFailed() throws Exception {
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setStatus(mFallbackConnection, 404);

        runFailedNetworkTest();
        verify(mFallbackConnection, times(1)).getResponseCode();
        verify(mOtherFallbackConnection, never()).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_InvalidUrlSkipped() throws Exception {
        setFallbackUrl("invalid");
        setOtherFallbackUrls("otherinvalid," + TEST_OTHER_FALLBACK_URL + ",yetanotherinvalid");

        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setPortal302(mOtherFallbackConnection);
        runPortalNetworkTest();
        verify(mOtherFallbackConnection, times(1)).getResponseCode();
        verify(mFallbackConnection, never()).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_CapportApiIsPortalWithNullPortalUrl() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setSslException(mHttpsConnection);
        final long bytesRemaining = 10_000L;
        final long secondsRemaining = 500L;
        // Set content without partal url.
        setApiContent(mCapportApiConnection, "{'captive': true,"
                + "'venue-info-url': '" + TEST_VENUE_INFO_URL + "',"
                + "'bytes-remaining': " + bytesRemaining + ","
                + "'seconds-remaining': " + secondsRemaining + "}");
        setPortal302(mHttpConnection);

        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES, VALIDATION_RESULT_PORTAL,
                0 /* probesSucceeded*/, TEST_LOGIN_URL);

        verify(mCapportApiConnection).getResponseCode();

        verify(mHttpConnection, times(1)).getResponseCode();
        verify(mCallbacks, never()).notifyCaptivePortalDataChanged(any());
    }

    @Test
    public void testIsCaptivePortal_CapportApiIsPortalWithValidPortalUrl() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setSslException(mHttpsConnection);
        final long bytesRemaining = 10_000L;
        final long secondsRemaining = 500L;

        setApiContent(mCapportApiConnection, "{'captive': true,"
                + "'user-portal-url': '" + TEST_LOGIN_URL + "',"
                + "'venue-info-url': '" + TEST_VENUE_INFO_URL + "',"
                + "'bytes-remaining': " + bytesRemaining + ","
                + "'seconds-remaining': " + secondsRemaining + "}");

        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES, VALIDATION_RESULT_PORTAL,
                0 /* probesSucceeded*/, TEST_LOGIN_URL);

        verify(mHttpConnection, never()).getResponseCode();
        verify(mCapportApiConnection).getResponseCode();

        final ArgumentCaptor<CaptivePortalData> capportDataCaptor =
                ArgumentCaptor.forClass(CaptivePortalData.class);
        verify(mCallbacks).notifyCaptivePortalDataChanged(capportDataCaptor.capture());
        final CaptivePortalData p = capportDataCaptor.getValue();
        assertTrue(p.isCaptive());
        assertEquals(Uri.parse(TEST_LOGIN_URL), p.getUserPortalUrl());
        assertEquals(Uri.parse(TEST_VENUE_INFO_URL), p.getVenueInfoUrl());
        assertEquals(bytesRemaining, p.getByteLimit());
        final long expectedExpiry = currentTimeMillis() + secondsRemaining * 1000;
        // Actual expiry will be slightly lower as some time as passed
        assertTrue(p.getExpiryTimeMillis() <= expectedExpiry);
        assertTrue(p.getExpiryTimeMillis() > expectedExpiry - 30_000);
    }

    @Test
    public void testIsCaptivePortal_CapportApiRevalidation() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setValidProbes();
        final NetworkMonitor nm = runValidatedNetworkTest();

        setApiContent(mCapportApiConnection, "{'captive': true, "
                + "'user-portal-url': '" + TEST_LOGIN_URL + "'}");
        nm.notifyLinkPropertiesChanged(makeCapportLPs());

        verifyNetworkTested(VALIDATION_RESULT_PORTAL, 0 /* probesSucceeded */,
                TEST_LOGIN_URL);
        final ArgumentCaptor<CaptivePortalData> capportCaptor = ArgumentCaptor.forClass(
                CaptivePortalData.class);
        verify(mCallbacks).notifyCaptivePortalDataChanged(capportCaptor.capture());
        assertEquals(Uri.parse(TEST_LOGIN_URL), capportCaptor.getValue().getUserPortalUrl());

        // HTTP probe was sent on first validation but not re-sent when there was a portal URL.
        verify(mHttpConnection, times(1)).getResponseCode();
        verify(mCapportApiConnection, times(1)).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_NoRevalidationBeforeNetworkConnected() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());

        final NetworkMonitor nm = makeCellMeteredNetworkMonitor();

        final LinkProperties lp = makeCapportLPs();

        // LinkProperties changed, but NM should not revalidate before notifyNetworkConnected
        nm.notifyLinkPropertiesChanged(lp);
        verify(mHttpConnection, after(100).never()).getResponseCode();
        verify(mHttpsConnection, never()).getResponseCode();
        verify(mCapportApiConnection, never()).getResponseCode();

        setValidProbes();
        setApiContent(mCapportApiConnection, "{'captive': true, "
                + "'user-portal-url': '" + TEST_LOGIN_URL + "'}");

        // After notifyNetworkConnected, validation uses the capport API contents
        nm.notifyNetworkConnected(lp, CELL_METERED_CAPABILITIES);
        verifyNetworkTested(VALIDATION_RESULT_PORTAL, 0 /* probesSucceeded */, TEST_LOGIN_URL);

        verify(mHttpConnection, never()).getResponseCode();
        verify(mCapportApiConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_CapportApiNotPortalNotValidated() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setApiContent(mCapportApiConnection, "{'captive': false,"
                + "'venue-info-url': '" + TEST_VENUE_INFO_URL + "'}");
        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES, VALIDATION_RESULT_INVALID,
                0 /* probesSucceeded */, null /* redirectUrl */);

        final ArgumentCaptor<CaptivePortalData> capportCaptor = ArgumentCaptor.forClass(
                CaptivePortalData.class);
        verify(mCallbacks).notifyCaptivePortalDataChanged(capportCaptor.capture());
        assertEquals(Uri.parse(TEST_VENUE_INFO_URL), capportCaptor.getValue().getVenueInfoUrl());
    }

    @Test
    public void testIsCaptivePortal_CapportApiNotPortalPartial() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 204);
        setApiContent(mCapportApiConnection, "{'captive': false,"
                + "'venue-info-url': '" + TEST_VENUE_INFO_URL + "'}");
        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES,
                NETWORK_VALIDATION_RESULT_PARTIAL,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP,
                null /* redirectUrl */);

        final ArgumentCaptor<CaptivePortalData> capportCaptor = ArgumentCaptor.forClass(
                CaptivePortalData.class);
        verify(mCallbacks).notifyCaptivePortalDataChanged(capportCaptor.capture());
        assertEquals(Uri.parse(TEST_VENUE_INFO_URL), capportCaptor.getValue().getVenueInfoUrl());
    }

    @Test
    public void testIsCaptivePortal_CapportApiNotPortalValidated() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);
        setApiContent(mCapportApiConnection, "{'captive': false,"
                + "'venue-info-url': '" + TEST_VENUE_INFO_URL + "'}");
        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES,
                NETWORK_VALIDATION_RESULT_VALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP
                        | NETWORK_VALIDATION_PROBE_HTTPS,
                null /* redirectUrl */);

        final ArgumentCaptor<CaptivePortalData> capportCaptor = ArgumentCaptor.forClass(
                CaptivePortalData.class);
        verify(mCallbacks).notifyCaptivePortalDataChanged(capportCaptor.capture());
        assertEquals(Uri.parse(TEST_VENUE_INFO_URL), capportCaptor.getValue().getVenueInfoUrl());
    }

    @Test
    public void testIsCaptivePortal_CapportApiInvalidContent() throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);
        setApiContent(mCapportApiConnection, "{SomeInvalidText");
        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES,
                VALIDATION_RESULT_PORTAL, 0 /* probesSucceeded */,
                TEST_LOGIN_URL);

        verify(mCallbacks, never()).notifyCaptivePortalDataChanged(any());
        verify(mHttpConnection).getResponseCode();
    }

    private void runCapportApiInvalidUrlTest(String url) throws Exception {
        assumeTrue(CaptivePortalDataShimImpl.isSupported());
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);
        final LinkProperties lp = new LinkProperties(TEST_LINK_PROPERTIES);
        lp.setCaptivePortalApiUrl(Uri.parse(url));
        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES,
                VALIDATION_RESULT_PORTAL, 0 /* probesSucceeded */,
                TEST_LOGIN_URL);

        verify(mCallbacks, never()).notifyCaptivePortalDataChanged(any());
        verify(mCapportApiConnection, never()).getInputStream();
        verify(mHttpConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_HttpIsInvalidCapportApiScheme() throws Exception {
        runCapportApiInvalidUrlTest("http://capport.example.com");
    }

    @Test
    public void testIsCaptivePortal_FileIsInvalidCapportApiScheme() throws Exception {
        runCapportApiInvalidUrlTest("file://localhost/myfile");
    }

    @Test
    public void testIsCaptivePortal_InvalidUrlFormat() throws Exception {
        runCapportApiInvalidUrlTest("ThisIsNotAValidUrl");
    }

    @Test @IgnoreUpTo(Build.VERSION_CODES.Q)
    public void testIsCaptivePortal_CapportApiNotSupported() throws Exception {
        // Test that on a R+ device, if NetworkStack was compiled without CaptivePortalData support
        // (built against Q), NetworkMonitor behaves as expected.
        assumeFalse(CaptivePortalDataShimImpl.isSupported());
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);
        setApiContent(mCapportApiConnection, "{'captive': false,"
                + "'venue-info-url': '" + TEST_VENUE_INFO_URL + "'}");
        runNetworkTest(makeCapportLPs(), CELL_METERED_CAPABILITIES, VALIDATION_RESULT_PORTAL,
                0 /* probesSucceeded */,
                TEST_LOGIN_URL);

        verify(mCallbacks, never()).notifyCaptivePortalDataChanged(any());
        verify(mHttpConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_HttpsProbeMatchesFailRegex() throws Exception {
        setStatus(mHttpsConnection, 200);
        setStatus(mHttpConnection, 500);
        final String content = "test";
        doReturn(new ByteArrayInputStream(content.getBytes(StandardCharsets.UTF_8)))
                .when(mHttpsConnection).getInputStream();
        doReturn(Long.valueOf(content.length())).when(mHttpsConnection).getContentLengthLong();
        doReturn(1).when(mResources).getInteger(R.integer.config_min_matches_http_content_length);
        doReturn(10).when(mResources).getInteger(
                R.integer.config_max_matches_http_content_length);
        doReturn("te.t").when(mResources).getString(
                R.string.config_network_validation_failed_content_regexp);
        runFailedNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_HttpProbeMatchesSuccessRegex() throws Exception {
        setStatus(mHttpsConnection, 500);
        setStatus(mHttpConnection, 200);
        final String content = "test";
        doReturn(new ByteArrayInputStream(content.getBytes(StandardCharsets.UTF_8)))
                .when(mHttpConnection).getInputStream();
        doReturn(Long.valueOf(content.length())).when(mHttpConnection).getContentLengthLong();
        doReturn(1).when(mResources).getInteger(R.integer.config_min_matches_http_content_length);
        doReturn(10).when(mResources).getInteger(
                R.integer.config_max_matches_http_content_length);
        doReturn("te.t").when(mResources).getString(
                R.string.config_network_validation_success_content_regexp);
        runPartialConnectivityNetworkTest(
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP);
    }

    private void setupFallbackSpec() throws IOException {
        setFallbackSpecs("http://example.com@@/@@204@@/@@"
                + "@@,@@"
                + TEST_OTHER_FALLBACK_URL + "@@/@@30[12]@@/@@https://(www\\.)?google.com/?.*");

        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);

        // Use the 2nd fallback spec
        when(mRandom.nextInt()).thenReturn(1);
    }

    @Test
    public void testIsCaptivePortal_FallbackSpecIsPartial() throws Exception {
        setupFallbackSpec();
        set302(mOtherFallbackConnection, "https://www.google.com/test?q=3");

        // HTTPS failed, fallback spec went through -> partial connectivity
        runPartialConnectivityNetworkTest(
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_FALLBACK);
        verify(mOtherFallbackConnection, times(1)).getResponseCode();
        verify(mFallbackConnection, never()).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_FallbackSpecIsPortal() throws Exception {
        setupFallbackSpec();
        setPortal302(mOtherFallbackConnection);
        runPortalNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_IgnorePortals() throws Exception {
        setCaptivePortalMode(Settings.Global.CAPTIVE_PORTAL_MODE_IGNORE);
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);

        runNoValidationNetworkTest();
    }

    @Test
    public void testIsCaptivePortal_OverriddenHttpsUrlValid() throws Exception {
        setDeviceConfig(TEST_URL_EXPIRATION_TIME,
                String.valueOf(currentTimeMillis() + TimeUnit.MINUTES.toMillis(9)));
        setDeviceConfig(TEST_CAPTIVE_PORTAL_HTTPS_URL, TEST_OVERRIDE_URL);
        setStatus(mTestOverriddenUrlConnection, 204);
        setStatus(mHttpConnection, 204);

        runValidatedNetworkTest();
        verify(mHttpsConnection, never()).getResponseCode();
        verify(mTestOverriddenUrlConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_OverriddenHttpUrlPortal() throws Exception {
        setDeviceConfig(TEST_URL_EXPIRATION_TIME,
                String.valueOf(currentTimeMillis() + TimeUnit.MINUTES.toMillis(9)));
        setDeviceConfig(TEST_CAPTIVE_PORTAL_HTTP_URL, TEST_OVERRIDE_URL);
        setStatus(mHttpsConnection, 500);
        setPortal302(mTestOverriddenUrlConnection);

        runPortalNetworkTest();
        verify(mHttpConnection, never()).getResponseCode();
        verify(mTestOverriddenUrlConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_InvalidHttpOverrideUrl() throws Exception {
        setDeviceConfig(TEST_URL_EXPIRATION_TIME,
                String.valueOf(currentTimeMillis() + TimeUnit.MINUTES.toMillis(9)));
        setDeviceConfig(TEST_CAPTIVE_PORTAL_HTTP_URL, TEST_INVALID_OVERRIDE_URL);
        setStatus(mHttpsConnection, 500);
        setPortal302(mHttpConnection);

        runPortalNetworkTest();
        verify(mTestOverriddenUrlConnection, never()).getResponseCode();
        verify(mHttpConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_InvalidHttpsOverrideUrl() throws Exception {
        setDeviceConfig(TEST_URL_EXPIRATION_TIME,
                String.valueOf(currentTimeMillis() + TimeUnit.MINUTES.toMillis(9)));
        setDeviceConfig(TEST_CAPTIVE_PORTAL_HTTPS_URL, TEST_INVALID_OVERRIDE_URL);
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);

        runValidatedNetworkTest();
        verify(mTestOverriddenUrlConnection, never()).getResponseCode();
        verify(mHttpsConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_ExpiredHttpsOverrideUrl() throws Exception {
        setDeviceConfig(TEST_URL_EXPIRATION_TIME,
                String.valueOf(currentTimeMillis() - TimeUnit.MINUTES.toMillis(1)));
        setDeviceConfig(TEST_CAPTIVE_PORTAL_HTTPS_URL, TEST_OVERRIDE_URL);
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);

        runValidatedNetworkTest();
        verify(mTestOverriddenUrlConnection, never()).getResponseCode();
        verify(mHttpsConnection).getResponseCode();
    }

    @Test
    public void testIsCaptivePortal_TestHttpUrlExpirationTooLarge() throws Exception {
        setDeviceConfig(TEST_URL_EXPIRATION_TIME,
                String.valueOf(currentTimeMillis() + TimeUnit.MINUTES.toMillis(20)));
        setDeviceConfig(TEST_CAPTIVE_PORTAL_HTTP_URL, TEST_OVERRIDE_URL);
        setStatus(mHttpsConnection, 500);
        setPortal302(mHttpConnection);

        runPortalNetworkTest();
        verify(mTestOverriddenUrlConnection, never()).getResponseCode();
        verify(mHttpConnection).getResponseCode();
    }

    @Test
    public void testIsDataStall_EvaluationDisabled() {
        setDataStallEvaluationType(0);
        WrappedNetworkMonitor wrappedMonitor = makeCellMeteredNetworkMonitor();
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 100);
        assertFalse(wrappedMonitor.isDataStall());
    }

    @Test
    public void testIsDataStall_EvaluationDnsOnNotMeteredNetwork() throws Exception {
        WrappedNetworkMonitor wrappedMonitor = makeCellNotMeteredNetworkMonitor();
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 100);
        makeDnsTimeoutEvent(wrappedMonitor, DEFAULT_DNS_TIMEOUT_THRESHOLD);
        assertTrue(wrappedMonitor.isDataStall());
        verify(mCallbacks).notifyDataStallSuspected(
                matchDnsDataStallParcelable(DEFAULT_DNS_TIMEOUT_THRESHOLD));
    }

    @Test
    public void testIsDataStall_EvaluationDnsOnMeteredNetwork() throws Exception {
        WrappedNetworkMonitor wrappedMonitor = makeCellMeteredNetworkMonitor();
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 100);
        assertFalse(wrappedMonitor.isDataStall());

        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 1000);
        makeDnsTimeoutEvent(wrappedMonitor, DEFAULT_DNS_TIMEOUT_THRESHOLD);
        assertTrue(wrappedMonitor.isDataStall());
        verify(mCallbacks).notifyDataStallSuspected(
                matchDnsDataStallParcelable(DEFAULT_DNS_TIMEOUT_THRESHOLD));
    }

    @Test
    public void testIsDataStall_EvaluationDnsWithDnsTimeoutCount() throws Exception {
        WrappedNetworkMonitor wrappedMonitor = makeCellMeteredNetworkMonitor();
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 1000);
        makeDnsTimeoutEvent(wrappedMonitor, 3);
        assertFalse(wrappedMonitor.isDataStall());
        // Reset consecutive timeout counts.
        makeDnsSuccessEvent(wrappedMonitor, 1);
        makeDnsTimeoutEvent(wrappedMonitor, 2);
        assertFalse(wrappedMonitor.isDataStall());

        makeDnsTimeoutEvent(wrappedMonitor, 3);
        assertTrue(wrappedMonitor.isDataStall());

        // The expected timeout count is the previous 2 DNS timeouts + the most recent 3 timeouts
        verify(mCallbacks).notifyDataStallSuspected(
                matchDnsDataStallParcelable(5 /* timeoutCount */));

        // Set the value to larger than the default dns log size.
        setConsecutiveDnsTimeoutThreshold(51);
        wrappedMonitor = makeCellMeteredNetworkMonitor();
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 1000);
        makeDnsTimeoutEvent(wrappedMonitor, 50);
        assertFalse(wrappedMonitor.isDataStall());

        makeDnsTimeoutEvent(wrappedMonitor, 1);
        assertTrue(wrappedMonitor.isDataStall());

        // The expected timeout count is the previous 50 DNS timeouts + the most recent timeout
        verify(mCallbacks).notifyDataStallSuspected(
                matchDnsDataStallParcelable(51 /* timeoutCount */));
    }

    @Test
    public void testIsDataStall_SkipEvaluateOnValidationNotRequiredNetwork() {
        // Make DNS and TCP stall condition satisfied.
        setDataStallEvaluationType(DATA_STALL_EVALUATION_TYPE_DNS | DATA_STALL_EVALUATION_TYPE_TCP);
        when(mTstDependencies.isTcpInfoParsingSupported()).thenReturn(true);
        when(mTst.getLatestReceivedCount()).thenReturn(0);
        when(mTst.isDataStallSuspected()).thenReturn(true);
        final WrappedNetworkMonitor nm = makeMonitor(CELL_NO_INTERNET_CAPABILITIES);
        nm.setLastProbeTime(SystemClock.elapsedRealtime() - 1000);
        makeDnsTimeoutEvent(nm, DEFAULT_DNS_TIMEOUT_THRESHOLD);
        assertFalse(nm.isDataStall());
    }

    @Test
    public void testIsDataStall_EvaluationDnsWithDnsTimeThreshold() throws Exception {
        // Test dns events happened in valid dns time threshold.
        WrappedNetworkMonitor wrappedMonitor = makeCellMeteredNetworkMonitor();
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 100);
        makeDnsTimeoutEvent(wrappedMonitor, DEFAULT_DNS_TIMEOUT_THRESHOLD);
        assertFalse(wrappedMonitor.isDataStall());
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 1000);
        assertTrue(wrappedMonitor.isDataStall());
        verify(mCallbacks).notifyDataStallSuspected(
                matchDnsDataStallParcelable(DEFAULT_DNS_TIMEOUT_THRESHOLD));

        // Test dns events happened before valid dns time threshold.
        setValidDataStallDnsTimeThreshold(0);
        wrappedMonitor = makeCellMeteredNetworkMonitor();
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 100);
        makeDnsTimeoutEvent(wrappedMonitor, DEFAULT_DNS_TIMEOUT_THRESHOLD);
        assertFalse(wrappedMonitor.isDataStall());
        wrappedMonitor.setLastProbeTime(SystemClock.elapsedRealtime() - 1000);
        assertFalse(wrappedMonitor.isDataStall());
    }

    @Test
    public void testIsDataStall_EvaluationTcp() throws Exception {
        // Evaluate TCP only. Expect ignoring DNS signal.
        setDataStallEvaluationType(DATA_STALL_EVALUATION_TYPE_TCP);
        WrappedNetworkMonitor wrappedMonitor = makeMonitor(CELL_METERED_CAPABILITIES);
        assertFalse(wrappedMonitor.isDataStall());
        // Packet received.
        when(mTstDependencies.isTcpInfoParsingSupported()).thenReturn(true);
        when(mTst.getLatestReceivedCount()).thenReturn(5);
        // Trigger a tcp event immediately.
        setTcpPollingInterval(0);
        wrappedMonitor.sendTcpPollingEvent();
        HandlerUtilsKt.waitForIdle(wrappedMonitor.getHandler(), HANDLER_TIMEOUT_MS);
        assertFalse(wrappedMonitor.isDataStall());

        when(mTst.getLatestReceivedCount()).thenReturn(0);
        when(mTst.isDataStallSuspected()).thenReturn(true);
        // Trigger a tcp event immediately.
        setTcpPollingInterval(0);
        wrappedMonitor.sendTcpPollingEvent();
        HandlerUtilsKt.waitForIdle(wrappedMonitor.getHandler(), HANDLER_TIMEOUT_MS);
        assertTrue(wrappedMonitor.isDataStall());
        verify(mCallbacks).notifyDataStallSuspected(matchTcpDataStallParcelable());
    }

    @Test
    public void testIsDataStall_DisableTcp() {
        // Disable tcp detection with only DNS detect. keep the tcp signal but set to no DNS signal.
        setDataStallEvaluationType(DATA_STALL_EVALUATION_TYPE_DNS);
        WrappedNetworkMonitor wrappedMonitor = makeMonitor(CELL_METERED_CAPABILITIES);
        makeDnsSuccessEvent(wrappedMonitor, 1);
        wrappedMonitor.sendTcpPollingEvent();
        HandlerUtilsKt.waitForIdle(wrappedMonitor.getHandler(), HANDLER_TIMEOUT_MS);
        assertFalse(wrappedMonitor.isDataStall());
        verify(mTst, never()).isDataStallSuspected();
        verify(mTst, never()).pollSocketsInfo();
    }

    @Test
    public void testBrokenNetworkNotValidated() throws Exception {
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setStatus(mFallbackConnection, 404);

        runFailedNetworkTest();
    }

    @Test
    public void testNoInternetCapabilityValidated() throws Exception {
        runNetworkTest(TEST_LINK_PROPERTIES, CELL_NO_INTERNET_CAPABILITIES,
                NETWORK_VALIDATION_RESULT_VALID, 0 /* probesSucceeded */, null /* redirectUrl */);
        verify(mCleartextDnsNetwork, never()).openConnection(any());
    }

    @Test
    public void testLaunchCaptivePortalApp() throws Exception {
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);
        when(mHttpConnection.getHeaderField(eq("location"))).thenReturn(TEST_LOGIN_URL);
        final NetworkMonitor nm = makeMonitor(CELL_METERED_CAPABILITIES);
        notifyNetworkConnected(nm, CELL_METERED_CAPABILITIES);

        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).times(1))
                .showProvisioningNotification(any(), any());

        assertEquals(1, mRegisteredReceivers.size());

        // Check that startCaptivePortalApp sends the expected intent.
        nm.launchCaptivePortalApp();

        final ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        final ArgumentCaptor<Network> networkCaptor = ArgumentCaptor.forClass(Network.class);
        verify(mCm, timeout(HANDLER_TIMEOUT_MS).times(1))
                .startCaptivePortalApp(networkCaptor.capture(), bundleCaptor.capture());
        verify(mNotifier).notifyCaptivePortalValidationPending(networkCaptor.getValue());
        final Bundle bundle = bundleCaptor.getValue();
        final Network bundleNetwork = bundle.getParcelable(ConnectivityManager.EXTRA_NETWORK);
        assertEquals(TEST_NETID, bundleNetwork.netId);
        // network is passed both in bundle and as parameter, as the bundle is opaque to the
        // framework and only intended for the captive portal app, but the framework needs
        // the network to identify the right NetworkMonitor.
        assertEquals(TEST_NETID, networkCaptor.getValue().netId);
        // Portal URL should be detection URL.
        final String redirectUrl = bundle.getString(ConnectivityManager.EXTRA_CAPTIVE_PORTAL_URL);
        assertEquals(TEST_HTTP_URL, redirectUrl);

        // Have the app report that the captive portal is dismissed, and check that we revalidate.
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);

        resetCallbacks();
        nm.notifyCaptivePortalAppFinished(APP_RETURN_DISMISSED);
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).atLeastOnce())
                .notifyNetworkTestedWithExtras(matchNetworkTestResultParcelable(
                        NETWORK_VALIDATION_RESULT_VALID,
                        NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP));
        assertEquals(0, mRegisteredReceivers.size());
    }

    @Test
    public void testPrivateDnsSuccess() throws Exception {
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);

        // Verify dns query only get v6 address.
        mFakeDns.setAnswer("dns6.google", new String[]{"2001:db8::53"}, TYPE_AAAA);
        WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        wnm.notifyPrivateDnsSettingsChanged(new PrivateDnsConfig("dns6.google",
                new InetAddress[0]));
        notifyNetworkConnected(wnm, CELL_NOT_METERED_CAPABILITIES);
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_VALID, PROBES_PRIVDNS_VALID);
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).times(1)).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(PROBES_PRIVDNS_VALID));

        // Verify dns query only get v4 address.
        resetCallbacks();
        mFakeDns.setAnswer("dns4.google", new String[]{"192.0.2.1"}, TYPE_A);
        wnm.notifyPrivateDnsSettingsChanged(new PrivateDnsConfig("dns4.google",
                new InetAddress[0]));
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_VALID, PROBES_PRIVDNS_VALID);
        // NetworkMonitor will check if the probes has changed or not, if the probes has not
        // changed, the callback won't be fired.
        verify(mCallbacks, never()).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(PROBES_PRIVDNS_VALID));

        // Verify dns query get both v4 and v6 address.
        resetCallbacks();
        mFakeDns.setAnswer("dns.google", new String[]{"2001:db8::54"}, TYPE_AAAA);
        mFakeDns.setAnswer("dns.google", new String[]{"192.0.2.3"}, TYPE_A);
        wnm.notifyPrivateDnsSettingsChanged(new PrivateDnsConfig("dns.google", new InetAddress[0]));
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_VALID, PROBES_PRIVDNS_VALID);
        verify(mCallbacks, never()).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(PROBES_PRIVDNS_VALID));
    }

    @Test
    public void testProbeStatusChanged() throws Exception {
        // Set no record in FakeDns and expect validation to fail.
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);

        WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        wnm.notifyPrivateDnsSettingsChanged(new PrivateDnsConfig("dns.google", new InetAddress[0]));
        wnm.notifyNetworkConnected(TEST_LINK_PROPERTIES, CELL_NOT_METERED_CAPABILITIES);
        verifyNetworkTested(VALIDATION_RESULT_INVALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS);
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS)).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(NETWORK_VALIDATION_PROBE_DNS
                | NETWORK_VALIDATION_PROBE_HTTPS));
        // Fix DNS and retry, expect validation to succeed.
        resetCallbacks();
        mFakeDns.setAnswer("dns.google", new String[]{"2001:db8::1"}, TYPE_AAAA);

        wnm.forceReevaluation(Process.myUid());
        // ProbeCompleted should be reset to 0
        HandlerUtilsKt.waitForIdle(wnm.getHandler(), HANDLER_TIMEOUT_MS);
        assertEquals(wnm.getEvaluationState().getProbeCompletedResult(), 0);
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_VALID, PROBES_PRIVDNS_VALID);
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS)).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(PROBES_PRIVDNS_VALID));
    }

    @Test
    public void testPrivateDnsResolutionRetryUpdate() throws Exception {
        // Set no record in FakeDns and expect validation to fail.
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);

        WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        wnm.notifyPrivateDnsSettingsChanged(new PrivateDnsConfig("dns.google", new InetAddress[0]));
        wnm.notifyNetworkConnected(TEST_LINK_PROPERTIES, CELL_NOT_METERED_CAPABILITIES);
        verifyNetworkTested(VALIDATION_RESULT_INVALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS);
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).times(1)).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(NETWORK_VALIDATION_PROBE_DNS
                | NETWORK_VALIDATION_PROBE_HTTPS));

        // Fix DNS and retry, expect validation to succeed.
        resetCallbacks();
        mFakeDns.setAnswer("dns.google", new String[]{"2001:db8::1"}, TYPE_AAAA);

        wnm.forceReevaluation(Process.myUid());
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).atLeastOnce())
                .notifyNetworkTestedWithExtras(matchNetworkTestResultParcelable(
                        NETWORK_VALIDATION_RESULT_VALID, PROBES_PRIVDNS_VALID));
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).times(1)).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(PROBES_PRIVDNS_VALID));

        // Change configuration to an invalid DNS name, expect validation to fail.
        resetCallbacks();
        mFakeDns.setAnswer("dns.bad", new String[0], TYPE_A);
        mFakeDns.setAnswer("dns.bad", new String[0], TYPE_AAAA);
        wnm.notifyPrivateDnsSettingsChanged(new PrivateDnsConfig("dns.bad", new InetAddress[0]));
        // Strict mode hostname resolve fail. Expect only notification for evaluation fail. No probe
        // notification.
        verifyNetworkTested(VALIDATION_RESULT_INVALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS);
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).times(1)).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(NETWORK_VALIDATION_PROBE_DNS
                | NETWORK_VALIDATION_PROBE_HTTPS));

        // Change configuration back to working again, but make private DNS not work.
        // Expect validation to fail.
        resetCallbacks();
        mFakeDns.setNonBypassPrivateDnsWorking(false);
        wnm.notifyPrivateDnsSettingsChanged(new PrivateDnsConfig("dns.google",
                new InetAddress[0]));
        verifyNetworkTested(VALIDATION_RESULT_INVALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS);
        // NetworkMonitor will check if the probes has changed or not, if the probes has not
        // changed, the callback won't be fired.
        verify(mCallbacks, never()).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(NETWORK_VALIDATION_PROBE_DNS
                | NETWORK_VALIDATION_PROBE_HTTPS));

        // Make private DNS work again. Expect validation to succeed.
        resetCallbacks();
        mFakeDns.setNonBypassPrivateDnsWorking(true);
        wnm.forceReevaluation(Process.myUid());
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_VALID, PROBES_PRIVDNS_VALID);
        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).times(1)).notifyProbeStatusChanged(
                eq(PROBES_PRIVDNS_VALID), eq(PROBES_PRIVDNS_VALID));
    }

    @Test
    public void testDataStall_StallDnsSuspectedAndSendMetricsOnCell() throws Exception {
        testDataStall_StallDnsSuspectedAndSendMetrics(NetworkCapabilities.TRANSPORT_CELLULAR,
                CELL_METERED_CAPABILITIES);
    }

    @Test
    public void testDataStall_StallDnsSuspectedAndSendMetricsOnWifi() throws Exception {
        testDataStall_StallDnsSuspectedAndSendMetrics(NetworkCapabilities.TRANSPORT_WIFI,
                WIFI_NOT_METERED_CAPABILITIES);
    }

    private void testDataStall_StallDnsSuspectedAndSendMetrics(int transport,
            NetworkCapabilities nc) throws Exception {
        // NM suspects data stall from DNS signal and sends data stall metrics.
        final WrappedNetworkMonitor nm = prepareNetworkMonitorForVerifyDataStall(nc);
        makeDnsTimeoutEvent(nm, 5);
        // Trigger a dns signal to start evaluate data stall and upload metrics.
        nm.notifyDnsResponse(RETURN_CODE_DNS_TIMEOUT);
        // Verify data sent as expected.
        verifySendDataStallDetectionStats(nm, DATA_STALL_EVALUATION_TYPE_DNS, transport);
    }

    @Test
    public void testDataStall_NoStallSuspectedAndSendMetrics() throws Exception {
        final WrappedNetworkMonitor nm = prepareNetworkMonitorForVerifyDataStall(
                CELL_METERED_CAPABILITIES);
        // Setup no data stall dns signal.
        makeDnsTimeoutEvent(nm, 3);
        assertFalse(nm.isDataStall());
        // Trigger a dns signal to start evaluate data stall.
        nm.notifyDnsResponse(RETURN_CODE_DNS_SUCCESS);
        verify(mDependencies, never()).writeDataStallDetectionStats(any(), any());
    }

    @Test
    public void testDataStall_StallTcpSuspectedAndSendMetricsOnCell() throws Exception {
        testDataStall_StallTcpSuspectedAndSendMetrics(NetworkCapabilities.TRANSPORT_CELLULAR,
                CELL_METERED_CAPABILITIES);
    }

    @Test
    public void testDataStall_StallTcpSuspectedAndSendMetricsOnWifi() throws Exception {
        testDataStall_StallTcpSuspectedAndSendMetrics(NetworkCapabilities.TRANSPORT_WIFI,
                WIFI_NOT_METERED_CAPABILITIES);
    }

    private void testDataStall_StallTcpSuspectedAndSendMetrics(int transport,
            NetworkCapabilities nc) throws Exception {
        assumeTrue(ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q));
        setupTcpDataStall();
        // NM suspects data stall from TCP signal and sends data stall metrics.
        setDataStallEvaluationType(DATA_STALL_EVALUATION_TYPE_TCP);
        final WrappedNetworkMonitor nm = prepareNetworkMonitorForVerifyDataStall(nc);

        // Trigger a tcp event immediately.
        setTcpPollingInterval(0);
        nm.sendTcpPollingEvent();
        verifySendDataStallDetectionStats(nm, DATA_STALL_EVALUATION_TYPE_TCP, transport);
    }

    private WrappedNetworkMonitor prepareNetworkMonitorForVerifyDataStall(NetworkCapabilities nc)
            throws Exception {
        // Connect a VALID network to simulate the data stall detection because data stall
        // evaluation will only start from validated state.
        setStatus(mHttpsConnection, 204);
        final WrappedNetworkMonitor nm;
        final int[] transports = nc.getTransportTypes();
        // Though multiple transport types are allowed, use the first transport type for
        // simplification.
        switch (transports[0]) {
            case NetworkCapabilities.TRANSPORT_CELLULAR:
                nm = makeCellMeteredNetworkMonitor();
                break;
            case NetworkCapabilities.TRANSPORT_WIFI:
                nm = makeWifiNotMeteredNetworkMonitor();
                setupTestWifiInfo();
                break;
            default:
                nm = null;
                fail("Undefined transport type");
        }
        nm.notifyNetworkConnected(TEST_LINK_PROPERTIES, nc);
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_VALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS);
        nm.setLastProbeTime(SystemClock.elapsedRealtime() - 1000);
        return nm;
    }

    private void setupTcpDataStall() {
        when(mTstDependencies.isTcpInfoParsingSupported()).thenReturn(true);
        when(mTst.getLatestReceivedCount()).thenReturn(0);
        when(mTst.getLatestPacketFailPercentage()).thenReturn(TEST_TCP_FAIL_RATE);
        when(mTst.getSentSinceLastRecv()).thenReturn(TEST_TCP_PACKET_COUNT);
        when(mTst.isDataStallSuspected()).thenReturn(true);
        when(mTst.pollSocketsInfo()).thenReturn(true);
    }

    private void verifySendDataStallDetectionStats(WrappedNetworkMonitor nm, int evalType,
            int transport) {
        // Verify data sent as expectated.
        final ArgumentCaptor<CaptivePortalProbeResult> probeResultCaptor =
                ArgumentCaptor.forClass(CaptivePortalProbeResult.class);
        final ArgumentCaptor<DataStallDetectionStats> statsCaptor =
                ArgumentCaptor.forClass(DataStallDetectionStats.class);
        verify(mDependencies, timeout(HANDLER_TIMEOUT_MS).times(1))
                .writeDataStallDetectionStats(statsCaptor.capture(), probeResultCaptor.capture());
        assertTrue(nm.isDataStall());
        assertTrue(probeResultCaptor.getValue().isSuccessful());
        verifyTestDataStallDetectionStats(evalType, transport, statsCaptor.getValue());
    }

    private void verifyTestDataStallDetectionStats(int evalType, int transport,
            DataStallDetectionStats stats) {
        assertEquals(transport, stats.mNetworkType);
        switch (transport) {
            case NetworkCapabilities.TRANSPORT_WIFI:
                assertArrayEquals(makeTestWifiDataNano(), stats.mWifiInfo);
                // Expedient way to check stats.mCellularInfo contains the neutral byte array that
                // is sent to represent a lack of data, as stats.mCellularInfo is not supposed to
                // contain null.
                assertArrayEquals(DataStallDetectionStats.emptyCellDataIfNull(null),
                        stats.mCellularInfo);
                break;
            case NetworkCapabilities.TRANSPORT_CELLULAR:
                // Expedient way to check stats.mWifiInfo contains the neutral byte array that is
                // sent to represent a lack of data, as stats.mWifiInfo is not supposed to contain
                // null.
                assertArrayEquals(DataStallDetectionStats.emptyWifiInfoIfNull(null),
                        stats.mWifiInfo);
                assertArrayEquals(makeTestCellDataNano(), stats.mCellularInfo);
                break;
            default:
                // Add other cases.
                fail("Unexpected transport type");
        }

        assertEquals(evalType, stats.mEvaluationType);
        if ((evalType & DATA_STALL_EVALUATION_TYPE_TCP) != 0) {
            assertEquals(TEST_TCP_FAIL_RATE, stats.mTcpFailRate);
            assertEquals(TEST_TCP_PACKET_COUNT, stats.mTcpSentSinceLastRecv);
        } else {
            assertEquals(DataStallDetectionStats.UNSPECIFIED_TCP_FAIL_RATE, stats.mTcpFailRate);
            assertEquals(DataStallDetectionStats.UNSPECIFIED_TCP_PACKETS_COUNT,
                    stats.mTcpSentSinceLastRecv);
        }

        if ((evalType & DATA_STALL_EVALUATION_TYPE_DNS) != 0) {
            assertArrayEquals(stats.mDns, makeTestDnsTimeoutNano(DEFAULT_DNS_TIMEOUT_THRESHOLD));
        } else {
            assertArrayEquals(stats.mDns, makeTestDnsTimeoutNano(0 /* times */));
        }
    }

    private DataStallDetectionStats makeTestDataStallDetectionStats(int evaluationType,
            int transportType) {
        final DataStallDetectionStats.Builder stats = new DataStallDetectionStats.Builder()
                .setEvaluationType(evaluationType)
                .setNetworkType(transportType);
        switch (transportType) {
            case NetworkCapabilities.TRANSPORT_CELLULAR:
                stats.setCellData(TelephonyManager.NETWORK_TYPE_LTE /* radioType */,
                        true /* roaming */,
                        TEST_MCCMNC /* networkMccmnc */,
                        TEST_MCCMNC /* simMccmnc */,
                        CellSignalStrength.SIGNAL_STRENGTH_NONE_OR_UNKNOWN /* signalStrength */);
                break;
            case NetworkCapabilities.TRANSPORT_WIFI:
                setupTestWifiInfo();
                stats.setWiFiData(mWifiInfo);
                break;
            default:
                break;
        }

        if ((evaluationType & DATA_STALL_EVALUATION_TYPE_TCP) != 0) {
            generateTestTcpStats(stats);
        }

        if ((evaluationType & DATA_STALL_EVALUATION_TYPE_DNS) != 0) {
            generateTimeoutDnsEvent(stats, DEFAULT_DNS_TIMEOUT_THRESHOLD);
        }

        return stats.build();
    }

    private byte[] makeTestDnsTimeoutNano(int timeoutCount) {
        // Make a expected nano dns message.
        final DnsEvent event = new DnsEvent();
        event.dnsReturnCode = new int[timeoutCount];
        event.dnsTime = new long[timeoutCount];
        Arrays.fill(event.dnsReturnCode, RETURN_CODE_DNS_TIMEOUT);
        Arrays.fill(event.dnsTime, TEST_ELAPSED_TIME_MS);
        return MessageNano.toByteArray(event);
    }

    private byte[] makeTestCellDataNano() {
        final CellularData data = new CellularData();
        data.ratType = DataStallEventProto.RADIO_TECHNOLOGY_LTE;
        data.networkMccmnc = TEST_MCCMNC;
        data.simMccmnc = TEST_MCCMNC;
        data.isRoaming = true;
        data.signalStrength = 0;
        return MessageNano.toByteArray(data);
    }

    private byte[] makeTestWifiDataNano() {
        final WifiData data = new WifiData();
        data.wifiBand = DataStallEventProto.AP_BAND_2GHZ;
        data.signalStrength = TEST_SIGNAL_STRENGTH;
        return MessageNano.toByteArray(data);
    }

    private void setupTestWifiInfo() {
        when(mWifi.getConnectionInfo()).thenReturn(mWifiInfo);
        when(mWifiInfo.getRssi()).thenReturn(TEST_SIGNAL_STRENGTH);
        // Set to 2.4G band. Map to DataStallEventProto.AP_BAND_2GHZ proto definition.
        when(mWifiInfo.getFrequency()).thenReturn(2450);
    }

    private void testDataStallMetricsWithCellular(int evalType) {
        testDataStallMetrics(evalType, NetworkCapabilities.TRANSPORT_CELLULAR);
    }

    private void testDataStallMetricsWithWiFi(int evalType) {
        testDataStallMetrics(evalType, NetworkCapabilities.TRANSPORT_WIFI);
    }

    private void testDataStallMetrics(int evalType, int transportType) {
        setDataStallEvaluationType(evalType);
        final NetworkCapabilities nc = new NetworkCapabilities()
                .addTransportType(transportType)
                .addCapability(NET_CAPABILITY_INTERNET);
        final WrappedNetworkMonitor wrappedMonitor = makeMonitor(nc);
        setupTestWifiInfo();
        final DataStallDetectionStats stats =
                makeTestDataStallDetectionStats(evalType, transportType);
        assertEquals(wrappedMonitor.buildDataStallDetectionStats(transportType, evalType), stats);

        if ((evalType & DATA_STALL_EVALUATION_TYPE_TCP) != 0) {
            verify(mTst, timeout(HANDLER_TIMEOUT_MS).atLeastOnce()).getLatestPacketFailPercentage();
        } else {
            verify(mTst, never()).getLatestPacketFailPercentage();
        }
    }

    @Test
    public void testCollectDataStallMetrics_DnsWithCellular() {
        testDataStallMetricsWithCellular(DATA_STALL_EVALUATION_TYPE_DNS);
    }

    @Test
    public void testCollectDataStallMetrics_DnsWithWiFi() {
        testDataStallMetricsWithWiFi(DATA_STALL_EVALUATION_TYPE_DNS);
    }

    @Test
    public void testCollectDataStallMetrics_TcpWithCellular() {
        assumeTrue(ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q));
        testDataStallMetricsWithCellular(DATA_STALL_EVALUATION_TYPE_TCP);
    }

    @Test
    public void testCollectDataStallMetrics_TcpWithWiFi() {
        assumeTrue(ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q));
        testDataStallMetricsWithWiFi(DATA_STALL_EVALUATION_TYPE_TCP);
    }

    @Test
    public void testCollectDataStallMetrics_TcpAndDnsWithWifi() {
        assumeTrue(ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q));
        testDataStallMetricsWithWiFi(
                DATA_STALL_EVALUATION_TYPE_TCP | DATA_STALL_EVALUATION_TYPE_DNS);
    }

    @Test
    public void testCollectDataStallMetrics_TcpAndDnsWithCellular() {
        assumeTrue(ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q));
        testDataStallMetricsWithCellular(
                DATA_STALL_EVALUATION_TYPE_TCP | DATA_STALL_EVALUATION_TYPE_DNS);
    }

    @Test
    public void testIgnoreHttpsProbe() throws Exception {
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 204);
        // Expect to send HTTP, HTTPS, FALLBACK probe and evaluation result notifications to CS.
        final NetworkMonitor nm = runNetworkTest(NETWORK_VALIDATION_RESULT_PARTIAL,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP,
                null /* redirectUrl */);

        resetCallbacks();
        nm.setAcceptPartialConnectivity();
        // Expect to update evaluation result notifications to CS.
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_PARTIAL | NETWORK_VALIDATION_RESULT_VALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP);
    }

    @Test
    public void testIsPartialConnectivity() throws Exception {
        setStatus(mHttpsConnection, 500);
        setStatus(mHttpConnection, 204);
        setStatus(mFallbackConnection, 500);
        runPartialConnectivityNetworkTest(
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP);

        resetCallbacks();
        setStatus(mHttpsConnection, 500);
        setStatus(mHttpConnection, 500);
        setStatus(mFallbackConnection, 204);
        runPartialConnectivityNetworkTest(
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_FALLBACK);
    }

    private void assertIpAddressArrayEquals(String[] expected, InetAddress[] actual) {
        String[] actualStrings = new String[actual.length];
        for (int i = 0; i < actual.length; i++) {
            actualStrings[i] = actual[i].getHostAddress();
        }
        assertArrayEquals("Array of IP addresses differs", expected, actualStrings);
    }

    @Test
    public void testSendDnsProbeWithTimeout() throws Exception {
        WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        final int shortTimeoutMs = 200;
        // v6 only.
        String[] expected = new String[]{"2001:db8::"};
        mFakeDns.setAnswer("www.google.com", expected, TYPE_AAAA);
        InetAddress[] actual = wnm.sendDnsProbeWithTimeout("www.google.com", shortTimeoutMs);
        assertIpAddressArrayEquals(expected, actual);
        // v4 only.
        expected = new String[]{"192.0.2.1"};
        mFakeDns.setAnswer("www.android.com", expected, TYPE_A);
        actual = wnm.sendDnsProbeWithTimeout("www.android.com", shortTimeoutMs);
        assertIpAddressArrayEquals(expected, actual);
        // Both v4 & v6.
        expected = new String[]{"192.0.2.1", "2001:db8::"};
        mFakeDns.setAnswer("www.googleapis.com", new String[]{"192.0.2.1"}, TYPE_A);
        mFakeDns.setAnswer("www.googleapis.com", new String[]{"2001:db8::"}, TYPE_AAAA);
        actual = wnm.sendDnsProbeWithTimeout("www.googleapis.com", shortTimeoutMs);
        assertIpAddressArrayEquals(expected, actual);
        // Clear DNS response.
        mFakeDns.setAnswer("www.android.com", new String[0], TYPE_A);
        try {
            actual = wnm.sendDnsProbeWithTimeout("www.android.com", shortTimeoutMs);
            fail("No DNS results, expected UnknownHostException");
        } catch (UnknownHostException e) {
        }

        mFakeDns.setAnswer("www.android.com", null, TYPE_A);
        mFakeDns.setAnswer("www.android.com", null, TYPE_AAAA);
        try {
            wnm.sendDnsProbeWithTimeout("www.android.com", shortTimeoutMs);
            fail("DNS query timed out, expected UnknownHostException");
        } catch (UnknownHostException e) {
        }
    }

    @Test
    public void testNotifyNetwork_WithforceReevaluation() throws Exception {
        setValidProbes();
        final NetworkMonitor nm = runValidatedNetworkTest();
        // Verify forceReevaluation will not reset the validation result but only probe result until
        // getting the validation result.
        resetCallbacks();
        setSslException(mHttpsConnection);
        setStatus(mHttpConnection, 500);
        setStatus(mFallbackConnection, 204);
        nm.forceReevaluation(Process.myUid());
        // Expect to send HTTP, HTTPs, FALLBACK and evaluation results.
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_PARTIAL,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_FALLBACK);
    }

    @Test
    public void testNotifyNetwork_NotifyNetworkTestedOldInterfaceVersion() throws Exception {
        // Use old interface version so notifyNetworkTested is used over
        // notifyNetworkTestedWithExtras
        resetCallbacks(4);

        // Trigger Network validation
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);
        final NetworkMonitor nm = makeMonitor(CELL_METERED_CAPABILITIES);
        nm.notifyNetworkConnected(TEST_LINK_PROPERTIES, CELL_METERED_CAPABILITIES);

        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS))
                .notifyNetworkTested(eq(NETWORK_VALIDATION_RESULT_VALID
                        | NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS),
                        eq(null) /* redirectUrl */);
    }

    @Test
    public void testDismissPortalInValidatedNetworkEnabledOsSupported() throws Exception {
        assumeTrue(ShimUtils.isAtLeastR());
        testDismissPortalInValidatedNetworkEnabled(TEST_LOGIN_URL, TEST_LOGIN_URL);
    }

    @Test
    public void testDismissPortalInValidatedNetworkEnabledOsSupported_NullLocationUrl()
            throws Exception {
        assumeTrue(ShimUtils.isAtLeastR());
        testDismissPortalInValidatedNetworkEnabled(TEST_HTTP_URL, null /* locationUrl */);
    }

    @Test
    public void testDismissPortalInValidatedNetworkEnabledOsSupported_InvalidLocationUrl()
            throws Exception {
        assumeTrue(ShimUtils.isAtLeastR());
        testDismissPortalInValidatedNetworkEnabled(TEST_HTTP_URL, TEST_RELATIVE_URL);
    }

    @Test
    public void testDismissPortalInValidatedNetworkEnabledOsNotSupported() throws Exception {
        assumeFalse(ShimUtils.isAtLeastR());
        testDismissPortalInValidatedNetworkEnabled(TEST_HTTP_URL, TEST_LOGIN_URL);
    }

    private void testDismissPortalInValidatedNetworkEnabled(String expectedUrl, String locationUrl)
            throws Exception {
        setDismissPortalInValidatedNetwork(true);
        setSslException(mHttpsConnection);
        setPortal302(mHttpConnection);
        when(mHttpConnection.getHeaderField(eq("location"))).thenReturn(locationUrl);
        final NetworkMonitor nm = makeMonitor(CELL_METERED_CAPABILITIES);
        notifyNetworkConnected(nm, CELL_METERED_CAPABILITIES);

        verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS).times(1))
            .showProvisioningNotification(any(), any());

        assertEquals(1, mRegisteredReceivers.size());
        // Check that startCaptivePortalApp sends the expected intent.
        nm.launchCaptivePortalApp();

        final ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        final ArgumentCaptor<Network> networkCaptor = ArgumentCaptor.forClass(Network.class);
        verify(mCm, timeout(HANDLER_TIMEOUT_MS).times(1))
            .startCaptivePortalApp(networkCaptor.capture(), bundleCaptor.capture());
        verify(mNotifier).notifyCaptivePortalValidationPending(networkCaptor.getValue());
        final Bundle bundle = bundleCaptor.getValue();
        final Network bundleNetwork = bundle.getParcelable(ConnectivityManager.EXTRA_NETWORK);
        assertEquals(TEST_NETID, bundleNetwork.netId);
        // Network is passed both in bundle and as parameter, as the bundle is opaque to the
        // framework and only intended for the captive portal app, but the framework needs
        // the network to identify the right NetworkMonitor.
        assertEquals(TEST_NETID, networkCaptor.getValue().netId);
        // Portal URL should be redirect URL.
        final String redirectUrl = bundle.getString(ConnectivityManager.EXTRA_CAPTIVE_PORTAL_URL);
        assertEquals(expectedUrl, redirectUrl);
    }

    @Test
    public void testEvaluationState_clearProbeResults() throws Exception {
        setValidProbes();
        final NetworkMonitor nm = runValidatedNetworkTest();
        nm.getEvaluationState().clearProbeResults();
        // Verify probe results are all reset and only evaluation result left.
        assertEquals(NETWORK_VALIDATION_RESULT_VALID,
                nm.getEvaluationState().getEvaluationResult());
        assertEquals(0, nm.getEvaluationState().getProbeResults());
    }

    @Test
    public void testEvaluationState_reportProbeResult() throws Exception {
        setValidProbes();
        final NetworkMonitor nm = runValidatedNetworkTest();

        resetCallbacks();

        nm.reportHttpProbeResult(NETWORK_VALIDATION_PROBE_HTTP,
                CaptivePortalProbeResult.success(1 << PROBE_HTTP));
        // Verify result should be appended and notifyNetworkTestedWithExtras callback is triggered
        // once.
        assertEquals(NETWORK_VALIDATION_RESULT_VALID,
                nm.getEvaluationState().getEvaluationResult());
        assertEquals(NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS
                | NETWORK_VALIDATION_PROBE_HTTP, nm.getEvaluationState().getProbeResults());

        nm.reportHttpProbeResult(NETWORK_VALIDATION_PROBE_HTTP,
                CaptivePortalProbeResult.failed(1 << PROBE_HTTP));
        // Verify DNS probe result should not be cleared.
        assertEquals(NETWORK_VALIDATION_PROBE_DNS,
                nm.getEvaluationState().getProbeResults() & NETWORK_VALIDATION_PROBE_DNS);
    }

    @Test
    public void testEvaluationState_reportEvaluationResult() throws Exception {
        setStatus(mHttpsConnection, 500);
        setStatus(mHttpConnection, 204);
        final NetworkMonitor nm = runNetworkTest(NETWORK_VALIDATION_RESULT_PARTIAL,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP,
                null /* redirectUrl */);

        nm.getEvaluationState().reportEvaluationResult(NETWORK_VALIDATION_RESULT_VALID,
                null /* redirectUrl */);
        verifyNetworkTested(NETWORK_VALIDATION_RESULT_VALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP);

        nm.getEvaluationState().reportEvaluationResult(
                NETWORK_VALIDATION_RESULT_VALID | NETWORK_VALIDATION_RESULT_PARTIAL,
                null /* redirectUrl */);
        verifyNetworkTested(
                NETWORK_VALIDATION_RESULT_VALID | NETWORK_VALIDATION_RESULT_PARTIAL,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP);

        nm.getEvaluationState().reportEvaluationResult(VALIDATION_RESULT_INVALID,
                TEST_REDIRECT_URL);
        verifyNetworkTested(VALIDATION_RESULT_INVALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTP,
                TEST_REDIRECT_URL);
    }

    @Test
    public void testExtractCharset() {
        assertEquals(StandardCharsets.UTF_8, extractCharset(null));
        assertEquals(StandardCharsets.UTF_8, extractCharset("text/html;charset=utf-8"));
        assertEquals(StandardCharsets.UTF_8, extractCharset("text/html;charset=UtF-8"));
        assertEquals(StandardCharsets.UTF_8, extractCharset("text/html; Charset=\"utf-8\""));
        assertEquals(StandardCharsets.UTF_8, extractCharset("image/png"));
        assertEquals(StandardCharsets.UTF_8, extractCharset("Text/HTML;"));
        assertEquals(StandardCharsets.UTF_8, extractCharset("multipart/form-data; boundary=-aa*-"));
        assertEquals(StandardCharsets.UTF_8, extractCharset("text/plain;something=else"));
        assertEquals(StandardCharsets.UTF_8, extractCharset("text/plain;charset=ImNotACharset"));

        assertEquals(StandardCharsets.ISO_8859_1, extractCharset("text/plain; CharSeT=ISO-8859-1"));
        assertEquals(Charset.forName("Shift_JIS"), extractCharset("text/plain;charset=Shift_JIS"));
        assertEquals(Charset.forName("Windows-1251"), extractCharset(
                "text/plain;charset=Windows-1251 ; somethingelse"));
    }

    @Test
    public void testReadAsString() throws IOException {
        final String repeatedString = "1aテスト-?";
        // Infinite stream repeating characters
        class TestInputStream extends InputStream {
            private final byte[] mBytes = repeatedString.getBytes(StandardCharsets.UTF_8);
            private int mPosition = -1;

            @Override
            public int read() {
                mPosition = (mPosition + 1) % mBytes.length;
                return mBytes[mPosition];
            }
        }

        final String readString = NetworkMonitor.readAsString(new TestInputStream(),
                1500 /* maxLength */, StandardCharsets.UTF_8);

        assertEquals(1500, readString.length());
        for (int i = 0; i < readString.length(); i++) {
            assertEquals(repeatedString.charAt(i % repeatedString.length()), readString.charAt(i));
        }
    }

    @Test
    public void testReadAsString_StreamShorterThanLimit() throws Exception {
        final WrappedNetworkMonitor wnm = makeCellNotMeteredNetworkMonitor();
        final byte[] content = "The HTTP response code is 200 but it is not a captive portal."
                .getBytes(StandardCharsets.UTF_8);
        assertEquals(new String(content), wnm.readAsString(new ByteArrayInputStream(content),
                content.length, StandardCharsets.UTF_8));
        // Test the case that the stream ends earlier than the limit.
        assertEquals(new String(content), wnm.readAsString(new ByteArrayInputStream(content),
                content.length + 10, StandardCharsets.UTF_8));
    }

    @Test
    public void testMultipleProbesOnPortalNetwork() throws Exception {
        setupResourceForMultipleProbes();
        // One of the http probes is portal, then result is portal.
        setPortal302(mOtherHttpConnection1);
        runPortalNetworkTest();
        // Get conclusive result from one of the HTTP probe. Expect to create 2 HTTP and 2 HTTPS
        // probes as resource configuration, but the portal can be detected before other probes
        // start.
        verify(mCleartextDnsNetwork, atMost(4)).openConnection(any());
        verify(mCleartextDnsNetwork, atLeastOnce()).openConnection(any());
        verify(mOtherHttpConnection1).getResponseCode();
    }

    @Test
    public void testMultipleProbesOnValidNetwork() throws Exception {
        setupResourceForMultipleProbes();
        // One of the https probes succeeds, then it's validated.
        setStatus(mOtherHttpsConnection2, 204);
        runValidatedNetworkTest();
        // Get conclusive result from one of the HTTPS probe. Expect to create 2 HTTP and 2 HTTPS
        // probes as resource configuration, but the network may validate from the HTTPS probe
        // before other probes start.
        verify(mCleartextDnsNetwork, atMost(4)).openConnection(any());
        verify(mCleartextDnsNetwork, atLeastOnce()).openConnection(any());
        verify(mOtherHttpsConnection2).getResponseCode();
    }

    @Test
    public void testMultipleProbesOnInValidNetworkForPrioritizedResource() throws Exception {
        setupResourceForMultipleProbes();
        // The configuration resource is prioritized. Only use configurations from resource.(i.e
        // Only configuration for mOtherHttpsConnection2, mOtherHttpsConnection2,
        // mOtherHttpConnection2, mOtherHttpConnection2 will affect the result.)
        // Configure mHttpsConnection is no-op.
        setStatus(mHttpsConnection, 204);
        runFailedNetworkTest();
        // No conclusive result from both HTTP and HTTPS probes. Expect to create 2 HTTP and 2 HTTPS
        // probes as resource configuration. All probes are expected to have been run because this
        // network is set to never validate (no probe has a success or portal result), so NM tests
        // all probes to completion.
        verify(mCleartextDnsNetwork, times(4)).openConnection(any());
        verify(mHttpsConnection, never()).getResponseCode();
    }

    @Test
    public void testMultipleProbesOnInValidNetwork() throws Exception {
        setupResourceForMultipleProbes();
        runFailedNetworkTest();
        // No conclusive result from both HTTP and HTTPS probes. Expect to create 2 HTTP and 2 HTTPS
        // probes as resource configuration.
        verify(mCleartextDnsNetwork, times(4)).openConnection(any());
    }

    private void setupResourceForMultipleProbes() {
        // Configure the resource to send multiple probe.
        when(mResources.getStringArray(R.array.config_captive_portal_https_urls))
                .thenReturn(TEST_HTTPS_URLS);
        when(mResources.getStringArray(R.array.config_captive_portal_http_urls))
                .thenReturn(TEST_HTTP_URLS);
    }

    private void makeDnsTimeoutEvent(WrappedNetworkMonitor wrappedMonitor, int count) {
        for (int i = 0; i < count; i++) {
            wrappedMonitor.getDnsStallDetector().accumulateConsecutiveDnsTimeoutCount(
                    RETURN_CODE_DNS_TIMEOUT);
        }
    }

    private void makeDnsSuccessEvent(WrappedNetworkMonitor wrappedMonitor, int count) {
        for (int i = 0; i < count; i++) {
            wrappedMonitor.getDnsStallDetector().accumulateConsecutiveDnsTimeoutCount(
                    RETURN_CODE_DNS_SUCCESS);
        }
    }

    private DataStallDetectionStats makeEmptyDataStallDetectionStats() {
        return new DataStallDetectionStats.Builder().build();
    }

    private void setDataStallEvaluationType(int type) {
        when(mDependencies.getDeviceConfigPropertyInt(any(),
            eq(CONFIG_DATA_STALL_EVALUATION_TYPE), anyInt())).thenReturn(type);
    }

    private void setMinDataStallEvaluateInterval(int time) {
        when(mDependencies.getDeviceConfigPropertyInt(any(),
            eq(CONFIG_DATA_STALL_MIN_EVALUATE_INTERVAL), anyInt())).thenReturn(time);
    }

    private void setValidDataStallDnsTimeThreshold(int time) {
        when(mDependencies.getDeviceConfigPropertyInt(any(),
            eq(CONFIG_DATA_STALL_VALID_DNS_TIME_THRESHOLD), anyInt())).thenReturn(time);
    }

    private void setConsecutiveDnsTimeoutThreshold(int num) {
        when(mDependencies.getDeviceConfigPropertyInt(any(),
            eq(CONFIG_DATA_STALL_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD), anyInt())).thenReturn(num);
    }

    private void setTcpPollingInterval(int time) {
        when(mDependencies.getDeviceConfigPropertyInt(any(),
                eq(CONFIG_DATA_STALL_TCP_POLLING_INTERVAL), anyInt())).thenReturn(time);
    }

    private void setFallbackUrl(String url) {
        when(mDependencies.getSetting(any(),
                eq(Settings.Global.CAPTIVE_PORTAL_FALLBACK_URL), any())).thenReturn(url);
    }

    private void setOtherFallbackUrls(String urls) {
        when(mDependencies.getDeviceConfigProperty(any(),
                eq(CAPTIVE_PORTAL_OTHER_FALLBACK_URLS), any())).thenReturn(urls);
    }

    private void setFallbackSpecs(String specs) {
        when(mDependencies.getDeviceConfigProperty(any(),
                eq(CAPTIVE_PORTAL_FALLBACK_PROBE_SPECS), any())).thenReturn(specs);
    }

    private void setCaptivePortalMode(int mode) {
        when(mDependencies.getSetting(any(),
                eq(Settings.Global.CAPTIVE_PORTAL_MODE), anyInt())).thenReturn(mode);
    }

    private void setDismissPortalInValidatedNetwork(boolean enabled) {
        when(mDependencies.isFeatureEnabled(any(), any(),
                eq(DISMISS_PORTAL_IN_VALIDATED_NETWORK), anyBoolean())).thenReturn(enabled);
    }

    private void setDeviceConfig(String key, String value) {
        doReturn(value).when(mDependencies).getDeviceConfigProperty(eq(NAMESPACE_CONNECTIVITY),
                eq(key), any() /* defaultValue */);
    }

    private NetworkMonitor runPortalNetworkTest() throws RemoteException {
        final NetworkMonitor nm = runNetworkTest(VALIDATION_RESULT_PORTAL,
                0 /* probesSucceeded */, TEST_LOGIN_URL);
        assertEquals(1, mRegisteredReceivers.size());
        return nm;
    }

    private NetworkMonitor runNoValidationNetworkTest() throws RemoteException {
        final NetworkMonitor nm = runNetworkTest(NETWORK_VALIDATION_RESULT_VALID,
                0 /* probesSucceeded */, null /* redirectUrl */);
        assertEquals(0, mRegisteredReceivers.size());
        return nm;
    }

    private NetworkMonitor runFailedNetworkTest() throws RemoteException {
        final NetworkMonitor nm = runNetworkTest(
                VALIDATION_RESULT_INVALID, 0 /* probesSucceeded */, null /* redirectUrl */);
        assertEquals(0, mRegisteredReceivers.size());
        return nm;
    }

    private NetworkMonitor runPartialConnectivityNetworkTest(int probesSucceeded)
            throws RemoteException {
        final NetworkMonitor nm = runNetworkTest(NETWORK_VALIDATION_RESULT_PARTIAL,
                probesSucceeded, null /* redirectUrl */);
        assertEquals(0, mRegisteredReceivers.size());
        return nm;
    }

    private NetworkMonitor runValidatedNetworkTest() throws RemoteException {
        // Expect to send HTTPS and evaluation results.
        return runNetworkTest(NETWORK_VALIDATION_RESULT_VALID,
                NETWORK_VALIDATION_PROBE_DNS | NETWORK_VALIDATION_PROBE_HTTPS,
                null /* redirectUrl */);
    }

    private NetworkMonitor runNetworkTest(int testResult, int probesSucceeded, String redirectUrl)
            throws RemoteException {
        return runNetworkTest(TEST_LINK_PROPERTIES, CELL_METERED_CAPABILITIES, testResult,
                probesSucceeded, redirectUrl);
    }

    private NetworkMonitor runNetworkTest(LinkProperties lp, NetworkCapabilities nc,
            int testResult, int probesSucceeded, String redirectUrl) throws RemoteException {
        final NetworkMonitor monitor = makeMonitor(nc);
        monitor.notifyNetworkConnected(lp, nc);
        verifyNetworkTested(testResult, probesSucceeded, redirectUrl);
        HandlerUtilsKt.waitForIdle(monitor.getHandler(), HANDLER_TIMEOUT_MS);

        return monitor;
    }

    private void verifyNetworkTested(int testResult, int probesSucceeded) throws RemoteException {
        verifyNetworkTested(testResult, probesSucceeded, null /* redirectUrl */);
    }

    private void verifyNetworkTested(int testResult, int probesSucceeded, String redirectUrl)
            throws RemoteException {
        try {
            verify(mCallbacks, timeout(HANDLER_TIMEOUT_MS)).notifyNetworkTestedWithExtras(
                    matchNetworkTestResultParcelable(testResult, probesSucceeded, redirectUrl));
        } catch (AssertionFailedError e) {
            // Capture the callbacks up to now to give a better error message
            final ArgumentCaptor<NetworkTestResultParcelable> captor =
                    ArgumentCaptor.forClass(NetworkTestResultParcelable.class);

            // Call verify() again to verify the same method call verified by the previous verify
            // call which failed, but this time use a captor to log the exact parcel sent by
            // NetworkMonitor.
            // This assertion will fail if notifyNetworkTested was not called at all.
            verify(mCallbacks).notifyNetworkTestedWithExtras(captor.capture());

            final NetworkTestResultParcelable lastResult = captor.getValue();
            fail(String.format("notifyNetworkTestedWithExtras was not called with the "
                    + "expected result within timeout. "
                    + "Expected result %d, probes succeeded %d, redirect URL %s, "
                    + "last result was (%d, %d, %s).",
                    testResult, probesSucceeded, redirectUrl,
                    lastResult.result, lastResult.probesSucceeded, lastResult.redirectUrl));
        }
    }

    private void notifyNetworkConnected(NetworkMonitor nm, NetworkCapabilities nc) {
        nm.notifyNetworkConnected(TEST_LINK_PROPERTIES, nc);
    }

    private void setSslException(HttpURLConnection connection) throws IOException {
        doThrow(new SSLHandshakeException("Invalid cert")).when(connection).getResponseCode();
    }

    private void setValidProbes() throws IOException {
        setStatus(mHttpsConnection, 204);
        setStatus(mHttpConnection, 204);
    }

    private void set302(HttpURLConnection connection, String location) throws IOException {
        setStatus(connection, 302);
        doReturn(location).when(connection).getHeaderField(LOCATION_HEADER);
    }

    private void setPortal302(HttpURLConnection connection) throws IOException {
        set302(connection, TEST_LOGIN_URL);
    }

    private void setApiContent(HttpURLConnection connection, String content) throws IOException {
        setStatus(connection, 200);
        final Map<String, List<String>> headerFields = new HashMap<>();
        headerFields.put(
                CONTENT_TYPE_HEADER, singletonList("application/captive+json;charset=UTF-8"));
        doReturn(headerFields).when(connection).getHeaderFields();
        doReturn(new ByteArrayInputStream(content.getBytes(StandardCharsets.UTF_8)))
                .when(connection).getInputStream();
    }

    private void setStatus(HttpURLConnection connection, int status) throws IOException {
        doReturn(status).when(connection).getResponseCode();
    }

    private void generateTimeoutDnsEvent(DataStallDetectionStats.Builder stats, int num) {
        for (int i = 0; i < num; i++) {
            stats.addDnsEvent(RETURN_CODE_DNS_TIMEOUT, TEST_ELAPSED_TIME_MS /* timeMs */);
        }
    }

    private void generateTestTcpStats(DataStallDetectionStats.Builder stats) {
        when(mTst.getLatestPacketFailPercentage()).thenReturn(TEST_TCP_FAIL_RATE);
        when(mTst.getSentSinceLastRecv()).thenReturn(TEST_TCP_PACKET_COUNT);
        stats.setTcpFailRate(TEST_TCP_FAIL_RATE).setTcpSentSinceLastRecv(TEST_TCP_PACKET_COUNT);
    }

    private NetworkTestResultParcelable matchNetworkTestResultParcelable(final int result,
            final int probesSucceeded) {
        return matchNetworkTestResultParcelable(result, probesSucceeded, null /* redirectUrl */);
    }

    private NetworkTestResultParcelable matchNetworkTestResultParcelable(final int result,
            final int probesSucceeded, String redirectUrl) {
        // TODO: also verify probesAttempted
        return argThat(p -> p.result == result && p.probesSucceeded == probesSucceeded
                && Objects.equals(p.redirectUrl, redirectUrl));
    }

    private DataStallReportParcelable matchDnsDataStallParcelable(final int timeoutCount) {
        return argThat(p -> p.detectionMethod == ConstantsShim.DETECTION_METHOD_DNS_EVENTS
                && p.dnsConsecutiveTimeouts == timeoutCount);
    }

    private DataStallReportParcelable matchTcpDataStallParcelable() {
        return argThat(p -> p.detectionMethod == ConstantsShim.DETECTION_METHOD_TCP_METRICS);
    }
}

