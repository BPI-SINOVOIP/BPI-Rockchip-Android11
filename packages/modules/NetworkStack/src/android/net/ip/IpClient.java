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

package android.net.ip;

import static android.net.RouteInfo.RTN_UNICAST;
import static android.net.dhcp.DhcpResultsParcelableUtil.toStableParcelable;
import static android.provider.DeviceConfig.NAMESPACE_CONNECTIVITY;

import static com.android.server.util.NetworkStackConstants.VENDOR_SPECIFIC_IE_ID;
import static com.android.server.util.PermissionUtil.enforceNetworkStackCallingPermission;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.DhcpResults;
import android.net.INetd;
import android.net.IpPrefix;
import android.net.Layer2InformationParcelable;
import android.net.Layer2PacketParcelable;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.MacAddress;
import android.net.NattKeepalivePacketDataParcelable;
import android.net.NetworkStackIpMemoryStore;
import android.net.ProvisioningConfigurationParcelable;
import android.net.ProxyInfo;
import android.net.RouteInfo;
import android.net.TcpKeepalivePacketDataParcelable;
import android.net.Uri;
import android.net.apf.ApfCapabilities;
import android.net.apf.ApfFilter;
import android.net.dhcp.DhcpClient;
import android.net.dhcp.DhcpPacket;
import android.net.metrics.IpConnectivityLog;
import android.net.metrics.IpManagerEvent;
import android.net.shared.InitialConfiguration;
import android.net.shared.ProvisioningConfiguration;
import android.net.shared.ProvisioningConfiguration.ScanResultInfo;
import android.net.shared.ProvisioningConfiguration.ScanResultInfo.InformationElement;
import android.net.util.InterfaceParams;
import android.net.util.NetworkStackUtils;
import android.net.util.SharedLog;
import android.os.Build;
import android.os.ConditionVariable;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.os.ServiceSpecificException;
import android.os.SystemClock;
import android.stats.connectivity.DisconnectCode;
import android.text.TextUtils;
import android.util.LocalLog;
import android.util.Log;
import android.util.Pair;
import android.util.SparseArray;

import androidx.annotation.NonNull;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.HexDump;
import com.android.internal.util.IState;
import com.android.internal.util.IndentingPrintWriter;
import com.android.internal.util.MessageUtils;
import com.android.internal.util.State;
import com.android.internal.util.StateMachine;
import com.android.internal.util.WakeupMessage;
import com.android.networkstack.apishim.NetworkInformationShimImpl;
import com.android.networkstack.apishim.common.NetworkInformationShim;
import com.android.networkstack.apishim.common.ShimUtils;
import com.android.networkstack.metrics.IpProvisioningMetrics;
import com.android.server.NetworkObserverRegistry;
import com.android.server.NetworkStackService.NetworkStackServiceManager;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.function.Predicate;
import java.util.stream.Collectors;

/**
 * IpClient
 *
 * This class provides the interface to IP-layer provisioning and maintenance
 * functionality that can be used by transport layers like Wi-Fi, Ethernet,
 * et cetera.
 *
 * [ Lifetime ]
 * IpClient is designed to be instantiated as soon as the interface name is
 * known and can be as long-lived as the class containing it (i.e. declaring
 * it "private final" is okay).
 *
 * @hide
 */
public class IpClient extends StateMachine {
    private static final boolean DBG = false;

    // For message logging.
    private static final Class[] sMessageClasses = { IpClient.class, DhcpClient.class };
    private static final SparseArray<String> sWhatToString =
            MessageUtils.findMessageNames(sMessageClasses);
    // Two static concurrent hashmaps of interface name to logging classes.
    // One holds StateMachine logs and the other connectivity packet logs.
    private static final ConcurrentHashMap<String, SharedLog> sSmLogs = new ConcurrentHashMap<>();
    private static final ConcurrentHashMap<String, LocalLog> sPktLogs = new ConcurrentHashMap<>();
    private final NetworkStackIpMemoryStore mIpMemoryStore;
    private final NetworkInformationShim mShim = NetworkInformationShimImpl.newInstance();
    private final IpProvisioningMetrics mIpProvisioningMetrics = new IpProvisioningMetrics();

    /**
     * Dump all state machine and connectivity packet logs to the specified writer.
     * @param skippedIfaces Interfaces for which logs should not be dumped.
     */
    public static void dumpAllLogs(PrintWriter writer, Set<String> skippedIfaces) {
        for (String ifname : sSmLogs.keySet()) {
            if (skippedIfaces.contains(ifname)) continue;

            writer.println(String.format("--- BEGIN %s ---", ifname));

            final SharedLog smLog = sSmLogs.get(ifname);
            if (smLog != null) {
                writer.println("State machine log:");
                smLog.dump(null, writer, null);
            }

            writer.println("");

            final LocalLog pktLog = sPktLogs.get(ifname);
            if (pktLog != null) {
                writer.println("Connectivity packet log:");
                pktLog.readOnlyLocalLog().dump(null, writer, null);
            }

            writer.println(String.format("--- END %s ---", ifname));
        }
    }

    // Use a wrapper class to log in order to ensure complete and detailed
    // logging. This method is lighter weight than annotations/reflection
    // and has the following benefits:
    //
    //     - No invoked method can be forgotten.
    //       Any new method added to IpClient.Callback must be overridden
    //       here or it will never be called.
    //
    //     - No invoking call site can be forgotten.
    //       Centralized logging in this way means call sites don't need to
    //       remember to log, and therefore no call site can be forgotten.
    //
    //     - No variation in log format among call sites.
    //       Encourages logging of any available arguments, and all call sites
    //       are necessarily logged identically.
    //
    // NOTE: Log first because passed objects may or may not be thread-safe and
    // once passed on to the callback they may be modified by another thread.
    //
    // TODO: Find an lighter weight approach.
    public static class IpClientCallbacksWrapper {
        private static final String PREFIX = "INVOKE ";
        private final IIpClientCallbacks mCallback;
        private final SharedLog mLog;
        @NonNull
        private final NetworkInformationShim mShim;

        @VisibleForTesting
        protected IpClientCallbacksWrapper(IIpClientCallbacks callback, SharedLog log,
                @NonNull NetworkInformationShim shim) {
            mCallback = callback;
            mLog = log;
            mShim = shim;
        }

        private void log(String msg) {
            mLog.log(PREFIX + msg);
        }

        private void log(String msg, Throwable e) {
            mLog.e(PREFIX + msg, e);
        }

        /**
         * Callback called prior to DHCP discovery/renewal only if the pre DHCP action
         * is enabled.
         */
        public void onPreDhcpAction() {
            log("onPreDhcpAction()");
            try {
                mCallback.onPreDhcpAction();
            } catch (RemoteException e) {
                log("Failed to call onPreDhcpAction", e);
            }
        }

        /**
         * Callback called after DHCP discovery/renewal only if the pre DHCP action
         * is enabled.
         */
        public void onPostDhcpAction() {
            log("onPostDhcpAction()");
            try {
                mCallback.onPostDhcpAction();
            } catch (RemoteException e) {
                log("Failed to call onPostDhcpAction", e);
            }
        }

        /**
         * Callback called when new DHCP results are available.
         */
        public void onNewDhcpResults(DhcpResults dhcpResults) {
            log("onNewDhcpResults({" + dhcpResults + "})");
            try {
                mCallback.onNewDhcpResults(toStableParcelable(dhcpResults));
            } catch (RemoteException e) {
                log("Failed to call onNewDhcpResults", e);
            }
        }

        /**
         * Indicates that provisioning was successful.
         */
        public void onProvisioningSuccess(LinkProperties newLp) {
            log("onProvisioningSuccess({" + newLp + "})");
            try {
                mCallback.onProvisioningSuccess(mShim.makeSensitiveFieldsParcelingCopy(newLp));
            } catch (RemoteException e) {
                log("Failed to call onProvisioningSuccess", e);
            }
        }

        /**
         * Indicates that provisioning failed.
         */
        public void onProvisioningFailure(LinkProperties newLp) {
            log("onProvisioningFailure({" + newLp + "})");
            try {
                mCallback.onProvisioningFailure(mShim.makeSensitiveFieldsParcelingCopy(newLp));
            } catch (RemoteException e) {
                log("Failed to call onProvisioningFailure", e);
            }
        }

        /**
         * Invoked on LinkProperties changes.
         */
        public void onLinkPropertiesChange(LinkProperties newLp) {
            log("onLinkPropertiesChange({" + newLp + "})");
            try {
                mCallback.onLinkPropertiesChange(mShim.makeSensitiveFieldsParcelingCopy(newLp));
            } catch (RemoteException e) {
                log("Failed to call onLinkPropertiesChange", e);
            }
        }

        /**
         * Called when the internal IpReachabilityMonitor (if enabled) has detected the loss of
         * required neighbors (e.g. on-link default gw or dns servers) due to NUD_FAILED.
         */
        public void onReachabilityLost(String logMsg) {
            log("onReachabilityLost(" + logMsg + ")");
            try {
                mCallback.onReachabilityLost(logMsg);
            } catch (RemoteException e) {
                log("Failed to call onReachabilityLost", e);
            }
        }

        /**
         * Called when the IpClient state machine terminates.
         */
        public void onQuit() {
            log("onQuit()");
            try {
                mCallback.onQuit();
            } catch (RemoteException e) {
                log("Failed to call onQuit", e);
            }
        }

        /**
         * Called to indicate that a new APF program must be installed to filter incoming packets.
         */
        public void installPacketFilter(byte[] filter) {
            log("installPacketFilter(byte[" + filter.length + "])");
            try {
                mCallback.installPacketFilter(filter);
            } catch (RemoteException e) {
                log("Failed to call installPacketFilter", e);
            }
        }

        /**
         * Called to indicate that the APF Program & data buffer must be read asynchronously from
         * the wifi driver.
         */
        public void startReadPacketFilter() {
            log("startReadPacketFilter()");
            try {
                mCallback.startReadPacketFilter();
            } catch (RemoteException e) {
                log("Failed to call startReadPacketFilter", e);
            }
        }

        /**
         * If multicast filtering cannot be accomplished with APF, this function will be called to
         * actuate multicast filtering using another means.
         */
        public void setFallbackMulticastFilter(boolean enabled) {
            log("setFallbackMulticastFilter(" + enabled + ")");
            try {
                mCallback.setFallbackMulticastFilter(enabled);
            } catch (RemoteException e) {
                log("Failed to call setFallbackMulticastFilter", e);
            }
        }

