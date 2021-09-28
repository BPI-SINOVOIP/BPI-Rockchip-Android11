/*
 * Copyright (C) 2014 The Android Open Source Project
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
import static android.net.CaptivePortal.APP_RETURN_UNWANTED;
import static android.net.CaptivePortal.APP_RETURN_WANTED_AS_IS;
import static android.net.ConnectivityManager.EXTRA_CAPTIVE_PORTAL_PROBE_SPEC;
import static android.net.ConnectivityManager.EXTRA_CAPTIVE_PORTAL_URL;
import static android.net.DnsResolver.FLAG_EMPTY;
import static android.net.INetworkMonitor.NETWORK_TEST_RESULT_INVALID;
import static android.net.INetworkMonitor.NETWORK_TEST_RESULT_PARTIAL_CONNECTIVITY;
import static android.net.INetworkMonitor.NETWORK_TEST_RESULT_VALID;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_DNS;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_FALLBACK;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_HTTP;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_HTTPS;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_PROBE_PRIVDNS;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_RESULT_PARTIAL;
import static android.net.INetworkMonitor.NETWORK_VALIDATION_RESULT_VALID;
import static android.net.NetworkCapabilities.NET_CAPABILITY_NOT_METERED;
import static android.net.captiveportal.CaptivePortalProbeSpec.parseCaptivePortalProbeSpecs;
import static android.net.metrics.ValidationProbeEvent.DNS_FAILURE;
import static android.net.metrics.ValidationProbeEvent.DNS_SUCCESS;
import static android.net.metrics.ValidationProbeEvent.PROBE_FALLBACK;
import static android.net.metrics.ValidationProbeEvent.PROBE_PRIVDNS;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_EVALUATION_TYPE;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_MIN_EVALUATE_INTERVAL;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_TCP_POLLING_INTERVAL;
import static android.net.util.DataStallUtils.CONFIG_DATA_STALL_VALID_DNS_TIME_THRESHOLD;
import static android.net.util.DataStallUtils.DATA_STALL_EVALUATION_TYPE_DNS;
import static android.net.util.DataStallUtils.DATA_STALL_EVALUATION_TYPE_NONE;
import static android.net.util.DataStallUtils.DATA_STALL_EVALUATION_TYPE_TCP;
import static android.net.util.DataStallUtils.DEFAULT_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD;
import static android.net.util.DataStallUtils.DEFAULT_DATA_STALL_EVALUATION_TYPES;
import static android.net.util.DataStallUtils.DEFAULT_DATA_STALL_MIN_EVALUATE_TIME_MS;
import static android.net.util.DataStallUtils.DEFAULT_DATA_STALL_VALID_DNS_TIME_THRESHOLD_MS;
import static android.net.util.DataStallUtils.DEFAULT_DNS_LOG_SIZE;
import static android.net.util.DataStallUtils.DEFAULT_TCP_POLLING_INTERVAL_MS;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_FALLBACK_PROBE_SPECS;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_FALLBACK_URL;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_HTTPS_URL;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_HTTP_URL;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_MODE;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_MODE_IGNORE;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_MODE_PROMPT;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_OTHER_FALLBACK_URLS;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_OTHER_HTTPS_URLS;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_OTHER_HTTP_URLS;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_USER_AGENT;
import static android.net.util.NetworkStackUtils.CAPTIVE_PORTAL_USE_HTTPS;
import static android.net.util.NetworkStackUtils.DEFAULT_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT;
import static android.net.util.NetworkStackUtils.DEFAULT_CAPTIVE_PORTAL_FALLBACK_PROBE_SPECS;
import static android.net.util.NetworkStackUtils.DEFAULT_CAPTIVE_PORTAL_HTTPS_URLS;
import static android.net.util.NetworkStackUtils.DEFAULT_CAPTIVE_PORTAL_HTTP_URLS;
import static android.net.util.NetworkStackUtils.DISMISS_PORTAL_IN_VALIDATED_NETWORK;
import static android.net.util.NetworkStackUtils.DNS_PROBE_PRIVATE_IP_NO_INTERNET_VERSION;
import static android.net.util.NetworkStackUtils.TEST_CAPTIVE_PORTAL_HTTPS_URL;
import static android.net.util.NetworkStackUtils.TEST_CAPTIVE_PORTAL_HTTP_URL;
import static android.net.util.NetworkStackUtils.TEST_URL_EXPIRATION_TIME;
import static android.net.util.NetworkStackUtils.isEmpty;
import static android.net.util.NetworkStackUtils.isIPv6ULA;
import static android.provider.DeviceConfig.NAMESPACE_CONNECTIVITY;

import static com.android.networkstack.apishim.ConstantsShim.DETECTION_METHOD_DNS_EVENTS;
import static com.android.networkstack.apishim.ConstantsShim.DETECTION_METHOD_TCP_METRICS;
import static com.android.networkstack.apishim.ConstantsShim.TRANSPORT_TEST;
import static com.android.networkstack.util.DnsUtils.PRIVATE_DNS_PROBE_HOST_SUFFIX;
import static com.android.networkstack.util.DnsUtils.TYPE_ADDRCONFIG;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.net.ConnectivityManager;
import android.net.DataStallReportParcelable;
import android.net.DnsResolver;
import android.net.INetworkMonitorCallbacks;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkTestResultParcelable;
import android.net.ProxyInfo;
import android.net.TrafficStats;
import android.net.Uri;
import android.net.captiveportal.CapportApiProbeResult;
import android.net.captiveportal.CaptivePortalProbeResult;
import android.net.captiveportal.CaptivePortalProbeSpec;
import android.net.metrics.IpConnectivityLog;
import android.net.metrics.NetworkEvent;
import android.net.metrics.ValidationProbeEvent;
import android.net.shared.NetworkMonitorUtils;
import android.net.shared.PrivateDnsConfig;
import android.net.util.DataStallUtils.EvaluationType;
import android.net.util.NetworkStackUtils;
import android.net.util.SharedLog;
import android.net.util.Stopwatch;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Message;
import android.os.Process;
import android.os.RemoteException;
import android.os.SystemClock;
import android.provider.DeviceConfig;
import android.provider.Settings;
import android.stats.connectivity.ProbeResult;
import android.stats.connectivity.ProbeType;
import android.telephony.CellIdentityNr;
import android.telephony.CellInfo;
import android.telephony.CellInfoGsm;
import android.telephony.CellInfoLte;
import android.telephony.CellInfoNr;
import android.telephony.CellInfoTdscdma;
import android.telephony.CellInfoWcdma;
import android.telephony.CellSignalStrength;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;
import android.util.SparseArray;

import androidx.annotation.ArrayRes;
import androidx.annotation.BoolRes;
import androidx.annotation.IntegerRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.util.RingBufferIndices;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;
import com.android.internal.util.TrafficStatsConstants;
import com.android.networkstack.NetworkStackNotifier;
import com.android.networkstack.R;
import com.android.networkstack.apishim.CaptivePortalDataShimImpl;
import com.android.networkstack.apishim.NetworkInformationShimImpl;
import com.android.networkstack.apishim.common.CaptivePortalDataShim;
import com.android.networkstack.apishim.common.ShimUtils;
import com.android.networkstack.apishim.common.UnsupportedApiLevelException;
import com.android.networkstack.metrics.DataStallDetectionStats;
import com.android.networkstack.metrics.DataStallStatsUtils;
import com.android.networkstack.metrics.NetworkValidationMetrics;
import com.android.networkstack.netlink.TcpSocketTracker;
import com.android.networkstack.util.DnsUtils;
import com.android.server.NetworkStackService.NetworkStackServiceManager;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.InterruptedIOException;
import java.net.HttpURLConnection;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.UnknownHostException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Random;
import java.util.StringJoiner;
import java.util.UUID;
import java.util.concurrent.CompletionService;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorCompletionService;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Function;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

/**
 * {@hide}
 */
public class NetworkMonitor extends StateMachine {
    private static final String TAG = NetworkMonitor.class.getSimpleName();
    private static final boolean DBG  = true;
    private static final boolean VDBG = false;
    private static final boolean VDBG_STALL = Log.isLoggable(TAG, Log.DEBUG);
    private static final String DEFAULT_USER_AGENT    = "Mozilla/5.0 (X11; Linux x86_64) "
                                                      + "AppleWebKit/537.36 (KHTML, like Gecko) "
                                                      + "Chrome/60.0.3112.32 Safari/537.36";

    @VisibleForTesting
    static final String CONFIG_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT =
            "captive_portal_dns_probe_timeout";

    private static final int SOCKET_TIMEOUT_MS = 10000;
    private static final int PROBE_TIMEOUT_MS  = 3000;
    private static final long TEST_URL_EXPIRATION_MS = TimeUnit.MINUTES.toMillis(10);

    private static final int UNSET_MCC_OR_MNC = -1;

    private static final int CAPPORT_API_MAX_JSON_LENGTH = 4096;
    private static final String ACCEPT_HEADER = "Accept";
    private static final String CONTENT_TYPE_HEADER = "Content-Type";
    private static final String CAPPORT_API_CONTENT_TYPE = "application/captive+json";

    enum EvaluationResult {
        VALIDATED(true),
        CAPTIVE_PORTAL(false);
        final boolean mIsValidated;
        EvaluationResult(boolean isValidated) {
            this.mIsValidated = isValidated;
        }
    }

    enum ValidationStage {
        FIRST_VALIDATION(true),
        REVALIDATION(false);
        final boolean mIsFirstValidation;
        ValidationStage(boolean isFirstValidation) {
            this.mIsFirstValidation = isFirstValidation;
        }
    }

    @VisibleForTesting
    protected static final class MccMncOverrideInfo {
        public final int mcc;
        public final int mnc;
        MccMncOverrideInfo(int mcc, int mnc) {
            this.mcc = mcc;
            this.mnc = mnc;
        }
    }

    @VisibleForTesting
    protected static final SparseArray<MccMncOverrideInfo> sCarrierIdToMccMnc = new SparseArray<>();

    static {
        // CTC
        sCarrierIdToMccMnc.put(1854, new MccMncOverrideInfo(460, 03));
    }

    /**
     * ConnectivityService has sent a notification to indicate that network has connected.
     * Initiates Network Validation.
     */
    private static final int CMD_NETWORK_CONNECTED = 1;

    /**
     * Message to self indicating it's time to evaluate a network's connectivity.
     * arg1 = Token to ignore old messages.
     */
    private static final int CMD_REEVALUATE = 6;

    /**
     * ConnectivityService has sent a notification to indicate that network has disconnected.
     */
    private static final int CMD_NETWORK_DISCONNECTED = 7;

    /**
     * Force evaluation even if it has succeeded in the past.
     * arg1 = UID responsible for requesting this reeval.  Will be billed for data.
     */
    private static final int CMD_FORCE_REEVALUATION = 8;

    /**
     * Message to self indicating captive portal app finished.
     * arg1 = one of: APP_RETURN_DISMISSED,
     *                APP_RETURN_UNWANTED,
     *                APP_RETURN_WANTED_AS_IS
     * obj = mCaptivePortalLoggedInResponseToken as String
     */
    private static final int CMD_CAPTIVE_PORTAL_APP_FINISHED = 9;

    /**
     * Message indicating sign-in app should be launched.
     * Sent by mLaunchCaptivePortalAppBroadcastReceiver when the
     * user touches the sign in notification, or sent by
     * ConnectivityService when the user touches the "sign into
     * network" button in the wifi access point detail page.
     */
    private static final int CMD_LAUNCH_CAPTIVE_PORTAL_APP = 11;

    /**
     * Retest network to see if captive portal is still in place.
     * arg1 = UID responsible for requesting this reeval.  Will be billed for data.
     *        0 indicates self-initiated, so nobody to blame.
     */
    private static final int CMD_CAPTIVE_PORTAL_RECHECK = 12;

    /**
     * ConnectivityService notifies NetworkMonitor of settings changes to
     * Private DNS. If a DNS resolution is required, e.g. for DNS-over-TLS in
     * strict mode, then an event is sent back to ConnectivityService with the
     * result of the resolution attempt.
     *
     * A separate message is used to trigger (re)evaluation of the Private DNS
     * configuration, so that the message can be handled as needed in different
     * states, including being ignored until after an ongoing captive portal
     * validation phase is completed.
     */
    private static final int CMD_PRIVATE_DNS_SETTINGS_CHANGED = 13;
    private static final int CMD_EVALUATE_PRIVATE_DNS = 15;

    /**
     * Message to self indicating captive portal detection is completed.
     * obj = CaptivePortalProbeResult for detection result;
     */
    private static final int CMD_PROBE_COMPLETE = 16;

    /**
     * ConnectivityService notifies NetworkMonitor of DNS query responses event.
     * arg1 = returncode in OnDnsEvent which indicates the response code for the DNS query.
     */
    private static final int EVENT_DNS_NOTIFICATION = 17;

    /**
     * ConnectivityService notifies NetworkMonitor that the user accepts partial connectivity and
     * NetworkMonitor should ignore the https probe.
     */
    private static final int EVENT_ACCEPT_PARTIAL_CONNECTIVITY = 18;

    /**
     * ConnectivityService notifies NetworkMonitor of changed LinkProperties.
     * obj = new LinkProperties.
     */
    private static final int EVENT_LINK_PROPERTIES_CHANGED = 19;

    /**
     * ConnectivityService notifies NetworkMonitor of changed NetworkCapabilities.
     * obj = new NetworkCapabilities.
     */
    private static final int EVENT_NETWORK_CAPABILITIES_CHANGED = 20;

    /**
     * Message to self to poll current tcp status from kernel.
     */
    private static final int EVENT_POLL_TCPINFO = 21;

    /**
     * Message to self to do the bandwidth check in EvaluatingBandwidthState.
     */
    private static final int CMD_EVALUATE_BANDWIDTH = 22;

    /**
     * Message to self to know the bandwidth check is completed.
     */
    private static final int CMD_BANDWIDTH_CHECK_COMPLETE = 23;

    /**
     * Message to self to know the bandwidth check is timeouted.
     */
    private static final int CMD_BANDWIDTH_CHECK_TIMEOUT = 24;

    // Start mReevaluateDelayMs at this value and double.
    @VisibleForTesting
    static final int INITIAL_REEVALUATE_DELAY_MS = 1000;
    private static final int MAX_REEVALUATE_DELAY_MS = 10 * 60 * 1000;
    // Default timeout of evaluating network bandwidth.
    private static final int DEFAULT_EVALUATING_BANDWIDTH_TIMEOUT_MS = 10_000;
    // Before network has been evaluated this many times, ignore repeated reevaluate requests.
    private static final int IGNORE_REEVALUATE_ATTEMPTS = 5;
    private int mReevaluateToken = 0;
    private static final int NO_UID = 0;
    private static final int INVALID_UID = -1;
    private int mUidResponsibleForReeval = INVALID_UID;
    // Stop blaming UID that requested re-evaluation after this many attempts.
    private static final int BLAME_FOR_EVALUATION_ATTEMPTS = 5;
    // Delay between reevaluations once a captive portal has been found.
    private static final int CAPTIVE_PORTAL_REEVALUATE_DELAY_MS = 10 * 60 * 1000;
    private static final int NETWORK_VALIDATION_RESULT_INVALID = 0;
    // Max thread pool size for parallel probing. Fixed thread pool size to control the thread
    // number used for either HTTP or HTTPS probing.
    @VisibleForTesting
    static final int MAX_PROBE_THREAD_POOL_SIZE = 5;
    private String mPrivateDnsProviderHostname = "";

    private final Context mContext;
    private final INetworkMonitorCallbacks mCallback;
    private final int mCallbackVersion;
    private final Network mCleartextDnsNetwork;
    private final Network mNetwork;
    private final TelephonyManager mTelephonyManager;
    private final WifiManager mWifiManager;
    private final ConnectivityManager mCm;
    @Nullable
    private final NetworkStackNotifier mNotifier;
    private final IpConnectivityLog mMetricsLog;
    private final Dependencies mDependencies;
    private final TcpSocketTracker mTcpTracker;
    // Configuration values for captive portal detection probes.
    private final String mCaptivePortalUserAgent;
    private final URL[] mCaptivePortalFallbackUrls;
    @NonNull
    private final URL[] mCaptivePortalHttpUrls;
    @NonNull
    private final URL[] mCaptivePortalHttpsUrls;
    @Nullable
    private final CaptivePortalProbeSpec[] mCaptivePortalFallbackSpecs;
    // Configuration values for network bandwidth check.
    @Nullable
    private final String mEvaluatingBandwidthUrl;
    private final int mMaxRetryTimerMs;
    private final int mEvaluatingBandwidthTimeoutMs;
    private final AtomicInteger mNextEvaluatingBandwidthThreadId = new AtomicInteger(1);

    private NetworkCapabilities mNetworkCapabilities;
    private LinkProperties mLinkProperties;

    @VisibleForTesting
    protected boolean mIsCaptivePortalCheckEnabled;

    private boolean mUseHttps;
    /**
     * The total number of completed validation attempts (network validated or a captive portal was
     * detected) for this NetworkMonitor instance.
     * This does not include attempts that were interrupted, retried or finished with a result that
     * is not success or portal. See {@code mValidationIndex} in {@link NetworkValidationMetrics}
     * for a count of all attempts.
     * TODO: remove when removing legacy metrics.
     */
    private int mValidations = 0;

    // Set if the user explicitly selected "Do not use this network" in captive portal sign-in app.
    private boolean mUserDoesNotWant = false;
    // Avoids surfacing "Sign in to network" notification.
    private boolean mDontDisplaySigninNotification = false;
    // Set to true once the evaluating network bandwidth is passed or the captive portal respond
    // APP_RETURN_WANTED_AS_IS which means the user wants to use this network anyway.
    @VisibleForTesting
    protected boolean mIsBandwidthCheckPassedOrIgnored = false;