        /**
         * Enabled/disable Neighbor Discover offload functionality. This is called, for example,
         * whenever 464xlat is being started or stopped.
         */
        public void setNeighborDiscoveryOffload(boolean enable) {
            log("setNeighborDiscoveryOffload(" + enable + ")");
            try {
                mCallback.setNeighborDiscoveryOffload(enable);
            } catch (RemoteException e) {
                log("Failed to call setNeighborDiscoveryOffload", e);
            }
        }

        /**
         * Invoked on starting preconnection process.
         */
        public void onPreconnectionStart(List<Layer2PacketParcelable> packets) {
            log("onPreconnectionStart(Layer2Packets[" + packets.size() + "])");
            try {
                mCallback.onPreconnectionStart(packets);
            } catch (RemoteException e) {
                log("Failed to call onPreconnectionStart", e);
            }
        }
    }

    public static final String DUMP_ARG_CONFIRM = "confirm";

    // Below constants are picked up by MessageUtils and exempt from ProGuard optimization.
    private static final int CMD_TERMINATE_AFTER_STOP             = 1;
    private static final int CMD_STOP                             = 2;
    private static final int CMD_START                            = 3;
    private static final int CMD_CONFIRM                          = 4;
    private static final int EVENT_PRE_DHCP_ACTION_COMPLETE       = 5;
    // Triggered by NetlinkTracker to communicate netlink events.
    private static final int EVENT_NETLINK_LINKPROPERTIES_CHANGED = 6;
    private static final int CMD_UPDATE_TCP_BUFFER_SIZES          = 7;
    private static final int CMD_UPDATE_HTTP_PROXY                = 8;
    private static final int CMD_SET_MULTICAST_FILTER             = 9;
    private static final int EVENT_PROVISIONING_TIMEOUT           = 10;
    private static final int EVENT_DHCPACTION_TIMEOUT             = 11;
    private static final int EVENT_READ_PACKET_FILTER_COMPLETE    = 12;
    private static final int CMD_ADD_KEEPALIVE_PACKET_FILTER_TO_APF = 13;
    private static final int CMD_REMOVE_KEEPALIVE_PACKET_FILTER_FROM_APF = 14;
    private static final int CMD_UPDATE_L2KEY_CLUSTER = 15;
    private static final int CMD_COMPLETE_PRECONNECTION = 16;
    private static final int CMD_UPDATE_L2INFORMATION = 17;

    private static final int ARG_LINKPROP_CHANGED_LINKSTATE_DOWN = 0;
    private static final int ARG_LINKPROP_CHANGED_LINKSTATE_UP = 1;

    // Internal commands to use instead of trying to call transitionTo() inside
    // a given State's enter() method. Calling transitionTo() from enter/exit
    // encounters a Log.wtf() that can cause trouble on eng builds.
    private static final int CMD_ADDRESSES_CLEARED                = 100;
    private static final int CMD_JUMP_RUNNING_TO_STOPPING         = 101;
    private static final int CMD_JUMP_STOPPING_TO_STOPPED         = 102;

    // IpClient shares a handler with DhcpClient: commands must not overlap
    public static final int DHCPCLIENT_CMD_BASE = 1000;

    // Settings and default values.
    private static final int MAX_LOG_RECORDS = 500;
    private static final int MAX_PACKET_RECORDS = 100;

    @VisibleForTesting
    static final String CONFIG_MIN_RDNSS_LIFETIME = "ipclient_min_rdnss_lifetime";
    private static final int DEFAULT_MIN_RDNSS_LIFETIME =
            ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q) ? 120 : 0;

    private static final boolean NO_CALLBACKS = false;
    private static final boolean SEND_CALLBACKS = true;

    // This must match the interface prefix in clatd.c.
    // TODO: Revert this hack once IpClient and Nat464Xlat work in concert.
    private static final String CLAT_PREFIX = "v4-";

    private static final int IMMEDIATE_FAILURE_DURATION = 0;

    private static final int PROV_CHANGE_STILL_NOT_PROVISIONED = 1;
    private static final int PROV_CHANGE_LOST_PROVISIONING = 2;
    private static final int PROV_CHANGE_GAINED_PROVISIONING = 3;
    private static final int PROV_CHANGE_STILL_PROVISIONED = 4;

    // Specific vendor OUI(3 bytes)/vendor specific type(1 byte) pattern for upstream hotspot
    // device detection. Add new byte array pattern below in turn.
    private static final List<byte[]> METERED_IE_PATTERN_LIST = Collections.unmodifiableList(
            Arrays.asList(
                    new byte[] { (byte) 0x00, (byte) 0x17, (byte) 0xf2, (byte) 0x06 }
    ));

    // Initialize configurable particular SSID set supporting DHCP Roaming feature. See
    // b/131797393 for more details.
    private static final Set<String> DHCP_ROAMING_SSID_SET = new HashSet<>(
            Arrays.asList(
                    "0001docomo", "ollehWiFi", "olleh GiGa WiFi", "KT WiFi",
                    "KT GiGA WiFi", "marente"
    ));

    private final State mStoppedState = new StoppedState();
    private final State mStoppingState = new StoppingState();
    private final State mClearingIpAddressesState = new ClearingIpAddressesState();
    private final State mStartedState = new StartedState();
    private final State mRunningState = new RunningState();
    private final State mPreconnectingState = new PreconnectingState();

    private final String mTag;
    private final Context mContext;
    private final String mInterfaceName;
    private final String mClatInterfaceName;
    @VisibleForTesting
    protected final IpClientCallbacksWrapper mCallback;
    private final Dependencies mDependencies;
    private final CountDownLatch mShutdownLatch;
    private final ConnectivityManager mCm;
    private final INetd mNetd;
    private final NetworkObserverRegistry mObserverRegistry;
    private final IpClientLinkObserver mLinkObserver;
    private final WakeupMessage mProvisioningTimeoutAlarm;
    private final WakeupMessage mDhcpActionTimeoutAlarm;
    private final SharedLog mLog;
    private final LocalLog mConnectivityPacketLog;
    private final MessageHandlingLogger mMsgStateLogger;
    private final IpConnectivityLog mMetricsLog;
    private final InterfaceController mInterfaceCtrl;

    // Ignore nonzero RDNSS option lifetimes below this value. 0 = disabled.
    private final int mMinRdnssLifetimeSec;

    private InterfaceParams mInterfaceParams;

    /**
     * Non-final member variables accessed only from within our StateMachine.
     */
    private LinkProperties mLinkProperties;
    private android.net.shared.ProvisioningConfiguration mConfiguration;
    private IpReachabilityMonitor mIpReachabilityMonitor;
    private DhcpClient mDhcpClient;
    private DhcpResults mDhcpResults;
    private String mTcpBufferSizes;
    private ProxyInfo mHttpProxy;
    private ApfFilter mApfFilter;
    private String mL2Key; // The L2 key for this network, for writing into the memory store
    private String mCluster; // The cluster for this network, for writing into the memory store
    private boolean mMulticastFiltering;
    private long mStartTimeMillis;
    private MacAddress mCurrentBssid;
    private boolean mHasDisabledIPv6OnProvLoss;

    /**
     * Reading the snapshot is an asynchronous operation initiated by invoking
     * Callback.startReadPacketFilter() and completed when the WiFi Service responds with an
     * EVENT_READ_PACKET_FILTER_COMPLETE message. The mApfDataSnapshotComplete condition variable
     * signals when a new snapshot is ready.
     */
    private final ConditionVariable mApfDataSnapshotComplete = new ConditionVariable();

    public static class Dependencies {
        /**
         * Get interface parameters for the specified interface.
         */
        public InterfaceParams getInterfaceParams(String ifname) {
            return InterfaceParams.getByName(ifname);
        }

        /**
         * Get a INetd connector.
         */
        public INetd getNetd(Context context) {
            return INetd.Stub.asInterface((IBinder) context.getSystemService(Context.NETD_SERVICE));
        }

        /**
         * Get a IpMemoryStore instance.
         */
        public NetworkStackIpMemoryStore getIpMemoryStore(Context context,
                NetworkStackServiceManager nssManager) {
            return new NetworkStackIpMemoryStore(context, nssManager.getIpMemoryStoreService());
        }

        /**
         * Get a DhcpClient instance.
         */
        public DhcpClient makeDhcpClient(Context context, StateMachine controller,
                InterfaceParams ifParams, DhcpClient.Dependencies deps) {
            return DhcpClient.makeDhcpClient(context, controller, ifParams, deps);
        }

        /**
         * Get a DhcpClient Dependencies instance.
         */
        public DhcpClient.Dependencies getDhcpClientDependencies(
                NetworkStackIpMemoryStore ipMemoryStore, IpProvisioningMetrics metrics) {
            return new DhcpClient.Dependencies(ipMemoryStore, metrics);
        }

        /**
         * Read an integer DeviceConfig property.
         */
        public int getDeviceConfigPropertyInt(String name, int defaultValue) {
            return NetworkStackUtils.getDeviceConfigPropertyInt(NAMESPACE_CONNECTIVITY, name,
                    defaultValue);
        }

        /**
         * Get a IpConnectivityLog instance.
         */
        public IpConnectivityLog getIpConnectivityLog() {
            return new IpConnectivityLog();
        }
    }

    public IpClient(Context context, String ifName, IIpClientCallbacks callback,
            NetworkObserverRegistry observerRegistry, NetworkStackServiceManager nssManager) {
        this(context, ifName, callback, observerRegistry, nssManager, new Dependencies());
    }

    @VisibleForTesting
    IpClient(Context context, String ifName, IIpClientCallbacks callback,
            NetworkObserverRegistry observerRegistry, NetworkStackServiceManager nssManager,
            Dependencies deps) {
        super(IpClient.class.getSimpleName() + "." + ifName);
        Objects.requireNonNull(ifName);
        Objects.requireNonNull(callback);

        mTag = getName();

        mContext = context;
        mInterfaceName = ifName;
        mClatInterfaceName = CLAT_PREFIX + ifName;
        mDependencies = deps;
        mMetricsLog = deps.getIpConnectivityLog();
        mShutdownLatch = new CountDownLatch(1);
        mCm = mContext.getSystemService(ConnectivityManager.class);
        mObserverRegistry = observerRegistry;
        mIpMemoryStore = deps.getIpMemoryStore(context, nssManager);

        sSmLogs.putIfAbsent(mInterfaceName, new SharedLog(MAX_LOG_RECORDS, mTag));
        mLog = sSmLogs.get(mInterfaceName);
        sPktLogs.putIfAbsent(mInterfaceName, new LocalLog(MAX_PACKET_RECORDS));
        mConnectivityPacketLog = sPktLogs.get(mInterfaceName);
        mMsgStateLogger = new MessageHandlingLogger();
        mCallback = new IpClientCallbacksWrapper(callback, mLog, mShim);

        // TODO: Consider creating, constructing, and passing in some kind of
        // InterfaceController.Dependencies class.
        mNetd = deps.getNetd(mContext);
        mInterfaceCtrl = new InterfaceController(mInterfaceName, mNetd, mLog);

        mMinRdnssLifetimeSec = mDependencies.getDeviceConfigPropertyInt(
                CONFIG_MIN_RDNSS_LIFETIME, DEFAULT_MIN_RDNSS_LIFETIME);

        IpClientLinkObserver.Configuration config = new IpClientLinkObserver.Configuration(
                mMinRdnssLifetimeSec);

        mLinkObserver = new IpClientLinkObserver(
                mContext, getHandler(),
                mInterfaceName,
                (ifaceUp) -> sendMessage(EVENT_NETLINK_LINKPROPERTIES_CHANGED, ifaceUp
                        ? ARG_LINKPROP_CHANGED_LINKSTATE_UP
                        : ARG_LINKPROP_CHANGED_LINKSTATE_DOWN),
                config, mLog) {
            @Override
            public void onInterfaceAdded(String iface) {
                super.onInterfaceAdded(iface);
                if (mClatInterfaceName.equals(iface)) {
                    mCallback.setNeighborDiscoveryOffload(false);
                } else if (!mInterfaceName.equals(iface)) {
                    return;
                }

                final String msg = "interfaceAdded(" + iface + ")";
                logMsg(msg);
            }

            @Override
            public void onInterfaceRemoved(String iface) {
                super.onInterfaceRemoved(iface);
                // TODO: Also observe mInterfaceName going down and take some
                // kind of appropriate action.
                if (mClatInterfaceName.equals(iface)) {
                    // TODO: consider sending a message to the IpClient main
                    // StateMachine thread, in case "NDO enabled" state becomes
                    // tied to more things that 464xlat operation.
                    mCallback.setNeighborDiscoveryOffload(true);
                } else if (!mInterfaceName.equals(iface)) {
                    return;
                }

                final String msg = "interfaceRemoved(" + iface + ")";
                logMsg(msg);
            }

            private void logMsg(String msg) {
                Log.d(mTag, msg);
                getHandler().post(() -> mLog.log("OBSERVED " + msg));
            }
        };

        mLinkProperties = new LinkProperties();
        mLinkProperties.setInterfaceName(mInterfaceName);

        mProvisioningTimeoutAlarm = new WakeupMessage(mContext, getHandler(),
                mTag + ".EVENT_PROVISIONING_TIMEOUT", EVENT_PROVISIONING_TIMEOUT);
        mDhcpActionTimeoutAlarm = new WakeupMessage(mContext, getHandler(),
                mTag + ".EVENT_DHCPACTION_TIMEOUT", EVENT_DHCPACTION_TIMEOUT);

        // Anything the StateMachine may access must have been instantiated
        // before this point.
        configureAndStartStateMachine();

        // Anything that may send messages to the StateMachine must only be
        // configured to do so after the StateMachine has started (above).
        startStateMachineUpdaters();
    }

    /**
     * Make a IIpClient connector to communicate with this IpClient.
     */
    public IIpClient makeConnector() {
        return new IpClientConnector();
    }

    class IpClientConnector extends IIpClient.Stub {
        @Override
        public void completedPreDhcpAction() {
            enforceNetworkStackCallingPermission();
            IpClient.this.completedPreDhcpAction();
        }
        @Override
        public void confirmConfiguration() {
            enforceNetworkStackCallingPermission();
            IpClient.this.confirmConfiguration();
        }
        @Override
        public void readPacketFilterComplete(byte[] data) {
            enforceNetworkStackCallingPermission();
            IpClient.this.readPacketFilterComplete(data);
        }
        @Override
        public void shutdown() {
            enforceNetworkStackCallingPermission();
            IpClient.this.shutdown();
        }
        @Override
        public void startProvisioning(ProvisioningConfigurationParcelable req) {
            enforceNetworkStackCallingPermission();
            IpClient.this.startProvisioning(ProvisioningConfiguration.fromStableParcelable(req));
        }
        @Override
        public void stop() {
            enforceNetworkStackCallingPermission();
            IpClient.this.stop();
        }
        @Override
        public void setL2KeyAndGroupHint(String l2Key, String cluster) {
            enforceNetworkStackCallingPermission();
            IpClient.this.setL2KeyAndCluster(l2Key, cluster);
        }
        @Override
        public void setTcpBufferSizes(String tcpBufferSizes) {
            enforceNetworkStackCallingPermission();
            IpClient.this.setTcpBufferSizes(tcpBufferSizes);
        }
        @Override
        public void setHttpProxy(ProxyInfo proxyInfo) {
            enforceNetworkStackCallingPermission();
            IpClient.this.setHttpProxy(proxyInfo);
        }
        @Override
        public void setMulticastFilter(boolean enabled) {
            enforceNetworkStackCallingPermission();
            IpClient.this.setMulticastFilter(enabled);
        }
        @Override
        public void addKeepalivePacketFilter(int slot, TcpKeepalivePacketDataParcelable pkt) {
            enforceNetworkStackCallingPermission();
            IpClient.this.addKeepalivePacketFilter(slot, pkt);
        }
        @Override
        public void addNattKeepalivePacketFilter(int slot, NattKeepalivePacketDataParcelable pkt) {
            enforceNetworkStackCallingPermission();
            IpClient.this.addNattKeepalivePacketFilter(slot, pkt);
        }
        @Override
        public void removeKeepalivePacketFilter(int slot) {
            enforceNetworkStackCallingPermission();
            IpClient.this.removeKeepalivePacketFilter(slot);
        }
        @Override
        public void notifyPreconnectionComplete(boolean success) {
            enforceNetworkStackCallingPermission();
            IpClient.this.notifyPreconnectionComplete(success);
        }
        @Override
        public void updateLayer2Information(Layer2InformationParcelable info) {
            enforceNetworkStackCallingPermission();
            IpClient.this.updateLayer2Information(info);
        }

        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    }

    public String getInterfaceName() {
        return mInterfaceName;
    }

    private void configureAndStartStateMachine() {
        // CHECKSTYLE:OFF IndentationCheck
        addState(mStoppedState);
        addState(mStartedState);
            addState(mPreconnectingState, mStartedState);
            addState(mClearingIpAddressesState, mStartedState);
            addState(mRunningState, mStartedState);
        addState(mStoppingState);
        // CHECKSTYLE:ON IndentationCheck

        setInitialState(mStoppedState);

        super.start();
    }

    private void startStateMachineUpdaters() {
        mObserverRegistry.registerObserverForNonblockingCallback(mLinkObserver);
    }

    private void stopStateMachineUpdaters() {
        mObserverRegistry.unregisterObserver(mLinkObserver);
        mLinkObserver.shutdown();
    }

    @Override
    protected void onQuitting() {
        mCallback.onQuit();
        mShutdownLatch.countDown();
    }

    /**
     * Shut down this IpClient instance altogether.
     */
    public void shutdown() {
        stop();
        sendMessage(CMD_TERMINATE_AFTER_STOP);
    }

    /**
     * Start provisioning with the provided parameters.
     */
    public void startProvisioning(ProvisioningConfiguration req) {
        if (!req.isValid()) {
            doImmediateProvisioningFailure(IpManagerEvent.ERROR_INVALID_PROVISIONING);
            return;
        }

        final ScanResultInfo scanResultInfo = req.mScanResultInfo;
        mCurrentBssid = null;
        if (scanResultInfo != null) {
            try {
                mCurrentBssid = MacAddress.fromString(scanResultInfo.getBssid());
            } catch (IllegalArgumentException e) {
                Log.wtf(mTag, "Invalid BSSID: " + scanResultInfo.getBssid()
                        + " in provisioning configuration", e);
            }
        }

        if (req.mLayer2Info != null) {
            mL2Key = req.mLayer2Info.mL2Key;
            mCluster = req.mLayer2Info.mCluster;
        }
        sendMessage(CMD_START, new android.net.shared.ProvisioningConfiguration(req));
    }

    /**
     * Stop this IpClient.
     *
     * <p>This does not shut down the StateMachine itself, which is handled by {@link #shutdown()}.
     *    The message "arg1" parameter is used to record the disconnect code metrics.
     *    Usually this method is called by the peer (e.g. wifi) intentionally to stop IpClient,
     *    consider that's the normal user termination.
     */
    public void stop() {
        sendMessage(CMD_STOP, DisconnectCode.DC_NORMAL_TERMINATION.getNumber());
    }

    /**
     * Confirm the provisioning configuration.
     */
    public void confirmConfiguration() {
        sendMessage(CMD_CONFIRM);
    }

    /**
     * For clients using {@link ProvisioningConfiguration.Builder#withPreDhcpAction()}, must be
     * called after {@link IIpClientCallbacks#onPreDhcpAction} to indicate that DHCP is clear to
     * proceed.
     */
    public void completedPreDhcpAction() {
        sendMessage(EVENT_PRE_DHCP_ACTION_COMPLETE);
    }

    /**
     * Indicate that packet filter read is complete.
     */
    public void readPacketFilterComplete(byte[] data) {
        sendMessage(EVENT_READ_PACKET_FILTER_COMPLETE, data);
    }

    /**
     * Set the TCP buffer sizes to use.
     *
     * This may be called, repeatedly, at any time before or after a call to
     * #startProvisioning(). The setting is cleared upon calling #stop().
     */
    public void setTcpBufferSizes(String tcpBufferSizes) {
        sendMessage(CMD_UPDATE_TCP_BUFFER_SIZES, tcpBufferSizes);
    }

    /**
     * Set the L2 key and cluster for storing info into the memory store.
     *
     * This method is only supported on Q devices. For R or above releases,
     * caller should call #updateLayer2Information() instead.
     */
    public void setL2KeyAndCluster(String l2Key, String cluster) {
        if (!ShimUtils.isReleaseOrDevelopmentApiAbove(Build.VERSION_CODES.Q)) {
            sendMessage(CMD_UPDATE_L2KEY_CLUSTER, new Pair<>(l2Key, cluster));
        }
    }

    /**
     * Set the HTTP Proxy configuration to use.
     *
     * This may be called, repeatedly, at any time before or after a call to
     * #startProvisioning(). The setting is cleared upon calling #stop().
     */
    public void setHttpProxy(ProxyInfo proxyInfo) {
        sendMessage(CMD_UPDATE_HTTP_PROXY, proxyInfo);
    }

    /**
     * Enable or disable the multicast filter.  Attempts to use APF to accomplish the filtering,
     * if not, Callback.setFallbackMulticastFilter() is called.
     */
    public void setMulticastFilter(boolean enabled) {
        sendMessage(CMD_SET_MULTICAST_FILTER, enabled);
    }

    /**
     * Called by WifiStateMachine to add TCP keepalive packet filter before setting up
     * keepalive offload.
     */
    public void addKeepalivePacketFilter(int slot, @NonNull TcpKeepalivePacketDataParcelable pkt) {
        sendMessage(CMD_ADD_KEEPALIVE_PACKET_FILTER_TO_APF, slot, 0 /* Unused */, pkt);
    }

    /**
     *  Called by WifiStateMachine to add NATT keepalive packet filter before setting up
     *  keepalive offload.
     */
    public void addNattKeepalivePacketFilter(int slot,
            @NonNull NattKeepalivePacketDataParcelable pkt) {
        sendMessage(CMD_ADD_KEEPALIVE_PACKET_FILTER_TO_APF, slot, 0 /* Unused */ , pkt);
    }

    /**
     * Called by WifiStateMachine to remove keepalive packet filter after stopping keepalive
     * offload.
     */
    public void removeKeepalivePacketFilter(int slot) {
        sendMessage(CMD_REMOVE_KEEPALIVE_PACKET_FILTER_FROM_APF, slot, 0 /* Unused */);
    }

    /**
     * Notify IpClient that preconnection is complete and that the link is ready for use.
     * The success parameter indicates whether the packets passed in by onPreconnectionStart were
     * successfully sent to the network or not.
     */
    public void notifyPreconnectionComplete(boolean success) {
        sendMessage(CMD_COMPLETE_PRECONNECTION, success ? 1 : 0);
    }

    /**
     * Update the network bssid, L2Key and cluster on L2 roaming happened.
     */
    public void updateLayer2Information(@NonNull Layer2InformationParcelable info) {
        sendMessage(CMD_UPDATE_L2INFORMATION, info);
    }

    /**
     * Dump logs of this IpClient.
     */
    public void dump(FileDescriptor fd, PrintWriter writer, String[] args) {
        if (args != null && args.length > 0 && DUMP_ARG_CONFIRM.equals(args[0])) {
            // Execute confirmConfiguration() and take no further action.
            confirmConfiguration();
            return;
        }

        // Thread-unsafe access to mApfFilter but just used for debugging.
        final ApfFilter apfFilter = mApfFilter;
        final android.net.shared.ProvisioningConfiguration provisioningConfig = mConfiguration;
        final ApfCapabilities apfCapabilities = (provisioningConfig != null)
                ? provisioningConfig.mApfCapabilities : null;

        IndentingPrintWriter pw = new IndentingPrintWriter(writer, "  ");
        pw.println(mTag + " APF dump:");
        pw.increaseIndent();
        if (apfFilter != null) {
            if (apfCapabilities.hasDataAccess()) {
                // Request a new snapshot, then wait for it.
                mApfDataSnapshotComplete.close();
                mCallback.startReadPacketFilter();
                if (!mApfDataSnapshotComplete.block(1000)) {
                    pw.print("TIMEOUT: DUMPING STALE APF SNAPSHOT");
                }
            }
            apfFilter.dump(pw);

        } else {
            pw.print("No active ApfFilter; ");
            if (provisioningConfig == null) {
                pw.println("IpClient not yet started.");
            } else if (apfCapabilities == null || apfCapabilities.apfVersionSupported == 0) {
                pw.println("Hardware does not support APF.");
            } else {
                pw.println("ApfFilter not yet started, APF capabilities: " + apfCapabilities);
            }
        }
        pw.decreaseIndent();
        pw.println();
        pw.println(mTag + " current ProvisioningConfiguration:");
        pw.increaseIndent();
        pw.println(Objects.toString(provisioningConfig, "N/A"));
        pw.decreaseIndent();

        final IpReachabilityMonitor iprm = mIpReachabilityMonitor;
        if (iprm != null) {
            pw.println();
            pw.println(mTag + " current IpReachabilityMonitor state:");
            pw.increaseIndent();
            iprm.dump(pw);
            pw.decreaseIndent();
        }

        pw.println();
        pw.println(mTag + " StateMachine dump:");
        pw.increaseIndent();
        mLog.dump(fd, pw, args);
        pw.decreaseIndent();

        pw.println();
        pw.println(mTag + " connectivity packet log:");
        pw.println();
        pw.println("Debug with python and scapy via:");
        pw.println("shell$ python");
        pw.println(">>> from scapy import all as scapy");
        pw.println(">>> scapy.Ether(\"<paste_hex_string>\".decode(\"hex\")).show2()");
        pw.println();

        pw.increaseIndent();
        mConnectivityPacketLog.readOnlyLocalLog().dump(fd, pw, args);
        pw.decreaseIndent();
    }


    /**
     * Internals.
     */

    @Override
    protected String getWhatToString(int what) {
        return sWhatToString.get(what, "UNKNOWN: " + Integer.toString(what));
    }

    @Override
    protected String getLogRecString(Message msg) {
        final String logLine = String.format(
                "%s/%d %d %d %s [%s]",
                mInterfaceName, (mInterfaceParams == null) ? -1 : mInterfaceParams.index,
                msg.arg1, msg.arg2, Objects.toString(msg.obj), mMsgStateLogger);

        final String richerLogLine = getWhatToString(msg.what) + " " + logLine;
        mLog.log(richerLogLine);
        if (DBG) {
            Log.d(mTag, richerLogLine);
        }

        mMsgStateLogger.reset();
        return logLine;
    }

    @Override
    protected boolean recordLogRec(Message msg) {
        // Don't log EVENT_NETLINK_LINKPROPERTIES_CHANGED. They can be noisy,
        // and we already log any LinkProperties change that results in an
        // invocation of IpClient.Callback#onLinkPropertiesChange().
        final boolean shouldLog = (msg.what != EVENT_NETLINK_LINKPROPERTIES_CHANGED);
        if (!shouldLog) {
            mMsgStateLogger.reset();
        }
        return shouldLog;
    }

    private void logError(String fmt, Throwable e, Object... args) {
        mLog.e(String.format(fmt, args), e);
    }

    private void logError(String fmt, Object... args) {
        logError(fmt, null, args);
    }

    // This needs to be called with care to ensure that our LinkProperties
    // are in sync with the actual LinkProperties of the interface. For example,
    // we should only call this if we know for sure that there are no IP addresses
    // assigned to the interface, etc.
    private void resetLinkProperties() {
        mLinkObserver.clearLinkProperties();
        mConfiguration = null;
        mDhcpResults = null;
        mTcpBufferSizes = "";
        mHttpProxy = null;

        mLinkProperties = new LinkProperties();
        mLinkProperties.setInterfaceName(mInterfaceName);
    }

    private void recordMetric(final int type) {
        // We may record error metrics prior to starting.
        // Map this to IMMEDIATE_FAILURE_DURATION.
        final long duration = (mStartTimeMillis > 0)
                ? (SystemClock.elapsedRealtime() - mStartTimeMillis)
                : IMMEDIATE_FAILURE_DURATION;
        mMetricsLog.log(mInterfaceName, new IpManagerEvent(type, duration));
    }

    // Record the DisconnectCode and transition to StoppingState.
    private void transitionToStoppingState(final DisconnectCode code) {
        mIpProvisioningMetrics.setDisconnectCode(code);
        transitionTo(mStoppingState);
    }

    // For now: use WifiStateMachine's historical notion of provisioned.
    @VisibleForTesting
    static boolean isProvisioned(LinkProperties lp, InitialConfiguration config) {
        // For historical reasons, we should connect even if all we have is
        // an IPv4 address and nothing else.
        if (lp.hasIpv4Address() || lp.isProvisioned()) {
            return true;
        }
        if (config == null) {
            return false;
        }

        // When an InitialConfiguration is specified, ignore any difference with previous
        // properties and instead check if properties observed match the desired properties.
        return config.isProvisionedBy(lp.getLinkAddresses(), lp.getRoutes());
    }

    // TODO: Investigate folding all this into the existing static function
    // LinkProperties.compareProvisioning() or some other single function that
    // takes two LinkProperties objects and returns a ProvisioningChange
    // object that is a correct and complete assessment of what changed, taking
    // account of the asymmetries described in the comments in this function.
    // Then switch to using it everywhere (IpReachabilityMonitor, etc.).
    private int compareProvisioning(LinkProperties oldLp, LinkProperties newLp) {
        int delta;
        InitialConfiguration config = mConfiguration != null ? mConfiguration.mInitialConfig : null;
        final boolean wasProvisioned = isProvisioned(oldLp, config);
        final boolean isProvisioned = isProvisioned(newLp, config);

        if (!wasProvisioned && isProvisioned) {
            delta = PROV_CHANGE_GAINED_PROVISIONING;
        } else if (wasProvisioned && isProvisioned) {
            delta = PROV_CHANGE_STILL_PROVISIONED;
        } else if (!wasProvisioned && !isProvisioned) {
            delta = PROV_CHANGE_STILL_NOT_PROVISIONED;
        } else {
            // (wasProvisioned && !isProvisioned)
            //
            // Note that this is true even if we lose a configuration element
            // (e.g., a default gateway) that would not be required to advance
            // into provisioned state. This is intended: if we have a default
            // router and we lose it, that's a sure sign of a problem, but if
            // we connect to a network with no IPv4 DNS servers, we consider
            // that to be a network without DNS servers and connect anyway.
            //
            // See the comment below.
            delta = PROV_CHANGE_LOST_PROVISIONING;
        }

        final boolean lostIPv6 = oldLp.isIpv6Provisioned() && !newLp.isIpv6Provisioned();
        final boolean lostIPv4Address = oldLp.hasIpv4Address() && !newLp.hasIpv4Address();
        final boolean lostIPv6Router = oldLp.hasIpv6DefaultRoute() && !newLp.hasIpv6DefaultRoute();

        // If bad wifi avoidance is disabled, then ignore IPv6 loss of
        // provisioning. Otherwise, when a hotspot that loses Internet
        // access sends out a 0-lifetime RA to its clients, the clients
        // will disconnect and then reconnect, avoiding the bad hotspot,
        // instead of getting stuck on the bad hotspot. http://b/31827713 .
        //
        // This is incorrect because if the hotspot then regains Internet
        // access with a different prefix, TCP connections on the
        // deprecated addresses will remain stuck.
        //
        // Note that we can still be disconnected by IpReachabilityMonitor
        // if the IPv6 default gateway (but not the IPv6 DNS servers; see
        // accompanying code in IpReachabilityMonitor) is unreachable.
        final boolean ignoreIPv6ProvisioningLoss = mHasDisabledIPv6OnProvLoss
                || (mConfiguration != null && mConfiguration.mUsingMultinetworkPolicyTracker
                        && !mCm.shouldAvoidBadWifi());

        // Additionally:
        //
        // Partial configurations (e.g., only an IPv4 address with no DNS
        // servers and no default route) are accepted as long as DHCPv4
        // succeeds. On such a network, isProvisioned() will always return
        // false, because the configuration is not complete, but we want to
        // connect anyway. It might be a disconnected network such as a
        // Chromecast or a wireless printer, for example.
        //
        // Because on such a network isProvisioned() will always return false,
        // delta will never be LOST_PROVISIONING. So check for loss of
        // provisioning here too.
        if (lostIPv4Address || (lostIPv6 && !ignoreIPv6ProvisioningLoss)) {
            delta = PROV_CHANGE_LOST_PROVISIONING;
        }

        // Additionally:
        //
        // If the previous link properties had a global IPv6 address and an
        // IPv6 default route then also consider the loss of that default route
        // to be a loss of provisioning. See b/27962810.
        if (oldLp.hasGlobalIpv6Address() && (lostIPv6Router && !ignoreIPv6ProvisioningLoss)) {
            // Although link properties have lost IPv6 default route in this case, if IPv4 is still
            // working with appropriate routes and DNS servers, we can keep the current connection
            // without disconnecting from the network, just disable IPv6 on that given network until
            // to the next provisioning. Disabling IPv6 will result in all IPv6 connectivity torn
            // down and all IPv6 sockets being closed, the non-routable IPv6 DNS servers will be
            // stripped out, so applications will be able to reconnect immediately over IPv4. See
            // b/131781810.
            if (newLp.isIpv4Provisioned()) {
                mInterfaceCtrl.disableIPv6();
                mHasDisabledIPv6OnProvLoss = true;
                delta = PROV_CHANGE_STILL_PROVISIONED;
                if (DBG) {
                    mLog.log("Disable IPv6 stack completely when the default router has gone");
                }
            } else {
                delta = PROV_CHANGE_LOST_PROVISIONING;
            }
        }

        return delta;
    }

    private void dispatchCallback(int delta, LinkProperties newLp) {
        switch (delta) {
            case PROV_CHANGE_GAINED_PROVISIONING:
                if (DBG) {
                    Log.d(mTag, "onProvisioningSuccess()");
                }
                recordMetric(IpManagerEvent.PROVISIONING_OK);
                mCallback.onProvisioningSuccess(newLp);
                break;

            case PROV_CHANGE_LOST_PROVISIONING:
                if (DBG) {
                    Log.d(mTag, "onProvisioningFailure()");
                }
                recordMetric(IpManagerEvent.PROVISIONING_FAIL);
                mCallback.onProvisioningFailure(newLp);
                break;

            default:
                if (DBG) {
                    Log.d(mTag, "onLinkPropertiesChange()");
                }
                mCallback.onLinkPropertiesChange(newLp);
                break;
        }
    }

    // Updates all IpClient-related state concerned with LinkProperties.
    // Returns a ProvisioningChange for possibly notifying other interested
    // parties that are not fronted by IpClient.
    private int setLinkProperties(LinkProperties newLp) {
        if (mApfFilter != null) {
            mApfFilter.setLinkProperties(newLp);
        }
        if (mIpReachabilityMonitor != null) {
            mIpReachabilityMonitor.updateLinkProperties(newLp);
        }

        int delta = compareProvisioning(mLinkProperties, newLp);
        mLinkProperties = new LinkProperties(newLp);

        if (delta == PROV_CHANGE_GAINED_PROVISIONING) {
            // TODO: Add a proper ProvisionedState and cancel the alarm in
            // its enter() method.
            mProvisioningTimeoutAlarm.cancel();
        }

        return delta;
    }

    private LinkProperties assembleLinkProperties() {
        // [1] Create a new LinkProperties object to populate.
        LinkProperties newLp = new LinkProperties();
        newLp.setInterfaceName(mInterfaceName);

        // [2] Pull in data from netlink:
        //         - IPv4 addresses
        //         - IPv6 addresses
        //         - IPv6 routes
        //         - IPv6 DNS servers
        //
        // N.B.: this is fundamentally race-prone and should be fixed by
        // changing IpClientLinkObserver from a hybrid edge/level model to an
        // edge-only model, or by giving IpClient its own netlink socket(s)
        // so as to track all required information directly.
        LinkProperties netlinkLinkProperties = mLinkObserver.getLinkProperties();
        newLp.setLinkAddresses(netlinkLinkProperties.getLinkAddresses());
        for (RouteInfo route : netlinkLinkProperties.getRoutes()) {
            newLp.addRoute(route);
        }
        addAllReachableDnsServers(newLp, netlinkLinkProperties.getDnsServers());
        newLp.setNat64Prefix(netlinkLinkProperties.getNat64Prefix());

        // [3] Add in data from DHCPv4, if available.
        //
        // mDhcpResults is never shared with any other owner so we don't have
        // to worry about concurrent modification.
        if (mDhcpResults != null) {
            final List<RouteInfo> routes =
                    mDhcpResults.toStaticIpConfiguration().getRoutes(mInterfaceName);
            for (RouteInfo route : routes) {
                newLp.addRoute(route);
            }
            addAllReachableDnsServers(newLp, mDhcpResults.dnsServers);
            newLp.setDomains(mDhcpResults.domains);

            if (mDhcpResults.mtu != 0) {
                newLp.setMtu(mDhcpResults.mtu);
            }

            if (mDhcpResults.serverAddress != null) {
                mShim.setDhcpServerAddress(newLp, mDhcpResults.serverAddress);
            }

            final String capportUrl = mDhcpResults.captivePortalApiUrl;
            // Uri.parse does no syntax check; do a simple check to eliminate garbage.
            // If the URL is still incorrect data fetching will fail later, which is fine.
            if (isParseableUrl(capportUrl)) {
                NetworkInformationShimImpl.newInstance()
                        .setCaptivePortalApiUrl(newLp, Uri.parse(capportUrl));
            }
            // TODO: also look at the IPv6 RA (netlink) for captive portal URL
        }

        // [4] Add in TCP buffer sizes and HTTP Proxy config, if available.
        if (!TextUtils.isEmpty(mTcpBufferSizes)) {
            newLp.setTcpBufferSizes(mTcpBufferSizes);
        }
        if (mHttpProxy != null) {
            newLp.setHttpProxy(mHttpProxy);
        }

        // [5] Add data from InitialConfiguration
        if (mConfiguration != null && mConfiguration.mInitialConfig != null) {
            InitialConfiguration config = mConfiguration.mInitialConfig;
            // Add InitialConfiguration routes and dns server addresses once all addresses
            // specified in the InitialConfiguration have been observed with Netlink.
            if (config.isProvisionedBy(newLp.getLinkAddresses(), null)) {
                for (IpPrefix prefix : config.directlyConnectedRoutes) {
                    newLp.addRoute(new RouteInfo(prefix, null, mInterfaceName, RTN_UNICAST));
                }
            }
            addAllReachableDnsServers(newLp, config.dnsServers);
        }
        final LinkProperties oldLp = mLinkProperties;
        if (DBG) {
            Log.d(mTag, String.format("Netlink-seen LPs: %s, new LPs: %s; old LPs: %s",
                    netlinkLinkProperties, newLp, oldLp));
        }

        // TODO: also learn via netlink routes specified by an InitialConfiguration and specified
        // from a static IP v4 config instead of manually patching them in in steps [3] and [5].
        return newLp;
    }

    private static boolean isParseableUrl(String url) {
        // Verify that a URL has a reasonable format that can be parsed as per the URL constructor.
        // This does not use Patterns.WEB_URL as that pattern excludes URLs without TLDs, such as on
        // localhost.
        if (url == null) return false;
        try {
            new URL(url);
            return true;
        } catch (MalformedURLException e) {
            return false;
        }
    }

    private static void addAllReachableDnsServers(
            LinkProperties lp, Iterable<InetAddress> dnses) {
        // TODO: Investigate deleting this reachability check.  We should be
        // able to pass everything down to netd and let netd do evaluation
        // and RFC6724-style sorting.
        for (InetAddress dns : dnses) {
            if (!dns.isAnyLocalAddress() && lp.isReachable(dns)) {
                lp.addDnsServer(dns);
            }
        }
    }

    // Returns false if we have lost provisioning, true otherwise.
    private boolean handleLinkPropertiesUpdate(boolean sendCallbacks) {
        final LinkProperties newLp = assembleLinkProperties();
        if (Objects.equals(newLp, mLinkProperties)) {
            return true;
        }

        // Either success IPv4 or IPv6 provisioning triggers new LinkProperties update,
        // wait for the provisioning completion and record the latency.
        mIpProvisioningMetrics.setIPv4ProvisionedLatencyOnFirstTime(newLp.isIpv4Provisioned());
        mIpProvisioningMetrics.setIPv6ProvisionedLatencyOnFirstTime(newLp.isIpv6Provisioned());

        final int delta = setLinkProperties(newLp);
        // Most of the attributes stored in the memory store are deduced from
        // the link properties, therefore when the properties update the memory
        // store record should be updated too.
        maybeSaveNetworkToIpMemoryStore();
        if (sendCallbacks) {
            dispatchCallback(delta, newLp);
        }
        return (delta != PROV_CHANGE_LOST_PROVISIONING);
    }

    @VisibleForTesting
    static String removeDoubleQuotes(@NonNull String ssid) {
        final int length = ssid.length();
        if ((length > 1) && (ssid.charAt(0) == '"') && (ssid.charAt(length - 1) == '"')) {
            return ssid.substring(1, length - 1);
        }
        return ssid;
    }

    private List<ByteBuffer> getVendorSpecificIEs(@NonNull ScanResultInfo scanResultInfo) {
        ArrayList<ByteBuffer> vendorSpecificPayloadList = new ArrayList<>();
        for (InformationElement ie : scanResultInfo.getInformationElements()) {
            if (ie.getId() == VENDOR_SPECIFIC_IE_ID) {
                vendorSpecificPayloadList.add(ie.getPayload());
            }
        }
        return vendorSpecificPayloadList;
    }

    private boolean detectUpstreamHotspotFromVendorIe() {
        if (mConfiguration.mScanResultInfo == null) return false;
        final ScanResultInfo scanResultInfo = mConfiguration.mScanResultInfo;
        final String ssid = scanResultInfo.getSsid();
        final List<ByteBuffer> vendorSpecificPayloadList = getVendorSpecificIEs(scanResultInfo);

        if (mConfiguration.mDisplayName == null
                || !removeDoubleQuotes(mConfiguration.mDisplayName).equals(ssid)) {
            return false;
        }

        for (ByteBuffer payload : vendorSpecificPayloadList) {
            byte[] ouiAndType = new byte[4];
            try {
                payload.get(ouiAndType);
            } catch (BufferUnderflowException e) {
                Log.e(mTag, "Couldn't parse vendor specific IE, buffer underflow");
                return false;
            }
            for (byte[] pattern : METERED_IE_PATTERN_LIST) {
                if (Arrays.equals(pattern, ouiAndType)) {
                    if (DBG) {
                        Log.d(mTag, "detected upstream hotspot that matches OUI:"
                                + HexDump.toHexString(ouiAndType));
                    }
                    return true;
                }
            }
        }
        return false;
    }

    private void handleIPv4Success(DhcpResults dhcpResults) {
        mDhcpResults = new DhcpResults(dhcpResults);
        final LinkProperties newLp = assembleLinkProperties();
        final int delta = setLinkProperties(newLp);

        if (mDhcpResults.vendorInfo == null && detectUpstreamHotspotFromVendorIe()) {
            mDhcpResults.vendorInfo = DhcpPacket.VENDOR_INFO_ANDROID_METERED;
        }

        if (DBG) {
            Log.d(mTag, "onNewDhcpResults(" + Objects.toString(mDhcpResults) + ")");
            Log.d(mTag, "handleIPv4Success newLp{" + newLp + "}");
        }
        mCallback.onNewDhcpResults(mDhcpResults);
        maybeSaveNetworkToIpMemoryStore();

        dispatchCallback(delta, newLp);
    }

    private void handleIPv4Failure() {
        // TODO: Investigate deleting this clearIPv4Address() call.
        //
        // DhcpClient will send us CMD_CLEAR_LINKADDRESS in all circumstances
        // that could trigger a call to this function. If we missed handling
        // that message in StartedState for some reason we would still clear
        // any addresses upon entry to StoppedState.
        mInterfaceCtrl.clearIPv4Address();
        mDhcpResults = null;
        if (DBG) {
            Log.d(mTag, "onNewDhcpResults(null)");
        }
        mCallback.onNewDhcpResults(null);

        handleProvisioningFailure(DisconnectCode.DC_PROVISIONING_FAIL);
    }

    private void handleProvisioningFailure(final DisconnectCode code) {
        final LinkProperties newLp = assembleLinkProperties();
        int delta = setLinkProperties(newLp);
        // If we've gotten here and we're still not provisioned treat that as
        // a total loss of provisioning.
        //
        // Either (a) static IP configuration failed or (b) DHCPv4 failed AND
        // there was no usable IPv6 obtained before a non-zero provisioning
        // timeout expired.
        //
        // Regardless: GAME OVER.
        if (delta == PROV_CHANGE_STILL_NOT_PROVISIONED) {
            delta = PROV_CHANGE_LOST_PROVISIONING;
        }

        dispatchCallback(delta, newLp);
        if (delta == PROV_CHANGE_LOST_PROVISIONING) {
            transitionToStoppingState(code);
        }
    }

    private void doImmediateProvisioningFailure(int failureType) {
        logError("onProvisioningFailure(): %s", failureType);
        recordMetric(failureType);
        mCallback.onProvisioningFailure(mLinkProperties);
    }

    private boolean startIPv4() {
        // If we have a StaticIpConfiguration attempt to apply it and
        // handle the result accordingly.
        if (mConfiguration.mStaticIpConfig != null) {
            if (mInterfaceCtrl.setIPv4Address(mConfiguration.mStaticIpConfig.getIpAddress())) {
                handleIPv4Success(new DhcpResults(mConfiguration.mStaticIpConfig));
            } else {
                return false;
            }
        } else {
            if (mDhcpClient != null) {
                Log.wtf(mTag, "DhcpClient should never be non-null in startIPv4()");
            }
            startDhcpClient();
        }

        return true;
    }

    private boolean startIPv6() {
        return mInterfaceCtrl.setIPv6PrivacyExtensions(true)
                && mInterfaceCtrl.setIPv6AddrGenModeIfSupported(mConfiguration.mIPv6AddrGenMode)
                && mInterfaceCtrl.enableIPv6();
    }

    private boolean applyInitialConfig(InitialConfiguration config) {
        // TODO: also support specifying a static IPv4 configuration in InitialConfiguration.
        for (LinkAddress addr : findAll(config.ipAddresses, LinkAddress::isIpv6)) {
            if (!mInterfaceCtrl.addAddress(addr)) return false;
        }

        return true;
    }

    private boolean startIpReachabilityMonitor() {
        try {
            mIpReachabilityMonitor = new IpReachabilityMonitor(
                    mContext,
                    mInterfaceParams,
                    getHandler(),
                    mLog,
                    new IpReachabilityMonitor.Callback() {
                        @Override
                        public void notifyLost(InetAddress ip, String logMsg) {
                            mCallback.onReachabilityLost(logMsg);
                        }
                    },
                    mConfiguration.mUsingMultinetworkPolicyTracker,
                    mNetd);
        } catch (IllegalArgumentException iae) {
            // Failed to start IpReachabilityMonitor. Log it and call
            // onProvisioningFailure() immediately.
            //
            // See http://b/31038971.
            logError("IpReachabilityMonitor failure: %s", iae);
            mIpReachabilityMonitor = null;
        }

        return (mIpReachabilityMonitor != null);
    }

    private void stopAllIP() {
        // We don't need to worry about routes, just addresses, because:
        //     - disableIpv6() will clear autoconf IPv6 routes as well, and
        //     - we don't get IPv4 routes from netlink
        // so we neither react to nor need to wait for changes in either.

        mInterfaceCtrl.disableIPv6();
        mInterfaceCtrl.clearAllAddresses();
    }

    private void maybeSaveNetworkToIpMemoryStore() {
        // TODO : implement this
    }

    private void maybeRestoreInterfaceMtu() {
        InterfaceParams params = mDependencies.getInterfaceParams(mInterfaceName);
        if (params == null) {
            Log.w(mTag, "interface: " + mInterfaceName + " is gone");
            return;
        }

        if (params.index != mInterfaceParams.index) {
            Log.w(mTag, "interface: " + mInterfaceName + " has a different index: " + params.index);
            return;
        }

        if (params.defaultMtu == mInterfaceParams.defaultMtu) return;

        try {
            mNetd.interfaceSetMtu(mInterfaceName, mInterfaceParams.defaultMtu);
        } catch (RemoteException | ServiceSpecificException e) {
            logError("Couldn't reset MTU on " + mInterfaceName + " from "
                    + params.defaultMtu + " to " + mInterfaceParams.defaultMtu, e);
        }
    }

    private void handleUpdateL2Information(@NonNull Layer2InformationParcelable info) {
        mL2Key = info.l2Key;
        mCluster = info.cluster;

        if (info.bssid == null || mCurrentBssid == null) {
            Log.wtf(mTag, "bssid in the parcelable or current tracked bssid should be non-null");
            return;
        }

        // If the BSSID has not changed, there is nothing to do.
        if (info.bssid.equals(mCurrentBssid)) return;

        if (mIpReachabilityMonitor != null) {
            mIpReachabilityMonitor.probeAll();
        }

        // Check whether to refresh previous IP lease on L2 roaming happened.
        final String ssid = removeDoubleQuotes(mConfiguration.mDisplayName);
        if (DHCP_ROAMING_SSID_SET.contains(ssid) && mDhcpClient != null) {
            if (DBG) {
                Log.d(mTag, "L2 roaming happened from " + mCurrentBssid
                        + " to " + info.bssid
                        + " , SSID: " + ssid
                        + " , starting refresh leased IP address");
            }
            mDhcpClient.sendMessage(DhcpClient.CMD_REFRESH_LINKADDRESS);
        }
        mCurrentBssid = info.bssid;
    }

    class StoppedState extends State {
        @Override
        public void enter() {
            stopAllIP();
            mHasDisabledIPv6OnProvLoss = false;

            mLinkObserver.clearInterfaceParams();
            resetLinkProperties();
            if (mStartTimeMillis > 0) {
                // Completed a life-cycle; send a final empty LinkProperties
                // (cleared in resetLinkProperties() above) and record an event.
                mCallback.onLinkPropertiesChange(mLinkProperties);
                recordMetric(IpManagerEvent.COMPLETE_LIFECYCLE);
                mStartTimeMillis = 0;
            }
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_TERMINATE_AFTER_STOP:
                    stopStateMachineUpdaters();
                    quit();
                    break;

                case CMD_STOP:
                    break;

                case CMD_START:
                    mConfiguration = (android.net.shared.ProvisioningConfiguration) msg.obj;
                    transitionTo(mClearingIpAddressesState);
                    break;

                case EVENT_NETLINK_LINKPROPERTIES_CHANGED:
                    handleLinkPropertiesUpdate(NO_CALLBACKS);
                    break;

                case CMD_UPDATE_TCP_BUFFER_SIZES:
                    mTcpBufferSizes = (String) msg.obj;
                    handleLinkPropertiesUpdate(NO_CALLBACKS);
                    break;

                case CMD_UPDATE_HTTP_PROXY:
                    mHttpProxy = (ProxyInfo) msg.obj;
                    handleLinkPropertiesUpdate(NO_CALLBACKS);
                    break;

                case CMD_UPDATE_L2KEY_CLUSTER: {
                    final Pair<String, String> args = (Pair<String, String>) msg.obj;
                    mL2Key = args.first;
                    mCluster = args.second;
                    break;
                }

                case CMD_SET_MULTICAST_FILTER:
                    mMulticastFiltering = (boolean) msg.obj;
                    break;

                case DhcpClient.CMD_ON_QUIT:
                    // Everything is already stopped.
                    logError("Unexpected CMD_ON_QUIT (already stopped).");
                    break;

                default:
                    return NOT_HANDLED;
            }

            mMsgStateLogger.handled(this, getCurrentState());
            return HANDLED;
        }
    }

    class StoppingState extends State {
        @Override
        public void enter() {
            if (mDhcpClient == null) {
                // There's no DHCPv4 for which to wait; proceed to stopped.
                deferMessage(obtainMessage(CMD_JUMP_STOPPING_TO_STOPPED));
            }

            // Restore the interface MTU to initial value if it has changed.
            maybeRestoreInterfaceMtu();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_JUMP_STOPPING_TO_STOPPED:
                    transitionTo(mStoppedState);
                    break;

                case CMD_STOP:
                    break;

                case DhcpClient.CMD_CLEAR_LINKADDRESS:
                    mInterfaceCtrl.clearIPv4Address();
                    break;

                case DhcpClient.CMD_ON_QUIT:
                    mDhcpClient = null;
                    transitionTo(mStoppedState);
                    break;

                default:
                    deferMessage(msg);
            }

            mMsgStateLogger.handled(this, getCurrentState());
            return HANDLED;
        }
    }

    private boolean isUsingPreconnection() {
        return mConfiguration.mEnablePreconnection && mConfiguration.mStaticIpConfig == null;
    }

    private void startDhcpClient() {
        // Start DHCPv4.
        mDhcpClient = mDependencies.makeDhcpClient(mContext, IpClient.this, mInterfaceParams,
                mDependencies.getDhcpClientDependencies(mIpMemoryStore, mIpProvisioningMetrics));

        // If preconnection is enabled, there is no need to ask Wi-Fi to disable powersaving
        // during DHCP, because the DHCP handshake will happen during association. In order to
        // ensure that future renews still do the DHCP action (if configured),
        // registerForPreDhcpNotification is called later when processing the CMD_*_PRECONNECTION
        // messages.
        if (!isUsingPreconnection()) mDhcpClient.registerForPreDhcpNotification();
        mDhcpClient.sendMessage(DhcpClient.CMD_START_DHCP, new DhcpClient.Configuration(mL2Key,
                isUsingPreconnection()));
    }

    class ClearingIpAddressesState extends State {
        @Override
        public void enter() {
            // Ensure that interface parameters are fetched on the handler thread so they are
            // properly ordered with other events, such as restoring the interface MTU on teardown.
            mInterfaceParams = mDependencies.getInterfaceParams(mInterfaceName);
            if (mInterfaceParams == null) {
                logError("Failed to find InterfaceParams for " + mInterfaceName);
                doImmediateProvisioningFailure(IpManagerEvent.ERROR_INTERFACE_NOT_FOUND);
                deferMessage(obtainMessage(CMD_STOP,
                        DisconnectCode.DC_INTERFACE_NOT_FOUND.getNumber()));
                return;
            }

            mLinkObserver.setInterfaceParams(mInterfaceParams);

            if (readyToProceed()) {
                deferMessage(obtainMessage(CMD_ADDRESSES_CLEARED));
            } else {
                // Clear all IPv4 and IPv6 before proceeding to RunningState.
                // Clean up any leftover state from an abnormal exit from
                // tethering or during an IpClient restart.
                stopAllIP();
            }

            mCallback.setNeighborDiscoveryOffload(true);
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_ADDRESSES_CLEARED:
                    transitionTo(isUsingPreconnection() ? mPreconnectingState : mRunningState);
                    break;

                case EVENT_NETLINK_LINKPROPERTIES_CHANGED:
                    handleLinkPropertiesUpdate(NO_CALLBACKS);
                    if (readyToProceed()) {
                        transitionTo(isUsingPreconnection() ? mPreconnectingState : mRunningState);
                    }
                    break;

                case CMD_STOP:
                case EVENT_PROVISIONING_TIMEOUT:
                    // Fall through to StartedState.
                    return NOT_HANDLED;

                default:
                    // It's safe to process messages out of order because the
                    // only message that can both
                    //     a) be received at this time and
                    //     b) affect provisioning state
                    // is EVENT_NETLINK_LINKPROPERTIES_CHANGED (handled above).
                    deferMessage(msg);
            }
            return HANDLED;
        }

        private boolean readyToProceed() {
            return !mLinkProperties.hasIpv4Address() && !mLinkProperties.hasGlobalIpv6Address();
        }
    }

    class PreconnectingState extends State {
        @Override
        public void enter() {
            startDhcpClient();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_COMPLETE_PRECONNECTION:
                    boolean success = (msg.arg1 == 1);
                    mDhcpClient.registerForPreDhcpNotification();
                    if (!success) {
                        mDhcpClient.sendMessage(DhcpClient.CMD_ABORT_PRECONNECTION);
                    }
                    // The link is ready for use. Advance to running state, start IPv6, etc.
                    transitionTo(mRunningState);
                    break;

                case DhcpClient.CMD_START_PRECONNECTION:
                    final Layer2PacketParcelable l2Packet = (Layer2PacketParcelable) msg.obj;
                    mCallback.onPreconnectionStart(Collections.singletonList(l2Packet));
                    break;

                case CMD_STOP:
                case EVENT_PROVISIONING_TIMEOUT:
                    // Fall through to StartedState.
                    return NOT_HANDLED;

                default:
                    deferMessage(msg);
            }
            return HANDLED;
        }
    }

    class StartedState extends State {
        @Override
        public void enter() {
            mIpProvisioningMetrics.reset();
            mStartTimeMillis = SystemClock.elapsedRealtime();
            if (mConfiguration.mProvisioningTimeoutMs > 0) {
                final long alarmTime = SystemClock.elapsedRealtime()
                        + mConfiguration.mProvisioningTimeoutMs;
                mProvisioningTimeoutAlarm.schedule(alarmTime);
            }
        }

        @Override
        public void exit() {
            mProvisioningTimeoutAlarm.cancel();

            // Record metrics information once this provisioning has completed due to certain
            // reason (normal termination, provisioning timeout, lost provisioning and etc).
            mIpProvisioningMetrics.statsWrite();
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_STOP:
                    transitionToStoppingState(DisconnectCode.forNumber(msg.arg1));
                    break;

                case CMD_UPDATE_L2KEY_CLUSTER: {
                    final Pair<String, String> args = (Pair<String, String>) msg.obj;
                    mL2Key = args.first;
                    mCluster = args.second;
                    // TODO : attributes should be saved to the memory store with
                    // these new values if they differ from the previous ones.
                    // If the state machine is in pure StartedState, then the values to input
                    // are not known yet and should be updated when the LinkProperties are updated.
                    // If the state machine is in RunningState (which is a child of StartedState)
                    // then the next NUD check should be used to store the new values to avoid
                    // inputting current values for what may be a different L3 network.
                    break;
                }

                case CMD_UPDATE_L2INFORMATION:
                    handleUpdateL2Information((Layer2InformationParcelable) msg.obj);
                    break;

                case EVENT_PROVISIONING_TIMEOUT:
                    handleProvisioningFailure(DisconnectCode.DC_PROVISIONING_TIMEOUT);
                    break;

                default:
                    return NOT_HANDLED;
            }

            mMsgStateLogger.handled(this, getCurrentState());
            return HANDLED;
        }
    }

    class RunningState extends State {
        private ConnectivityPacketTracker mPacketTracker;
        private boolean mDhcpActionInFlight;

        @Override
        public void enter() {
            ApfFilter.ApfConfiguration apfConfig = new ApfFilter.ApfConfiguration();
            apfConfig.apfCapabilities = mConfiguration.mApfCapabilities;
            apfConfig.multicastFilter = mMulticastFiltering;
            // Get the Configuration for ApfFilter from Context
            apfConfig.ieee802_3Filter = ApfCapabilities.getApfDrop8023Frames();
            apfConfig.ethTypeBlackList = ApfCapabilities.getApfEtherTypeBlackList();
            apfConfig.minRdnssLifetimeSec = mMinRdnssLifetimeSec;
            mApfFilter = ApfFilter.maybeCreate(mContext, apfConfig, mInterfaceParams, mCallback);
            // TODO: investigate the effects of any multicast filtering racing/interfering with the
            // rest of this IP configuration startup.
            if (mApfFilter == null) {
                mCallback.setFallbackMulticastFilter(mMulticastFiltering);
            }

            mPacketTracker = createPacketTracker();
            if (mPacketTracker != null) mPacketTracker.start(mConfiguration.mDisplayName);

            if (mConfiguration.mEnableIPv6 && !startIPv6()) {
                doImmediateProvisioningFailure(IpManagerEvent.ERROR_STARTING_IPV6);
                enqueueJumpToStoppingState(DisconnectCode.DC_ERROR_STARTING_IPV6);
                return;
            }

            if (mConfiguration.mEnableIPv4 && !isUsingPreconnection() && !startIPv4()) {
                doImmediateProvisioningFailure(IpManagerEvent.ERROR_STARTING_IPV4);
                enqueueJumpToStoppingState(DisconnectCode.DC_ERROR_STARTING_IPV4);
                return;
            }

            final InitialConfiguration config = mConfiguration.mInitialConfig;
            if ((config != null) && !applyInitialConfig(config)) {
                // TODO introduce a new IpManagerEvent constant to distinguish this error case.
                doImmediateProvisioningFailure(IpManagerEvent.ERROR_INVALID_PROVISIONING);
                enqueueJumpToStoppingState(DisconnectCode.DC_INVALID_PROVISIONING);
                return;
            }

            if (mConfiguration.mUsingIpReachabilityMonitor && !startIpReachabilityMonitor()) {
                doImmediateProvisioningFailure(
                        IpManagerEvent.ERROR_STARTING_IPREACHABILITYMONITOR);
                enqueueJumpToStoppingState(DisconnectCode.DC_ERROR_STARTING_IPREACHABILITYMONITOR);
                return;
            }
        }

        @Override
        public void exit() {
            stopDhcpAction();

            if (mIpReachabilityMonitor != null) {
                mIpReachabilityMonitor.stop();
                mIpReachabilityMonitor = null;
            }

            if (mDhcpClient != null) {
                mDhcpClient.sendMessage(DhcpClient.CMD_STOP_DHCP);
                mDhcpClient.doQuit();
            }

            if (mPacketTracker != null) {
                mPacketTracker.stop();
                mPacketTracker = null;
            }

            if (mApfFilter != null) {
                mApfFilter.shutdown();
                mApfFilter = null;
            }

            resetLinkProperties();
        }

        private void enqueueJumpToStoppingState(final DisconnectCode code) {
            deferMessage(obtainMessage(CMD_JUMP_RUNNING_TO_STOPPING, code.getNumber()));
        }

        private ConnectivityPacketTracker createPacketTracker() {
            try {
                return new ConnectivityPacketTracker(
                        getHandler(), mInterfaceParams, mConnectivityPacketLog);
            } catch (IllegalArgumentException e) {
                return null;
            }
        }

        private void ensureDhcpAction() {
            if (!mDhcpActionInFlight) {
                mCallback.onPreDhcpAction();
                mDhcpActionInFlight = true;
                final long alarmTime = SystemClock.elapsedRealtime()
                        + mConfiguration.mRequestedPreDhcpActionMs;
                mDhcpActionTimeoutAlarm.schedule(alarmTime);
            }
        }

        private void stopDhcpAction() {
            mDhcpActionTimeoutAlarm.cancel();
            if (mDhcpActionInFlight) {
                mCallback.onPostDhcpAction();
                mDhcpActionInFlight = false;
            }
        }

        @Override
        public boolean processMessage(Message msg) {
            switch (msg.what) {
                case CMD_JUMP_RUNNING_TO_STOPPING:
                case CMD_STOP:
                    transitionToStoppingState(DisconnectCode.forNumber(msg.arg1));
                    break;

                case CMD_START:
                    logError("ALERT: START received in StartedState. Please fix caller.");
                    break;

                case CMD_CONFIRM:
                    // TODO: Possibly introduce a second type of confirmation
                    // that both probes (a) on-link neighbors and (b) does
                    // a DHCPv4 RENEW.  We used to do this on Wi-Fi framework
                    // roams.
                    if (mIpReachabilityMonitor != null) {
                        mIpReachabilityMonitor.probeAll();
                    }
                    break;

                case EVENT_PRE_DHCP_ACTION_COMPLETE:
                    // It's possible to reach here if, for example, someone
                    // calls completedPreDhcpAction() after provisioning with
                    // a static IP configuration.
                    if (mDhcpClient != null) {
                        mDhcpClient.sendMessage(DhcpClient.CMD_PRE_DHCP_ACTION_COMPLETE);
                    }
                    break;

                case EVENT_NETLINK_LINKPROPERTIES_CHANGED:
                    // EVENT_NETLINK_LINKPROPERTIES_CHANGED message will be received in both of
                    // provisioning loss and normal user termination cases (e.g. turn off wifi or
                    // switch to another wifi ssid), hence, checking the current interface link
                    // state (down or up) helps distinguish the two cases: if the link state is
                    // down, provisioning is only lost because the link is being torn down (for
                    // example when turning off wifi), so treat it as a normal termination.
                    if (!handleLinkPropertiesUpdate(SEND_CALLBACKS)) {
                        final boolean linkStateUp = (msg.arg1 == ARG_LINKPROP_CHANGED_LINKSTATE_UP);
                        transitionToStoppingState(linkStateUp ? DisconnectCode.DC_PROVISIONING_FAIL
                                : DisconnectCode.DC_NORMAL_TERMINATION);
                    }
                    break;

                case CMD_UPDATE_TCP_BUFFER_SIZES:
                    mTcpBufferSizes = (String) msg.obj;
                    // This cannot possibly change provisioning state.
                    handleLinkPropertiesUpdate(SEND_CALLBACKS);
                    break;

                case CMD_UPDATE_HTTP_PROXY:
                    mHttpProxy = (ProxyInfo) msg.obj;
                    // This cannot possibly change provisioning state.
                    handleLinkPropertiesUpdate(SEND_CALLBACKS);
                    break;

                case CMD_SET_MULTICAST_FILTER: {
                    mMulticastFiltering = (boolean) msg.obj;
                    if (mApfFilter != null) {
                        mApfFilter.setMulticastFilter(mMulticastFiltering);
                    } else {
                        mCallback.setFallbackMulticastFilter(mMulticastFiltering);
                    }
                    break;
                }

                case EVENT_READ_PACKET_FILTER_COMPLETE: {
                    if (mApfFilter != null) {
                        mApfFilter.setDataSnapshot((byte[]) msg.obj);
                    }
                    mApfDataSnapshotComplete.open();
                    break;
                }

                case CMD_ADD_KEEPALIVE_PACKET_FILTER_TO_APF: {
                    final int slot = msg.arg1;

                    if (mApfFilter != null) {
                        if (msg.obj instanceof NattKeepalivePacketDataParcelable) {
                            mApfFilter.addNattKeepalivePacketFilter(slot,
                                    (NattKeepalivePacketDataParcelable) msg.obj);
                        } else if (msg.obj instanceof TcpKeepalivePacketDataParcelable) {
                            mApfFilter.addTcpKeepalivePacketFilter(slot,
                                    (TcpKeepalivePacketDataParcelable) msg.obj);
                        }
                    }
                    break;
                }

                case CMD_REMOVE_KEEPALIVE_PACKET_FILTER_FROM_APF: {
                    final int slot = msg.arg1;
                    if (mApfFilter != null) {
                        mApfFilter.removeKeepalivePacketFilter(slot);
                    }
                    break;
                }

                case EVENT_DHCPACTION_TIMEOUT:
                    stopDhcpAction();
                    break;

                case DhcpClient.CMD_PRE_DHCP_ACTION:
                    if (mConfiguration.mRequestedPreDhcpActionMs > 0) {
                        ensureDhcpAction();
                    } else {
                        sendMessage(EVENT_PRE_DHCP_ACTION_COMPLETE);
                    }
                    break;

                case DhcpClient.CMD_CLEAR_LINKADDRESS:
                    mInterfaceCtrl.clearIPv4Address();
                    break;

                case DhcpClient.CMD_CONFIGURE_LINKADDRESS: {
                    final LinkAddress ipAddress = (LinkAddress) msg.obj;
                    if (mInterfaceCtrl.setIPv4Address(ipAddress)) {
                        mDhcpClient.sendMessage(DhcpClient.EVENT_LINKADDRESS_CONFIGURED);
                    } else {
                        logError("Failed to set IPv4 address.");
                        dispatchCallback(PROV_CHANGE_LOST_PROVISIONING, mLinkProperties);
                        transitionToStoppingState(DisconnectCode.DC_PROVISIONING_FAIL);
                    }
                    break;
                }

                // This message is only received when:
                //
                //     a) initial address acquisition succeeds,
                //     b) renew succeeds or is NAK'd,
                //     c) rebind succeeds or is NAK'd, or
                //     c) the lease expires,
                //
                // but never when initial address acquisition fails. The latter
                // condition is now governed by the provisioning timeout.
                case DhcpClient.CMD_POST_DHCP_ACTION:
                    stopDhcpAction();

                    switch (msg.arg1) {
                        case DhcpClient.DHCP_SUCCESS:
                            handleIPv4Success((DhcpResults) msg.obj);
                            break;
                        case DhcpClient.DHCP_FAILURE:
                            handleIPv4Failure();
                            break;
                        default:
                            logError("Unknown CMD_POST_DHCP_ACTION status: %s", msg.arg1);
                    }
                    break;

                case DhcpClient.CMD_ON_QUIT:
                    // DHCPv4 quit early for some reason.
                    logError("Unexpected CMD_ON_QUIT.");
                    mDhcpClient = null;
                    break;

                default:
                    return NOT_HANDLED;
            }

            mMsgStateLogger.handled(this, getCurrentState());
            return HANDLED;
        }
    }

    private static class MessageHandlingLogger {
        public String processedInState;
        public String receivedInState;

        public void reset() {
            processedInState = null;
            receivedInState = null;
        }

        public void handled(State processedIn, IState receivedIn) {
            processedInState = processedIn.getClass().getSimpleName();
            receivedInState = receivedIn.getName();
        }

        public String toString() {
            return String.format("rcvd_in=%s, proc_in=%s",
                                 receivedInState, processedInState);
        }
    }

    // TODO: extract out into CollectionUtils.
    static <T> boolean any(Iterable<T> coll, Predicate<T> fn) {
        for (T t : coll) {
            if (fn.test(t)) {
                return true;
            }
        }
        return false;
    }

    static <T> boolean all(Iterable<T> coll, Predicate<T> fn) {
        return !any(coll, not(fn));
    }

    static <T> Predicate<T> not(Predicate<T> fn) {
        return (t) -> !fn.test(t);
    }

    static <T> String join(String delimiter, Collection<T> coll) {
        return coll.stream().map(Object::toString).collect(Collectors.joining(delimiter));
    }

    static <T> T find(Iterable<T> coll, Predicate<T> fn) {
        for (T t: coll) {
            if (fn.test(t)) {
                return t;
            }
        }
        return null;
    }

    static <T> List<T> findAll(Collection<T> coll, Predicate<T> fn) {
        return coll.stream().filter(fn).collect(Collectors.toList());
    }
}