    private final State mDefaultState = new DefaultState();
    private final State mValidatedState = new ValidatedState();
    private final State mMaybeNotifyState = new MaybeNotifyState();
    private final State mEvaluatingState = new EvaluatingState();
    private final State mCaptivePortalState = new CaptivePortalState();
    private final State mEvaluatingPrivateDnsState = new EvaluatingPrivateDnsState();
    private final State mProbingState = new ProbingState();
    private final State mWaitingForNextProbeState = new WaitingForNextProbeState();
    private final State mEvaluatingBandwidthState = new EvaluatingBandwidthState();

    private CustomIntentReceiver mLaunchCaptivePortalAppBroadcastReceiver = null;

    private final SharedLog mValidationLogs;

    private final Stopwatch mEvaluationTimer = new Stopwatch();

    // This variable is set before transitioning to the mCaptivePortalState.
    private CaptivePortalProbeResult mLastPortalProbeResult =
            CaptivePortalProbeResult.failed(CaptivePortalProbeResult.PROBE_UNKNOWN);

    // Random generator to select fallback URL index
    private final Random mRandom;
    private int mNextFallbackUrlIndex = 0;


    private int mReevaluateDelayMs = INITIAL_REEVALUATE_DELAY_MS;
    private int mEvaluateAttempts = 0;
    private volatile int mProbeToken = 0;
    private final int mConsecutiveDnsTimeoutThreshold;
    private final int mDataStallMinEvaluateTime;
    private final int mDataStallValidDnsTimeThreshold;
    private final int mDataStallEvaluationType;
    @Nullable
    private final DnsStallDetector mDnsStallDetector;
    private long mLastProbeTime;
    // The signal causing a data stall to be suspected. Reset to 0 after metrics are sent to statsd.
    private @EvaluationType int mDataStallTypeToCollect;
    private boolean mAcceptPartialConnectivity = false;
    private final EvaluationState mEvaluationState = new EvaluationState();

    private final boolean mPrivateIpNoInternetEnabled;

    private final boolean mMetricsEnabled;

    // The validation metrics are accessed by individual probe threads, and by the StateMachine
    // thread. All accesses must be synchronized to make sure the StateMachine thread can see
    // reports from all probes.
    // TODO: as that most usage is in the StateMachine thread and probes only add their probe
    // events, consider having probes return their stats to the StateMachine, and only access this
    // member on the StateMachine thread without synchronization.
    @GuardedBy("mNetworkValidationMetrics")
    private final NetworkValidationMetrics mNetworkValidationMetrics =
            new NetworkValidationMetrics();

    private int getCallbackVersion(INetworkMonitorCallbacks cb) {
        int version;
        try {
            version = cb.getInterfaceVersion();
        } catch (RemoteException e) {
            version = 0;
        }
        // The AIDL was freezed from Q beta 5 but it's unfreezing from R before releasing. In order
        // to distinguish the behavior between R and Q beta 5 and before Q beta 5, add SDK and
        // CODENAME check here. Basically, it's only expected to return 0 for Q beta 4 and below
        // because the test result has changed.
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.Q
                && Build.VERSION.CODENAME.equals("REL")
                && version == Build.VERSION_CODES.CUR_DEVELOPMENT) version = 0;
        return version;
    }

    public NetworkMonitor(Context context, INetworkMonitorCallbacks cb, Network network,
            SharedLog validationLog, @NonNull NetworkStackServiceManager serviceManager) {
        this(context, cb, network, new IpConnectivityLog(), validationLog, serviceManager,
                Dependencies.DEFAULT, getTcpSocketTrackerOrNull(context, network));
    }

    @VisibleForTesting
    public NetworkMonitor(Context context, INetworkMonitorCallbacks cb, Network network,
            IpConnectivityLog logger, SharedLog validationLogs,
            @NonNull NetworkStackServiceManager serviceManager, Dependencies deps,
            @Nullable TcpSocketTracker tst) {
        // Add suffix indicating which NetworkMonitor we're talking about.
        super(TAG + "/" + network.toString());

        // Logs with a tag of the form given just above, e.g.
        //     <timestamp>   862  2402 D NetworkMonitor/NetworkAgentInfo [WIFI () - 100]: ...
        setDbg(VDBG);

        mContext = context;
        mMetricsLog = logger;
        mValidationLogs = validationLogs;
        mCallback = cb;
        mCallbackVersion = getCallbackVersion(cb);
        mDependencies = deps;
        mNetwork = network;
        mCleartextDnsNetwork = deps.getPrivateDnsBypassNetwork(network);
        mTelephonyManager = (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        mWifiManager = (WifiManager) context.getSystemService(Context.WIFI_SERVICE);
        mCm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        mNotifier = serviceManager.getNotifier();

        // CHECKSTYLE:OFF IndentationCheck
        addState(mDefaultState);
        addState(mMaybeNotifyState, mDefaultState);
            addState(mEvaluatingState, mMaybeNotifyState);
                addState(mProbingState, mEvaluatingState);
                addState(mWaitingForNextProbeState, mEvaluatingState);
            addState(mCaptivePortalState, mMaybeNotifyState);
        addState(mEvaluatingPrivateDnsState, mDefaultState);
        addState(mEvaluatingBandwidthState, mDefaultState);
        addState(mValidatedState, mDefaultState);
        setInitialState(mDefaultState);
        // CHECKSTYLE:ON IndentationCheck

        mIsCaptivePortalCheckEnabled = getIsCaptivePortalCheckEnabled();
        mPrivateIpNoInternetEnabled = getIsPrivateIpNoInternetEnabled();
        mMetricsEnabled = deps.isFeatureEnabled(context, NAMESPACE_CONNECTIVITY,
                NetworkStackUtils.VALIDATION_METRICS_VERSION, true /* defaultEnabled */);
        mUseHttps = getUseHttpsValidation();
        mCaptivePortalUserAgent = getCaptivePortalUserAgent();
        mCaptivePortalHttpsUrls = makeCaptivePortalHttpsUrls();
        mCaptivePortalHttpUrls = makeCaptivePortalHttpUrls();
        mCaptivePortalFallbackUrls = makeCaptivePortalFallbackUrls();
        mCaptivePortalFallbackSpecs = makeCaptivePortalFallbackProbeSpecs();
        mRandom = deps.getRandom();
        // TODO: Evaluate to move data stall configuration to a specific class.
        mConsecutiveDnsTimeoutThreshold = getConsecutiveDnsTimeoutThreshold();
        mDataStallMinEvaluateTime = getDataStallMinEvaluateTime();
        mDataStallValidDnsTimeThreshold = getDataStallValidDnsTimeThreshold();
        mDataStallEvaluationType = getDataStallEvaluationType();
        mDnsStallDetector = initDnsStallDetectorIfRequired(mDataStallEvaluationType,
                mConsecutiveDnsTimeoutThreshold);
        mTcpTracker = tst;
        // Read the configurations of evaluating network bandwidth.
        mEvaluatingBandwidthUrl = getResStringConfig(mContext,
                R.string.config_evaluating_bandwidth_url, null);
        mMaxRetryTimerMs = getResIntConfig(mContext,
                R.integer.config_evaluating_bandwidth_max_retry_timer_ms,
                MAX_REEVALUATE_DELAY_MS);
        mEvaluatingBandwidthTimeoutMs = getResIntConfig(mContext,
                R.integer.config_evaluating_bandwidth_timeout_ms,
                DEFAULT_EVALUATING_BANDWIDTH_TIMEOUT_MS);

        // Provide empty LinkProperties and NetworkCapabilities to make sure they are never null,
        // even before notifyNetworkConnected.
        mLinkProperties = new LinkProperties();
        mNetworkCapabilities = new NetworkCapabilities(null);
    }

    /**
     * ConnectivityService notifies NetworkMonitor that the user already accepted partial
     * connectivity previously, so NetworkMonitor can validate the network even if it has partial
     * connectivity.
     */
    public void setAcceptPartialConnectivity() {
        sendMessage(EVENT_ACCEPT_PARTIAL_CONNECTIVITY);
    }

    /**
     * Request the NetworkMonitor to reevaluate the network.
     */
    public void forceReevaluation(int responsibleUid) {
        sendMessage(CMD_FORCE_REEVALUATION, responsibleUid, 0);
    }

    /**
     * Send a notification to NetworkMonitor indicating that there was a DNS query response event.
     * @param returnCode the DNS return code of the response.
     */
    public void notifyDnsResponse(int returnCode) {
        sendMessage(EVENT_DNS_NOTIFICATION, returnCode);
    }

    /**
     * Send a notification to NetworkMonitor indicating that private DNS settings have changed.
     * @param newCfg The new private DNS configuration.
     */
    public void notifyPrivateDnsSettingsChanged(PrivateDnsConfig newCfg) {
        // Cancel any outstanding resolutions.
        removeMessages(CMD_PRIVATE_DNS_SETTINGS_CHANGED);
        // Send the update to the proper thread.
        sendMessage(CMD_PRIVATE_DNS_SETTINGS_CHANGED, newCfg);
    }

    /**
     * Send a notification to NetworkMonitor indicating that the network is now connected.
     */
    public void notifyNetworkConnected(LinkProperties lp, NetworkCapabilities nc) {
        sendMessage(CMD_NETWORK_CONNECTED, new Pair<>(
                new LinkProperties(lp), new NetworkCapabilities(nc)));
    }

    private void updateConnectedNetworkAttributes(Message connectedMsg) {
        final Pair<LinkProperties, NetworkCapabilities> attrs =
                (Pair<LinkProperties, NetworkCapabilities>) connectedMsg.obj;
        mLinkProperties = attrs.first;
        mNetworkCapabilities = attrs.second;
    }

    /**
     * Send a notification to NetworkMonitor indicating that the network is now disconnected.
     */
    public void notifyNetworkDisconnected() {
        sendMessage(CMD_NETWORK_DISCONNECTED);
    }

    /**
     * Send a notification to NetworkMonitor indicating that link properties have changed.
     */
    public void notifyLinkPropertiesChanged(final LinkProperties lp) {
        sendMessage(EVENT_LINK_PROPERTIES_CHANGED, new LinkProperties(lp));
    }

    /**
     * Send a notification to NetworkMonitor indicating that network capabilities have changed.
     */
    public void notifyNetworkCapabilitiesChanged(final NetworkCapabilities nc) {
        sendMessage(EVENT_NETWORK_CAPABILITIES_CHANGED, new NetworkCapabilities(nc));
    }

    /**
     * Request the captive portal application to be launched.
     */
    public void launchCaptivePortalApp() {
        sendMessage(CMD_LAUNCH_CAPTIVE_PORTAL_APP);
    }

    /**
     * Notify that the captive portal app was closed with the provided response code.
     */
    public void notifyCaptivePortalAppFinished(int response) {
        sendMessage(CMD_CAPTIVE_PORTAL_APP_FINISHED, response);
    }

    @Override
    protected void log(String s) {
        if (DBG) Log.d(TAG + "/" + mCleartextDnsNetwork.toString(), s);
    }

    private void validationLog(int probeType, Object url, String msg) {
        String probeName = ValidationProbeEvent.getProbeName(probeType);
        validationLog(String.format("%s %s %s", probeName, url, msg));
    }

    private void validationLog(String s) {
        if (DBG) log(s);
        mValidationLogs.log(s);
    }

    private ValidationStage validationStage() {
        return 0 == mValidations ? ValidationStage.FIRST_VALIDATION : ValidationStage.REVALIDATION;
    }

    private boolean isValidationRequired() {
        return NetworkMonitorUtils.isValidationRequired(mNetworkCapabilities);
    }

    private boolean isPrivateDnsValidationRequired() {
        return NetworkMonitorUtils.isPrivateDnsValidationRequired(mNetworkCapabilities);
    }

    private void notifyNetworkTested(NetworkTestResultParcelable result) {
        try {
            if (mCallbackVersion <= 5) {
                mCallback.notifyNetworkTested(
                        getLegacyTestResult(result.result, result.probesSucceeded),
                        result.redirectUrl);
            } else {
                mCallback.notifyNetworkTestedWithExtras(result);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Error sending network test result", e);
        }
    }

    /**
     * Get the test result that was used as an int up to interface version 5.
     *
     * <p>For callback version < 3 (only used in Q beta preview builds), the int represented one of
     * the NETWORK_TEST_RESULT_* constants.
     *
     * <p>Q released with version 3, which used a single int for both the evaluation result bitmask,
     * and the probesSucceeded bitmask.
     */
    protected int getLegacyTestResult(int evaluationResult, int probesSucceeded) {
        if (mCallbackVersion < 3) {
            if ((evaluationResult & NETWORK_VALIDATION_RESULT_VALID) != 0) {
                return NETWORK_TEST_RESULT_VALID;
            }
            if ((evaluationResult & NETWORK_VALIDATION_RESULT_PARTIAL) != 0) {
                return NETWORK_TEST_RESULT_PARTIAL_CONNECTIVITY;
            }
            return NETWORK_TEST_RESULT_INVALID;
        }

        return evaluationResult | probesSucceeded;
    }

    private void notifyProbeStatusChanged(int probesCompleted, int probesSucceeded) {
        try {
            mCallback.notifyProbeStatusChanged(probesCompleted, probesSucceeded);
        } catch (RemoteException e) {
            Log.e(TAG, "Error sending probe status", e);
        }
    }

    private void showProvisioningNotification(String action) {
        try {
            mCallback.showProvisioningNotification(action, mContext.getPackageName());
        } catch (RemoteException e) {
            Log.e(TAG, "Error showing provisioning notification", e);
        }
    }

    private void hideProvisioningNotification() {
        try {
            mCallback.hideProvisioningNotification();
        } catch (RemoteException e) {
            Log.e(TAG, "Error hiding provisioning notification", e);
        }
    }

    private void notifyDataStallSuspected(@NonNull DataStallReportParcelable p) {
        try {
            mCallback.notifyDataStallSuspected(p);
        } catch (RemoteException e) {
            Log.e(TAG, "Error sending notification for suspected data stall", e);
        }
    }

    private void startMetricsCollection() {
        if (!mMetricsEnabled) return;
        try {
            synchronized (mNetworkValidationMetrics) {
                mNetworkValidationMetrics.startCollection(mNetworkCapabilities);
            }
        } catch (Exception e) {
            Log.wtf(TAG, "Error resetting validation metrics", e);
        }
    }

    private void recordProbeEventMetrics(ProbeType type, long latencyMicros, ProbeResult result,
            CaptivePortalDataShim capportData) {
        if (!mMetricsEnabled) return;
        try {
            synchronized (mNetworkValidationMetrics) {
                mNetworkValidationMetrics.addProbeEvent(type, latencyMicros, result, capportData);
            }
        } catch (Exception e) {
            Log.wtf(TAG, "Error recording probe event", e);
        }
    }

    private void recordValidationResult(int result, String redirectUrl) {
        if (!mMetricsEnabled) return;
        try {
            synchronized (mNetworkValidationMetrics) {
                mNetworkValidationMetrics.setValidationResult(result, redirectUrl);
            }
        } catch (Exception e) {
            Log.wtf(TAG, "Error recording validation result", e);
        }
    }

    private void maybeStopCollectionAndSendMetrics() {
        if (!mMetricsEnabled) return;
        try {
            synchronized (mNetworkValidationMetrics) {
                mNetworkValidationMetrics.maybeStopCollectionAndSend();
            }
        } catch (Exception e) {
            Log.wtf(TAG, "Error sending validation stats", e);
        }
    }

    // DefaultState is the parent of all States.  It exists only to handle CMD_* messages but
    // does not entail any real state (hence no enter() or exit() routines).
    private class DefaultState extends State {
        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CMD_NETWORK_CONNECTED:
                    updateConnectedNetworkAttributes(message);
                    logNetworkEvent(NetworkEvent.NETWORK_CONNECTED);
                    transitionTo(mEvaluatingState);
                    return HANDLED;
                case CMD_NETWORK_DISCONNECTED:
                    maybeStopCollectionAndSendMetrics();
                    logNetworkEvent(NetworkEvent.NETWORK_DISCONNECTED);
                    quit();
                    return HANDLED;
                case CMD_FORCE_REEVALUATION:
                case CMD_CAPTIVE_PORTAL_RECHECK:
                    if (getCurrentState() == mDefaultState) {
                        // Before receiving CMD_NETWORK_CONNECTED (when still in mDefaultState),
                        // requests to reevaluate are not valid: drop them.
                        return HANDLED;
                    }
                    String msg = "Forcing reevaluation for UID " + message.arg1;
                    final DnsStallDetector dsd = getDnsStallDetector();
                    if (dsd != null) {
                        msg += ". Dns signal count: " + dsd.getConsecutiveTimeoutCount();
                    }
                    validationLog(msg);
                    mUidResponsibleForReeval = message.arg1;
                    transitionTo(mEvaluatingState);
                    return HANDLED;
                case CMD_CAPTIVE_PORTAL_APP_FINISHED:
                    log("CaptivePortal App responded with " + message.arg1);

                    // If the user has seen and acted on a captive portal notification, and the
                    // captive portal app is now closed, disable HTTPS probes. This avoids the
                    // following pathological situation:
                    //
                    // 1. HTTP probe returns a captive portal, HTTPS probe fails or times out.
                    // 2. User opens the app and logs into the captive portal.
                    // 3. HTTP starts working, but HTTPS still doesn't work for some other reason -
                    //    perhaps due to the network blocking HTTPS?
                    //
                    // In this case, we'll fail to validate the network even after the app is
                    // dismissed. There is now no way to use this network, because the app is now
                    // gone, so the user cannot select "Use this network as is".
                    mUseHttps = false;

                    switch (message.arg1) {
                        case APP_RETURN_DISMISSED:
                            sendMessage(CMD_FORCE_REEVALUATION, NO_UID, 0);
                            break;
                        case APP_RETURN_WANTED_AS_IS:
                            mDontDisplaySigninNotification = true;
                            // If the user wants to use this network anyway, there is no need to
                            // perform the bandwidth check even if configured.
                            mIsBandwidthCheckPassedOrIgnored = true;
                            // TODO: Distinguish this from a network that actually validates.
                            // Displaying the "x" on the system UI icon may still be a good idea.
                            transitionTo(mEvaluatingPrivateDnsState);
                            break;
                        case APP_RETURN_UNWANTED:
                            mDontDisplaySigninNotification = true;
                            mUserDoesNotWant = true;
                            mEvaluationState.reportEvaluationResult(
                                    NETWORK_VALIDATION_RESULT_INVALID, null);
                            // TODO: Should teardown network.
                            mUidResponsibleForReeval = 0;
                            transitionTo(mEvaluatingState);
                            break;
                    }
                    return HANDLED;
                case CMD_PRIVATE_DNS_SETTINGS_CHANGED: {
                    final PrivateDnsConfig cfg = (PrivateDnsConfig) message.obj;
                    if (!isPrivateDnsValidationRequired() || cfg == null || !cfg.inStrictMode()) {
                        // No DNS resolution required.
                        //
                        // We don't force any validation in opportunistic mode
                        // here. Opportunistic mode nameservers are validated
                        // separately within netd.
                        //
                        // Reset Private DNS settings state.
                        mPrivateDnsProviderHostname = "";
                        break;
                    }

                    mPrivateDnsProviderHostname = cfg.hostname;

                    // DNS resolutions via Private DNS strict mode block for a
                    // few seconds (~4.2) checking for any IP addresses to
                    // arrive and validate. Initiating a (re)evaluation now
                    // should not significantly alter the validation outcome.
                    //
                    // No matter what: enqueue a validation request; one of
                    // three things can happen with this request:
                    //     [1] ignored (EvaluatingState or CaptivePortalState)
                    //     [2] transition to EvaluatingPrivateDnsState
                    //         (DefaultState and ValidatedState)
                    //     [3] handled (EvaluatingPrivateDnsState)
                    //
                    // The Private DNS configuration to be evaluated will:
                    //     [1] be skipped (not in strict mode), or
                    //     [2] validate (huzzah), or
                    //     [3] encounter some problem (invalid hostname,
                    //         no resolved IP addresses, IPs unreachable,
                    //         port 853 unreachable, port 853 is not running a
                    //         DNS-over-TLS server, et cetera).
                    // Cancel any outstanding CMD_EVALUATE_PRIVATE_DNS.
                    removeMessages(CMD_EVALUATE_PRIVATE_DNS);
                    sendMessage(CMD_EVALUATE_PRIVATE_DNS);
                    break;
                }
                case EVENT_DNS_NOTIFICATION:
                    final DnsStallDetector detector = getDnsStallDetector();
                    if (detector != null) {
                        detector.accumulateConsecutiveDnsTimeoutCount(message.arg1);
                    }
                    break;
                // Set mAcceptPartialConnectivity to true and if network start evaluating or
                // re-evaluating and get the result of partial connectivity, ProbingState will
                // disable HTTPS probe and transition to EvaluatingPrivateDnsState.
                case EVENT_ACCEPT_PARTIAL_CONNECTIVITY:
                    maybeDisableHttpsProbing(true /* acceptPartial */);
                    break;
                case EVENT_LINK_PROPERTIES_CHANGED:
                    final Uri oldCapportUrl = getCaptivePortalApiUrl(mLinkProperties);
                    mLinkProperties = (LinkProperties) message.obj;
                    final Uri newCapportUrl = getCaptivePortalApiUrl(mLinkProperties);
                    if (!Objects.equals(oldCapportUrl, newCapportUrl)) {
                        sendMessage(CMD_FORCE_REEVALUATION, NO_UID, 0);
                    }
                    break;
                case EVENT_NETWORK_CAPABILITIES_CHANGED:
                    mNetworkCapabilities = (NetworkCapabilities) message.obj;
                    break;
                default:
                    break;
            }
            return HANDLED;
        }
    }

    // Being in the ValidatedState State indicates a Network is:
    // - Successfully validated, or
    // - Wanted "as is" by the user, or
    // - Does not satisfy the default NetworkRequest and so validation has been skipped.
    private class ValidatedState extends State {
        @Override
        public void enter() {
            maybeLogEvaluationResult(
                    networkEventType(validationStage(), EvaluationResult.VALIDATED));
            // If the user has accepted partial connectivity and HTTPS probing is disabled, then
            // mark the network as validated and partial so that settings can keep informing the
            // user that the connection is limited.
            int result = NETWORK_VALIDATION_RESULT_VALID;
            if (!mUseHttps && mAcceptPartialConnectivity) {
                result |= NETWORK_VALIDATION_RESULT_PARTIAL;
            }
            mEvaluationState.reportEvaluationResult(result, null /* redirectUrl */);
            mValidations++;
            initSocketTrackingIfRequired();
            // start periodical polling.
            sendTcpPollingEvent();
            maybeStopCollectionAndSendMetrics();
        }

        private void initSocketTrackingIfRequired() {
            if (!isValidationRequired()) return;

            final TcpSocketTracker tst = getTcpSocketTracker();
            if (tst != null) {
                tst.pollSocketsInfo();
            }
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CMD_NETWORK_CONNECTED:
                    updateConnectedNetworkAttributes(message);
                    transitionTo(mValidatedState);
                    break;
                case CMD_EVALUATE_PRIVATE_DNS:
                    // TODO: this causes reevaluation of a single probe that is not counted in
                    // metrics. Add support for such reevaluation probes in metrics, and log them
                    // separately.
                    transitionTo(mEvaluatingPrivateDnsState);
                    break;
                case EVENT_DNS_NOTIFICATION:
                    final DnsStallDetector dsd = getDnsStallDetector();
                    if (dsd == null) break;

                    dsd.accumulateConsecutiveDnsTimeoutCount(message.arg1);
                    if (evaluateDataStall()) {
                        transitionTo(mEvaluatingState);
                    }
                    break;
                case EVENT_POLL_TCPINFO:
                    final TcpSocketTracker tst = getTcpSocketTracker();
                    if (tst == null) break;
                    // Transit if retrieve socket info is succeeded and suspected as a stall.
                    if (tst.pollSocketsInfo() && evaluateDataStall()) {
                        transitionTo(mEvaluatingState);
                    } else {
                        sendTcpPollingEvent();
                    }
                    break;
                default:
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        boolean evaluateDataStall() {
            if (isDataStall()) {
                validationLog("Suspecting data stall, reevaluate");
                return true;
            }
            return false;
        }

        @Override
        public void exit() {
            // Not useful for non-ValidatedState.
            removeMessages(EVENT_POLL_TCPINFO);
        }
    }

    @VisibleForTesting
    void sendTcpPollingEvent() {
        if (isValidationRequired()) {
            sendMessageDelayed(EVENT_POLL_TCPINFO, getTcpPollingInterval());
        }
    }

    private void maybeWriteDataStallStats(@NonNull final CaptivePortalProbeResult result) {
        if (mDataStallTypeToCollect == DATA_STALL_EVALUATION_TYPE_NONE) return;
        /*
         * Collect data stall detection level information for each transport type. Collect type
         * specific information for cellular and wifi only currently. Generate
         * DataStallDetectionStats for each transport type. E.g., if a network supports both
         * TRANSPORT_WIFI and TRANSPORT_VPN, two DataStallDetectionStats will be generated.
         */
        final int[] transports = mNetworkCapabilities.getTransportTypes();
        for (int i = 0; i < transports.length; i++) {
            final DataStallDetectionStats stats =
                    buildDataStallDetectionStats(transports[i], mDataStallTypeToCollect);
            mDependencies.writeDataStallDetectionStats(stats, result);
        }
        mDataStallTypeToCollect = DATA_STALL_EVALUATION_TYPE_NONE;
    }

    @VisibleForTesting
    protected DataStallDetectionStats buildDataStallDetectionStats(int transport,
            @EvaluationType int evaluationType) {
        final DataStallDetectionStats.Builder stats = new DataStallDetectionStats.Builder();
        if (VDBG_STALL) {
            log("collectDataStallMetrics: type=" + transport + ", evaluation=" + evaluationType);
        }
        stats.setEvaluationType(evaluationType);
        stats.setNetworkType(transport);
        switch (transport) {
            case NetworkCapabilities.TRANSPORT_WIFI:
                // TODO: Update it if status query in dual wifi is supported.
                final WifiInfo wifiInfo = mWifiManager.getConnectionInfo();
                stats.setWiFiData(wifiInfo);
                break;
            case NetworkCapabilities.TRANSPORT_CELLULAR:
                final boolean isRoaming = !mNetworkCapabilities.hasCapability(
                        NetworkCapabilities.NET_CAPABILITY_NOT_ROAMING);
                final SignalStrength ss = mTelephonyManager.getSignalStrength();
                // TODO(b/120452078): Support multi-sim.
                stats.setCellData(
                        mTelephonyManager.getDataNetworkType(),
                        isRoaming,
                        mTelephonyManager.getNetworkOperator(),
                        mTelephonyManager.getSimOperator(),
                        (ss != null)
                        ? ss.getLevel() : CellSignalStrength.SIGNAL_STRENGTH_NONE_OR_UNKNOWN);
                break;
            default:
                // No transport type specific information for the other types.
                break;
        }

        addDnsEvents(stats);
        addTcpStats(stats);

        return stats.build();
    }

    private void addTcpStats(@NonNull final DataStallDetectionStats.Builder stats) {
        final TcpSocketTracker tst = getTcpSocketTracker();
        if (tst == null) return;

        stats.setTcpSentSinceLastRecv(tst.getSentSinceLastRecv());
        stats.setTcpFailRate(tst.getLatestPacketFailPercentage());
    }

    @VisibleForTesting
    protected void addDnsEvents(@NonNull final DataStallDetectionStats.Builder stats) {
        final DnsStallDetector dsd = getDnsStallDetector();
        if (dsd == null) return;

        final int size = dsd.mResultIndices.size();
        for (int i = 1; i <= DEFAULT_DNS_LOG_SIZE && i <= size; i++) {
            final int index = dsd.mResultIndices.indexOf(size - i);
            stats.addDnsEvent(dsd.mDnsEvents[index].mReturnCode, dsd.mDnsEvents[index].mTimeStamp);
        }
    }


    // Being in the MaybeNotifyState State indicates the user may have been notified that sign-in
    // is required.  This State takes care to clear the notification upon exit from the State.
    private class MaybeNotifyState extends State {
        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CMD_LAUNCH_CAPTIVE_PORTAL_APP:
                    final Bundle appExtras = new Bundle();
                    // OneAddressPerFamilyNetwork is not parcelable across processes.
                    final Network network = new Network(mCleartextDnsNetwork);
                    appExtras.putParcelable(ConnectivityManager.EXTRA_NETWORK, network);
                    final CaptivePortalProbeResult probeRes = mLastPortalProbeResult;
                    // Use redirect URL from AP if exists.
                    final String portalUrl =
                            (useRedirectUrlForPortal() && makeURL(probeRes.redirectUrl) != null)
                            ? probeRes.redirectUrl : probeRes.detectUrl;
                    appExtras.putString(EXTRA_CAPTIVE_PORTAL_URL, portalUrl);
                    if (probeRes.probeSpec != null) {
                        final String encodedSpec = probeRes.probeSpec.getEncodedSpec();
                        appExtras.putString(EXTRA_CAPTIVE_PORTAL_PROBE_SPEC, encodedSpec);
                    }
                    appExtras.putString(ConnectivityManager.EXTRA_CAPTIVE_PORTAL_USER_AGENT,
                            mCaptivePortalUserAgent);
                    if (mNotifier != null) {
                        mNotifier.notifyCaptivePortalValidationPending(network);
                    }
                    mCm.startCaptivePortalApp(network, appExtras);
                    return HANDLED;
                default:
                    return NOT_HANDLED;
            }
        }

        private boolean useRedirectUrlForPortal() {
            // It must match the conditions in CaptivePortalLogin in which the redirect URL is not
            // used to validate that the portal is gone.
            final boolean aboveQ =
                    ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q);
            return aboveQ && mDependencies.isFeatureEnabled(mContext, NAMESPACE_CONNECTIVITY,
                    DISMISS_PORTAL_IN_VALIDATED_NETWORK, aboveQ /* defaultEnabled */);
        }

        @Override
        public void exit() {
            if (mLaunchCaptivePortalAppBroadcastReceiver != null) {
                mContext.unregisterReceiver(mLaunchCaptivePortalAppBroadcastReceiver);
                mLaunchCaptivePortalAppBroadcastReceiver = null;
            }
            hideProvisioningNotification();
        }
    }

    // Being in the EvaluatingState State indicates the Network is being evaluated for internet
    // connectivity, or that the user has indicated that this network is unwanted.
    private class EvaluatingState extends State {
        private Uri mEvaluatingCapportUrl;

        @Override
        public void enter() {
            // If we have already started to track time spent in EvaluatingState
            // don't reset the timer due simply to, say, commands or events that
            // cause us to exit and re-enter EvaluatingState.
            if (!mEvaluationTimer.isStarted()) {
                mEvaluationTimer.start();
            }
            sendMessage(CMD_REEVALUATE, ++mReevaluateToken, 0);
            if (mUidResponsibleForReeval != INVALID_UID) {
                TrafficStats.setThreadStatsUid(mUidResponsibleForReeval);
                mUidResponsibleForReeval = INVALID_UID;
            }
            mReevaluateDelayMs = INITIAL_REEVALUATE_DELAY_MS;
            mEvaluateAttempts = 0;
            mEvaluatingCapportUrl = getCaptivePortalApiUrl(mLinkProperties);
            // Reset all current probe results to zero, but retain current validation state until
            // validation succeeds or fails.
            mEvaluationState.clearProbeResults();
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CMD_REEVALUATE:
                    if (message.arg1 != mReevaluateToken || mUserDoesNotWant) {
                        return HANDLED;
                    }
                    // Don't bother validating networks that don't satisfy the default request.
                    // This includes:
                    //  - VPNs which can be considered explicitly desired by the user and the
                    //    user's desire trumps whether the network validates.
                    //  - Networks that don't provide Internet access.  It's unclear how to
                    //    validate such networks.
                    //  - Untrusted networks.  It's unsafe to prompt the user to sign-in to
                    //    such networks and the user didn't express interest in connecting to
                    //    such networks (an app did) so the user may be unhappily surprised when
                    //    asked to sign-in to a network they didn't want to connect to in the
                    //    first place.  Validation could be done to adjust the network scores
                    //    however these networks are app-requested and may not be intended for
                    //    general usage, in which case general validation may not be an accurate
                    //    measure of the network's quality.  Only the app knows how to evaluate
                    //    the network so don't bother validating here.  Furthermore sending HTTP
                    //    packets over the network may be undesirable, for example an extremely
                    //    expensive metered network, or unwanted leaking of the User Agent string.
                    //
                    // On networks that need to support private DNS in strict mode (e.g., VPNs, but
                    // not networks that don't provide Internet access), we still need to perform
                    // private DNS server resolution.
                    if (!isValidationRequired()) {
                        if (isPrivateDnsValidationRequired()) {
                            validationLog("Network would not satisfy default request, "
                                    + "resolving private DNS");
                            transitionTo(mEvaluatingPrivateDnsState);
                        } else {
                            validationLog("Network would not satisfy default request, "
                                    + "not validating");
                            transitionTo(mValidatedState);
                        }
                        return HANDLED;
                    }
                    mEvaluateAttempts++;

                    transitionTo(mProbingState);
                    return HANDLED;
                case CMD_FORCE_REEVALUATION:
                    // The evaluation process restarts via EvaluatingState#enter.
                    return shouldAcceptForceRevalidation() ? NOT_HANDLED : HANDLED;
                // Disable HTTPS probe and transition to EvaluatingPrivateDnsState because:
                // 1. Network is connected and finish the network validation.
                // 2. NetworkMonitor detects network is partial connectivity and user accepts it.
                case EVENT_ACCEPT_PARTIAL_CONNECTIVITY:
                    maybeDisableHttpsProbing(true /* acceptPartial */);
                    transitionTo(mEvaluatingPrivateDnsState);
                    return HANDLED;
                default:
                    return NOT_HANDLED;
            }
        }

        private boolean shouldAcceptForceRevalidation() {
            // If the captive portal URL has changed since the last evaluation attempt, always
            // revalidate. Otherwise, ignore any re-evaluation requests before
            // IGNORE_REEVALUATE_ATTEMPTS are made.
            return mEvaluateAttempts >= IGNORE_REEVALUATE_ATTEMPTS
                    || !Objects.equals(
                            mEvaluatingCapportUrl, getCaptivePortalApiUrl(mLinkProperties));
        }

        @Override
        public void exit() {
            TrafficStats.clearThreadStatsUid();
        }
    }

    // BroadcastReceiver that waits for a particular Intent and then posts a message.
    private class CustomIntentReceiver extends BroadcastReceiver {
        private final int mToken;
        private final int mWhat;
        private final String mAction;
        CustomIntentReceiver(String action, int token, int what) {
            mToken = token;
            mWhat = what;
            mAction = action + "_" + mCleartextDnsNetwork.getNetworkHandle() + "_" + token;
            mContext.registerReceiver(this, new IntentFilter(mAction));
        }
        public PendingIntent getPendingIntent() {
            final Intent intent = new Intent(mAction);
            intent.setPackage(mContext.getPackageName());
            return PendingIntent.getBroadcast(mContext, 0, intent, 0);
        }
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(mAction)) sendMessage(obtainMessage(mWhat, mToken));
        }
    }

    // Being in the CaptivePortalState State indicates a captive portal was detected and the user
    // has been shown a notification to sign-in.
    private class CaptivePortalState extends State {
        private static final String ACTION_LAUNCH_CAPTIVE_PORTAL_APP =
                "android.net.netmon.launchCaptivePortalApp";

        @Override
        public void enter() {
            maybeLogEvaluationResult(
                    networkEventType(validationStage(), EvaluationResult.CAPTIVE_PORTAL));
            // Don't annoy user with sign-in notifications.
            if (mDontDisplaySigninNotification) return;
            // Create a CustomIntentReceiver that sends us a
            // CMD_LAUNCH_CAPTIVE_PORTAL_APP message when the user
            // touches the notification.
            if (mLaunchCaptivePortalAppBroadcastReceiver == null) {
                // Wait for result.
                mLaunchCaptivePortalAppBroadcastReceiver = new CustomIntentReceiver(
                        ACTION_LAUNCH_CAPTIVE_PORTAL_APP, new Random().nextInt(),
                        CMD_LAUNCH_CAPTIVE_PORTAL_APP);
                // Display the sign in notification.
                // Only do this once for every time we enter MaybeNotifyState. b/122164725
                showProvisioningNotification(mLaunchCaptivePortalAppBroadcastReceiver.mAction);
            }
            // Retest for captive portal occasionally.
            sendMessageDelayed(CMD_CAPTIVE_PORTAL_RECHECK, 0 /* no UID */,
                    CAPTIVE_PORTAL_REEVALUATE_DELAY_MS);
            mValidations++;
            maybeStopCollectionAndSendMetrics();
        }

        @Override
        public void exit() {
            removeMessages(CMD_CAPTIVE_PORTAL_RECHECK);
        }
    }

    private class EvaluatingPrivateDnsState extends State {
        private int mPrivateDnsReevalDelayMs;
        private PrivateDnsConfig mPrivateDnsConfig;

        @Override
        public void enter() {
            mPrivateDnsReevalDelayMs = INITIAL_REEVALUATE_DELAY_MS;
            mPrivateDnsConfig = null;
            sendMessage(CMD_EVALUATE_PRIVATE_DNS);
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_EVALUATE_PRIVATE_DNS:
                    if (inStrictMode()) {
                        if (!isStrictModeHostnameResolved()) {
                            resolveStrictModeHostname();

                            if (isStrictModeHostnameResolved()) {
                                notifyPrivateDnsConfigResolved();
                            } else {
                                handlePrivateDnsEvaluationFailure();
                                // The private DNS probe fails-fast if the server hostname cannot
                                // be resolved. Record it as a failure with zero latency.
                                // TODO: refactor this together with the probe recorded in
                                // sendPrivateDnsProbe, so logging is symmetric / easier to follow.
                                recordProbeEventMetrics(ProbeType.PT_PRIVDNS, 0 /* latency */,
                                        ProbeResult.PR_FAILURE, null /* capportData */);
                                break;
                            }
                        }

                        // Look up a one-time hostname, to bypass caching.
                        //
                        // Note that this will race with ConnectivityService
                        // code programming the DNS-over-TLS server IP addresses
                        // into netd (if invoked, above). If netd doesn't know
                        // the IP addresses yet, or if the connections to the IP
                        // addresses haven't yet been validated, netd will block
                        // for up to a few seconds before failing the lookup.
                        if (!sendPrivateDnsProbe()) {
                            handlePrivateDnsEvaluationFailure();
                            break;
                        }
                        handlePrivateDnsEvaluationSuccess();
                    } else {
                        mEvaluationState.removeProbeResult(NETWORK_VALIDATION_PROBE_PRIVDNS);
                    }

                    if (needEvaluatingBandwidth()) {
                        transitionTo(mEvaluatingBandwidthState);
                    } else {
                        // All good!
                        transitionTo(mValidatedState);
                    }
                    break;
                case CMD_PRIVATE_DNS_SETTINGS_CHANGED:
                    // When settings change the reevaluation timer must be reset.
                    mPrivateDnsReevalDelayMs = INITIAL_REEVALUATE_DELAY_MS;
                    // Let the message bubble up and be handled by parent states as usual.
                    return NOT_HANDLED;
                default:
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        private boolean inStrictMode() {
            return !TextUtils.isEmpty(mPrivateDnsProviderHostname);
        }

        private boolean isStrictModeHostnameResolved() {
            return (mPrivateDnsConfig != null)
                    && mPrivateDnsConfig.hostname.equals(mPrivateDnsProviderHostname)
                    && (mPrivateDnsConfig.ips.length > 0);
        }

        private void resolveStrictModeHostname() {
            try {
                // Do a blocking DNS resolution using the network-assigned nameservers.
                final InetAddress[] ips = DnsUtils.getAllByName(mDependencies.getDnsResolver(),
                        mCleartextDnsNetwork, mPrivateDnsProviderHostname, getDnsProbeTimeout(),
                        str -> validationLog("Strict mode hostname resolution " + str));
                mPrivateDnsConfig = new PrivateDnsConfig(mPrivateDnsProviderHostname, ips);
            } catch (UnknownHostException uhe) {
                mPrivateDnsConfig = null;
            }
        }

        private void notifyPrivateDnsConfigResolved() {
            try {
                mCallback.notifyPrivateDnsConfigResolved(mPrivateDnsConfig.toParcel());
            } catch (RemoteException e) {
                Log.e(TAG, "Error sending private DNS config resolved notification", e);
            }
        }

        private void handlePrivateDnsEvaluationSuccess() {
            mEvaluationState.noteProbeResult(NETWORK_VALIDATION_PROBE_PRIVDNS,
                    true /* succeeded */);
        }

        private void handlePrivateDnsEvaluationFailure() {
            mEvaluationState.noteProbeResult(NETWORK_VALIDATION_PROBE_PRIVDNS,
                    false /* succeeded */);
            mEvaluationState.reportEvaluationResult(NETWORK_VALIDATION_RESULT_INVALID,
                    null /* redirectUrl */);
            // Queue up a re-evaluation with backoff.
            //
            // TODO: Consider abandoning this state after a few attempts and
            // transitioning back to EvaluatingState, to perhaps give ourselves
            // the opportunity to (re)detect a captive portal or something.
            //
            sendMessageDelayed(CMD_EVALUATE_PRIVATE_DNS, mPrivateDnsReevalDelayMs);
            mPrivateDnsReevalDelayMs *= 2;
            if (mPrivateDnsReevalDelayMs > MAX_REEVALUATE_DELAY_MS) {
                mPrivateDnsReevalDelayMs = MAX_REEVALUATE_DELAY_MS;
            }
        }

        private boolean sendPrivateDnsProbe() {
            final String host = UUID.randomUUID().toString().substring(0, 8)
                    + PRIVATE_DNS_PROBE_HOST_SUFFIX;
            final Stopwatch watch = new Stopwatch().start();
            boolean success = false;
            long time;
            try {
                final InetAddress[] ips = mNetwork.getAllByName(host);
                time = watch.stop();
                final String strIps = Arrays.toString(ips);
                success = (ips != null && ips.length > 0);
                validationLog(PROBE_PRIVDNS, host, String.format("%dus: %s", time, strIps));
            } catch (UnknownHostException uhe) {
                time = watch.stop();
                validationLog(PROBE_PRIVDNS, host,
                        String.format("%dus - Error: %s", time, uhe.getMessage()));
            }
            recordProbeEventMetrics(ProbeType.PT_PRIVDNS, time, success ? ProbeResult.PR_SUCCESS :
                    ProbeResult.PR_FAILURE, null /* capportData */);
            logValidationProbe(time, PROBE_PRIVDNS, success ? DNS_SUCCESS : DNS_FAILURE);
            return success;
        }
    }

    private class ProbingState extends State {
        private Thread mThread;

        @Override
        public void enter() {
            // When starting a full probe cycle here, record any pending stats (for example if
            // CMD_FORCE_REEVALUATE was called before evaluation finished, as can happen in
            // EvaluatingPrivateDnsState).
            maybeStopCollectionAndSendMetrics();
            // Restart the metrics collection timers. Metrics will be stopped and sent when the
            // validation attempt finishes (as success, failure or portal), or if it is interrupted
            // (by being restarted or if NetworkMonitor stops).
            startMetricsCollection();
            if (mEvaluateAttempts >= BLAME_FOR_EVALUATION_ATTEMPTS) {
                //Don't continue to blame UID forever.
                TrafficStats.clearThreadStatsUid();
            }

            final int token = ++mProbeToken;
            final EvaluationThreadDeps deps = new EvaluationThreadDeps(mNetworkCapabilities);
            mThread = new Thread(() -> sendMessage(obtainMessage(CMD_PROBE_COMPLETE, token, 0,
                    isCaptivePortal(deps))));
            mThread.start();
        }

        @Override
        public boolean processMessage(Message message) {
            switch (message.what) {
                case CMD_PROBE_COMPLETE:
                    // Ensure that CMD_PROBE_COMPLETE from stale threads are ignored.
                    if (message.arg1 != mProbeToken) {
                        return HANDLED;
                    }

                    final CaptivePortalProbeResult probeResult =
                            (CaptivePortalProbeResult) message.obj;
                    mLastProbeTime = SystemClock.elapsedRealtime();

                    maybeWriteDataStallStats(probeResult);

                    if (probeResult.isSuccessful()) {
                        // Transit EvaluatingPrivateDnsState to get to Validated
                        // state (even if no Private DNS validation required).
                        transitionTo(mEvaluatingPrivateDnsState);
                    } else if (probeResult.isPortal()) {
                        mEvaluationState.reportEvaluationResult(NETWORK_VALIDATION_RESULT_INVALID,
                                probeResult.redirectUrl);
                        mLastPortalProbeResult = probeResult;
                        transitionTo(mCaptivePortalState);
                    } else if (probeResult.isPartialConnectivity()) {
                        mEvaluationState.reportEvaluationResult(NETWORK_VALIDATION_RESULT_PARTIAL,
                                null /* redirectUrl */);
                        maybeDisableHttpsProbing(mAcceptPartialConnectivity);
                        if (mAcceptPartialConnectivity) {
                            transitionTo(mEvaluatingPrivateDnsState);
                        } else {
                            transitionTo(mWaitingForNextProbeState);
                        }
                    } else {
                        logNetworkEvent(NetworkEvent.NETWORK_VALIDATION_FAILED);
                        mEvaluationState.reportEvaluationResult(NETWORK_VALIDATION_RESULT_INVALID,
                                null /* redirectUrl */);
                        transitionTo(mWaitingForNextProbeState);
                    }
                    return HANDLED;
                case EVENT_DNS_NOTIFICATION:
                case EVENT_ACCEPT_PARTIAL_CONNECTIVITY:
                    // Leave the event to DefaultState.
                    return NOT_HANDLED;
                default:
                    // Wait for probe result and defer events to next state by default.
                    deferMessage(message);
                    return HANDLED;
            }
        }

        @Override
        public void exit() {
            if (mThread.isAlive()) {
                mThread.interrupt();
            }
            mThread = null;
        }
    }

    // Being in the WaitingForNextProbeState indicates that evaluating probes failed and state is
    // transited from ProbingState. This ensures that the state machine is only in ProbingState
    // while a probe is in progress, not while waiting to perform the next probe. That allows
    // ProbingState to defer most messages until the probe is complete, which keeps the code simple
    // and matches the pre-Q behaviour where probes were a blocking operation performed on the state
    // machine thread.
    private class WaitingForNextProbeState extends State {
        @Override
        public void enter() {
            // Send metrics for this evaluation attempt. Metrics collection (and its timers) will be
            // restarted when the next probe starts.
            maybeStopCollectionAndSendMetrics();
            scheduleNextProbe();
        }

        private void scheduleNextProbe() {
            final Message msg = obtainMessage(CMD_REEVALUATE, ++mReevaluateToken, 0);
            sendMessageDelayed(msg, mReevaluateDelayMs);
            mReevaluateDelayMs *= 2;
            if (mReevaluateDelayMs > MAX_REEVALUATE_DELAY_MS) {
                mReevaluateDelayMs = MAX_REEVALUATE_DELAY_MS;
            }
        }

        @Override
        public boolean processMessage(Message message) {
            return NOT_HANDLED;
        }
    }

    private final class EvaluatingBandwidthThread extends Thread {
        final int mThreadId;

        EvaluatingBandwidthThread(int id) {
            mThreadId = id;
        }

        @Override
        public void run() {
            HttpURLConnection urlConnection = null;
            try {
                final URL url = makeURL(mEvaluatingBandwidthUrl);
                urlConnection = makeProbeConnection(url, true /* followRedirects */);
                // In order to exclude the time of DNS lookup, send the delay message of timeout
                // here.
                sendMessageDelayed(CMD_BANDWIDTH_CHECK_TIMEOUT, mEvaluatingBandwidthTimeoutMs);
                readContentFromDownloadUrl(urlConnection);
            } catch (InterruptedIOException e) {
                // There is a timing issue that someone triggers the forcing reevaluation when
                // executing the getInputStream(). The InterruptedIOException is thrown by
                // Timeout#throwIfReached, it will reset the interrupt flag of Thread. So just
                // return and wait for the bandwidth reevaluation, otherwise the
                // CMD_BANDWIDTH_CHECK_COMPLETE will be sent.
                validationLog("The thread is interrupted when executing the getInputStream(),"
                        + " return and wait for the bandwidth reevaluation");
                return;
            } catch (IOException e) {
                validationLog("Evaluating bandwidth failed: " + e + ", if the thread is not"
                        + " interrupted, transition to validated state directly to make sure user"
                        + " can use wifi normally.");
            } finally {
                if (urlConnection != null) {
                    urlConnection.disconnect();
                }
            }
            // Don't send CMD_BANDWIDTH_CHECK_COMPLETE if the IO is interrupted or timeout.
            // Only send CMD_BANDWIDTH_CHECK_COMPLETE when the download is finished normally.
            // Add a serial number for CMD_BANDWIDTH_CHECK_COMPLETE to prevent handling the obsolete
            // CMD_BANDWIDTH_CHECK_COMPLETE.
            if (!isInterrupted()) sendMessage(CMD_BANDWIDTH_CHECK_COMPLETE, mThreadId);
        }

        private void readContentFromDownloadUrl(@NonNull final HttpURLConnection conn)
                throws IOException {
            final byte[] buffer = new byte[1000];
            final InputStream is = conn.getInputStream();
            while (!isInterrupted() && is.read(buffer) > 0) { /* read again */ }
        }
    }

    private class EvaluatingBandwidthState extends State {
        private EvaluatingBandwidthThread mEvaluatingBandwidthThread;
        private int mRetryBandwidthDelayMs;
        private int mCurrentThreadId;

        @Override
        public void enter() {
            mRetryBandwidthDelayMs = getResIntConfig(mContext,
                    R.integer.config_evaluating_bandwidth_min_retry_timer_ms,
                    INITIAL_REEVALUATE_DELAY_MS);
            sendMessage(CMD_EVALUATE_BANDWIDTH);
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_EVALUATE_BANDWIDTH:
                    mCurrentThreadId = mNextEvaluatingBandwidthThreadId.getAndIncrement();
                    mEvaluatingBandwidthThread = new EvaluatingBandwidthThread(mCurrentThreadId);
                    mEvaluatingBandwidthThread.start();
                    break;
                case CMD_BANDWIDTH_CHECK_COMPLETE:
                    // Only handle the CMD_BANDWIDTH_CHECK_COMPLETE which is sent by the newest
                    // EvaluatingBandwidthThread.
                    if (mCurrentThreadId == msg.arg1) {
                        mIsBandwidthCheckPassedOrIgnored = true;
                        transitionTo(mValidatedState);
                    }
                    break;
                case CMD_BANDWIDTH_CHECK_TIMEOUT:
                    validationLog("Evaluating bandwidth timeout!");
                    mEvaluatingBandwidthThread.interrupt();
                    scheduleReevaluatingBandwidth();
                    break;
                default:
                    return NOT_HANDLED;
            }
            return HANDLED;
        }

        private void scheduleReevaluatingBandwidth() {
            sendMessageDelayed(obtainMessage(CMD_EVALUATE_BANDWIDTH), mRetryBandwidthDelayMs);
            mRetryBandwidthDelayMs *= 2;
            if (mRetryBandwidthDelayMs > mMaxRetryTimerMs) {
                mRetryBandwidthDelayMs = mMaxRetryTimerMs;
            }
        }

        @Override
        public void exit() {
            mEvaluatingBandwidthThread.interrupt();
            removeMessages(CMD_EVALUATE_BANDWIDTH);
            removeMessages(CMD_BANDWIDTH_CHECK_TIMEOUT);
        }
    }

    // Limits the list of IP addresses returned by getAllByName or tried by openConnection to at
    // most one per address family. This ensures we only wait up to 20 seconds for TCP connections
    // to complete, regardless of how many IP addresses a host has.
    private static class OneAddressPerFamilyNetwork extends Network {
        OneAddressPerFamilyNetwork(Network network) {
            // Always bypass Private DNS.
            super(network.getPrivateDnsBypassingCopy());
        }

        @Override
        public InetAddress[] getAllByName(String host) throws UnknownHostException {
            final List<InetAddress> addrs = Arrays.asList(super.getAllByName(host));

            // Ensure the address family of the first address is tried first.
            LinkedHashMap<Class, InetAddress> addressByFamily = new LinkedHashMap<>();
            addressByFamily.put(addrs.get(0).getClass(), addrs.get(0));
            Collections.shuffle(addrs);

            for (InetAddress addr : addrs) {
                addressByFamily.put(addr.getClass(), addr);
            }

            return addressByFamily.values().toArray(new InetAddress[addressByFamily.size()]);
        }
    }

    @VisibleForTesting
    boolean onlyWifiTransport() {
        int[] transportTypes = mNetworkCapabilities.getTransportTypes();
        return transportTypes.length == 1
                && transportTypes[0] == NetworkCapabilities.TRANSPORT_WIFI;
    }

    @VisibleForTesting
    boolean needEvaluatingBandwidth() {
        if (mIsBandwidthCheckPassedOrIgnored
                || TextUtils.isEmpty(mEvaluatingBandwidthUrl)
                || !mNetworkCapabilities.hasCapability(NET_CAPABILITY_NOT_METERED)
                || !onlyWifiTransport()) {
            return false;
        }

        return true;
    }

    private boolean getIsCaptivePortalCheckEnabled() {
        String symbol = CAPTIVE_PORTAL_MODE;
        int defaultValue = CAPTIVE_PORTAL_MODE_IGNORE;
        int mode = mDependencies.getSetting(mContext, symbol, defaultValue);
        return mode != CAPTIVE_PORTAL_MODE_IGNORE;
    }

    private boolean getIsPrivateIpNoInternetEnabled() {
        return mDependencies.isFeatureEnabled(mContext, DNS_PROBE_PRIVATE_IP_NO_INTERNET_VERSION)
                || mContext.getResources().getBoolean(
                        R.bool.config_force_dns_probe_private_ip_no_internet);
    }

    private boolean getUseHttpsValidation() {
        return mDependencies.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY,
                CAPTIVE_PORTAL_USE_HTTPS, 1) == 1;
    }

    @Nullable
    private String getMccFromCellInfo(final CellInfo cell) {
        if (cell instanceof CellInfoGsm) {
            return ((CellInfoGsm) cell).getCellIdentity().getMccString();
        } else if (cell instanceof CellInfoLte) {
            return ((CellInfoLte) cell).getCellIdentity().getMccString();
        } else if (cell instanceof CellInfoWcdma) {
            return ((CellInfoWcdma) cell).getCellIdentity().getMccString();
        } else if (cell instanceof CellInfoTdscdma) {
            return ((CellInfoTdscdma) cell).getCellIdentity().getMccString();
        } else if (cell instanceof CellInfoNr) {
            return ((CellIdentityNr) ((CellInfoNr) cell).getCellIdentity()).getMccString();
        } else {
            return null;
        }
    }

    /**
     * Return location mcc.
     */
    @VisibleForTesting
    @Nullable
    protected String getLocationMcc() {
        // Adding this check is because the new permission won't be granted by mainline update,
        // the new permission only be granted by OTA for current design. Tracking: b/145774617.
        if (mContext.checkPermission(android.Manifest.permission.ACCESS_FINE_LOCATION,
                Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_DENIED) {
            log("getLocationMcc : NetworkStack does not hold ACCESS_FINE_LOCATION");
            return null;
        }
        try {
            final List<CellInfo> cells = mTelephonyManager.getAllCellInfo();
            if (cells == null) return null;
            final Map<String, Integer> countryCodeMap = new HashMap<>();
            int maxCount = 0;
            for (final CellInfo cell : cells) {
                final String mcc = getMccFromCellInfo(cell);
                if (mcc != null) {
                    final int count = countryCodeMap.getOrDefault(mcc, 0) + 1;
                    countryCodeMap.put(mcc, count);
                }
            }
            // Return the MCC which occurs most.
            if (countryCodeMap.size() <= 0) return null;
            return Collections.max(countryCodeMap.entrySet(),
                    (e1, e2) -> e1.getValue().compareTo(e2.getValue())).getKey();
        } catch (SecurityException e) {
            log("Permission is not granted:" + e);
            return null;
        }
    }

    /**
     * Return a matched MccMncOverrideInfo if carrier id and sim mccmnc are matching a record in
     * sCarrierIdToMccMnc.
     */
    @VisibleForTesting
    @Nullable
    MccMncOverrideInfo getMccMncOverrideInfo() {
        final int carrierId = mTelephonyManager.getSimCarrierId();
        return sCarrierIdToMccMnc.get(carrierId);
    }

    private Context getContextByMccMnc(final int mcc, final int mnc) {
        final Configuration config = mContext.getResources().getConfiguration();
        if (mcc != UNSET_MCC_OR_MNC) config.mcc = mcc;
        if (mnc != UNSET_MCC_OR_MNC) config.mnc = mnc;
        return mContext.createConfigurationContext(config);
    }

    @VisibleForTesting
    protected Context getCustomizedContextOrDefault() {
        // Return customized context if carrier id can match a record in sCarrierIdToMccMnc.
        final MccMncOverrideInfo overrideInfo = getMccMncOverrideInfo();
        if (overrideInfo != null) {
            return getContextByMccMnc(overrideInfo.mcc, overrideInfo.mnc);
        }

        // Use neighbor mcc feature only works when the config_no_sim_card_uses_neighbor_mcc is
        // true and there is no sim card inserted.
        final boolean useNeighborResource =
                getResBooleanConfig(mContext, R.bool.config_no_sim_card_uses_neighbor_mcc, false);
        if (!useNeighborResource
                || TelephonyManager.SIM_STATE_READY == mTelephonyManager.getSimState()) {
            return mContext;
        }

        final String mcc = getLocationMcc();
        if (TextUtils.isEmpty(mcc)) {
            return mContext;
        }

        return getContextByMccMnc(Integer.parseInt(mcc), UNSET_MCC_OR_MNC);
    }

    @Nullable
    private String getTestUrl(@NonNull String key) {
        final String strExpiration = mDependencies.getDeviceConfigProperty(NAMESPACE_CONNECTIVITY,
                TEST_URL_EXPIRATION_TIME, null);
        if (strExpiration == null) return null;

        final long expTime;
        try {
            expTime = Long.parseUnsignedLong(strExpiration);
        } catch (NumberFormatException e) {
            loge("Invalid test URL expiration time format", e);
            return null;
        }

        final long now = System.currentTimeMillis();
        if (expTime < now || (expTime - now) > TEST_URL_EXPIRATION_MS) return null;

        return mDependencies.getDeviceConfigProperty(NAMESPACE_CONNECTIVITY,
                key, null /* defaultValue */);
    }

    private String getCaptivePortalServerHttpsUrl() {
        final String testUrl = getTestUrl(TEST_CAPTIVE_PORTAL_HTTPS_URL);
        if (isValidTestUrl(testUrl)) return testUrl;
        final Context targetContext = getCustomizedContextOrDefault();
        return getSettingFromResource(targetContext,
                R.string.config_captive_portal_https_url, CAPTIVE_PORTAL_HTTPS_URL,
                targetContext.getResources().getString(R.string.default_captive_portal_https_url));
    }

    private static boolean isValidTestUrl(@Nullable String url) {
        if (TextUtils.isEmpty(url)) return false;

        try {
            // Only accept test URLs on localhost
            return Uri.parse(url).getHost().equals("localhost");
        } catch (Throwable e) {
            Log.wtf(TAG, "Error parsing test URL", e);
            return false;
        }
    }

    private int getDnsProbeTimeout() {
        return getIntSetting(mContext, R.integer.config_captive_portal_dns_probe_timeout,
                CONFIG_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT, DEFAULT_CAPTIVE_PORTAL_DNS_PROBE_TIMEOUT);
    }

    /**
     * Gets an integer setting from resources or device config
     *
     * configResource is used if set, followed by device config if set, followed by defaultValue.
     * If none of these are set then an exception is thrown.
     *
     * TODO: move to a common location such as a ConfigUtils class.
     * TODO(b/130324939): test that the resources can be overlayed by an RRO package.
     */
    @VisibleForTesting
    int getIntSetting(@NonNull final Context context, @StringRes int configResource,
            @NonNull String symbol, int defaultValue) {
        final Resources res = context.getResources();
        try {
            return res.getInteger(configResource);
        } catch (Resources.NotFoundException e) {
            return mDependencies.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY,
                    symbol, defaultValue);
        }
    }

    @VisibleForTesting
    boolean getResBooleanConfig(@NonNull final Context context,
            @BoolRes int configResource, final boolean defaultValue) {
        final Resources res = context.getResources();
        try {
            return res.getBoolean(configResource);
        } catch (Resources.NotFoundException e) {
            return defaultValue;
        }
    }

    /**
     * Gets integer config from resources.
     */
    @VisibleForTesting
    int getResIntConfig(@NonNull final Context context,
            @IntegerRes final int configResource, final int defaultValue) {
        final Resources res = context.getResources();
        try {
            return res.getInteger(configResource);
        } catch (Resources.NotFoundException e) {
            return defaultValue;
        }
    }

    /**
     * Gets string config from resources.
     */
    @VisibleForTesting
    String getResStringConfig(@NonNull final Context context,
            @StringRes final int configResource, @Nullable final String defaultValue) {
        final Resources res = context.getResources();
        try {
            return res.getString(configResource);
        } catch (Resources.NotFoundException e) {
            return defaultValue;
        }
    }

    /**
     * Get the captive portal server HTTP URL that is configured on the device.
     *
     * NetworkMonitor does not use {@link ConnectivityManager#getCaptivePortalServerUrl()} as
     * it has its own updatable strategies to detect captive portals. The framework only advises
     * on one URL that can be used, while NetworkMonitor may implement more complex logic.
     */
    public String getCaptivePortalServerHttpUrl() {
        final String testUrl = getTestUrl(TEST_CAPTIVE_PORTAL_HTTP_URL);
        if (isValidTestUrl(testUrl)) return testUrl;
        final Context targetContext = getCustomizedContextOrDefault();
        return getSettingFromResource(targetContext,
                R.string.config_captive_portal_http_url, CAPTIVE_PORTAL_HTTP_URL,
                targetContext.getResources().getString(R.string.default_captive_portal_http_url));
    }

    private int getConsecutiveDnsTimeoutThreshold() {
        return mDependencies.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY,
                CONFIG_DATA_STALL_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD,
                DEFAULT_CONSECUTIVE_DNS_TIMEOUT_THRESHOLD);
    }

    private int getDataStallMinEvaluateTime() {
        return mDependencies.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY,
                CONFIG_DATA_STALL_MIN_EVALUATE_INTERVAL,
                DEFAULT_DATA_STALL_MIN_EVALUATE_TIME_MS);
    }

    private int getDataStallValidDnsTimeThreshold() {
        return mDependencies.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY,
                CONFIG_DATA_STALL_VALID_DNS_TIME_THRESHOLD,
                DEFAULT_DATA_STALL_VALID_DNS_TIME_THRESHOLD_MS);
    }

    @VisibleForTesting
    int getDataStallEvaluationType() {
        return mDependencies.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY,
                CONFIG_DATA_STALL_EVALUATION_TYPE,
                DEFAULT_DATA_STALL_EVALUATION_TYPES);
    }

    private int getTcpPollingInterval() {
        return mDependencies.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY,
                CONFIG_DATA_STALL_TCP_POLLING_INTERVAL,
                DEFAULT_TCP_POLLING_INTERVAL_MS);
    }

    @VisibleForTesting
    URL[] makeCaptivePortalFallbackUrls() {
        try {
            final String firstUrl = mDependencies.getSetting(mContext, CAPTIVE_PORTAL_FALLBACK_URL,
                    null);
            final URL[] settingProviderUrls =
                combineCaptivePortalUrls(firstUrl, CAPTIVE_PORTAL_OTHER_FALLBACK_URLS);
            return getProbeUrlArrayConfig(settingProviderUrls,
                    R.array.config_captive_portal_fallback_urls,
                    R.array.default_captive_portal_fallback_urls,
                    this::makeURL);
        } catch (Exception e) {
            // Don't let a misconfiguration bootloop the system.
            Log.e(TAG, "Error parsing configured fallback URLs", e);
            return new URL[0];
        }
    }

    private CaptivePortalProbeSpec[] makeCaptivePortalFallbackProbeSpecs() {
        try {
            final String settingsValue = mDependencies.getDeviceConfigProperty(
                    NAMESPACE_CONNECTIVITY, CAPTIVE_PORTAL_FALLBACK_PROBE_SPECS, null);

            final CaptivePortalProbeSpec[] emptySpecs = new CaptivePortalProbeSpec[0];
            final CaptivePortalProbeSpec[] providerValue = TextUtils.isEmpty(settingsValue)
                    ? emptySpecs
                    : parseCaptivePortalProbeSpecs(settingsValue).toArray(emptySpecs);

            return getProbeUrlArrayConfig(providerValue,
                    R.array.config_captive_portal_fallback_probe_specs,
                    DEFAULT_CAPTIVE_PORTAL_FALLBACK_PROBE_SPECS,
                    CaptivePortalProbeSpec::parseSpecOrNull);
        } catch (Exception e) {
            // Don't let a misconfiguration bootloop the system.
            Log.e(TAG, "Error parsing configured fallback probe specs", e);
            return null;
        }
    }

    private URL[] makeCaptivePortalHttpsUrls() {
        final String firstUrl = getCaptivePortalServerHttpsUrl();
        try {
            final URL[] settingProviderUrls =
                combineCaptivePortalUrls(firstUrl, CAPTIVE_PORTAL_OTHER_HTTPS_URLS);
            // firstUrl will at least be default configuration, so default value in
            // getProbeUrlArrayConfig is actually never used.
            return getProbeUrlArrayConfig(settingProviderUrls,
                    R.array.config_captive_portal_https_urls,
                    DEFAULT_CAPTIVE_PORTAL_HTTPS_URLS, this::makeURL);
        } catch (Exception e) {
            // Don't let a misconfiguration bootloop the system.
            Log.e(TAG, "Error parsing configured https URLs", e);
            // Ensure URL aligned with legacy configuration.
            return new URL[]{makeURL(firstUrl)};
        }
    }

    private URL[] makeCaptivePortalHttpUrls() {
        final String firstUrl = getCaptivePortalServerHttpUrl();
        try {
            final URL[] settingProviderUrls =
                    combineCaptivePortalUrls(firstUrl, CAPTIVE_PORTAL_OTHER_HTTP_URLS);
            // firstUrl will at least be default configuration, so default value in
            // getProbeUrlArrayConfig is actually never used.
            return getProbeUrlArrayConfig(settingProviderUrls,
                    R.array.config_captive_portal_http_urls,
                    DEFAULT_CAPTIVE_PORTAL_HTTP_URLS, this::makeURL);
        } catch (Exception e) {
            // Don't let a misconfiguration bootloop the system.
            Log.e(TAG, "Error parsing configured http URLs", e);
            // Ensure URL aligned with legacy configuration.
            return new URL[]{makeURL(firstUrl)};
        }
    }

    private URL[] combineCaptivePortalUrls(final String firstUrl, final String propertyName) {
        if (TextUtils.isEmpty(firstUrl)) return new URL[0];

        final String otherUrls = mDependencies.getDeviceConfigProperty(
                NAMESPACE_CONNECTIVITY, propertyName, "");
        // otherUrls may be empty, but .split() ignores trailing empty strings
        final String separator = ",";
        final String[] urls = (firstUrl + separator + otherUrls).split(separator);
        return convertStrings(urls, this::makeURL, new URL[0]);
    }

    /**
     * Read a setting from a resource or the settings provider.
     *
     * <p>The configuration resource is prioritized, then the provider value.
     * @param context The context
     * @param configResource The resource id for the configuration parameter
     * @param symbol The symbol in the settings provider
     * @param defaultValue The default value
     * @return The best available value
     */
    @Nullable
    private String getSettingFromResource(@NonNull final Context context,
            @StringRes int configResource, @NonNull String symbol, @NonNull String defaultValue) {
        final Resources res = context.getResources();
        String setting = res.getString(configResource);

        if (!TextUtils.isEmpty(setting)) return setting;

        setting = mDependencies.getSetting(context, symbol, null);

        if (!TextUtils.isEmpty(setting)) return setting;

        return defaultValue;
    }

    /**
     * Get an array configuration from resources or the settings provider.
     *
     * <p>The configuration resource is prioritized, then the provider values, then the default
     * resource values.
     * @param providerValue Values obtained from the setting provider.
     * @param configResId ID of the configuration resource.
     * @param defaultResId ID of the default resource.
     * @param resourceConverter Converter from the resource strings to stored setting class. Null
     *                          return values are ignored.
     */
    private <T> T[] getProbeUrlArrayConfig(@NonNull T[] providerValue, @ArrayRes int configResId,
            @ArrayRes int defaultResId, @NonNull Function<String, T> resourceConverter) {
        final Resources res = getCustomizedContextOrDefault().getResources();
        return getProbeUrlArrayConfig(providerValue, configResId, res.getStringArray(defaultResId),
                resourceConverter);
    }

    /**
     * Get an array configuration from resources or the settings provider.
     *
     * <p>The configuration resource is prioritized, then the provider values, then the default
     * resource values.
     * @param providerValue Values obtained from the setting provider.
     * @param configResId ID of the configuration resource.
     * @param defaultConfig Values of default configuration.
     * @param resourceConverter Converter from the resource strings to stored setting class. Null
     *                          return values are ignored.
     */
    private <T> T[] getProbeUrlArrayConfig(@NonNull T[] providerValue, @ArrayRes int configResId,
            String[] defaultConfig, @NonNull Function<String, T> resourceConverter) {
        final Resources res = getCustomizedContextOrDefault().getResources();
        String[] configValue = res.getStringArray(configResId);

        if (configValue.length == 0) {
            if (providerValue.length > 0) {
                return providerValue;
            }

            configValue = defaultConfig;
        }

        return convertStrings(configValue, resourceConverter, Arrays.copyOf(providerValue, 0));
    }

    /**
     * Convert a String array to an array of some other type using the specified converter.
     *
     * <p>Any null value, or value for which the converter throws a {@link RuntimeException}, will
     * not be added to the output array, so the output array may be smaller than the input.
     */
    private <T> T[] convertStrings(
            @NonNull String[] strings, Function<String, T> converter, T[] emptyArray) {
        final ArrayList<T> convertedValues = new ArrayList<>(strings.length);
        for (String configString : strings) {
            T convertedValue = null;
            try {
                convertedValue = converter.apply(configString);
            } catch (Exception e) {
                Log.e(TAG, "Error parsing configuration", e);
                // Fall through
            }
            if (convertedValue != null) {
                convertedValues.add(convertedValue);
            }
        }
        return convertedValues.toArray(emptyArray);
    }

    private String getCaptivePortalUserAgent() {
        return mDependencies.getDeviceConfigProperty(NAMESPACE_CONNECTIVITY,
                CAPTIVE_PORTAL_USER_AGENT, DEFAULT_USER_AGENT);
    }

    private URL nextFallbackUrl() {
        if (mCaptivePortalFallbackUrls.length == 0) {
            return null;
        }
        int idx = Math.abs(mNextFallbackUrlIndex) % mCaptivePortalFallbackUrls.length;
        mNextFallbackUrlIndex += mRandom.nextInt(); // randomly change url without memory.
        return mCaptivePortalFallbackUrls[idx];
    }

    private CaptivePortalProbeSpec nextFallbackSpec() {
        if (isEmpty(mCaptivePortalFallbackSpecs)) {
            return null;
        }
        // Randomly change spec without memory. Also randomize the first attempt.
        final int idx = Math.abs(mRandom.nextInt()) % mCaptivePortalFallbackSpecs.length;
        return mCaptivePortalFallbackSpecs[idx];
    }

    /**
     * Parameters that can be accessed by the evaluation thread in a thread-safe way.
     *
     * Parameters such as LinkProperties and NetworkCapabilities cannot be accessed by the
     * evaluation thread directly, as they are managed in the state machine thread and not
     * synchronized. This class provides a copy of the required data that is not modified and can be
     * used safely by the evaluation thread.
     */
    private static class EvaluationThreadDeps {
        // TODO: add parameters that are accessed in a non-thread-safe way from the evaluation
        // thread (read from LinkProperties, NetworkCapabilities, useHttps, validationStage)
        private final boolean mIsTestNetwork;

        EvaluationThreadDeps(NetworkCapabilities nc) {
            this.mIsTestNetwork = nc.hasTransport(TRANSPORT_TEST);
        }
    }

    private CaptivePortalProbeResult isCaptivePortal(EvaluationThreadDeps deps) {
        if (!mIsCaptivePortalCheckEnabled) {
            validationLog("Validation disabled.");
            return CaptivePortalProbeResult.success(CaptivePortalProbeResult.PROBE_UNKNOWN);
        }

        URL pacUrl = null;
        final URL[] httpsUrls = mCaptivePortalHttpsUrls;
        final URL[] httpUrls = mCaptivePortalHttpUrls;

        // On networks with a PAC instead of fetching a URL that should result in a 204
        // response, we instead simply fetch the PAC script.  This is done for a few reasons:
        // 1. At present our PAC code does not yet handle multiple PACs on multiple networks
        //    until something like https://android-review.googlesource.com/#/c/115180/ lands.
        //    Network.openConnection() will ignore network-specific PACs and instead fetch
        //    using NO_PROXY.  If a PAC is in place, the only fetch we know will succeed with
        //    NO_PROXY is the fetch of the PAC itself.
        // 2. To proxy the generate_204 fetch through a PAC would require a number of things
        //    happen before the fetch can commence, namely:
        //        a) the PAC script be fetched
        //        b) a PAC script resolver service be fired up and resolve the captive portal
        //           server.
        //    Network validation could be delayed until these prerequisities are satisifed or
        //    could simply be left to race them.  Neither is an optimal solution.
        // 3. PAC scripts are sometimes used to block or restrict Internet access and may in
        //    fact block fetching of the generate_204 URL which would lead to false negative
        //    results for network validation.
        final ProxyInfo proxyInfo = mLinkProperties.getHttpProxy();
        if (proxyInfo != null && !Uri.EMPTY.equals(proxyInfo.getPacFileUrl())) {
            pacUrl = makeURL(proxyInfo.getPacFileUrl().toString());
            if (pacUrl == null) {
                return CaptivePortalProbeResult.failed(CaptivePortalProbeResult.PROBE_UNKNOWN);
            }
        }

        if ((pacUrl == null) && (httpUrls.length == 0 || httpsUrls.length == 0
                || httpUrls[0] == null || httpsUrls[0] == null)) {
            return CaptivePortalProbeResult.failed(CaptivePortalProbeResult.PROBE_UNKNOWN);
        }

        long startTime = SystemClock.elapsedRealtime();

        final CaptivePortalProbeResult result;
        if (pacUrl != null) {
            result = sendDnsAndHttpProbes(null, pacUrl, ValidationProbeEvent.PROBE_PAC);
            reportHttpProbeResult(NETWORK_VALIDATION_PROBE_HTTP, result);
        } else if (mUseHttps && httpsUrls.length == 1 && httpUrls.length == 1) {
            // Probe results are reported inside sendHttpAndHttpsParallelWithFallbackProbes.
            result = sendHttpAndHttpsParallelWithFallbackProbes(deps, proxyInfo,
                    httpsUrls[0], httpUrls[0]);
        } else if (mUseHttps) {
            // Support result aggregation from multiple Urls.
            result = sendMultiParallelHttpAndHttpsProbes(deps, proxyInfo, httpsUrls, httpUrls);
        } else {
            result = sendDnsAndHttpProbes(proxyInfo, httpUrls[0], ValidationProbeEvent.PROBE_HTTP);
            reportHttpProbeResult(NETWORK_VALIDATION_PROBE_HTTP, result);
        }

        long endTime = SystemClock.elapsedRealtime();

        log("isCaptivePortal: isSuccessful()=" + result.isSuccessful()
                + " isPortal()=" + result.isPortal()
                + " RedirectUrl=" + result.redirectUrl
                + " isPartialConnectivity()=" + result.isPartialConnectivity()
                + " Time=" + (endTime - startTime) + "ms");

        return result;
    }

    /**
     * Do a DNS resolution and URL fetch on a known web server to see if we get the data we expect.
     * @return a CaptivePortalProbeResult inferred from the HTTP response.
     */
    private CaptivePortalProbeResult sendDnsAndHttpProbes(ProxyInfo proxy, URL url, int probeType) {
        // Pre-resolve the captive portal server host so we can log it.
        // Only do this if HttpURLConnection is about to, to avoid any potentially
        // unnecessary resolution.
        final String host = (proxy != null) ? proxy.getHost() : url.getHost();
        // This method cannot safely report probe results because it might not be running on the
        // state machine thread. Reporting results here would cause races and potentially send
        // information to callers that does not make sense because the state machine has already
        // changed state.
        final InetAddress[] resolvedAddr = sendDnsProbe(host);
        // The private IP logic only applies to captive portal detection (the HTTP probe), not
        // network validation (the HTTPS probe, which would likely fail anyway) or the PAC probe.
        if (mPrivateIpNoInternetEnabled && probeType == ValidationProbeEvent.PROBE_HTTP
                && (proxy == null) && hasPrivateIpAddress(resolvedAddr)) {
            recordProbeEventMetrics(NetworkValidationMetrics.probeTypeToEnum(probeType),
                    0 /* latency */, ProbeResult.PR_PRIVATE_IP_DNS, null /* capportData */);
            return CaptivePortalProbeResult.PRIVATE_IP;
        }
        return sendHttpProbe(url, probeType, null);
    }

    /** Do a DNS lookup for the given server, or throw UnknownHostException after timeoutMs */
    @VisibleForTesting
    protected InetAddress[] sendDnsProbeWithTimeout(String host, int timeoutMs)
                throws UnknownHostException {
        return DnsUtils.getAllByName(mDependencies.getDnsResolver(), mCleartextDnsNetwork, host,
                TYPE_ADDRCONFIG, FLAG_EMPTY, timeoutMs,
                str -> validationLog(ValidationProbeEvent.PROBE_DNS, host, str));
    }

    /** Do a DNS resolution of the given server. */
    private InetAddress[] sendDnsProbe(String host) {
        if (TextUtils.isEmpty(host)) {
            return null;
        }

        final Stopwatch watch = new Stopwatch().start();
        int result;
        InetAddress[] addresses;
        try {
            addresses = sendDnsProbeWithTimeout(host, getDnsProbeTimeout());
            result = ValidationProbeEvent.DNS_SUCCESS;
        } catch (UnknownHostException e) {
            addresses = null;
            result = ValidationProbeEvent.DNS_FAILURE;
        }
        final long latency = watch.stop();
        recordProbeEventMetrics(ProbeType.PT_DNS, latency,
                (result == ValidationProbeEvent.DNS_SUCCESS) ? ProbeResult.PR_SUCCESS :
                ProbeResult.PR_FAILURE, null /* capportData */);
        logValidationProbe(latency, ValidationProbeEvent.PROBE_DNS, result);
        return addresses;
    }

    /**
     * Check if any of the provided IP addresses include a private IP.
     * @return true if an IP address is private.
     */
    private static boolean hasPrivateIpAddress(@Nullable InetAddress[] addresses) {
        if (addresses == null) {
            return false;
        }
        for (InetAddress address : addresses) {
            if (address.isLinkLocalAddress() || address.isSiteLocalAddress()
                    || isIPv6ULA(address)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Do a URL fetch on a known web server to see if we get the data we expect.
     * @return a CaptivePortalProbeResult inferred from the HTTP response.
     */
    @VisibleForTesting
    protected CaptivePortalProbeResult sendHttpProbe(URL url, int probeType,
            @Nullable CaptivePortalProbeSpec probeSpec) {
        HttpURLConnection urlConnection = null;
        int httpResponseCode = CaptivePortalProbeResult.FAILED_CODE;
        String redirectUrl = null;
        final Stopwatch probeTimer = new Stopwatch().start();
        final int oldTag = TrafficStats.getAndSetThreadStatsTag(
                TrafficStatsConstants.TAG_SYSTEM_PROBE);
        try {
            // Follow redirects for PAC probes as such probes verify connectivity by fetching the
            // PAC proxy file, which may be configured behind a redirect.
            final boolean followRedirect = probeType == ValidationProbeEvent.PROBE_PAC;
            urlConnection = makeProbeConnection(url, followRedirect);
            // cannot read request header after connection
            String requestHeader = urlConnection.getRequestProperties().toString();

            // Time how long it takes to get a response to our request
            long requestTimestamp = SystemClock.elapsedRealtime();

            httpResponseCode = urlConnection.getResponseCode();
            redirectUrl = urlConnection.getHeaderField("location");

            // Time how long it takes to get a response to our request
            long responseTimestamp = SystemClock.elapsedRealtime();

            validationLog(probeType, url, "time=" + (responseTimestamp - requestTimestamp) + "ms"
                    + " ret=" + httpResponseCode
                    + " request=" + requestHeader
                    + " headers=" + urlConnection.getHeaderFields());
            // NOTE: We may want to consider an "HTTP/1.0 204" response to be a captive
            // portal.  The only example of this seen so far was a captive portal.  For
            // the time being go with prior behavior of assuming it's not a captive
            // portal.  If it is considered a captive portal, a different sign-in URL
            // is needed (i.e. can't browse a 204).  This could be the result of an HTTP
            // proxy server.
            if (httpResponseCode == 200) {
                long contentLength = urlConnection.getContentLengthLong();
                if (probeType == ValidationProbeEvent.PROBE_PAC) {
                    validationLog(
                            probeType, url, "PAC fetch 200 response interpreted as 204 response.");
                    httpResponseCode = CaptivePortalProbeResult.SUCCESS_CODE;
                } else if (contentLength == -1) {
                    // When no Content-length (default value == -1), attempt to read a byte
                    // from the response. Do not use available() as it is unreliable.
                    // See http://b/33498325.
                    if (urlConnection.getInputStream().read() == -1) {
                        validationLog(probeType, url,
                                "Empty 200 response interpreted as failed response.");
                        httpResponseCode = CaptivePortalProbeResult.FAILED_CODE;
                    }
                } else if (matchesHttpContentLength(contentLength)) {
                    final InputStream is = new BufferedInputStream(urlConnection.getInputStream());
                    final String content = readAsString(is, (int) contentLength,
                            extractCharset(urlConnection.getContentType()));
                    if (matchesHttpContent(content,
                            R.string.config_network_validation_failed_content_regexp)) {
                        httpResponseCode = CaptivePortalProbeResult.FAILED_CODE;
                    } else if (matchesHttpContent(content,
                            R.string.config_network_validation_success_content_regexp)) {
                        httpResponseCode = CaptivePortalProbeResult.SUCCESS_CODE;
                    }

                    if (httpResponseCode != 200) {
                        validationLog(probeType, url, "200 response with Content-length ="
                                + contentLength + ", content matches custom regexp, interpreted"
                                + " as " + httpResponseCode
                                + " response.");
                    }
                } else if (contentLength <= 4) {
                    // Consider 200 response with "Content-length <= 4" to not be a captive
                    // portal. There's no point in considering this a captive portal as the
                    // user cannot sign-in to an empty page. Probably the result of a broken
                    // transparent proxy. See http://b/9972012 and http://b/122999481.
                    validationLog(probeType, url, "200 response with Content-length <= 4"
                            + " interpreted as failed response.");
                    httpResponseCode = CaptivePortalProbeResult.FAILED_CODE;
                }
            }
        } catch (IOException e) {
            validationLog(probeType, url, "Probe failed with exception " + e);
            if (httpResponseCode == CaptivePortalProbeResult.FAILED_CODE) {
                // TODO: Ping gateway and DNS server and log results.
            }
        } finally {
            if (urlConnection != null) {
                urlConnection.disconnect();
            }
            TrafficStats.setThreadStatsTag(oldTag);
        }
        logValidationProbe(probeTimer.stop(), probeType, httpResponseCode);

        final CaptivePortalProbeResult probeResult;
        if (probeSpec == null) {
            probeResult = new CaptivePortalProbeResult(httpResponseCode, redirectUrl,
                    url.toString(),   1 << probeType);
        } else {
            probeResult = probeSpec.getResult(httpResponseCode, redirectUrl);
        }
        recordProbeEventMetrics(NetworkValidationMetrics.probeTypeToEnum(probeType),
                probeTimer.stop(), NetworkValidationMetrics.httpProbeResultToEnum(probeResult),
                null /* capportData */);
        return probeResult;
    }

    @VisibleForTesting
    boolean matchesHttpContent(final String content, @StringRes final int configResource) {
        final String resString = getResStringConfig(mContext, configResource, "");
        try {
            return content.matches(resString);
        } catch (PatternSyntaxException e) {
            Log.e(TAG, "Pattern syntax exception occurs when matching the resource=" + resString,
                    e);
            return false;
        }
    }

    @VisibleForTesting
    boolean matchesHttpContentLength(final long contentLength) {
        // Consider that the Resources#getInteger() is returning an integer, so if the contentLength
        // is lower or equal to 0 or higher than Integer.MAX_VALUE, then it's an invalid value.
        if (contentLength <= 0) return false;
        if (contentLength > Integer.MAX_VALUE) {
            logw("matchesHttpContentLength : Get invalid contentLength = " + contentLength);
            return false;
        }
        return (contentLength > getResIntConfig(mContext,
                R.integer.config_min_matches_http_content_length, Integer.MAX_VALUE)
                &&
                contentLength < getResIntConfig(mContext,
                R.integer.config_max_matches_http_content_length, 0));
    }

    private HttpURLConnection makeProbeConnection(URL url, boolean followRedirects)
            throws IOException {
        final HttpURLConnection conn = (HttpURLConnection) mCleartextDnsNetwork.openConnection(url);
        conn.setInstanceFollowRedirects(followRedirects);
        conn.setConnectTimeout(SOCKET_TIMEOUT_MS);
        conn.setReadTimeout(SOCKET_TIMEOUT_MS);
        conn.setRequestProperty("Connection", "close");
        conn.setUseCaches(false);
        if (mCaptivePortalUserAgent != null) {
            conn.setRequestProperty("User-Agent", mCaptivePortalUserAgent);
        }
        return conn;
    }

    @VisibleForTesting
    @NonNull
    protected static String readAsString(InputStream is, int maxLength, Charset charset)
            throws IOException {
        final InputStreamReader reader = new InputStreamReader(is, charset);
        final char[] buffer = new char[1000];
        final StringBuilder builder = new StringBuilder();
        int totalReadLength = 0;
        while (totalReadLength < maxLength) {
            final int availableLength = Math.min(maxLength - totalReadLength, buffer.length);
            final int currentLength = reader.read(buffer, 0, availableLength);
            if (currentLength < 0) break; // EOF

            totalReadLength += currentLength;
            builder.append(buffer, 0, currentLength);
        }
        return builder.toString();
    }

    /**
     * Attempt to extract the {@link Charset} of the response from its Content-Type header.
     *
     * <p>If the {@link Charset} cannot be extracted, UTF-8 is returned by default.
     */
    @VisibleForTesting
    @NonNull
    protected static Charset extractCharset(@Nullable String contentTypeHeader) {
        if (contentTypeHeader == null) return StandardCharsets.UTF_8;
        // See format in https://tools.ietf.org/html/rfc7231#section-3.1.1.1
        final Pattern charsetPattern = Pattern.compile("; *charset=\"?([^ ;\"]+)\"?",
                Pattern.CASE_INSENSITIVE);
        final Matcher matcher = charsetPattern.matcher(contentTypeHeader);
        if (!matcher.find()) return StandardCharsets.UTF_8;

        try {
            return Charset.forName(matcher.group(1));
        } catch (IllegalArgumentException e) {
            return StandardCharsets.UTF_8;
        }
    }

    private class ProbeThread extends Thread {
        private final CountDownLatch mLatch;
        private final Probe mProbe;

        ProbeThread(CountDownLatch latch, EvaluationThreadDeps deps, ProxyInfo proxy, URL url,
                int probeType, Uri captivePortalApiUrl) {
            mLatch = latch;
            mProbe = (probeType == ValidationProbeEvent.PROBE_HTTPS)
                    ? new HttpsProbe(deps, proxy, url, captivePortalApiUrl)
                    : new HttpProbe(deps, proxy, url, captivePortalApiUrl);
            mResult = CaptivePortalProbeResult.failed(probeType);
        }

        private volatile CaptivePortalProbeResult mResult;

        public CaptivePortalProbeResult result() {
            return mResult;
        }

        @Override
        public void run() {
            mResult = mProbe.sendProbe();
            if (isConclusiveResult(mResult, mProbe.mCaptivePortalApiUrl)) {
                // Stop waiting immediately if any probe is conclusive.
                while (mLatch.getCount() > 0) {
                    mLatch.countDown();
                }
            }
            // Signal this probe has completed.
            mLatch.countDown();
        }
    }

    private abstract static class Probe {
        protected final EvaluationThreadDeps mDeps;
        protected final ProxyInfo mProxy;
        protected final URL mUrl;
        protected final Uri mCaptivePortalApiUrl;

        protected Probe(EvaluationThreadDeps deps, ProxyInfo proxy, URL url,
                Uri captivePortalApiUrl) {
            mDeps = deps;
            mProxy = proxy;
            mUrl = url;
            mCaptivePortalApiUrl = captivePortalApiUrl;
        }
        // sendProbe() is synchronous and blocks until it has the result.
        protected abstract CaptivePortalProbeResult sendProbe();
    }

    final class HttpsProbe extends Probe {
        HttpsProbe(EvaluationThreadDeps deps, ProxyInfo proxy, URL url, Uri captivePortalApiUrl) {
            super(deps, proxy, url, captivePortalApiUrl);
        }

        @Override
        protected CaptivePortalProbeResult sendProbe() {
            return sendDnsAndHttpProbes(mProxy, mUrl, ValidationProbeEvent.PROBE_HTTPS);
        }
    }

    final class HttpProbe extends Probe {
        HttpProbe(EvaluationThreadDeps deps, ProxyInfo proxy, URL url, Uri captivePortalApiUrl) {
            super(deps, proxy, url, captivePortalApiUrl);
        }

        private CaptivePortalDataShim sendCapportApiProbe() {
            // TODO: consider adding metrics counters for each case returning null in this method
            // (cases where the API is not implemented properly).
            validationLog("Fetching captive portal data from " + mCaptivePortalApiUrl);

            final String apiContent;
            try {
                final URL url = new URL(mCaptivePortalApiUrl.toString());
                // Protocol must be HTTPS
                // (as per https://www.ietf.org/id/draft-ietf-capport-api-07.txt, #4).
                // Only allow HTTP on localhost, for testing.
                final boolean isTestLocalhostHttp = mDeps.mIsTestNetwork
                        && "localhost".equals(url.getHost()) && "http".equals(url.getProtocol());
                if (!"https".equals(url.getProtocol()) && !isTestLocalhostHttp) {
                    validationLog("Invalid captive portal API protocol: " + url.getProtocol());
                    return null;
                }

                final HttpURLConnection conn = makeProbeConnection(
                        url, true /* followRedirects */);
                conn.setRequestProperty(ACCEPT_HEADER, CAPPORT_API_CONTENT_TYPE);
                final int responseCode = conn.getResponseCode();
                if (responseCode != 200) {
                    validationLog("Non-200 API response code: " + conn.getResponseCode());
                    return null;
                }
                final Charset charset = extractCharset(conn.getHeaderField(CONTENT_TYPE_HEADER));
                if (charset != StandardCharsets.UTF_8) {
                    validationLog("Invalid charset for capport API: " + charset);
                    return null;
                }

                apiContent = readAsString(conn.getInputStream(),
                        CAPPORT_API_MAX_JSON_LENGTH, charset);
            } catch (IOException e) {
                validationLog("I/O error reading capport data: " + e.getMessage());
                return null;
            }

            try {
                final JSONObject info = new JSONObject(apiContent);
                final CaptivePortalDataShim capportData = CaptivePortalDataShimImpl.fromJson(info);
                if (capportData != null && capportData.isCaptive()
                        && capportData.getUserPortalUrl() == null) {
                    validationLog("Missing user-portal-url from capport response");
                    return null;
                }
                return capportData;
            } catch (JSONException e) {
                validationLog("Could not parse capport API JSON: " + e.getMessage());
                return null;
            } catch (UnsupportedApiLevelException e) {
                // This should never happen because LinkProperties would not have a capport URL
                // before R.
                validationLog("Platform API too low to support capport API");
                return null;
            }
        }

        private CaptivePortalDataShim tryCapportApiProbe() {
            if (mCaptivePortalApiUrl == null) return null;
            final Stopwatch capportApiWatch = new Stopwatch().start();
            final CaptivePortalDataShim capportData = sendCapportApiProbe();
            recordProbeEventMetrics(ProbeType.PT_CAPPORT_API, capportApiWatch.stop(),
                    capportData == null ? ProbeResult.PR_FAILURE : ProbeResult.PR_SUCCESS,
                    capportData);
            return capportData;
        }

        @Override
        protected CaptivePortalProbeResult sendProbe() {
            final CaptivePortalDataShim capportData = tryCapportApiProbe();
            if (capportData != null && capportData.isCaptive()) {
                final String loginUrlString = capportData.getUserPortalUrl().toString();
                // Starting from R (where CaptivePortalData was introduced), the captive portal app
                // delegates to NetworkMonitor for verifying when the network validates instead of
                // probing the detectUrl. So pass the detectUrl to have the portal open on that,
                // page; CaptivePortalLogin will not use it for probing.
                return new CapportApiProbeResult(
                        CaptivePortalProbeResult.PORTAL_CODE,
                        loginUrlString /* redirectUrl */,
                        loginUrlString /* detectUrl */,
                        capportData,
                        1 << ValidationProbeEvent.PROBE_HTTP);
            }

            // If the API says it's not captive, still check for HTTP connectivity. This helps
            // with partial connectivity detection, and a broken API saying that there is no
            // redirect when there is one.
            final CaptivePortalProbeResult res =
                    sendDnsAndHttpProbes(mProxy, mUrl, ValidationProbeEvent.PROBE_HTTP);
            return mCaptivePortalApiUrl == null ? res : new CapportApiProbeResult(res, capportData);
        }
    }

    private static boolean isConclusiveResult(@NonNull CaptivePortalProbeResult result,
            @Nullable Uri captivePortalApiUrl) {
        // isPortal() is not expected on the HTTPS probe, but treat the network as portal would make
        // sense if the probe reports portal. In case the capport API is available, the API is
        // authoritative on whether there is a portal, so the HTTPS probe is not enough to conclude
        // there is connectivity, and a determination will be made once the capport API probe
        // returns. Note that the API can only force the system to detect a portal even if the HTTPS
        // probe succeeds. It cannot force the system to detect no portal if the HTTPS probe fails.
        return result.isPortal()
                || (result.isConcludedFromHttps() && result.isSuccessful()
                        && captivePortalApiUrl == null);
    }

    private CaptivePortalProbeResult sendMultiParallelHttpAndHttpsProbes(
            @NonNull EvaluationThreadDeps deps, @Nullable ProxyInfo proxy, @NonNull URL[] httpsUrls,
            @NonNull URL[] httpUrls) {
        // If multiple URLs are required to ensure the correctness of validation, send parallel
        // probes to explore the result in separate probe threads and aggregate those results into
        // one as the final result for either HTTP or HTTPS.

        // Number of probes to wait for.
        final int num = httpsUrls.length + httpUrls.length;
        // Fixed pool to prevent configuring too many urls to exhaust system resource.
        final ExecutorService executor = Executors.newFixedThreadPool(
                Math.min(num, MAX_PROBE_THREAD_POOL_SIZE));
        final CompletionService<CaptivePortalProbeResult> ecs =
                new ExecutorCompletionService<CaptivePortalProbeResult>(executor);
        final Uri capportApiUrl = getCaptivePortalApiUrl(mLinkProperties);
        final List<Future<CaptivePortalProbeResult>> futures = new ArrayList<>();

        try {
            // Queue https and http probe.

            // Each of these HTTP probes will start with probing capport API if present. So if
            // multiple HTTP URLs are configured, AP will send multiple identical accesses to the
            // capport URL. Thus, send capport API probing with one of the HTTP probe is enough.
            // Probe capport API with the first HTTP probe.
            // TODO: Have the capport probe as a different probe for cleanliness.
            final URL urlMaybeWithCapport = httpUrls[0];
            for (final URL url : httpUrls) {
                futures.add(ecs.submit(() -> new HttpProbe(deps, proxy, url,
                        url.equals(urlMaybeWithCapport) ? capportApiUrl : null).sendProbe()));
            }

            for (final URL url : httpsUrls) {
                futures.add(ecs.submit(() -> new HttpsProbe(deps, proxy, url, capportApiUrl)
                        .sendProbe()));
            }

            final ArrayList<CaptivePortalProbeResult> completedProbes = new ArrayList<>();
            for (int i = 0; i < num; i++) {
                completedProbes.add(ecs.take().get());
                final CaptivePortalProbeResult res = evaluateCapportResult(
                        completedProbes, httpsUrls.length, capportApiUrl != null /* hasCapport */);
                if (res != null) {
                    reportProbeResult(res);
                    return res;
                }
            }
        } catch (ExecutionException e) {
            Log.e(TAG, "Error sending probes.", e);
        } catch (InterruptedException e) {
            // Ignore interrupted probe result because result is not important to conclude the
            // result.
        } finally {
            // Interrupt ongoing probes since we have already gotten result from one of them.
            futures.forEach(future -> future.cancel(true));
            executor.shutdownNow();
        }

        return CaptivePortalProbeResult.failed(ValidationProbeEvent.PROBE_HTTPS);
    }

    @Nullable
    private CaptivePortalProbeResult evaluateCapportResult(
            List<CaptivePortalProbeResult> probes, int numHttps, boolean hasCapport) {
        CaptivePortalProbeResult capportResult = null;
        CaptivePortalProbeResult httpPortalResult = null;
        int httpSuccesses = 0;
        int httpsSuccesses = 0;
        int httpsFailures = 0;

        for (CaptivePortalProbeResult probe : probes) {
            if (probe instanceof CapportApiProbeResult) {
                capportResult = probe;
            } else if (probe.isConcludedFromHttps()) {
                if (probe.isSuccessful()) httpsSuccesses++;
                else httpsFailures++;
            } else { // http probes
                if (probe.isPortal()) {
                    // Unlike https probe, http probe may have redirect url information kept in the
                    // probe result. Thus, the result can not be newly created with response code
                    // only. If the captive portal behavior will be varied because of different
                    // probe URLs, this means that if the portal returns different redirect URLs for
                    // different probes and has a different behavior depending on the URL, then the
                    // behavior of the login page may differ depending on the order in which the
                    // probes terminate. However, NetworkMonitor does have to choose one of the
                    // redirect URLs and right now there is no clue at all which of the probe has
                    // the better redirect URL, so there is no telling which is best to use.
                    // Therefore the current code just uses whichever happens to be the last one to
                    // complete.
                    httpPortalResult = probe;
                } else if (probe.isSuccessful()) {
                    httpSuccesses++;
                }
            }
        }
        // If there is Capport url configured but the result is not available yet, wait for it.
        if (hasCapport && capportResult == null) return null;
        // Capport API saying it's a portal is authoritative.
        if (capportResult != null && capportResult.isPortal()) return capportResult;
        // Any HTTP probes saying probe portal is conclusive.
        if (httpPortalResult != null) return httpPortalResult;
        // Any HTTPS probes works then the network validates.
        if (httpsSuccesses > 0) {
            return CaptivePortalProbeResult.success(1 << ValidationProbeEvent.PROBE_HTTPS);
        }
        // All HTTPS failed and at least one HTTP succeeded, then it's partial.
        if (httpsFailures == numHttps && httpSuccesses > 0) {
            return CaptivePortalProbeResult.PARTIAL;
        }
        // Otherwise, the result is unknown yet.
        return null;
    }

    private void reportProbeResult(@NonNull CaptivePortalProbeResult res) {
        if (res instanceof CapportApiProbeResult) {
            maybeReportCaptivePortalData(((CapportApiProbeResult) res).getCaptivePortalData());
        }

        // This is not a if-else case since partial connectivity will concluded from both HTTP and
        // HTTPS probe. Both HTTP and HTTPS result should be reported.
        if (res.isConcludedFromHttps()) {
            reportHttpProbeResult(NETWORK_VALIDATION_PROBE_HTTPS, res);
        }

        if (res.isConcludedFromHttp()) {
            reportHttpProbeResult(NETWORK_VALIDATION_PROBE_HTTP, res);
        }
    }

    private CaptivePortalProbeResult sendHttpAndHttpsParallelWithFallbackProbes(
            EvaluationThreadDeps deps, ProxyInfo proxy, URL httpsUrl, URL httpUrl) {
        // Number of probes to wait for. If a probe completes with a conclusive answer
        // it shortcuts the latch immediately by forcing the count to 0.
        final CountDownLatch latch = new CountDownLatch(2);

        final Uri capportApiUrl = getCaptivePortalApiUrl(mLinkProperties);
        final ProbeThread httpsProbe = new ProbeThread(latch, deps, proxy, httpsUrl,
                ValidationProbeEvent.PROBE_HTTPS, capportApiUrl);
        final ProbeThread httpProbe = new ProbeThread(latch, deps, proxy, httpUrl,
                ValidationProbeEvent.PROBE_HTTP, capportApiUrl);

        try {
            httpsProbe.start();
            httpProbe.start();
            latch.await(PROBE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            validationLog("Error: probes wait interrupted!");
            return CaptivePortalProbeResult.failed(CaptivePortalProbeResult.PROBE_UNKNOWN);
        }

        final CaptivePortalProbeResult httpsResult = httpsProbe.result();
        final CaptivePortalProbeResult httpResult = httpProbe.result();

        // Look for a conclusive probe result first.
        if (isConclusiveResult(httpResult, capportApiUrl)) {
            reportProbeResult(httpProbe.result());
            return httpResult;
        }

        if (isConclusiveResult(httpsResult, capportApiUrl)) {
            reportProbeResult(httpsProbe.result());
            return httpsResult;
        }
        // Consider a DNS response with a private IP address on the HTTP probe as an indication that
        // the network is not connected to the Internet, and have the whole evaluation fail in that
        // case, instead of potentially detecting a captive portal. This logic only affects portal
        // detection, not network validation.
        // This only applies if the DNS probe completed within PROBE_TIMEOUT_MS, as the fallback
        // probe should not be delayed by this check.
        if (mPrivateIpNoInternetEnabled && (httpResult.isDnsPrivateIpResponse())) {
            validationLog("DNS response to the URL is private IP");
            return CaptivePortalProbeResult.failed(1 << ValidationProbeEvent.PROBE_HTTP);
        }
        // If a fallback method exists, use it to retry portal detection.
        // If we have new-style probe specs, use those. Otherwise, use the fallback URLs.
        final CaptivePortalProbeSpec probeSpec = nextFallbackSpec();
        final URL fallbackUrl = (probeSpec != null) ? probeSpec.getUrl() : nextFallbackUrl();
        CaptivePortalProbeResult fallbackProbeResult = null;
        if (fallbackUrl != null) {
            fallbackProbeResult = sendHttpProbe(fallbackUrl, PROBE_FALLBACK, probeSpec);
            reportHttpProbeResult(NETWORK_VALIDATION_PROBE_FALLBACK, fallbackProbeResult);
            if (fallbackProbeResult.isPortal()) {
                return fallbackProbeResult;
            }
        }
        // Otherwise wait until http and https probes completes and use their results.
        try {
            httpProbe.join();
            reportProbeResult(httpProbe.result());

            if (httpProbe.result().isPortal()) {
                return httpProbe.result();
            }

            httpsProbe.join();
            reportHttpProbeResult(NETWORK_VALIDATION_PROBE_HTTPS, httpsProbe.result());

            final boolean isHttpSuccessful =
                    (httpProbe.result().isSuccessful()
                    || (fallbackProbeResult != null && fallbackProbeResult.isSuccessful()));
            if (httpsProbe.result().isFailed() && isHttpSuccessful) {
                return CaptivePortalProbeResult.PARTIAL;
            }
            return httpsProbe.result();
        } catch (InterruptedException e) {
            validationLog("Error: http or https probe wait interrupted!");
            return CaptivePortalProbeResult.failed(CaptivePortalProbeResult.PROBE_UNKNOWN);
        }
    }

    private URL makeURL(String url) {
        if (url != null) {
            try {
                return new URL(url);
            } catch (MalformedURLException e) {
                validationLog("Bad URL: " + url);
            }
        }
        return null;
    }

    private void logNetworkEvent(int evtype) {
        int[] transports = mNetworkCapabilities.getTransportTypes();
        mMetricsLog.log(mCleartextDnsNetwork, transports, new NetworkEvent(evtype));
    }

    private int networkEventType(ValidationStage s, EvaluationResult r) {
        if (s.mIsFirstValidation) {
            if (r.mIsValidated) {
                return NetworkEvent.NETWORK_FIRST_VALIDATION_SUCCESS;
            } else {
                return NetworkEvent.NETWORK_FIRST_VALIDATION_PORTAL_FOUND;
            }
        } else {
            if (r.mIsValidated) {
                return NetworkEvent.NETWORK_REVALIDATION_SUCCESS;
            } else {
                return NetworkEvent.NETWORK_REVALIDATION_PORTAL_FOUND;
            }
        }
    }

    private void maybeLogEvaluationResult(int evtype) {
        if (mEvaluationTimer.isRunning()) {
            int[] transports = mNetworkCapabilities.getTransportTypes();
            mMetricsLog.log(mCleartextDnsNetwork, transports,
                    new NetworkEvent(evtype, mEvaluationTimer.stop() / 1000));
            mEvaluationTimer.reset();
        }
    }

    private void logValidationProbe(long durationUs, int probeType, int probeResult) {
        int[] transports = mNetworkCapabilities.getTransportTypes();
        boolean isFirstValidation = validationStage().mIsFirstValidation;
        ValidationProbeEvent ev = new ValidationProbeEvent.Builder()
                .setProbeType(probeType, isFirstValidation)
                .setReturnCode(probeResult)
                .setDurationMs(durationUs / 1000)
                .build();
        mMetricsLog.log(mCleartextDnsNetwork, transports, ev);
    }

    @VisibleForTesting
    public static class Dependencies {
        public Network getPrivateDnsBypassNetwork(Network network) {
            return new OneAddressPerFamilyNetwork(network);
        }

        public DnsResolver getDnsResolver() {
            return DnsResolver.getInstance();
        }

        public Random getRandom() {
            return new Random();
        }

        /**
         * Get the value of a global integer setting.
         * @param symbol Name of the setting
         * @param defaultValue Value to return if the setting is not defined.
         */
        public int getSetting(Context context, String symbol, int defaultValue) {
            return Settings.Global.getInt(context.getContentResolver(), symbol, defaultValue);
        }

        /**
         * Get the value of a global String setting.
         * @param symbol Name of the setting
         * @param defaultValue Value to return if the setting is not defined.
         */
        public String getSetting(Context context, String symbol, String defaultValue) {
            final String value = Settings.Global.getString(context.getContentResolver(), symbol);
            return value != null ? value : defaultValue;
        }

        /**
         * Look up the value of a property in DeviceConfig.
         * @param namespace The namespace containing the property to look up.
         * @param name The name of the property to look up.
         * @param defaultValue The value to return if the property does not exist or has no non-null
         *                     value.
         * @return the corresponding value, or defaultValue if none exists.
         */
        @Nullable
        public String getDeviceConfigProperty(@NonNull String namespace, @NonNull String name,
                @Nullable String defaultValue) {
            return NetworkStackUtils.getDeviceConfigProperty(namespace, name, defaultValue);
        }

        /**
         * Look up the value of a property in DeviceConfig.
         * @param namespace The namespace containing the property to look up.
         * @param name The name of the property to look up.
         * @param defaultValue The value to return if the property does not exist or has no non-null
         *                     value.
         * @return the corresponding value, or defaultValue if none exists.
         */
        public int getDeviceConfigPropertyInt(@NonNull String namespace, @NonNull String name,
                int defaultValue) {
            return NetworkStackUtils.getDeviceConfigPropertyInt(namespace, name, defaultValue);
        }

        /**
         * Check whether or not one experimental feature in the connectivity namespace is
         * enabled.
         * @param name Flag name of the experiment in the connectivity namespace.
         * @see NetworkStackUtils#isFeatureEnabled(Context, String, String)
         */
        public boolean isFeatureEnabled(@NonNull Context context, @NonNull String name) {
            return NetworkStackUtils.isFeatureEnabled(context, NAMESPACE_CONNECTIVITY, name);
        }

        /**
         * Check whether or not one specific experimental feature for a particular namespace from
         * {@link DeviceConfig} is enabled by comparing NetworkStack module version
         * {@link NetworkStack} with current version of property. If this property version is valid,
         * the corresponding experimental feature would be enabled, otherwise disabled.
         * @param context The global context information about an app environment.
         * @param namespace The namespace containing the property to look up.
         * @param name The name of the property to look up.
         * @param defaultEnabled The value to return if the property does not exist or its value is
         *                       null.
         * @return true if this feature is enabled, or false if disabled.
         */
        public boolean isFeatureEnabled(@NonNull Context context, @NonNull String namespace,
                @NonNull String name, boolean defaultEnabled) {
            return NetworkStackUtils.isFeatureEnabled(context, namespace, name, defaultEnabled);
        }

        /**
         * Collect data stall detection level information for each transport type. Write metrics
         * data to statsd pipeline.
         * @param stats a {@link DataStallDetectionStats} that contains the detection level
         *              information.
         * @para result the network reevaluation result.
         */
        public void writeDataStallDetectionStats(@NonNull final DataStallDetectionStats stats,
                @NonNull final CaptivePortalProbeResult result) {
            DataStallStatsUtils.write(stats, result);
        }

        public static final Dependencies DEFAULT = new Dependencies();
    }

    /**
     * Methods in this class perform no locking because all accesses are performed on the state
     * machine's thread. Need to consider the thread safety if it ever could be accessed outside the
     * state machine.
     */
    @VisibleForTesting
    protected class DnsStallDetector {
        private int mConsecutiveTimeoutCount = 0;
        private int mSize;
        final DnsResult[] mDnsEvents;
        final RingBufferIndices mResultIndices;

        DnsStallDetector(int size) {
            mSize = Math.max(DEFAULT_DNS_LOG_SIZE, size);
            mDnsEvents = new DnsResult[mSize];
            mResultIndices = new RingBufferIndices(mSize);
        }

        @VisibleForTesting
        protected void accumulateConsecutiveDnsTimeoutCount(int code) {
            final DnsResult result = new DnsResult(code);
            mDnsEvents[mResultIndices.add()] = result;
            if (result.isTimeout()) {
                mConsecutiveTimeoutCount++;
            } else {
                // Keep the event in mDnsEvents without clearing it so that there are logs to do the
                // simulation and analysis.
                mConsecutiveTimeoutCount = 0;
            }
        }

        private boolean isDataStallSuspected(int timeoutCountThreshold, int validTime) {
            if (timeoutCountThreshold <= 0) {
                Log.wtf(TAG, "Timeout count threshold should be larger than 0.");
                return false;
            }

            // Check if the consecutive timeout count reach the threshold or not.
            if (mConsecutiveTimeoutCount < timeoutCountThreshold) {
                return false;
            }

            // Check if the target dns event index is valid or not.
            final int firstConsecutiveTimeoutIndex =
                    mResultIndices.indexOf(mResultIndices.size() - timeoutCountThreshold);

            // If the dns timeout events happened long time ago, the events are meaningless for
            // data stall evaluation. Thus, check if the first consecutive timeout dns event
            // considered in the evaluation happened in defined threshold time.
            final long now = SystemClock.elapsedRealtime();
            final long firstTimeoutTime = now - mDnsEvents[firstConsecutiveTimeoutIndex].mTimeStamp;
            return (firstTimeoutTime < validTime);
        }

        int getConsecutiveTimeoutCount() {
            return mConsecutiveTimeoutCount;
        }
    }

    private static class DnsResult {
        // TODO: Need to move the DNS return code definition to a specific class once unify DNS
        // response code is done.
        private static final int RETURN_CODE_DNS_TIMEOUT = 255;

        private final long mTimeStamp;
        private final int mReturnCode;

        DnsResult(int code) {
            mTimeStamp = SystemClock.elapsedRealtime();
            mReturnCode = code;
        }

        private boolean isTimeout() {
            return mReturnCode == RETURN_CODE_DNS_TIMEOUT;
        }
    }

    @VisibleForTesting
    @Nullable
    protected DnsStallDetector getDnsStallDetector() {
        return mDnsStallDetector;
    }

    @Nullable
    private TcpSocketTracker getTcpSocketTracker() {
        return mTcpTracker;
    }

    private boolean dataStallEvaluateTypeEnabled(int type) {
        return (mDataStallEvaluationType & type) != 0;
    }

    @VisibleForTesting
    protected long getLastProbeTime() {
        return mLastProbeTime;
    }

    @VisibleForTesting
    protected boolean isDataStall() {
        if (!isValidationRequired()) {
            return false;
        }

        Boolean result = null;
        final StringJoiner msg = (DBG || VDBG_STALL) ? new StringJoiner(", ") : null;
        // Reevaluation will generate traffic. Thus, set a minimal reevaluation timer to limit the
        // possible traffic cost in metered network.
        if (!mNetworkCapabilities.hasCapability(NET_CAPABILITY_NOT_METERED)
                && (SystemClock.elapsedRealtime() - getLastProbeTime()
                < mDataStallMinEvaluateTime)) {
            return false;
        }
        // Check TCP signal. Suspect it may be a data stall if :
        // 1. TCP connection fail rate(lost+retrans) is higher than threshold.
        // 2. Accumulate enough packets count.
        final TcpSocketTracker tst = getTcpSocketTracker();
        if (dataStallEvaluateTypeEnabled(DATA_STALL_EVALUATION_TYPE_TCP) && tst != null) {
            if (tst.getLatestReceivedCount() > 0) {
                result = false;
            } else if (tst.isDataStallSuspected()) {
                result = true;
                mDataStallTypeToCollect = DATA_STALL_EVALUATION_TYPE_TCP;

                final DataStallReportParcelable p = new DataStallReportParcelable();
                p.detectionMethod = DETECTION_METHOD_TCP_METRICS;
                p.timestampMillis = SystemClock.elapsedRealtime();
                p.tcpPacketFailRate = tst.getLatestPacketFailPercentage();
                p.tcpMetricsCollectionPeriodMillis = getTcpPollingInterval();

                notifyDataStallSuspected(p);
            }
            if (DBG || VDBG_STALL) {
                msg.add("tcp packets received=" + tst.getLatestReceivedCount())
                    .add("latest tcp fail rate=" + tst.getLatestPacketFailPercentage());
            }
        }

        // Check dns signal. Suspect it may be a data stall if both :
        // 1. The number of consecutive DNS query timeouts >= mConsecutiveDnsTimeoutThreshold.
        // 2. Those consecutive DNS queries happened in the last mValidDataStallDnsTimeThreshold ms.
        final DnsStallDetector dsd = getDnsStallDetector();
        if ((result == null) && (dsd != null)
                && dataStallEvaluateTypeEnabled(DATA_STALL_EVALUATION_TYPE_DNS)) {
            if (dsd.isDataStallSuspected(mConsecutiveDnsTimeoutThreshold,
                    mDataStallValidDnsTimeThreshold)) {
                result = true;
                mDataStallTypeToCollect = DATA_STALL_EVALUATION_TYPE_DNS;
                logNetworkEvent(NetworkEvent.NETWORK_CONSECUTIVE_DNS_TIMEOUT_FOUND);

                final DataStallReportParcelable p = new DataStallReportParcelable();
                p.detectionMethod = DETECTION_METHOD_DNS_EVENTS;
                p.timestampMillis = SystemClock.elapsedRealtime();
                p.dnsConsecutiveTimeouts = mDnsStallDetector.getConsecutiveTimeoutCount();
                notifyDataStallSuspected(p);
            }
            if (DBG || VDBG_STALL) {
                msg.add("consecutive dns timeout count=" + dsd.getConsecutiveTimeoutCount());
            }
        }
        // log only data stall suspected.
        if ((DBG && Boolean.TRUE.equals(result)) || VDBG_STALL) {
            log("isDataStall: result=" + result + ", " + msg);
        }

        return (result == null) ? false : result;
    }

    // Class to keep state of evaluation results and probe results.
    //
    // The main purpose was to ensure NetworkMonitor can notify ConnectivityService of probe results
    // as soon as they happen, without triggering any other changes. This requires keeping state on
    // the most recent evaluation result. Calling noteProbeResult will ensure that the results
    // reported to ConnectivityService contain the previous evaluation result, and thus won't
    // trigger a validation or partial connectivity state change.
    //
    // Note that this class is not currently being used for this purpose. The reason is that some
    // of the system behaviour triggered by reporting network validation - notably, NetworkAgent
    // behaviour - depends not only on the value passed by notifyNetworkTested, but also on the
    // fact that notifyNetworkTested was called. For example, telephony triggers network recovery
    // any time it is told that validation failed, i.e., if the result does not contain
    // NETWORK_VALIDATION_RESULT_VALID. But with this scheme, the first two or three validation
    // reports are all failures, because they are "HTTP succeeded but validation not yet passed",
    // "HTTP and HTTPS succeeded but validation not yet passed", etc.
    @VisibleForTesting
    protected class EvaluationState {
        // The latest validation result for this network. This is a bitmask of
        // INetworkMonitor.NETWORK_VALIDATION_RESULT_* constants.
        private int mEvaluationResult = NETWORK_VALIDATION_RESULT_INVALID;
        // Indicates which probes have succeeded since clearProbeResults was called.
        // This is a bitmask of INetworkMonitor.NETWORK_VALIDATION_PROBE_* constants.
        private int mProbeResults = 0;
        // A bitmask to record which probes are completed.
        private int mProbeCompleted = 0;
        // The latest redirect URL.
        private String mRedirectUrl;

        protected void clearProbeResults() {
            mProbeResults = 0;
            mProbeCompleted = 0;
        }

        private void maybeNotifyProbeResults(@NonNull final Runnable modif) {
            final int oldCompleted = mProbeCompleted;
            final int oldResults = mProbeResults;
            modif.run();
            if (oldCompleted != mProbeCompleted || oldResults != mProbeResults) {
                notifyProbeStatusChanged(mProbeCompleted, mProbeResults);
            }
        }

        protected void removeProbeResult(final int probeResult) {
            maybeNotifyProbeResults(() -> {
                mProbeCompleted &= ~probeResult;
                mProbeResults &= ~probeResult;
            });
        }

        protected void noteProbeResult(final int probeResult, final boolean succeeded) {
            maybeNotifyProbeResults(() -> {
                mProbeCompleted |= probeResult;
                if (succeeded) {
                    mProbeResults |= probeResult;
                } else {
                    mProbeResults &= ~probeResult;
                }
            });
        }

        protected void reportEvaluationResult(int result, @Nullable String redirectUrl) {
            mEvaluationResult = result;
            mRedirectUrl = redirectUrl;
            final NetworkTestResultParcelable p = new NetworkTestResultParcelable();
            p.result = result;
            p.probesSucceeded = mProbeResults;
            p.probesAttempted = mProbeCompleted;
            p.redirectUrl = redirectUrl;
            p.timestampMillis = SystemClock.elapsedRealtime();
            notifyNetworkTested(p);
            recordValidationResult(result, redirectUrl);
        }

        @VisibleForTesting
        protected int getEvaluationResult() {
            return mEvaluationResult;
        }

        @VisibleForTesting
        protected int getProbeResults() {
            return mProbeResults;
        }

        @VisibleForTesting
        protected int getProbeCompletedResult() {
            return mProbeCompleted;
        }
    }

    @VisibleForTesting
    protected EvaluationState getEvaluationState() {
        return mEvaluationState;
    }

    private void maybeDisableHttpsProbing(boolean acceptPartial) {
        mAcceptPartialConnectivity = acceptPartial;
        // Ignore https probe in next validation if user accept partial connectivity on a partial
        // connectivity network.
        if (((mEvaluationState.getEvaluationResult() & NETWORK_VALIDATION_RESULT_PARTIAL) != 0)
                && mAcceptPartialConnectivity) {
            mUseHttps = false;
        }
    }

    // Report HTTP, HTTP or FALLBACK probe result.
    @VisibleForTesting
    protected void reportHttpProbeResult(int probeResult,
                @NonNull final CaptivePortalProbeResult result) {
        boolean succeeded = result.isSuccessful();
        // The success of a HTTP probe does not tell us whether the DNS probe succeeded.
        // The DNS and HTTP probes run one after the other in sendDnsAndHttpProbes, and that
        // method cannot report the result of the DNS probe because that it could be running
        // on a different thread which is racing with the main state machine thread. So, if
        // an HTTP or HTTPS probe succeeded, assume that the DNS probe succeeded. But if an
        // HTTP or HTTPS probe failed, don't assume that DNS is not working.
        // TODO: fix this.
        if (succeeded) {
            probeResult |= NETWORK_VALIDATION_PROBE_DNS;
        }
        mEvaluationState.noteProbeResult(probeResult, succeeded);
    }

    private void maybeReportCaptivePortalData(@Nullable CaptivePortalDataShim data) {
        // Do not clear data even if it is null: access points should not stop serving the API, so
        // if the API disappears this is treated as a temporary failure, and previous data should
        // remain valid.
        if (data == null) return;
        try {
            data.notifyChanged(mCallback);
        } catch (RemoteException e) {
            Log.e(TAG, "Error notifying ConnectivityService of new capport data", e);
        }
    }

    /**
     * Interface for logging dns results.
     */
    public interface DnsLogFunc {
        /**
         * Log function.
         */
        void log(String s);
    }

    @Nullable
    private static TcpSocketTracker getTcpSocketTrackerOrNull(Context context, Network network) {
        return ((Dependencies.DEFAULT.getDeviceConfigPropertyInt(
                NAMESPACE_CONNECTIVITY,
                CONFIG_DATA_STALL_EVALUATION_TYPE,
                DEFAULT_DATA_STALL_EVALUATION_TYPES)
                & DATA_STALL_EVALUATION_TYPE_TCP) != 0)
                    ? new TcpSocketTracker(new TcpSocketTracker.Dependencies(context), network)
                    : null;
    }

    @Nullable
    private DnsStallDetector initDnsStallDetectorIfRequired(int type, int threshold) {
        return ((type & DATA_STALL_EVALUATION_TYPE_DNS) != 0)
                ? new DnsStallDetector(threshold) : null;
    }

    private static Uri getCaptivePortalApiUrl(LinkProperties lp) {
        return NetworkInformationShimImpl.newInstance().getCaptivePortalApiUrl(lp);
    }
}
